// ============================================================================
//  appointment.cpp — نوبت‌دهی (Appointment) module (v1.6.0)
//   • RIGHT  : «جستجوی بیمار رزرو کرده» + «مشخصات نوبت» + «مشخصات بیمار»
//   • LEFT   : a read-only DataGridView of the appointments (RTL columns)
//   • The patient block is DISABLED until a citizen is found; entering a valid
//     national-id + Enter auto-fills it from the (offline) Civil-Registry.
//   • Reuses the themed owner-draw combobox, the flat buttons and the smart
//     Jalali date mask so it matches the rest of the program exactly.
// ============================================================================
#include "app.h"
#include <commctrl.h>
#include <stdio.h>

#define AP_CLASS L"AzAppointment"

// ---- control ids -----------------------------------------------------------
enum {
    // search group
    AP_S_NID = 1400, AP_S_MOBILE, AP_S_FIRST, AP_S_LAST, AP_S_CANCELLED, AP_S_GO,
    // appointment-details group
    AP_A_DOCTOR, AP_A_DOC_REFRESH, AP_A_DOC_TODAY,
    AP_A_SERVICE, AP_A_SVC_F5, AP_A_SVC_F4, AP_A_SVC_F3,
    AP_A_DATE, AP_A_NEXT, AP_A_KIND,
    // patient-details group
    AP_P_NID, AP_P_FIRST, AP_P_FATHER, AP_P_MOBILE, AP_P_LAST, AP_P_GENDER,
    AP_P_DESC, AP_P_FOREIGN, AP_P_RESERVE, AP_P_NEW, AP_P_CANCEL,
    // grid toolbar
    AP_T_MSG, AP_T_TRANSFER, AP_T_PRINT, AP_T_SAVELAYOUT, AP_T_DELLAYOUT
};

// ---- grid columns (RIGHT → LEFT) -------------------------------------------
struct GCol { const wchar_t* title; int w; };   // w is a relative weight
static const GCol GCOLS[] = {
    { L"ابطال",          58 },
    { L"ویرایش",         58 },
    { L"چاپ",            48 },
    { L"کاربر",          90 },
    { L"مو...",          90 },   // موبایل
    { L"پزشک",          130 },
    { L"نام خانوادگی",  120 },
    { L"نام",            90 },
    { L"ش نوبت",         60 },
    { L"ساعت نوبت",      78 },
    { L"روز",            74 },
    { L"تاریخ نوبت",     96 },
    { L"نوع",            72 },
};
static const int N_GCOLS = (int)(sizeof(GCOLS)/sizeof(GCOLS[0]));

// ---- per-page state --------------------------------------------------------
struct ApptUI {
    HWND page;
    // search
    HWND sNid, sMobile, sFirst, sLast, sCancelled, sGo;
    // appointment details
    HWND aDoctor, aDocRefresh, aDocToday;
    HWND aService, aSvcF5, aSvcF4, aSvcF3;
    HWND aDate, aNext, aKind;
    // patient details
    HWND pNid, pFirst, pFather, pMobile, pLast, pGender, pDesc;
    HWND pForeign, pReserve, pNew, pCancel;
    // grid toolbar
    HWND tMsg, tTransfer, tPrint, tSaveLayout, tDelLayout;
    // data
    std::vector<Appointment> rows;   // shown rows (newest first)
    std::vector<int>         srcIndex;   // map row → loadAppointments(true) index
    int  sel, hot;                   // selected / hovered grid row
    int  scroll;                     // first visible grid row
    bool patientOn;                  // patient block enabled?
    bool foreign;                    // foreign-national / newborn
    bool kindInPerson;               // true=حضوری false=غیرحضوری
    int  editSrc;                    // source index being edited (-1 = new)
    bool idChecked;                  // an inquiry was performed
    bool idVerified;                 // a trusted source verified the identity
    std::wstring statusMsg; COLORREF statusCol;
    ApptUI():page(0),sel(-1),hot(-1),scroll(0),patientOn(false),foreign(false),
        kindInPerson(true),editSrc(-1),idChecked(false),idVerified(false),
        statusCol(0){}
};

// ============================================================== metrics =====
static int apRowH()    { return S(58); }  // v1.9.4: must hold input(28) + the
                                          // well's 4px bottom bleed + air + the
                                          // NEXT row's label(16) + label gap, so
                                          // no label is ever overlapped by the
                                          // control above it.
static int apHdrH()    { return S(34); }
// v1.9.4 — vertical rhythm helpers so a group's first field always clears the
// group-title band and every label sits fully ABOVE (never under) its control.
// The input "well" drawn in apPaint extends S(4) above the input top, so the
// label-top must be >= 16(label height) + 4(well) + a little air above the input.
static int apLblGap()  { return S(22); }  // distance from a row's label-top to its input-top
static int apGrpPad()  { return S(10); }  // breathing room below the group title before row 1

// ---------------------------------------------------------------- doctors ---
static void fillDoctors(ApptUI* u, bool todayOnly){
    SendMessageW(u->aDoctor, CB_RESETCONTENT, 0, 0);
    std::vector<DoctorDef> ds = todayOnly ? todaysDoctors() : loadDoctors();
    for(auto& d : ds){
        std::wstring s = d.name + L" — " + d.specialty;
        SendMessageW(u->aDoctor, CB_ADDSTRING, 0, (LPARAM)s.c_str());
    }
    if(!ds.empty()) SendMessageW(u->aDoctor, CB_SETCURSEL, 0, 0);
    // refresh the service list for the first doctor
    SendMessageW(u->aService, CB_RESETCONTENT, 0, 0);
    if(!ds.empty()){
        for(auto& sv : ds[0].services)
            SendMessageW(u->aService, CB_ADDSTRING, 0, (LPARAM)sv.c_str());
        if(!ds[0].services.empty()) SendMessageW(u->aService, CB_SETCURSEL, 0, 0);
    }
}
static void onDoctorChanged(ApptUI* u){
    int idx = (int)SendMessageW(u->aDoctor, CB_GETCURSEL, 0, 0);
    if(idx < 0) return;
    auto ds = loadDoctors();
    // match by the displayed "name — specialty" text
    wchar_t buf[256]; GetWindowTextW(u->aDoctor, buf, 256);
    std::wstring shown = buf;
    SendMessageW(u->aService, CB_RESETCONTENT, 0, 0);
    for(auto& d : ds){
        std::wstring s = d.name + L" — " + d.specialty;
        if(s == shown){
            for(auto& sv : d.services)
                SendMessageW(u->aService, CB_ADDSTRING, 0, (LPARAM)sv.c_str());
            if(!d.services.empty()) SendMessageW(u->aService, CB_SETCURSEL, 0, 0);
            break;
        }
    }
}

// ---------------------------------------------------------------- grid ------
static void refreshGrid(ApptUI* u){
    bool inclCancelled =
        (SendMessageW(u->sCancelled, BM_GETCHECK, 0, 0) == BST_CHECKED);
    std::vector<Appointment> all = loadAppointments(true);
    u->rows.clear(); u->srcIndex.clear();
    // newest first
    for(int i=(int)all.size()-1; i>=0; --i){
        if(all[i].cancelled && !inclCancelled) continue;
        u->rows.push_back(all[i]);
        u->srcIndex.push_back(i);
    }
    if(u->sel >= (int)u->rows.size()) u->sel = -1;
    if(u->page) InvalidateRect(u->page, NULL, FALSE);
}

// ---------------------------------------------------- patient block on/off --
static void setPatientEnabled(ApptUI* u, bool on){
    u->patientOn = on;
    HWND ctls[] = { u->pNid,u->pFirst,u->pFather,u->pMobile,u->pLast,
                    u->pGender,u->pDesc,u->pForeign,u->pReserve };
    for(HWND c : ctls) if(c) EnableWindow(c, on);
    // the «جدید»/«انصراف» buttons stay enabled so the user can reset/clear.
}
static void resetPatient(ApptUI* u){
    SetWindowTextW(u->pNid,   L"");
    SetWindowTextW(u->pFirst, L"");
    SetWindowTextW(u->pFather, L"");
    SetWindowTextW(u->pMobile, L"");
    SetWindowTextW(u->pLast,  L"");
    SetWindowTextW(u->pDesc,  L"");
    SendMessageW(u->pGender, CB_SETCURSEL, 0, 0);
    SendMessageW(u->pForeign, BM_SETCHECK, BST_UNCHECKED, 0);
    u->foreign = false;
    u->idChecked = false; u->idVerified = false;
    setPatientEnabled(u, false);
}

