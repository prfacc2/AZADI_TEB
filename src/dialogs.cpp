// ============================================================================
//  dialogs.cpp — Win11-style centered login card + shift selection dialog
// ============================================================================
#include "app.h"
#include <stdio.h>

// =============================================================== LOGIN =====
//  Modal overlay: dim layer over frame, rounded card in center (Win11 look).
#define LGN_CLASS L"AzLogin"
#define ID_LG_USER  201
#define ID_LG_PASS  202
#define ID_LG_OK    203
#define ID_LG_CANCEL 204

struct LoginData {
    int role; User* out; bool* ok;
    HWND eUser, ePass, bOk, bCancel;
    std::wstring errMsg;
    int shake;                         // error shake animation offset
};

static const wchar_t* roleTitle(int r){
    switch(r){
        case 0: return L"ورود به پذیرش درمانگاه";
        case 1: return L"ورود به پنل مدیریت";
        default:return L"پنل مخفی ادمین";
    }
}

static void loginLayout(HWND h, LoginData* d){
    RECT rc; GetClientRect(h,&rc);
    int cw=S(420), chh=S(430);
    int cx=(rc.right-cw)/2, cy=(rc.bottom-chh)/2;
    int ew=cw-S(80), ex=cx+S(40);
    MoveWindow(d->eUser, ex, cy+S(150), ew, S(40), TRUE);
    MoveWindow(d->ePass, ex, cy+S(235), ew, S(40), TRUE);
    MoveWindow(d->bOk,    ex, cy+S(320), ew, S(46), TRUE);
    MoveWindow(d->bCancel,ex, cy+S(374), ew, S(38), TRUE);
}

