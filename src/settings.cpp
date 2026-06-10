// ============================================================================
//  settings.cpp — slide-over "settings" panel (v1.3.0)
//
//  Styled like a social-network profile page: a dim scrim over the whole frame
//  and a tall card panel that slides in from the (RTL) right edge.  The panel
//  shows the signed-in person's avatar + identity, then grouped option rows:
//      • theme switch        (روشن / تیره)
//      • check for update     (بررسی به‌روزرسانی)
//      • UI density           (متعارف / فشرده)        — applied on next launch
//      • auto-print receipt   (چاپ خودکار قبض)        on/off toggle
//      • server URL           (آدرس سامانه مدیریت)    — future panel link
//      • about                (درباره برنامه)
//
//  Pure owner-drawn (GDI+) — no extra controls except a couple of edit boxes,
//  so it inherits the app's theme automatically and stays a single static EXE.
// ============================================================================
#include "app.h"
#include <stdio.h>

#define SET_CLASS  L"AzSettings"
#define IDC_SRV     900     // server-url edit box

// ----- the option rows we draw (id drives the click action) ----------------
enum {
    ROW_THEME = 1,
    ROW_UPDATE,
    ROW_DENSITY,
    ROW_AUTOPRINT,
    ROW_SERVER,
    ROW_ABOUT,
    ROW_LOGOUT
};

struct SetState {
    HWND owner;             // the frame
    HWND eServer;           // server URL edit
    int  hot;               // hovered row id (-1)
    int  panelW;            // current animated width of the panel
    int  panelTarget;       // target width
    bool closing;
    // live mirrors of the persisted settings
    bool dark;
    bool compact;
    bool autoPrint;
};
static HWND s_set = NULL;          // single panel instance
static SetState* s_st = NULL;

// row geometry helpers ------------------------------------------------------
static int panelFullW(){ int w=S(440); return w; }
static int avatarH(){ return S(210); }      // header area (avatar + identity)
static int rowH(){ return S(64); }

// the panel rect (right-anchored, full content height) inside the frame -----
static RECT panelRect(HWND h){
    RECT rc; GetClientRect(h,&rc);
    RECT p={ rc.right - (s_st?s_st->panelW:panelFullW()), 0, rc.right, rc.bottom };
    return p;
}
// rect of a given option row (RTL: text right, control/chevron left) --------
static RECT rowRect(const RECT& panel, int index){
    int x0 = panel.left + S(20);
    int x1 = panel.right - S(20);
    int y  = panel.top + avatarH() + S(18) + index*rowH();
    RECT r={ x0, y, x1, y + rowH() - S(10) };
    return r;
}

// hit-test a point against the option rows; returns ROW_* or 0 --------------
static int hitRow(HWND h, POINT pt){
    RECT panel = panelRect(h);
    if(pt.x < panel.left) return -1;            // -1 = on the scrim (close)
    const int rows[] = { ROW_THEME, ROW_UPDATE, ROW_DENSITY, ROW_AUTOPRINT,
                         ROW_SERVER, ROW_ABOUT, ROW_LOGOUT };
    for(int i=0;i<7;i++){
        RECT r = rowRect(panel,i);
        if(pt.x>=r.left && pt.x<=r.right && pt.y>=r.top && pt.y<=r.bottom)
            return rows[i];
    }
    return 0;
}

// position the (single) server edit box over its row ------------------------
static void layoutServerEdit(HWND h){
    if(!s_st || !s_st->eServer) return;
    RECT panel = panelRect(h);
    RECT r = rowRect(panel, 4);   // ROW_SERVER is the 5th row (index 4)
    // edit sits in the lower half of the row, full width
    int ex0 = r.left + S(14), ex1 = r.right - S(14);
    MoveWindow(s_st->eServer, ex0, r.top+S(30), ex1-ex0, S(26), TRUE);
    ShowWindow(s_st->eServer, SW_SHOW);
}

