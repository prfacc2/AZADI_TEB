// ============================================================================
//  setup_splash.cpp — first-run / prerequisite preparation splash.
//
//  WHY: the hybrid Reception/Appointment surface is rendered by the *system*
//  MSHTML (Trident) WebBrowser OLE control. On a fresh client machine three
//  things must be true for it to render the modern HTML/CSS/JS correctly AND
//  for the synchronous C++↔JS bridge to work:
//    1. The EXE must be registered under FEATURE_BROWSER_EMULATION (value
//       11001 = IE11 standards), otherwise Trident defaults to IE7 *quirks*
//       mode and flexbox/grid/JSON/querySelector silently fail — which looks
//       exactly like "the UI didn't change / the bridge is broken".
//    2. The Vazirmatn font should be installed per-user so Persian text renders
//       with the intended metrics.
//    3. MSHTML itself must be resolvable (it is on every Windows 7→11, but we
//       probe so we can fall back deterministically).
//  We also make sure the data\ and logs\ folders exist so saveReception() and
//  the error log can actually be written (a missing/locked folder is the #1
//  real cause of "خطا در ثبت").
//
//  This module shows a small, centred, branded progress window while it does
//  the above, the first time it runs (and again after a version bump, so a new
//  build re-applies the emulation key). On subsequent runs it returns instantly
//  with no window. Pure Win32/GDI — no extra DLLs, fits the single-EXE rule.
// ============================================================================
#include "app.h"   // brings in <windows.h>, <string>, and the project decls

// from handlers.cpp
extern void installVazirFont();

// ----------------------------------------------------------------------------
//  Small state shared between the worker thread and the painting window.
// ----------------------------------------------------------------------------
struct SetupState {
    volatile LONG pct   = 0;        // 0..100
    wchar_t       step[160] = L"در حال آماده‌سازی…";
    volatile LONG done  = 0;        // worker finished
    bool          webOk = false;    // MSHTML probe result
};
static SetupState* g_ss = nullptr;

static const wchar_t* SS_CLASS = L"AzadiTebSetupSplash";

// ----------------------------------------------------------------------------
//  Helpers
// ----------------------------------------------------------------------------
static void ssSet(SetupState* s, int pct, const wchar_t* step){
    InterlockedExchange(&s->pct, pct);
    if(step) lstrcpynW(s->step, step, 160);
}

// Decide whether the splash needs to run at all. We run it on first ever launch
// and whenever the stored "setup_done_version" differs from the current build,
// so a freshly-deployed EXE always re-applies the browser-emulation key.
static bool ssNeedsRun(){
    //  v1.17.0: the only state we track is whether prerequisites were prepared
    //  for THIS build. There is no longer any browser-emulation registry key to
    //  validate (the HTML/MSHTML layer was retired in favour of a 100% native
    //  C++ UI), so a simple version compare is sufficient and instant.
    std::wstring done = getSetting(L"setup_done_version", L"");
    return done != std::wstring(APP_VERSION_W);
}

// ----------------------------------------------------------------------------
//  Worker: performs the actual preparation, updating progress as it goes.
//  Deliberately paced (short sleeps) so the bar is *seen* — the operations
//  themselves are near-instant, but a flash-and-vanish window confuses users
//  who reported "nothing happens / it didn't apply".
// ----------------------------------------------------------------------------
static DWORD WINAPI ssWorker(LPVOID p){
    SetupState* s = (SetupState*)p;

    ssSet(s, 6,  L"بررسی پوشه‌های برنامه…");
    // ensure data/ and logs/ exist & are writable (root cause of save errors)
    dataDir();   // auto-creates <exe>\data (or override)
    logsDir();   // auto-creates <exe>\logs
    Sleep(220);

    ssSet(s, 34, L"نصب فونت فارسی (Vazirmatn)…");
    installVazirFont();
    Sleep(300);

    ssSet(s, 70, L"آماده‌سازی رابط کاربری بومی (C++)…");
    //  v1.17.0: the interface is rendered fully in native C++ (Win32/GDI) — no
    //  embedded browser, no Trident/IE registry key, no C++↔JS bridge. Nothing
    //  external to configure here; the native engine is always available.
    s->webOk = false;
    Sleep(300);

    ssSet(s, 94, L"تکمیل راه‌اندازی…");
    // mark this build's setup complete so next launch is instant.
    setSetting(L"setup_done_version", APP_VERSION_W);
    logLine(L"setup: prerequisites prepared (native C++ UI)");
    Sleep(200);

    ssSet(s, 100, L"آماده است");
    Sleep(260);
    InterlockedExchange(&s->done, 1);
    return 0;
}