// fill the patient block from a national-id (Civil-Registry simulation)
static void lookupPatient(ApptUI* u){
    wchar_t buf[32]; GetWindowTextW(u->pNid, buf, 32);
    std::wstring nid = trim(buf);
    bool foreign = (SendMessageW(u->pForeign, BM_GETCHECK, 0, 0) == BST_CHECKED);
    u->foreign = foreign;
    u->idChecked = true; u->idVerified = false;
    if(foreign){
        // foreign nationals / newborns are NOT in the registry — just enable the
        // fields so the operator can type the identity manually.
        setPatientEnabled(u, true);
        u->idChecked = false;   // manual entry is expected, not a failed verify
        u->statusMsg = L"تابعیت غیرایرانی/نوزاد — مشخصات را دستی وارد کنید.";
        u->statusCol = g_theme.warn;
        SetFocus(u->pFirst);
        InvalidateRect(u->page, NULL, FALSE);
        return;
    }
    if(!validNationalId(nid)){
        setPatientEnabled(u, true);   // allow manual entry, no guessing
        u->statusMsg = L"کد ملی نامعتبر است؛ مشخصات را دستی وارد کنید (قاب قرمز).";
        u->statusCol = g_theme.danger;
        SetFocus(u->pFirst);
        InvalidateRect(u->page, NULL, FALSE);
        return;
    }
    CitizenInfo c = lookupCitizen(nid);
    if(!c.found){
        // No trusted source verified it → manual entry, never fabricate.
        setPatientEnabled(u, true);
        if(c.lookupFailed)
            u->statusMsg = L"استعلام برخط ناموفق بود؛ مشخصات را دستی وارد کنید.";
        else
            u->statusMsg = L"هویت تأیید نشد؛ نام و نام خانوادگی را دستی وارد کنید (قاب قرمز).";
        u->statusCol = g_theme.danger;
        SetFocus(u->pFirst);
        InvalidateRect(u->page, NULL, FALSE);
        return;
    }
    setPatientEnabled(u, true);
    u->idVerified = true;
    if(!c.firstName.empty())  SetWindowTextW(u->pFirst,  c.firstName.c_str());
    if(!c.lastName.empty())   SetWindowTextW(u->pLast,   c.lastName.c_str());
    if(!c.fatherName.empty()) SetWindowTextW(u->pFather, c.fatherName.c_str());
    if(!c.mobile.empty())     SetWindowTextW(u->pMobile, c.mobile.c_str());
    if(c.gender==L"زن")  SendMessageW(u->pGender, CB_SETCURSEL, 1, 0);
    else if(c.gender==L"مرد") SendMessageW(u->pGender, CB_SETCURSEL, 0, 0);
    u->statusMsg = (c.source==CS_LOCAL)
        ? L"مشخصات بیمار از سوابق همین درمانگاه بازیابی شد."
        : L"مشخصات بیمار از سامانهٔ معتبر دریافت شد.";
    u->statusCol = g_theme.success;
    InvalidateRect(u->page, NULL, FALSE);
}

// register the appointment (رزرو عادی نوبت)
static void submitAppointment(ApptUI* u){
    if(!u->patientOn){
        u->statusMsg = L"ابتدا بیمار را با کد ملی پیدا کنید.";
        u->statusCol = g_theme.danger;
        InvalidateRect(u->page, NULL, FALSE); return;
    }
    Appointment a;
    wchar_t b[256];
    GetWindowTextW(u->pNid,   b,256); a.nationalId = trim(b);
    GetWindowTextW(u->pFirst, b,256); a.firstName  = trim(b);
    GetWindowTextW(u->pLast,  b,256); a.lastName   = trim(b);
    GetWindowTextW(u->pMobile,b,256); a.mobile     = trim(b);
    GetWindowTextW(u->aDoctor,b,256); a.doctor     = trim(b);
    GetWindowTextW(u->aService,b,256);a.service    = trim(b);
    GetWindowTextW(u->aDate,  b,256); a.apptDate   = trim(b);
    if(a.apptDate.empty()) a.apptDate = jalaliDateShort(iranNow());
    a.kind  = u->kindInPerson ? L"حضوری" : L"غیرحضوری";
    a.user  = g_session.user.fullname.empty()?g_session.user.username
                                             :g_session.user.fullname;
    if(a.firstName.empty() || a.lastName.empty()){
        u->statusMsg = L"نام و نام خانوادگی بیمار الزامی است.";
        u->statusCol = g_theme.danger;
        InvalidateRect(u->page, NULL, FALSE); return;
    }
    if(u->editSrc >= 0){
        // editing an existing appointment — preserve its slot, update fields
        std::vector<Appointment> all = loadAppointments(true);
        if(u->editSrc < (int)all.size()){
            a.apptTime = all[u->editSrc].apptTime;
            a.day      = all[u->editSrc].day;
            a.shift    = all[u->editSrc].shift;
            a.cancelled= all[u->editSrc].cancelled;
        }
        updateAppointment(u->editSrc, a);
        u->statusMsg = L"تغییرات نوبت ذخیره شد."; u->statusCol = g_theme.success;
        u->editSrc = -1;
        resetPatient(u);
        refreshGrid(u);
        return;
    }
    int q = saveAppointment(a);   // assigns queue no / time / day
    // v1.7.0: remember the REAL identity the operator confirmed for next-time
    // recall — never fabricated, only what was actually entered/verified here.
    {
        int gi=(int)SendMessageW(u->pGender,CB_GETCURSEL,0,0);
        std::wstring gender = gi==1?L"زن":L"مرد";
        wchar_t fb[128]; GetWindowTextW(u->pFather,fb,128);
        rememberPatient(a.nationalId,a.firstName,a.lastName,trim(fb),
            gender,L"",a.mobile,std::vector<int>());
    }
    wchar_t mb[200];
    swprintf(mb,200,L"نوبت با شماره %d برای %s %s ثبت شد.",
        q, a.firstName.c_str(), a.lastName.c_str());
    u->statusMsg = toFaDigits(mb); u->statusCol = g_theme.success;
    resetPatient(u);
    refreshGrid(u);
}

// ---- load an existing row into the form for editing ------------------------
static void loadForEdit(ApptUI* u, int rowIdx){
    if(rowIdx<0 || rowIdx>=(int)u->rows.size()) return;
    const Appointment& a = u->rows[rowIdx];
    u->editSrc = (rowIdx<(int)u->srcIndex.size())?u->srcIndex[rowIdx]:-1;
    setPatientEnabled(u, true);
    SetWindowTextW(u->pNid,   a.nationalId.c_str());
    SetWindowTextW(u->pFirst, a.firstName.c_str());
    SetWindowTextW(u->pLast,  a.lastName.c_str());
    SetWindowTextW(u->pMobile,a.mobile.c_str());
    SetWindowTextW(u->aDate,  a.apptDate.c_str());
    // pick the doctor in the combo if present
    int n=(int)SendMessageW(u->aDoctor,CB_GETCOUNT,0,0);
    for(int i=0;i<n;i++){
        wchar_t b[256]; SendMessageW(u->aDoctor,CB_GETLBTEXT,i,(LPARAM)b);
        if(a.doctor==b || std::wstring(b).find(a.doctor)!=std::wstring::npos){
            SendMessageW(u->aDoctor,CB_SETCURSEL,i,0); onDoctorChanged(u); break;
        }
    }
    u->kindInPerson = (a.kind!=L"غیرحضوری");
    u->statusMsg = L"حالت ویرایش — تغییرات را اعمال و «رزرو/ذخیره» را بزنید.";
    u->statusCol = g_theme.warn;
    SetFocus(u->pNid);
    InvalidateRect(u->page,NULL,FALSE);
}

