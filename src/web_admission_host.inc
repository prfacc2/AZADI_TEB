// ============================================================================
//  web_admission_host.inc — embeds the WebView2 control inside the reception
//  tab and wires the two-way IPC. Included into web_admission.cpp.
//
//  Design:
//   * WebView2Loader.dll (or the Evergreen runtime export) is loaded lazily.
//   * CreateCoreWebView2EnvironmentWithOptions -> environment; then
//     CreateCoreWebView2Controller(parent) -> controller -> ICoreWebView2.
//   * Both async steps are driven by lightweight IUnknown callback objects; we
//     pump messages until the controller is ready (bounded by a timeout) so the
//     public API stays synchronous and simple for the caller.
//   * JS messages (chrome.webview.postMessage) arrive on WebMessageReceived; we
//     parse {verb,id,payload}, call admissionApi, and post {id,result} back.
//   * C++ -> JS pushes go through PostWebMessageAsJson.
// ============================================================================
namespace {

// per-view WebView2 objects, keyed by the host child HWND.
struct AdView {
    HWND host=nullptr;                 // child window that owns the webview
    ICoreWebView2Controller* ctrl=nullptr;
    ICoreWebView2* web=nullptr;
    EventRegistrationToken msgTok{0};
    EventRegistrationToken accelTok{0};
};
static std::map<HWND,AdView*> g_adViews;
static std::mutex g_adViewsMx;

static AdView* adViewFor(HWND h){
    std::lock_guard<std::mutex> lk(g_adViewsMx);
    auto it=g_adViews.find(h); return it==g_adViews.end()?nullptr:it->second;
}

// ---- WebView2Loader.dll dynamic binding ------------------------------------
static PFN_CreateWebView2Env         pCreateEnv=nullptr;
static PFN_GetAvailableBrowserVersion pGetVer=nullptr;
static bool adLoaderTried=false;

static void adLoadLoader(){
    if(adLoaderTried) return; adLoaderTried=true;
    HMODULE m=LoadLibraryW(L"WebView2Loader.dll");
    if(!m){
        // try the fixed-runtime folder next to the exe
        wchar_t path[MAX_PATH]; GetModuleFileNameW(NULL,path,MAX_PATH);
        std::wstring p=path; size_t s=p.find_last_of(L"\\/"); if(s!=std::wstring::npos) p=p.substr(0,s+1);
        p+=L"WebView2Loader.dll";
        m=LoadLibraryW(p.c_str());
    }
    if(m){
        pCreateEnv=reinterpret_cast<PFN_CreateWebView2Env>(
            reinterpret_cast<void*>(GetProcAddress(m,"CreateCoreWebView2EnvironmentWithOptions")));
        pGetVer=reinterpret_cast<PFN_GetAvailableBrowserVersion>(
            reinterpret_cast<void*>(GetProcAddress(m,"GetAvailableCoreWebView2BrowserVersionString")));
    }
}

// ============================================================================
//  COM callback objects (single-use, self-deleting on final Release).
// ============================================================================

// WebMessageReceived handler → route to admissionApi and reply.
struct MsgHandler : public ICoreWebView2WebMessageReceivedEventHandler {
    LONG rc=1; HWND host;
    MsgHandler(HWND h):host(h){}
    virtual ~MsgHandler(){}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if(!ppv) return E_POINTER;
        if(IsEqualIID(riid,IID_IUnknown)||
           IsEqualIID(riid,IID_ICoreWebView2WebMessageReceivedEventHandler_)){
            *ppv=this; InterlockedIncrement(&rc); return S_OK; }
        *ppv=nullptr; return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return InterlockedIncrement(&rc); }
    ULONG STDMETHODCALLTYPE Release() override { LONG r=InterlockedDecrement(&rc); if(!r) delete this; return r; }
    HRESULT STDMETHODCALLTYPE Invoke(ICoreWebView2* sender,
            ICoreWebView2WebMessageReceivedEventArgs* args) override {
        if(!args) return S_OK;
        LPWSTR js=nullptr;
        if(FAILED(args->get_WebMessageAsJson(&js))||!js) return S_OK;
        std::string body=w2u8(js); CoTaskMemFree(js);
        // body = {"verb":"...","id":"...","payload":{...}}
        std::string verb,id;
        jsonGetString(body,"verb",verb);
        jsonGetString(body,"id",id);
        std::string payload; if(!jsonGetRaw(body,"payload",payload)) payload="{}";
        std::string result=admissionApi(verb,payload);
        // reply: {"id":"<id>","result":<result>}
        std::string reply="{\"id\":"+jstr(u82w(id))+",\"result\":"+result+"}";
        if(sender) sender->PostWebMessageAsJson(u82w(reply).c_str());
        return S_OK;
    }
};

