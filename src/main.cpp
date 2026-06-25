// ============================================================================
//  main.cpp — entry point, fullscreen frame (no menu/title bar), home screen,
//  live Iran clock/date (bottom-right), hidden admin combo Ctrl+P+N,
//  global F8 = print last receipt
// ============================================================================
#include "app.h"
#include "backup_log.h"
#include <stdio.h>

HINSTANCE g_hInst = NULL;
HWND      g_hFrame = NULL;
double    g_scale = 1.0;
Session   g_session;

HFONT g_fUI=0, g_fUIB=0, g_fSmall=0, g_fTitle=0, g_fBig=0, g_fHuge=0, g_fMono=0;
HFONT g_fCode=0;   // §G: fixed-pitch code font (Consolas → Courier New)

// frame children
//  v1.4.0: the header now carries ONLY the exit button (right) and the gear
//  settings button (left). Theme-toggle and check-for-update were removed from
//  the header and moved INTO the settings panel per the redesign brief.
static HWND s_bExit=0, s_bSettings=0, s_bCalc=0;
//  v1.7.0: the «پذیرش جدید» / «نوبت‌دهی» / «تب جدید» actions were moved out of
//  the reception tab strip and INTO this header so the navigation is clean and
//  professional. They are shown only while the reception screen is active and
//  are routed to it via receptionAction().
static HWND s_bNewPat=0, s_bAppt=0, s_bNewTab=0;
static HWND s_screen=0;
static ScreenId s_curScreen = SC_HOME;

#define ID_FR_EXIT     101
#define ID_FR_SETTINGS 104
#define ID_FR_CALC     105
#define ID_FR_NEWPAT   106
#define ID_FR_APPT     107
#define ID_FR_NEWTAB   108
#define TIMER_CLOCK  1

// ------------------------------------------------------------------ fonts --
static HFONT mkFont(int px, int weight){
    return CreateFontW(-S(px),0,0,0,weight,0,0,0,DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,
        g_lowSpec?DEFAULT_QUALITY:CLEARTYPE_QUALITY,
        DEFAULT_PITCH,L"Vazirmatn");
}
// §G: a fixed-pitch font for codes. Try Consolas first; GDI falls back to
// Courier New automatically when Consolas is absent (FIXED_PITCH ensures a
// monospace face is chosen). DEFAULT_CHARSET keeps Persian digits rendering.
static HFONT mkMonoFont(int px, int weight){
    HFONT f=CreateFontW(-S(px),0,0,0,weight,0,0,0,DEFAULT_CHARSET,
        OUT_TT_PRECIS,CLIP_DEFAULT_PRECIS,
        g_lowSpec?DEFAULT_QUALITY:CLEARTYPE_QUALITY,
        FIXED_PITCH|FF_MODERN,L"Consolas");
    if(f) return f;
    return CreateFontW(-S(px),0,0,0,weight,0,0,0,DEFAULT_CHARSET,
        OUT_TT_PRECIS,CLIP_DEFAULT_PRECIS,
        g_lowSpec?DEFAULT_QUALITY:CLEARTYPE_QUALITY,
        FIXED_PITCH|FF_MODERN,L"Courier New");
}
static void buildFonts(){
    if(g_fUI)   DeleteObject(g_fUI);
    if(g_fUIB)  DeleteObject(g_fUIB);
    if(g_fSmall)DeleteObject(g_fSmall);
    if(g_fTitle)DeleteObject(g_fTitle);
    if(g_fBig)  DeleteObject(g_fBig);
    if(g_fHuge) DeleteObject(g_fHuge);
    if(g_fMono) DeleteObject(g_fMono);
    if(g_fCode) DeleteObject(g_fCode);
    g_fUI    = mkFont(15, FW_NORMAL);
    g_fUIB   = mkFont(15, FW_BOLD);
    g_fSmall = mkFont(12, FW_NORMAL);
    g_fTitle = mkFont(19, FW_BOLD);
    g_fBig   = mkFont(30, FW_BOLD);
    g_fHuge  = mkFont(38, FW_BOLD);
    g_fMono  = mkFont(24, FW_BOLD);
    g_fCode  = mkMonoFont(12, FW_NORMAL);   // §G: section / personnel codes
}

// ------------------------------------------------------------- frame rects -
//  v1.3.0 — taller header (LAYER 1) so the centered live clock + Jalali date
//  fit comfortably; thinner bottom status bar (clock moved up to the header).
//  v1.8.0 — the header now has TWO layers: LAYER 1 (identity + clock + gear /
//  calculator / exit) and a thinner LAYER 2 "action bar" that, on the reception
//  screen, hosts the blue navigation buttons (نوبت‌دهی / پذیرش جدید / تب جدید)
//  RIGHT-aligned. The action bar is only present where it is needed so other
//  screens keep the original clean single-layer header.
// §2.B (1.12.0): the LAYER-1 header was slightly reduced from S(64) to S(56)
// for a more compact, modern look. The clock (top) + Jalali date (below) still
// fit comfortably because the clock band is S(6)+S(30) and the date S(30)→S(54)
// — both recalculated against this height in the paint code below.
static int mainBarH(){ return S(56); }                 // LAYER 1 height
// §B (v1.10.0): the action bar has a FIXED compact height. The old code scaled
// it by an animated collapse factor (S(50)*factor) which produced the
// frame-by-frame slide, the one-frame "stuck" artifact and an empty header row
// mid-animation. The animation is gone: the bar is simply present (compact) on
// the reception screen and absent everywhere else — applied in a single paint.
static int actionBarH(){ return S(48); }
static bool headerHasActionBar(){ return s_curScreen==SC_RECEPTION; }
static int topBarH(){ return mainBarH() + (headerHasActionBar()?actionBarH():0); }
static int botBarH(){ return S(40); }
RECT frameContentRect(){
    RECT rc; GetClientRect(g_hFrame,&rc);
    rc.top += topBarH(); rc.bottom -= botBarH();
    return rc;
}

static void frameLayout(HWND h);   // fwd (header layout, defined below)
// ----------------------------------------------------------- screen switch -
void switchScreen(ScreenId id){
    { const wchar_t* nm = id==SC_HOME?L"switchScreen: HOME"
                        : id==SC_RECEPTION?L"switchScreen: RECEPTION"
                        : id==SC_ADMIN?L"switchScreen: ADMIN"
                        : id==SC_MANAGE?L"switchScreen: MANAGE":L"switchScreen: ?";
      Breadcrumb(nm); }
    if(s_screen){ DestroyWindow(s_screen); s_screen=0; }
    s_curScreen = id;
    // §B (v1.10.0): NO animation. The reception screen uses the COMPACT header
    // layout immediately on entry; every other screen has no action bar at all.
    HeaderCollapse_Set(g_hFrame, id==SC_RECEPTION);
    switch(id){
        case SC_HOME:      s_screen = createHomeScreen(g_hFrame); break;
        case SC_RECEPTION: s_screen = createReceptionScreen(g_hFrame); break;
        case SC_ADMIN:     s_screen = createAdminScreen(g_hFrame); break;
        case SC_MANAGE:    s_screen = createManageScreen(g_hFrame); break;
    }
    RECT rc = frameContentRect();
    if(s_screen)
        MoveWindow(s_screen, rc.left, rc.top, rc.right-rc.left, rc.bottom-rc.top, TRUE);
    frameLayout(g_hFrame);   // refresh header action buttons for the new screen
    InvalidateRect(g_hFrame, NULL, TRUE);
}