// ---- transfer (reschedule) the selected appointment to a new date ----------
static void transferAppointment(ApptUI* u, HWND h){
    if(u->sel<0 || u->sel>=(int)u->rows.size()) return;
    int src = (u->sel<(int)u->srcIndex.size())?u->srcIndex[u->sel]:-1;
    if(src<0){
        MessageBoxW(h,L"این نوبت قابل انتقال نیست.",L"انتقال نوبت",
            MB_OK|MB_ICONWARNING); return;
    }
    // ask for a new date: take it from the «تاریخ نوبت» field; if empty use today
    wchar_t db[64]; GetWindowTextW(u->aDate,db,64);
    std::wstring nd = trim(db);
    if(nd.empty()) nd = jalaliDateShort(iranNow());
    std::wstring msg = L"نوبت «"+u->rows[u->sel].firstName+L" "+
        u->rows[u->sel].lastName+L"» به تاریخ "+toFaDigits(nd)+L" منتقل شود؟";
    if(MessageBoxW(h,msg.c_str(),L"انتقال نوبت",MB_YESNO|MB_ICONQUESTION)!=IDYES)
        return;
    std::vector<Appointment> all = loadAppointments(true);
    if(src<(int)all.size()){
        Appointment a=all[src];
        a.apptDate = nd;
        // recompute weekday note: keep existing day label (date driven manually)
        updateAppointment(src,a);
        // notify the patient via cartable
        pushMessageT(g_session.user.username,a.nationalId,
            L"نوبت شما به تاریخ "+toFaDigits(nd)+L" منتقل شد.",KMSG_NORMAL);
        u->statusMsg=L"نوبت با موفقیت منتقل شد."; u->statusCol=g_theme.success;
        refreshGrid(u);
    }
}

// ---- tiny modal text prompt (themed) ---------------------------------------
struct PromptState { std::wstring text; bool ok; HWND edit; };
static LRESULT CALLBACK promptProc(HWND h,UINT m,WPARAM w,LPARAM l){
    PromptState* p=(PromptState*)GetWindowLongPtrW(h,GWLP_USERDATA);
    switch(m){
    case WM_CREATE: {
        CREATESTRUCTW* cs=(CREATESTRUCTW*)l;
        SetWindowLongPtrW(h,GWLP_USERDATA,(LONG_PTR)cs->lpCreateParams);
        return 0; }
    case WM_COMMAND: {
        int id=LOWORD(w);
        if(id==IDOK){
            wchar_t b[256]; GetWindowTextW(p->edit,b,256);
            p->text=trim(b); p->ok=true; DestroyWindow(h);
        } else if(id==IDCANCEL){ p->ok=false; DestroyWindow(h); }
        return 0; }
    case WM_CTLCOLOREDIT: {
        HDC dc=(HDC)w; SetTextColor(dc,g_theme.inputText);
        SetBkColor(dc,g_theme.inputBg); return (LRESULT)g_brInput; }
    case WM_CTLCOLORSTATIC: {
        HDC dc=(HDC)w; SetTextColor(dc,g_theme.text);
        SetBkColor(dc,g_theme.surface); return (LRESULT)g_brSurface; }
    case WM_CLOSE: if(p){p->ok=false;} DestroyWindow(h); return 0;
    }
    return DefWindowProcW(h,m,w,l);
}
static bool apPrompt(HWND owner,const wchar_t* title,const wchar_t* label,
                     std::wstring& out){
    static bool reg=false;
    if(!reg){ WNDCLASSW wc={0}; wc.lpfnWndProc=promptProc; wc.hInstance=g_hInst;
        wc.hCursor=LoadCursor(NULL,IDC_ARROW);
        wc.hbrBackground=g_brSurface; wc.lpszClassName=L"AzApPrompt";
        RegisterClassW(&wc); reg=true; }
    PromptState ps; ps.ok=false;
    RECT rc; GetWindowRect(owner,&rc);
    int W=S(380),H=S(180);
    int x=rc.left+((rc.right-rc.left)-W)/2, y=rc.top+((rc.bottom-rc.top)-H)/2;
    HWND dlg=CreateWindowExW(WS_EX_DLGMODALFRAME|WS_EX_TOPMOST,L"AzApPrompt",title,
        WS_POPUP|WS_CAPTION|WS_SYSMENU,x,y,W,H,owner,NULL,g_hInst,&ps);
    if(!dlg) return false;
    CreateWindowW(L"STATIC",label,WS_CHILD|WS_VISIBLE|SS_RIGHT,
        S(14),S(14),W-S(28),S(22),dlg,NULL,g_hInst,NULL);
    ps.edit=CreateWindowExW(0,L"EDIT",L"",
        WS_CHILD|WS_VISIBLE|ES_AUTOHSCROLL|ES_RIGHT,
        S(14),S(42),W-S(28),S(28),dlg,NULL,g_hInst,NULL);
    SendMessageW(ps.edit,WM_SETFONT,(WPARAM)g_fUI,TRUE);
    HWND ok=createFlatButton(dlg,IDOK,L"تأیید",ICO_NONE,BS_PRIMARY,
        W-S(200),S(86),S(86),S(34));
    HWND ca=createFlatButton(dlg,IDCANCEL,L"انصراف",ICO_NONE,BS_OUTLINE,
        W-S(104),S(86),S(86),S(34));
    (void)ok;(void)ca;
    EnableWindow(owner,FALSE);
    ShowWindow(dlg,SW_SHOW); SetFocus(ps.edit);
    MSG msg;
    while(IsWindow(dlg) && GetMessageW(&msg,NULL,0,0)){
        if(msg.message==WM_KEYDOWN && msg.hwnd && (msg.hwnd==ps.edit||GetParent(msg.hwnd)==dlg)){
            if(msg.wParam==VK_RETURN){ SendMessageW(dlg,WM_COMMAND,IDOK,0); continue; }
            if(msg.wParam==VK_ESCAPE){ SendMessageW(dlg,WM_COMMAND,IDCANCEL,0); continue; }
        }
        if(!IsDialogMessageW(dlg,&msg)){
            TranslateMessage(&msg); DispatchMessageW(&msg);
        }
    }
    EnableWindow(owner,TRUE); SetForegroundWindow(owner);
    if(ps.ok) out=ps.text;
    return ps.ok;
}

// ---- F3/F4/F5 service helpers ----------------------------------------------
static void svcAddCustom(ApptUI* u, HWND h){    // F4 — افزودن خدمت
    std::wstring s;
    if(apPrompt(h,L"افزودن خدمت",L"نام خدمت جدید:",s) && !s.empty()){
        int idx=(int)SendMessageW(u->aService,CB_ADDSTRING,0,(LPARAM)s.c_str());
        SendMessageW(u->aService,CB_SETCURSEL,idx,0);
        u->statusMsg=L"خدمت افزوده شد."; u->statusCol=g_theme.success;
        InvalidateRect(u->page,NULL,FALSE);
    }
}
static void svcRefresh(ApptUI* u){              // F5 — بازخوانی خدمات پزشک
    onDoctorChanged(u);
    u->statusMsg=L"فهرست خدمات پزشک بازخوانی شد."; u->statusCol=g_theme.accent;
    InvalidateRect(u->page,NULL,FALSE);
}
static void svcClear(ApptUI* u){                // F3 — پاک‌سازی انتخاب خدمت
    SendMessageW(u->aService,CB_SETCURSEL,(WPARAM)-1,0);
    SetWindowTextW(u->aService,L"");
    u->statusMsg=L"انتخاب خدمت پاک شد."; u->statusCol=g_theme.textDim;
    InvalidateRect(u->page,NULL,FALSE);
}

// ---------------------------------------------------------------- search ----
static void doSearch(ApptUI* u){
    wchar_t b[128];
    std::wstring nid,mob,fn,ln;
    GetWindowTextW(u->sNid,   b,128); nid=trim(b);
    GetWindowTextW(u->sMobile,b,128); mob=trim(b);
    GetWindowTextW(u->sFirst, b,128); fn =trim(b);
    GetWindowTextW(u->sLast,  b,128); ln =trim(b);
    bool inclCancelled =
        (SendMessageW(u->sCancelled, BM_GETCHECK, 0, 0) == BST_CHECKED);
    if(nid.empty()&&mob.empty()&&fn.empty()&&ln.empty()){
        refreshGrid(u);   // nothing typed → show everything
        return;
    }
    std::vector<Appointment> res =
        searchAppointments(nid,mob,fn,ln,inclCancelled);
    // map results back to source indices so ابطال still works
    std::vector<Appointment> all = loadAppointments(true);
    u->rows.clear(); u->srcIndex.clear();
    for(auto& r : res){
        u->rows.push_back(r);
        int found=-1;
        for(int i=(int)all.size()-1;i>=0;--i){
            if(all[i].nationalId==r.nationalId && all[i].queueNo==r.queueNo
               && all[i].apptDate==r.apptDate){ found=i; break; }
        }
        u->srcIndex.push_back(found);
    }
    u->sel=-1;
    wchar_t mb[80]; swprintf(mb,80,L"%d نتیجه یافت شد.",(int)res.size());
    u->statusMsg=toFaDigits(mb); u->statusCol=g_theme.accent;
    InvalidateRect(u->page,NULL,FALSE);
}

