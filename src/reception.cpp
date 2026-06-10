// ============================================================================
//  reception.cpp — reception workspace:
//   • info bar (current user, access type, live Iran date/time, calculator)
//   • browser-style tab strip: open/close/detach tabs (پذیرش <بخش>)
//   • reception form (Enter = next field) + live billing (Iranian insurances)
//   • print: رسید بیمه / چاپ نسخه / چاپ آخرین قبض (F8) — real printer output
// ============================================================================
#include "app.h"
#include <commctrl.h>
#include <stdio.h>

#define RC_CLASS   L"AzReception"
#define TABPG_CLASS L"AzRecTab"
#define DET_CLASS  L"AzDetached"

// info-bar buttons
#define ID_RC_CALC    501
#define ID_RC_NEWTAB  502
// per-tab form ids
#define ID_F_FIRST    601   // base for sequential edits
#define ID_F_GENDER   620
#define ID_F_PTYPE    621
#define ID_F_INS      622
#define ID_F_SUPP     623
#define ID_F_NTYPE    624
#define ID_F_PRICE    630
#define ID_F_DISCOUNT 631
#define ID_F_SUBMIT   640
#define ID_F_PRT_INS  641
#define ID_F_PRT_RX   642
#define ID_F_PRT_LAST 643
#define ID_F_CLOSE    644

// ============================================================== TAB PAGE ===
struct TabPage {
    HWND page;                // container window (child of reception OR detached)
    std::wstring title;
    // form controls
    HWND eFirst,eLast,eNid,eFather,eBirth,cGender,eMobile,ePhone,eAddr;
    HWND cPType,cIns,cSupp,cNType;
    HWND ePrice,eDiscount;
    HWND bSubmit,bPrtIns,bPrtRx,bPrtLast,bClose;
    // computed billing
    long long total,mainShare,patientShare,baseDiff,orgShare,paid;
    bool detached;
    std::wstring lastMsg; COLORREF msgCol;
    TabPage():page(0),total(0),mainShare(0),patientShare(0),baseDiff(0),
        orgShare(0),paid(0),detached(false),msgCol(0){}
};

struct RecData {
    HWND bCalc, bNewTab;
    std::vector<TabPage*> tabs;
    int active;
    int hotTab, hotClose, hotDetach;     // hover indices
    RecData():active(-1),hotTab(-1),hotClose(-1),hotDetach(-1){}
};
static RecData* s_rd = NULL;             // single reception screen at a time

// metrics
static int infoBarH(){ return S(54); }
static int tabBarH(){ return S(40); }
static int tabW()    { return S(210); }

// ---------------------------------------------------------------- billing --
static void recalc(TabPage* t){
    if(!t || !t->ePrice || !t->eDiscount) return;
    wchar_t buf[64];
    GetWindowTextW(t->ePrice,buf,64);
    long long price = parseMoney(buf);
    GetWindowTextW(t->eDiscount,buf,64);
    long long disc = parseMoney(buf);

    int insIdx  = (int)SendMessageW(t->cIns, CB_GETCURSEL,0,0);
    int suppIdx = (int)SendMessageW(t->cSupp,CB_GETCURSEL,0,0);
    if(insIdx<0  || insIdx>=N_INSURANCES) insIdx=0;
    if(suppIdx<0 || suppIdx>=N_SUPP)      suppIdx=0;

    t->total      = price;
    t->mainShare  = price * INSURANCES[insIdx].pct / 100;
    t->baseDiff   = price - t->mainShare;                 // مابه‌التفاوت پایه
    t->orgShare   = t->baseDiff * SUPP_INSURANCES[suppIdx].pct / 100;
    t->patientShare = t->baseDiff - t->orgShare;
    long long pay = t->patientShare - disc;
    if(pay < 0) pay = 0;
    t->paid = pay;
}

