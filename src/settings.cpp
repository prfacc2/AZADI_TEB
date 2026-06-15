// ============================================================================
//  settings.cpp — CENTERED modal settings panel (v1.4.0)
//
//  Redesign brief:
//   • The settings panel now opens in the CENTER of the screen (like the
//     "messenger" settings dialogs), not slid-over from the edge.
//   • ESC closes it. Clicking the dim scrim closes it.
//   • The rows shown depend on WHO is signed in:
//       - NOT logged in (guest)      → only «بررسی به‌روزرسانی» + «تغییر تم».
//                                       Profile shows «کاربر مهمان», no avatar
//                                       editing.
//       - پذیرش (reception)           → تغییر تم، بررسی آپدیت، تنظیمات اعلان،
//                                       تنظیمات چاپگر, و profile (name+photo)
//                                       that must be APPROVED by management.
//       - مدیریت / admin              → same + printer-design access toggle.
//   • Theme switch & check-for-update were removed from the header and live
//     here now.
//
//  Pure owner-drawn (GDI+) so it inherits the theme automatically.
// ============================================================================
#include "app.h"
#include <stdio.h>

#define SET_CLASS  L"AzSettings"
#define IDC_SRV     900     // server-url edit box

// ----- the option rows we may draw (id drives the click action) ------------
enum {
    ROW_THEME = 1,
    ROW_UPDATE,
    ROW_NOTIFY,       // notification settings (reception+)
    ROW_PRINTER,      // printer settings (reception+)
    ROW_PRINTDESIGN,  // print designer (reception+/manager)
    ROW_PROFILE,      // edit profile name+photo (reception+, needs approval)
    ROW_DENSITY,
    ROW_AUTOPRINT,
    ROW_SERVER,
    ROW_ABOUT,
    ROW_LOGOUT
};

struct RowDef { int id; const wchar_t* label; int icon; const wchar_t* hint; bool toggle; };

struct SetState {
    HWND owner;
    HWND eServer;
    int  hot;
    bool dark, compact, autoPrint, notify;
    std::vector<RowDef> rows;       // built per-role on open
    int  role;                      // -1 guest, 0 reception, 1 manage, 2 admin
};
static HWND s_set = NULL;
static SetState* s_st = NULL;
// v1.7.0 perf: cache of the EXPENSIVE static layers (scrim + GDI+ shadow +
// alpha blur + card gradient + cover + avatar + identity). Rebuilt only when
// the window is sized or the theme flips — NOT on every mouse move. Hover
// repaints just blit this cache for the dirty strip, then redraw the rows.
static HDC     s_bgDC  = NULL;
static HBITMAP s_bgBmp = NULL;
static HGDIOBJ s_bgOld = NULL;
static int     s_bgW=0, s_bgH=0;
static void freeBgCache(){
    if(s_bgDC){ SelectObject(s_bgDC,s_bgOld); DeleteDC(s_bgDC); s_bgDC=NULL; }
    if(s_bgBmp){ DeleteObject(s_bgBmp); s_bgBmp=NULL; }
    s_bgW=s_bgH=0;
}

