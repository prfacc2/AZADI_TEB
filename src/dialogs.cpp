// ============================================================================
//  dialogs.cpp — Win11-style centered login card + shift selection dialog
//  v1.0.1: dialogs are now OWNED POPUP windows (not children) so modal
//  EnableWindow() on the frame no longer disables the dialog itself.
//  No WS_EX_LAYOUTRTL anywhere (avoids GDI mirroring bugs) — RTL is manual.
//  v1.1.0 CRITICAL FIX: dialogs no longer call PostQuitMessage(0) in
//  WM_DESTROY. The old code queued a WM_QUIT that runModal never consumed
//  (its IsWindow() check exits the loop first), so the stray WM_QUIT leaked
//  into the MAIN message loop and silently terminated the whole app right
//  after every login / cancel — the "instant crash" bug.
// ============================================================================
#include "app.h"
#include <stdio.h>

// =============================================================== LOGIN =====
#define LGN_CLASS L"AzLogin"
#define ID_LG_USER  201
#define ID_LG_PASS  202
#define ID_LG_OK    203
#define ID_LG_CANCEL 204

struct LoginData {
    int role; User* out; bool* ok;
    HWND eUser, ePass, bOk, bCancel;
    std::wstring errMsg;
    int shake;
};

static const wchar_t* roleTitle(int r){
    switch(r){
        case 0: return L"ورود به پذیرش درمانگاه";
        case 1: return L"ورود به پنل مدیریت";
        default:return L"پنل مخفی ادمین";
    }
}

// card geometry (shared by layout + paint)
static void loginCard(HWND h, RECT& card){
    RECT rc; GetClientRect(h,&rc);
    int cw=S(420), chh=S(460);
    int cx=(rc.right-cw)/2, cy=(rc.bottom-chh)/2;
    card.left=cx; card.top=cy; card.right=cx+cw; card.bottom=cy+chh;
}
static void loginLayout(HWND h, LoginData* d){
    RECT c; loginCard(h,c);
    int cw=c.right-c.left;
    int ew=cw-S(96), ex=c.left+S(48);
    MoveWindow(d->eUser, ex, c.top+S(168), ew, S(30), TRUE);
    MoveWindow(d->ePass, ex, c.top+S(252), ew, S(30), TRUE);
    MoveWindow(d->bOk,    c.left+S(40), c.top+S(346), cw-S(80), S(46), TRUE);
    MoveWindow(d->bCancel,c.left+S(40), c.top+S(400), cw-S(80), S(38), TRUE);
}