// ------------------------------------------------------------- tab layout --
// Manual RTL, plain (non-mirrored) coordinates:
//   • billing card pinned to the RIGHT edge of the page
//   • form occupies the remaining LEFT area
//   • "right column" (first field of each row) sits at the form's right side
static void rcMetrics(int W, int pad, int& bw, int& formLeft, int& formRight,
                      int& fw, int& colW, int& xr, int& xl, bool& stacked){
    bw = S(330);                               // billing card width
    formLeft  = pad;
    formRight = W - pad - bw - pad;            // form ends before billing card
    fw = formRight - formLeft;
    stacked = fw < S(300);
    if(stacked){ bw = 0; formRight = W - pad; fw = formRight - formLeft; }
    colW = (fw - S(20))/2;
    xr = formRight - colW;                     // right column (RTL first)
    xl = formLeft;                             // left column
}
//  v1.1.0: vertical metrics adapt to available height so the form never
//  overflows on small / low-res monitors (responsive requirement).
static void rcVMetrics(int H, int& y0, int& step, int& rh){
    y0 = S(46); step = S(62); rh = S(36);
    int need = y0 + 8*step + S(120);           // rows + submit + messages
    if(H > 0 && need > H){
        step = (H - S(150)) / 8;
        if(step < S(44)) step = S(44);
        y0 = S(40);
        rh = step - S(26); if(rh > S(36)) rh = S(36); if(rh < S(24)) rh = S(24);
    }
}
static void tabPageLayout(HWND h, TabPage* t){
    if(!t) return;
    RECT rc; GetClientRect(h,&rc);
    int W=rc.right, H=rc.bottom;
    if(W<=0 || H<=0) return;
    int pad=S(18);
    int bw,formLeft,formRight,fw,colW,xr,xl; bool stacked;
    rcMetrics(W,pad,bw,formLeft,formRight,fw,colW,xr,xl,stacked);
    int y0,step,rh; rcVMetrics(H,y0,step,rh);

    MoveWindow(t->eFirst, xr, y0,          colW, rh, TRUE);
    MoveWindow(t->eLast,  xl, y0,          colW, rh, TRUE);
    MoveWindow(t->eNid,   xr, y0+step,     colW, rh, TRUE);
    MoveWindow(t->eFather,xl, y0+step,     colW, rh, TRUE);
    MoveWindow(t->eBirth, xr, y0+2*step,   colW, rh, TRUE);
    MoveWindow(t->cGender,xl, y0+2*step,   colW, S(200), TRUE);
    MoveWindow(t->eMobile,xr, y0+3*step,   colW, rh, TRUE);
    MoveWindow(t->ePhone, xl, y0+3*step,   colW, rh, TRUE);
    MoveWindow(t->eAddr,  formLeft, y0+4*step, fw, rh, TRUE);
    MoveWindow(t->cPType, xr, y0+5*step,   colW, S(200), TRUE);
    MoveWindow(t->cNType, xl, y0+5*step,   colW, S(200), TRUE);
    MoveWindow(t->cIns,   xr, y0+6*step,   colW, S(220), TRUE);
    MoveWindow(t->cSupp,  xl, y0+6*step,   colW, S(220), TRUE);
    MoveWindow(t->ePrice, xr, y0+7*step,   colW, rh, TRUE);
    MoveWindow(t->eDiscount,xl,y0+7*step,  colW, rh, TRUE);
    MoveWindow(t->bSubmit,formLeft, y0+8*step+S(4), fw, S(48), TRUE);

    // billing panel buttons (bottom of billing card, right edge)
    if(!stacked){
        int cardL = W - pad - bw;
        int bx = cardL + S(12), byy = H - S(206);
        int bbw = bw - S(24);
        MoveWindow(t->bPrtIns, bx, byy,        bbw, S(40), TRUE);
        MoveWindow(t->bPrtRx,  bx, byy+S(48),  bbw, S(40), TRUE);
        MoveWindow(t->bPrtLast,bx, byy+S(96),  bbw, S(40), TRUE);
        MoveWindow(t->bClose,  bx, byy+S(150), bbw, S(40), TRUE);
        ShowWindow(t->bPrtIns,SW_SHOW); ShowWindow(t->bPrtRx,SW_SHOW);
        ShowWindow(t->bPrtLast,SW_SHOW); ShowWindow(t->bClose,SW_SHOW);
    } else {
        ShowWindow(t->bPrtIns,SW_HIDE); ShowWindow(t->bPrtRx,SW_HIDE);
        ShowWindow(t->bPrtLast,SW_HIDE); ShowWindow(t->bClose,SW_HIDE);
    }
}

// gather form into record
static void collect(TabPage* t, ReceptionRecord& r){
    wchar_t b[512];
    GetWindowTextW(t->eFirst,b,512);  r.firstName=trim(b);
    GetWindowTextW(t->eLast,b,512);   r.lastName=trim(b);
    GetWindowTextW(t->eNid,b,512);    r.nationalId=trim(b);
    GetWindowTextW(t->eFather,b,512); r.fatherName=trim(b);
    GetWindowTextW(t->eBirth,b,512);  r.birthDate=trim(b);
    GetWindowTextW(t->eMobile,b,512); r.mobile=trim(b);
    GetWindowTextW(t->ePhone,b,512);  r.landline=trim(b);
    GetWindowTextW(t->eAddr,b,512);   r.address=trim(b);
    int gi=(int)SendMessageW(t->cGender,CB_GETCURSEL,0,0);
    r.gender = gi==1?L"زن":L"مرد";
    int pi=(int)SendMessageW(t->cPType,CB_GETCURSEL,0,0);
    r.patientType = pi==1?L"سرپایی":pi==2?L"بستری":L"عادی";
    int ni=(int)SendMessageW(t->cNType,CB_GETCURSEL,0,0);
    if(ni==1) r.patientType += L" — اورژانس";
    else if(ni==2) r.patientType += L" — پرسنلی";
    int ii=(int)SendMessageW(t->cIns,CB_GETCURSEL,0,0);  if(ii<0||ii>=N_INSURANCES)ii=0;
    int si=(int)SendMessageW(t->cSupp,CB_GETCURSEL,0,0); if(si<0||si>=N_SUPP)si=0;
    r.insIdx=ii; r.suppIdx=si;
    r.insurance=INSURANCES[ii].name; r.suppInsurance=SUPP_INSURANCES[si].name;
    recalc(t);
    r.total=t->total; r.mainShare=t->mainShare; r.patientShare=t->patientShare;
    r.baseDiff=t->baseDiff; r.orgShare=t->orgShare;
    wchar_t db[64]; GetWindowTextW(t->eDiscount,db,64);
    r.discount=parseMoney(db);
    r.paid=t->paid; r.finalTotal=t->total;
    r.shift=shiftName(g_session.shift);
    r.dept=g_session.user.dept;
    r.userName=g_session.user.username;
}

static void closeTab(TabPage* t);   // fwd

