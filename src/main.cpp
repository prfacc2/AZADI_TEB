// ============================================================================
//  main.cpp — entry point, fullscreen frame (no menu/title bar), home screen,
//  live Iran clock/date (bottom-right), hidden admin combo Ctrl+P+N,
//  global F8 = print last receipt
// ============================================================================
#include "app.h"
#include <stdio.h>

HINSTANCE g_hInst = NULL;
HWND      g_hFrame = NULL;
double    g_scale = 1.0;
Session   g_session;

HFONT g_fUI=0, g_fUIB=0, g_fSmall=0, g_fTitle=0, g_fBig=0, g_fHuge=0, g_fMono=0;

// frame children
static HWND s_bExit=0, s_bTheme=0, s_bUpdate=0;
static HWND s_screen=0;
static ScreenId s_curScreen = SC_HOME;

#define ID_FR_EXIT   101
#define ID_FR_THEME  102
#define ID_FR_UPDATE 103
#define TIMER_CLOCK  1

// ------------------------------------------------------------------ fonts --
static HFONT mkFont(int px, int weight){
    return CreateFontW(-S(px),0,0,0,weight,0,0,0,DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,
        g_lowSpec?DEFAULT_QUALITY:CLEARTYPE_QUALITY,
        DEFAULT_PITCH,L"Vazirmatn");
}
static void buildFonts(){
    if(g_fUI)   DeleteObject(g_fUI);
    if(g_fUIB)  DeleteObject(g_fUIB);
    if(g_fSmall)DeleteObject(g_fSmall);
    if(g_fTitle)DeleteObject(g_fTitle);
    if(g_fBig)  DeleteObject(g_fBig);
    if(g_fHuge) DeleteObject(g_fHuge);
    if(g_fMono) DeleteObject(g_fMono);
    g_fUI    = mkFont(15, FW_NORMAL);
    g_fUIB   = mkFont(15, FW_BOLD);
    g_fSmall = mkFont(12, FW_NORMAL);
    g_fTitle = mkFont(19, FW_BOLD);
    g_fBig   = mkFont(30, FW_BOLD);
    g_fHuge  = mkFont(38, FW_BOLD);
    g_fMono  = mkFont(24, FW_BOLD);
}

// ------------------------------------------------------------- frame rects -
static int topBarH(){ return S(56); }
static int botBarH(){ return S(64); }
RECT frameContentRect(){
    RECT rc; GetClientRect(g_hFrame,&rc);
    rc.top += topBarH(); rc.bottom -= botBarH();
    return rc;
}

// ----------------------------------------------------------- screen switch -
void switchScreen(ScreenId id){
    if(s_screen){ DestroyWindow(s_screen); s_screen=0; }
    s_curScreen = id;
    switch(id){
        case SC_HOME:      s_screen = createHomeScreen(g_hFrame); break;
        case SC_RECEPTION: s_screen = createReceptionScreen(g_hFrame); break;
        case SC_ADMIN:     s_screen = createAdminScreen(g_hFrame); break;
        case SC_MANAGE:    s_screen = createManageScreen(g_hFrame); break;
    }
    RECT rc = frameContentRect();
    if(s_screen)
        MoveWindow(s_screen, rc.left, rc.top, rc.right-rc.left, rc.bottom-rc.top, TRUE);
    InvalidateRect(g_hFrame, NULL, TRUE);
}

// ================================================================ HOME =====
#define HM_CLASS L"AzHome"
#define ID_HM_RECEPTION 111
#define ID_HM_MANAGE    112