// ----------------------------------------------------------------------------
//  Window proc — paints a centred branded card with a determinate bar.
// ----------------------------------------------------------------------------
static LRESULT CALLBACK ssProc(HWND h, UINT m, WPARAM w, LPARAM l){
    switch(m){
    case WM_ERASEBKGND: return 1; // we paint everything in WM_PAINT
    case WM_TIMER:
        InvalidateRect(h, NULL, FALSE);
        if(g_ss && InterlockedCompareExchange(&g_ss->done,0,0)){
            // hold one extra tick at 100% so it doesn't blink away
            KillTimer(h,1);
            PostMessageW(h, WM_CLOSE, 0, 0);
        }
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps; HDC dc=BeginPaint(h,&ps);
        RECT rc; GetClientRect(h,&rc);
        int W=rc.right, H=rc.bottom;

        // double-buffer
        HDC mem=CreateCompatibleDC(dc);
        HBITMAP bm=CreateCompatibleBitmap(dc,W,H);
        HBITMAP ob=(HBITMAP)SelectObject(mem,bm);

        // card background (white) with a soft top accent band
        HBRUSH bg=CreateSolidBrush(RGB(255,255,255));
        FillRect(mem,&rc,bg); DeleteObject(bg);
        // accent header band
        RECT band={0,0,W,86};
        HBRUSH hb=CreateSolidBrush(RGB(43,109,244));
        FillRect(mem,&band,hb); DeleteObject(hb);

        SetBkMode(mem,TRANSPARENT);

        // brand glyph circle
        int cx=W/2, cyTop=43;
        HBRUSH wb=CreateSolidBrush(RGB(255,255,255));
        HPEN   np=(HPEN)GetStockObject(NULL_PEN);
        HGDIOBJ opn=SelectObject(mem,np); HGDIOBJ obr=SelectObject(mem,wb);
        Ellipse(mem, cx-26, cyTop-26, cx+26, cyTop+26);
        SelectObject(mem,opn); SelectObject(mem,obr); DeleteObject(wb);

        // fonts
        HFONT fBrandGlyph=CreateFontW(34,0,0,0,FW_BOLD,0,0,0,DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,
            DEFAULT_PITCH,L"Vazirmatn");
        HFONT fTitle=CreateFontW(22,0,0,0,FW_BOLD,0,0,0,DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,
            DEFAULT_PITCH,L"Vazirmatn");
        HFONT fStep=CreateFontW(15,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,
            DEFAULT_PITCH,L"Vazirmatn");
        HFONT fPct=CreateFontW(13,0,0,0,FW_BOLD,0,0,0,DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,
            DEFAULT_PITCH,L"Vazirmatn");

        // brand glyph «آ» (blue on white circle)
        SelectObject(mem,fBrandGlyph); SetTextColor(mem,RGB(43,109,244));
        RECT rg={cx-26,cyTop-22,cx+26,cyTop+26};
        DrawTextW(mem,L"\u0622",-1,&rg,DT_CENTER|DT_VCENTER|DT_SINGLELINE);

        // title
        SelectObject(mem,fTitle); SetTextColor(mem,RGB(31,42,68));
        RECT rt={0,108,W,140};
        DrawTextW(mem, APP_NAME_W, -1, &rt, DT_CENTER|DT_SINGLELINE);

        // step text
        SelectObject(mem,fStep); SetTextColor(mem,RGB(110,122,140));
        RECT rs={20,150,W-20,176};
        DrawTextW(mem, g_ss?g_ss->step:L"", -1, &rs,
                  DT_CENTER|DT_SINGLELINE|DT_END_ELLIPSIS|DT_RTLREADING);

        // progress track + fill
        int pct = g_ss? (int)InterlockedCompareExchange(&g_ss->pct,0,0):0;
        if(pct<3) pct=3; if(pct>100) pct=100;
        int trackL=34, trackR=W-34, trackY=196, trackH=12;
        RECT trk={trackL,trackY,trackR,trackY+trackH};
        HBRUSH tb=CreateSolidBrush(RGB(231,237,245));
        // rounded track
        HGDIOBJ obr2=SelectObject(mem,tb);
        HGDIOBJ opn2=SelectObject(mem,(HPEN)GetStockObject(NULL_PEN));
        RoundRect(mem,trk.left,trk.top,trk.right,trk.bottom,trackH,trackH);
        // fill
        int fillW=(int)((trackR-trackL)*(pct/100.0));
        if(fillW<trackH) fillW=trackH;
        HBRUSH fb=CreateSolidBrush(RGB(43,109,244));
        SelectObject(mem,fb);
        RoundRect(mem,trk.left,trk.top,trk.left+fillW,trk.bottom,trackH,trackH);
        SelectObject(mem,obr2); SelectObject(mem,opn2);
        DeleteObject(tb); DeleteObject(fb);

        // percent label
        SelectObject(mem,fPct); SetTextColor(mem,RGB(43,109,244));
        wchar_t pb[16]; wsprintfW(pb,L"%d%%",pct);
        RECT rp={trackL,trackY+18,trackR,trackY+40};
        DrawTextW(mem,pb,-1,&rp,DT_CENTER|DT_SINGLELINE);

        // blit
        BitBlt(dc,0,0,W,H,mem,0,0,SRCCOPY);

        DeleteObject(fBrandGlyph); DeleteObject(fTitle);
        DeleteObject(fStep); DeleteObject(fPct);
        SelectObject(mem,ob); DeleteObject(bm); DeleteDC(mem);
        EndPaint(h,&ps);
        return 0;
    }
    case WM_CLOSE:
        DestroyWindow(h);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(h,m,w,l);
}

