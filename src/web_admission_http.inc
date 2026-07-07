// ============================================================================
//  web_admission_http.inc — loopback (127.0.0.1) HTTP host for the admission
//  bundle. Included into web_admission.cpp. Single blocking-accept thread; only
//  serves the embedded assets + /api bridge. Loopback-only → no net exposure.
//  Assets are embedded as RCDATA (ids 400..405) so the UI is 100% offline.
// ============================================================================
namespace {

static CRITICAL_SECTION g_adCs;
static bool   g_adCsInit=false;
static int    g_adPort=0;
static SOCKET g_adListen=INVALID_SOCKET;
static HANDLE g_adThread=NULL;
static volatile LONG g_adStop=0;

// queue of C++ -> JS events for the HTTP transport (dev / fallback). In the
// embedded WebView2 path events are pushed directly via PostWebMessageAsJson.
static std::vector<std::string> g_adEventQ;

static void adLock(){ if(g_adCsInit) EnterCriticalSection(&g_adCs); }
static void adUnlock(){ if(g_adCsInit) LeaveCriticalSection(&g_adCs); }

// load an embedded RCDATA asset
static bool adLoadRes(int id, std::string& out){
    HRSRC h=FindResourceW(g_hInst,MAKEINTRESOURCEW(id),RT_RCDATA);
    if(!h) return false;
    HGLOBAL g=LoadResource(g_hInst,h); if(!g) return false;
    void* p=LockResource(g); DWORD sz=SizeofResource(g_hInst,h);
    if(!p||!sz) return false;
    out.assign((const char*)p,(size_t)sz); return true;
}

static bool adAssetForPath(const std::string& path, int& id, const char*& ctype){
    std::string p=path; size_t q=p.find('?'); if(q!=std::string::npos) p=p.substr(0,q);
    if(p=="/"||p=="/index.html"){ id=400; ctype="text/html; charset=utf-8"; return true; }
    if(p=="/admission.css"){ id=401; ctype="text/css; charset=utf-8"; return true; }
    if(p=="/bridge.js"){ id=402; ctype="application/javascript; charset=utf-8"; return true; }
    if(p=="/admission.js"){ id=403; ctype="application/javascript; charset=utf-8"; return true; }
    if(p=="/vazir.ttf"){ id=404; ctype="font/ttf"; return true; }
    return false;
}

static void adSendAll(SOCKET s, const char* d, int len){
    int off=0; while(off<len){ int n=send(s,d+off,len-off,0); if(n<=0)break; off+=n; }
}
static void adReply(SOCKET s, int code, const char* ctype, const std::string& body){
    const char* st=(code==200)?"OK":(code==404?"Not Found":"Error");
    char hdr[512];
    int n=snprintf(hdr,sizeof(hdr),
        "HTTP/1.1 %d %s\r\nContent-Type: %s\r\nContent-Length: %d\r\n"
        "Cache-Control: no-store\r\nAccess-Control-Allow-Origin: *\r\nConnection: close\r\n\r\n",
        code,st,ctype,(int)body.size());
    adSendAll(s,hdr,n); if(!body.empty()) adSendAll(s,body.data(),(int)body.size());
}

static DWORD WINAPI adHttpThread(LPVOID){
    for(;;){
        sockaddr_in ca; int cl=sizeof(ca);
        SOCKET c=accept(g_adListen,(sockaddr*)&ca,&cl);
        if(c==INVALID_SOCKET){ if(InterlockedCompareExchange(&g_adStop,0,0)) break; Sleep(5); continue; }
        std::string req; char buf[2048]; int got; size_t hdrEnd=std::string::npos;
        while((got=recv(c,buf,sizeof(buf),0))>0){
            req.append(buf,got); hdrEnd=req.find("\r\n\r\n");
            if(hdrEnd!=std::string::npos) break;
            if(req.size()>65536) break;
        }
        if(hdrEnd==std::string::npos){ closesocket(c); continue; }
        std::string method,path;
        { size_t sp1=req.find(' '); size_t sp2=(sp1==std::string::npos)?std::string::npos:req.find(' ',sp1+1);
          if(sp1!=std::string::npos&&sp2!=std::string::npos){ method=req.substr(0,sp1); path=req.substr(sp1+1,sp2-sp1-1);} }
        size_t clen=0;
        { size_t cp=req.find("Content-Length:"); if(cp==std::string::npos) cp=req.find("content-length:");
          if(cp!=std::string::npos) clen=(size_t)atoi(req.c_str()+cp+15); }
        std::string bdy=req.substr(hdrEnd+4);
        while(bdy.size()<clen && (got=recv(c,buf,sizeof(buf),0))>0) bdy.append(buf,got);
        if(bdy.size()>clen) bdy=bdy.substr(0,clen);

        if(path.compare(0,5,"/api/")==0){
            std::string verb=path.substr(5); size_t q=verb.find('?'); if(q!=std::string::npos) verb=verb.substr(0,q);
            if(verb=="poll"){
                std::string o="{\"events\":[";
                adLock();
                for(size_t i=0;i<g_adEventQ.size();++i){ if(i)o+=","; o+=g_adEventQ[i]; }
                g_adEventQ.clear();
                adUnlock();
                o+="]}";
                adReply(c,200,"application/json; charset=utf-8",o);
            } else {
                std::string out=admissionApi(verb,bdy);
                adReply(c,200,"application/json; charset=utf-8",out);
            }
        } else {
            int id; const char* ctype;
            if(adAssetForPath(path,id,ctype)){
                std::string data;
                if(adLoadRes(id,data)) adReply(c,200,ctype,data);
                else adReply(c,404,"text/plain","not found");
            } else adReply(c,404,"text/plain","not found");
        }
        closesocket(c);
        if(InterlockedCompareExchange(&g_adStop,0,0)) break;
    }
    return 0;
}

} // namespace

int WebAdmission_EnsureHost(){
    if(g_adPort) return g_adPort;
    if(!g_adCsInit){ InitializeCriticalSection(&g_adCs); g_adCsInit=true; }
    WSADATA wsa; static bool wsaUp=false;
    if(!wsaUp){ if(WSAStartup(MAKEWORD(2,2),&wsa)!=0) return 0; wsaUp=true; }
    SOCKET s=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
    if(s==INVALID_SOCKET) return 0;
    sockaddr_in a={}; a.sin_family=AF_INET;
    a.sin_addr.s_addr=htonl(INADDR_LOOPBACK); a.sin_port=0;
    if(bind(s,(sockaddr*)&a,sizeof(a))!=0){ closesocket(s); return 0; }
    int alen=sizeof(a);
    if(getsockname(s,(sockaddr*)&a,&alen)!=0){ closesocket(s); return 0; }
    if(listen(s,8)!=0){ closesocket(s); return 0; }
    g_adListen=s; g_adPort=ntohs(a.sin_port);
    g_adThread=CreateThread(NULL,0,adHttpThread,NULL,0,NULL);
    return g_adPort;
}