static LRESULT CALLBACK loginProc(HWND h, UINT m, WPARAM w, LPARAM l){
    LoginData* d=(LoginData*)GetWindowLongPtrW(h,GWLP_USERDATA);
    switch(m){
    case WM_CREATE: {
        CREATESTRUCTW* cs=(CREATESTRUCTW*)l;
        d=(LoginData*)cs->lpCreateParams;
        SetWindowLongPtrW(h,GWLP_USERDATA,(LONG_PTR)d);
        DWORD es = WS_CHILD|WS_VISIBLE|WS_TABSTOP|ES_AUTOHSCROLL;
        d->eUser = CreateWindowExW(0,L"EDIT",L"",es,0,0,10,10,h,(HMENU)ID_LG_USER,g_hInst,0);
        d->ePass = CreateWindowExW(0,L"EDIT",L"",es|ES_PASSWORD,0,0,10,10,h,(HMENU)ID_LG_PASS,g_hInst,0);
        SendMessageW(d->eUser,WM_SETFONT,(WPARAM)g_fUI,TRUE);
        SendMessageW(d->ePass,WM_SETFONT,(WPARAM)g_fUI,TRUE);
        d->bOk     = createFlatButton(h,ID_LG_OK,L"ورود",ICO_CHECK,BS_PRIMARY,0,0,10,10);
        d->bCancel = createFlatButton(h,ID_LG_CANCEL,L"انصراف",0,BS_OUTLINE,0,0,10,10);
        loginLayout(h,d);
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
        if(!d) return 0;
        int id=LOWORD(w);
        if(id==ID_LG_OK){
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
    case WM_CLOSE:
        if(d){ *d->ok=false; } DestroyWindow(h); return 0;
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps; HDC dc0=BeginPaint(h,&ps);
        RECT rc; GetClientRect(h,&rc);
        HDC dc=CreateCompatibleDC(dc0);
        HBITMAP bmp=CreateCompatibleBitmap(dc0,rc.right,rc.bottom);
        HGDIOBJ obm=SelectObject(dc,bmp);
        // dim layer
        HBRUSH dim=CreateSolidBrush(g_dark?RGB(8,10,14):RGB(150,160,176));
        FillRect(dc,&rc,dim); DeleteObject(dim);

        RECT c; loginCard(h,c);
        int sh = (d && d->shake>0) ? ((d->shake%2)? S(6):-S(6)) : 0;
        OffsetRect(&c, sh, 0);
        int cx=c.left, cy=c.top, cw=c.right-c.left;
        // shadow + card
        RECT shd=c; OffsetRect(&shd,0,S(6));
        fillRoundRect(dc,shd,S(18),g_dark?RGB(5,7,10):RGB(120,130,146),CLR_INVALID);
        fillRoundRect(dc,c,S(18),g_theme.surface,g_theme.border);

        SetBkMode(dc,TRANSPARENT);
        // icon circle
        int ir=S(28);
        RECT ic={cx+cw/2-ir,cy+S(26),cx+cw/2+ir,cy+S(26)+2*ir};
        fillRoundRect(dc,ic,4*ir, (d&&d->role==2)?g_theme.danger:g_theme.accent, CLR_INVALID);
        RECT ii={ic.left+S(14),ic.top+S(14),ic.right-S(14),ic.bottom-S(14)};
        drawIcon(dc, (d&&d->role==2)?ICO_SHIELD:ICO_USER, ii, RGB(255,255,255), S(2)+1);

        SetTextColor(dc,g_theme.text);
        SelectObject(dc,g_fTitle);
        RECT tr={cx,cy+S(94),cx+cw,cy+S(128)};
        DrawTextW(dc,roleTitle(d?d->role:0),-1,&tr,
            DT_CENTER|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);

        // labels + input boxes (frames around the EDIT controls)
        SelectObject(dc,g_fSmall);
        SetTextColor(dc,g_theme.textDim);
        RECT lu={cx+S(48),cy+S(136),cx+cw-S(48),cy+S(158)};
        DrawTextW(dc,L"نام کاربری",-1,&lu,DT_RIGHT|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);
        RECT lp={cx+S(48),cy+S(220),cx+cw-S(48),cy+S(242)};
        DrawTextW(dc,L"رمز عبور",-1,&lp,DT_RIGHT|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);

        RECT bu={cx+S(40),cy+S(160),cx+cw-S(40),cy+S(206)};
        fillRoundRect(dc,bu,S(8),g_theme.inputBg,g_theme.border);
        RECT bp={cx+S(40),cy+S(244),cx+cw-S(40),cy+S(290)};
        fillRoundRect(dc,bp,S(8),g_theme.inputBg,g_theme.border);

        if(d && !d->errMsg.empty()){
            SetTextColor(dc,g_theme.danger);
            SelectObject(dc,g_fUI);
            RECT er={cx+S(40),cy+S(300),cx+cw-S(40),cy+S(338)};
            DrawTextW(dc,d->errMsg.c_str(),-1,&er,
                DT_CENTER|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);
        }
        BitBlt(dc0,0,0,rc.right,rc.bottom,dc,0,0,SRCCOPY);
        SelectObject(dc,obm); DeleteObject(bmp); DeleteDC(dc);
        EndPaint(h,&ps);
        return 0; }
    // NOTE: no PostQuitMessage here — runModal exits via IsWindow() check.
    }
    return DefWindowProcW(h,m,w,l);
}

static void regClass(const wchar_t* name, WNDPROC proc){
    WNDCLASSW wc={0};
    wc.lpfnWndProc=proc; wc.hInstance=g_hInst;
    wc.hCursor=LoadCursor(NULL,IDC_ARROW);
    wc.lpszClassName=name;
    RegisterClassW(&wc);
}

//  Nested message loop until the popup closes. The popup is an OWNED
//  top-level window, so disabling the frame doesn't disable it.
static void runModal(HWND overlay, HWND parent, HWND firstFocus,
                     int idUserEdit, int idOkBtn){
    if(!overlay || !IsWindow(overlay)) return;   // creation failed — never
                                                 // leave the frame disabled
    EnableWindow(parent, FALSE);
    SetForegroundWindow(overlay);
    if(firstFocus) SetFocus(firstFocus);
    MSG msg;
    while(IsWindow(overlay) && GetMessageW(&msg,NULL,0,0)){
        if(msg.message==WM_QUIT){               // never ours — re-post and bail
            PostQuitMessage((int)msg.wParam);
            break;
        }
        if(msg.message==WM_KEYDOWN && msg.wParam==VK_TAB &&
           GetAncestor(msg.hwnd,GA_ROOT)==overlay){
            HWND f=GetFocus();
            HWND nxt=GetNextDlgTabItem(overlay,f,
                (GetKeyState(VK_SHIFT)&0x8000)?TRUE:FALSE);
            if(nxt && nxt!=f){ SetFocus(nxt); SendMessageW(nxt,EM_SETSEL,0,-1); }
            continue;
        }
        if(msg.message==WM_KEYDOWN && msg.wParam==VK_RETURN &&
           GetAncestor(msg.hwnd,GA_ROOT)==overlay){
            HWND f=GetFocus();
            wchar_t cls[32]={0}; if(f) GetClassNameW(f,cls,32);
            if(!wcscmp(cls,L"EDIT")){
                if(idUserEdit && GetDlgCtrlID(f)==idUserEdit){
                    HWND nxt=GetNextDlgTabItem(overlay,f,FALSE);
                    if(nxt){ SetFocus(nxt); SendMessageW(nxt,EM_SETSEL,0,-1); }
                } else if(idOkBtn){
                    SendMessageW(overlay,WM_COMMAND,MAKEWPARAM(idOkBtn,0),0);
                }
                continue;
            }
            if(idOkBtn){ SendMessageW(overlay,WM_COMMAND,MAKEWPARAM(idOkBtn,0),0); continue; }
        }
        if(msg.message==WM_KEYDOWN && msg.wParam==VK_ESCAPE &&
           GetAncestor(msg.hwnd,GA_ROOT)==overlay){
            SendMessageW(overlay,WM_CLOSE,0,0);
            continue;
        }
        TranslateMessage(&msg); DispatchMessageW(&msg);
    }
    EnableWindow(parent, TRUE);
    SetForegroundWindow(parent);
    SetFocus(parent);
}

bool showLoginDialog(HWND parent, int role, User& out){
    static bool reg=false;
    if(!reg){ regClass(LGN_CLASS, loginProc); reg=true; }
    bool ok=false;
    LoginData d; d.role=role; d.out=&out; d.ok=&ok; d.shake=0;
    d.eUser=d.ePass=d.bOk=d.bCancel=NULL;
    RECT fr; GetWindowRect(parent,&fr);
    HWND ov = CreateWindowExW(0,LGN_CLASS,L"",
        WS_POPUP|WS_VISIBLE,
        fr.left,fr.top,fr.right-fr.left,fr.bottom-fr.top,
        parent,NULL,g_hInst,&d);
    if(!ov) return false;
    runModal(ov, parent, d.eUser, ID_LG_USER, ID_LG_OK);
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
static void shiftCard(HWND h, RECT& card){
    RECT rc; GetClientRect(h,&rc);
    int cw=S(470), chh=S(430);
    card.left=(rc.right-cw)/2; card.top=(rc.bottom-chh)/2;
    card.right=card.left+cw; card.bottom=card.top+chh;
}
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
    RECT c; shiftCard(h,c);
    int cw=c.right-c.left;
    int ew=cw-S(60), ex=c.left+S(30);
    MoveWindow(d->bAuto, ex, c.top+S(100), ew, S(44), TRUE);
    MoveWindow(d->b0,    ex, c.top+S(160), ew, S(48), TRUE);
    MoveWindow(d->b1,    ex, c.top+S(216), ew, S(48), TRUE);
    MoveWindow(d->b2,    ex, c.top+S(272), ew, S(48), TRUE);
    MoveWindow(d->bOk,   ex, c.top+S(344), ew/2-S(6), S(46), TRUE);
    MoveWindow(d->bCancel, ex+ew/2+S(6), c.top+S(344), ew/2-S(6), S(46), TRUE);
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
        SetTimer(h, 9, 30000, NULL);
        return 0; }
    case WM_TIMER:
        if(w==9 && d && d->autoMode){ shiftRefresh(d); InvalidateRect(h,NULL,TRUE);}
        return 0;
    case WM_SIZE: if(d) shiftLayout(h,d); return 0;
    case WM_COMMAND: {
        if(!d) return 0;
        int id=LOWORD(w);
        switch(id){
        case ID_SH_AUTO:
            d->autoMode = !d->autoMode;
            setSetting(L"shift_auto", d->autoMode?L"1":L"0");
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
    case WM_CLOSE:
        if(d){ *d->ok=false; } KillTimer(h,9); DestroyWindow(h); return 0;
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps; HDC dc0=BeginPaint(h,&ps);
        RECT rc; GetClientRect(h,&rc);
        HDC dc=CreateCompatibleDC(dc0);
        HBITMAP bmp=CreateCompatibleBitmap(dc0,rc.right,rc.bottom);
        HGDIOBJ obm=SelectObject(dc,bmp);
        HBRUSH dim=CreateSolidBrush(g_dark?RGB(8,10,14):RGB(150,160,176));
        FillRect(dc,&rc,dim); DeleteObject(dim);
        RECT c; shiftCard(h,c);
        RECT shd=c; OffsetRect(&shd,0,S(6));
        fillRoundRect(dc,shd,S(18),g_dark?RGB(5,7,10):RGB(120,130,146),CLR_INVALID);
        fillRoundRect(dc,c,S(18),g_theme.surface,g_theme.border);
        SetBkMode(dc,TRANSPARENT);
        SetTextColor(dc,g_theme.text);
        SelectObject(dc,g_fTitle);
        RECT tr={c.left,c.top+S(24),c.right,c.top+S(58)};
        DrawTextW(dc,L"انتخاب شیفت کاری",-1,&tr,
            DT_CENTER|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);
        SelectObject(dc,g_fSmall);
        SetTextColor(dc,g_theme.textDim);
        SYSTEMTIME st=iranNow();
        std::wstring sub = L"ساعت ایران: " + toFaDigits(iranTimeStr(st,false))
            + L"  —  شیفت تشخیص داده‌شده: " + shiftName(detectShift());
        RECT sr={c.left,c.top+S(62),c.right,c.top+S(90)};
        DrawTextW(dc,sub.c_str(),-1,&sr,
            DT_CENTER|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);
        BitBlt(dc0,0,0,rc.right,rc.bottom,dc,0,0,SRCCOPY);
        SelectObject(dc,obm); DeleteObject(bmp); DeleteDC(dc);
        EndPaint(h,&ps);
        return 0; }
    // NOTE: no PostQuitMessage here — runModal exits via IsWindow() check.
    }
    return DefWindowProcW(h,m,w,l);
}
bool showShiftDialog(HWND parent, int& shift){
    static bool reg=false;
    if(!reg){ regClass(SH_CLASS, shiftProc); reg=true; }
    bool ok=false;
    ShiftData d; d.shift=&shift; d.ok=&ok;
    d.autoMode = getSetting(L"shift_auto", L"1") == L"1";
    d.sel = detectShift();
    d.bAuto=d.b0=d.b1=d.b2=d.bOk=d.bCancel=NULL;
    RECT fr; GetWindowRect(parent,&fr);
    HWND ov=CreateWindowExW(0,SH_CLASS,L"",
        WS_POPUP|WS_VISIBLE,
        fr.left,fr.top,fr.right-fr.left,fr.bottom-fr.top,
        parent,NULL,g_hInst,&d);
    if(!ov) return false;
    runModal(ov,parent,NULL,0,ID_SH_OK);
    return ok;
}
