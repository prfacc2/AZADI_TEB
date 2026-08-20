// ============================================================================
//  web_crm_host.inc — embeds the WebView2 control inside the manage tab and
//  wires the two-way IPC for the CRM page. Included into web_crm.cpp.
//
//  Mirrors web_admission_host.inc: WebView2Loader.dll is loaded lazily; env ->
//  controller -> ICoreWebView2 are created via single-use COM callbacks; we pump
//  messages until navigation + the JS bridge are ready (bounded by a timeout) so
//  the public API stays synchronous. JS messages arrive on WebMessageReceived,
//  are routed to CrmDispatchVerb, and the reply is posted back as JSON. C++ -> JS
//  pushes go through PostWebMessageAsJson.
// ============================================================================
namespace {

// per-view WebView2 objects, keyed by the host child HWND.
struct CrmView {
    HWND host=nullptr;                 // child window that owns the webview
    ICoreWebView2Controller* ctrl=nullptr;
    ICoreWebView2* web=nullptr;
    EventRegistrationToken msgTok{0};
    EventRegistrationToken accelTok{0};
    EventRegistrationToken navTok{0};
    volatile LONG bridgeReady=0;
};
static std::map<HWND,CrmView*> g_crmViews;
static std::mutex g_crmViewsMx;

static CrmView* crmViewFor(HWND h){
    std::lock_guard<std::mutex> lk(g_crmViewsMx);
    auto it=g_crmViews.find(h); return it==g_crmViews.end()?nullptr:it->second;
}

// ---- WebView2Loader.dll dynamic binding ------------------------------------
static PFN_CreateWebView2Env         pCrmCreateEnv=nullptr;
static PFN_GetAvailableBrowserVersion pCrmGetVer=nullptr;
static bool crmLoaderTried=false;

// Extract the embedded WebView2Loader.dll (RCDATA 700) to a per-user writable
// folder and return its full path. Idempotent (a file that already matches the
// embedded size is reused). Shared blob with the admission host — both extract
// the same DLL; LoadLibrary refcounts it.
static std::wstring crmExtractLoaderPath(){
    wchar_t base[MAX_PATH]={0};
    if(FAILED(SHGetFolderPathW(NULL,CSIDL_LOCAL_APPDATA,NULL,0,base))) return L"";
    std::wstring dir=base; dir+=L"\\DarmanPlus\\runtime";
    CreateDirectoryW(dir.c_str(),NULL);
    std::wstring path=dir+L"\\WebView2Loader.dll";

    HRSRC hr=FindResourceW(g_hInst,MAKEINTRESOURCEW(700),RT_RCDATA);
    if(!hr) return L"";
    DWORD need=SizeofResource(g_hInst,hr);
    HGLOBAL hg=LoadResource(g_hInst,hr); if(!hg) return L"";
    void* dat=LockResource(hg); if(!dat||!need) return L"";

    bool write=true;
    HANDLE ex=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,NULL,
                          OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    if(ex!=INVALID_HANDLE_VALUE){
        DWORD sz=GetFileSize(ex,NULL);
        CloseHandle(ex);
        if(sz==need) write=false;
    }
    if(write){
        HANDLE hf=CreateFileW(path.c_str(),GENERIC_WRITE,0,NULL,
                              CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
        if(hf!=INVALID_HANDLE_VALUE){
            DWORD wr=0; WriteFile(hf,dat,need,&wr,NULL); CloseHandle(hf);
            if(wr!=need) return L"";
        } else return L"";
    }
    return path;
}

static void crmLoadLoader(){
    if(crmLoaderTried) return; crmLoaderTried=true;
    HMODULE m=LoadLibraryW(L"WebView2Loader.dll");
    if(!m){
        wchar_t path[MAX_PATH]; GetModuleFileNameW(NULL,path,MAX_PATH);
        std::wstring p=path; size_t s=p.find_last_of(L"\\/"); if(s!=std::wstring::npos) p=p.substr(0,s+1);
        p+=L"WebView2Loader.dll";
        m=LoadLibraryW(p.c_str());
    }
    if(!m){
        std::wstring extracted=crmExtractLoaderPath();
        if(!extracted.empty()) m=LoadLibraryW(extracted.c_str());
    }
    if(m){
        pCrmCreateEnv=reinterpret_cast<PFN_CreateWebView2Env>(
            reinterpret_cast<void*>(GetProcAddress(m,"CreateCoreWebView2EnvironmentWithOptions")));
        pCrmGetVer=reinterpret_cast<PFN_GetAvailableBrowserVersion>(
            reinterpret_cast<void*>(GetProcAddress(m,"GetAvailableCoreWebView2BrowserVersionString")));
    }
}

// ============================================================================
//  COM callback objects (single-use, self-deleting on final Release).
// ============================================================================

// WebMessageReceived handler → route to CrmDispatchVerb and reply.
struct CrmMsgHandler : public ICoreWebView2WebMessageReceivedEventHandler {
    LONG rc=1; HWND host;
    CrmMsgHandler(HWND h):host(h){}
    virtual ~CrmMsgHandler(){}
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
        std::string verb,id,page;
        jsonGetString(body,"verb",verb);
        jsonGetString(body,"id",id);
        jsonGetString(body,"page",page);
        if(verb=="crm.init"||verb=="init"){
            CrmView* v=crmViewFor(host);
            if(v) InterlockedExchange(&v->bridgeReady,1);
        }
        std::string payload; if(!jsonGetRaw(body,"payload",payload)) payload="{}";
        std::string result=CrmDispatchVerb(verb,payload,page);
        std::string reply="{\"id\":"+jstr(u82w(id))+",\"result\":"+result+"}";
        if(sender) sender->PostWebMessageAsJson(u82w(reply).c_str());
        return S_OK;
    }
};

// AcceleratorKeyPressed handler → let the WebView handle nav/edit keys itself.
struct CrmAccelHandler : public ICoreWebView2AcceleratorKeyPressedEventHandler {
    LONG rc=1;
    CrmAccelHandler(){}
    virtual ~CrmAccelHandler(){}
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
                args->put_Handled(FALSE);
            }
        }
        return S_OK;
    }
};