static LRESULT CALLBACK loginProc(HWND h, UINT m, WPARAM w, LPARAM l){
    LoginData* d=(LoginData*)GetWindowLongPtrW(h,GWLP_USERDATA);
    switch(m){
    case WM_CREATE: {
        CREATESTRUCTW* cs=(CREATESTRUCTW*)l;
        d=(LoginData*)cs->lpCreateParams;
        SetWindowLongPtrW(h,GWLP_USERDATA,(LONG_PTR)d);
        DWORD es = WS_CHILD|WS_VISIBLE|ES_AUTOHSCROLL;
        d->eUser = CreateWindowExW(0,L"EDIT",L"",es,0,0,10,10,h,(HMENU)ID_LG_USER,g_hInst,0);
        d->ePass = CreateWindowExW(0,L"EDIT",L"",es|ES_PASSWORD,0,0,10,10,h,(HMENU)ID_LG_PASS,g_hInst,0);
        SendMessageW(d->eUser,WM_SETFONT,(WPARAM)g_fUI,TRUE);
        SendMessageW(d->ePass,WM_SETFONT,(WPARAM)g_fUI,TRUE);
        d->bOk     = createFlatButton(h,ID_LG_OK,L"ورود",ICO_CHECK,BS_PRIMARY,0,0,10,10);
        d->bCancel = createFlatButton(h,ID_LG_CANCEL,L"انصراف",0,BS_OUTLINE,0,0,10,10);
        loginLayout(h,d);
        SetFocus(d->eUser);
        return 0; }
    case WM_SIZE: if(d) loginLayout(h,d); return 0;
    case WM_CTLCOLOREDIT: {
        HDC dc=(HDC)w;
        SetTextColor(dc,g_theme.inputText);
        SetBkColor(dc,g_theme.inputBg);
        return (LRESULT)g_brInput; }
    case WM_CTLCOLORSTATIC: {
        HDC dc=(HDC)w;
        SetBkColor(dc,g_theme.surface);
        return (LRESULT)g_brSurface; }
    case WM_COMMAND: {
        int id=LOWORD(w);
        if(id==ID_LG_OK || (id==ID_LG_PASS && HIWORD(w)==0)){
            wchar_t ub[128], pb[128];
            GetWindowTextW(d->eUser,ub,128);
            GetWindowTextW(d->ePass,pb,128);
            std::wstring err;
            if(verifyLogin(trim(ub),pb,d->role,*d->out,err)){
                *d->ok = true; DestroyWindow(h);
            } else {
                d->errMsg = err; d->shake = 8;
                SetTimer(h, 7, 30, NULL);
                InvalidateRect(h,NULL,TRUE);
            }
        } else if(id==ID_LG_CANCEL){
            *d->ok=false; DestroyWindow(h);
        }
        return 0; }
    case WM_TIMER:
        if(w==7 && d){
            d->shake--;
            if(d->shake<=0){ d->shake=0; KillTimer(h,7); }
            InvalidateRect(h,NULL,TRUE);
        }
        return 0;
    case WM_KEYDOWN:
        if(w==VK_ESCAPE){ *d->ok=false; DestroyWindow(h); return 0; }
        break;
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps; HDC dc0=BeginPaint(h,&ps);
        RECT rc; GetClientRect(h,&rc);
        HDC dc=CreateCompatibleDC(dc0);
        HBITMAP bmp=CreateCompatibleBitmap(dc0,rc.right,rc.bottom);
        HGDIOBJ obm=SelectObject(dc,bmp);
        // dim layer
        HBRUSH dim=CreateSolidBrush(g_dark?RGB(8,10,14):RGB(160,170,184));
        FillRect(dc,&rc,dim); DeleteObject(dim);

        int cw=S(420), chh=S(430);
        int sh = d ? (d->shake%2 ? S(6) : -S(6)) * (d->shake>0?1:0) : 0;
        int cx=(rc.right-cw)/2 + sh, cy=(rc.bottom-chh)/2;
        RECT card={cx,cy,cx+cw,cy+chh};
        // shadow
        RECT shd=card; OffsetRect(&shd,0,S(6));
        fillRoundRect(dc,shd,S(18),g_dark?RGB(5,7,10):RGB(196,204,216),CLR_INVALID);
        fillRoundRect(dc,card,S(18),g_theme.surface,g_theme.border);

        SetBkMode(dc,TRANSPARENT);
        // icon circle
        int ir=S(28);
        RECT ic={cx+cw/2-ir,cy+S(28),cx+cw/2+ir,cy+S(28)+2*ir};
        fillRoundRect(dc,ic,4*ir, d->role==2?g_theme.danger:g_theme.accent, CLR_INVALID);
        RECT ii={ic.left+S(14),ic.top+S(14),ic.right-S(14),ic.bottom-S(14)};
        drawIcon(dc, d->role==2?ICO_SHIELD:ICO_USER, ii, RGB(255,255,255), S(2)+1);

        SetTextColor(dc,g_theme.text);
        SelectObject(dc,g_fTitle);
        RECT tr={cx,cy+S(92),cx+cw,cy+S(124)};
        DrawTextW(dc,roleTitle(d->role),-1,&tr,
            DT_CENTER|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);

        SelectObject(dc,g_fSmall);
        SetTextColor(dc,g_theme.textDim);
        RECT lu={cx+S(40),cy+S(126),cx+cw-S(40),cy+S(148)};
        DrawTextW(dc,L"نام کاربری",-1,&lu,DT_RIGHT|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);
        RECT lp={cx+S(40),cy+S(211),cx+cw-S(40),cy+S(233)};
        DrawTextW(dc,L"رمز عبور",-1,&lp,DT_RIGHT|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);

        if(d && !d->errMsg.empty()){
            SetTextColor(dc,g_theme.danger);
            RECT er={cx+S(40),cy+S(283),cx+cw-S(40),cy+S(312)};
            DrawTextW(dc,d->errMsg.c_str(),-1,&er,
                DT_CENTER|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);
        }
        BitBlt(dc0,0,0,rc.right,rc.bottom,dc,0,0,SRCCOPY);
        SelectObject(dc,obm); DeleteObject(bmp); DeleteDC(dc);
        EndPaint(h,&ps);
        return 0; }
    case WM_DESTROY:
        PostQuitMessage(0);   // breaks the modal loop below (filtered)
        return 0;
    }
    return DefWindowProcW(h,m,w,l);
}

static bool s_lgnReg=false;
static void regLogin(){
    if(s_lgnReg) return;
    WNDCLASSW wc={0};
    wc.lpfnWndProc=loginProc; wc.hInstance=g_hInst;
    wc.hCursor=LoadCursor(NULL,IDC_ARROW);
    wc.lpszClassName=LGN_CLASS;
    RegisterClassW(&wc);
    s_lgnReg=true;
}

//  Runs a nested message loop until the overlay closes.
static void runModal(HWND overlay, HWND parent){
    EnableWindow(parent, FALSE);
    MSG msg;
    while(IsWindow(overlay) && GetMessageW(&msg,NULL,0,0)){
        if(msg.message==WM_QUIT) break;
        // Tab navigation inside overlay
        if(msg.message==WM_KEYDOWN && msg.wParam==VK_TAB){
            HWND f=GetFocus();
            HWND nxt=GetNextDlgTabItem(overlay,f,(GetKeyState(VK_SHIFT)&0x8000)?TRUE:FALSE);
            if(nxt){ SetFocus(nxt); continue; }
        }
        if(msg.message==WM_KEYDOWN && msg.wParam==VK_RETURN){
            HWND f=GetFocus();
            wchar_t cls[32]; GetClassNameW(f,cls,32);
            if(!wcscmp(cls,L"EDIT")){
                // move focus or submit
                SendMessageW(overlay, WM_COMMAND, MAKEWPARAM(GetDlgCtrlID(f)==ID_LG_USER?0:ID_LG_OK,0),0);
                if(GetDlgCtrlID(f)==ID_LG_USER){
                    SetFocus(GetNextDlgTabItem(overlay,f,FALSE));
                }
                continue;
            }
        }
        if(msg.message==WM_KEYDOWN && msg.wParam==VK_ESCAPE){
            SendMessageW(overlay, WM_KEYDOWN, VK_ESCAPE, 0);
            continue;
        }
        TranslateMessage(&msg); DispatchMessageW(&msg);
    }
    EnableWindow(parent, TRUE);
    SetForegroundWindow(parent);
}

