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

// diagnostics: how many times the loaded page has actually hit /api/init.
// Used by the headless admission_probe to prove the embedded JS ran without a
// syntax error (a broken script would never reach the bridge).
static volatile LONG g_adInitHits=0;

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
    if(p=="/contextmenu.js"){ id=405; ctype="application/javascript; charset=utf-8"; return true; }
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

// Serve a single accepted connection. Runs on its own worker thread so that
// MSHTML/WebView2 (which opens several parallel connections for the page +
// css + js + font) never has an asset request stall behind another. This was
// the reason the CSS/JS "did not load": a single blocking accept loop could
// only satisfy one asset at a time, so parallel requests timed out and the
// page rendered raw (unstyled, no script).
// Case-insensitive scan for an HTTP header value (returns "" when absent).
static std::string adHeaderValue(const std::string& req, const char* name){
    std::string lower=req; for(size_t i=0;i<lower.size();++i) lower[i]=(char)tolower((unsigned char)lower[i]);
    std::string key=name; for(size_t i=0;i<key.size();++i) key[i]=(char)tolower((unsigned char)key[i]);
    size_t p=lower.find("\n"+key+":"); size_t base;
    if(p!=std::string::npos) base=p+1+key.size()+1;
    else { // header may be the very first line (no preceding \n) — rare
        if(lower.compare(0,key.size()+1,key+":")==0) base=key.size()+1; else return "";
    }
    // value spans from `base` in the ORIGINAL request to end-of-line.
    size_t eol=req.find("\r\n",base); if(eol==std::string::npos) eol=req.find('\n',base);
    if(eol==std::string::npos) eol=req.size();
    std::string v=req.substr(base,eol-base);
    // trim surrounding whitespace.
    size_t a=v.find_first_not_of(" \t"); size_t b=v.find_last_not_of(" \t\r");
    if(a==std::string::npos) return "";
    return v.substr(a,b-a+1);
}

static void adServeConn(SOCKET c){
    std::string req; char buf[2048]; int got; size_t hdrEnd=std::string::npos;
    while((got=recv(c,buf,sizeof(buf),0))>0){
        req.append(buf,got); hdrEnd=req.find("\r\n\r\n");
        if(hdrEnd!=std::string::npos) break;
        if(req.size()>65536) break;
    }
    // v1.44.0 §2.6: a dead/slow peer that never completes the request headers
    // trips the recv timeout (got<0, WSAETIMEDOUT). Log ONE warn line and drop
    // the socket so a wedged peer can never pin a worker forever.
    if(hdrEnd==std::string::npos){
        if(got<0 && WSAGetLastError()==WSAETIMEDOUT)
            ClientLog_Warn(L"admission", L"loopback recv timeout", L"{}");
        closesocket(c); return;
    }
    std::string method,path;
    { size_t sp1=req.find(' '); size_t sp2=(sp1==std::string::npos)?std::string::npos:req.find(' ',sp1+1);
      if(sp1!=std::string::npos&&sp2!=std::string::npos){ method=req.substr(0,sp1); path=req.substr(sp1+1,sp2-sp1-1);} }
    size_t clen=0;
    { size_t cp=req.find("Content-Length:"); if(cp==std::string::npos) cp=req.find("content-length:");
      if(cp!=std::string::npos) clen=(size_t)atoi(req.c_str()+cp+15); }
    std::string bdy=req.substr(hdrEnd+4);
    while(bdy.size()<clen && (got=recv(c,buf,sizeof(buf),0))>0) bdy.append(buf,got);
    if(bdy.size()>clen) bdy=bdy.substr(0,clen);

    // v1.40.0: the shell stamps every /api request with the calling page id so
    // the server can namespace verbs + correlate client.log / client.metrics.
    std::string azPage=adHeaderValue(req,"X-Az-Page");

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
            if(verb=="init") InterlockedIncrement(&g_adInitHits);
            // v1.40.0: try the generic multi-page verb registry FIRST (ping,
            // client.log, client.metrics + any future page verb). Fall back to
            // the legacy admission handler so every existing verb keeps working.
            std::string out;
            if(!WebPages_DispatchVerb(verb,bdy,azPage,out))
                out=admissionApiSafe(verb,bdy);   // v1.43.0: exception-hardened
            adReply(c,200,"application/json; charset=utf-8",out);
        }
    } else {
        // Asset resolution order:
        //   1) legacy admission bundle (hard-coded 400..405 switch) — unchanged
        //   2) generic multi-page registry (shell 500.. + ping 600.. + future)
        int id; const char* ctype;
        if(adAssetForPath(path,id,ctype)){
            std::string data;
            if(adLoadRes(id,data)) adReply(c,200,ctype,data);
            else adReply(c,404,"text/plain","not found");
        } else if(WebPages_ResolveAsset(path,id,ctype)){
            std::string data;
            if(adLoadRes(id,data)) adReply(c,200,ctype,data);
            else adReply(c,404,"text/plain","not found");
        } else adReply(c,404,"text/plain","not found");
    }
    closesocket(c);
}

