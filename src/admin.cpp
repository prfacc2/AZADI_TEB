// ============================================================================
//  admin.cpp — hidden admin panel (create users, list/delete users)
//              + management panel placeholder (extensible)
// ============================================================================
#include "app.h"
#include <commctrl.h>
#include <stdio.h>
#include <algorithm>

// ================================================================ ADMIN ====
#define AD_CLASS L"AzAdmin"
#define ID_AD_FULL    401
#define ID_AD_USER    402
#define ID_AD_PASS    403
#define ID_AD_DEPT    404
#define ID_AD_ROLE    405
#define ID_AD_CREATE  406
#define ID_AD_LIST    407
#define ID_AD_DELETE  408

struct AdminData {
    HWND eFull, eUser, ePass, eDept, cRole, bCreate, list, bDelete;
    std::wstring msg; COLORREF msgCol;
};

static void adRefreshList(AdminData* d){
    ListView_DeleteAllItems(d->list);
    auto us = loadUsers();
    int i=0;
    for(auto& u : us){
        LVITEMW it={0}; it.mask=LVIF_TEXT; it.iItem=i;
        it.pszText=(LPWSTR)u.username.c_str();
        ListView_InsertItem(d->list,&it);
        ListView_SetItemText(d->list,i,1,(LPWSTR)u.fullname.c_str());
        ListView_SetItemText(d->list,i,2,(LPWSTR)u.dept.c_str());
        ListView_SetItemText(d->list,i,3,
            (LPWSTR)(u.role==0?L"پذیرش":L"مدیریت"));
        i++;
    }
}
static void adLayout(HWND h, AdminData* d){
    RECT rc; GetClientRect(h,&rc);
    int W=rc.right;
    // form card on the right (RTL), list on the left
    int fw=S(360), pad=S(24);
    int fx=W-pad-fw, fy=S(90);
    int ew=fw-S(40), ex=fx+S(20);
    int rh=S(40), gap=S(64);
    MoveWindow(d->eFull, ex, fy+S(28),        ew, rh, TRUE);
    MoveWindow(d->eUser, ex, fy+S(28)+gap,    ew, rh, TRUE);
    MoveWindow(d->ePass, ex, fy+S(28)+2*gap,  ew, rh, TRUE);
    MoveWindow(d->eDept, ex, fy+S(28)+3*gap,  ew, rh, TRUE);
    MoveWindow(d->cRole, ex, fy+S(28)+4*gap,  ew, S(200), TRUE);
    MoveWindow(d->bCreate, ex, fy+S(28)+5*gap+S(8), ew, S(46), TRUE);

    int lx=pad, lw=W-3*pad-fw;
    MoveWindow(d->list, lx, S(118), lw, rc.bottom-S(180), TRUE);
    MoveWindow(d->bDelete, lx, rc.bottom-S(54), S(180), S(40), TRUE);
}
static LRESULT CALLBACK adminProc(HWND h, UINT m, WPARAM w, LPARAM l){
    AdminData* d=(AdminData*)GetWindowLongPtrW(h,GWLP_USERDATA);
    switch(m){
    case WM_CREATE: {
        d = new AdminData(); d->msgCol = g_theme.textDim;
        SetWindowLongPtrW(h,GWLP_USERDATA,(LONG_PTR)d);
        DWORD es=WS_CHILD|WS_VISIBLE|WS_TABSTOP|ES_AUTOHSCROLL;
        d->eFull=CreateWindowExW(0,L"EDIT",L"",es,0,0,10,10,h,(HMENU)ID_AD_FULL,g_hInst,0);
        d->eUser=CreateWindowExW(0,L"EDIT",L"",es,0,0,10,10,h,(HMENU)ID_AD_USER,g_hInst,0);
        d->ePass=CreateWindowExW(0,L"EDIT",L"",es|ES_PASSWORD,0,0,10,10,h,(HMENU)ID_AD_PASS,g_hInst,0);
        d->eDept=CreateWindowExW(0,L"EDIT",L"",es,0,0,10,10,h,(HMENU)ID_AD_DEPT,g_hInst,0);
        d->cRole=CreateWindowExW(0,L"COMBOBOX",L"",
            WS_CHILD|WS_VISIBLE|WS_TABSTOP|CBS_DROPDOWNLIST,
            0,0,10,10,h,(HMENU)ID_AD_ROLE,g_hInst,0);
        SendMessageW(d->cRole,CB_ADDSTRING,0,(LPARAM)L"پذیرش");
        SendMessageW(d->cRole,CB_ADDSTRING,0,(LPARAM)L"مدیریت");
        SendMessageW(d->cRole,CB_SETCURSEL,0,0);
        d->bCreate=createFlatButton(h,ID_AD_CREATE,L"ساخت کاربر",ICO_PLUS,BS_PRIMARY,0,0,10,10);
        // user table (LAYOUTRTL only on the ListView itself is safe:
        // the control paints itself, no custom BitBlt involved)
        d->list=CreateWindowExW(WS_EX_LAYOUTRTL|WS_EX_RTLREADING,WC_LISTVIEWW,L"",
            WS_CHILD|WS_VISIBLE|WS_TABSTOP|LVS_REPORT|LVS_SINGLESEL|LVS_SHOWSELALWAYS,
            0,0,10,10,h,(HMENU)ID_AD_LIST,g_hInst,0);
        // v1.8.0: drop GRIDLINES — borderless, clean, theme-integrated list.
        ListView_SetExtendedListViewStyle(d->list,
            LVS_EX_FULLROWSELECT|LVS_EX_DOUBLEBUFFER);
        const wchar_t* cols[4]={L"نام کاربری",L"نام شخص",L"بخش",L"نوع دسترسی"};
        int widths[4]={S(140),S(190),S(170),S(120)};
        for(int i=0;i<4;i++){
            LVCOLUMNW c={0}; c.mask=LVCF_TEXT|LVCF_WIDTH;
            c.pszText=(LPWSTR)cols[i]; c.cx=widths[i];
            ListView_InsertColumn(d->list,i,&c);
        }
        SendMessageW(d->list,WM_SETFONT,(WPARAM)g_fUI,TRUE);
        ListView_SetBkColor(d->list,g_theme.surface);
        ListView_SetTextBkColor(d->list,g_theme.surface);
        ListView_SetTextColor(d->list,g_theme.text);
        d->bDelete=createFlatButton(h,ID_AD_DELETE,L"حذف کاربر انتخابی",ICO_TRASH,BS_DANGER,0,0,10,10);
        HWND eds[4]={d->eFull,d->eUser,d->ePass,d->eDept};
        for(int i=0;i<4;i++){
            SendMessageW(eds[i],WM_SETFONT,(WPARAM)g_fUI,TRUE);
            enableEnterNavigation(eds[i]);
        }
        SendMessageW(d->cRole,WM_SETFONT,(WPARAM)g_fUI,TRUE);
        adRefreshList(d);
        return 0; }
    case WM_NCDESTROY: delete d; break;
    case WM_SIZE: if(d) adLayout(h,d); return 0;
    case WM_APP_THEME:        // v1.1.0: re-color ListView on theme switch
        if(d && d->list){
            ListView_SetBkColor(d->list,g_theme.surface);
            ListView_SetTextBkColor(d->list,g_theme.surface);
            ListView_SetTextColor(d->list,g_theme.text);
            InvalidateRect(d->list,NULL,TRUE);
        }
        return 0;
    case WM_CTLCOLOREDIT: {
        HDC dc=(HDC)w;
        SetTextColor(dc,g_theme.inputText); SetBkColor(dc,g_theme.inputBg);
        return (LRESULT)g_brInput; }
    case WM_CTLCOLORLISTBOX: {
        HDC dc=(HDC)w;
        SetTextColor(dc,g_theme.inputText); SetBkColor(dc,g_theme.inputBg);
        return (LRESULT)g_brInput; }
    case WM_CTLCOLORSTATIC: {
        HDC dc=(HDC)w;
        SetTextColor(dc,g_theme.inputText); SetBkColor(dc,g_theme.inputBg);
        return (LRESULT)g_brInput; }
    case WM_COMMAND: {
        if(!d) return 0;
        int id=LOWORD(w);
        if(id==ID_AD_CREATE){
            wchar_t fb[128],ub[128],pb[128],db[128];
            GetWindowTextW(d->eFull,fb,128); GetWindowTextW(d->eUser,ub,128);
            GetWindowTextW(d->ePass,pb,128); GetWindowTextW(d->eDept,db,128);
            int role=(int)SendMessageW(d->cRole,CB_GETCURSEL,0,0);
            if(!wcslen(pb)){
                d->msg=L"رمز عبور نمی‌تواند خالی باشد."; d->msgCol=g_theme.danger;
            } else {
                User u; u.fullname=trim(fb); u.username=trim(ub);
                u.dept=trim(db); u.role=role; u.hash=hashPassword(pb);
                std::wstring err;
                if(addUser(u,err)){
                    d->msg=L"کاربر «"+u.username+L"» با موفقیت ساخته شد.";
                    d->msgCol=g_theme.success;
                    SetWindowTextW(d->eFull,L""); SetWindowTextW(d->eUser,L"");
                    SetWindowTextW(d->ePass,L""); SetWindowTextW(d->eDept,L"");
                    adRefreshList(d);
                } else { d->msg=err; d->msgCol=g_theme.danger; }
            }
            InvalidateRect(h,NULL,TRUE);
        }
        else if(id==ID_AD_DELETE){
            int sel=ListView_GetNextItem(d->list,-1,LVNI_SELECTED);
            if(sel>=0){
                wchar_t un[128];
                ListView_GetItemText(d->list,sel,0,un,128);
                std::wstring q=L"کاربر «"+std::wstring(un)+L"» حذف شود؟";
                if(MessageBoxW(h,q.c_str(),L"حذف کاربر",
                    MB_YESNO|MB_ICONWARNING)==IDYES){
                    removeUser(un); adRefreshList(d);
                    d->msg=L"کاربر حذف شد."; d->msgCol=g_theme.textDim;
                    InvalidateRect(h,NULL,TRUE);
                }
            }
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

        SetTextColor(dc,g_theme.text);
        SelectObject(dc,g_fTitle);
        RECT tr={S(24),S(18),rc.right-S(24),S(56)};
        DrawTextW(dc,L"پنل مخفی ادمین — مدیریت کاربران",-1,&tr,
            DT_RIGHT|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);

        // form card frame
        int fw=S(360), pad=S(24);
        int fx=rc.right-pad-fw, fy=S(90);
        RECT card={fx-S(0),fy-S(20),fx+fw,fy+S(28)+5*S(64)+S(76)};
        fillRoundRect(dc,card,S(14),g_theme.surface,g_theme.border);

        SelectObject(dc,g_fSmall);
        SetTextColor(dc,g_theme.textDim);
        const wchar_t* labels[5]={L"نام شخص (استفاده‌کننده)",L"نام کاربری (برای ورود)",
            L"رمز عبور",L"بخش (مثلاً دندانپزشکی)",L"نوع دسترسی"};
        for(int i=0;i<5;i++){
            RECT lr={fx+S(20),fy+i*S(64)+S(2),fx+fw-S(20),fy+i*S(64)+S(26)};
            DrawTextW(dc,labels[i],-1,&lr,
                DT_RIGHT|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);
        }
        // message
        if(d && !d->msg.empty()){
            SetTextColor(dc,d->msgCol);
            RECT mrr={fx+S(10),card.bottom+S(8),fx+fw-S(10),card.bottom+S(60)};
            DrawTextW(dc,d->msg.c_str(),-1,&mrr,
                DT_RIGHT|DT_WORDBREAK|DT_RTLREADING|DT_NOPREFIX);
        }
        // list title
        SetTextColor(dc,g_theme.text);
        SelectObject(dc,g_fUIB);
        RECT lt={pad,S(88),rc.right-2*pad-fw,S(114)};
        DrawTextW(dc,L"لیست کاربران",-1,&lt,
            DT_RIGHT|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);

        BitBlt(dc0,0,0,rc.right,rc.bottom,dc,0,0,SRCCOPY);
        SelectObject(dc,obm); DeleteObject(bmp); DeleteDC(dc);
        EndPaint(h,&ps);
        return 0; }
    }
    return DefWindowProcW(h,m,w,l);
}
HWND createAdminScreen(HWND frame){
    static bool reg=false;
    if(!reg){
        WNDCLASSW wc={0};
        wc.lpfnWndProc=adminProc; wc.hInstance=g_hInst;
        wc.hCursor=LoadCursor(NULL,IDC_ARROW);
        wc.lpszClassName=AD_CLASS;
        RegisterClassW(&wc); reg=true;
    }
    RECT rc=frameContentRect();
    return CreateWindowExW(0,AD_CLASS,L"",
        WS_CHILD|WS_VISIBLE|WS_CLIPCHILDREN,
        rc.left,rc.top,rc.right-rc.left,rc.bottom-rc.top,frame,NULL,g_hInst,NULL);
}

// ============================================================ MANAGEMENT ===
//  Full management panel (v1.4.0):
//   • Department CATEGORIES (add name/manager/auto-or-manual id/icon).
//   • Filter the category grid by alphabet / newest.
//   • Click a category → employee TABLE (photo, name, username, shift,
//     online status «برخط»/offline, details button).
//   • Details → formal full info + print preview (A4/A5) of the personnel card.
//   • Create-employee request collects full identity + doc paths.
//   • Printer-design access toggle (per the brief) opens the print designer.
#include "manage.inc"