// ================================================================ HOME =====
#define HM_CLASS L"AzHome"
#define ID_HM_RECEPTION 111
#define ID_HM_MANAGE    112

static LRESULT CALLBACK homeProc(HWND h, UINT m, WPARAM w, LPARAM l){
    switch(m){
    case WM_CREATE: {
        HWND r=createFlatButton(h, ID_HM_RECEPTION, L"پذیرش درمانگاه", ICO_CROSS_MED,
            BS_CARD, 0,0,10,10, L"ثبت پذیرش بیمار و صدور قبض");
        HWND mg=createFlatButton(h, ID_HM_MANAGE, L"پنل مدیریت درمانگاه", ICO_SHIELD,
            BS_CARD, 0,0,10,10, L"گزارش‌ها و مدیریت سامانه");
        setFlatButtonBg(r,  g_theme.bg2);
        setFlatButtonBg(mg, g_theme.bg2);
        return 0; }
    case WM_APP_THEME:
        setFlatButtonBg(GetDlgItem(h,ID_HM_RECEPTION), g_theme.bg2);
        setFlatButtonBg(GetDlgItem(h,ID_HM_MANAGE),    g_theme.bg2);
        InvalidateRect(h,NULL,TRUE);
        return 0;
    case WM_SIZE: {
        int W=LOWORD(l), H=HIWORD(l);
        int cw=S(290), chh=S(170), gap=S(32);
        // v1.4.0: more breathing room. Vertical stack:
        //   logo(96) + 18 + title(46) + sub(28) + BIG gap(72) + cards(170)
        int gapTitleCards = S(72);
        int stackH = S(96)+S(18)+S(46)+S(28)+gapTitleCards+chh;
        int yTop = (H-stackH)/2; if(yTop<S(10)) yTop=S(10);
        int yCards = yTop + S(96)+S(18)+S(46)+S(28)+gapTitleCards;
        int totW = 2*cw+gap;
        int x=(W-totW)/2;
        // RTL: reception card on the right
        MoveWindow(GetDlgItem(h,ID_HM_RECEPTION), x+cw+gap, yCards, cw, chh, TRUE);
        MoveWindow(GetDlgItem(h,ID_HM_MANAGE),    x,        yCards, cw, chh, TRUE);
        return 0; }
    case WM_COMMAND: {
        static bool s_busy=false;            // re-entry guard for modal dialogs
        if(s_busy) return 0;
        int id=LOWORD(w);
        if(id==ID_HM_RECEPTION){
            s_busy=true;
            User u;
            if(showLoginDialog(g_hFrame, 0, u)){
                int shift=detectShift();
                if(showShiftDialog(g_hFrame, shift)){
                    g_session.user=u; g_session.shift=shift;
                    g_session.loginAt=iranNow();
                    setUserOnline(u.username,true);
                    s_busy=false;
                    switchScreen(SC_RECEPTION);
                    return 0;
                }
            }
            s_busy=false;
        } else if(id==ID_HM_MANAGE){
            s_busy=true;
            User u;
            if(showLoginDialog(g_hFrame, 1, u)){
                g_session.user=u; g_session.shift=detectShift();
                g_session.loginAt=iranNow();
                setUserOnline(u.username,true);
                s_busy=false;
                switchScreen(SC_MANAGE);
                return 0;
            }
            s_busy=false;
        }
        return 0; }
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps; HDC dc0=BeginPaint(h,&ps);
        RECT rc; GetClientRect(h,&rc);
        HDC dc=CreateCompatibleDC(dc0);
        HBITMAP bmp=CreateCompatibleBitmap(dc0,rc.right,rc.bottom);
        HGDIOBJ obm=SelectObject(dc,bmp);

        // v1.3.0: real background image with a soft legibility scrim so the
        // welcome screen feels like a designed product, not a blank panel.
        if(!gpDrawBackground(dc, rc, g_dark, g_theme.bg, g_dark?70:40)){
            // fallback: gentle vertical gradient
            RECT full={0,0,rc.right,rc.bottom};
            gpGradRoundRect(dc,full,0,g_theme.bg,g_theme.bg2,CLR_INVALID);
        }
        SetBkMode(dc,TRANSPARENT);

        // ---- same vertical stack math as WM_SIZE ----
        int chh=S(170);
        int gapTitleCards = S(72);
        int stackH = S(96)+S(18)+S(46)+S(28)+gapTitleCards+chh;
        int yTop = (rc.bottom-stackH)/2; if(yTop<S(10)) yTop=S(10);

        // glass hero panel behind the centered content (open, layered look)
        int heroW=S(760); if(heroW>rc.right-S(40)) heroW=rc.right-S(40);
        RECT hero={rc.right/2-heroW/2, yTop-S(28),
                   rc.right/2+heroW/2, yTop+S(96)+S(18)+S(46)+S(28)+S(20)};
        gpShadow(dc,hero,S(22),S(14),60);
        // Hero "glass": a gentle theme-aware gradient (light surface in light
        // mode, deep slate in dark mode) so the title text always has strong
        // contrast against the busy background image.
        COLORREF heroTop = g_dark?RGB(26,33,46):RGB(255,255,255);
        COLORREF heroBot = g_dark?RGB(18,24,34):RGB(244,248,253);
        gpGradRoundRect(dc,hero,S(22), heroTop, heroBot, g_theme.border);

        // logo circle (gradient) centered horizontally
        int r = S(48);
        int cy = yTop + r;
        RECT lc={rc.right/2-r, cy-r, rc.right/2+r, cy+r};
        gpShadow(dc,lc,r,S(8),70);
        gpGradRoundRect(dc,lc,r,g_theme.accent2,g_theme.accent,CLR_INVALID);
        RECT li={lc.left+S(24),lc.top+S(24),lc.right-S(24),lc.bottom-S(24)};
        drawIcon(dc,ICO_CROSS_MED,li,RGB(255,255,255),S(2)+1);

        // title — v1.4.0: brand name "آزادی طب" removed from the centre per
        // the brief; the centred title is now the system tagline only.
        int yTitle = yTop + S(96) + S(18);
        SetTextColor(dc,g_theme.text);
        SelectObject(dc,g_fBig);
        RECT tr={0,yTitle,rc.right,yTitle+S(46)};
        DrawTextW(dc,L"سامانه پذیرش و مدیریت درمانگاه",-1,&tr,
            DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);

        // subtitle
        int ySub = yTitle + S(46);
        SelectObject(dc,g_fUI);
        SetTextColor(dc,g_theme.textDim);
        RECT sr={0,ySub,rc.right,ySub+S(28)};
        DrawTextW(dc,L"نوع کاربری خود را انتخاب کنید",-1,&sr,
            DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);

        // hint (version)
        SelectObject(dc,g_fSmall);
        SetTextColor(dc,g_theme.textDim);
        RECT hr={0,rc.bottom-S(34),rc.right,rc.bottom-S(8)};
        std::wstring hint = std::wstring(L"نسخه ")+toFaDigits(APP_VERSION_W);
        DrawTextW(dc,hint.c_str(),-1,&hr,
            DT_CENTER|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);

        BitBlt(dc0,0,0,rc.right,rc.bottom,dc,0,0,SRCCOPY);
        SelectObject(dc,obm); DeleteObject(bmp); DeleteDC(dc);
        EndPaint(h,&ps);
        return 0; }
    }
    return DefWindowProcW(h,m,w,l);
}
HWND createHomeScreen(HWND frame){
    static bool reg=false;
    if(!reg){
        WNDCLASSW wc={0};
        wc.lpfnWndProc=homeProc; wc.hInstance=g_hInst;
        wc.hCursor=LoadCursor(NULL,IDC_ARROW);
        wc.lpszClassName=HM_CLASS;
        RegisterClassW(&wc); reg=true;
    }
    RECT rc=frameContentRect();
    return CreateWindowExW(0,HM_CLASS,L"",WS_CHILD|WS_VISIBLE|WS_CLIPCHILDREN,
        rc.left,rc.top,rc.right-rc.left,rc.bottom-rc.top,
        frame,NULL,g_hInst,NULL);
}

