// ============================================================================
//  webhost_host.inc — OLE site + bridge IDispatch + loader + window proc.
//  Included into webhost.cpp.
// ============================================================================

// DISPID our bridge exposes to JS as window.external.call(verb, args).
#define DISPID_AZ_CALL  1
#define DISPID_AZ_LOG   2

// Forward decl of the host struct (defined below).
struct WebHost;
// Forward decl of lifecycle hooks implemented in webhost_run.inc.
static void WebHost_OnDocumentComplete(WebHost* host);

// ---------------------------------------------------------------------------
//  Bridge IDispatch — window.external in the page. Synchronous: JS calls
//  external.call("verb", jsonArgs) and gets back a JSON string. All validation
//  / persistence happens C++-side inside WebHostBridge_Call.
// ---------------------------------------------------------------------------
class BridgeDispatch : public IDispatch {
    LONG  m_ref;
    int   m_kind;
public:
    BridgeDispatch(int kind):m_ref(1),m_kind(kind){}
    virtual ~BridgeDispatch(){}
    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv){
        if(!ppv) return E_POINTER;
        if(riid==IID_IUnknown || riid==IID_IDispatch){ *ppv=this; AddRef(); return S_OK; }
        *ppv=NULL; return E_NOINTERFACE;
    }
    STDMETHODIMP_(ULONG) AddRef(){ return InterlockedIncrement(&m_ref); }
    STDMETHODIMP_(ULONG) Release(){ LONG r=InterlockedDecrement(&m_ref); if(!r) delete this; return r; }
    // IDispatch
    STDMETHODIMP GetTypeInfoCount(UINT* p){ if(p)*p=0; return S_OK; }
    STDMETHODIMP GetTypeInfo(UINT,LCID,ITypeInfo** p){ if(p)*p=NULL; return E_NOTIMPL; }
    STDMETHODIMP GetIDsOfNames(REFIID,LPOLESTR* names,UINT cNames,LCID,DISPID* ids){
        if(cNames<1 || !names || !ids) return E_INVALIDARG;
        HRESULT hr=S_OK;
        for(UINT i=0;i<cNames;i++){
            if(_wcsicmp(names[i],L"call")==0)      ids[i]=DISPID_AZ_CALL;
            else if(_wcsicmp(names[i],L"log")==0)  ids[i]=DISPID_AZ_LOG;
            else { ids[i]=DISPID_UNKNOWN; hr=DISP_E_UNKNOWNNAME; }
        }
        return hr;
    }
    STDMETHODIMP Invoke(DISPID id,REFIID,LCID,WORD flags,DISPPARAMS* dp,
                        VARIANT* res,EXCEPINFO*,UINT*){
        if(!(flags & DISPATCH_METHOD)) return DISP_E_MEMBERNOTFOUND;
        if(id==DISPID_AZ_LOG){
            if(dp && dp->cArgs>=1 && dp->rgvarg[0].vt==VT_BSTR)
                WebHost_LogError(L"js", dp->rgvarg[0].bstrVal);
            if(res){ VariantInit(res); }
            return S_OK;
        }
        if(id==DISPID_AZ_CALL){
            // args are pushed in REVERSE order: rgvarg[0]=args, rgvarg[1]=verb.
            std::wstring verb, args;
            if(dp){
                if(dp->cArgs>=2 && dp->rgvarg[1].vt==VT_BSTR) verb=dp->rgvarg[1].bstrVal;
                if(dp->cArgs>=1 && dp->rgvarg[0].vt==VT_BSTR) args=dp->rgvarg[0].bstrVal;
            }
            std::wstring out;
            // Bridge calls must NEVER throw across the COM boundary. A C++
            // exception escaping here would surface to JS as an empty result
            // (`{}`), which the page reports as the generic «خطا در ثبت». We
            // therefore contain every exception and return a well-formed JSON
            // error so the operator sees the real reason instead.
            try {
                out = WebHostBridge_Call(m_kind, verb, args);
            } catch(...){
                // Any C++ exception (std or otherwise) is contained and reported
                // as a clean JSON error so JS never receives an empty result.
                WebHost_LogError(L"verb-exc", verb.c_str());
                out = L"{\"ok\":false,\"error\":\"\\u062e\\u0637\\u0627\\u06cc \\u062f\\u0627\\u062e\\u0644\\u06cc \\u062f\\u0631 \\u062b\\u0628\\u062a\"}";
            }
            if(res){
                VariantInit(res);
                res->vt=VT_BSTR;
                res->bstrVal=SysAllocStringLen(out.c_str(),(UINT)out.size());
            }
            return S_OK;
        }
        return DISP_E_MEMBERNOTFOUND;
    }
};