// ---------------------------------------------------- print one slip --------
//  Real GDI print of an appointment slip on the default (or chosen) printer.
static bool printApptSlip(HWND owner, const Appointment& a){
    // Use the configured reception printer if set, otherwise the system default.
    std::wstring printer = getSetting(L"printer_name",L"");
    HDC dc=NULL;
    if(!printer.empty())
        dc=CreateDCW(L"WINSPOOL",printer.c_str(),NULL,NULL);
    if(!dc){
        PRINTDLGW pd={sizeof(pd)};
        pd.hwndOwner=owner; pd.Flags=PD_RETURNDC|PD_NOPAGENUMS|PD_NOSELECTION;
        if(!PrintDlgW(&pd) || !pd.hDC){
            return false;
        }
        dc=pd.hDC;
    }
    DOCINFOW di={sizeof(di)}; di.lpszDocName=L"آزادی طب — قبض نوبت";
    if(StartDocW(dc,&di)<=0){ DeleteDC(dc); return false; }
    StartPage(dc);
    int dpiY=GetDeviceCaps(dc,LOGPIXELSY);
    int W=GetDeviceCaps(dc,HORZRES);
    int m=dpiY/2;
    SetBkMode(dc,TRANSPARENT);
    SetTextAlign(dc,TA_RIGHT|TA_RTLREADING);
    HFONT fT=CreateFontW(-(dpiY*20/72),0,0,0,FW_BOLD,0,0,0,DEFAULT_CHARSET,
        0,0,CLEARTYPE_QUALITY,0,L"Vazirmatn");
    HFONT fB=CreateFontW(-(dpiY*13/72),0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,
        0,0,CLEARTYPE_QUALITY,0,L"Vazirmatn");
    HGDIOBJ of=SelectObject(dc,fT);
    int y=m;
    RECT rt={m,y,W-m,y+dpiY};
    DrawTextW(dc,L"آزادی طب — قبض نوبت",-1,&rt,
        DT_CENTER|DT_RTLREADING|DT_NOPREFIX|DT_SINGLELINE);
    y+=dpiY;
    SelectObject(dc,fB);
    auto line=[&](const std::wstring& label,const std::wstring& val){
        RECT r={m,y,W-m,y+dpiY*9/16};
        std::wstring s=label+L": "+val;
        DrawTextW(dc,s.c_str(),-1,&r,DT_RIGHT|DT_RTLREADING|DT_NOPREFIX|DT_SINGLELINE);
        y+=dpiY*9/16;
    };
    wchar_t qn[24]; swprintf(qn,24,L"%d",a.queueNo);
    line(L"شماره نوبت",toFaDigits(qn));
    line(L"نام بیمار",a.firstName+L" "+a.lastName);
    line(L"کد ملی",toFaDigits(a.nationalId));
    line(L"پزشک",a.doctor);
    if(!a.service.empty()) line(L"خدمت",a.service);
    line(L"تاریخ نوبت",toFaDigits(a.apptDate)+L"  ("+a.day+L")");
    line(L"ساعت",toFaDigits(a.apptTime));
    if(!a.shift.empty()) line(L"شیفت",a.shift);
    line(L"نوع",a.kind);
    line(L"پذیرش‌کننده",a.user);
    SelectObject(dc,of); DeleteObject(fT); DeleteObject(fB);
    EndPage(dc); EndDoc(dc); DeleteDC(dc);
    return true;
}

// =============================================== layout ======
//  RIGHT panel width ~ 46% of the page; LEFT panel (grid) takes the rest.
static int apRightW(HWND h){
    RECT rc; GetClientRect(h,&rc);
    int w = (int)(rc.right*0.46);
    if(w < S(420)) w = S(420);
    if(w > rc.right - S(360)) w = rc.right - S(360);
    return w;
}
static RECT apGridRect(HWND h){
    RECT rc; GetClientRect(h,&rc);
    RECT g;
    g.left   = S(12);
    g.right  = rc.right - apRightW(h) - S(12);
    g.top    = S(54);    // below the grid toolbar
    g.bottom = rc.bottom - S(12);
    return g;
}

static void apLayout(HWND h, ApptUI* u){
    RECT rc; GetClientRect(h,&rc);
    int rW = apRightW(h);
    int rx = rc.right - rW;                 // right panel left edge
    int pad = S(14);
    int innerR = rc.right - pad;            // right inner edge
    int innerL = rx + pad;                  // left inner edge of right panel
    int colW   = (innerR - innerL - S(10)) / 2;
    int rightColX = innerR - colW;          // right column x
    int leftColX  = innerL;                 // left column x
    int fh = S(28);   // v1.8.0: fixed, comfortable input height (rows are taller
                      // now so the extra space becomes label gap, not a giant box)

    // ---- search group (top) -----------------------------------------------
    // v1.9.2: every group's first row starts BELOW the title band + label gap,
    // so the label clears the group title and the input clears the label.
    int searchTop = S(10);
    int y = searchTop + apHdrH() + apGrpPad() + apLblGap();
    // row1: کد ملی (right) | نام (left)
    MoveWindow(u->sNid,    rightColX, y, colW, fh, TRUE);
    MoveWindow(u->sFirst,  leftColX,  y, colW, fh, TRUE);
    y += apRowH();
    // row2: شماره موبایل (right) | نام خانوادگی (left)
    MoveWindow(u->sMobile, rightColX, y, colW, fh, TRUE);
    MoveWindow(u->sLast,   leftColX,  y, colW, fh, TRUE);
    y += apRowH();
    // checkbox (full width)
    MoveWindow(u->sCancelled, leftColX, y, innerR-innerL, S(22), TRUE);
    y += S(30);
    // جستجو button (right aligned)
    MoveWindow(u->sGo, innerR-S(120), y, S(120), S(34), TRUE);
    int searchBottom = y + S(34) + apGrpPad();

    // ---- appointment-details group ----------------------------------------
    int apptTop = searchBottom + S(12);
    int gy = apptTop + apHdrH() + apGrpPad() + apLblGap();   // clears the group title
    // پزشک combo + refresh + today
    MoveWindow(u->aDocToday,  innerL, gy, S(96), fh, TRUE);
    MoveWindow(u->aDocRefresh,innerL+S(102), gy, S(34), fh, TRUE);
    MoveWindow(u->aDoctor,    innerL+S(140), gy, innerR-innerL-S(140), fh, TRUE);
    gy += apRowH();
    // خدمت combo + F5/F4/F3
    MoveWindow(u->aSvcF3, innerL,        gy, S(34), fh, TRUE);
    MoveWindow(u->aSvcF4, innerL+S(38),  gy, S(34), fh, TRUE);
    MoveWindow(u->aSvcF5, innerL+S(76),  gy, S(34), fh, TRUE);
    MoveWindow(u->aService, innerL+S(114), gy, innerR-innerL-S(114), fh, TRUE);
    gy += apRowH();
    // تاریخ نوبت
    MoveWindow(u->aDate, rightColX, gy, colW, fh, TRUE);
    // نوع دریافت نوبت toggle (left)
    MoveWindow(u->aKind, leftColX, gy, colW, fh, TRUE);
    gy += apRowH();
    // ثبت و مرحله بعد
    MoveWindow(u->aNext, innerR-S(150), gy, S(150), S(34), TRUE);
    int apptBottom = gy + S(34) + apGrpPad();

    // ---- patient-details group --------------------------------------------
    int patTop = apptBottom + S(12);
    // foreign checkbox (full width) — its own row right under the title band
    int pForeignY = patTop + apHdrH() + apGrpPad();
    MoveWindow(u->pForeign, leftColX, pForeignY, innerR-innerL, S(22), TRUE);
    int py = pForeignY + S(30) + apLblGap();
    // کد ملی (right) | نام (left)  — actually nid full row for clarity
    MoveWindow(u->pNid, rightColX, py, colW, fh, TRUE);
    MoveWindow(u->pFirst, leftColX, py, colW, fh, TRUE);
    py += apRowH();
    // نام پدر (right) | نام خانوادگی (left)
    MoveWindow(u->pFather, rightColX, py, colW, fh, TRUE);
    MoveWindow(u->pLast,   leftColX,  py, colW, fh, TRUE);
    py += apRowH();
    // تلفن همراه (right) | جنسیت (left)
    MoveWindow(u->pMobile, rightColX, py, colW, fh, TRUE);
    MoveWindow(u->pGender, leftColX,  py, colW, fh, TRUE);
    py += apRowH();
    // توضیحات (full width)
    MoveWindow(u->pDesc, leftColX, py, innerR-innerL, fh, TRUE);
    py += apRowH()+S(2);
    // buttons: رزرو عادی نوبت | جدید | انصراف
    // v1.9.0: give the primary «رزرو عادی نوبت» (icon + long text) extra width
    // so it is never truncated; the two short buttons share the remainder.
    int totalBW=innerR-innerL-S(16);
    int bwR=(int)(totalBW*0.46);              // reserve (wide)
    int bwS=(totalBW-bwR-S(8))/2;             // جدید / انصراف
    MoveWindow(u->pReserve, innerR-bwR,            py, bwR, S(34), TRUE);
    MoveWindow(u->pNew,     innerR-bwR-S(8)-bwS,   py, bwS, S(34), TRUE);
    MoveWindow(u->pCancel,  innerL,                py, bwS, S(34), TRUE);

    // ---- grid toolbar (top-left) ------------------------------------------
    RECT g = apGridRect(h);
    int tbY=S(14), tbH=S(32), tx=g.right;
    // v1.9.0: widen the layout buttons so «ذخیره چیدمان» / «حذف چیدمان» (icon +
    // text) are never truncated.
    int wMsg=S(118),wTr=S(122),wPr=S(76),wSv=S(140),wDl=S(132),gp=S(6);
    MoveWindow(u->tMsg,      tx-wMsg, tbY, wMsg, tbH, TRUE);
    MoveWindow(u->tTransfer, tx-wMsg-gp-wTr, tbY, wTr, tbH, TRUE);
    MoveWindow(u->tPrint,    tx-wMsg-gp-wTr-gp-wPr, tbY, wPr, tbH, TRUE);
    MoveWindow(u->tSaveLayout,g.left+wDl+gp, tbY, wSv, tbH, TRUE);
    MoveWindow(u->tDelLayout, g.left,         tbY, wDl, tbH, TRUE);
}