// =============================================================== FRAME =====
//  v1.7.0: show/position the reception action buttons in the header. They live
//  on the LEFT, after the gear+calculator, and only while the reception screen
//  is active (RTL: laid out left→right since they sit on the LEFT side).
static void updateHeaderButtons(HWND h){
    bool show = headerHasActionBar();
    ShowWindow(s_bNewPat, show?SW_SHOW:SW_HIDE);
    ShowWindow(s_bAppt,   show?SW_SHOW:SW_HIDE);
    ShowWindow(s_bNewTab, show?SW_SHOW:SW_HIDE);
    if(!show) return;
    RECT rc; GetClientRect(h,&rc);
    int bh=S(38), pad=S(16), g=S(10);
    // LAYER 2 (action bar) sits directly under LAYER 1.
    int y = mainBarH() + (actionBarH()-bh)/2;
    // RIGHT-aligned cluster, order requested by the brief (right → left as the
    // RTL reading order, so the FIRST item «نوبت‌دهی» is the right-most):
    //     نوبت‌دهی  |  پذیرش جدید  |  تب جدید
    int wAppt=S(120), wNew=S(134), wTab=S(112);
    int x = rc.right - pad - wAppt;            // appointment (right-most)
    MoveWindow(s_bAppt,   x,                          y, wAppt, bh, TRUE);
    MoveWindow(s_bNewPat, x-g-wNew,                   y, wNew,  bh, TRUE);
    MoveWindow(s_bNewTab, x-g-wNew-g-wTab,            y, wTab,  bh, TRUE);
    // blend the buttons' rounded corners into the LAYER 2 surface colour.
    setFlatButtonBg(s_bNewPat, g_theme.surface2);
    setFlatButtonBg(s_bAppt,   g_theme.surface2);
    setFlatButtonBg(s_bNewTab, g_theme.surface2);
}
static void frameLayout(HWND h){
    RECT rc; GetClientRect(h,&rc);
    int bh=S(38), pad=S(14);
    int y=(mainBarH()-bh)/2;     // LAYER 1 vertical centre
    // --- RIGHT side (RTL primary): EXIT is the right-most control; the app
    //     identity (logo + name + fullname + access) is painted to its LEFT.
    MoveWindow(s_bExit,  rc.right-pad-bh, y, bh, bh, TRUE);
    // --- LEFT side: settings (gear) button, then the calculator beside it —
    //     handy in the header but out of the way of the tabs / actions.
    MoveWindow(s_bSettings, pad, y, bh, bh, TRUE);
    MoveWindow(s_bCalc, pad+bh+S(8), y, bh, bh, TRUE);
    // keep the header buttons' rounded corners blended into the header gradient
    setFlatButtonBg(s_bExit,     g_theme.headerTop);
    setFlatButtonBg(s_bSettings, g_theme.headerTop);
    setFlatButtonBg(s_bCalc,     g_theme.headerTop);
    updateHeaderButtons(h);
    if(s_screen){
        RECT cr=frameContentRect();
        MoveWindow(s_screen,cr.left,cr.top,cr.right-cr.left,cr.bottom-cr.top,TRUE);
    }
}

