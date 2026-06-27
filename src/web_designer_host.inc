// ============================================================================
//  web_designer_host.inc — app-owned window hosting the system WebBrowser
//  (Trident) ActiveX control, pointed at the loopback designer URL.
//  Included into web_designer.cpp.
//
//  This uses the WebBrowser control present on EVERY Windows 7→11 (no SDK, no
//  WebView2 runtime, no external browser). We raise FEATURE_BROWSER_EMULATION
//  for our process so the page renders in a modern (IE11/edge) document mode.
//  A minimal IOleClientSite/IOleInPlaceSite/IOleInPlaceFrame implements the
//  hosting contract. The page<->C++ sync is over the loopback HTTP /api bridge
//  (see web_designer_http.inc), so no IDispatch plumbing is required here.
// ============================================================================
namespace {

// --- raise document mode for this exe (HKCU, idempotent, best-effort) -------
static void wdSetBrowserEmulation(){
    wchar_t exe[MAX_PATH]; GetModuleFileNameW(NULL,exe,MAX_PATH);
    std::wstring name=exe; size_t s=name.find_last_of(L"\\/");
    if(s!=std::wstring::npos) name=name.substr(s+1);
    HKEY k;
    if(RegCreateKeyExW(HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Internet Explorer\\Main\\FeatureControl\\FEATURE_BROWSER_EMULATION",
        0,NULL,0,KEY_SET_VALUE,NULL,&k,NULL)==ERROR_SUCCESS){
        DWORD mode=11001;   // IE11 edge mode
        RegSetValueExW(k,name.c_str(),0,REG_DWORD,(const BYTE*)&mode,sizeof(mode));
        RegCloseKey(k);
    }
}

// --- WebView2 availability probe (informational only) -----------------------
static bool wdWebView2Present(){
    HMODULE m=LoadLibraryW(L"WebView2Loader.dll");
    if(m){ FreeLibrary(m); return true; }
    return false;
}

// --- minimal OLE host site --------------------------------------------------
struct HostSite : public IOleClientSite, public IOleInPlaceSite, public IOleInPlaceFrame, public IStorage {
    LONG ref; HWND hwnd;
    HostSite(HWND h):ref(1),hwnd(h){}
    virtual ~HostSite(){}
    // IUnknown
    STDMETHODIMP QueryInterface(REFIID r, void** pp){
        if(r==IID_IUnknown||r==IID_IOleClientSite){ *pp=(IOleClientSite*)this; AddRef(); return S_OK; }
        if(r==IID_IOleInPlaceSite){ *pp=(IOleInPlaceSite*)this; AddRef(); return S_OK; }
        if(r==IID_IOleInPlaceFrame){ *pp=(IOleInPlaceFrame*)this; AddRef(); return S_OK; }
        if(r==IID_IStorage){ *pp=(IStorage*)this; AddRef(); return S_OK; }
        *pp=NULL; return E_NOINTERFACE;
    }
    STDMETHODIMP_(ULONG) AddRef(){ return InterlockedIncrement(&ref); }
    STDMETHODIMP_(ULONG) Release(){ LONG c=InterlockedDecrement(&ref); if(c==0) delete this; return c; }
    // IOleClientSite
    STDMETHODIMP SaveObject(){ return S_OK; }
    STDMETHODIMP GetMoniker(DWORD,DWORD,IMoniker** pp){ *pp=NULL; return E_NOTIMPL; }
    STDMETHODIMP GetContainer(IOleContainer** pp){ *pp=NULL; return E_NOINTERFACE; }
    STDMETHODIMP ShowObject(){ return S_OK; }
    STDMETHODIMP OnShowWindow(BOOL){ return S_OK; }
    STDMETHODIMP RequestNewObjectLayout(){ return E_NOTIMPL; }
    // IOleWindow (shared)
    STDMETHODIMP GetWindow(HWND* p){ *p=hwnd; return S_OK; }
    STDMETHODIMP ContextSensitiveHelp(BOOL){ return S_OK; }
    // IOleInPlaceSite
    STDMETHODIMP CanInPlaceActivate(){ return S_OK; }
    STDMETHODIMP OnInPlaceActivate(){ return S_OK; }
    STDMETHODIMP OnUIActivate(){ return S_OK; }
    STDMETHODIMP GetWindowContext(IOleInPlaceFrame** ppF,IOleInPlaceUIWindow** ppU,LPRECT pr,LPRECT pc,LPOLEINPLACEFRAMEINFO fi){
        *ppF=(IOleInPlaceFrame*)this; AddRef(); *ppU=NULL;
        GetClientRect(hwnd,pr); GetClientRect(hwnd,pc);
        if(fi){ fi->fMDIApp=FALSE; fi->hwndFrame=hwnd; fi->haccel=NULL; fi->cAccelEntries=0; }
        return S_OK;
    }
    STDMETHODIMP Scroll(SIZE){ return S_OK; }
    STDMETHODIMP OnUIDeactivate(BOOL){ return S_OK; }
    STDMETHODIMP OnInPlaceDeactivate(){ return S_OK; }
    STDMETHODIMP DiscardUndoState(){ return S_OK; }
    STDMETHODIMP DeactivateAndUndo(){ return S_OK; }
    STDMETHODIMP OnPosRectChange(LPCRECT){ return S_OK; }
    // IOleInPlaceFrame / UIWindow
    STDMETHODIMP GetBorder(LPRECT pr){ GetClientRect(hwnd,pr); return S_OK; }
    STDMETHODIMP RequestBorderSpace(LPCBORDERWIDTHS){ return S_OK; }
    STDMETHODIMP SetBorderSpace(LPCBORDERWIDTHS){ return S_OK; }
    STDMETHODIMP SetActiveObject(IOleInPlaceActiveObject*,LPCOLESTR){ return S_OK; }
    STDMETHODIMP InsertMenus(HMENU,LPOLEMENUGROUPWIDTHS){ return S_OK; }
    STDMETHODIMP SetMenu(HMENU,HOLEMENU,HWND){ return S_OK; }
    STDMETHODIMP RemoveMenus(HMENU){ return S_OK; }
    STDMETHODIMP SetStatusText(LPCOLESTR){ return S_OK; }
    STDMETHODIMP EnableModeless(BOOL){ return S_OK; }
    STDMETHODIMP TranslateAccelerator(LPMSG,WORD){ return S_FALSE; }
    // IStorage (stubs — WebBrowser tolerates a minimal storage)
    STDMETHODIMP CreateStream(const OLECHAR*,DWORD,DWORD,DWORD,IStream**){ return E_NOTIMPL; }
    STDMETHODIMP OpenStream(const OLECHAR*,void*,DWORD,DWORD,IStream**){ return E_NOTIMPL; }
    STDMETHODIMP CreateStorage(const OLECHAR*,DWORD,DWORD,DWORD,IStorage**){ return E_NOTIMPL; }
    STDMETHODIMP OpenStorage(const OLECHAR*,IStorage*,DWORD,SNB,DWORD,IStorage**){ return E_NOTIMPL; }
    STDMETHODIMP CopyTo(DWORD,const IID*,SNB,IStorage*){ return E_NOTIMPL; }
    STDMETHODIMP MoveElementTo(const OLECHAR*,IStorage*,const OLECHAR*,DWORD){ return E_NOTIMPL; }
    STDMETHODIMP Commit(DWORD){ return S_OK; }
    STDMETHODIMP Revert(){ return S_OK; }
    STDMETHODIMP EnumElements(DWORD,void*,DWORD,IEnumSTATSTG**){ return E_NOTIMPL; }
    STDMETHODIMP DestroyElement(const OLECHAR*){ return E_NOTIMPL; }
    STDMETHODIMP RenameElement(const OLECHAR*,const OLECHAR*){ return E_NOTIMPL; }
    STDMETHODIMP SetElementTimes(const OLECHAR*,const FILETIME*,const FILETIME*,const FILETIME*){ return E_NOTIMPL; }
    STDMETHODIMP SetClass(REFCLSID){ return S_OK; }
    STDMETHODIMP SetStateBits(DWORD,DWORD){ return S_OK; }
    STDMETHODIMP Stat(STATSTG*,DWORD){ return E_NOTIMPL; }
};

struct WebHost {
    HostSite* site=nullptr;
    IOleObject* ole=nullptr;
    IWebBrowser2* wb=nullptr;
    HWND hwnd=nullptr;
};

static const wchar_t* WD_CLS=L"AzWebDesigner";

static bool createBrowser(WebHost* wh){
    OleInitialize(NULL);
    wh->site=new HostSite(wh->hwnd);
    HRESULT hr=CoCreateInstance(CLSID_WebBrowser,NULL,CLSCTX_INPROC_SERVER,
        IID_IOleObject,(void**)&wh->ole);
    if(FAILED(hr)||!wh->ole) return false;
    wh->ole->SetClientSite(wh->site);
    OleSetContainedObject(wh->ole,TRUE);
    RECT rc; GetClientRect(wh->hwnd,&rc);
    hr=wh->ole->DoVerb(OLEIVERB_INPLACEACTIVATE,NULL,wh->site,0,wh->hwnd,&rc);
    if(FAILED(hr)) return false;
    hr=wh->ole->QueryInterface(IID_IWebBrowser2,(void**)&wh->wb);
    if(FAILED(hr)||!wh->wb) return false;
    return true;
}

static void sizeBrowser(WebHost* wh){
    if(!wh||!wh->ole) return;
    RECT rc; GetClientRect(wh->hwnd,&rc);
    IOleInPlaceObject* ip=nullptr;
    if(SUCCEEDED(wh->ole->QueryInterface(IID_IOleInPlaceObject,(void**)&ip))&&ip){
        ip->SetObjectRects(&rc,&rc); ip->Release();
    }
    if(wh->wb){ wh->wb->put_Width(rc.right-rc.left); wh->wb->put_Height(rc.bottom-rc.top); }
}

static void navigate(WebHost* wh, const std::wstring& url){
    if(!wh->wb) return;
    VARIANT v; VariantInit(&v);
    BSTR b=SysAllocString(url.c_str());
    wh->wb->Navigate(b,&v,&v,&v,&v);
    SysFreeString(b);
}

static void destroyBrowser(WebHost* wh){
    if(wh->wb){ wh->wb->Stop(); wh->wb->Release(); wh->wb=nullptr; }
    if(wh->ole){ wh->ole->Close(OLECLOSE_NOSAVE); wh->ole->SetClientSite(NULL); wh->ole->Release(); wh->ole=nullptr; }
    if(wh->site){ wh->site->Release(); wh->site=nullptr; }
}

// poll the exit flag the page sets via /api/exit
static VOID CALLBACK exitPoll(HWND h,UINT,UINT_PTR id,DWORD){
    if(InterlockedCompareExchange(&g_wdExitFlag,0,0)){
        KillTimer(h,id); DestroyWindow(h);
    }
}

static LRESULT CALLBACK WdProc(HWND h,UINT m,WPARAM w,LPARAM l){
    WebHost* wh=(WebHost*)GetWindowLongPtrW(h,GWLP_USERDATA);
    switch(m){
        case WM_SIZE: if(wh) sizeBrowser(wh); return 0;
        case WM_CLOSE: DestroyWindow(h); return 0;
        case WM_DESTROY: if(wh){ destroyBrowser(wh); } return 0;
    }
    return DefWindowProcW(h,m,w,l);
}

} // namespace