// ----------------------------------------------------------- tab page proc -
static LRESULT CALLBACK tabPageProc(HWND h, UINT m, WPARAM w, LPARAM l){
    TabPage* t=(TabPage*)GetWindowLongPtrW(h,GWLP_USERDATA);
    switch(m){
    case WM_CREATE: {
        CREATESTRUCTW* cs=(CREATESTRUCTW*)l;
        t=(TabPage*)cs->lpCreateParams;
        SetWindowLongPtrW(h,GWLP_USERDATA,(LONG_PTR)t);
        t->page=h;
        DWORD es=WS_CHILD|WS_VISIBLE|WS_TABSTOP|ES_AUTOHSCROLL;
        DWORD cbs=WS_CHILD|WS_VISIBLE|WS_TABSTOP|CBS_DROPDOWNLIST;
        t->eFirst =CreateWindowExW(0,L"EDIT",L"",es,0,0,10,10,h,(HMENU)(ID_F_FIRST+0),g_hInst,0);
        t->eLast  =CreateWindowExW(0,L"EDIT",L"",es,0,0,10,10,h,(HMENU)(ID_F_FIRST+1),g_hInst,0);
        t->eNid   =CreateWindowExW(0,L"EDIT",L"",es|ES_NUMBER,0,0,10,10,h,(HMENU)(ID_F_FIRST+2),g_hInst,0);
        t->eFather=CreateWindowExW(0,L"EDIT",L"",es,0,0,10,10,h,(HMENU)(ID_F_FIRST+3),g_hInst,0);
        t->eBirth =CreateWindowExW(0,L"EDIT",L"",es,0,0,10,10,h,(HMENU)(ID_F_FIRST+4),g_hInst,0);
        t->cGender=CreateWindowExW(0,L"COMBOBOX",L"",cbs,0,0,10,10,h,(HMENU)ID_F_GENDER,g_hInst,0);
        t->eMobile=CreateWindowExW(0,L"EDIT",L"",es,0,0,10,10,h,(HMENU)(ID_F_FIRST+5),g_hInst,0);
        t->ePhone =CreateWindowExW(0,L"EDIT",L"",es,0,0,10,10,h,(HMENU)(ID_F_FIRST+6),g_hInst,0);
        t->eAddr  =CreateWindowExW(0,L"EDIT",L"",es,0,0,10,10,h,(HMENU)(ID_F_FIRST+7),g_hInst,0);
        t->cPType =CreateWindowExW(0,L"COMBOBOX",L"",cbs,0,0,10,10,h,(HMENU)ID_F_PTYPE,g_hInst,0);
        t->cNType =CreateWindowExW(0,L"COMBOBOX",L"",cbs,0,0,10,10,h,(HMENU)ID_F_NTYPE,g_hInst,0);
        t->cIns   =CreateWindowExW(0,L"COMBOBOX",L"",cbs,0,0,10,10,h,(HMENU)ID_F_INS,g_hInst,0);
        t->cSupp  =CreateWindowExW(0,L"COMBOBOX",L"",cbs,0,0,10,10,h,(HMENU)ID_F_SUPP,g_hInst,0);
        t->ePrice =CreateWindowExW(0,L"EDIT",L"",es,0,0,10,10,h,(HMENU)ID_F_PRICE,g_hInst,0);
        t->eDiscount=CreateWindowExW(0,L"EDIT",L"",es,0,0,10,10,h,(HMENU)ID_F_DISCOUNT,g_hInst,0);

        SendMessageW(t->cGender,CB_ADDSTRING,0,(LPARAM)L"مرد");
        SendMessageW(t->cGender,CB_ADDSTRING,0,(LPARAM)L"زن");
        SendMessageW(t->cGender,CB_SETCURSEL,0,0);
        SendMessageW(t->cPType,CB_ADDSTRING,0,(LPARAM)L"عادی");
        SendMessageW(t->cPType,CB_ADDSTRING,0,(LPARAM)L"سرپایی");
        SendMessageW(t->cPType,CB_ADDSTRING,0,(LPARAM)L"بستری");
        SendMessageW(t->cPType,CB_SETCURSEL,0,0);
        SendMessageW(t->cNType,CB_ADDSTRING,0,(LPARAM)L"عادی");
        SendMessageW(t->cNType,CB_ADDSTRING,0,(LPARAM)L"اورژانس");
        SendMessageW(t->cNType,CB_ADDSTRING,0,(LPARAM)L"پرسنلی");
        SendMessageW(t->cNType,CB_SETCURSEL,0,0);
        for(int i=0;i<N_INSURANCES;i++)
            SendMessageW(t->cIns,CB_ADDSTRING,0,(LPARAM)INSURANCES[i].name);
        SendMessageW(t->cIns,CB_SETCURSEL,0,0);
        for(int i=0;i<N_SUPP;i++)
            SendMessageW(t->cSupp,CB_ADDSTRING,0,(LPARAM)SUPP_INSURANCES[i].name);
        SendMessageW(t->cSupp,CB_SETCURSEL,0,0);

        t->bSubmit =createFlatButton(h,ID_F_SUBMIT,L"ثبت پذیرش و صدور قبض",ICO_SAVE,BS_PRIMARY,0,0,10,10);
        t->bPrtIns =createFlatButton(h,ID_F_PRT_INS,L"رسید بیمه",ICO_PRINT,BS_OUTLINE,0,0,10,10);
        t->bPrtRx  =createFlatButton(h,ID_F_PRT_RX,L"چاپ نسخه",ICO_PRINT,BS_OUTLINE,0,0,10,10);
        t->bPrtLast=createFlatButton(h,ID_F_PRT_LAST,L"چاپ آخرین قبض (F8)",ICO_PRINT,BS_OUTLINE,0,0,10,10);
        t->bClose  =createFlatButton(h,ID_F_CLOSE,L"خروج (بستن تب)",ICO_LOGOUT,BS_DANGER,0,0,10,10);

        HWND eds[12]={t->eFirst,t->eLast,t->eNid,t->eFather,t->eBirth,
                      t->eMobile,t->ePhone,t->eAddr,t->ePrice,t->eDiscount,0,0};
        for(int i=0;eds[i];i++){
            SendMessageW(eds[i],WM_SETFONT,(WPARAM)g_fUI,TRUE);
            enableEnterNavigation(eds[i]);
        }
        HWND cbsArr[5]={t->cGender,t->cPType,t->cNType,t->cIns,t->cSupp};
        for(int i=0;i<5;i++)
            SendMessageW(cbsArr[i],WM_SETFONT,(WPARAM)g_fUI,TRUE);
        return 0; }
    case WM_SIZE: if(t) tabPageLayout(h,t); return 0;
    case WM_CTLCOLOREDIT: {
        HDC dc=(HDC)w;
        SetTextColor(dc,g_theme.inputText); SetBkColor(dc,g_theme.inputBg);
        return (LRESULT)g_brInput; }
    case WM_CTLCOLORLISTBOX: {
        HDC dc=(HDC)w;
        SetTextColor(dc,g_theme.inputText); SetBkColor(dc,g_theme.inputBg);
        return (LRESULT)g_brInput; }
    case WM_COMMAND: {
        if(!t) return 0;
        int id=LOWORD(w), code=HIWORD(w);
        if((id==ID_F_INS||id==ID_F_SUPP) && code==CBN_SELCHANGE){
            recalc(t); InvalidateRect(h,NULL,FALSE);
        }
        else if((id==ID_F_PRICE||id==ID_F_DISCOUNT) && code==EN_CHANGE){
            recalc(t); InvalidateRect(h,NULL,FALSE);
        }
        else if(id==ID_F_SUBMIT){
            ReceptionRecord r; collect(t,r);
            if(r.firstName.empty()||r.lastName.empty()){
                t->lastMsg=L"نام و نام خانوادگی بیمار الزامی است.";
                t->msgCol=g_theme.danger;
            } else if(r.total<=0){
                t->lastMsg=L"مبلغ ویزیت/خدمت را وارد کنید.";
                t->msgCol=g_theme.danger;
            } else {
                int q=saveReception(r);
                wchar_t mb[160];
                swprintf(mb,160,L"پذیرش با شماره نوبت %d ثبت شد — %s %s",
                    q, r.firstName.c_str(), r.lastName.c_str());
                t->lastMsg=toFaDigits(mb); t->msgCol=g_theme.success;
                if(MessageBoxW(h,L"پذیرش ثبت شد. قبض چاپ شود؟",
                    L"ثبت موفق",MB_YESNO|MB_ICONQUESTION)==IDYES)
                    printReceipt(r,2,h);
                // reset patient fields for next reception
                HWND clr[8]={t->eFirst,t->eLast,t->eNid,t->eFather,
                             t->eBirth,t->eMobile,t->ePhone,t->eAddr};
                for(int i=0;i<8;i++) SetWindowTextW(clr[i],L"");
                SetWindowTextW(t->ePrice,L"");
                SetWindowTextW(t->eDiscount,L"");
                recalc(t);
                SetFocus(t->eFirst);
            }
            InvalidateRect(h,NULL,FALSE);
        }
        else if(id==ID_F_PRT_INS){
            ReceptionRecord r;
            if(loadLastReceipt(r)) printReceipt(r,0,h);
            else MessageBoxW(h,L"ابتدا یک پذیرش ثبت کنید.",L"رسید بیمه",MB_OK|MB_ICONINFORMATION);
        }
        else if(id==ID_F_PRT_RX){
            ReceptionRecord r;
            if(loadLastReceipt(r)) printReceipt(r,1,h);
            else MessageBoxW(h,L"ابتدا یک پذیرش ثبت کنید.",L"چاپ نسخه",MB_OK|MB_ICONINFORMATION);
        }
        else if(id==ID_F_PRT_LAST) printLastReceipt(h);
        else if(id==ID_F_CLOSE) closeTab(t);
        return 0; }
    case WM_KEYDOWN:
        if(w==VK_F8){ printLastReceipt(h); return 0; }
        break;
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps; HDC dc0=BeginPaint(h,&ps);
        RECT rc; GetClientRect(h,&rc);
        if(rc.right<=0 || rc.bottom<=0){ EndPaint(h,&ps); return 0; }
        HDC dc=CreateCompatibleDC(dc0);
        HBITMAP bmp=CreateCompatibleBitmap(dc0,rc.right,rc.bottom);
        HGDIOBJ obm=SelectObject(dc,bmp);
        FillRect(dc,&rc,g_brBg);
        SetBkMode(dc,TRANSPARENT);

        int pad=S(18);
        int bw,formLeft,formRight,fw,colW,xr,xl; bool stacked;
        rcMetrics(rc.right,pad,bw,formLeft,formRight,fw,colW,xr,xl,stacked);
        int y0,step,rh2; rcVMetrics(rc.bottom,y0,step,rh2);

        // field labels (above each control)
        SelectObject(dc,g_fSmall);
        SetTextColor(dc,g_theme.textDim);
        struct LBL{const wchar_t* s;int x,y,w;};
        LBL Ls[16]={
            {L"نام",xr,y0-S(24),colW},{L"نام خانوادگی",xl,y0-S(24),colW},
            {L"کد ملی",xr,y0+step-S(24),colW},{L"نام پدر",xl,y0+step-S(24),colW},
            {L"تاریخ تولد (مثلاً ۱۳۷۰/۰۱/۰۱)",xr,y0+2*step-S(24),colW},{L"جنسیت",xl,y0+2*step-S(24),colW},
            {L"تلفن همراه",xr,y0+3*step-S(24),colW},{L"تلفن ثابت",xl,y0+3*step-S(24),colW},
            {L"آدرس",formLeft,y0+4*step-S(24),fw},{NULL,0,0,0},
            {L"نوع بیمار",xr,y0+5*step-S(24),colW},{L"نوع نوبت",xl,y0+5*step-S(24),colW},
            {L"بیمه اصلی",xr,y0+6*step-S(24),colW},{L"بیمه مکمل",xl,y0+6*step-S(24),colW},
            {L"مبلغ خدمت (ریال)",xr,y0+7*step-S(24),colW},{L"تخفیف (ریال)",xl,y0+7*step-S(24),colW},
        };
        for(int i=0;i<16;i++){
            if(!Ls[i].s) continue;
            RECT lr={Ls[i].x,Ls[i].y,Ls[i].x+Ls[i].w,Ls[i].y+S(22)};
            DrawTextW(dc,Ls[i].s,-1,&lr,DT_RIGHT|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);
        }
        // auto date note
        SYSTEMTIME st=iranNow();
        std::wstring dn=L"تاریخ نوبت (خودکار): "+toFaDigits(jalaliDateShort(st))
            +L"   ساعت: "+toFaDigits(iranTimeStr(st,false))
            +L"   شیفت: "+shiftName(g_session.shift);
        SetTextColor(dc,g_theme.accent);
        RECT dnr={formLeft,y0+8*step+S(58),formRight,y0+8*step+S(84)};
        DrawTextW(dc,dn.c_str(),-1,&dnr,DT_RIGHT|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);
        // status message
        if(t && !t->lastMsg.empty()){
            SetTextColor(dc,t->msgCol);
            SelectObject(dc,g_fUIB);
            RECT mr2={formLeft,y0+8*step+S(86),formRight,y0+8*step+S(140)};
            DrawTextW(dc,t->lastMsg.c_str(),-1,&mr2,
                DT_RIGHT|DT_WORDBREAK|DT_RTLREADING|DT_NOPREFIX);
        }

        // ============ billing card (pinned to the right edge) ============
        if(!stacked && t){
            recalc(t);
            RECT card={rc.right-pad-bw,S(16),rc.right-pad,rc.bottom-S(16)};
            fillRoundRect(dc,card,S(14),g_theme.surface,g_theme.border);
            SetTextColor(dc,g_theme.text);
            SelectObject(dc,g_fUIB);
            RECT bt={card.left+S(12),card.top+S(10),card.right-S(12),card.top+S(38)};
            DrawTextW(dc,L"صدور قبض",-1,&bt,DT_RIGHT|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);

            struct ROW{const wchar_t* k; long long v; bool strong; bool sep;};
            ROW rows[11]={
                {L"بیمه اصلی (سهم بیمه)", t->mainShare, false,false},
                {L"جمع کل",               t->total,     false,false},
                {L"سهم بیمار",            t->patientShare,false,true},
                {L"بیمه مکمل — جمع کل",   t->total,     false,false},
                {L"مابه‌التفاوت پایه",     t->baseDiff,  false,false},
                {L"سهم سازمان",           t->orgShare,  false,true},
                {L"مبلغ نهایی",           0,            false,false},
                {L"جمع کل",               t->total,     false,false},
                {L"تخفیف",                t->patientShare - t->paid, false,false},
                {L"پرداختی",              t->paid,      true, false},
            };
            int ry=card.top+S(46);
            SelectObject(dc,g_fUI);
            for(int i=0;i<10;i++){
                if(i==6){ // section header
                    SetTextColor(dc,g_theme.accent);
                    SelectObject(dc,g_fUIB);
                    RECT hr={card.left+S(12),ry,card.right-S(12),ry+S(26)};
                    DrawTextW(dc,rows[i].k,-1,&hr,DT_RIGHT|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);
                    SelectObject(dc,g_fUI);
                    ry+=S(30); continue;
                }
                if(i==3){
                    SetTextColor(dc,g_theme.accent);
                    SelectObject(dc,g_fUIB);
                    RECT hr={card.left+S(12),ry,card.right-S(12),ry+S(26)};
                    DrawTextW(dc,L"بیمه مکمل",-1,&hr,DT_RIGHT|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);
                    SelectObject(dc,g_fUI);
                    ry+=S(30);
                }
                const wchar_t* key = (i==3)?L"جمع کل":rows[i].k;
                SetTextColor(dc, rows[i].strong?g_theme.success:g_theme.textDim);
                RECT kr={card.left+S(12),ry,card.right-S(12),ry+S(24)};
                DrawTextW(dc,key,-1,&kr,DT_RIGHT|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);
                SetTextColor(dc, rows[i].strong?g_theme.success:g_theme.text);
                SelectObject(dc, rows[i].strong?g_fUIB:g_fUI);
                std::wstring v=toFaDigits(formatMoney(rows[i].v))+L" ریال";
                DrawTextW(dc,v.c_str(),-1,&kr,DT_LEFT|DT_SINGLELINE|DT_NOPREFIX);
                SelectObject(dc,g_fUI);
                ry+=S(26);
                if(rows[i].sep){
                    HPEN pn=CreatePen(PS_DOT,1,g_theme.border);
                    HGDIOBJ op=SelectObject(dc,pn);
                    MoveToEx(dc,card.left+S(12),ry+S(2),0);
                    LineTo(dc,card.right-S(12),ry+S(2));
                    SelectObject(dc,op); DeleteObject(pn);
                    ry+=S(10);
                }
            }
        }
        BitBlt(dc0,0,0,rc.right,rc.bottom,dc,0,0,SRCCOPY);
        SelectObject(dc,obm); DeleteObject(bmp); DeleteDC(dc);
        EndPaint(h,&ps);
        return 0; }
    }
    return DefWindowProcW(h,m,w,l);
}