static LRESULT CALLBACK homeProc(HWND h, UINT m, WPARAM w, LPARAM l){
    switch(m){
    case WM_CREATE:
        createFlatButton(h, ID_HM_RECEPTION, L"پذیرش درمانگاه", ICO_CROSS_MED,
            BS_CARD, 0,0,10,10, L"ثبت پذیرش بیمار و صدور قبض");
        createFlatButton(h, ID_HM_MANAGE, L"پنل مدیریت درمانگاه", ICO_SHIELD,
            BS_CARD, 0,0,10,10, L"گزارش‌ها و مدیریت سامانه");
        return 0;
    case WM_SIZE: {
        int W=LOWORD(l), H=HIWORD(l);
        int cw=S(280), chh=S(170), gap=S(28);
        // vertical stack: logo(88) + 14 + title(44) + sub(28) + 36 + cards(170)
        int stackH = S(88)+S(14)+S(44)+S(28)+S(36)+chh;
        int yTop = (H-stackH)/2; if(yTop<S(10)) yTop=S(10);
        int yCards = yTop + S(88)+S(14)+S(44)+S(28)+S(36);
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
        FillRect(dc,&rc,g_brBg);
        SetBkMode(dc,TRANSPARENT);

        // ---- same vertical stack math as WM_SIZE ----
        // stack: logo(88) + 14 + title(44) + sub(28) + 36 + cards(170)
        int chh=S(170);
        int stackH = S(88)+S(14)+S(44)+S(28)+S(36)+chh;
        int yTop = (rc.bottom-stackH)/2; if(yTop<S(10)) yTop=S(10);

        // logo circle (88px tall, centered horizontally)
        int r = S(44);
        int cy = yTop + r;                 // logo center
        RECT lc={rc.right/2-r, cy-r, rc.right/2+r, cy+r};
        fillRoundRect(dc,lc,4*r,g_theme.accent,CLR_INVALID);
        RECT li={lc.left+S(22),lc.top+S(22),lc.right-S(22),lc.bottom-S(22)};
        drawIcon(dc,ICO_CROSS_MED,li,RGB(255,255,255),S(2)+1);

        // title (44px band, starts 14px under logo)
        int yTitle = yTop + S(88) + S(14);
        SetTextColor(dc,g_theme.text);
        SelectObject(dc,g_fBig);
        RECT tr={0,yTitle,rc.right,yTitle+S(44)};
        DrawTextW(dc,L"سامانه پذیرش و مدیریت درمانگاه آزادی طب",-1,&tr,
            DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);

        // subtitle (28px band, right under title)
        int ySub = yTitle + S(44);
        SelectObject(dc,g_fUI);
        SetTextColor(dc,g_theme.textDim);
        RECT sr={0,ySub,rc.right,ySub+S(28)};
        DrawTextW(dc,L"نوع کاربری خود را انتخاب کنید",-1,&sr,
            DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);

        // hint
        SelectObject(dc,g_fSmall);
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
static void frameLayout(HWND h){
    RECT rc; GetClientRect(h,&rc);
    int bh=S(36), pad=S(10);
    int y=(topBarH()-bh)/2;
    // exit: top-right
    MoveWindow(s_bExit, rc.right-pad-bh, y, bh, bh, TRUE);
    MoveWindow(s_bTheme, rc.right-pad-2*bh-S(8), y, bh, bh, TRUE);
    MoveWindow(s_bUpdate, rc.right-pad-3*bh-S(16), y, bh, bh, TRUE);
    if(s_screen){
        RECT cr=frameContentRect();
        MoveWindow(s_screen,cr.left,cr.top,cr.right-cr.left,cr.bottom-cr.top,TRUE);
    }
}

static LRESULT CALLBACK frameProc(HWND h, UINT m, WPARAM w, LPARAM l){
    switch(m){
    case WM_CREATE:
        g_hFrame = h;
        s_bExit   = createFlatButton(h, ID_FR_EXIT,  L"", ICO_X,      BS_GHOST,0,0,10,10);
        s_bTheme  = createFlatButton(h, ID_FR_THEME, L"", g_dark?ICO_SUN:ICO_MOON, BS_GHOST,0,0,10,10);
        s_bUpdate = createFlatButton(h, ID_FR_UPDATE,L"", ICO_UPDATE, BS_GHOST,0,0,10,10);
        SetTimer(h, TIMER_CLOCK, g_lowSpec?1000:500, NULL);
        return 0;
    case WM_SIZE: frameLayout(h); return 0;
    case WM_TIMER:
        if(w==TIMER_CLOCK){
            // repaint only the clock zone (bottom-right)
            RECT rc; GetClientRect(h,&rc);
            RECT cz={rc.right-S(380), rc.bottom-botBarH(), rc.right, rc.bottom};
            InvalidateRect(h,&cz,FALSE);
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
                    g_session = Session();
                    switchScreen(SC_HOME);
                }
            }
        }
        else if(id==ID_FR_THEME){
            applyTheme(!g_dark);
            // v1.1.0: swap icon in place — never destroy the button that
            // generated this very command (use-after-free crash).
            setFlatButtonIcon(s_bTheme, g_dark?ICO_SUN:ICO_MOON);
            broadcastThemeChange();
        }
        else if(id==ID_FR_UPDATE) checkRemoteUpdate(h);
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

        // top bar
        RECT tb={0,0,rc.right,topBarH()};
        FillRect(dc,&tb,g_brSurface2);
        // bottom bar
        RECT bb={0,rc.bottom-botBarH(),rc.right,rc.bottom};
        FillRect(dc,&bb,g_brSurface2);
        // middle bg
        RECT mid={0,topBarH(),rc.right,rc.bottom-botBarH()};
        FillRect(dc,&mid,g_brBg);
        // separators
        HPEN pen=CreatePen(PS_SOLID,1,g_theme.border);
        HGDIOBJ op=SelectObject(dc,pen);
        MoveToEx(dc,0,topBarH()-1,0); LineTo(dc,rc.right,topBarH()-1);
        MoveToEx(dc,0,rc.bottom-botBarH(),0); LineTo(dc,rc.right,rc.bottom-botBarH());
        SelectObject(dc,op); DeleteObject(pen);

        SetBkMode(dc,TRANSPARENT);
        // app name (top-left)
        SelectObject(dc,g_fUIB);
        SetTextColor(dc,g_theme.text);
        RECT nr={S(16),0,S(460),topBarH()};
        std::wstring caption = std::wstring(APP_NAME_W) +
            (g_session.user.username.empty() ? L"" :
             L"  \u2014  " + g_session.user.fullname);
        DrawTextW(dc,caption.c_str(),-1,&nr,
            DT_LEFT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);

        // ===== bottom-right: live Iran clock (top) + date (below) =====
        SYSTEMTIME st=iranNow();
        SetTextColor(dc,g_theme.accent);
        SelectObject(dc,g_fMono);
        RECT ck={rc.right-S(370),rc.bottom-botBarH()+S(2),rc.right-S(16),rc.bottom-S(28)};
        DrawTextW(dc,toFaDigits(iranTimeStr(st,true)).c_str(),-1,&ck,
            DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_NOPREFIX);
        SetTextColor(dc,g_theme.textDim);
        SelectObject(dc,g_fSmall);
        RECT dr={rc.right-S(370),rc.bottom-S(28),rc.right-S(16),rc.bottom-S(4)};
        DrawTextW(dc,jalaliDateStr(st).c_str(),-1,&dr,
            DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);

        // bottom-left: shift indicator
        SetTextColor(dc,g_theme.textDim);
        SelectObject(dc,g_fSmall);
        RECT shf={S(16),rc.bottom-botBarH(),S(460),rc.bottom};
        std::wstring sf = L"شیفت جاری: " + shiftName(detectShift());
        if(!g_session.user.username.empty() && s_curScreen==SC_RECEPTION)
            sf += L"   |   شیفت ورود: " + shiftName(g_session.shift);
        DrawTextW(dc,sf.c_str(),-1,&shf,
            DT_LEFT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);

        BitBlt(dc0,0,0,rc.right,rc.bottom,dc,0,0,SRCCOPY);
        SelectObject(dc,obm); DeleteObject(bmp); DeleteDC(dc);
        EndPaint(h,&ps);
        return 0; }
    case WM_DESTROY:
        KillTimer(h,TIMER_CLOCK);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(h,m,w,l);
}

// ============================================== Enter => next field ========
static WNDPROC s_oldEdit = NULL;
static LRESULT CALLBACK enterEditProc(HWND h, UINT m, WPARAM w, LPARAM l){
    if(m==WM_KEYDOWN && w==VK_RETURN){
        HWND top = GetAncestor(h, GA_ROOT);
        HWND parent = GetParent(h);
        HWND nxt = GetNextDlgTabItem(parent, h, FALSE);
        if(!nxt || nxt==h){
            // search in grandparent (nested panels)
            nxt = GetNextDlgTabItem(top, h, FALSE);
        }
        if(nxt && nxt!=h){ SetFocus(nxt); SendMessageW(nxt, EM_SETSEL, 0, -1); }
        return 0;
    }
    if(m==WM_CHAR && w==VK_RETURN) return 0;  // kill the beep
    return CallWindowProcW(s_oldEdit, h, m, w, l);
}
void enableEnterNavigation(HWND ctl){
    WNDPROC old = (WNDPROC)SetWindowLongPtrW(ctl, GWLP_WNDPROC,
        (LONG_PTR)enterEditProc);
    if(!s_oldEdit) s_oldEdit = old;
}

// ================================================================ MAIN =====
int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, LPWSTR, int){
    g_hInst = hInst;

    installCrashHandler();           // crash handler
    detectSpec();                    // speed handler
    logLine(L"=== Azadi-Teb start v" APP_VERSION_W L" ===");

    // single instance
    CreateMutexW(NULL, TRUE, L"AzadiTeb_SingleInstance");
    if(GetLastError()==ERROR_ALREADY_EXISTS){
        HWND ex=FindWindowW(APP_CLASS_W,NULL);
        if(ex) SetForegroundWindow(ex);
        return 0;
    }

    INITCOMMONCONTROLSEX icc={sizeof(icc),ICC_STANDARD_CLASSES|ICC_LISTVIEW_CLASSES|ICC_TAB_CLASSES};
    InitCommonControlsEx(&icc);

    installVazirFont();              // embedded Vazirmatn + per-user install

    // responsive scale: based on monitor size + DPI
    HDC sdc=GetDC(NULL);
    int dpi = GetDeviceCaps(sdc,LOGPIXELSY);
    ReleaseDC(NULL,sdc);
    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);
    g_scale = dpi/96.0;
    double fit = sh/900.0;           // design height 900
    if(fit < g_scale) g_scale = fit; // shrink on small displays
    if(g_scale < 0.70) g_scale = 0.70;
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
    logLine(L"=== Azadi-Teb exit ===");
    return 0;
}
