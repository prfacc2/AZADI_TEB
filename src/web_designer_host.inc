// ============================================================================
//  web_designer_host.inc — opens the HTML/CSS/JS print designer (served by the
//  loopback HTTP host, see web_designer_http.inc) and keeps the app in sync.
//  Included into web_designer.cpp.
//
//  §1.19.1 — the previous embedded WebBrowser (Trident) host was REMOVED: on
//  many machines it produced a blank/white screen and a "not responding" hang
//  (Trident's synchronous XHR pumped against the same UI thread as the modal
//  message loop). The designer is now opened in the operator's DEFAULT BROWSER
//  via ShellExecute on the loopback URL. This is:
//    • crash-proof / hang-proof — the browser runs in its own process;
//    • universal — every Windows has a default browser, no WebView2 runtime;
//    • lightweight — nothing heavy is loaded inside our process;
//    • fully synced with C++ — the page saves through the /api/save bridge,
//      which persists the design AND posts WM_APP_DESIGN_PUSHED so reception
//      picks up the new layout immediately (no extra plumbing here).
//
//  While the browser is open we show a small, native "designer session" panel
//  so the operator can return focus / finish the session deliberately. The
//  panel auto-closes when the page signals exit (sets g_wdExitFlag via
//  /api/exit) or when a design is saved and the operator dismisses it.
// ============================================================================
namespace {

static const wchar_t* WD_CLS = L"AzWebDesignerWait";

struct WaitState {
    HWND owner = nullptr;
    std::wstring url;
    bool savedAtLeastOnce = false;
};

// poll: reflect save state + close automatically if the page asked to exit.
static VOID CALLBACK wdExitPoll(HWND h, UINT, UINT_PTR id, DWORD){
    // refresh the "saved" indicator so the operator gets clear feedback that the
    // design was persisted & applied to the section (no guessing).
    bool saved=false; wdLock(); saved=g_wdHaveSaved; wdUnlock();
    HWND lbl=GetDlgItem(h,200);
    if(lbl){
        static bool lastSaved=false;
        if(saved!=lastSaved || saved){
            lastSaved=saved;
            SetWindowTextW(lbl, saved
                ? L"\u2714 \u0637\u0631\u062d \u0630\u062e\u06cc\u0631\u0647 \u0648 \u0628\u0631 \u0628\u062e\u0634 \u0627\u0639\u0645\u0627\u0644 \u0634\u062f."
                : L"");
        }
    }
    if(InterlockedCompareExchange(&g_wdExitFlag,0,0)){
        KillTimer(h,id);
        DestroyWindow(h);
    }
}

static void wdOpenInBrowser(const std::wstring& url){
    // ShellExecute on an http URL launches the user's default browser. We do
    // NOT shell to a specific browser so it works on locked-down machines.
    ShellExecuteW(NULL, L"open", url.c_str(), NULL, NULL, SW_SHOWNORMAL);
}

static LRESULT CALLBACK WdWaitProc(HWND h, UINT m, WPARAM w, LPARAM l){
    WaitState* st = (WaitState*)GetWindowLongPtrW(h, GWLP_USERDATA);
    switch(m){
    case WM_COMMAND:{
        int id=LOWORD(w);
        if(id==101){ // «باز کردن دوباره در مرورگر»
            if(st) wdOpenInBrowser(st->url);
            return 0;
        }
        if(id==102){ // «پایان طراحی»
            InterlockedExchange(&g_wdExitFlag,1);
            DestroyWindow(h);
            return 0;
        }
        return 0; }
    case WM_CLOSE: DestroyWindow(h); return 0;
    case WM_DESTROY: return 0;
    }
    return DefWindowProcW(h,m,w,l);
}

} // namespace

bool WebDesigner_WebViewAvailable(){
    // No embedded engine is required any more; the system browser is always
    // available. Returning true keeps the public contract intact.
    return true;
}