// AcceleratorKeyPressed handler → let the WebView handle nav/edit keys itself
// (Tab / Enter / Ctrl+A / arrows / Home / End / Esc) so the page keydown
// listeners fire. Everything else keeps WebView2's default behaviour.
struct AccelHandler : public ICoreWebView2AcceleratorKeyPressedEventHandler {
    LONG rc=1;
    AccelHandler(){}
    virtual ~AccelHandler(){}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if(!ppv) return E_POINTER;
        if(IsEqualIID(riid,IID_IUnknown)||
           IsEqualIID(riid,IID_ICoreWebView2AcceleratorKeyPressedEventHandler_)){
            *ppv=this; InterlockedIncrement(&rc); return S_OK; }
        *ppv=nullptr; return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return InterlockedIncrement(&rc); }
    ULONG STDMETHODCALLTYPE Release() override { LONG r=InterlockedDecrement(&rc); if(!r) delete this; return r; }
    HRESULT STDMETHODCALLTYPE Invoke(ICoreWebView2Controller*,
            ICoreWebView2AcceleratorKeyPressedEventArgs* args) override {
        if(!args) return S_OK;
        int kind=0; UINT vk=0;
        args->get_KeyEventKind(&kind);
        args->get_VirtualKey(&vk);
        bool isDown = (kind==COREWEBVIEW2_KEY_EVENT_KIND_KEY_DOWN ||
                       kind==COREWEBVIEW2_KEY_EVENT_KIND_SYSTEM_KEY_DOWN);
        if(isDown){
            bool ctrl=(GetKeyState(VK_CONTROL)&0x8000)!=0;
            if(vk==VK_TAB || vk==VK_RETURN || vk==VK_LEFT || vk==VK_RIGHT ||
               vk==VK_UP  || vk==VK_DOWN   || vk==VK_HOME || vk==VK_END  ||
               vk==VK_ESCAPE || (ctrl && vk=='A')){
                args->put_Handled(FALSE);   // WebView processes it → JS keydown fires
            }
        }
        return S_OK;
    }
};

// Controller-created handler.
struct CtrlHandler : public ICoreWebView2CreateCoreWebView2ControllerCompletedHandler {
    LONG rc=1; HWND host; volatile LONG* done; HRESULT* outHr;
    CtrlHandler(HWND h, volatile LONG* d, HRESULT* o):host(h),done(d),outHr(o){}
    virtual ~CtrlHandler(){}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if(!ppv) return E_POINTER;
        if(IsEqualIID(riid,IID_IUnknown)||
           IsEqualIID(riid,IID_ICoreWebView2CreateCoreWebView2ControllerCompletedHandler_)){
            *ppv=this; InterlockedIncrement(&rc); return S_OK; }
        *ppv=nullptr; return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return InterlockedIncrement(&rc); }
    ULONG STDMETHODCALLTYPE Release() override { LONG r=InterlockedDecrement(&rc); if(!r) delete this; return r; }
    HRESULT STDMETHODCALLTYPE Invoke(HRESULT result, ICoreWebView2Controller* ctrl) override {
        *outHr=result;
        if(SUCCEEDED(result) && ctrl){
            AdView* v=adViewFor(host);
            if(v){
                ctrl->AddRef(); v->ctrl=ctrl;
                ctrl->get_CoreWebView2(&v->web);
                // subscribe to accelerator keys so Tab/Enter/Ctrl+A/arrows are
                // handled by the WebView (and thus reach the page's keydown).
                {
                    AccelHandler* ah=new AccelHandler();
                    ctrl->add_AcceleratorKeyPressed(ah, &v->accelTok);
                    ah->Release();
                }
                if(v->web){
                    // wire JS -> C++
                    MsgHandler* mh=new MsgHandler(host);
                    v->web->add_WebMessageReceived(mh, &v->msgTok);
                    mh->Release();
                    // size + navigate to the loopback bundle
                    RECT rc; GetClientRect(host,&rc);
                    ctrl->put_Bounds(rc);
                    ctrl->put_IsVisible(TRUE);
                    int port=WebAdmission_EnsureHost();
                    wchar_t url[96]; swprintf(url,96,L"http://127.0.0.1:%d/index.html",port);
                    v->web->Navigate(url);
                }
            }
        }
        InterlockedExchange(done,1);
        return S_OK;
    }
};