// ---------------------------------------------------------------- actions --
static void persistDensity(bool compact){
    setSetting(L"density", compact ? L"compact" : L"normal");
}
static void doThemeToggle(HWND h){
    applyTheme(!g_dark);            // applyTheme persists "theme" itself
    if(s_st) s_st->dark = g_dark;
    broadcastThemeChange();
    InvalidateRect(h,NULL,FALSE);
}
static void doDensityToggle(HWND h){
    if(!s_st) return;
    s_st->compact = !s_st->compact;
    persistDensity(s_st->compact);
    MessageBoxW(h,
        L"تغییر چگالی رابط در اجرای بعدی برنامه اعمال می‌شود.",
        L"چگالی رابط", MB_OK|MB_ICONINFORMATION);
    InvalidateRect(h,NULL,FALSE);
}
static void doAutoPrintToggle(HWND h){
    if(!s_st) return;
    s_st->autoPrint = !s_st->autoPrint;
    setSetting(L"auto_print", s_st->autoPrint?L"1":L"0");
    InvalidateRect(h,NULL,FALSE);
}
static void saveServerUrl(){
    if(!s_st || !s_st->eServer) return;
    wchar_t buf[512]={0}; GetWindowTextW(s_st->eServer,buf,512);
    std::wstring v = trim(buf);
    if(!v.empty()) setSetting(L"server_url", v);
}
static void doAbout(HWND h){
    std::wstring msg =
        std::wstring(APP_NAME_W) + L"\n"
        L"سامانه پذیرش و مدیریت درمانگاه\n\n"
        L"نسخه: " + toFaDigits(APP_VERSION_W) + L"\n"
        L"اجرای تک‌فایل، سازگار با ویندوز ۷ تا ۱۱\n\n"
        L"© آزادی طب";
    MessageBoxW(h, msg.c_str(), L"درباره برنامه", MB_OK|MB_ICONINFORMATION);
}

// --------------------------------------------------------------- painting --
//  a pill toggle switch (on = accent, off = grey) drawn at the LEFT of a row
static void drawToggle(HDC dc, int cx, int cy, bool on){
    int w=S(46), hh=S(24);
    RECT tr={cx-w/2, cy-hh/2, cx+w/2, cy+hh/2};
    gpRoundRect(dc,tr,hh/2, on?g_theme.accent:g_theme.border, CLR_INVALID, 255);
    int kn=hh-S(6);
    int kx = on ? (tr.right-S(3)-kn) : (tr.left+S(3));
    RECT kr={kx, tr.top+S(3), kx+kn, tr.bottom-S(3)};
    gpRoundRect(dc,kr,kn/2, RGB(255,255,255), CLR_INVALID, 255);
}
//  a small value chip (e.g. "تیره"/"روشن") at the LEFT of a row
static void drawValueChip(HDC dc, RECT row, const wchar_t* val){
    SIZE sz; HGDIOBJ of=SelectObject(dc,g_fSmall);
    GetTextExtentPoint32W(dc,val,(int)wcslen(val),&sz);
    int pad=S(12);
    RECT chip={ row.left+S(8), (row.top+row.bottom)/2-S(13),
                row.left+S(8)+sz.cx+pad*2, (row.top+row.bottom)/2+S(13) };
    gpRoundRect(dc,chip,S(13), g_theme.surface2, g_theme.border, 255);
    SetTextColor(dc,g_theme.textDim);
    DrawTextW(dc,val,-1,&chip,DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_NOPREFIX);
    SelectObject(dc,of);
}

