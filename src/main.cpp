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
static HWND s_bExit=0, s_bTheme=0, s_bUpdate=0, s_bSettings=0;
static HWND s_screen=0;
static ScreenId s_curScreen = SC_HOME;

#define ID_FR_EXIT     101
#define ID_FR_THEME    102
#define ID_FR_UPDATE   103
#define ID_FR_SETTINGS 104
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
//  v1.3.0 — taller header (LAYER 1) so the centered live clock + Jalali date
//  fit comfortably; thinner bottom status bar (clock moved up to the header).
static int topBarH(){ return S(72); }
static int botBarH(){ return S(40); }
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
        int cw=S(290), chh=S(170), gap=S(28);
        // vertical stack: logo(96) + 16 + title(46) + sub(28) + 40 + cards(170)
        int stackH = S(96)+S(16)+S(46)+S(28)+S(40)+chh;
        int yTop = (H-stackH)/2; if(yTop<S(10)) yTop=S(10);
        int yCards = yTop + S(96)+S(16)+S(46)+S(28)+S(40);
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
    case WM_APP_THEME: InvalidateRect(h,NULL,TRUE); return 0;
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
        int stackH = S(96)+S(16)+S(46)+S(28)+S(40)+chh;
        int yTop = (rc.bottom-stackH)/2; if(yTop<S(10)) yTop=S(10);

        // glass hero panel behind the centered content (open, layered look)
        int heroW=S(720); if(heroW>rc.right-S(40)) heroW=rc.right-S(40);
        RECT hero={rc.right/2-heroW/2, yTop-S(28),
                   rc.right/2+heroW/2, yTop+stackH-chh+S(8)};
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

        // title
        int yTitle = yTop + S(96) + S(16);
        SetTextColor(dc,g_theme.text);
        SelectObject(dc,g_fBig);
        RECT tr={0,yTitle,rc.right,yTitle+S(46)};
        DrawTextW(dc,L"سامانه پذیرش و مدیریت درمانگاه آزادی طب",-1,&tr,
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
static void frameLayout(HWND h){
    RECT rc; GetClientRect(h,&rc);
    int bh=S(38), pad=S(14);
    int y=(topBarH()-bh)/2;
    // --- RIGHT side (RTL primary): EXIT is the right-most control; the app
    //     identity (logo + name + fullname + access) is painted to its LEFT.
    MoveWindow(s_bExit,  rc.right-pad-bh, y, bh, bh, TRUE);
    // --- LEFT side: utility cluster — settings (gear), theme, update.
    MoveWindow(s_bSettings, pad,             y, bh, bh, TRUE);
    MoveWindow(s_bTheme,    pad+(bh+S(8)),   y, bh, bh, TRUE);
    MoveWindow(s_bUpdate,   pad+2*(bh+S(8)), y, bh, bh, TRUE);
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
        s_bTheme    = createFlatButton(h, ID_FR_THEME,   L"", g_dark?ICO_SUN:ICO_MOON, BS_GHOST,0,0,10,10);
        s_bUpdate   = createFlatButton(h, ID_FR_UPDATE,  L"", ICO_UPDATE, BS_GHOST,0,0,10,10);
        SetTimer(h, TIMER_CLOCK, g_lowSpec?1000:500, NULL);
        return 0;
    case WM_SIZE: frameLayout(h); return 0;
    case WM_APP_THEME:
        // theme may have been switched from inside the settings panel — keep the
        // header's theme button glyph in sync and repaint the whole frame.
        setFlatButtonIcon(s_bTheme, g_dark?ICO_SUN:ICO_MOON);
        InvalidateRect(h,NULL,TRUE);
        return 0;
    case WM_TIMER:
        if(w==TIMER_CLOCK){
            // repaint only the centered clock/date zone in the top header
            RECT rc; GetClientRect(h,&rc);
            RECT cz={rc.right/2-S(260), 0, rc.right/2+S(260), topBarH()};
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
        else if(id==ID_FR_SETTINGS) openSettingsPanel(h);
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

        // ===================== LAYER 1 — top header bar =====================
        // Soft vertical gradient so the header reads as a distinct, polished
        // surface (not a flat strip).
        RECT tb={0,0,rc.right,topBarH()};
        gpGradRoundRect(dc,tb,0,g_theme.headerTop,g_theme.headerBot,CLR_INVALID);
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
        int logoCy = topBarH()/2;
        RECT lc={logoCx-logoR,logoCy-logoR,logoCx+logoR,logoCy+logoR};
        gpGradRoundRect(dc,lc,logoR,g_theme.accent2,g_theme.accent,CLR_INVALID);
        RECT li={lc.left+S(7),lc.top+S(7),lc.right-S(7),lc.bottom-S(7)};
        drawIcon(dc,ICO_CROSS_MED,li,RGB(255,255,255),S(2));

        int idRight = logoCx-logoR-S(12);
        bool loggedIn = !g_session.user.username.empty();
        if(loggedIn){
            // two stacked lines: app name (top) + person/role (bottom)
            SelectObject(dc,g_fUIB);
            SetTextColor(dc,g_theme.text);
            RECT nr={S(160),S(8),idRight,S(8)+S(24)};
            DrawTextW(dc,APP_NAME_W,-1,&nr,
                DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
            SelectObject(dc,g_fSmall);
            SetTextColor(dc,g_theme.textDim);
            const wchar_t* role =
                g_session.user.role==2 ? L"مدیر سامانه" :
                g_session.user.role==1 ? L"مدیریت درمانگاه" : L"پذیرش درمانگاه";
            std::wstring sub = g_session.user.fullname + L"  •  " + role +
                (g_session.user.dept.empty()?L"":(L"  •  "+g_session.user.dept));
            RECT sr={S(160),S(8)+S(24),idRight,topBarH()-S(8)};
            DrawTextW(dc,sub.c_str(),-1,&sr,
                DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
        } else {
            SelectObject(dc,g_fTitle);
            SetTextColor(dc,g_theme.text);
            RECT nr={S(160),0,idRight,topBarH()};
            DrawTextW(dc,APP_NAME_W,-1,&nr,
                DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
        }

        // ===== CENTER of LAYER 1: live clock (bold, top) + Jalali date =====
        SYSTEMTIME st=iranNow();
        // clock — big & bold, perfectly centered
        SetTextColor(dc,g_theme.accent);
        SelectObject(dc,g_fMono);
        RECT ck={rc.right/2-S(220),S(6),rc.right/2+S(220),S(6)+S(34)};
        DrawTextW(dc,toFaDigits(iranTimeStr(st,true)).c_str(),-1,&ck,
            DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_NOPREFIX);
        // date — centered just below the clock
        SetTextColor(dc,g_theme.textDim);
        SelectObject(dc,g_fSmall);
        RECT dr={rc.right/2-S(260),S(6)+S(34),rc.right/2+S(260),topBarH()-S(4)};
        DrawTextW(dc,jalaliDateStr(st).c_str(),-1,&dr,
            DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);

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
    if(m==WM_CHAR){
        if(w==VK_RETURN || w==VK_TAB) return 0;            // no beep
        wchar_t buf[64]; GetWindowTextW(h,buf,64);
        std::wstring cur(buf);
        if(w==VK_BACK){
            if(!cur.empty()) cur.pop_back();
            // re-normalise after deletion
            std::wstring formatted = formatJalaliMask(cur);
            SetWindowTextW(h, formatted.c_str());
            SendMessageW(h, EM_SETSEL, formatted.size(), formatted.size());
            return 0;
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
    gdipStartup();                   // v1.3.0: GDI+ rendering layer

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
            else if(!wcscmp(dbg,L"manage"))    switchScreen(SC_MANAGE);
            else if(!wcscmp(dbg,L"settings")){ switchScreen(SC_RECEPTION);
                                               openSettingsPanel(f); }
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
    logLine(L"=== Azadi-Teb exit ===");
    return 0;
}