// Heap-owned state shared by both WebView2 completion callbacks.
struct CrmCreateState {
    LONG rc=1;
    volatile LONG done=0;
    volatile LONG cancelled=0;
    volatile LONG navReady=0;
    HRESULT hr=E_PENDING;
    HWND host;
    explicit CrmCreateState(HWND h):host(h){}
    void AddRef(){ InterlockedIncrement(&rc); }
    void Release(){ if(!InterlockedDecrement(&rc)) delete this; }
    bool IsCancelled(){ return InterlockedCompareExchange(&cancelled,0,0)!=0; }
    void Cancel(){ InterlockedExchange(&cancelled,1); }
    void Complete(HRESULT h){ hr=h; InterlockedExchange(&done,1); }
};

struct CrmNavHandler : public ICoreWebView2NavigationCompletedEventHandler {
    LONG rc=1; CrmCreateState* state;
    explicit CrmNavHandler(CrmCreateState* s):state(s){ state->AddRef(); }
    virtual ~CrmNavHandler(){ state->Release(); }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if(!ppv) return E_POINTER;
        if(IsEqualIID(riid,IID_IUnknown)||
           IsEqualIID(riid,IID_ICoreWebView2NavigationCompletedEventHandler_)){
            *ppv=this; InterlockedIncrement(&rc); return S_OK; }
        *ppv=nullptr; return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return InterlockedIncrement(&rc); }
    ULONG STDMETHODCALLTYPE Release() override { LONG r=InterlockedDecrement(&rc); if(!r) delete this; return r; }
    HRESULT STDMETHODCALLTYPE Invoke(ICoreWebView2*,
            ICoreWebView2NavigationCompletedEventArgs* args) override {
        if(state->IsCancelled()) return S_OK;
        BOOL ok=FALSE;
        HRESULT hr=args?args->get_IsSuccess(&ok):E_POINTER;
        if(SUCCEEDED(hr) && ok) InterlockedExchange(&state->navReady,1);
        else state->Complete(FAILED(hr)?hr:E_FAIL);
        return S_OK;
    }
};

struct CrmCtrlHandler : public ICoreWebView2CreateCoreWebView2ControllerCompletedHandler {
    LONG rc=1; CrmCreateState* state;
    explicit CrmCtrlHandler(CrmCreateState* s):state(s){ state->AddRef(); }
    virtual ~CrmCtrlHandler(){ state->Release(); }
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
        if(state->IsCancelled()) return S_OK;
        if(FAILED(result) || !ctrl){ state->Complete(FAILED(result)?result:E_FAIL); return S_OK; }

