// ============================================================================
//  web_admission_mshtml.inc — UNIVERSAL fallback renderer for the admission
//  page: an embedded OLE WebBrowser control (Trident/MSHTML) hosted inside a
//  child window of the reception tab. Included into web_admission.cpp.
//
//  Why: the modern WebView2 (Chromium) surface is preferred, but it needs the
//  Evergreen runtime installed. On machines without it we STILL render the
//  HTML/CSS/JS admission page *inside the app* (never an external browser) via
//  the WebBrowser ActiveX control, which ships with every Windows and is
//  available under Wine. The page is served by the same loopback HTTP host and
//  talks to C++ through the same /api bridge (fetch), so it stays fully synced.
//
//  We force FEATURE_BROWSER_EMULATION to the highest available document mode so
//  the control uses the most modern Trident engine (not IE7 quirks) — the page
//  is written in tolerant ES5-compatible JS, so it runs correctly here too.
// ============================================================================
#include <exdisp.h>
#include <exdispid.h>
#include <mshtml.h>
#include <mshtmhst.h>   // IDocHostUIHandler — needed so Tab/Enter/Ctrl+A reach the page
#include <docobj.h>
#include <ocidl.h>

namespace {

// ICustomDoc's IID/interface is not always exported by MinGW's import libs, so
// declare both here (MinGW-friendly). ICustomDoc::SetUIHandler lets us register
// our IDocHostUIHandler on the document so MSHTML consults it for accelerators.
static const IID IID_ICustomDoc_local =
  {0x3050f3f0,0x98b5,0x11cf,{0xbb,0x82,0x00,0xaa,0x00,0xbd,0xce,0x0b}};
struct ICustomDocLocal : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE SetUIHandler(IDocHostUIHandler* pUIHandler) = 0;
};

// ---- put the process into the highest browser-emulation mode (per-user) ----
static void mshtmlSetBrowserEmulation(){
    wchar_t exe[MAX_PATH]={0}; GetModuleFileNameW(NULL,exe,MAX_PATH);
    const wchar_t* base=exe; for(const wchar_t* p=exe;*p;++p) if(*p==L'\\'||*p==L'/') base=p+1;
    HKEY k;
    if(RegCreateKeyExW(HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Internet Explorer\\Main\\FeatureControl\\FEATURE_BROWSER_EMULATION",
        0,NULL,0,KEY_SET_VALUE,NULL,&k,NULL)==ERROR_SUCCESS){
        DWORD v=11001; // edge/latest available document mode
        RegSetValueExW(k,base,0,REG_DWORD,(const BYTE*)&v,sizeof(v));
        RegCloseKey(k);
    }
}