// ---------------------------------------------------------------------------
//  DocHostUIHandler — disables the default context menu / 3D border and points
//  GetExternal at our bridge so window.external resolves to it.
// ---------------------------------------------------------------------------
class DocHostUI : public IDocHostUIHandler {
    LONG m_ref; IDispatch* m_external;
public:
    DocHostUI(IDispatch* ext):m_ref(1),m_external(ext){ if(m_external) m_external->AddRef(); }
    virtual ~DocHostUI(){ if(m_external) m_external->Release(); }
    STDMETHODIMP QueryInterface(REFIID riid,void** ppv){
        if(!ppv) return E_POINTER;
        if(riid==IID_IUnknown || riid==IID_IDocHostUIHandler){ *ppv=this; AddRef(); return S_OK; }
        *ppv=NULL; return E_NOINTERFACE;
    }
    STDMETHODIMP_(ULONG) AddRef(){ return InterlockedIncrement(&m_ref); }
    STDMETHODIMP_(ULONG) Release(){ LONG r=InterlockedDecrement(&m_ref); if(!r) delete this; return r; }
    STDMETHODIMP ShowContextMenu(DWORD,POINT*,IUnknown*,IDispatch*){ return S_OK; /* suppress */ }
    STDMETHODIMP GetHostInfo(DOCHOSTUIINFO* info){
        if(!info) return E_POINTER;
        info->cbSize=sizeof(DOCHOSTUIINFO);
        info->dwFlags=DOCHOSTUIFLAG_NO3DBORDER|DOCHOSTUIFLAG_FLAT_SCROLLBAR|
                      DOCHOSTUIFLAG_DPI_AWARE|DOCHOSTUIFLAG_THEME;
        info->dwDoubleClick=DOCHOSTUIDBLCLK_DEFAULT;
        return S_OK;
    }
    STDMETHODIMP ShowUI(DWORD,IOleInPlaceActiveObject*,IOleCommandTarget*,
                        IOleInPlaceFrame*,IOleInPlaceUIWindow*){ return S_FALSE; }
    STDMETHODIMP HideUI(){ return S_OK; }
    STDMETHODIMP UpdateUI(){ return S_OK; }
    STDMETHODIMP EnableModeless(BOOL){ return S_OK; }
    STDMETHODIMP OnDocWindowActivate(BOOL){ return S_OK; }
    STDMETHODIMP OnFrameWindowActivate(BOOL){ return S_OK; }
    STDMETHODIMP ResizeBorder(LPCRECT,IOleInPlaceUIWindow*,BOOL){ return S_OK; }
    STDMETHODIMP TranslateAccelerator(LPMSG,const GUID*,DWORD){ return S_FALSE; }
    STDMETHODIMP GetOptionKeyPath(LPOLESTR* p,DWORD){ if(p)*p=NULL; return S_FALSE; }
    STDMETHODIMP GetDropTarget(IDropTarget*,IDropTarget** p){ if(p)*p=NULL; return E_NOTIMPL; }
    STDMETHODIMP GetExternal(IDispatch** ppDisp){
        if(!ppDisp) return E_POINTER;
        *ppDisp=m_external;
        if(m_external){ m_external->AddRef(); return S_OK; }
        return S_FALSE;
    }
    STDMETHODIMP TranslateUrl(DWORD,LPWSTR,LPWSTR* p){ if(p)*p=NULL; return S_FALSE; }
    STDMETHODIMP FilterDataObject(IDataObject*,IDataObject** p){ if(p)*p=NULL; return S_FALSE; }
};

// ---------------------------------------------------------------------------
//  Minimal OLE client site that also implements the in-place + UI handler +
//  DWebBrowserEvents2 sink (for DocumentComplete). One object, many interfaces.
// ---------------------------------------------------------------------------
struct WebHost {
    HWND          hwnd       = NULL;   // the host child window
    HWND          hBrowser   = NULL;   // OLE in-place window of the browser
    int           kind       = WH_RECEPTION;
    IWebBrowser2* web        = NULL;
    IOleObject*   oleObj     = NULL;
    IConnectionPoint* cp     = NULL;
    DWORD         cookie     = 0;
    DocHostUI*    docUI      = NULL;
    BridgeDispatch* bridge   = NULL;
    bool          ready      = false;  // DocumentComplete fired + boot pushed
    // loader state
    bool          showLoader = true;
    int           loaderPct  = 0;      // 0..100 progress
    UINT_PTR      loaderTimer= 0;
    int           loaderTicks= 0;      // §4: timer ticks since create (deadline watchdog)
    bool          docCompleted=false;  // §4: real DocumentComplete observed (vs. forced)
    bool          forcedReveal=false;  // §4: revealed by watchdog rather than event
};