// Environment-created handler → immediately create the controller.
struct EnvHandler : public ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler {
    LONG rc=1; HWND host; volatile LONG* done; HRESULT* outHr;
    EnvHandler(HWND h, volatile LONG* d, HRESULT* o):host(h),done(d),outHr(o){}
    virtual ~EnvHandler(){}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if(!ppv) return E_POINTER;
        if(IsEqualIID(riid,IID_IUnknown)||
           IsEqualIID(riid,IID_ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler_)){
            *ppv=this; InterlockedIncrement(&rc); return S_OK; }
        *ppv=nullptr; return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return InterlockedIncrement(&rc); }
    ULONG STDMETHODCALLTYPE Release() override { LONG r=InterlockedDecrement(&rc); if(!r) delete this; return r; }
    HRESULT STDMETHODCALLTYPE Invoke(HRESULT result, ICoreWebView2Environment* env) override {
        if(SUCCEEDED(result) && env){
            CtrlHandler* ch=new CtrlHandler(host,done,outHr);
            HRESULT hr=env->CreateCoreWebView2Controller(host, ch);
            ch->Release();
            if(FAILED(hr)){ *outHr=hr; InterlockedExchange(done,1); }
        } else {
            *outHr=result; InterlockedExchange(done,1);
        }
        return S_OK;
    }
};

// user-data folder for the runtime (per-user, offline-safe)
static std::wstring adUserDataFolder(){
    wchar_t buf[MAX_PATH]={0};
    if(SUCCEEDED(SHGetFolderPathW(NULL,CSIDL_LOCAL_APPDATA,NULL,0,buf))){
        std::wstring p=buf; p+=L"\\AzadiTeb\\WebView2";
        return p;
    }
    return L"";
}

} // namespace

// ============================================================================
//  Public API
// ============================================================================
static bool WV2_Available(){
    adLoadLoader();
    if(!pCreateEnv) return false;
    if(pGetVer){
        LPWSTR ver=nullptr;
        HRESULT hr=pGetVer(nullptr,&ver);
        bool ok = SUCCEEDED(hr) && ver && ver[0];
        if(ver) CoTaskMemFree(ver);
        return ok;
    }
    return true; // loader present but no version query — assume usable
}