// ============================================================== grid paint ==
//  Columns flow RIGHT → LEFT. We compute each column width by its weight so the
//  whole table fills the available grid rect.
static void computeColX(RECT g, int* xs /*N_GCOLS+1, RIGHT→LEFT edges*/){
    int totalW=0; for(int i=0;i<N_GCOLS;i++) totalW+=GCOLS[i].w;
    int avail=g.right-g.left;
    int x=g.right;
    xs[0]=x;
    for(int i=0;i<N_GCOLS;i++){
        int w=GCOLS[i].w*avail/totalW;
        x-=w; xs[i+1]=x;
    }
    xs[N_GCOLS]=g.left;   // snap last edge
}
static int apVisibleRows(HWND h){
    RECT g=apGridRect(h);
    return (g.bottom-(g.top+apHdrH())) / apRowH();
}
static std::wstring cellText(const Appointment& a, int col){
    switch(col){
        case 0: return a.cancelled?L"باطل":L"●";        // ابطال
        case 1: return L"✎";                            // ویرایش
        case 2: return L"🖶";                            // چاپ
        case 3: return a.user;                          // کاربر
        case 4: return a.mobile;                        // موبایل
        case 5: return a.doctor;                        // پزشک
        case 6: return a.lastName;                      // نام خانوادگی
        case 7: return a.firstName;                     // نام
        case 8: { wchar_t b[16]; swprintf(b,16,L"%d",a.queueNo); return toFaDigits(b); }
        case 9: return toFaDigits(a.apptTime);          // ساعت
        case 10:return a.day;                           // روز
        case 11:return toFaDigits(a.apptDate);          // تاریخ
        case 12:return a.kind;                          // نوع
    }
    return L"";
}
static void paintGrid(HDC dc, HWND h, ApptUI* u){
    RECT g=apGridRect(h);
    // card background (v1.8.0: corners patched to page bg)
    gpRoundRectBg(dc,g,S(10),g_theme.surface,g_theme.border,g_theme.bg);
    int xs[N_GCOLS+1]; computeColX(g,xs);
    // header row
    RECT hr={g.left,g.top,g.right,g.top+apHdrH()};
    HRGN clip=CreateRoundRectRgn(g.left,g.top,g.right+1,g.bottom+1,S(10),S(10));
    SelectClipRgn(dc,clip);
    RECT hrFill=hr;
    {
        HBRUSH b=CreateSolidBrush(g_theme.surface2);
        FillRect(dc,&hrFill,b); DeleteObject(b);
    }
    SelectObject(dc,g_fSmall);
    SetTextColor(dc,g_theme.text);
    for(int c=0;c<N_GCOLS;c++){
        RECT cr={xs[c+1],hr.top,xs[c],hr.bottom};
        RECT tr=cr; tr.left+=S(2); tr.right-=S(2);
        DrawTextW(dc,GCOLS[c].title,-1,&tr,
            DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX|DT_END_ELLIPSIS);
    }
    // grid lines + rows
    HPEN pen=CreatePen(PS_SOLID,1,g_theme.border);
    HGDIOBJ op=SelectObject(dc,pen);
    int vis=apVisibleRows(h);
    int y=g.top+apHdrH();
    SelectObject(dc,g_fUI);
    for(int r=0; r<vis; r++){
        int idx=u->scroll+r;
        if(idx>=(int)u->rows.size()) break;
        RECT row={g.left,y,g.right,y+apRowH()};
        if(idx==u->sel){
            HBRUSH b=CreateSolidBrush(g_theme.accent);
            FillRect(dc,&row,b); DeleteObject(b);
        } else if(idx==u->hot){
            HBRUSH b=CreateSolidBrush(g_theme.hover);
            FillRect(dc,&row,b); DeleteObject(b);
        } else if(r&1){
            HBRUSH b=CreateSolidBrush(g_theme.bg2);
            FillRect(dc,&row,b); DeleteObject(b);
        }
        const Appointment& a=u->rows[idx];
        COLORREF txt = (idx==u->sel)?g_theme.accentText
                       : a.cancelled?g_theme.textDim : g_theme.text;
        SetTextColor(dc,txt);
        for(int c=0;c<N_GCOLS;c++){
            RECT cr={xs[c+1],row.top,xs[c],row.bottom};
            RECT tr=cr; tr.left+=S(3); tr.right-=S(3);
            std::wstring s=cellText(a,c);
            // ابطال column: tint
            if(c==0 && !a.cancelled) SetTextColor(dc,(idx==u->sel)?g_theme.accentText:g_theme.danger);
            else if(c==0 && a.cancelled) SetTextColor(dc,g_theme.textDim);
            else SetTextColor(dc,txt);
            DrawTextW(dc,s.c_str(),-1,&tr,
                DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX|DT_END_ELLIPSIS);
        }
        // row separator
        MoveToEx(dc,g.left,row.bottom,0); LineTo(dc,g.right,row.bottom);
        y+=apRowH();
    }
    // column separators
    for(int c=1;c<N_GCOLS;c++){
        MoveToEx(dc,xs[c],g.top,0); LineTo(dc,xs[c],g.bottom);
    }
    // header underline
    MoveToEx(dc,g.left,g.top+apHdrH(),0); LineTo(dc,g.right,g.top+apHdrH());
    SelectObject(dc,op); DeleteObject(pen);
    SelectClipRgn(dc,NULL); DeleteObject(clip);
    // empty hint
    if(u->rows.empty()){
        SetTextColor(dc,g_theme.textDim); SelectObject(dc,g_fUI);
        RECT er={g.left,g.top+apHdrH(),g.right,g.bottom};
        DrawTextW(dc,L"نوبتی برای نمایش وجود ندارد",-1,&er,
            DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);
    }
}
// returns row index (in u->rows) or -1; *col set to clicked column
static int hitGridRow(HWND h, ApptUI* u, POINT pt, int* col){
    RECT g=apGridRect(h);
    if(!PtInRect(&g,pt)) return -1;
    if(pt.y < g.top+apHdrH()) return -1;
    int r=(pt.y-(g.top+apHdrH()))/apRowH();
    int idx=u->scroll+r;
    if(idx<0||idx>=(int)u->rows.size()) return -1;
    if(col){
        int xs[N_GCOLS+1]; computeColX(g,xs);
        *col=-1;
        for(int c=0;c<N_GCOLS;c++)
            if(pt.x>=xs[c+1]&&pt.x<xs[c]){ *col=c; break; }
    }
    return idx;
}