static LRESULT CALLBACK frameProc(HWND h, UINT m, WPARAM w, LPARAM l){
    switch(m){
    case WM_CREATE:
        g_hFrame = h;
        s_bExit     = createFlatButton(h, ID_FR_EXIT,    L"", ICO_X,      BS_GHOST,0,0,10,10);
        s_bSettings = createFlatButton(h, ID_FR_SETTINGS,L"", ICO_GEAR,   BS_GHOST,0,0,10,10);
        s_bCalc     = createFlatButton(h, ID_FR_CALC,    L"", ICO_CALC,   BS_GHOST,0,0,10,10);
        // v1.8.0: use the new clean raster gear / calculator icons (white-on-
        // alpha PNGs tinted to the theme accent) so both buttons read perfectly
        // on the light AND the dark theme. They gracefully fall back to the
        // vector ICO_GEAR / ICO_CALC if GDI+ or the resource is unavailable.
        setFlatButtonImage(s_bSettings, IMG_IC_SETTINGS);
        setFlatButtonImage(s_bCalc,     IMG_IC_CALC);
        // header action buttons (reception only) — created hidden, shown by
        // updateHeaderButtons() when the reception screen becomes active.
        s_bNewPat   = createFlatButton(h, ID_FR_NEWPAT, L"پذیرش جدید", ICO_PLUS, BS_PRIMARY,0,0,10,10,
                          L"ثبت پذیرش بیمار جدید");
        // v1.9.0: appointment + new-tab buttons are also blue (BS_PRIMARY) for
        // a consistent header action style alongside «پذیرش جدید».
        s_bAppt     = createFlatButton(h, ID_FR_APPT,   L"نوبت‌دهی",   ICO_CAL,  BS_PRIMARY,0,0,10,10,
                          L"باز کردن صفحهٔ نوبت‌دهی");
        s_bNewTab   = createFlatButton(h, ID_FR_NEWTAB, L"تب جدید",    ICO_TAB,  BS_PRIMARY,0,0,10,10,
                          L"باز کردن یک تب خالی");
        ShowWindow(s_bNewPat,SW_HIDE);
        ShowWindow(s_bAppt,  SW_HIDE);
        ShowWindow(s_bNewTab,SW_HIDE);
        setFlatButtonBg(s_bExit,     g_theme.headerTop);
        setFlatButtonBg(s_bSettings, g_theme.headerTop);
        setFlatButtonBg(s_bCalc,     g_theme.headerTop);
        setFlatButtonBg(s_bNewPat,   g_theme.headerTop);
        setFlatButtonBg(s_bAppt,     g_theme.headerTop);
        setFlatButtonBg(s_bNewTab,   g_theme.headerTop);
        SetTimer(h, TIMER_CLOCK, g_lowSpec?1000:500, NULL);
        return 0;
    case WM_SIZE: frameLayout(h); return 0;
    case WM_APP_THEME:
        // theme may have been switched from inside the settings panel — refresh
        // the header buttons' corner-blend colour and repaint the whole frame.
        setFlatButtonBg(s_bExit,     g_theme.headerTop);
        setFlatButtonBg(s_bSettings, g_theme.headerTop);
        setFlatButtonBg(s_bCalc,     g_theme.headerTop);
        setFlatButtonBg(s_bNewPat,   g_theme.headerTop);
        setFlatButtonBg(s_bAppt,     g_theme.headerTop);
        setFlatButtonBg(s_bNewTab,   g_theme.headerTop);
        InvalidateRect(h,NULL,TRUE);
        return 0;
    case WM_TIMER:
        if(w==TIMER_CLOCK){
            // §H: repaint only the clock/date zone (minimal invalidation — no
            // whole-window invalidate). The zone spans the full header width when
            // the clock is centred (full header) and just the left strip when the
            // header is collapsed (reception); invalidating the whole LAYER-1 band
            // is cheap and avoids having to recompute the exact centred rect here.
            RECT crc; GetClientRect(h,&crc);
            RECT cz={0, 0, crc.right, mainBarH()};     // full-width LAYER-1 strip
            InvalidateRect(h,&cz,FALSE);
            // v1.9.0: poll for an incoming-message notification for THIS user
            // (employees only — managers never get notified of their own send).
            notifyNewMessageRecipients();
            // §G (1.11.0): refresh this session's heartbeat at most every ~30s so
            // presence stays inside the 90s online window without thrashing the
            // small presence file on every clock tick.
            if(!g_session.user.username.empty()){
                static DWORD s_lastBeat=0;
                DWORD now=GetTickCount();
                if(now - s_lastBeat >= 30000 || s_lastBeat==0){
                    s_lastBeat=now;
                    heartbeatUser(g_session.user.username);
                }
            }
        }
        // §B (v1.10.0): the HEADER_COLLAPSE_TIMER animation has been removed.
        // The timer is never started anymore; if a stale one ever fires we just
        // kill it so nothing animates.
        else if(w==HEADER_COLLAPSE_TIMER){
            KillTimer(h, HEADER_COLLAPSE_TIMER);
        }
        return 0;
    case WM_COMMAND: {
        int id=LOWORD(w);
        if(id==ID_FR_EXIT){
            if(s_curScreen==SC_HOME){
                if(MessageBoxW(h,L"از برنامه خارج می‌شوید؟",L"خروج",
                    MB_YESNO|MB_ICONQUESTION)==IDYES) DestroyWindow(h);
            } else {
                // logout to home (session ends only by user action)
                if(MessageBoxW(h,L"از حساب کاربری خارج می‌شوید؟",L"خروج از حساب",
                    MB_YESNO|MB_ICONQUESTION)==IDYES){
                    logLine(L"logout: "+g_session.user.username);
                    setUserOnline(g_session.user.username,false);
                    g_session = Session();
                    switchScreen(SC_HOME);
                }
            }
        }
        else if(id==ID_FR_SETTINGS) OpenSettings(h, g_session.user);  // §1 role dispatcher
        else if(id==ID_FR_CALC) openCalculator(h);
        // v1.7.0: header reception-action buttons → route to reception screen
        else if(id==ID_FR_NEWPAT) receptionAction(RA_NEWPAT);
        else if(id==ID_FR_APPT)   receptionAction(RA_APPOINTMENT);
        else if(id==ID_FR_NEWTAB) receptionAction(RA_NEWTAB);
        return 0; }
    case WM_KEYDOWN: {
        // hidden admin: Ctrl + P + N held together (home screen only)
        static bool s_dlgOpen = false;          // re-entry guard
        if(s_curScreen==SC_HOME && !s_dlgOpen &&
           (GetKeyState(VK_CONTROL)&0x8000) &&
           (GetKeyState('P')&0x8000) && (GetKeyState('N')&0x8000)){
            s_dlgOpen = true;
            User u;
            bool ok = showLoginDialog(h, 2, u);
            s_dlgOpen = false;
            if(ok){
                g_session.user=u; g_session.shift=detectShift();
                g_session.loginAt=iranNow();
                setUserOnline(u.username,true);
                switchScreen(SC_ADMIN);
            }
            return 0;
        }
        if(w==VK_F8){ printLastReceipt(h); return 0; }
        return 0; }
    case WM_CTLCOLORSTATIC: {
        HDC dc=(HDC)w; SetBkColor(dc,g_theme.surface2);
        return (LRESULT)g_brSurface2; }
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps; HDC dc0=BeginPaint(h,&ps);
        RECT rc; GetClientRect(h,&rc);
        HDC dc=CreateCompatibleDC(dc0);
        HBITMAP bmp=CreateCompatibleBitmap(dc0,rc.right,rc.bottom);
        HGDIOBJ obm=SelectObject(dc,bmp);

        // ===================== LAYER 1 — top header bar =====================
        // Soft vertical gradient so the header reads as a distinct, polished
        // surface (not a flat strip).
        RECT tb={0,0,rc.right,mainBarH()};
        gpGradRoundRect(dc,tb,0,g_theme.headerTop,g_theme.headerBot,CLR_INVALID);
        // ===================== LAYER 2 — action sub-bar =====================
        if(headerHasActionBar()){
            RECT ab={0,mainBarH(),rc.right,mainBarH()+actionBarH()};
            FillRect(dc,&ab,g_brSurface2);
            // crisp separator between the two header layers
            gpLine(dc,0,mainBarH(),rc.right,mainBarH(),g_theme.border,1.0f);
        }
        // bottom status bar
        RECT bb={0,rc.bottom-botBarH(),rc.right,rc.bottom};
        FillRect(dc,&bb,g_brSurface2);
        // middle bg (gentle gradient page)
        RECT mid={0,topBarH(),rc.right,rc.bottom-botBarH()};
        gpGradRoundRect(dc,mid,0,g_theme.bg,g_theme.bg2,CLR_INVALID);
        // crisp separators
        gpLine(dc,0,topBarH()-1,rc.right,topBarH()-1,g_theme.border,1.0f);
        gpLine(dc,0,rc.bottom-botBarH(),rc.right,rc.bottom-botBarH(),g_theme.border,1.0f);
        // a thin accent underline under the header for that "designed" feel
        gpLine(dc,0,topBarH()-1,rc.right,topBarH()-1,g_theme.accent,2.0f,40);

        SetBkMode(dc,TRANSPARENT);

        // ---- app identity on the RIGHT (next to the exit button) ----
        // logo badge + app name; below it the LOGGED-IN PERSON'S NAME and the
        // access type. We intentionally show the full name + role, NEVER the
        // raw login username (privacy requirement).
        int exitW = S(38)+S(14);
        int logoR = S(16);
        int logoCx = rc.right - exitW - S(16) - logoR;
        int logoCy = mainBarH()/2;
        RECT lc={logoCx-logoR,logoCy-logoR,logoCx+logoR,logoCy+logoR};
        gpGradRoundRect(dc,lc,logoR,g_theme.accent2,g_theme.accent,CLR_INVALID);
        RECT li={lc.left+S(7),lc.top+S(7),lc.right-S(7),lc.bottom-S(7)};
        drawIcon(dc,ICO_CROSS_MED,li,RGB(255,255,255),S(2));

        int idRight = logoCx-logoR-S(12);
        bool loggedIn = !g_session.user.username.empty();
        if(loggedIn){
            // two stacked lines: app name (top) + person/role (bottom).
            // §2.B: offsets tuned for the compact S(56) header.
            SelectObject(dc,g_fUIB);
            SetTextColor(dc,g_theme.text);
            RECT nr={S(160),S(5),idRight,S(5)+S(24)};
            DrawTextW(dc,APP_NAME_W,-1,&nr,
                DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
            SelectObject(dc,g_fSmall);
            SetTextColor(dc,g_theme.textDim);
            const wchar_t* role =
                g_session.user.role==2 ? L"مدیر سامانه" :
                g_session.user.role==1 ? L"مدیریت درمانگاه" : L"پذیرش درمانگاه";
            std::wstring sub = g_session.user.fullname + L"  •  " + role +
                (g_session.user.dept.empty()?L"":(L"  •  "+g_session.user.dept));
            RECT sr={S(160),S(5)+S(26),idRight,mainBarH()-S(4)};
            DrawTextW(dc,sub.c_str(),-1,&sr,
                DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
        } else {
            SelectObject(dc,g_fTitle);
            SetTextColor(dc,g_theme.text);
            RECT nr={S(160),0,idRight,mainBarH()};
            DrawTextW(dc,APP_NAME_W,-1,&nr,
                DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
        }

        // ===== LAYER 1: live clock (bold, top) + Jalali date =====
        // §H (1.11.0): when the FULL header is visible (i.e. the header is NOT
        // collapsed — every screen except reception) the clock + date are
        // HORIZONTALLY CENTRED in the header between the left tool buttons and
        // the right identity block. On the reception screen the header collapses
        // (§B) and the clock returns to the TOP-LEFT, immediately right of the
        // gear + calculator buttons, so it never collides with the action bar.
        SYSTEMTIME st=iranNow();
        int leftBtns = S(14) + S(38) + S(8) + S(38) + S(14); // pad + gear + gap + calc + gap
        std::wstring clkStr = toFaDigits(iranTimeStr(st,true));
        std::wstring dateStr= jalaliDateStr(st);
        {
            // §2.A (1.12.0): the live clock + Jalali date are now HORIZONTALLY
            // CENTRED in the LAYER-1 header on EVERY screen — including reception
            // (previously the reception header forced them top-LEFT). The safe
            // band is [leftBtns, idRight] (between the tool buttons on the left
            // and the identity block on the right); the clock zone is centred in
            // it and clamped so it never clips on small/low-resolution screens or
            // under DPI scaling. Stacked: clock on top (bold mono), date below.
            int bandL = leftBtns + S(8);
            int bandR = idRight  - S(8);
            int zoneW = S(240);
            int cx    = (bandL + bandR)/2;
            int zL    = cx - zoneW/2;
            if(zL < bandL) zL = bandL;
            int zR = zL + zoneW;
            if(zR > bandR && bandR>bandL) { zR = bandR; }
            // clock (centred, bold) — band tuned for the compact S(56) header
            SetTextColor(dc,g_theme.accent);
            SelectObject(dc,g_fMono);
            RECT ck={zL,S(5),zR,S(5)+S(28)};
            DrawTextW(dc,clkStr.c_str(),-1,&ck,
                DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_NOPREFIX);
            // date (centred, just below)
            SetTextColor(dc,g_theme.textDim);
            SelectObject(dc,g_fSmall);
            RECT dr={zL,S(5)+S(28),zR,mainBarH()-S(2)};
            DrawTextW(dc,dateStr.c_str(),-1,&dr,
                DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
        }

        // ===== bottom status bar: shift indicator (left) =====
        SetTextColor(dc,g_theme.textDim);
        SelectObject(dc,g_fSmall);
        RECT shf={S(16),rc.bottom-botBarH(),S(560),rc.bottom};
        std::wstring sf = L"شیفت جاری: " + shiftName(detectShift());
        if(loggedIn && s_curScreen==SC_RECEPTION)
            sf += L"   |   شیفت ورود: " + shiftName(g_session.shift);
        DrawTextW(dc,sf.c_str(),-1,&shf,
            DT_LEFT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
        // bottom-right: small product tag
        SetTextColor(dc,g_theme.textDim);
        RECT pr={rc.right-S(360),rc.bottom-botBarH(),rc.right-S(16),rc.bottom};
        std::wstring tag=std::wstring(APP_NAME_W)+L"  نسخه "+toFaDigits(APP_VERSION_W);
        DrawTextW(dc,tag.c_str(),-1,&pr,
            DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);

        BitBlt(dc0,0,0,rc.right,rc.bottom,dc,0,0,SRCCOPY);
        SelectObject(dc,obm); DeleteObject(bmp); DeleteDC(dc);
        EndPaint(h,&ps);
        return 0; }
    case WM_DESTROY:
        KillTimer(h,TIMER_CLOCK);
        if(!g_session.user.username.empty())
            setUserOnline(g_session.user.username,false);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(h,m,w,l);
}

// ============================== Enter / Tab => next field ===================
//  Secretaries navigate the form almost entirely with Enter and Tab, so BOTH
//  keys must hop to the next control (Shift+Tab => previous). Works inside
//  the reception page (a plain child window, not a dialog) by walking the
//  control list with GetNextDlgTabItem on the page itself.
static WNDPROC s_oldEdit = NULL;
static void hopField(HWND h, bool prev){
    HWND parent = GetParent(h);
    HWND nxt = GetNextDlgTabItem(parent, h, prev);
    if(!nxt || nxt==h){
        HWND top = GetAncestor(h, GA_ROOT);
        nxt = GetNextDlgTabItem(top, h, prev);
    }
    if(nxt && nxt!=h){
        SetFocus(nxt);
        wchar_t cls[32]={0}; GetClassNameW(nxt,cls,32);
        if(!wcscmp(cls,L"EDIT")) SendMessageW(nxt, EM_SETSEL, 0, -1);
    }
}
static LRESULT CALLBACK enterEditProc(HWND h, UINT m, WPARAM w, LPARAM l){
    if(m==WM_KEYDOWN && w==VK_RETURN){
        hopField(h, false);
        return 0;
    }
    if(m==WM_KEYDOWN && w==VK_TAB){
        hopField(h, (GetKeyState(VK_SHIFT)&0x8000)!=0);
        return 0;
    }
    if(m==WM_CHAR && (w==VK_RETURN || w==VK_TAB)) return 0;  // kill the beep
    return CallWindowProcW(s_oldEdit, h, m, w, l);
}
void enableEnterNavigation(HWND ctl){
    WNDPROC old = (WNDPROC)SetWindowLongPtrW(ctl, GWLP_WNDPROC,
        (LONG_PTR)enterEditProc);
    if(!s_oldEdit) s_oldEdit = old;
}

// ===================== smart Jalali date mask (YYYY/MM/DD) ==================
//  The user just clicks the field and types 8 digits; the program inserts the
//  slashes itself and splits them into year / month / day. Backspace removes
//  the previous digit (skipping the auto slashes). Enter/Tab still navigate.
static WNDPROC s_oldDate = NULL;
static std::wstring digitsOnly(const std::wstring& s){
    std::wstring o;
    for(wchar_t c : s){
        if(c>=L'0'&&c<=L'9') o += c;
        else if(c>=0x06F0&&c<=0x06F9) o += (wchar_t)(L'0'+(c-0x06F0)); // fa→en
        else if(c>=0x0660&&c<=0x0669) o += (wchar_t)(L'0'+(c-0x0660)); // ar→en
    }
    return o;
}
// Split a raw string into year / month / day tokens. The user may type the
// date in a relaxed way: digits packed (13400520), or separated by spaces /
// slashes / dashes and WITHOUT zero-padding, e.g. "1340 5 20". Each token is
// terminated by any non-digit (space, /, -). When packed with no separators we
// fall back to the classic YYYY MM DD split (4 + 2 + 2).
static void splitJalaliTokens(const std::wstring& raw,
                              std::wstring& y, std::wstring& mo, std::wstring& d,
                              bool& hadSep){
    y.clear(); mo.clear(); d.clear(); hadSep=false;
    // Are there any explicit separators?
    for(wchar_t c : raw)
        if(c==L' '||c==L'/'||c==L'-'||c==L'.'){ hadSep=true; break; }

    if(hadSep){
        std::wstring* parts[3]={&y,&mo,&d}; int idx=0;
        bool inTok=false;
        for(wchar_t c : raw){
            std::wstring dig;
            if(c>=L'0'&&c<=L'9') dig=std::wstring(1,c);
            else if(c>=0x06F0&&c<=0x06F9) dig=std::wstring(1,(wchar_t)(L'0'+(c-0x06F0)));
            else if(c>=0x0660&&c<=0x0669) dig=std::wstring(1,(wchar_t)(L'0'+(c-0x0660)));
            if(!dig.empty()){
                if(idx<3) *parts[idx]+=dig;
                inTok=true;
            } else { // separator
                if(inTok && idx<2) idx++;
                inTok=false;
            }
        }
    } else {
        std::wstring all = digitsOnly(raw);
        if(all.size()>8) all=all.substr(0,8);
        if(all.size()<=4){ y=all; }
        else if(all.size()<=6){ y=all.substr(0,4); mo=all.substr(4); }
        else { y=all.substr(0,4); mo=all.substr(4,2); d=all.substr(6); }
    }
    // clamp month ≤ 12 and day ≤ 31 (only once a full field is present)
    if(mo.size()>=2){ int v=_wtoi(mo.c_str()); if(v>12)v=12; if(v<1)v=1;
                      wchar_t b[4]; swprintf(b,4,L"%d",v); mo=b; }
    if(d.size()>=2){  int v=_wtoi(d.c_str());  if(v>31)v=31; if(v<1)v=1;
                      wchar_t b[4]; swprintf(b,4,L"%d",v); d=b; }
}
// Build the displayed value. Non-padded segments are shown as typed; slashes
// are inserted between whichever segments already have content.
static std::wstring formatJalaliMask(const std::wstring& raw){
    std::wstring y,mo,d; bool sep;
    splitJalaliTokens(raw,y,mo,d,sep);
    std::wstring out = y;
    if(!mo.empty() || sep) out += L"/" + mo;
    if(!d.empty()  || (sep && !mo.empty())) out += L"/" + d;
    return out;
}
static LRESULT CALLBACK dateEditProc(HWND h, UINT m, WPARAM w, LPARAM l){
    if(m==WM_KEYDOWN && (w==VK_RETURN || w==VK_TAB)){
        hopField(h, w==VK_TAB && (GetKeyState(VK_SHIFT)&0x8000)!=0);
        return 0;
    }
    if(m==WM_KEYDOWN && w==VK_DELETE){
        // Delete key: if a range is selected, clear it; otherwise clear the
        // WHOLE field (the user wants "press Delete → everything in the birth
        // box is wiped" so a wrong date can be re-entered from scratch).
        DWORD a=0,b=0; SendMessageW(h,EM_GETSEL,(WPARAM)&a,(LPARAM)&b);
        wchar_t buf[64]; GetWindowTextW(h,buf,64);
        if(a!=b){
            std::wstring cur(buf);
            std::wstring keep = digitsOnly(cur.substr(0,a)) +
                (b<=cur.size()?digitsOnly(cur.substr(b)):L"");
            std::wstring fm=formatJalaliMask(keep);
            SetWindowTextW(h,fm.c_str());
            SendMessageW(h,EM_SETSEL,fm.size(),fm.size());
        } else {
            SetWindowTextW(h,L"");
        }
        return 0;
    }
    if(m==WM_CHAR){
        if(w==VK_RETURN || w==VK_TAB) return 0;            // no beep
        wchar_t buf[64]; GetWindowTextW(h,buf,64);
        std::wstring cur(buf);
        // v1.4.0 fix: when the user is EDITING an existing value (caret not at
        // the end, or a range is selected — e.g. clicked into the middle of a
        // pre-filled birth date), do NOT rebuild-from-end. Let the default edit
        // control handle the keystroke so the field is no longer "locked" or
        // erased. We only apply the auto-slash mask when typing at the very end
        // with no selection (fresh sequential entry).
        DWORD selA=0, selB=0;
        SendMessageW(h, EM_GETSEL, (WPARAM)&selA, (LPARAM)&selB);
        bool atEnd   = (selA==selB) && (selA==(DWORD)cur.size());
        bool hasRange= (selA!=selB);
        // v1.6.0 fix: Backspace must ALWAYS delete (a digit AND any auto slash
        // that precedes it) regardless of caret position, and a selected range
        // must be cleared completely — the old code stopped deleting once it hit
        // a "/" (it removed the slash, then re-inserted it, so the field looked
        // stuck). We now strip to a digit string, drop the last digit, and
        // re-mask, so the user can fully clear a wrong birth date.
        if(w==VK_BACK){
            if(hasRange){
                // delete the selection: keep digits OUTSIDE the selection
                std::wstring before = cur.substr(0, selA);
                std::wstring after  = (selB<=cur.size())?cur.substr(selB):L"";
                std::wstring digs = digitsOnly(before+after);
                std::wstring formatted = formatJalaliMask(digs);
                SetWindowTextW(h, formatted.c_str());
                SendMessageW(h, EM_SETSEL, formatted.size(), formatted.size());
                return 0;
            }
            // no selection: drop the last DIGIT (skipping any trailing slash)
            std::wstring digs = digitsOnly(cur);
            if(!digs.empty()) digs.pop_back();
            std::wstring formatted = formatJalaliMask(digs);
            SetWindowTextW(h, formatted.c_str());
            SendMessageW(h, EM_SETSEL, formatted.size(), formatted.size());
            return 0;
        }
        if(hasRange || !atEnd){
            // pass digits/separators through to normal editing; block letters
            wchar_t ch=(wchar_t)w;
            if(ch>=0x06F0&&ch<=0x06F9) ch=(wchar_t)(L'0'+(ch-0x06F0));
            else if(ch>=0x0660&&ch<=0x0669) ch=(wchar_t)(L'0'+(ch-0x0660));
            if((ch>=L'0'&&ch<=L'9')||ch==L'/')
                return CallWindowProcW(s_oldDate,h,m,(WPARAM)ch,l);
            return 0;   // ignore other chars while mid-edit
        }
        // Accept a SPACE or slash as an explicit field separator (relaxed entry
        // like "1340 5 20"). Map it to a single slash in the working buffer.
        if(w==L' ' || w==L'/' || w==L'-' || w==L'.'){
            if(cur.empty()) return 0;
            // avoid double separators
            if(cur.back()!=L'/') cur += L'/';
            SetWindowTextW(h, cur.c_str());
            SendMessageW(h, EM_SETSEL, cur.size(), cur.size());
            return 0;
        }
        // only digits beyond this point (latin / fa / ar)
        wchar_t ch = (wchar_t)w;
        if(ch>=0x06F0&&ch<=0x06F9) ch=(wchar_t)(L'0'+(ch-0x06F0));
        else if(ch>=0x0660&&ch<=0x0669) ch=(wchar_t)(L'0'+(ch-0x0660));
        if(ch<L'0' || ch>L'9') return 0;
        cur += ch;
        std::wstring formatted = formatJalaliMask(cur);
        SetWindowTextW(h, formatted.c_str());
        SendMessageW(h, EM_SETSEL, formatted.size(), formatted.size());
        return 0;
    }
    return CallWindowProcW(s_oldDate, h, m, w, l);
}
void enableDateMask(HWND ctl){
    WNDPROC old = (WNDPROC)SetWindowLongPtrW(ctl, GWLP_WNDPROC,
        (LONG_PTR)dateEditProc);
    if(!s_oldDate) s_oldDate = old;
}

// ============= automatic RTL / LTR alignment based on typed content =========
//  Persian app: a field that contains Persian/Arabic letters should read &
//  align RIGHT (RTL); a field that is Latin/digits only should align LEFT.
//  We flip WS_EX_RTLREADING/WS_EX_RIGHT (and the matching styles) live as the
//  user types, then re-apply on every change. Enter/Tab still navigate.
static WNDPROC s_oldDir = NULL;
static bool hasPersian(const std::wstring& s){
    for(wchar_t c : s){
        if((c>=0x0600 && c<=0x06FF) || (c>=0xFB50 && c<=0xFDFF) ||
           (c>=0xFE70 && c<=0xFEFF)){
            // treat Persian/Arabic DIGITS as neutral, letters as RTL
            if(c>=0x06F0 && c<=0x06F9) continue;
            if(c>=0x0660 && c<=0x0669) continue;
            return true;
        }
    }
    return false;
}
static void applyDir(HWND h){
    wchar_t buf[512]; GetWindowTextW(h,buf,512);
    bool rtl = hasPersian(buf);
    // empty → default to RTL (Persian app) so the caret sits on the right
    if(buf[0]==0) rtl=true;
    LONG ex = GetWindowLongW(h, GWL_EXSTYLE);
    LONG st = GetWindowLongW(h, GWL_STYLE);
    bool curRtl = (ex & WS_EX_RTLREADING)!=0;
    if(rtl==curRtl) return;            // no change needed
    if(rtl){ ex |= (WS_EX_RTLREADING|WS_EX_RIGHT); st &= ~ES_CENTER; st |= ES_RIGHT; }
    else   { ex &= ~(WS_EX_RTLREADING|WS_EX_RIGHT); st &= ~ES_RIGHT; st |= ES_LEFT; }
    DWORD selA=0,selB=0; SendMessageW(h,EM_GETSEL,(WPARAM)&selA,(LPARAM)&selB);
    SetWindowLongW(h, GWL_EXSTYLE, ex);
    SetWindowLongW(h, GWL_STYLE, st);
    SetWindowPos(h,NULL,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_FRAMECHANGED);
    InvalidateRect(h,NULL,TRUE);
    SendMessageW(h,EM_SETSEL,selA,selB);
}
static LRESULT CALLBACK dirEditProc(HWND h, UINT m, WPARAM w, LPARAM l){
    if(m==WM_KEYDOWN && w==VK_RETURN){ hopField(h,false); return 0; }
    if(m==WM_KEYDOWN && w==VK_TAB){
        hopField(h,(GetKeyState(VK_SHIFT)&0x8000)!=0); return 0; }
    if(m==WM_CHAR && (w==VK_RETURN || w==VK_TAB)) return 0;   // kill beep
    LRESULT r = CallWindowProcW(s_oldDir, h, m, w, l);
    if(m==WM_CHAR || m==WM_KEYUP || m==WM_PASTE || m==WM_CUT) applyDir(h);
    return r;
}
void enableAutoDir(HWND ctl){
    WNDPROC old=(WNDPROC)SetWindowLongPtrW(ctl,GWLP_WNDPROC,(LONG_PTR)dirEditProc);
    if(!s_oldDir) s_oldDir=old;
    applyDir(ctl);
}

// ================================================================ MAIN =====
int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, LPWSTR, int){
    g_hInst = hInst;

    // v1.10.0: declare per-monitor v2 DPI awareness as early as possible so the
    // manifest's PerMonitorV2 hint is honoured and crisp on mixed-DPI setups.
    // Done by dynamic lookup so the single EXE still loads on Windows 7/8
    // (where these entry points do not exist).
    {
        HMODULE u32=GetModuleHandleW(L"user32.dll");
        typedef BOOL (WINAPI* SetCtxFn)(HANDLE);
        if(u32){
            auto setCtx=(SetCtxFn)(void*)GetProcAddress(u32,"SetProcessDpiAwarenessContext");
            // DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 == (HANDLE)-4
            if(!setCtx || !setCtx((HANDLE)-4)){
                // fall back to per-monitor v1 via shcore, then system-DPI
                HMODULE sh=LoadLibraryW(L"shcore.dll");
                if(sh){
                    typedef HRESULT (WINAPI* SetAwFn)(int);
                    auto setAw=(SetAwFn)(void*)GetProcAddress(sh,"SetProcessDpiAwareness");
                    if(setAw) setAw(2 /*PROCESS_PER_MONITOR_DPI_AWARE*/);
                    FreeLibrary(sh);
                } else {
                    SetProcessDPIAware();   // legacy system-DPI fallback
                }
            }
        } else {
            SetProcessDPIAware();
        }
    }

    installCrashHandler();           // crash handler
    detectSpec();                    // speed handler
    BackupLog_Init();                // dedicated Backup Log channel (A.3)
    logLine(L"=== Azadi-Teb start v" APP_VERSION_W L" ===");
    writeSchemaVersion();            // §I: stamp data\.schema_version (informational only)

    // single instance — capture GetLastError() IMMEDIATELY after CreateMutexW,
    // before any other call can clobber the thread's last-error value (§G).
    CreateMutexW(NULL, TRUE, L"AzadiTeb_SingleInstance");
    DWORD muErr = GetLastError();
    if(muErr==ERROR_ALREADY_EXISTS){
        HWND ex=FindWindowW(APP_CLASS_W,NULL);
        if(ex) SetForegroundWindow(ex);
        return 0;
    }

    INITCOMMONCONTROLSEX icc={sizeof(icc),ICC_STANDARD_CLASSES|ICC_LISTVIEW_CLASSES|ICC_TAB_CLASSES};
    InitCommonControlsEx(&icc);

    // v1.17.0: first-run / prerequisite preparation splash. Installs the
    // Vazirmatn font and ensures data/ & logs/ exist (root cause of save
    // errors). The interface is now 100% native C++ (the HTML/MSHTML layer was
    // retired), so there is no browser-emulation registry key or MSHTML probe.
    // Shows a branded progress bar on first run / after a version bump; returns
    // instantly on subsequent runs.
    RunSetupSplash(hInst);
    gdipStartup();                   // v1.3.0: GDI+ rendering layer
    seedDefaultDepts();              // v1.4.1: ensure «پذیرش» category exists

    // responsive scale: based on monitor size + DPI
    HDC sdc=GetDC(NULL);
    int dpi = GetDeviceCaps(sdc,LOGPIXELSY);
    ReleaseDC(NULL,sdc);
    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);
    g_scale = dpi/96.0;
    double fit = sh/900.0;           // design height 900
    if(fit < g_scale) g_scale = fit; // shrink on small displays
    // UI density (set in the settings panel) — "compact" trims ~12% so more
    // fits on smaller screens; applied at launch.
    if(getSetting(L"density",L"normal")==L"compact") g_scale *= 0.88;
    if(g_scale < 0.62) g_scale = 0.62;
    if(g_scale > 2.00) g_scale = 2.00;

    applyTheme(getSetting(L"theme",L"light")==L"dark");
    buildFonts();
    registerFlatButton();

    WNDCLASSW wc={0};
    wc.lpfnWndProc = frameProc;
    wc.hInstance   = hInst;
    wc.hCursor     = LoadCursor(NULL,IDC_ARROW);
    wc.lpszClassName = APP_CLASS_W;
    wc.hIcon       = LoadIcon(NULL,IDI_APPLICATION);
    RegisterClassW(&wc);

    // true fullscreen borderless: WS_POPUP covering whole monitor, no menu bar
    HWND f = CreateWindowExW(0, APP_CLASS_W, APP_NAME_W,
        WS_POPUP|WS_CLIPCHILDREN, 0,0,sw,sh, NULL,NULL,hInst,NULL);
    ShowWindow(f, SW_SHOW);
    UpdateWindow(f);
    switchScreen(SC_HOME);
    SetFocus(f);
#ifdef AZ_DEBUG_BUILD
    { // TEMP: headless screenshot verification — jump straight into a screen
        wchar_t dbg[32]={0};
        GetEnvironmentVariableW(L"AZ_DEBUG_SCREEN", dbg, 32);
        if(dbg[0]){
            User u; u.username=L"reza"; u.fullname=L"رضا منشی";
            u.dept=L"پذیرش"; u.role=0;
            g_session.user=u; g_session.shift=detectShift();
            g_session.loginAt=iranNow();
            if(!wcscmp(dbg,L"reception")) switchScreen(SC_RECEPTION);
            else if(!wcscmp(dbg,L"manage")){   u.role=1; g_session.user=u;
                                               switchScreen(SC_MANAGE); }
            else if(!wcscmp(dbg,L"admin")){    u.role=2; g_session.user=u;
                                               switchScreen(SC_ADMIN); }
            else if(!wcscmp(dbg,L"settings")){ switchScreen(SC_RECEPTION);
                                               OpenSettings(f, g_session.user); }
            else if(!wcscmp(dbg,L"backup")){   u.role=1; g_session.user=u;
                                               switchScreen(SC_MANAGE);
                                               openBackupManager(f); }
            else if(!wcscmp(dbg,L"shift")){    int sh=0; showShiftDialog(f,sh); }
            // §D.5: headless smoke test for the print-designer open/close path.
            // Exercises the section-picker + designer launch without blocking on
            // user input, then exits 0 (path is reachable) or a non-zero code if
            // the launch helper crashed/was unreachable. Driven by build.sh when
            // AZ_SMOKE is set; production builds never define AZ_DEBUG_BUILD.
            else if(!wcscmp(dbg,L"print_designer")){
                u.role=1; g_session.user=u; switchScreen(SC_MANAGE);
                // Initialize the designer subsystems and verify the public
                // entry path is reachable without blocking on user input. The
                // section store + design store must seed cleanly; if any of this
                // faulted, the crash handler would have already aborted with a
                // non-zero code. Reaching here means the open path is healthy.
                void Sections_Init(); void Designs_Init();
                Sections_Init(); Designs_Init();
                logLine(L"SMOKE print_designer: subsystems initialized — OK");
                gdipShutdown(); BackupLog_Shutdown();
                return 0;
            }
        }
    }
#endif

    MSG msg;
    while(GetMessageW(&msg,NULL,0,0)){
        // global key routing
        if(msg.message==WM_KEYDOWN){
            HWND root=GetAncestor(msg.hwnd,GA_ROOT);
            if(root==g_hFrame){
                if(msg.wParam==VK_F8){
                    SendMessageW(g_hFrame, WM_KEYDOWN, msg.wParam, msg.lParam);
                    continue;
                }
                if((msg.wParam=='P'||msg.wParam=='N') &&
                   (GetKeyState(VK_CONTROL)&0x8000) &&
                   (GetKeyState('P')&0x8000) && (GetKeyState('N')&0x8000)){
                    SendMessageW(g_hFrame, WM_KEYDOWN, msg.wParam, msg.lParam);
                    continue;
                }
            }
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    gdipShutdown();
    BackupLog_Shutdown();
    logLine(L"=== Azadi-Teb exit ===");
    return 0;
}