// =========================================================== DETACHED WIN ==
static LRESULT CALLBACK detachedProc(HWND h, UINT m, WPARAM w, LPARAM l){
    switch(m){
    case WM_SIZE: {
        HWND page=(HWND)GetWindowLongPtrW(h,GWLP_USERDATA);
        if(page) MoveWindow(page,0,0,LOWORD(l),HIWORD(l),TRUE);
        return 0; }
    case WM_CLOSE: {
        // re-attach page back into the tab strip (if reception still exists)
        HWND page=(HWND)GetWindowLongPtrW(h,GWLP_USERDATA);
        TabPage* t = page?(TabPage*)GetWindowLongPtrW(page,GWLP_USERDATA):NULL;
        HWND rec = FindWindowExW(g_hFrame,NULL,RC_CLASS,NULL);
        if(t && s_rd && rec && IsWindow(rec)){
            t->detached=false;
            SetWindowLongPtrW(h,GWLP_USERDATA,0);   // detach link BEFORE reparent
            SetParent(page, rec);
            RECT rc; GetClientRect(rec,&rc);
            MoveWindow(page,0,infoBarH()+tabBarH(),
                rc.right,rc.bottom-infoBarH()-tabBarH(),TRUE);
            for(size_t i=0;i<s_rd->tabs.size();i++)
                if(s_rd->tabs[i]==t) s_rd->active=(int)i;
            for(auto* tp : s_rd->tabs)
                ShowWindow(tp->page,
                    (tp->detached || tp==t) ? SW_SHOW : SW_HIDE);
            InvalidateRect(rec,NULL,TRUE);
        } else if(t && s_rd){
            // reception screen gone — remove tab entirely (page dies with us)
            SetWindowLongPtrW(h,GWLP_USERDATA,0);
            for(size_t i=0;i<s_rd->tabs.size();i++)
                if(s_rd->tabs[i]==t){
                    s_rd->tabs.erase(s_rd->tabs.begin()+i);
                    delete t;
                    if(s_rd->active>=(int)s_rd->tabs.size())
                        s_rd->active=(int)s_rd->tabs.size()-1;
                    break;
                }
        }
        DestroyWindow(h);
        return 0; }
    }
    return DefWindowProcW(h,m,w,l);
}
static void detachTab(TabPage* t){
    static bool reg=false;
    if(!reg){
        WNDCLASSW wc={0};
        wc.lpfnWndProc=detachedProc; wc.hInstance=g_hInst;
        wc.hCursor=LoadCursor(NULL,IDC_ARROW);
        wc.hbrBackground=NULL;
        wc.lpszClassName=DET_CLASS;
        RegisterClassW(&wc); reg=true;
    }
    int W=S(1040),H=S(700);
    RECT scr; SystemParametersInfoW(SPI_GETWORKAREA,0,&scr,0);
    HWND win=CreateWindowExW(0,DET_CLASS,
        (t->title+L" — "+APP_NAME_W).c_str(),
        WS_OVERLAPPEDWINDOW|WS_VISIBLE,
        (scr.right-W)/2,(scr.bottom-H)/2,W,H,NULL,NULL,g_hInst,NULL);
    SetWindowLongPtrW(win,GWLP_USERDATA,(LONG_PTR)t->page);
    t->detached=true;
    SetParent(t->page,win);
    RECT rc; GetClientRect(win,&rc);
    MoveWindow(t->page,0,0,rc.right,rc.bottom,TRUE);
    ShowWindow(t->page,SW_SHOW);
}