// ============================================================== group boxes =
static void drawGroup(HDC dc, RECT r, const wchar_t* title){
    gpRoundRect(dc,r,S(10),g_theme.surface,g_theme.border);
    // title chip on the top-right
    SelectObject(dc,g_fUIB); SetTextColor(dc,g_theme.accent);
    RECT tr={r.left+S(12),r.top-S(2),r.right-S(14),r.top+apHdrH()};
    DrawTextW(dc,title,-1,&tr,
        DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
}
static void drawLabel(HDC dc, int x, int y, int w, const wchar_t* s){
    SelectObject(dc,g_fSmall); SetTextColor(dc,g_theme.textDim);
    RECT r={x,y,x+w,y+S(16)};
    DrawTextW(dc,s,-1,&r,DT_RIGHT|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);
}
static void apPaint(HWND h, ApptUI* u, HDC dc){
    RECT rc; GetClientRect(h,&rc);
    FillRect(dc,&rc,g_brBg);
    SetBkMode(dc,TRANSPARENT);
    int rW=apRightW(h);
    int rx=rc.right-rW;
    int pad=S(14);
    int innerR=rc.right-pad, innerL=rx+pad;
    int colW=(innerR-innerL-S(10))/2;
    int rightColX=innerR-colW, leftColX=innerL;

    // ---- group rects (mirror apLayout y-positions EXACTLY) ----
    int LG=apLblGap();
    // search group
    int searchTop = S(10);
    int y = searchTop + apHdrH() + apGrpPad() + LG;     // == apLayout sNid y
    int searchBottom = y + apRowH()*2 + S(30) + S(34) + apGrpPad();
    RECT gSearch={rx+S(6),searchTop,rc.right-S(6),searchBottom};
    drawGroup(dc,gSearch,L"جستجوی بیمار رزرو کرده");
    drawLabel(dc,rightColX,y-LG,colW,L"کد ملی");
    drawLabel(dc,leftColX, y-LG,colW,L"نام");
    drawLabel(dc,rightColX,y+apRowH()-LG,colW,L"شماره موبایل");
    drawLabel(dc,leftColX, y+apRowH()-LG,colW,L"نام خانوادگی");

    // appointment group
    int apptTop = searchBottom + S(12);
    int gy = apptTop + apHdrH() + apGrpPad() + LG;       // == apLayout aDoctor y
    // rows: doctor, service, date/kind  → then the «ثبت و مرحله بعد» button row.
    int apptBottom = gy + apRowH()*3 + S(34) + apGrpPad();
    RECT gAppt={rx+S(6),apptTop,rc.right-S(6),apptBottom};
    drawGroup(dc,gAppt,L"مشخصات نوبت");
    drawLabel(dc,innerL+S(140),gy-LG,colW,L"پزشک");
    drawLabel(dc,innerL+S(114),gy+apRowH()-LG,colW,L"خدمت");
    drawLabel(dc,rightColX,gy+apRowH()*2-LG,colW,L"تاریخ نوبت");
    drawLabel(dc,leftColX, gy+apRowH()*2-LG,colW,L"نوع دریافت نوبت");

    // patient group
    int patTop = apptBottom + S(12);
    int pForeignY = patTop + apHdrH() + apGrpPad();
    int py = pForeignY + S(30) + LG;                     // == apLayout pNid y
    RECT gPat={rx+S(6),patTop,rc.right-S(6),rc.bottom-S(10)};
    drawGroup(dc,gPat,u->patientOn?L"مشخصات بیمار":L"مشخصات بیمار (غیرفعال)");
    drawLabel(dc,rightColX,py-LG,colW,L"کد ملی");
    drawLabel(dc,leftColX, py-LG,colW,L"نام");
    drawLabel(dc,rightColX,py+apRowH()-LG,colW,L"نام پدر");
    drawLabel(dc,leftColX, py+apRowH()-LG,colW,L"نام خانوادگی");
    drawLabel(dc,rightColX,py+apRowH()*2-LG,colW,L"تلفن همراه");
    drawLabel(dc,leftColX, py+apRowH()*2-LG,colW,L"جنسیت");
    drawLabel(dc,leftColX, py+apRowH()*3-LG,innerR-innerL,L"توضیحات");

    // v1.8.0 UI: framed input wells behind every field (matches the reception
    // form). A focused field gets a THIN soft-red focus ring; unfocused fields
    // keep a subtle hairline border. This replaces the bare, borderless edits.
    {
        HWND edits[]={u->pNid,u->pFirst,u->pFather,u->pMobile,u->pLast,
            u->pGender,u->pDesc,u->aDate,u->sNid,u->sFirst,u->sMobile,u->sLast,
            u->aDoctor,u->aService};
        HWND foc=GetFocus();
        COLORREF focusRing = blendColor(g_theme.danger, g_theme.inputBg, 60);
        for(HWND ec:edits){
            if(!ec || !IsWindowVisible(ec)) continue;
            RECT wr; GetWindowRect(ec,&wr);
            POINT a={wr.left,wr.top}, b={wr.right,wr.bottom};
            ScreenToClient(h,&a); ScreenToClient(h,&b);
            if(b.y<=a.y) continue;
            RECT well={a.x-S(6),a.y-S(4),b.x+S(6),b.y+S(4)};
            COLORREF bord = (ec==foc)? focusRing : g_theme.border;
            fillRoundRect(dc,well,S(8),g_theme.inputBg,bord);
        }
    }

    // v1.7.0: when an inquiry could NOT verify the identity, draw a clear danger
    // frame around the name (pFirst) and surname (pLast) edit boxes so the
    // operator sees they must enter / re-check those by hand (no fabrication).
    if(u->idChecked && !u->idVerified){
        HWND warn[2]={u->pFirst,u->pLast};
        for(HWND wctl:warn){
            if(!wctl) continue;
            RECT wr; GetWindowRect(wctl,&wr);
            POINT a={wr.left,wr.top}, b={wr.right,wr.bottom};
            ScreenToClient(h,&a); ScreenToClient(h,&b);
            if(b.y<=a.y) continue;
            // v1.9.0: a single, very thin red hairline only — no glow / second ring.
            RECT fr={a.x-S(6),a.y-S(4),b.x+S(6),b.y+S(4)};
            fillRoundRect(dc,fr,S(8),g_theme.inputBg,g_theme.danger);
        }
    }

    // ---- status message line (above grid toolbar, on the left side) ----
    if(!u->statusMsg.empty()){
        RECT g=apGridRect(h);
        SelectObject(dc,g_fSmall); SetTextColor(dc,u->statusCol);
        RECT sr={g.left,S(50)-S(16),g.right,S(50)};
        // status is drawn just under the toolbar inside the grid card top — but
        // we keep it subtle; skip if it would overlap. Draw near bottom instead.
        RECT br={g.left,rc.bottom-S(0),g.right,rc.bottom};
        (void)sr;(void)br;
        // draw under the right panel groups bottom area is busy; show at the
        // very top-left toolbar baseline:
        RECT tr={g.left, S(46), g.right, S(46)+S(16)};
        DrawTextW(dc,u->statusMsg.c_str(),-1,&tr,
            DT_LEFT|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX|DT_END_ELLIPSIS);
    }

    // ---- the grid ----
    paintGrid(dc,h,u);
}

// ----------------------------------------------- national-id Enter subclass -
static WNDPROC g_nidPrev = NULL;
static LRESULT CALLBACK nidProc(HWND h, UINT m, WPARAM w, LPARAM l){
    if(m==WM_KEYDOWN && (w==VK_RETURN || w==VK_TAB)){
        ApptUI* u=(ApptUI*)GetWindowLongPtrW(GetParent(h),GWLP_USERDATA);
        if(u){ lookupPatient(u); return 0; }
    }
    if(m==WM_CHAR && w==VK_RETURN) return 0;   // suppress beep
    return CallWindowProcW(g_nidPrev,h,m,w,l);
}

// ============================================================== window proc =
static void updateKindButton(ApptUI* u){
    SetWindowTextW(u->aKind, u->kindInPerson?L"حضوری":L"غیرحضوری");
}
static void updateToolbarState(ApptUI* u){
    bool has = (u->sel>=0 && u->sel<(int)u->rows.size());
    EnableWindow(u->tMsg, has);
    EnableWindow(u->tTransfer, has);
}

static LRESULT CALLBACK apProc(HWND h, UINT m, WPARAM w, LPARAM l){
    ApptUI* u=(ApptUI*)GetWindowLongPtrW(h,GWLP_USERDATA);
    switch(m){
    case WM_CREATE: {
        CREATESTRUCTW* cs=(CREATESTRUCTW*)l;
        u=(ApptUI*)cs->lpCreateParams;
        SetWindowLongPtrW(h,GWLP_USERDATA,(LONG_PTR)u);
        u->page=h;
        DWORD es=WS_CHILD|WS_VISIBLE|WS_TABSTOP|ES_AUTOHSCROLL;
        // ---- search controls ----
        u->sNid   =CreateWindowExW(0,L"EDIT",L"",es|ES_NUMBER,0,0,10,10,h,(HMENU)AP_S_NID,g_hInst,0);
        u->sMobile=CreateWindowExW(0,L"EDIT",L"",es|ES_NUMBER,0,0,10,10,h,(HMENU)AP_S_MOBILE,g_hInst,0);
        u->sFirst =CreateWindowExW(0,L"EDIT",L"",es,0,0,10,10,h,(HMENU)AP_S_FIRST,g_hInst,0);
        u->sLast  =CreateWindowExW(0,L"EDIT",L"",es,0,0,10,10,h,(HMENU)AP_S_LAST,g_hInst,0);
        u->sCancelled=CreateWindowExW(0,L"BUTTON",L"نمایش بیمارانی که نوبت آن‌ها باطل شده است",
            WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX|BS_RIGHTBUTTON|BS_RIGHT,0,0,10,10,h,(HMENU)AP_S_CANCELLED,g_hInst,0);
        u->sGo=createFlatButton(h,AP_S_GO,L"جستجو",ICO_NONE,BS_PRIMARY,0,0,10,10);
        // ---- appointment-details controls ----
        u->aDoctor =createThemedCombo(h,AP_A_DOCTOR);
        u->aDocRefresh=createFlatButton(h,AP_A_DOC_REFRESH,L"",ICO_REFRESH,BS_OUTLINE,0,0,10,10);
        u->aDocToday=createFlatButton(h,AP_A_DOC_TODAY,L"پزشکان امروز",ICO_NONE,BS_OUTLINE,0,0,10,10);
        u->aService=createThemedCombo(h,AP_A_SERVICE);
        u->aSvcF5=createFlatButton(h,AP_A_SVC_F5,L"F5",ICO_NONE,BS_OUTLINE,0,0,10,10);
        u->aSvcF4=createFlatButton(h,AP_A_SVC_F4,L"F4",ICO_NONE,BS_OUTLINE,0,0,10,10);
        u->aSvcF3=createFlatButton(h,AP_A_SVC_F3,L"F3",ICO_NONE,BS_OUTLINE,0,0,10,10);
        u->aDate =CreateWindowExW(0,L"EDIT",L"",es,0,0,10,10,h,(HMENU)AP_A_DATE,g_hInst,0);
        u->aNext =createFlatButton(h,AP_A_NEXT,L"ثبت و مرحله بعد",ICO_CHECK,BS_PRIMARY,0,0,10,10);
        u->aKind =createFlatButton(h,AP_A_KIND,L"حضوری",ICO_NONE,BS_OUTLINE,0,0,10,10);
        // ---- patient-details controls ----
        u->pForeign=CreateWindowExW(0,L"BUTTON",L"تابعیت غیرایرانی / نوزادان",
            WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX|BS_RIGHTBUTTON|BS_RIGHT,0,0,10,10,h,(HMENU)AP_P_FOREIGN,g_hInst,0);
        u->pNid   =CreateWindowExW(0,L"EDIT",L"",es|ES_NUMBER,0,0,10,10,h,(HMENU)AP_P_NID,g_hInst,0);
        u->pFirst =CreateWindowExW(0,L"EDIT",L"",es,0,0,10,10,h,(HMENU)AP_P_FIRST,g_hInst,0);
        u->pFather=CreateWindowExW(0,L"EDIT",L"",es,0,0,10,10,h,(HMENU)AP_P_FATHER,g_hInst,0);
        u->pMobile=CreateWindowExW(0,L"EDIT",L"",es|ES_NUMBER,0,0,10,10,h,(HMENU)AP_P_MOBILE,g_hInst,0);
        u->pLast  =CreateWindowExW(0,L"EDIT",L"",es,0,0,10,10,h,(HMENU)AP_P_LAST,g_hInst,0);
        u->pGender=createThemedCombo(h,AP_P_GENDER);
        u->pDesc  =CreateWindowExW(0,L"EDIT",L"",es,0,0,10,10,h,(HMENU)AP_P_DESC,g_hInst,0);
        u->pReserve=createFlatButton(h,AP_P_RESERVE,L"رزرو عادی نوبت",ICO_SAVE,BS_PRIMARY,0,0,10,10);
        u->pNew   =createFlatButton(h,AP_P_NEW,L"جدید",ICO_PLUS,BS_OUTLINE,0,0,10,10);
        u->pCancel=createFlatButton(h,AP_P_CANCEL,L"انصراف",ICO_X,BS_OUTLINE,0,0,10,10);
        // ---- grid toolbar ----
        u->tMsg     =createFlatButton(h,AP_T_MSG,L"ارسال پیام",ICO_BELL,BS_OUTLINE,0,0,10,10);
        u->tTransfer=createFlatButton(h,AP_T_TRANSFER,L"انتقال نوبت",ICO_DETACH,BS_OUTLINE,0,0,10,10);
        u->tPrint   =createFlatButton(h,AP_T_PRINT,L"چاپ",ICO_PRINT,BS_OUTLINE,0,0,10,10);
        u->tSaveLayout=createFlatButton(h,AP_T_SAVELAYOUT,L"ذخیره چیدمان",ICO_SAVE,BS_OUTLINE,0,0,10,10);
        u->tDelLayout =createFlatButton(h,AP_T_DELLAYOUT,L"حذف چیدمان",ICO_TRASH,BS_OUTLINE,0,0,10,10);

        // fonts
        HWND eds[]={u->sNid,u->sMobile,u->sFirst,u->sLast,u->aDate,
                    u->pNid,u->pFirst,u->pFather,u->pMobile,u->pLast,u->pDesc};
        for(HWND e: eds){ SendMessageW(e,WM_SETFONT,(WPARAM)g_fUI,TRUE); enableEnterNavigation(e); }
        // auto RTL/LTR on the text (name) fields
        enableAutoDir(u->sFirst); enableAutoDir(u->sLast);
        enableAutoDir(u->pFirst); enableAutoDir(u->pFather);
        enableAutoDir(u->pLast);  enableAutoDir(u->pDesc);
        // appointment date uses the smart Jalali mask
        SendMessageW(u->aDate,EM_SETLIMITTEXT,10,0);
        enableDateMask(u->aDate);
        SetWindowTextW(u->aDate, jalaliDateShort(iranNow()).c_str());
        SendMessageW(u->sCancelled,WM_SETFONT,(WPARAM)g_fSmall,TRUE);
        SendMessageW(u->pForeign,WM_SETFONT,(WPARAM)g_fSmall,TRUE);
        // gender combo
        SendMessageW(u->pGender,CB_ADDSTRING,0,(LPARAM)L"مرد");
        SendMessageW(u->pGender,CB_ADDSTRING,0,(LPARAM)L"زن");
        SendMessageW(u->pGender,CB_SETCURSEL,0,0);
        // national-id Enter subclass
        g_nidPrev=(WNDPROC)SetWindowLongPtrW(u->pNid,GWLP_WNDPROC,(LONG_PTR)nidProc);

        fillDoctors(u,false);
        setPatientEnabled(u,false);
        updateToolbarState(u);
        refreshGrid(u);
        return 0; }
    case WM_SIZE: if(u) apLayout(h,u); return 0;
    case WM_APP_THEME: InvalidateRect(h,NULL,TRUE); return 0;
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
        SetTextColor(dc,g_theme.text); SetBkColor(dc,g_theme.surface);
        return (LRESULT)g_brSurface; }
    case WM_DRAWITEM: {
        if(drawThemedComboItem((LPDRAWITEMSTRUCT)l)) return TRUE;
        break; }
    case WM_MOUSEWHEEL: {
        if(!u) return 0;
        int delta=GET_WHEEL_DELTA_WPARAM(w);
        int step=(delta>0)?-1:1;
        int maxScroll=(int)u->rows.size()-apVisibleRows(h);
        if(maxScroll<0) maxScroll=0;
        u->scroll+=step;
        if(u->scroll<0)u->scroll=0;
        if(u->scroll>maxScroll)u->scroll=maxScroll;
        InvalidateRect(h,NULL,FALSE);
        return 0; }
    case WM_MOUSEMOVE: {
        if(!u) return 0;
        POINT pt={GET_X_LPARAM(l),GET_Y_LPARAM(l)};
        int col; int hit=hitGridRow(h,u,pt,&col);
        if(hit!=u->hot){ u->hot=hit; InvalidateRect(h,NULL,FALSE); }
        return 0; }
    case WM_LBUTTONDOWN: {
        if(!u) return 0;
        POINT pt={GET_X_LPARAM(l),GET_Y_LPARAM(l)};
        int col=-1; int hit=hitGridRow(h,u,pt,&col);
        if(hit>=0){
            u->sel=hit; updateToolbarState(u);
            if(col==0){   // ابطال
                const Appointment& a=u->rows[hit];
                if(!a.cancelled){
                    if(MessageBoxW(h,L"این نوبت باطل شود؟",L"ابطال نوبت",
                        MB_YESNO|MB_ICONWARNING)==IDYES){
                        int src=u->srcIndex[hit];
                        if(src>=0){ cancelAppointment(src); refreshGrid(u); }
                    }
                }
            } else if(col==1){   // ویرایش
                loadForEdit(u,hit);
            } else if(col==2){   // چاپ
                if(printApptSlip(h,u->rows[hit])){
                    u->statusMsg=L"قبض نوبت به چاپگر ارسال شد."; u->statusCol=g_theme.success;
                } else {
                    u->statusMsg=L"چاپ انجام نشد (چاپگر را بررسی کنید)."; u->statusCol=g_theme.danger;
                }
            }
            InvalidateRect(h,NULL,FALSE);
        }
        return 0; }
    case WM_COMMAND: {
        if(!u) return 0;
        int id=LOWORD(w), code=HIWORD(w);
        // v1.8.0: repaint so the focused field's thin red focus ring updates.
        if(code==EN_SETFOCUS || code==EN_KILLFOCUS ||
           code==CBN_SETFOCUS || code==CBN_KILLFOCUS){
            InvalidateRect(h,NULL,FALSE);
        }
        if(id==AP_S_GO) doSearch(u);
        else if(id==AP_S_CANCELLED && code==BN_CLICKED) refreshGrid(u);
        else if(id==AP_A_DOCTOR && code==CBN_SELCHANGE) onDoctorChanged(u);
        else if(id==AP_A_DOC_REFRESH) fillDoctors(u,false);
        else if(id==AP_A_DOC_TODAY)   fillDoctors(u,true);
        else if(id==AP_A_KIND){
            u->kindInPerson=!u->kindInPerson; updateKindButton(u);
        }
        else if(id==AP_A_NEXT){
            // "ثبت و مرحله بعد" — move focus to the patient block (enable on nid)
            SetFocus(u->pNid);
            u->statusMsg=L"کد ملی بیمار را وارد و Enter بزنید.";
            u->statusCol=g_theme.accent; InvalidateRect(h,NULL,FALSE);
        }
        else if(id==AP_A_SVC_F5) svcRefresh(u);   // F5: بازخوانی خدمات پزشک
        else if(id==AP_A_SVC_F4) svcAddCustom(u,h);// F4: افزودن خدمت
        else if(id==AP_A_SVC_F3) svcClear(u);      // F3: پاک‌سازی انتخاب
        else if(id==AP_P_FOREIGN && code==BN_CLICKED){
            bool f=(SendMessageW(u->pForeign,BM_GETCHECK,0,0)==BST_CHECKED);
            if(f) setPatientEnabled(u,true);   // allow manual entry immediately
        }
        else if(id==AP_P_RESERVE) submitAppointment(u);
        else if(id==AP_P_NEW){ resetPatient(u); SetFocus(u->pNid);
            u->statusMsg=L""; InvalidateRect(h,NULL,FALSE); }
        else if(id==AP_P_CANCEL){ resetPatient(u);
            u->statusMsg=L"انصراف انجام شد."; u->statusCol=g_theme.textDim;
            InvalidateRect(h,NULL,FALSE); }
        else if(id==AP_T_MSG){
            if(u->sel>=0){
                const Appointment& a=u->rows[u->sel];
                pushMessageT(g_session.user.username,a.nationalId,
                    L"یادآوری نوبت برای "+a.firstName+L" "+a.lastName,KMSG_NORMAL);
                u->statusMsg=L"پیام برای بیمار ثبت شد."; u->statusCol=g_theme.success;
                InvalidateRect(h,NULL,FALSE);
            }
        }
        else if(id==AP_T_TRANSFER) transferAppointment(u,h);
        else if(id==AP_T_PRINT){
            if(u->sel>=0 && u->sel<(int)u->rows.size()){
                if(printApptSlip(h,u->rows[u->sel])){
                    u->statusMsg=L"قبض نوبت به چاپگر ارسال شد."; u->statusCol=g_theme.success;
                } else { u->statusMsg=L"چاپ انجام نشد."; u->statusCol=g_theme.danger; }
                InvalidateRect(h,NULL,FALSE);
            } else refreshGrid(u);
        }
        else if(id==AP_T_SAVELAYOUT){
            setSetting(L"appt_layout_saved",L"1");
            u->statusMsg=L"چیدمان ذخیره شد."; u->statusCol=g_theme.success;
            InvalidateRect(h,NULL,FALSE);
        }
        else if(id==AP_T_DELLAYOUT){
            setSetting(L"appt_layout_saved",L"0");
            u->statusMsg=L"چیدمان حذف شد."; u->statusCol=g_theme.textDim;
            InvalidateRect(h,NULL,FALSE);
        }
        return 0; }
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps; HDC dc0=BeginPaint(h,&ps);
        RECT rc; GetClientRect(h,&rc);
        if(rc.right<=0||rc.bottom<=0){ EndPaint(h,&ps); return 0; }
        HDC dc=CreateCompatibleDC(dc0);
        HBITMAP bmp=CreateCompatibleBitmap(dc0,rc.right,rc.bottom);
        HGDIOBJ obm=SelectObject(dc,bmp);
        if(u) apPaint(h,u,dc);
        BitBlt(dc0,0,0,rc.right,rc.bottom,dc,0,0,SRCCOPY);
        SelectObject(dc,obm); DeleteObject(bmp); DeleteDC(dc);
        EndPaint(h,&ps);
        return 0; }
    case WM_NCDESTROY:
        if(u){ delete u; SetWindowLongPtrW(h,GWLP_USERDATA,0); }
        break;
    }
    return DefWindowProcW(h,m,w,l);
}

// ============================================================== factory =====
HWND createAppointmentPage(HWND parent){
    static bool reg=false;
    if(!reg){
        WNDCLASSW wc={0};
        wc.lpfnWndProc=apProc; wc.hInstance=g_hInst;
        wc.hCursor=LoadCursor(NULL,IDC_ARROW);
        wc.lpszClassName=AP_CLASS;
        RegisterClassW(&wc); reg=true;
    }
    ApptUI* u=new ApptUI();
    RECT rc; GetClientRect(parent,&rc);
    HWND h=CreateWindowExW(0,AP_CLASS,L"",
        WS_CHILD|WS_CLIPCHILDREN,
        0,0,rc.right,rc.bottom,parent,NULL,g_hInst,u);
    if(!h){ delete u; return NULL; }
    return h;
}