static void paintPanel(HWND h, HDC dc0){
    RECT rc; GetClientRect(h,&rc);
    HDC dc=CreateCompatibleDC(dc0);
    HBITMAP bmp=CreateCompatibleBitmap(dc0,rc.right,rc.bottom);
    HGDIOBJ obm=SelectObject(dc,bmp);

    // 1) dim scrim over the whole frame (click here to close). The child
    //    overlay's backing bitmap starts black, so we paint an OPAQUE dark
    //    scrim first (a translucent-looking slate) — robust on every GPU and
    //    even when GDI+ is unavailable.
    { HBRUSH sb = CreateSolidBrush(g_dark?RGB(6,9,14):RGB(28,36,48));
      FillRect(dc,&rc,sb); DeleteObject(sb); }

    // 2) the panel card on the RTL right — fully opaque so text is crisp
    RECT panel = panelRect(h);
    { HBRUSH pb = CreateSolidBrush(g_theme.surface);
      FillRect(dc,&panel,pb); DeleteObject(pb); }
    gpGradRoundRect(dc,panel,0, g_theme.surfaceTop, g_theme.surface, CLR_INVALID);
    gpLine(dc,panel.left,0,panel.left,rc.bottom, g_theme.border,1.5f);

    SetBkMode(dc,TRANSPARENT);

    // ---- profile header (cover gradient + avatar + identity) --------------
    RECT cover={panel.left,0,panel.right, S(120)};
    gpGradRoundRect(dc,cover,0, g_theme.accent2, g_theme.accent, CLR_INVALID);
    // close (×) top-left of the panel
    { RECT cb={panel.left+S(14),S(14),panel.left+S(40),S(40)};
      if(s_st && s_st->hot==-2) gpRoundRect(dc,cb,S(8),
          RGB(255,255,255),CLR_INVALID,60);
      RECT ci={cb.left+S(5),cb.top+S(5),cb.right-S(5),cb.bottom-S(5)};
      drawIcon(dc,ICO_X,ci,RGB(255,255,255),S(2)); }

    // avatar circle (initial) centred, overlapping the cover bottom
    int avR=S(46), avCx=(panel.left+panel.right)/2, avCy=S(120);
    RECT avo={avCx-avR-S(4),avCy-avR-S(4),avCx+avR+S(4),avCy+avR+S(4)};
    gpRoundRect(dc,avo,avR+S(4), g_theme.surfaceTop, CLR_INVALID, 255);
    RECT av={avCx-avR,avCy-avR,avCx+avR,avCy+avR};
    gpGradRoundRect(dc,av,avR, g_theme.accent2, g_theme.accent, CLR_INVALID);
    // initial letter (first char of full name) or a user glyph
    std::wstring fn = g_session.user.fullname;
    if(!fn.empty()){
        std::wstring ini = fn.substr(0,1);
        SelectObject(dc,g_fHuge); SetTextColor(dc,RGB(255,255,255));
        DrawTextW(dc,ini.c_str(),-1,&av,
            DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_NOPREFIX);
    } else {
        RECT ui={av.left+S(22),av.top+S(22),av.right-S(22),av.bottom-S(22)};
        drawIcon(dc,ICO_USER,ui,RGB(255,255,255),S(3));
    }

    // identity text under the avatar
    SelectObject(dc,g_fTitle); SetTextColor(dc,g_theme.text);
    RECT nr={panel.left+S(16), avCy+avR+S(8), panel.right-S(16), avCy+avR+S(42)};
    DrawTextW(dc, fn.empty()?L"کاربر":fn.c_str(), -1, &nr,
        DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);

    const wchar_t* role =
        g_session.user.role==2 ? L"مدیر سامانه" :
        g_session.user.role==1 ? L"مدیریت درمانگاه" : L"پذیرش درمانگاه";
    std::wstring sub = std::wstring(role) +
        (g_session.user.dept.empty()?L"":(L"  •  "+g_session.user.dept));
    SelectObject(dc,g_fSmall); SetTextColor(dc,g_theme.textDim);
    RECT srr={panel.left+S(16), nr.bottom, panel.right-S(16), nr.bottom+S(24)};
    DrawTextW(dc, sub.c_str(), -1, &srr,
        DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);

    // login meta (shift + login time)
    std::wstring meta = L"شیفت ورود: " + shiftName(g_session.shift) +
        L"   •   " + toFaDigits(iranTimeStr(g_session.loginAt,false));
    RECT mr={panel.left+S(16), srr.bottom+S(2), panel.right-S(16), srr.bottom+S(24)};
    DrawTextW(dc, meta.c_str(), -1, &mr,
        DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);

    // ---- option rows ------------------------------------------------------
    struct ROW { int id; const wchar_t* label; int icon; const wchar_t* hint; };
    ROW rows[7]={
        {ROW_THEME,     L"پوستهٔ برنامه",          ICO_MOON,   NULL},
        {ROW_UPDATE,    L"بررسی به‌روزرسانی",      ICO_UPDATE, L"دریافت آخرین نسخه"},
        {ROW_DENSITY,   L"چگالی رابط کاربری",      ICO_TAB,    NULL},
        {ROW_AUTOPRINT, L"چاپ خودکار قبض",         ICO_PRINT,  NULL},
        {ROW_SERVER,    L"آدرس سامانهٔ مدیریت",    ICO_SHIELD, NULL},
        {ROW_ABOUT,     L"درباره برنامه",          ICO_BELL,   L"نسخه و اطلاعات"},
        {ROW_LOGOUT,    L"خروج از حساب",           ICO_LOGOUT, NULL},
    };
    for(int i=0;i<7;i++){
        RECT r = rowRect(panel,i);
        bool hov = (s_st && s_st->hot==rows[i].id);
        bool danger = (rows[i].id==ROW_LOGOUT);
        // row card
        gpRoundRect(dc,r,S(12),
            hov?g_theme.hover:g_theme.surface,
            hov?g_theme.accent:g_theme.border, 255);
        COLORREF ic = danger?g_theme.danger:g_theme.accent;
        // icon flush-right
        RECT ir={r.right-S(40),r.top+S(12),r.right-S(14),r.top+S(38)};
        drawIcon(dc,rows[i].icon,ir,ic,S(2));
        // label (right-aligned, left of the icon)
        SelectObject(dc,g_fUIB);
        SetTextColor(dc, danger?g_theme.danger:g_theme.text);
        RECT lr={r.left+S(14), r.top+S(8), r.right-S(48), r.top+S(34)};
        DrawTextW(dc,rows[i].label,-1,&lr,
            DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
        // optional hint line (small, dim)
        if(rows[i].hint){
            SelectObject(dc,g_fSmall); SetTextColor(dc,g_theme.textDim);
            RECT hr={r.left+S(14), r.top+S(34), r.right-S(48), r.bottom-S(4)};
            DrawTextW(dc,rows[i].hint,-1,&hr,
                DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
        }
        // controls / values on the LEFT
        int lcx=r.left+S(34), lcy=(r.top+r.bottom)/2;
        switch(rows[i].id){
            case ROW_THEME:
                drawValueChip(dc,r, s_st&&s_st->dark?L"تیره":L"روشن"); break;
            case ROW_DENSITY:
                drawValueChip(dc,r, s_st&&s_st->compact?L"فشرده":L"متعارف"); break;
            case ROW_AUTOPRINT:
                drawToggle(dc,lcx,lcy, s_st&&s_st->autoPrint); break;
            case ROW_THEME+100: break;
            default: {
                // chevron affordance for action rows
                RECT cv={r.left+S(14),lcy-S(8),r.left+S(28),lcy+S(8)};
                if(rows[i].id!=ROW_SERVER)
                    drawIcon(dc,ICO_CHEVRON,cv,g_theme.textDim,S(2));
            } break;
        }
    }

    BitBlt(dc0,0,0,rc.right,rc.bottom,dc,0,0,SRCCOPY);
    SelectObject(dc,obm); DeleteObject(bmp); DeleteDC(dc);
}

// ----------------------------------------------------------------- wndproc -
static LRESULT CALLBACK setProc(HWND h, UINT m, WPARAM w, LPARAM l){
    switch(m){
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps; HDC dc=BeginPaint(h,&ps);
        paintPanel(h,dc);
        EndPaint(h,&ps);
        return 0; }
    case WM_APP_THEME:
        InvalidateRect(h,NULL,FALSE);
        return 0;
    case WM_MOUSEMOVE: {
        POINT pt={GET_X_LPARAM(l),GET_Y_LPARAM(l)};
        RECT panel=panelRect(h);
        int hr;
        // close button hot-zone
        RECT cb={panel.left+S(14),S(14),panel.left+S(40),S(40)};
        if(pt.x>=cb.left&&pt.x<=cb.right&&pt.y>=cb.top&&pt.y<=cb.bottom) hr=-2;
        else hr=hitRow(h,pt);
        if(s_st && hr!=s_st->hot){ s_st->hot=hr; InvalidateRect(h,NULL,FALSE); }
        TRACKMOUSEEVENT te={sizeof(te),TME_LEAVE,h,0}; TrackMouseEvent(&te);
        return 0; }
    case WM_MOUSELEAVE:
        if(s_st && s_st->hot!=0){ s_st->hot=0; InvalidateRect(h,NULL,FALSE); }
        return 0;
    case WM_LBUTTONDOWN: {
        POINT pt={GET_X_LPARAM(l),GET_Y_LPARAM(l)};
        RECT panel=panelRect(h);
        RECT cb={panel.left+S(14),S(14),panel.left+S(40),S(40)};
        if(pt.x>=cb.left&&pt.x<=cb.right&&pt.y>=cb.top&&pt.y<=cb.bottom){
            closeSettingsPanel(); return 0;
        }
        int id=hitRow(h,pt);
        if(id==-1){ closeSettingsPanel(); return 0; }    // clicked the scrim
        switch(id){
            case ROW_THEME:     doThemeToggle(h); break;
            case ROW_UPDATE:    saveServerUrl(); checkRemoteUpdate(h); break;
            case ROW_DENSITY:   doDensityToggle(h); break;
            case ROW_AUTOPRINT: doAutoPrintToggle(h); break;
            case ROW_SERVER:    if(s_st&&s_st->eServer) SetFocus(s_st->eServer); break;
            case ROW_ABOUT:     doAbout(h); break;
            case ROW_LOGOUT:    saveServerUrl(); closeSettingsPanel();
                                PostMessageW(g_hFrame, WM_COMMAND, 101, 0); // ID_FR_EXIT
                                break;
        }
        return 0; }
    case WM_KEYDOWN:
        if(w==VK_ESCAPE){ closeSettingsPanel(); return 0; }
        break;
    case WM_COMMAND:
        if(LOWORD(w)==IDC_SRV && HIWORD(w)==EN_KILLFOCUS) saveServerUrl();
        return 0;
    case WM_CTLCOLOREDIT: {
        HDC dc=(HDC)w;
        SetTextColor(dc,g_theme.inputText); SetBkColor(dc,g_theme.inputBg);
        return (LRESULT)g_brInput; }
    case WM_SIZE:
        layoutServerEdit(h);
        InvalidateRect(h,NULL,FALSE);
        return 0;
    case WM_DESTROY:
        if(s_st){ delete s_st; s_st=NULL; }
        s_set=NULL;
        return 0;
    }
    return DefWindowProcW(h,m,w,l);
}

// ------------------------------------------------------------------ public -
void openSettingsPanel(HWND frameOwner){
    if(s_set && IsWindow(s_set)){          // already open → toggle closed
        closeSettingsPanel();
        return;
    }
    static bool reg=false;
    if(!reg){
        WNDCLASSW wc={0};
        wc.lpfnWndProc=setProc; wc.hInstance=g_hInst;
        wc.hCursor=LoadCursor(NULL,IDC_ARROW);
        wc.lpszClassName=SET_CLASS;
        RegisterClassW(&wc); reg=true;
    }
    RECT rc; GetClientRect(frameOwner,&rc);
    POINT org={0,0}; ClientToScreen(frameOwner,&org);   // frame → screen
    s_st = new SetState();
    s_st->owner=frameOwner; s_st->hot=0;
    s_st->panelW=panelFullW(); s_st->panelTarget=panelFullW();
    s_st->dark=g_dark;
    s_st->compact = (getSetting(L"density",L"normal")==L"compact");
    s_st->autoPrint = (getSetting(L"auto_print",L"0")==L"1");

    // A top-level popup positioned EXACTLY over the frame, so it sits above all
    // of the frame's child screens (reception, tabs, header buttons). It owns
    // the frame so it always stays in front of it and closes with it.
    s_set = CreateWindowExW(WS_EX_TOPMOST, SET_CLASS, L"",
        WS_POPUP|WS_VISIBLE|WS_CLIPCHILDREN,
        org.x, org.y, rc.right, rc.bottom, frameOwner, NULL, g_hInst, NULL);

    // server-url edit box (only real child control on the panel)
    s_st->eServer = CreateWindowExW(0, L"EDIT",
        getSetting(L"server_url", L"").c_str(),
        WS_CHILD|WS_VISIBLE|ES_AUTOHSCROLL,
        0,0,10,10, s_set, (HMENU)IDC_SRV, g_hInst, NULL);
    SendMessageW(s_st->eServer, WM_SETFONT, (WPARAM)g_fSmall, TRUE);
    layoutServerEdit(s_set);

    BringWindowToTop(s_set);
    SetFocus(s_set);
    InvalidateRect(s_set,NULL,FALSE);
}
bool settingsPanelVisible(){ return s_set && IsWindow(s_set); }
void closeSettingsPanel(){
    if(s_set && IsWindow(s_set)){
        HWND v=s_set; s_set=NULL;
        DestroyWindow(v);
        if(g_hFrame) InvalidateRect(g_hFrame,NULL,TRUE);
    }
}