bool WebDesigner_Open(HWND owner, const std::vector<int>& sectionIds){
    int sid = sectionIds.empty()?0:sectionIds[0];
    std::wstring url = WebDesigner_Url(sid);
    if(url.empty()) return false;            // host failed → caller falls back

    // launch the page in the default browser
    wdOpenInBrowser(url);

    // register the small native "session" window class once
    static bool reg=false;
    if(!reg){
        WNDCLASSW wc={};
        wc.lpfnWndProc=WdWaitProc; wc.hInstance=g_hInst;
        wc.hCursor=LoadCursor(NULL,IDC_ARROW);
        wc.hbrBackground=(HBRUSH)(COLOR_BTNFACE+1);
        wc.lpszClassName=WD_CLS;
        if(!RegisterClassW(&wc)){ return true; } // browser already open; treat as handled
        reg=true;
    }

    int W=470, H=236;
    RECT wa; SystemParametersInfoW(SPI_GETWORKAREA,0,&wa,0);
    int x=wa.left+((wa.right-wa.left)-W)/2;
    int y=wa.top +((wa.bottom-wa.top)-H)/2;

    HWND h=CreateWindowExW(WS_EX_TOPMOST|WS_EX_DLGMODALFRAME,WD_CLS,
        L"\u0637\u0631\u0627\u062d \u0686\u0627\u067e \u2014 \u062c\u0644\u0633\u0647 \u0628\u0627\u0632 \u0627\u0633\u062a",
        WS_POPUPWINDOW|WS_CAPTION|WS_VISIBLE,
        x,y,W,H,owner,NULL,g_hInst,NULL);
    if(!h) return true; // browser is open even if the helper window failed

    WaitState* st=new WaitState();
    st->owner=owner; st->url=url;
    SetWindowLongPtrW(h,GWLP_USERDATA,(LONG_PTR)st);

    HFONT f = g_fUI ? g_fUI : (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    HWND info=CreateWindowExW(0,L"STATIC",
        L"\u0637\u0631\u0627\u062d \u0686\u0627\u067e \u062f\u0631 \u0645\u0631\u0648\u0631\u06af\u0631 "
        L"\u067e\u06cc\u0634\u200c\u0641\u0631\u0636 \u0628\u0627\u0632 \u0634\u062f.\n\n"
        L"\u062f\u0631 \u0635\u0641\u062d\u0647\u0654 \u0637\u0631\u0627\u062d\u06cc\u060c \u062f\u06a9\u0645\u0647\u0654 "
        L"\u00ab\u0630\u062e\u06cc\u0631\u0647 \u0648 \u0627\u0639\u0645\u0627\u0644\u00bb \u0631\u0627 \u0628\u0632\u0646\u06cc\u062f \u062a\u0627 "
        L"\u0637\u0631\u062d \u0630\u062e\u06cc\u0631\u0647 \u0648 \u0641\u0648\u0631\u06cc \u0628\u0631 \u0628\u062e\u0634 \u0627\u0639\u0645\u0627\u0644 \u0634\u0648\u062f.\n"
        L"\u067e\u0633 \u0627\u0632 \u067e\u0627\u06cc\u0627\u0646\u060c \u00ab\u067e\u0627\u06cc\u0627\u0646 \u0637\u0631\u0627\u062d\u06cc\u00bb \u0631\u0627 \u0628\u0632\u0646\u06cc\u062f.",
        WS_CHILD|WS_VISIBLE|SS_LEFT,
        16,12,W-34,96,h,NULL,g_hInst,NULL);
    SendMessageW(info,WM_SETFONT,(WPARAM)f,TRUE);

    // live "saved" status line (updated by wdExitPoll)
    HWND sline=CreateWindowExW(0,L"STATIC",L"",
        WS_CHILD|WS_VISIBLE|SS_LEFT,
        16,110,W-34,18,h,(HMENU)200,g_hInst,NULL);
    SendMessageW(sline,WM_SETFONT,(WPARAM)f,TRUE);

    HWND bReopen=CreateWindowExW(0,L"BUTTON",
        L"\u0628\u0627\u0632 \u06a9\u0631\u062f\u0646 \u062f\u0648\u0628\u0627\u0631\u0647 \u062f\u0631 \u0645\u0631\u0648\u0631\u06af\u0631",
        WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,
        16,H-66,210,30,h,(HMENU)101,g_hInst,NULL);
    SendMessageW(bReopen,WM_SETFONT,(WPARAM)f,TRUE);

    HWND bDone=CreateWindowExW(0,L"BUTTON",
        L"\u067e\u0627\u06cc\u0627\u0646 \u0637\u0631\u0627\u062d\u06cc",
        WS_CHILD|WS_VISIBLE|BS_DEFPUSHBUTTON,
        W-150,H-66,130,30,h,(HMENU)102,g_hInst,NULL);
    SendMessageW(bDone,WM_SETFONT,(WPARAM)f,TRUE);

    if(owner&&IsWindow(owner)) EnableWindow(owner,FALSE);
    SetTimer(h,1,300,wdExitPoll);

    // modal pump until the session window closes
    MSG msg;
    while(IsWindow(h) && GetMessageW(&msg,NULL,0,0)){
        if(msg.message==WM_QUIT){ PostQuitMessage((int)msg.wParam); break; }
        if(!IsWindow(h)) break;
        if(IsDialogMessageW(h,&msg)){ if(!IsWindow(h)) break; continue; }
        TranslateMessage(&msg); DispatchMessageW(&msg);
        if(!IsWindow(h)) break;
    }
    delete st;
    if(owner&&IsWindow(owner)){ EnableWindow(owner,TRUE); SetForegroundWindow(owner); }
    return true;
}
