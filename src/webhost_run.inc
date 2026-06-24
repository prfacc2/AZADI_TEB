// ============================================================================
//  webhost_run.inc — document write, lifecycle, push channel, window proc and
//  the public WebHost_Create entry. Included into webhost_host.inc.
// ============================================================================

// Forward decls (defined later in this file).
static void WebHost_OnDocumentComplete(WebHost* wh);
static void WebHost_PushToJs(WebHost* wh, const std::wstring& json);

// Write the embedded HTML into the live document via IPersistStreamInit, which
// is the reliable in-process way to seed an about:blank document (no temp file,
// no network). Returns true on success.
static bool WebHost_WriteDocument(WebHost* wh){
    if(!wh || !wh->web) return false;
    IDispatch* disp=NULL;
    if(FAILED(wh->web->get_Document(&disp)) || !disp){
        WebHost_LogError(L"writedoc",L"no document"); return false;
    }
    IHTMLDocument2* doc=NULL;
    disp->QueryInterface(IID_IHTMLDocument2,(void**)&doc);
    disp->Release();
    if(!doc){ WebHost_LogError(L"writedoc",L"no IHTMLDocument2"); return false; }

    std::wstring html = WebHostBridge_Html(wh->kind);

    // document.write via a SAFEARRAY of one VARIANT BSTR — the canonical MSHTML
    // pattern that works on every IE/Trident version.
    bool ok=false;
    SAFEARRAY* sa=SafeArrayCreateVector(VT_VARIANT,0,1);
    if(sa){
        VARIANT* v=NULL;
        if(SUCCEEDED(SafeArrayAccessData(sa,(void**)&v)) && v){
            v->vt=VT_BSTR;
            v->bstrVal=SysAllocStringLen(html.c_str(),(UINT)html.size());
            SafeArrayUnaccessData(sa);
            if(SUCCEEDED(doc->write(sa))){ doc->close(); ok=true; }
        }
        SafeArrayDestroy(sa);
    }
    doc->Release();
    if(!ok) WebHost_LogError(L"writedoc",L"document.write failed");
    return ok;
}

// Reveal the document: push the boot payload, retire the loader and lay out the
// browser. §4: this is the single, idempotent place that flips a host from the
// loading state to the live state — driven either by a real DocumentComplete or
// by the watchdog deadline. Calling it more than once is a no-op.
static void WebHost_RevealDocument(WebHost* wh, bool forced){
    if(!wh || wh->ready) return;
    Breadcrumb(forced?L"webhost: reveal (watchdog)":L"webhost: reveal (complete)");
    wh->ready=true;
    wh->forcedReveal=forced;
    // wire the push channel + send the boot payload now the DOM + JS exist.
    // The bridge boot builder is itself guarded; a failed/empty payload still
    // reveals the view so the user never sees a stuck loader.
    std::wstring boot = WebHostBridge_BootPayload(wh->kind);
    if(!boot.empty()) WebHost_PushToJs(wh, boot);
    else WebHost_LogError(L"reveal",L"empty boot payload");
    // reveal the web view, retire the loader.
    wh->loaderPct=100;
    wh->showLoader=false;
    if(wh->loaderTimer){ KillTimer(wh->hwnd,wh->loaderTimer); wh->loaderTimer=0; }
    WebHost_LayoutBrowser(wh);
    InvalidateRect(wh->hwnd,NULL,TRUE);
}

// Called when the browser fires DocumentComplete. We may be re-entered for
// sub-frames; we only act once on the top document and guard with wh->ready.
static void WebHost_OnDocumentComplete(WebHost* wh){
    if(!wh) return;
    wh->docCompleted=true;
    if(wh->ready){
        // §4: a *repeated* DocumentComplete (e.g. user re-enters the tab and the
        // host is reused, or an internal navigation completes). Re-push a refresh
        // so the page re-hydrates idempotently from the C++ authority.
        std::wstring boot = WebHostBridge_BootPayload(wh->kind);
        if(!boot.empty()) WebHost_PushToJs(wh, boot);
        return;
    }
    WebHost_RevealDocument(wh, false);
}