// ============================================================== TAB STRIP ==
static HWND recWnd(){ return s_rd?FindWindowExW(g_hFrame,NULL,RC_CLASS,NULL):NULL; }

static void recLayoutTabs(HWND h){
    RECT rc; GetClientRect(h,&rc);
    if(!s_rd) return;
    for(size_t i=0;i<s_rd->tabs.size();i++){
        TabPage* t=s_rd->tabs[i];
        if(t->detached) continue;
        if((int)i==s_rd->active){
            MoveWindow(t->page,0,infoBarH()+tabBarH(),
                rc.right,rc.bottom-infoBarH()-tabBarH(),TRUE);
            ShowWindow(t->page,SW_SHOW);
        } else ShowWindow(t->page,SW_HIDE);
    }
}
static void addTab(HWND h){
    if(!s_rd) return;
    static bool reg=false;
    if(!reg){
        WNDCLASSW wc={0};
        wc.lpfnWndProc=tabPageProc; wc.hInstance=g_hInst;
        wc.hCursor=LoadCursor(NULL,IDC_ARROW);
        wc.lpszClassName=TABPG_CLASS;
        RegisterClassW(&wc); reg=true;
    }
    TabPage* t=new TabPage();
    std::wstring dept=g_session.user.dept;
    t->title = dept.empty() ? L"پذیرش" : (L"پذیرش "+dept);
    RECT rc; GetClientRect(h,&rc);
    HWND pg=CreateWindowExW(0,TABPG_CLASS,L"",
        WS_CHILD|WS_CLIPCHILDREN,
        0,infoBarH()+tabBarH(),rc.right,rc.bottom-infoBarH()-tabBarH(),
        h,NULL,g_hInst,t);
    if(!pg){ delete t; return; }
    s_rd->tabs.push_back(t);
    s_rd->active=(int)s_rd->tabs.size()-1;
    recLayoutTabs(h);
    InvalidateRect(h,NULL,TRUE);
    SetFocus(t->eFirst);
}
static void closeTab(TabPage* t){
    if(!s_rd) return;
    HWND h=recWnd();
    for(size_t i=0;i<s_rd->tabs.size();i++){
        if(s_rd->tabs[i]==t){
            if(t->detached){
                HWND det=GetParent(t->page);
                DestroyWindow(t->page);
                SetWindowLongPtrW(det,GWLP_USERDATA,0);
                DestroyWindow(det);
            } else DestroyWindow(t->page);
            delete t;
            s_rd->tabs.erase(s_rd->tabs.begin()+i);
            if(s_rd->active>=(int)s_rd->tabs.size())
                s_rd->active=(int)s_rd->tabs.size()-1;
            break;
        }
    }
    if(h){ recLayoutTabs(h); InvalidateRect(h,NULL,TRUE); }
}