// Worker thread: owns one connection, serves it, then exits.
static DWORD WINAPI adConnThread(LPVOID p){
    SOCKET c=(SOCKET)(UINT_PTR)p;
    adServeConn(c);
    return 0;
}

static DWORD WINAPI adHttpThread(LPVOID){
    // v1.45.0 §6: if the listen socket is torn down (e.g. a WSAECONNRESET storm)
    // accept() can keep returning INVALID_SOCKET and spin this loop. Count
    // consecutive failures WITHOUT an intervening valid connection; after 200
    // in a row, log ONE warn line and reset the counter. We NEVER kill the loop
    // — the app must keep trying to accept so the /api bridge can recover.
    int adInvalidStreak=0;
    for(;;){
        sockaddr_in ca; int cl=sizeof(ca);
        SOCKET c=accept(g_adListen,(sockaddr*)&ca,&cl);
        if(c==INVALID_SOCKET){
            if(InterlockedCompareExchange(&g_adStop,0,0)) break;
            if(++adInvalidStreak>=200){
                ClientLog_Warn(L"admission", L"accept loop invalid-socket storm",
                    L"{\"streak\":200}");
                adInvalidStreak=0;   // re-arm; keep trying to accept
            }
            Sleep(5); continue;
        }
        adInvalidStreak=0;   // a valid connection resets the streak
        if(InterlockedCompareExchange(&g_adStop,0,0)){ closesocket(c); break; }
        // v1.40.0 LOOPBACK-ONLY ENFORCEMENT: even though we bind to
        // INADDR_LOOPBACK, defensively reject any peer that is not 127.0.0.0/8
        // so the /api bridge (which drives the real data layer) can NEVER be
        // reached from another host, regardless of future bind changes.
        if(ca.sin_family!=AF_INET || (ntohl(ca.sin_addr.s_addr)>>24)!=127){
            closesocket(c); continue;
        }
        // v1.44.0 §2.6 TIMEOUTS: a stuck/slow peer must never pin a worker
        // forever. 3s recv/send timeout on the accepted socket; on a timeout the
        // worker logs ONE warn line (see adServeConn) and drops the connection.
        DWORD tv=3000; // 3s recv/send timeout
        setsockopt(c,SOL_SOCKET,SO_RCVTIMEO,(const char*)&tv,sizeof(tv));
        setsockopt(c,SOL_SOCKET,SO_SNDTIMEO,(const char*)&tv,sizeof(tv));
        // v1.40.0: hand the connection to the bounded worker pool (2 low-power /
        // 4 default / cap 8). Falls back to a per-connection thread (and finally
        // to inline serving) so behaviour is never worse than before.
        if(WebPool_Submit(c)) continue;
        HANDLE th=CreateThread(NULL,0,adConnThread,(LPVOID)(UINT_PTR)c,0,NULL);
        if(th) CloseHandle(th);
        else { adServeConn(c); } // fallback: serve inline if thread spawn fails
    }
    return 0;
}

} // namespace

int WebAdmission_EnsureHost(){
    if(g_adPort) return g_adPort;
    if(!g_adCsInit){ InitializeCriticalSection(&g_adCs); g_adCsInit=true; }
    WSADATA wsa; static bool wsaUp=false;
    if(!wsaUp){ if(WSAStartup(MAKEWORD(2,2),&wsa)!=0) return 0; wsaUp=true; }

    // v1.40.0: populate the multi-page registry (shell + ping assets, ping /
    // client.log / client.metrics verbs) BEFORE the host starts accepting, and
    // bring up the bounded worker pool that serves accepted connections.
    WebPages_RegisterBuiltins();
    WebPool_Init([](SOCKET c){ adServeConn(c); });

    SOCKET s=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
    if(s==INVALID_SOCKET) return 0;
    // v1.40.0: SO_REUSEADDR so a quick relaunch after a crash can re-bind the
    // ephemeral loopback port without waiting out TIME_WAIT.
    { BOOL yes=TRUE; setsockopt(s,SOL_SOCKET,SO_REUSEADDR,(const char*)&yes,sizeof(yes)); }
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