// ---- geometry: a centered card -------------------------------------------
static int cardW(){ return S(460); }
static int headerH(){ return S(176); }   // cover + avatar + identity
static int rowH(){ return S(58); }
static int cardH(){
    int n = s_st ? (int)s_st->rows.size() : 5;
    return headerH() + S(14) + n*rowH() + S(20);
}
static RECT cardRect(HWND h){
    RECT rc; GetClientRect(h,&rc);
    int w=cardW(), hh=cardH();
    if(hh > rc.bottom-S(40)) hh = rc.bottom-S(40);
    RECT c={ (rc.right-w)/2, (rc.bottom-hh)/2, (rc.right+w)/2, (rc.bottom+hh)/2 };
    return c;
}
static RECT rowRect(const RECT& card, int index){
    int x0=card.left+S(20), x1=card.right-S(20);
    int y =card.top+headerH()+S(8)+index*rowH();
    RECT r={x0,y,x1,y+rowH()-S(8)};
    return r;
}
static int hitRow(HWND h, POINT pt){
    RECT card=cardRect(h);
    if(!PtInRect(&card,pt)) return -1;       // outside card → scrim (close)
    if(!s_st) return 0;
    for(size_t i=0;i<s_st->rows.size();i++){
        RECT r=rowRect(card,(int)i);
        if(PtInRect(&r,pt)) return s_st->rows[i].id;
    }
    return 0;
}
// v1.7.0 perf: return the on-screen rectangle for a hot-id (a row, the close
// button, or nothing). Used so WM_MOUSEMOVE invalidates ONLY the small region
// whose hover state changed — never the whole full-screen scrim/shadow/blur
// (that full repaint caused the stuttering on the settings window).
static bool hotRectFor(HWND h, int id, RECT& out){
    if(id==0 || id==-1) return false;
    RECT card=cardRect(h);
    if(id==-2){ // close (×) button top-left
        out = {card.left+S(10),card.top+S(10),card.left+S(44),card.top+S(44)};
        return true;
    }
    if(!s_st) return false;
    for(size_t i=0;i<s_st->rows.size();i++){
        if(s_st->rows[i].id==id){
            RECT r=rowRect(card,(int)i);
            // pad a couple px so the rounded border/hover ring is fully covered
            out = {r.left-S(2),r.top-S(2),r.right+S(2),r.bottom+S(2)};
            return true;
        }
    }
    return false;
}

// position the server edit box (only shown for admin/manage) ----------------
static int serverRowIndex(){
    if(!s_st) return -1;
    for(size_t i=0;i<s_st->rows.size();i++)
        if(s_st->rows[i].id==ROW_SERVER) return (int)i;
    return -1;
}
static void layoutServerEdit(HWND h){
    if(!s_st || !s_st->eServer) return;
    int idx=serverRowIndex();
    if(idx<0){ ShowWindow(s_st->eServer,SW_HIDE); return; }
    RECT card=cardRect(h);
    RECT r=rowRect(card,idx);
    MoveWindow(s_st->eServer, r.left+S(14), r.top+S(28), (r.right-r.left)-S(28), S(24), TRUE);
    ShowWindow(s_st->eServer, SW_SHOW);
}

// ---------------------------------------------------------------- actions --
static void doThemeToggle(HWND h){
    applyTheme(!g_dark);
    if(s_st) s_st->dark=g_dark;
    broadcastThemeChange();
    InvalidateRect(h,NULL,FALSE);
}
static void doDensityToggle(HWND h){
    if(!s_st) return;
    s_st->compact=!s_st->compact;
    setSetting(L"density", s_st->compact?L"compact":L"normal");
    MessageBoxW(h,L"تغییر چگالی رابط در اجرای بعدی برنامه اعمال می‌شود.",
        L"چگالی رابط", MB_OK|MB_ICONINFORMATION);
    InvalidateRect(h,NULL,FALSE);
}
static void doAutoPrintToggle(HWND h){
    if(!s_st) return;
    s_st->autoPrint=!s_st->autoPrint;
    setSetting(L"auto_print", s_st->autoPrint?L"1":L"0");
    InvalidateRect(h,NULL,FALSE);
}
static void doNotifyToggle(HWND h){
    if(!s_st) return;
    s_st->notify=!s_st->notify;
    setSetting(L"notify", s_st->notify?L"1":L"0");
    InvalidateRect(h,NULL,FALSE);
}
static void saveServerUrl(){
    if(!s_st || !s_st->eServer || serverRowIndex()<0) return;
    wchar_t buf[512]={0}; GetWindowTextW(s_st->eServer,buf,512);
    std::wstring v=trim(buf);
    if(!v.empty()) setSetting(L"server_url",v);
}
static void doAbout(HWND h){
    std::wstring msg=std::wstring(APP_NAME_W)+L"\n"
        L"سامانه پذیرش و مدیریت درمانگاه\n\n"
        L"نسخه: "+toFaDigits(APP_VERSION_W)+L"\n"
        L"اجرای تک‌فایل، سازگار با ویندوز ۷ تا ۱۱\n\n© آزادی طب";
    MessageBoxW(h,msg.c_str(),L"درباره برنامه",MB_OK|MB_ICONINFORMATION);
}
// reception profile editing (name+photo) — pending management approval -------
static void doProfile(HWND h){
    // Open the full profile-edit modal (current name read-only, new name edit,
    // photo picker, تأیید/انصراف). It queues a ProfReq for management approval.
    HWND root=GetAncestor(h,GA_ROOT); if(!root) root=h;
    showProfileDialog(root);
}