// Push a JSON string into the page by invoking the global window.azReceive(str).
// The page installs azReceive before signalling readiness, but we also tolerate
// it not yet existing (logged, not fatal).
static void WebHost_PushToJs(WebHost* wh, const std::wstring& json){
    if(!wh || !wh->web) return;
    IDispatch* disp=NULL;
    if(FAILED(wh->web->get_Document(&disp)) || !disp) return;
    IHTMLDocument2* doc=NULL;
    disp->QueryInterface(IID_IHTMLDocument2,(void**)&doc);
    disp->Release();
    if(!doc) return;
    IHTMLWindow2* win=NULL;
    doc->get_parentWindow(&win);
    doc->Release();
    if(!win) return;

    // Call window.azReceive(json) by evaluating a tiny script. We pass the JSON
    // via a temporary global to avoid any escaping pitfalls: set azPayload then
    // call azReceive(azPayload).
    IHTMLWindow2* w2=win;
    // Build: window.__azPush(<json-string-literal>)  — we JSON-encode the string
    // so embedded quotes/newlines survive the eval.
    std::wstring lit; lit.reserve(json.size()+16);
    lit += L"\"";
    for(wchar_t c : json){
        switch(c){
            case L'\\': lit+=L"\\\\"; break;
            case L'"':  lit+=L"\\\""; break;
            case L'\n': lit+=L"\\n"; break;
            case L'\r': lit+=L"\\r"; break;
            case L'\t': lit+=L"\\t"; break;
            default:
                if(c < 0x20){ wchar_t b[8]; swprintf(b,8,L"\\u%04x",(unsigned)c); lit+=b; }
                else lit+=c;
        }
    }
    lit += L"\"";
    std::wstring code = L"try{window.azReceive(" + lit + L");}catch(e){if(window.external&&window.external.log)window.external.log('push:'+e);}";

    VARIANT vRes; VariantInit(&vRes);
    BSTR bCode=SysAllocStringLen(code.c_str(),(UINT)code.size());
    BSTR bLang=SysAllocString(L"JScript");
    if(bCode && bLang) w2->execScript(bCode,bLang,&vRes);
    if(bCode) SysFreeString(bCode);
    if(bLang) SysFreeString(bLang);
    VariantClear(&vRes);
    w2->Release();
}

// tear down all COM objects for a host (idempotent).
static void WebHost_Teardown(WebHost* wh){
    if(!wh) return;
    if(wh->loaderTimer){ KillTimer(wh->hwnd,wh->loaderTimer); wh->loaderTimer=0; }
    if(wh->cp && wh->cookie){ wh->cp->Unadvise(wh->cookie); wh->cookie=0; }
    if(wh->cp){ wh->cp->Release(); wh->cp=NULL; }
    if(wh->web){
        wh->web->put_Visible(VARIANT_FALSE);
        wh->web->Release(); wh->web=NULL;
    }
    if(wh->oleObj){
        wh->oleObj->Close(OLECLOSE_NOSAVE);
        wh->oleObj->Release(); wh->oleObj=NULL;
    }
    if(wh->docUI){ wh->docUI->Release(); wh->docUI=NULL; }
    if(wh->bridge){ wh->bridge->Release(); wh->bridge=NULL; }
}