static HWND WV2_CreateView(HWND parent){
    if(!parent) return nullptr;
    adLoadLoader();
    if(!pCreateEnv) return nullptr;

    // ensure the asset host is up before we navigate
    if(!WebAdmission_EnsureHost()) return nullptr;

    // COM must be initialised on this (UI) thread for WebView2.
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    // a plain child window to host the webview (fills the parent)
    static bool cls=false;
    static const wchar_t* CLS=L"AzWebAdmissionHost";
    if(!cls){
        WNDCLASSW wc={}; wc.lpfnWndProc=DefWindowProcW; wc.hInstance=g_hInst;
        wc.hCursor=LoadCursor(NULL,IDC_ARROW);
        wc.hbrBackground=(HBRUSH)(COLOR_WINDOW+1);
        wc.lpszClassName=CLS;
        RegisterClassW(&wc); cls=true;
    }
    RECT rc; GetClientRect(parent,&rc);
    HWND host=CreateWindowExW(0,CLS,L"",WS_CHILD|WS_VISIBLE|WS_CLIPCHILDREN,
        0,0,rc.right,rc.bottom,parent,NULL,g_hInst,NULL);
    if(!host) return nullptr;

    AdView* v=new AdView(); v->host=host;
    { std::lock_guard<std::mutex> lk(g_adViewsMx); g_adViews[host]=v; }

    volatile LONG done=0; HRESULT outHr=E_FAIL;
    std::wstring udf=adUserDataFolder();
    EnvHandler* eh=new EnvHandler(host,&done,&outHr);
    HRESULT hr=pCreateEnv(nullptr, udf.empty()?nullptr:udf.c_str(), nullptr, eh);
    eh->Release();
    if(FAILED(hr)){
        DestroyWindow(host);
        { std::lock_guard<std::mutex> lk(g_adViewsMx); g_adViews.erase(host); }
        delete v; return nullptr;
    }

    // pump until the controller callback fires (bounded ~6s)
    DWORD start=GetTickCount();
    MSG msg;
    while(!InterlockedCompareExchange(&done,0,0)){
        if(GetTickCount()-start>6000) break;
        if(PeekMessageW(&msg,NULL,0,0,PM_REMOVE)){
            TranslateMessage(&msg); DispatchMessageW(&msg);
        } else Sleep(5);
    }

    if(!v->ctrl || !v->web){
        DestroyWindow(host);
        { std::lock_guard<std::mutex> lk(g_adViewsMx); g_adViews.erase(host); }
        delete v; return nullptr;
    }
    return host;
}

static void WV2_Resize(HWND view, int w, int h){
    AdView* v=adViewFor(view); if(!v||!v->ctrl) return;
    MoveWindow(view,0,0,w,h,TRUE);
    RECT rc={0,0,w,h};
    v->ctrl->put_Bounds(rc);
}

static void WV2_PushEvent(const char* eventName, const std::string& jsonData){
    // format: {"event":"name","data":<jsonData>}
    std::string msg="{\"event\":"+jstr(u82w(eventName))+",\"data\":"+
        (jsonData.empty()?std::string("{}"):jsonData)+"}";
    // HTTP fallback queue
    adLock(); g_adEventQ.push_back(msg); adUnlock();
    // direct push to every live embedded view
    std::lock_guard<std::mutex> lk(g_adViewsMx);
    for(auto& kv:g_adViews){
        AdView* v=kv.second;
        if(v && v->web) v->web->PostWebMessageAsJson(u82w(msg).c_str());
    }
}

static bool WV2_DestroyView(HWND view){
    AdView* v=nullptr;
    { std::lock_guard<std::mutex> lk(g_adViewsMx);
      auto it=g_adViews.find(view); if(it!=g_adViews.end()){ v=it->second; g_adViews.erase(it); } }
    if(!v) return false;   // not a WebView2 view
    if(v->web){ v->web->remove_WebMessageReceived(v->msgTok); v->web->Release(); v->web=nullptr; }
    if(v->ctrl){ v->ctrl->remove_AcceleratorKeyPressed(v->accelTok); v->ctrl->Close(); v->ctrl->Release(); v->ctrl=nullptr; }
    if(IsWindow(view)) DestroyWindow(view);
    delete v;
    return true;
}

// True if `view` is a live WebView2 host we own.
static bool WV2_Owns(HWND view){
    std::lock_guard<std::mutex> lk(g_adViewsMx);
    return g_adViews.find(view)!=g_adViews.end();
}

// WebView2 does NOT need TranslateAccelerator in the main pump — the
// add_AcceleratorKeyPressed subscription is sufficient. This exists only so the
// dispatch layer can treat both engines uniformly.
static bool WV2_TranslateAccel(MSG*){ return false; }