// ----------------------------------------------------------------------------
//  Public entry.
// ----------------------------------------------------------------------------
bool RunSetupSplash(HINSTANCE hInst){
    // Fast path: already prepared for this build → just (idempotently) ensure
    // the emulation key + folders without any UI, then return availability.
    if(!ssNeedsRun()){
        dataDir(); logsDir();            // idempotently ensure writable folders
        return false;                    // native UI — no web host
    }

    SetupState st; g_ss=&st;

    WNDCLASSW wc={0};
    wc.lpfnWndProc   = ssProc;
    wc.hInstance     = hInst;
    wc.hCursor       = LoadCursorW(NULL, IDC_ARROW);
    wc.lpszClassName = SS_CLASS;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
    RegisterClassW(&wc);

    int sw=GetSystemMetrics(SM_CXSCREEN), sh=GetSystemMetrics(SM_CYSCREEN);
    int W=420, H=260;
    int x=(sw-W)/2, y=(sh-H)/2;

    HWND h=CreateWindowExW(WS_EX_TOPMOST|WS_EX_DLGMODALFRAME,
        SS_CLASS, APP_NAME_W, WS_POPUP|WS_BORDER,
        x,y,W,H, NULL,NULL,hInst,NULL);
    if(!h){
        // could not show UI — still do the work silently.
        ssWorker(&st);
        g_ss=nullptr;
        return st.webOk;
    }

    ShowWindow(h, SW_SHOW);
    UpdateWindow(h);
    SetTimer(h, 1, 33, NULL);   // ~30fps repaint

    HANDLE th=CreateThread(NULL,0,ssWorker,&st,0,NULL);

    MSG msg;
    while(GetMessageW(&msg,NULL,0,0)){
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    if(th){ WaitForSingleObject(th, 2000); CloseHandle(th); }
    UnregisterClassW(SS_CLASS, hInst);
    g_ss=nullptr;
    return st.webOk;
}