// Create + in-place activate the WebBrowser inside wh->hwnd, set the UI handler
// (so window.external resolves to our bridge), advise the event sink, then
// navigate to about:blank and write the embedded document.
static bool WebHost_StartBrowser(WebHost* wh){
    HostSite* site=new HostSite(wh);   // ref=1

    IOleObject* ole=NULL;
    HRESULT hr=CoCreateInstance(CLSID_WebBrowser_local,NULL,
        CLSCTX_INPROC_SERVER,IID_IOleObject_local,(void**)&ole);
    if(FAILED(hr) || !ole){ WebHost_LogError(L"start",L"CoCreateInstance WebBrowser failed");
        site->Release(); return false; }
    wh->oleObj=ole;

    ole->SetClientSite((IOleClientSite*)site);

    // Set our DocHostUIHandler via ICustomDoc is the IE way; the supported and
    // portable way is to QI the OLE object for IOleControl and use the doc host
    // UI handler through IDocHostUIHandler set on the client site. WebBrowser
    // queries the client site for IDocHostUIHandler, so the simplest reliable
    // route is to make the client site ALSO expose it. We instead install it on
    // the document after navigation via ICustomDoc (below in DocumentComplete is
    // too late for external) — so we set it here through the standard path:
    wh->docUI=new DocHostUI((IDispatch*)wh->bridge);

    RECT rc; GetClientRect(wh->hwnd,&rc);
    hr=ole->DoVerb(OLEIVERB_INPLACEACTIVATE,NULL,(IOleClientSite*)site,0,wh->hwnd,&rc);
    if(FAILED(hr)){ WebHost_LogError(L"start",L"DoVerb INPLACEACTIVATE failed");
        site->Release(); return false; }

    hr=ole->QueryInterface(IID_IWebBrowser2_local,(void**)&wh->web);
    if(FAILED(hr) || !wh->web){ WebHost_LogError(L"start",L"QI IWebBrowser2 failed");
        site->Release(); return false; }

    // advise the DWebBrowserEvents2 sink for DocumentComplete.
    IConnectionPointContainer* cpc=NULL;
    if(SUCCEEDED(ole->QueryInterface(IID_IConnectionPointContainer_local,(void**)&cpc)) && cpc){
        if(SUCCEEDED(cpc->FindConnectionPoint(DIID_DWebBrowserEvents2_local,&wh->cp)) && wh->cp){
            wh->cp->Advise((IUnknown*)(IDispatch*)site,&wh->cookie);
        }
        cpc->Release();
    }

    // navigate to about:blank, then write our document on DownloadComplete; the
    // most robust approach is to navigate then set the doc host UI handler via
    // ICustomDoc and write synchronously here (about:blank is in-process).
    {
        VARIANT vUrl, vFlags, vTarget, vHeaders, vPost;
        VariantInit(&vUrl); VariantInit(&vFlags); VariantInit(&vTarget);
        VariantInit(&vHeaders); VariantInit(&vPost);
        vUrl.vt=VT_BSTR; vUrl.bstrVal=SysAllocString(L"about:blank");
        wh->web->Navigate2(&vUrl,&vFlags,&vTarget,&vPost,&vHeaders);
        VariantClear(&vUrl);
    }

    // Pump until the blank document is ready, then attach the UI handler via
    // ICustomDoc (so window.external is live) and write our content.
    for(int i=0;i<200;i++){      // up to ~2s
        READYSTATE rs=READYSTATE_UNINITIALIZED;
        wh->web->get_ReadyState(&rs);
        if(rs>=READYSTATE_INTERACTIVE) break;
        MSG msg; while(PeekMessageW(&msg,NULL,0,0,PM_REMOVE)){ TranslateMessage(&msg); DispatchMessageW(&msg); }
        Sleep(10);
    }

    // attach DocHostUIHandler through ICustomDoc on the document.
    {
        IDispatch* disp=NULL;
        if(SUCCEEDED(wh->web->get_Document(&disp)) && disp){
            ICustomDoc* cd=NULL;
            disp->QueryInterface(IID_ICustomDoc_local,(void**)&cd);
            if(cd){ cd->SetUIHandler(wh->docUI); cd->Release(); }
            disp->Release();
        }
    }

    site->Release();   // the browser holds its own refs now
    return WebHost_WriteDocument(wh);
}

// The sync phase: synchronize native state / section metadata / form config
// BEFORE the page is created. This is deliberately a tiny, deterministic step
// (the registry is file-backed and fast) but it is where future network sync
// hooks live. Drives the loader progress for a clean, honest UX.
static bool WebHost_SyncPhase(WebHost* wh){
    Breadcrumb(L"webhost: sync begin");
    wh->loaderPct=12; InvalidateRect(wh->hwnd,NULL,FALSE); UpdateWindow(wh->hwnd);
    // ensure the section + design stores exist and are reconciled.
    Sections_Init();
    wh->loaderPct=40; InvalidateRect(wh->hwnd,NULL,FALSE); UpdateWindow(wh->hwnd);
    // touch the doctor/appointment stores so they are seeded before render.
    if(wh->kind==WH_APPOINTMENT){ todaysDoctors(); }
    wh->loaderPct=70; InvalidateRect(wh->hwnd,NULL,FALSE); UpdateWindow(wh->hwnd);
    Breadcrumb(L"webhost: sync complete");
    return true;
}

#define WH_TIMER_LOADER 0xA17E