bool WebDesigner_WebViewAvailable(){ return wdWebView2Present(); }

bool WebDesigner_Open(HWND owner, const std::vector<int>& sectionIds){
    int sid = sectionIds.empty()?0:sectionIds[0];
    std::wstring url=WebDesigner_Url(sid);
    if(url.empty()) return false;          // host failed → fall back to native
    wdSetBrowserEmulation();

    static bool reg=false;
    if(!reg){ WNDCLASSW wc={}; wc.lpfnWndProc=WdProc; wc.hInstance=g_hInst;
        wc.hCursor=LoadCursor(NULL,IDC_ARROW); wc.lpszClassName=WD_CLS;
        wc.hbrBackground=(HBRUSH)(COLOR_WINDOW+1); wc.style=CS_HREDRAW|CS_VREDRAW;
        if(!RegisterClassW(&wc)) return false; reg=true; }

    RECT wa; SystemParametersInfoW(SPI_GETWORKAREA,0,&wa,0);
    HWND h=CreateWindowExW(0,WD_CLS,L"\u0637\u0631\u0627\u062d \u0686\u0627\u067e",
        WS_POPUP|WS_VISIBLE|WS_CLIPCHILDREN,
        wa.left,wa.top,wa.right-wa.left,wa.bottom-wa.top,owner,NULL,g_hInst,NULL);
    if(!h) return false;

    WebHost* wh=new WebHost(); wh->hwnd=h;
    SetWindowLongPtrW(h,GWLP_USERDATA,(LONG_PTR)wh);

    if(!createBrowser(wh)){
        destroyBrowser(wh); delete wh;
        DestroyWindow(h);
        return false;                      // → caller falls back to native
    }
    sizeBrowser(wh);
    navigate(wh,url);
    ShowWindow(h,SW_SHOW);
    if(owner&&IsWindow(owner)) EnableWindow(owner,FALSE);
    SetTimer(h,1,200,exitPoll);

    // modal pump until the designer window closes
    MSG msg;
    while(IsWindow(h) && GetMessageW(&msg,NULL,0,0)){
        if(msg.message==WM_QUIT){ PostQuitMessage((int)msg.wParam); break; }
        if(!IsWindow(h)) break;
        if(IsDialogMessageW(h,&msg)){ if(!IsWindow(h)) break; continue; }
        TranslateMessage(&msg); DispatchMessageW(&msg);
        if(!IsWindow(h)) break;
    }
    delete wh;
    if(owner&&IsWindow(owner)){ EnableWindow(owner,TRUE); SetForegroundWindow(owner); }
    return true;
}