class HostSite :
    public IOleClientSite,
    public IOleInPlaceSite,
    public IOleInPlaceFrame,
    public IDispatch          // DWebBrowserEvents2 sink
{
    LONG     m_ref;
    WebHost* m_host;
public:
    HostSite(WebHost* h):m_ref(1),m_host(h){}
    virtual ~HostSite(){}
    // ---- IUnknown ----
    STDMETHODIMP QueryInterface(REFIID riid,void** ppv){
        if(!ppv) return E_POINTER;
        if(riid==IID_IUnknown || riid==IID_IOleClientSite){ *ppv=(IOleClientSite*)this; }
        else if(riid==IID_IOleInPlaceSite){ *ppv=(IOleInPlaceSite*)this; }
        else if(riid==IID_IOleInPlaceFrame){ *ppv=(IOleInPlaceFrame*)this; }
        else if(riid==IID_IOleWindow){ *ppv=(IOleInPlaceFrame*)this; }
        else if(riid==IID_IDispatch || riid==DIID_DWebBrowserEvents2_local){ *ppv=(IDispatch*)this; }
        else { *ppv=NULL; return E_NOINTERFACE; }
        AddRef(); return S_OK;
    }
    STDMETHODIMP_(ULONG) AddRef(){ return InterlockedIncrement(&m_ref); }
    STDMETHODIMP_(ULONG) Release(){ LONG r=InterlockedDecrement(&m_ref); if(!r) delete this; return r; }
    // ---- IOleClientSite ----
    STDMETHODIMP SaveObject(){ return S_OK; }
    STDMETHODIMP GetMoniker(DWORD,DWORD,IMoniker** p){ if(p)*p=NULL; return E_NOTIMPL; }
    STDMETHODIMP GetContainer(IOleContainer** p){ if(p)*p=NULL; return E_NOINTERFACE; }
    STDMETHODIMP ShowObject(){ return S_OK; }
    STDMETHODIMP OnShowWindow(BOOL){ return S_OK; }
    STDMETHODIMP RequestNewObjectLayout(){ return E_NOTIMPL; }
    // ---- IOleWindow / IOleInPlaceFrame ----
    STDMETHODIMP GetWindow(HWND* p){ if(!p) return E_POINTER; *p=m_host?m_host->hwnd:NULL; return S_OK; }
    STDMETHODIMP ContextSensitiveHelp(BOOL){ return S_OK; }
    STDMETHODIMP GetBorder(LPRECT){ return E_NOTIMPL; }
    STDMETHODIMP RequestBorderSpace(LPCBORDERWIDTHS){ return E_NOTIMPL; }
    STDMETHODIMP SetBorderSpace(LPCBORDERWIDTHS){ return E_NOTIMPL; }
    STDMETHODIMP SetActiveObject(IOleInPlaceActiveObject*,LPCOLESTR){ return S_OK; }
    STDMETHODIMP InsertMenus(HMENU,LPOLEMENUGROUPWIDTHS){ return E_NOTIMPL; }
    STDMETHODIMP SetMenu(HMENU,HOLEMENU,HWND){ return E_NOTIMPL; }
    STDMETHODIMP RemoveMenus(HMENU){ return E_NOTIMPL; }
    STDMETHODIMP SetStatusText(LPCOLESTR){ return S_OK; }
    STDMETHODIMP EnableModeless(BOOL){ return S_OK; }
    STDMETHODIMP TranslateAccelerator(LPMSG,WORD){ return S_FALSE; }
    // ---- IOleInPlaceSite ----
    STDMETHODIMP CanInPlaceActivate(){ return S_OK; }
    STDMETHODIMP OnInPlaceActivate(){ return S_OK; }
    STDMETHODIMP OnUIActivate(){ return S_OK; }
    STDMETHODIMP GetWindowContext(IOleInPlaceFrame** ppFrame,
            IOleInPlaceUIWindow** ppDoc, LPRECT lprcPosRect,
            LPRECT lprcClipRect, LPOLEINPLACEFRAMEINFO info){
        if(ppFrame){ *ppFrame=(IOleInPlaceFrame*)this; AddRef(); }
        if(ppDoc) *ppDoc=NULL;
        RECT rc; GetClientRect(m_host?m_host->hwnd:NULL,&rc);
        if(lprcPosRect)  *lprcPosRect=rc;
        if(lprcClipRect) *lprcClipRect=rc;
        if(info){ info->cb=sizeof(OLEINPLACEFRAMEINFO); info->fMDIApp=FALSE;
                  info->hwndFrame=m_host?m_host->hwnd:NULL; info->haccel=NULL; info->cAccelEntries=0; }
        return S_OK;
    }
    STDMETHODIMP Scroll(SIZE){ return S_OK; }
    STDMETHODIMP OnUIDeactivate(BOOL){ return S_OK; }
    STDMETHODIMP OnInPlaceDeactivate(){ return S_OK; }
    STDMETHODIMP DiscardUndoState(){ return S_OK; }
    STDMETHODIMP DeactivateAndUndo(){ return S_OK; }
    STDMETHODIMP OnPosRectChange(LPCRECT){ return S_OK; }
    // ---- IDispatch (DWebBrowserEvents2 sink) ----
    STDMETHODIMP GetTypeInfoCount(UINT* p){ if(p)*p=0; return S_OK; }
    STDMETHODIMP GetTypeInfo(UINT,LCID,ITypeInfo** p){ if(p)*p=NULL; return E_NOTIMPL; }
    STDMETHODIMP GetIDsOfNames(REFIID,LPOLESTR*,UINT,LCID,DISPID*){ return E_NOTIMPL; }
    STDMETHODIMP Invoke(DISPID id,REFIID,LCID,WORD,DISPPARAMS*,VARIANT*,EXCEPINFO*,UINT*){
        if(id==DISPID_DOCUMENTCOMPLETE && m_host){
            WebHost_OnDocumentComplete(m_host);
        }
        return S_OK;
    }
};