// --------------------------------------------------------------- painting --
static void drawToggle(HDC dc, int cx, int cy, bool on){
    int w=S(46), hh=S(24);
    RECT tr={cx-w/2,cy-hh/2,cx+w/2,cy+hh/2};
    gpRoundRect(dc,tr,hh/2,on?g_theme.accent:g_theme.border,CLR_INVALID,255);
    int kn=hh-S(6);
    int kx=on?(tr.right-S(3)-kn):(tr.left+S(3));
    RECT kr={kx,tr.top+S(3),kx+kn,tr.bottom-S(3)};
    gpRoundRect(dc,kr,kn/2,RGB(255,255,255),CLR_INVALID,255);
}
static void drawValueChip(HDC dc, RECT row, const wchar_t* val){
    SIZE sz; HGDIOBJ of=SelectObject(dc,g_fSmall);
    GetTextExtentPoint32W(dc,val,(int)wcslen(val),&sz);
    int pad=S(12);
    RECT chip={ row.left+S(8),(row.top+row.bottom)/2-S(13),
                row.left+S(8)+sz.cx+pad*2,(row.top+row.bottom)/2+S(13) };
    gpRoundRect(dc,chip,S(13),g_theme.surface2,g_theme.border,255);
    SetTextColor(dc,g_theme.textDim);
    DrawTextW(dc,val,-1,&chip,DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_NOPREFIX);
    SelectObject(dc,of);
}