// tab geometry helpers — plain coords, tabs flow RIGHT → LEFT (RTL)
static RECT tabRect(HWND h, int i){
    RECT rc; GetClientRect(h,&rc);
    RECT r;
    r.right = rc.right - S(8) - i*(tabW()+S(6));
    r.left  = r.right - tabW();
    r.top   = infoBarH()+S(4);
    r.bottom= infoBarH()+tabBarH()-S(2);
    return r;
}
static int hitTab(HWND h, POINT pt, int* part){
    // part: 0=body 1=close 2=detach
    if(!s_rd) return -1;
    int vis=0;
    for(size_t i=0;i<s_rd->tabs.size();i++){
        RECT r=tabRect(h,(int)i);
        if(PtInRect(&r,pt)){
            RECT cl={r.left+S(6),r.top+S(7),r.left+S(26),r.bottom-S(7)};
            RECT dt={r.left+S(28),r.top+S(7),r.left+S(48),r.bottom-S(7)};
            if(PtInRect(&cl,pt)) *part=1;
            else if(PtInRect(&dt,pt)) *part=2;
            else *part=0;
            return (int)i;
        }
        vis++;
    }
    (void)vis;
    return -1;
}

// ------------------------------------------------------------- reception ---
static LRESULT CALLBACK recProc(HWND h, UINT m, WPARAM w, LPARAM l){
    switch(m){
    case WM_CREATE:
        s_rd = new RecData();
        s_rd->bCalc = createFlatButton(h,ID_RC_CALC,L"ماشین حساب",ICO_CALC,BS_OUTLINE,0,0,10,10);
        s_rd->bNewTab = createFlatButton(h,ID_RC_NEWTAB,L"پذیرش جدید",ICO_PLUS,BS_PRIMARY,0,0,10,10);
        return 0;
    case WM_NCDESTROY:
        if(s_rd){
            for(auto* t : s_rd->tabs){
                if(t->detached){
                    HWND det=GetParent(t->page);
                    DestroyWindow(t->page);
                    if(det){ SetWindowLongPtrW(det,GWLP_USERDATA,0); DestroyWindow(det); }
                }
                delete t;
            }
            delete s_rd; s_rd=NULL;
        }
        break;
    case WM_SIZE: {
        if(!s_rd) return 0;
        int bh=S(38), y=(infoBarH()-bh)/2;
        // action buttons on the LEFT side of the info bar (RTL UI)
        MoveWindow(s_rd->bCalc,   S(8),   y, S(150), bh, TRUE);
        MoveWindow(s_rd->bNewTab, S(166), y, S(150), bh, TRUE);
        recLayoutTabs(h);
        return 0; }
    case WM_COMMAND: {
        int id=LOWORD(w);
        if(id==ID_RC_CALC) openCalculator(g_hFrame);
        else if(id==ID_RC_NEWTAB) addTab(h);
        return 0; }
    case WM_MOUSEMOVE: {
        if(!s_rd) return 0;
        POINT pt={GET_X_LPARAM(l),GET_Y_LPARAM(l)};
        int part=0, hit=hitTab(h,pt,&part);
        int hc = (part==1)?hit:-1, hd=(part==2)?hit:-1;
        if(hit!=s_rd->hotTab || hc!=s_rd->hotClose || hd!=s_rd->hotDetach){
            s_rd->hotTab=hit; s_rd->hotClose=hc; s_rd->hotDetach=hd;
            RECT bar={0,infoBarH(),S(2000),infoBarH()+tabBarH()};
            InvalidateRect(h,&bar,FALSE);
        }
        TRACKMOUSEEVENT t={sizeof(t),TME_LEAVE,h,0}; TrackMouseEvent(&t);
        return 0; }
    case WM_MOUSELEAVE:
        if(s_rd && s_rd->hotTab!=-1){
            s_rd->hotTab=s_rd->hotClose=s_rd->hotDetach=-1;
            RECT bar={0,infoBarH(),S(2000),infoBarH()+tabBarH()};
            InvalidateRect(h,&bar,FALSE);
        }
        return 0;
    case WM_LBUTTONDOWN: {
        if(!s_rd) return 0;
        POINT pt={GET_X_LPARAM(l),GET_Y_LPARAM(l)};
        int part=0, hit=hitTab(h,pt,&part);
        if(hit>=0 && hit<(int)s_rd->tabs.size()){
            TabPage* t=s_rd->tabs[hit];
            if(part==1) closeTab(t);
            else if(part==2 && !t->detached) detachTab(t);
            else if(!t->detached){
                s_rd->active=hit; recLayoutTabs(h);
                RECT bar={0,infoBarH(),S(2000),infoBarH()+tabBarH()};
                InvalidateRect(h,&bar,FALSE);
            } else {
                // focus the detached window
                HWND det=GetParent(t->page);
                if(det) SetForegroundWindow(det);
            }
        }
        return 0; }
    case WM_KEYDOWN:
        if(w==VK_F8){ printLastReceipt(h); return 0; }
        break;
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps; HDC dc0=BeginPaint(h,&ps);
        RECT rc; GetClientRect(h,&rc);
        HDC dc=CreateCompatibleDC(dc0);
        HBITMAP bmp=CreateCompatibleBitmap(dc0,rc.right,rc.bottom);
        HGDIOBJ obm=SelectObject(dc,bmp);

        // info bar
        RECT ib={0,0,rc.right,infoBarH()};
        FillRect(dc,&ib,g_brSurface2);
        // tab bar
        RECT tb2={0,infoBarH(),rc.right,infoBarH()+tabBarH()};
        FillRect(dc,&tb2,g_brSurface);
        // body
        RECT bd={0,infoBarH()+tabBarH(),rc.right,rc.bottom};
        FillRect(dc,&bd,g_brBg);
        HPEN pen=CreatePen(PS_SOLID,1,g_theme.border);
        HGDIOBJ op=SelectObject(dc,pen);
        MoveToEx(dc,0,infoBarH()-1,0); LineTo(dc,rc.right,infoBarH()-1);
        MoveToEx(dc,0,infoBarH()+tabBarH()-1,0); LineTo(dc,rc.right,infoBarH()+tabBarH()-1);
        SelectObject(dc,op); DeleteObject(pen);

        SetBkMode(dc,TRANSPARENT);
        // ---- info bar texts (anchored to the RIGHT edge, RTL) ----
        SYSTEMTIME st=iranNow();
        SelectObject(dc,g_fUIB);
        SetTextColor(dc,g_theme.text);
        std::wstring info =
            L"کاربر جاری: " + g_session.user.username;
        RECT ir={rc.right-S(420),0,rc.right-S(12),infoBarH()};
        DrawTextW(dc,info.c_str(),-1,&ir,
            DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
        SelectObject(dc,g_fUI);
        SetTextColor(dc,g_theme.textDim);
        std::wstring info2 =
            L"نوع دسترسی: پذیرش   |   " + toFaDigits(jalaliDateShort(st)) +
            L"  " + toFaDigits(iranTimeStr(st,false));
        RECT ir2={rc.right-S(900),0,rc.right-S(430),infoBarH()};
        DrawTextW(dc,info2.c_str(),-1,&ir2,
            DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);

        // ---- tabs ----
        if(s_rd){
            for(size_t i=0;i<s_rd->tabs.size();i++){
                TabPage* t=s_rd->tabs[i];
                RECT r=tabRect(h,(int)i);
                bool act = ((int)i==s_rd->active) && !t->detached;
                bool hov = ((int)i==s_rd->hotTab);
                COLORREF fill = act?g_theme.bg : hov?g_theme.hover:g_theme.surface;
                fillRoundRect(dc,r,S(9),fill, act?g_theme.accent:g_theme.border);
                // title
                SetTextColor(dc, t->detached?g_theme.textDim:
                                 act?g_theme.text:g_theme.textDim);
                SelectObject(dc, act?g_fUIB:g_fUI);
                RECT tr2={r.left+S(52),r.top,r.right-S(10),r.bottom};
                std::wstring shown=t->title + (t->detached?L" (جدا شده)":L"");
                DrawTextW(dc,shown.c_str(),-1,&tr2,
                    DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX|DT_END_ELLIPSIS);
                // close ×
                RECT cl={r.left+S(6),r.top+S(7),r.left+S(26),r.bottom-S(7)};
                if((int)i==s_rd->hotClose)
                    fillRoundRect(dc,cl,S(6),g_theme.danger,CLR_INVALID);
                RECT cli={cl.left+S(5),cl.top+S(5),cl.right-S(5),cl.bottom-S(5)};
                drawIcon(dc,ICO_X,cli,
                    (int)i==s_rd->hotClose?RGB(255,255,255):g_theme.textDim,S(2));
                // detach ⧉
                if(!t->detached){
                    RECT dt={r.left+S(28),r.top+S(7),r.left+S(48),r.bottom-S(7)};
                    if((int)i==s_rd->hotDetach)
                        fillRoundRect(dc,dt,S(6),g_theme.accent,CLR_INVALID);
                    RECT dti={dt.left+S(4),dt.top+S(4),dt.right-S(4),dt.bottom-S(4)};
                    drawIcon(dc,ICO_DETACH,dti,
                        (int)i==s_rd->hotDetach?RGB(255,255,255):g_theme.textDim,S(2));
                }
            }
            if(s_rd->tabs.empty()){
                SetTextColor(dc,g_theme.textDim);
                SelectObject(dc,g_fUI);
                RECT er={0,infoBarH()+tabBarH(),rc.right,rc.bottom};
                DrawTextW(dc,
                    L"برای شروع، روی دکمه «پذیرش جدید» کلیک کنید",
                    -1,&er,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);
            }
        }
        BitBlt(dc0,0,0,rc.right,rc.bottom,dc,0,0,SRCCOPY);
        SelectObject(dc,obm); DeleteObject(bmp); DeleteDC(dc);
        EndPaint(h,&ps);
        return 0; }
    }
    return DefWindowProcW(h,m,w,l);
}
HWND createReceptionScreen(HWND frame){
    static bool reg=false;
    if(!reg){
        WNDCLASSW wc={0};
        wc.lpfnWndProc=recProc; wc.hInstance=g_hInst;
        wc.hCursor=LoadCursor(NULL,IDC_ARROW);
        wc.lpszClassName=RC_CLASS;
        RegisterClassW(&wc); reg=true;
    }
    RECT rc=frameContentRect();
    HWND h=CreateWindowExW(0,RC_CLASS,L"",
        WS_CHILD|WS_VISIBLE|WS_CLIPCHILDREN,
        rc.left,rc.top,rc.right-rc.left,rc.bottom-rc.top,frame,NULL,g_hInst,NULL);
    // open first tab automatically
    addTab(h);
    return h;
}