bool showLoginDialog(HWND parent, int role, User& out){
    regLogin();
    bool ok=false;
    LoginData d; d.role=role; d.out=&out; d.ok=&ok; d.shake=0;
    RECT rc; GetClientRect(parent,&rc);
    HWND ov = CreateWindowExW(0,LGN_CLASS,L"",WS_CHILD|WS_VISIBLE|WS_CLIPCHILDREN,
        0,0,rc.right,rc.bottom,parent,NULL,g_hInst,&d);
    SetWindowPos(ov,HWND_TOP,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE);
    SetFocus(d.eUser);
    runModal(ov, parent);
    return ok;
}

// =============================================================== SHIFT =====
#define SH_CLASS L"AzShift"
#define ID_SH_AUTO   301
#define ID_SH_S0     302
#define ID_SH_S1     303
#define ID_SH_S2     304
#define ID_SH_OK     305
#define ID_SH_CANCEL 306

struct ShiftData {
    int* shift; bool* ok;
    bool autoMode;
    int sel;
    HWND bAuto, b0, b1, b2, bOk, bCancel;
};
static void shiftRefresh(ShiftData* d){
    int as = detectShift();
    if(d->autoMode) d->sel = as;
    const wchar_t* names[3]={L"شیفت صبح  (۶:۰۰ تا ۱۴:۳۰)",
                             L"شیفت بعد از ظهر  (۱۴:۳۰ تا ۲۲:۳۰)",
                             L"شیفت شب  (۲۲:۳۰ تا ۶:۰۰)"};
    HWND bs[3]={d->b0,d->b1,d->b2};
    for(int i=0;i<3;i++){
        std::wstring t = names[i];
        if(i==d->sel) t = L"\u25CF  " + t;
        SetWindowTextW(bs[i], t.c_str());
        EnableWindow(bs[i], !d->autoMode);
    }
    SetWindowTextW(d->bAuto, d->autoMode
        ? L"\u2611  حالت خودکار فعال است — تشخیص بر اساس ساعت ایران"
        : L"\u2610  حالت خودکار (برای انتخاب دستی، تیک را بردارید)");
}
static void shiftLayout(HWND h, ShiftData* d){
    RECT rc; GetClientRect(h,&rc);
    int cw=S(460), chh=S(420);
    int cx=(rc.right-cw)/2, cy=(rc.bottom-chh)/2;
    int ew=cw-S(60), ex=cx+S(30);
    MoveWindow(d->bAuto, ex, cy+S(96),  ew, S(44), TRUE);
    MoveWindow(d->b0,    ex, cy+S(156), ew, S(48), TRUE);
    MoveWindow(d->b1,    ex, cy+S(212), ew, S(48), TRUE);
    MoveWindow(d->b2,    ex, cy+S(268), ew, S(48), TRUE);
    MoveWindow(d->bOk,   ex, cy+S(338), ew/2-S(6), S(46), TRUE);
    MoveWindow(d->bCancel, ex+ew/2+S(6), cy+S(338), ew/2-S(6), S(46), TRUE);
}
static LRESULT CALLBACK shiftProc(HWND h, UINT m, WPARAM w, LPARAM l){
    ShiftData* d=(ShiftData*)GetWindowLongPtrW(h,GWLP_USERDATA);
    switch(m){
    case WM_CREATE: {
        CREATESTRUCTW* cs=(CREATESTRUCTW*)l;
        d=(ShiftData*)cs->lpCreateParams;
        SetWindowLongPtrW(h,GWLP_USERDATA,(LONG_PTR)d);
        d->bAuto  = createFlatButton(h,ID_SH_AUTO,L"",0,BS_OUTLINE,0,0,10,10);
        d->b0     = createFlatButton(h,ID_SH_S0,L"",0,BS_OUTLINE,0,0,10,10);
        d->b1     = createFlatButton(h,ID_SH_S1,L"",0,BS_OUTLINE,0,0,10,10);
        d->b2     = createFlatButton(h,ID_SH_S2,L"",0,BS_OUTLINE,0,0,10,10);
        d->bOk    = createFlatButton(h,ID_SH_OK,L"تأیید و ورود",ICO_CHECK,BS_PRIMARY,0,0,10,10);
        d->bCancel= createFlatButton(h,ID_SH_CANCEL,L"انصراف",0,BS_OUTLINE,0,0,10,10);
        shiftRefresh(d);
        shiftLayout(h,d);
        SetTimer(h, 9, 30000, NULL);  // re-detect every 30s while open
        return 0; }
    case WM_TIMER: if(w==9 && d && d->autoMode){ shiftRefresh(d); InvalidateRect(h,NULL,TRUE);} return 0;
    case WM_SIZE: if(d) shiftLayout(h,d); return 0;
    case WM_COMMAND: {
        int id=LOWORD(w);
        switch(id){
        case ID_SH_AUTO:
            d->autoMode = !d->autoMode;
            setSetting(L"shift_auto", d->autoMode?L"1":L"0");   // remember!
            shiftRefresh(d); InvalidateRect(h,NULL,TRUE);
            break;
        case ID_SH_S0: case ID_SH_S1: case ID_SH_S2:
            if(!d->autoMode){ d->sel = id-ID_SH_S0; shiftRefresh(d); }
            break;
        case ID_SH_OK:
            *d->shift = d->autoMode ? detectShift() : d->sel;
            *d->ok = true; KillTimer(h,9); DestroyWindow(h);
            break;
        case ID_SH_CANCEL:
            *d->ok=false; KillTimer(h,9); DestroyWindow(h);
            break;
        }
        return 0; }
    case WM_CTLCOLORSTATIC: {
        HDC dc=(HDC)w; SetBkColor(dc,g_theme.surface);
        return (LRESULT)g_brSurface; }
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps; HDC dc0=BeginPaint(h,&ps);
        RECT rc; GetClientRect(h,&rc);
        HDC dc=CreateCompatibleDC(dc0);
        HBITMAP bmp=CreateCompatibleBitmap(dc0,rc.right,rc.bottom);
        HGDIOBJ obm=SelectObject(dc,bmp);
        HBRUSH dim=CreateSolidBrush(g_dark?RGB(8,10,14):RGB(160,170,184));
        FillRect(dc,&rc,dim); DeleteObject(dim);
        int cw=S(460), chh=S(420);
        int cx=(rc.right-cw)/2, cy=(rc.bottom-chh)/2;
        RECT card={cx,cy,cx+cw,cy+chh};
        RECT shd=card; OffsetRect(&shd,0,S(6));
        fillRoundRect(dc,shd,S(18),g_dark?RGB(5,7,10):RGB(196,204,216),CLR_INVALID);
        fillRoundRect(dc,card,S(18),g_theme.surface,g_theme.border);
        SetBkMode(dc,TRANSPARENT);
        SetTextColor(dc,g_theme.text);
        SelectObject(dc,g_fTitle);
        RECT tr={cx,cy+S(24),cx+cw,cy+S(58)};
        DrawTextW(dc,L"انتخاب شیفت کاری",-1,&tr,
            DT_CENTER|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);
        SelectObject(dc,g_fSmall);
        SetTextColor(dc,g_theme.textDim);
        SYSTEMTIME st=iranNow();
        std::wstring sub = L"ساعت ایران: " + toFaDigits(iranTimeStr(st,false))
            + L"  —  شیفت تشخیص داده‌شده: " + shiftName(detectShift());
        RECT sr={cx,cy+S(60),cx+cw,cy+S(88)};
        DrawTextW(dc,sub.c_str(),-1,&sr,
            DT_CENTER|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);
        BitBlt(dc0,0,0,rc.right,rc.bottom,dc,0,0,SRCCOPY);
        SelectObject(dc,obm); DeleteObject(bmp); DeleteDC(dc);
        EndPaint(h,&ps);
        return 0; }
    case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(h,m,w,l);
}
bool showShiftDialog(HWND parent, int& shift){
    static bool reg=false;
    if(!reg){
        WNDCLASSW wc={0};
        wc.lpfnWndProc=shiftProc; wc.hInstance=g_hInst;
        wc.hCursor=LoadCursor(NULL,IDC_ARROW);
        wc.lpszClassName=SH_CLASS;
        RegisterClassW(&wc); reg=true;
    }
    bool ok=false;
    ShiftData d; d.shift=&shift; d.ok=&ok;
    d.autoMode = getSetting(L"shift_auto", L"1") == L"1";   // remembered
    d.sel = detectShift();
    RECT rc; GetClientRect(parent,&rc);
    HWND ov=CreateWindowExW(0,SH_CLASS,L"",WS_CHILD|WS_VISIBLE|WS_CLIPCHILDREN,
        0,0,rc.right,rc.bottom,parent,NULL,g_hInst,&d);
    SetWindowPos(ov,HWND_TOP,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE);
    runModal(ov,parent);
    return ok;
}