static LRESULT CALLBACK WebHost_Proc(HWND h,UINT m,WPARAM w,LPARAM l){
    WebHost* wh=(WebHost*)GetWindowLongPtrW(h,GWLP_USERDATA);
    switch(m){
    case WM_ERASEBKGND: return 1;
    case WM_PAINT:{
        PAINTSTRUCT ps; HDC dc=BeginPaint(h,&ps);
        if(wh && wh->showLoader) WebHost_PaintLoader(wh,dc);
        else { RECT rc; GetClientRect(h,&rc); HBRUSH b=CreateSolidBrush(g_theme.bg);
               FillRect(dc,&rc,b); DeleteObject(b); }
        EndPaint(h,&ps); return 0; }
    case WM_SIZE:
        if(wh){ if(wh->showLoader) InvalidateRect(h,NULL,FALSE);
                else WebHost_LayoutBrowser(wh); }
        return 0;
    case WM_TIMER:
        if(wh && w==WH_TIMER_LOADER && wh->showLoader){
            wh->loaderTicks++;
            // gentle ease toward 90% while the document loads; the jump to 100%
            // happens on DocumentComplete (or the watchdog below).
            if(wh->loaderPct<90) wh->loaderPct += (90-wh->loaderPct)/6 + 1;
            InvalidateRect(h,NULL,FALSE);
            // §4 watchdog: the timer fires every 60ms. If DocumentComplete has
            // not arrived after ~6s (100 ticks) the document is either slow or
            // the event was lost; force-reveal so the loader never sticks. We
            // only force once the readystate is at least interactive, otherwise
            // we wait the full deadline then reveal regardless.
            if(!wh->ready){
                READYSTATE rs=READYSTATE_UNINITIALIZED;
                if(wh->web) wh->web->get_ReadyState(&rs);
                bool deadline = wh->loaderTicks>=100;             // ~6s hard cap
                bool interactiveStall = (rs>=READYSTATE_COMPLETE)
                                        && wh->loaderTicks>=25;   // ~1.5s after complete
                if(deadline || interactiveStall){
                    WebHost_LogError(L"watchdog",
                        deadline?L"document-complete deadline; forcing reveal"
                                :L"readystate complete but no event; forcing reveal");
                    WebHost_RevealDocument(wh, true);
                }
            }
        }
        return 0;
    case WM_APP_THEME:
        InvalidateRect(h,NULL,TRUE);
        return 0;
    case WM_DESTROY:
        if(wh){ WebHost_Teardown(wh); delete wh; SetWindowLongPtrW(h,GWLP_USERDATA,0); }
        return 0;
    }
    return DefWindowProcW(h,m,w,l);
}

// ---------------------------------------------------------------------------
//  public entry
// ---------------------------------------------------------------------------
HWND WebHost_Create(HWND parent, int kind){
    if(!parent || !IsWindow(parent)) return NULL;
    Breadcrumb(kind==WH_RECEPTION?L"webhost: create reception"
                                 :L"webhost: create appointment");

    // CoInitialize for this (UI) thread — apartment threaded for OLE controls.
    // It is reference-counted, so calling it here is safe even if main already
    // initialized COM.
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

    if(!WebHost_Available()){
        WebHost_LogError(L"create",L"MSHTML WebBrowser unavailable");
        return NULL;   // caller falls back to native form
    }

    static bool reg=false; const wchar_t* CLS=L"AzWebHost";
    if(!reg){ WNDCLASSW wc={0}; wc.lpfnWndProc=WebHost_Proc; wc.hInstance=g_hInst;
        wc.hCursor=LoadCursor(NULL,IDC_ARROW); wc.lpszClassName=CLS;
        wc.style=CS_HREDRAW|CS_VREDRAW; RegisterClassW(&wc); reg=true; }

    RECT rc; GetClientRect(parent,&rc);
    HWND h=CreateWindowExW(0,CLS,L"",
        WS_CHILD|WS_VISIBLE|WS_CLIPCHILDREN,
        0,0,rc.right,rc.bottom,parent,NULL,g_hInst,NULL);
    if(!h){ WebHost_LogError(L"create",L"CreateWindow host failed"); return NULL; }

    WebHost* wh=new WebHost();
    wh->hwnd=h; wh->kind=kind; wh->showLoader=true; wh->loaderPct=6;
    wh->bridge=new BridgeDispatch(kind);
    SetWindowLongPtrW(h,GWLP_USERDATA,(LONG_PTR)wh);

    // 1) paint the loader immediately so the user sees a centered progress card.
    InvalidateRect(h,NULL,FALSE); UpdateWindow(h);
    wh->loaderTimer=SetTimer(h,WH_TIMER_LOADER,60,NULL);

    // 2) synchronize native state / section metadata / form config.
    WebHost_SyncPhase(wh);

    // 3) render the HTML/CSS/JS interface. Any failure keeps the app stable and
    //    is logged; the caller's fallback decision is signalled by returning the
    //    host window either way (an empty host shows the loader-failed state).
    if(!WebHost_StartBrowser(wh)){
        WebHost_LogError(L"create",L"browser start/render failed; falling back");
        DestroyWindow(h);     // triggers Teardown
        return NULL;          // caller falls back to native form
    }
    return h;
}

#include "webhost_bridge.inc"