// ---------------------------------------------------------------------------
//  loader overlay (native GDI) — centered card with a determinate progress bar.
//  Clean & minimal; no internal debug detail is exposed to the user.
// ---------------------------------------------------------------------------
static void WebHost_PaintLoader(WebHost* wh, HDC dc0){
    RECT rc; GetClientRect(wh->hwnd,&rc);
    uikit::MemDC mem(dc0,rc.right,rc.bottom);
    HBRUSH bg=CreateSolidBrush(g_theme.bg); FillRect(mem.dc,&rc,bg); DeleteObject(bg);

    int cardW=S(320), cardH=S(150);
    int cx=rc.right/2, cy=rc.bottom/2;
    RECT card={cx-cardW/2, cy-cardH/2, cx+cardW/2, cy+cardH/2};
    gpShadow(mem.dc,card,S(16),S(10),60);
    gpGradRoundRectBg(mem.dc,card,S(16),g_theme.surfaceTop,g_theme.surface,
                      g_theme.border,g_theme.bg);

    // spinner-free, just the title + a determinate bar (minimal, clean).
    SetBkMode(mem.dc,TRANSPARENT);
    SelectObject(mem.dc,g_fUIB); SetTextColor(mem.dc,g_theme.text);
    RECT tr={card.left,card.top+S(26),card.right,card.top+S(26)+S(26)};
    DrawTextW(mem.dc,
        L"\u062f\u0631 \u062d\u0627\u0644 \u0628\u0627\u0631\u06af\u0630\u0627\u0631\u06cc\u2026",  // «در حال بارگذاری…»
        -1,&tr,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);

    // progress track
    int barW=cardW-S(56), barH=S(8);
    RECT track={cx-barW/2, card.bottom-S(48), cx+barW/2, card.bottom-S(48)+barH};
    gpRoundRectBg(mem.dc,track,barH/2,blendColor(g_theme.surface,g_theme.border,60),
                  g_theme.border,g_theme.surface);
    int pct=wh->loaderPct; if(pct<6) pct=6; if(pct>100) pct=100;
    int fillW=(track.right-track.left)*pct/100;
    if(fillW>0){
        RECT fill={track.left,track.top,track.left+fillW,track.bottom};
        gpGradRoundRectBg(mem.dc,fill,barH/2,g_theme.accent,g_theme.accent2,
                          g_theme.accent,g_theme.surface);
    }
    mem.blitTo(dc0);
}

// resize the OLE in-place window to fill the host client area.
static void WebHost_LayoutBrowser(WebHost* wh){
    if(!wh || !wh->web) return;
    RECT rc; GetClientRect(wh->hwnd,&rc);
    wh->web->put_Left(0);
    wh->web->put_Top(0);
    wh->web->put_Width(rc.right);
    wh->web->put_Height(rc.bottom);
    if(wh->hBrowser && IsWindow(wh->hBrowser))
        MoveWindow(wh->hBrowser,0,0,rc.right,rc.bottom,TRUE);
}

#include "webhost_run.inc"