        CrmView* v=crmViewFor(state->host);
        if(!v){ state->Complete(E_ABORT); return S_OK; }
        ctrl->AddRef(); v->ctrl=ctrl;
        HRESULT hr=ctrl->get_CoreWebView2(&v->web);
        if(FAILED(hr) || !v->web){ state->Complete(FAILED(hr)?hr:E_FAIL); return S_OK; }

        CrmNavHandler* nh=new CrmNavHandler(state);
        hr=v->web->add_NavigationCompleted(nh,&v->navTok);
        nh->Release();
        if(FAILED(hr)){ state->Complete(hr); return S_OK; }

        CrmAccelHandler* ah=new CrmAccelHandler();
        hr=ctrl->add_AcceleratorKeyPressed(ah,&v->accelTok);
        ah->Release();
        if(FAILED(hr)){ state->Complete(hr); return S_OK; }

        CrmMsgHandler* mh=new CrmMsgHandler(state->host);
        hr=v->web->add_WebMessageReceived(mh,&v->msgTok);
        mh->Release();
        if(FAILED(hr)){ state->Complete(hr); return S_OK; }

        RECT bounds; GetClientRect(state->host,&bounds);
        hr=ctrl->put_Bounds(bounds);
        if(SUCCEEDED(hr)) hr=ctrl->put_IsVisible(TRUE);
        if(FAILED(hr)){ state->Complete(hr); return S_OK; }

        const std::string& page=CrmInlinePage(true);
        if(page.empty()){ state->Complete(E_FAIL); return S_OK; }
        hr=v->web->NavigateToString(u82w(page).c_str());
        if(FAILED(hr)) state->Complete(hr);
        return S_OK;
    }
};

struct CrmEnvHandler : public ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler {
    LONG rc=1; CrmCreateState* state;
    explicit CrmEnvHandler(CrmCreateState* s):state(s){ state->AddRef(); }
    virtual ~CrmEnvHandler(){ state->Release(); }
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
        if(state->IsCancelled()) return S_OK;
        if(SUCCEEDED(result) && env){
            CrmCtrlHandler* ch=new CrmCtrlHandler(state);
            HRESULT hr=env->CreateCoreWebView2Controller(state->host,ch);
            ch->Release();
            if(FAILED(hr)) state->Complete(hr);
        } else {
            state->Complete(FAILED(result)?result:E_FAIL);
        }
        return S_OK;
    }
};

// user-data folder for the runtime (per-user, offline-safe). SEPARATE from the
// admission environment so two WebView2 environments never collide on the same
// folder (not supported by the runtime).
static std::wstring crmUserDataFolder(){
    wchar_t buf[MAX_PATH]={0};
    if(SUCCEEDED(SHGetFolderPathW(NULL,CSIDL_LOCAL_APPDATA,NULL,0,buf))){
        std::wstring p=buf; p+=L"\\DarmanPlus\\WebView2Crm";
        return p;
    }
    return L"";
}

} // namespace

// ============================================================================
//  Public-ish host API (file-static; wired into the dispatch layer)
// ============================================================================
static bool WV2Crm_DestroyView(HWND view);

static bool WV2Crm_Available(){
    crmLoadLoader();
    if(!pCrmCreateEnv) return false;
    if(pCrmGetVer){
        LPWSTR ver=nullptr;
        HRESULT hr=pCrmGetVer(nullptr,&ver);
        bool ok = SUCCEEDED(hr) && ver && ver[0];
        if(ver) CoTaskMemFree(ver);
        return ok;
    }
    return true;
}