// Build the static (expensive) background into the cache DC. Called only on
// open / size / theme change — never on hover.
static void buildBgCache(HWND h, HDC ref){
    RECT rc; GetClientRect(h,&rc);
    if(rc.right<=0||rc.bottom<=0) return;
    freeBgCache();
    s_bgDC=CreateCompatibleDC(ref);
    s_bgBmp=CreateCompatibleBitmap(ref,rc.right,rc.bottom);
    s_bgOld=SelectObject(s_bgDC,s_bgBmp);
    s_bgW=rc.right; s_bgH=rc.bottom;
    HDC dc=s_bgDC;

    // dim scrim
    { HBRUSH sb=CreateSolidBrush(g_dark?RGB(6,9,14):RGB(28,36,48));
      FillRect(dc,&rc,sb); DeleteObject(sb); }
    // a soft translucent dim using GDI+ for a glassy look
    gpFillAlpha(dc,rc,0,g_dark?RGB(0,0,0):RGB(20,28,40),120);

    RECT card=cardRect(h);
    // shadow + opaque card
    gpShadow(dc,card,S(20),S(22),80);
    { HBRUSH pb=CreateSolidBrush(g_theme.surface);
      FillRect(dc,&card,pb); DeleteObject(pb); }
    gpGradRoundRect(dc,card,S(20),g_theme.surfaceTop,g_theme.surface,g_theme.border);

    SetBkMode(dc,TRANSPARENT);

    // cover gradient (rounded top)
    RECT cover={card.left,card.top,card.right,card.top+S(96)};
    gpGradRoundRect(dc,cover,S(20),g_theme.accent2,g_theme.accent,CLR_INVALID);
    // re-square the cover bottom so it meets the card body cleanly
    RECT coverBot={card.left,card.top+S(60),card.right,card.top+S(96)};
    gpGradRoundRect(dc,coverBot,0,g_theme.accent2,g_theme.accent,CLR_INVALID);

    // close (×) icon top-left (the hover highlight is drawn live on top)
    { RECT cb={card.left+S(14),card.top+S(14),card.left+S(40),card.top+S(40)};
      RECT ci={cb.left+S(5),cb.top+S(5),cb.right-S(5),cb.bottom-S(5)};
      drawIcon(dc,ICO_X,ci,RGB(255,255,255),S(2)); }

    // avatar
    int avR=S(42), avCx=(card.left+card.right)/2, avCy=card.top+S(96);
    RECT avo={avCx-avR-S(4),avCy-avR-S(4),avCx+avR+S(4),avCy+avR+S(4)};
    gpRoundRect(dc,avo,avR+S(4),g_theme.surfaceTop,CLR_INVALID,255);
    RECT av={avCx-avR,avCy-avR,avCx+avR,avCy+avR};
    bool guest = (s_st && s_st->role<0);
    gpGradRoundRect(dc,av,avR,
        guest?g_theme.textDim:g_theme.accent2,
        guest?g_theme.border:g_theme.accent,CLR_INVALID);
    std::wstring fn = guest ? L"" : g_session.user.fullname;
    std::wstring photo = guest ? L"" : getSetting(L"photo_"+g_session.user.username,L"");
    if(!photo.empty() && gpDrawImageFileCircle(dc,photo,av)){
        // photo drawn into the circle — nothing else to do
    } else if(!fn.empty()){
        std::wstring ini=fn.substr(0,1);
        SelectObject(dc,g_fHuge); SetTextColor(dc,RGB(255,255,255));
        DrawTextW(dc,ini.c_str(),-1,&av,DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_NOPREFIX);
    } else {
        RECT ui={av.left+S(20),av.top+S(20),av.right-S(20),av.bottom-S(20)};
        drawIcon(dc,ICO_USER,ui,RGB(255,255,255),S(3));
    }

    // identity
    SelectObject(dc,g_fTitle); SetTextColor(dc,g_theme.text);
    RECT nr={card.left+S(16),avCy+avR+S(6),card.right-S(16),avCy+avR+S(38)};
    DrawTextW(dc, guest?L"کاربر مهمان":(fn.empty()?L"کاربر":fn.c_str()),-1,&nr,
        DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
    SelectObject(dc,g_fSmall); SetTextColor(dc,g_theme.textDim);
    const wchar_t* role = guest ? L"وارد نشده" :
        s_st->role==2 ? L"مدیر سامانه" :
        s_st->role==1 ? L"مدیریت درمانگاه" : L"پذیرش درمانگاه";
    std::wstring sub=std::wstring(role)+
        (guest||g_session.user.dept.empty()?L"":(L"  •  "+g_session.user.dept));
    RECT srr={card.left+S(16),nr.bottom,card.right-S(16),nr.bottom+S(22)};
    DrawTextW(dc,sub.c_str(),-1,&srr,
        DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
}

// Draw the interactive rows (and live close-button hover) onto dc, clipped by
// the caller. Cheap enough to run on every hover change.
static void paintRows(HWND h, HDC dc){
    RECT card=cardRect(h);
    SetBkMode(dc,TRANSPARENT);
    // live close (×) hover highlight (icon itself lives in the cached bg)
    if(s_st && s_st->hot==-2){
        RECT cb={card.left+S(14),card.top+S(14),card.left+S(40),card.top+S(40)};
        gpRoundRect(dc,cb,S(8),RGB(255,255,255),CLR_INVALID,60);
        RECT ci={cb.left+S(5),cb.top+S(5),cb.right-S(5),cb.bottom-S(5)};
        drawIcon(dc,ICO_X,ci,RGB(255,255,255),S(2));
    }
    if(s_st){
        for(size_t i=0;i<s_st->rows.size();i++){
            RowDef& rd=s_st->rows[i];
            RECT r=rowRect(card,(int)i);
            bool hov=(s_st->hot==rd.id);
            bool danger=(rd.id==ROW_LOGOUT);
            gpRoundRect(dc,r,S(11),hov?g_theme.hover:g_theme.surface,
                hov?g_theme.accent:g_theme.border,255);
            COLORREF ic=danger?g_theme.danger:g_theme.accent;
            RECT ir={r.right-S(38),r.top+S(10),r.right-S(14),r.top+S(34)};
            drawIcon(dc,rd.icon,ir,ic,S(2));
            SelectObject(dc,g_fUIB);
            SetTextColor(dc,danger?g_theme.danger:g_theme.text);
            bool hasHint=rd.hint!=NULL;
            RECT lr={r.left+S(14),r.top+(hasHint?S(7):S(0)),r.right-S(46),
                     r.top+(hasHint?S(30):rowH()-S(8))};
            DrawTextW(dc,rd.label,-1,&lr,
                DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
            if(hasHint){
                SelectObject(dc,g_fSmall); SetTextColor(dc,g_theme.textDim);
                RECT hr={r.left+S(14),r.top+S(30),r.right-S(46),r.bottom-S(4)};
                DrawTextW(dc,rd.hint,-1,&hr,
                    DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
            }
            int lcx=r.left+S(34), lcy=(r.top+r.bottom)/2;
            if(rd.id==ROW_THEME)        drawValueChip(dc,r, s_st->dark?L"تیره":L"روشن");
            else if(rd.id==ROW_DENSITY) drawValueChip(dc,r, s_st->compact?L"فشرده":L"متعارف");
            else if(rd.id==ROW_AUTOPRINT) drawToggle(dc,lcx,lcy,s_st->autoPrint);
            else if(rd.id==ROW_NOTIFY)  drawToggle(dc,lcx,lcy,s_st->notify);
            else if(rd.id==ROW_SERVER){ /* edit box overlays */ }
            else {
                RECT cv={r.left+S(14),lcy-S(8),r.left+S(28),lcy+S(8)};
                drawIcon(dc,ICO_CHEVRON,cv,g_theme.textDim,S(2));
            }
        }
    }
}

// Composite: blit the cached static background for the dirty strip, draw the
// (cheap) rows on top, then copy out. On open / size / theme change the cache
// is (re)built first; on hover it is reused, so the heavy GDI+ scrim/shadow/
// gradient work runs ONCE, not on every mouse move.
static void paintPanel(HWND h, HDC dc0, const RECT* dirty){
    RECT rc; GetClientRect(h,&rc);
    if(rc.right<=0||rc.bottom<=0) return;
    if(!s_bgDC || s_bgW!=rc.right || s_bgH!=rc.bottom)
        buildBgCache(h,dc0);
    if(!s_bgDC) return;

    HDC dc=CreateCompatibleDC(dc0);
    HBITMAP bmp=CreateCompatibleBitmap(dc0,rc.right,rc.bottom);
    HGDIOBJ obm=SelectObject(dc,bmp);

    RECT d = (dirty && dirty->right>dirty->left && dirty->bottom>dirty->top)
             ? *dirty : rc;
    int dw=d.right-d.left, dh=d.bottom-d.top;
    // 1) bring back the cached static layers for the dirty region only
    BitBlt(dc,d.left,d.top,dw,dh,s_bgDC,d.left,d.top,SRCCOPY);
    // 2) draw the interactive rows on top (GDI clip keeps this to the strip)
    HRGN clip=CreateRectRgn(d.left,d.top,d.right,d.bottom);
    SelectClipRgn(dc,clip);
    paintRows(h,dc);
    SelectClipRgn(dc,NULL); DeleteObject(clip);
    // 3) copy the composited strip to the screen
    BitBlt(dc0,d.left,d.top,dw,dh,dc,d.left,d.top,SRCCOPY);

    SelectObject(dc,obm); DeleteObject(bmp); DeleteDC(dc);
}

// ----------------------------------------------------------------- wndproc -
static LRESULT CALLBACK setProc(HWND h, UINT m, WPARAM w, LPARAM l){
    switch(m){
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: { PAINTSTRUCT ps; HDC dc=BeginPaint(h,&ps);
        // pass the invalidated rect so hover repaints copy only that strip
        RECT dirty=ps.rcPaint;
        paintPanel(h,dc, &dirty); EndPaint(h,&ps); return 0; }
    case WM_APP_THEME: freeBgCache(); InvalidateRect(h,NULL,FALSE); return 0;
    case WM_MOUSEMOVE: {
        POINT pt={GET_X_LPARAM(l),GET_Y_LPARAM(l)};
        RECT card=cardRect(h);
        int hr;
        RECT cb={card.left+S(14),card.top+S(14),card.left+S(40),card.top+S(40)};
        if(PtInRect(&cb,pt)) hr=-2; else hr=hitRow(h,pt);
        if(s_st && hr!=s_st->hot){
            // v1.7.0 perf: repaint ONLY the two affected rectangles (the row we
            // left + the row we entered) instead of the whole scrim. This stops
            // the heavy full-screen redraw (shadow+gradient+alpha) on every
            // mouse move that made the panel stutter.
            RECT oldR, newR;
            bool haveOld=hotRectFor(h,s_st->hot,oldR);
            int old=s_st->hot; s_st->hot=hr;
            bool haveNew=hotRectFor(h,hr,newR);
            (void)old;
            if(haveOld) InvalidateRect(h,&oldR,FALSE);
            if(haveNew) InvalidateRect(h,&newR,FALSE);
        }
        TRACKMOUSEEVENT te={sizeof(te),TME_LEAVE,h,0}; TrackMouseEvent(&te);
        return 0; }
    case WM_MOUSELEAVE:
        if(s_st && s_st->hot!=0){
            RECT oldR; bool haveOld=hotRectFor(h,s_st->hot,oldR);
            s_st->hot=0;
            if(haveOld) InvalidateRect(h,&oldR,FALSE);
        }
        return 0;
    case WM_LBUTTONDOWN: {
        POINT pt={GET_X_LPARAM(l),GET_Y_LPARAM(l)};
        RECT card=cardRect(h);
        RECT cb={card.left+S(14),card.top+S(14),card.left+S(40),card.top+S(40)};
        if(PtInRect(&cb,pt)){ closeSettingsPanel(); return 0; }
        int id=hitRow(h,pt);
        if(id==-1){ closeSettingsPanel(); return 0; }   // scrim
        switch(id){
            case ROW_THEME:       doThemeToggle(h); break;
            case ROW_UPDATE:      saveServerUrl(); checkRemoteUpdate(h); break;
            case ROW_NOTIFY:      doNotifyToggle(h); break;
            case ROW_PRINTER:     closeSettingsPanel(); openPrinterSettings(g_hFrame); break;
            case ROW_PRINTDESIGN: closeSettingsPanel(); openPrinterSettings(g_hFrame); break;
            case ROW_PROFILE:     doProfile(h); break;
            case ROW_DENSITY:     doDensityToggle(h); break;
            case ROW_AUTOPRINT:   doAutoPrintToggle(h); break;
            case ROW_SERVER:      if(s_st&&s_st->eServer) SetFocus(s_st->eServer); break;
            case ROW_ABOUT:       doAbout(h); break;
            case ROW_LOGOUT:      saveServerUrl(); closeSettingsPanel();
                                  PostMessageW(g_hFrame,WM_COMMAND,101,0); break;
        }
        return 0; }
    case WM_KEYDOWN:
        if(w==VK_ESCAPE){ closeSettingsPanel(); return 0; }
        break;
    case WM_COMMAND:
        if(LOWORD(w)==IDC_SRV && HIWORD(w)==EN_KILLFOCUS) saveServerUrl();
        return 0;
    case WM_CTLCOLOREDIT: { HDC dc=(HDC)w;
        SetTextColor(dc,g_theme.inputText); SetBkColor(dc,g_theme.inputBg);
        return (LRESULT)g_brInput; }
    case WM_SIZE: freeBgCache(); layoutServerEdit(h); InvalidateRect(h,NULL,FALSE); return 0;
    case WM_DESTROY:
        freeBgCache();
        if(s_st){ delete s_st; s_st=NULL; }
        s_set=NULL; return 0;
    }
    return DefWindowProcW(h,m,w,l);
}

// ----- build the row list for the current role -----------------------------
static void buildRows(SetState* st){
    st->rows.clear();
    bool guest = st->role<0;
    if(guest){
        // ONLY update + theme before login
        st->rows.push_back({ROW_UPDATE,L"بررسی به‌روزرسانی",ICO_UPDATE,L"دریافت آخرین نسخه",false});
        st->rows.push_back({ROW_THEME, L"تغییر پوسته (تم)",  ICO_MOON,  NULL,false});
        return;
    }
    // reception + management
    st->rows.push_back({ROW_THEME,   L"تغییر پوسته (تم)",   ICO_MOON,  NULL,false});
    st->rows.push_back({ROW_UPDATE,  L"بررسی به‌روزرسانی",  ICO_UPDATE,L"دریافت آخرین نسخه",false});
    st->rows.push_back({ROW_NOTIFY,  L"تنظیمات اعلان",      ICO_BELL,  L"صدا و هشدار پیام جدید",false});
    st->rows.push_back({ROW_PRINTER, L"تنظیمات چاپگر",      ICO_PRINT, L"پرینتر، اندازه و طراحی چاپ",false});
    if(st->role==0){
        // reception can request a profile change (needs approval)
        st->rows.push_back({ROW_PROFILE, L"پروفایل کاربر",  ICO_USER,  L"نام و عکس (نیازمند تأیید مدیریت)",false});
        st->rows.push_back({ROW_AUTOPRINT,L"چاپ خودکار قبض",ICO_RECEIPT,NULL,false});
    }
    if(st->role>=1){
        st->rows.push_back({ROW_AUTOPRINT,L"چاپ خودکار قبض",ICO_RECEIPT,NULL,false});
        st->rows.push_back({ROW_SERVER,  L"آدرس سامانهٔ مدیریت",ICO_SHIELD,NULL,false});
    }
    st->rows.push_back({ROW_ABOUT,   L"درباره برنامه",      ICO_BELL,  L"نسخه و اطلاعات",false});
    st->rows.push_back({ROW_LOGOUT,  L"خروج از حساب",       ICO_LOGOUT,NULL,false});
}

// ------------------------------------------------------------------ public -
void openSettingsPanel(HWND frameOwner){
    if(s_set && IsWindow(s_set)){ closeSettingsPanel(); return; }
    static bool reg=false;
    if(!reg){
        WNDCLASSW wc={0};
        wc.lpfnWndProc=setProc; wc.hInstance=g_hInst;
        wc.hCursor=LoadCursor(NULL,IDC_ARROW);
        wc.lpszClassName=SET_CLASS;
        RegisterClassW(&wc); reg=true;
    }
    RECT rc; GetClientRect(frameOwner,&rc);
    POINT org={0,0}; ClientToScreen(frameOwner,&org);
    s_st=new SetState();
    s_st->owner=frameOwner; s_st->hot=0;
    s_st->dark=g_dark;
    s_st->compact=(getSetting(L"density",L"normal")==L"compact");
    s_st->autoPrint=(getSetting(L"auto_print",L"0")==L"1");
    s_st->notify=(getSetting(L"notify",L"1")==L"1");
    s_st->role = g_session.user.username.empty() ? -1 : g_session.user.role;
    buildRows(s_st);

    s_set=CreateWindowExW(WS_EX_TOPMOST,SET_CLASS,L"",
        WS_POPUP|WS_VISIBLE|WS_CLIPCHILDREN,
        org.x,org.y,rc.right,rc.bottom,frameOwner,NULL,g_hInst,NULL);

    s_st->eServer=CreateWindowExW(0,L"EDIT",
        getSetting(L"server_url",L"").c_str(),
        WS_CHILD|ES_AUTOHSCROLL,0,0,10,10,s_set,(HMENU)IDC_SRV,g_hInst,NULL);
    SendMessageW(s_st->eServer,WM_SETFONT,(WPARAM)g_fSmall,TRUE);
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