// ============================================================================
//  Minimal OLE client site so the WebBrowser control can be hosted in-place.
// ============================================================================
struct MshtmlHost : public IOleClientSite, public IOleInPlaceSite,
                    public IOleInPlaceFrame, public IStorage,
                    public IDocHostUIHandler {
    LONG rc=1;
    HWND hwnd=nullptr;             // container child window
    IOleObject* oleObj=nullptr;
    IWebBrowser2* wb=nullptr;

    MshtmlHost(HWND h):hwnd(h){}
    virtual ~MshtmlHost(){}

    // ---- IUnknown ----
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if(!ppv) return E_POINTER;
        if(IsEqualIID(riid,IID_IUnknown)||IsEqualIID(riid,IID_IOleClientSite)) *ppv=static_cast<IOleClientSite*>(this);
        else if(IsEqualIID(riid,IID_IOleInPlaceSite)) *ppv=static_cast<IOleInPlaceSite*>(this);
        else if(IsEqualIID(riid,IID_IOleInPlaceFrame)) *ppv=static_cast<IOleInPlaceFrame*>(this);
        else if(IsEqualIID(riid,IID_IStorage)) *ppv=static_cast<IStorage*>(this);
        else if(IsEqualIID(riid,IID_IDocHostUIHandler)) *ppv=static_cast<IDocHostUIHandler*>(this);
        else { *ppv=nullptr; return E_NOINTERFACE; }
        AddRef(); return S_OK;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return InterlockedIncrement(&rc); }
    ULONG STDMETHODCALLTYPE Release() override { LONG r=InterlockedDecrement(&rc); if(!r) delete this; return r; }

    // ---- IOleClientSite ----
    HRESULT STDMETHODCALLTYPE SaveObject() override { return S_OK; }
    HRESULT STDMETHODCALLTYPE GetMoniker(DWORD,DWORD,IMoniker** m) override { if(m)*m=nullptr; return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetContainer(IOleContainer** c) override { if(c)*c=nullptr; return E_NOINTERFACE; }
    HRESULT STDMETHODCALLTYPE ShowObject() override { return S_OK; }
    HRESULT STDMETHODCALLTYPE OnShowWindow(BOOL) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE RequestNewObjectLayout() override { return E_NOTIMPL; }

    // ---- IOleWindow (shared by InPlaceSite/Frame) ----
    HRESULT STDMETHODCALLTYPE GetWindow(HWND* p) override { if(p)*p=hwnd; return S_OK; }
    HRESULT STDMETHODCALLTYPE ContextSensitiveHelp(BOOL) override { return S_OK; }

    // ---- IOleInPlaceSite ----
    HRESULT STDMETHODCALLTYPE CanInPlaceActivate() override { return S_OK; }
    HRESULT STDMETHODCALLTYPE OnInPlaceActivate() override { return S_OK; }
    HRESULT STDMETHODCALLTYPE OnUIActivate() override { return S_OK; }
    HRESULT STDMETHODCALLTYPE GetWindowContext(IOleInPlaceFrame** frame, IOleInPlaceUIWindow** doc,
            LPRECT pos, LPRECT clip, LPOLEINPLACEFRAMEINFO info) override {
        if(frame){ *frame=static_cast<IOleInPlaceFrame*>(this); AddRef(); }
        if(doc) *doc=nullptr;
        RECT rc; ::GetClientRect(hwnd,&rc);
        if(pos) *pos=rc;
        if(clip) *clip=rc;
        if(info){ info->cb=sizeof(OLEINPLACEFRAMEINFO); info->fMDIApp=FALSE; info->hwndFrame=hwnd;
                  info->haccel=NULL; info->cAccelEntries=0; }
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE Scroll(SIZE) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE OnUIDeactivate(BOOL) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE OnInPlaceDeactivate() override { return S_OK; }
    HRESULT STDMETHODCALLTYPE DiscardUndoState() override { return S_OK; }
    HRESULT STDMETHODCALLTYPE DeactivateAndUndo() override { return S_OK; }
    HRESULT STDMETHODCALLTYPE OnPosRectChange(LPCRECT) override { return S_OK; }

    // ---- IOleInPlaceFrame ----
    HRESULT STDMETHODCALLTYPE GetBorder(LPRECT) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE RequestBorderSpace(LPCBORDERWIDTHS) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetBorderSpace(LPCBORDERWIDTHS) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE SetActiveObject(IOleInPlaceActiveObject*, LPCOLESTR) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE InsertMenus(HMENU, LPOLEMENUGROUPWIDTHS) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE SetMenu(HMENU, HOLEMENU, HWND) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE RemoveMenus(HMENU) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE SetStatusText(LPCOLESTR) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE EnableModeless(BOOL) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE TranslateAccelerator(LPMSG, WORD) override { return S_FALSE; }

    // ---- IStorage (stubbed — the control tolerates a minimal impl) ----
    HRESULT STDMETHODCALLTYPE CreateStream(const OLECHAR*,DWORD,DWORD,DWORD,IStream**) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE OpenStream(const OLECHAR*,void*,DWORD,DWORD,IStream**) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE CreateStorage(const OLECHAR*,DWORD,DWORD,DWORD,IStorage**) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE OpenStorage(const OLECHAR*,IStorage*,DWORD,SNB,DWORD,IStorage**) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE CopyTo(DWORD,const IID*,SNB,IStorage*) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE MoveElementTo(const OLECHAR*,IStorage*,const OLECHAR*,DWORD) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE Commit(DWORD) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE Revert() override { return S_OK; }
    HRESULT STDMETHODCALLTYPE EnumElements(DWORD,void*,DWORD,IEnumSTATSTG**) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE DestroyElement(const OLECHAR*) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE RenameElement(const OLECHAR*,const OLECHAR*) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetElementTimes(const OLECHAR*,const FILETIME*,const FILETIME*,const FILETIME*) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetClass(REFCLSID) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE SetStateBits(DWORD,DWORD) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE Stat(STATSTG*,DWORD) override { return E_NOTIMPL; }

    // ---- IDocHostUIHandler --------------------------------------------------
    //  The whole reason this interface exists here: MSHTML asks the host site
    //  whether it wants to consume an accelerator BEFORE routing the key into
    //  the document. Returning S_FALSE from TranslateAccelerator tells MSHTML
    //  "the container did NOT handle it — please dispatch it inside the page",
    //  which is exactly what makes Tab / Enter / Ctrl+A / arrows reach the JS
    //  keydown listeners on each admission field. Without this, the embedded
    //  WebBrowser control never fires keydown at all.
    HRESULT STDMETHODCALLTYPE ShowContextMenu(DWORD,POINT*,IUnknown*,IDispatch*) override {
        return S_OK;   // suppress default IE menu; the page ships contextmenu.js
    }
    HRESULT STDMETHODCALLTYPE GetHostInfo(DOCHOSTUIINFO* pInfo) override {
        if(pInfo){
            pInfo->cbSize=sizeof(DOCHOSTUIINFO);
            pInfo->dwFlags=DOCHOSTUIFLAG_NO3DBORDER|DOCHOSTUIFLAG_THEME;
            pInfo->dwDoubleClick=DOCHOSTUIDBLCLK_DEFAULT;
        }
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE ShowUI(DWORD,IOleInPlaceActiveObject*,IOleCommandTarget*,
            IOleInPlaceFrame*,IOleInPlaceUIWindow*) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE HideUI() override { return S_OK; }
    HRESULT STDMETHODCALLTYPE UpdateUI() override { return S_OK; }
    // NOTE: EnableModeless(BOOL) is shared with IOleInPlaceFrame — the single
    // implementation above (in the IOleInPlaceFrame section) satisfies both, so
    // it must NOT be re-declared here.
    HRESULT STDMETHODCALLTYPE OnDocWindowActivate(BOOL) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE OnFrameWindowActivate(BOOL) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE ResizeBorder(LPCRECT,IOleInPlaceUIWindow*,BOOL) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE TranslateAccelerator(LPMSG,const GUID*,DWORD) override {
        return S_FALSE;   // <-- key line: let the document handle the keystroke
    }
    HRESULT STDMETHODCALLTYPE GetOptionKeyPath(LPOLESTR* pchKey,DWORD) override {
        if(pchKey) *pchKey=NULL; return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetDropTarget(IDropTarget*,IDropTarget** ppDropTarget) override {
        if(ppDropTarget) *ppDropTarget=NULL; return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE GetExternal(IDispatch** ppDispatch) override {
        if(ppDispatch) *ppDispatch=NULL; return S_OK;
    }
    HRESULT STDMETHODCALLTYPE TranslateUrl(DWORD,LPWSTR,LPWSTR* ppchURLOut) override {
        if(ppchURLOut) *ppchURLOut=NULL; return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE FilterDataObject(IDataObject*,IDataObject** ppDORet) override {
        if(ppDORet) *ppDORet=NULL; return E_NOTIMPL;
    }
};

// per-mshtml-view record
struct MshtmlView {
    HWND host=nullptr;
    MshtmlHost* site=nullptr;
    IWebBrowser2* wb=nullptr;
    IOleObject* oleObj=nullptr;
    IOleInPlaceActiveObject* ipao=nullptr;   // for TranslateAccelerator routing
};
static std::map<HWND,MshtmlView*> g_mshtmlViews;
static std::mutex g_mshtmlMx;

static MshtmlView* mshtmlViewFor(HWND h){
    std::lock_guard<std::mutex> lk(g_mshtmlMx);
    auto it=g_mshtmlViews.find(h); return it==g_mshtmlViews.end()?nullptr:it->second;
}

static void mshtmlResize(HWND host){
    MshtmlView* v=mshtmlViewFor(host); if(!v||!v->oleObj) return;
    RECT rc; GetClientRect(host,&rc);
    IOleInPlaceObject* ipo=nullptr;
    if(SUCCEEDED(v->oleObj->QueryInterface(IID_IOleInPlaceObject,(void**)&ipo)) && ipo){
        ipo->SetObjectRects(&rc,&rc); ipo->Release();
    }
    if(v->wb){ v->wb->put_Width(rc.right); v->wb->put_Height(rc.bottom); }
}

// window proc for the mshtml host child window
static LRESULT CALLBACK mshtmlHostProc(HWND h, UINT m, WPARAM w, LPARAM l){
    switch(m){
    case WM_SIZE: mshtmlResize(h); return 0;
    case WM_ERASEBKGND: return 1;
    }
    return DefWindowProcW(h,m,w,l);
}

} // namespace

// ---------------------------------------------------------------------------
//  Create an embedded MSHTML WebBrowser view in `parent`, navigate to the
//  loopback admission page. Returns the host HWND or NULL.
// ---------------------------------------------------------------------------
static HWND MshtmlAdmission_CreateView(HWND parent){
    if(!parent) return nullptr;
    if(!WebAdmission_EnsureHost()) return nullptr;
    mshtmlSetBrowserEmulation();
    if(FAILED(OleInitialize(NULL))) { /* may already be init'd */ }

    static bool cls=false; static const wchar_t* CLS=L"AzMshtmlAdmHost";
    if(!cls){
        WNDCLASSW wc={}; wc.lpfnWndProc=mshtmlHostProc; wc.hInstance=g_hInst;
        wc.hCursor=LoadCursor(NULL,IDC_ARROW);
        wc.hbrBackground=(HBRUSH)(COLOR_WINDOW+1);
        wc.lpszClassName=CLS; RegisterClassW(&wc); cls=true;
    }
    RECT rc; GetClientRect(parent,&rc);
    HWND host=CreateWindowExW(0,CLS,L"",WS_CHILD|WS_VISIBLE|WS_CLIPCHILDREN,
        0,0,rc.right,rc.bottom,parent,NULL,g_hInst,NULL);
    if(!host) return nullptr;

    MshtmlView* v=new MshtmlView(); v->host=host;
    MshtmlHost* site=new MshtmlHost(host); v->site=site;

    IOleObject* oleObj=nullptr;
    HRESULT hr=CoCreateInstance(CLSID_WebBrowser,NULL,CLSCTX_INPROC_SERVER,
        IID_IOleObject,(void**)&oleObj);
    if(FAILED(hr)||!oleObj){
        DestroyWindow(host); site->Release(); delete v; return nullptr;
    }
    v->oleObj=oleObj; site->oleObj=oleObj;
    oleObj->SetClientSite(site);

    // Register our IDocHostUIHandler on the control so MSHTML consults it for
    // TranslateAccelerator (this is what routes Tab/Enter/Ctrl+A into the page).
    {
        ICustomDocLocal* cd=nullptr;
        if(SUCCEEDED(oleObj->QueryInterface(IID_ICustomDoc_local,(void**)&cd)) && cd){
            cd->SetUIHandler(static_cast<IDocHostUIHandler*>(site));
            cd->Release();
        }
    }

    // in-place activate
    RECT r2; GetClientRect(host,&r2);
    hr=oleObj->DoVerb(OLEIVERB_INPLACEACTIVATE,NULL,site,0,host,&r2);

    // Grab the in-place active object so the message pump can call
    // IOleInPlaceActiveObject::TranslateAccelerator on every WM_KEYDOWN.
    if(SUCCEEDED(hr)){
        IOleInPlaceActiveObject* ipao=nullptr;
        if(SUCCEEDED(oleObj->QueryInterface(IID_IOleInPlaceActiveObject,(void**)&ipao)) && ipao)
            v->ipao=ipao;
    }

    IWebBrowser2* wb=nullptr;
    if(SUCCEEDED(oleObj->QueryInterface(IID_IWebBrowser2,(void**)&wb)) && wb){
        v->wb=wb; site->wb=wb;
        // Suppress any legacy script-error dialogs (the old "Script Error / Do you
        // want to continue…" popups). With ES5-safe JS there should be none, but
        // this guarantees a clean, dialog-free surface no matter what.
        wb->put_Silent(VARIANT_TRUE);
        wb->put_Width(r2.right); wb->put_Height(r2.bottom);
        { std::lock_guard<std::mutex> lk(g_mshtmlMx); g_mshtmlViews[host]=v; }
        // Give the WebBrowser control keyboard focus so it owns the tab-stop
        // chain the moment the admission tab becomes visible.
        SetFocus(host);
        int port=WebAdmission_EnsureHost();
        wchar_t url[96]; swprintf(url,96,L"http://127.0.0.1:%d/index.html",port);
        VARIANT ve; VariantInit(&ve);
        BSTR burl=SysAllocString(url);
        wb->Navigate(burl,&ve,&ve,&ve,&ve);
        SysFreeString(burl);
        return host;
    }

    // failed to get IWebBrowser2
    oleObj->Release(); v->oleObj=nullptr;
    DestroyWindow(host); site->Release(); delete v;
    return nullptr;
}

static void MshtmlAdmission_Resize(HWND host, int, int){ mshtmlResize(host); }

static void MshtmlAdmission_Destroy(HWND host){
    MshtmlView* v=nullptr;
    { std::lock_guard<std::mutex> lk(g_mshtmlMx);
      auto it=g_mshtmlViews.find(host); if(it!=g_mshtmlViews.end()){ v=it->second; g_mshtmlViews.erase(it); } }
    if(!v) return;
    if(v->ipao){ v->ipao->Release(); v->ipao=nullptr; }
    if(v->wb){ v->wb->Stop(); v->wb->Release(); v->wb=nullptr; }
    if(v->oleObj){ v->oleObj->Close(OLECLOSE_NOSAVE); v->oleObj->SetClientSite(NULL); v->oleObj->Release(); v->oleObj=nullptr; }
    if(v->site) v->site->Release();
    if(IsWindow(host)) DestroyWindow(host);
    delete v;
}

// True — the MSHTML WebBrowser control is present on every Windows (and Wine).
static bool MshtmlAdmission_Available(){ return true; }

// ---------------------------------------------------------------------------
//  Route a WM_KEYDOWN (or any) message through the embedded WebBrowser control
//  BEFORE the main pump calls TranslateMessage/DispatchMessage. We only forward
//  the message to the view whose host HWND is an ancestor of pMsg->hwnd, so we
//  never blindly eat keystrokes destined for other windows. Returns true iff a
//  view consumed the message (TranslateAccelerator returned S_OK).
// ---------------------------------------------------------------------------
static bool MshtmlAdmission_TranslateAccel(MSG* pMsg){
    if(!pMsg) return false;
    IOleInPlaceActiveObject* ipao=nullptr;
    {
        std::lock_guard<std::mutex> lk(g_mshtmlMx);
        for(std::map<HWND,MshtmlView*>::iterator it=g_mshtmlViews.begin();
            it!=g_mshtmlViews.end(); ++it){
            MshtmlView* v=it->second;
            if(!v || !v->ipao || !v->host) continue;
            // is the target window inside this view's host subtree?
            HWND w=pMsg->hwnd;
            bool inside=false;
            while(w){
                if(w==v->host){ inside=true; break; }
                w=GetParent(w);
            }
            if(inside){ ipao=v->ipao; ipao->AddRef(); break; }
        }
    }
    if(!ipao) return false;
    HRESULT hr=ipao->TranslateAccelerator(pMsg);
    ipao->Release();
    return hr==S_OK;
}