static HWND WV2Crm_CreateView(HWND parent){
    if(!parent) return nullptr;
    crmLoadLoader();
    if(!pCrmCreateEnv) return nullptr;

    if(CrmInlinePage(true).empty()) return nullptr;

    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    static bool cls=false;
    static const wchar_t* CLS=L"AzWebCrmHost";
    if(!cls){
        WNDCLASSW wc={}; wc.lpfnWndProc=DefWindowProcW; wc.hInstance=g_hInst;
        wc.hCursor=LoadCursor(NULL,IDC_ARROW);
        wc.hbrBackground=(HBRUSH)(COLOR_WINDOW+1);
        wc.lpszClassName=CLS;
        if(!RegisterClassW(&wc) && GetLastError()!=ERROR_CLASS_ALREADY_EXISTS)
            return nullptr;
        cls=true;
    }
    RECT rc; GetClientRect(parent,&rc);
    HWND host=CreateWindowExW(0,CLS,L"",WS_CHILD|WS_CLIPCHILDREN,
        0,0,rc.right,rc.bottom,parent,NULL,g_hInst,NULL);
    if(!host) return nullptr;

    CrmView* v=new CrmView(); v->host=host;
    { std::lock_guard<std::mutex> lk(g_crmViewsMx); g_crmViews[host]=v; }

    CrmCreateState* state=new CrmCreateState(host);
    state->AddRef();
    std::wstring udf=crmUserDataFolder();
    CrmEnvHandler* eh=new CrmEnvHandler(state);
    HRESULT hr=pCrmCreateEnv(nullptr, udf.empty()?nullptr:udf.c_str(), nullptr, eh);
    eh->Release();
    if(FAILED(hr)) state->Complete(hr);

    DWORD start=GetTickCount();
    MSG msg;
    while(!InterlockedCompareExchange(&state->done,0,0) &&
          !(InterlockedCompareExchange(&state->navReady,0,0) &&
            InterlockedCompareExchange(&v->bridgeReady,0,0))){
        if(GetTickCount()-start>8000) break;
        if(PeekMessageW(&msg,NULL,0,0,PM_REMOVE)){
            TranslateMessage(&msg); DispatchMessageW(&msg);
        } else Sleep(5);
    }

    bool ready=InterlockedCompareExchange(&state->done,0,0)==0 &&
               InterlockedCompareExchange(&state->navReady,0,0)!=0 &&
               InterlockedCompareExchange(&v->bridgeReady,0,0)!=0 &&
               v->ctrl && v->web;
    if(!ready) state->Cancel();
    state->Release();
    if(!ready){
        WV2Crm_DestroyView(host);
        state->Release();
        return nullptr;
    }
    state->Release();
    ShowWindow(host,SW_SHOW);
    return host;
}

static void WV2Crm_Resize(HWND view, int w, int h){
    CrmView* v=crmViewFor(view); if(!v||!v->ctrl) return;
    MoveWindow(view,0,0,w,h,TRUE);
    RECT rc={0,0,w,h};
    v->ctrl->put_Bounds(rc);
}

static void WV2Crm_PushEvent(const char* eventName, const std::string& jsonData){
    std::string msg="{\"event\":"+jstr(u82w(eventName))+",\"data\":"+
        (jsonData.empty()?std::string("{}"):jsonData)+"}";
    std::lock_guard<std::mutex> lk(g_crmViewsMx);
    for(auto& kv:g_crmViews){
        CrmView* v=kv.second;
        if(v && v->web) v->web->PostWebMessageAsJson(u82w(msg).c_str());
    }
}

static bool WV2Crm_DestroyView(HWND view){
    CrmView* v=nullptr;
    { std::lock_guard<std::mutex> lk(g_crmViewsMx);
      auto it=g_crmViews.find(view); if(it!=g_crmViews.end()){ v=it->second; g_crmViews.erase(it); } }
    if(!v) return false;
    if(v->web){
        if(v->navTok.value) v->web->remove_NavigationCompleted(v->navTok);
        if(v->msgTok.value) v->web->remove_WebMessageReceived(v->msgTok);
        v->web->Release(); v->web=nullptr;
    }
    if(v->ctrl){
        if(v->accelTok.value) v->ctrl->remove_AcceleratorKeyPressed(v->accelTok);
        v->ctrl->Close(); v->ctrl->Release(); v->ctrl=nullptr;
    }
    if(IsWindow(view)) DestroyWindow(view);
    delete v;
    return true;
}

static bool WV2Crm_Owns(HWND view){
    std::lock_guard<std::mutex> lk(g_crmViewsMx);
    return g_crmViews.find(view)!=g_crmViews.end();
}

static bool WV2Crm_TranslateAccel(MSG*){ return false; }
