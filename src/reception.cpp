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
#define ID_RC_NEWPAT  503   // "پذیرش جدید" — clears the ACTIVE tab's form
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
#define ID_F_INQUIRY  645   // استعلام بیمه با کد ملی
#define ID_F_HASINS   646   // چک‌باکس «دارای بیمه» کنار کد ملی
// ---- right info panel ids (v1.6.0) ----
#define ID_F_ARCHIVE     650
#define ID_F_ARCHIVE_GO  651
#define ID_F_FILE        652
#define ID_F_FILE_GO     653
#define ID_F_BOOKNO      654
#define ID_F_BOOKNO_CHK  655
#define ID_F_VALID       656
#define ID_F_VALID_AUTO  657
#define ID_F_RXDATE      658
#define ID_F_RXDATE_AUTO 659
#define ID_F_SUPP_PANEL  660
#define ID_F_SUPP_PCT    661
#define ID_F_SUPP_GO     662
#define ID_F_DOCCODE     663
#define ID_F_DOCNAME     664
#define ID_F_DOCSEARCH   665

// ============================================================== TAB PAGE ===
enum TabKind {
    TK_RECEPTION   = 0,   // the patient reception + billing form
    TK_PORTAL      = 1,   // پیام پرتابل — portal/admin message page (post-login)
    TK_EMPTY       = 2,   // a fresh blank tab (new-tab button)
    TK_APPOINTMENT = 3    // نوبت‌دهی — the appointment module (its own child page)
};
struct TabPage {
    HWND page;                // container window (child of reception OR detached)
    int  kind;                // TabKind
    std::wstring title;
    // form controls
    HWND eFirst,eLast,eNid,eFather,eBirth,cGender,eMobile,ePhone,eAddr;
    HWND cPType,cIns,cSupp,cNType;
    HWND ePrice,eDiscount;
    HWND bSubmit,bPrtIns,bPrtRx,bPrtLast,bClose,bInquiry;
    HWND chkIns;             // «دارای بیمه» — کنار کد ملی، پیش‌فرض تیک‌خورده
    HWND appt;               // نوبت‌دهی child page (kind==TK_APPOINTMENT)
    std::vector<int> insAllowed;   // insurances this patient carries (inquiry)
    // ---- right info panel (v1.6.0) ----
    HWND eArchive, bArchiveGo;     // ش بایگانی + استعلام
    HWND eFile,    bFileGo;        // ش پرونده + استعلام
    HWND eBookNo;  HWND chkBookNo; // ش دفترچه + enable checkbox
    HWND eValid;   HWND chkValidAuto;   // تاریخ اعتبار + اتوماتیک
    HWND eRxDate;  HWND chkRxAuto;      // تاریخ نسخه + اتوماتیک
    HWND cSuppPanel; HWND eSuppPct; HWND bSuppGo;  // بیمه مکمل + درصد + استعلام
    HWND eDocCode, eDocName, bDocSearch;           // پزشک معالج
    // computed billing
    long long total,mainShare,patientShare,baseDiff,orgShare,paid;
    bool detached;
    bool autoPrice;          // guard: ignore EN_CHANGE from our own auto-fill
    std::wstring lastMsg; COLORREF msgCol;
    TabPage():page(0),kind(TK_RECEPTION),appt(0),total(0),mainShare(0),patientShare(0),
        baseDiff(0),orgShare(0),paid(0),detached(false),autoPrice(false),msgCol(0){}
};

struct RecData {
    HWND bCalc, bNewTab, bNewPat;
    std::vector<TabPage*> tabs;
    int active;
    int hotTab, hotClose, hotDetach;     // hover indices
    int lastUnseen;                      // cartable poll state
    RecData():active(-1),hotTab(-1),hotClose(-1),hotDetach(-1),lastUnseen(0){}
};
static RecData* s_rd = NULL;             // single reception screen at a time

// metrics
static int infoBarH(){ return S(54); }
static int tabBarH(){ return S(40); }
static int tabW()    { return S(210); }

// ---------------------------------------------------------------- billing --
//  The program computes the bill ITSELF: if the secretary leaves the service
//  price empty, a default tariff is derived from patient type + appointment
//  type (اورژانس/پرسنلی) and auto-filled into the price box.
static void recalc(TabPage* t){
    if(!t || !t->ePrice || !t->eDiscount) return;
    wchar_t buf[64];
    GetWindowTextW(t->ePrice,buf,64);
    long long price = parseMoney(buf);

    int pType = (int)SendMessageW(t->cPType,CB_GETCURSEL,0,0); if(pType<0)pType=0;
    int nType = (int)SendMessageW(t->cNType,CB_GETCURSEL,0,0); if(nType<0)nType=0;

    // auto-fill default tariff when the field is empty / zero
    if(price <= 0){
        price = defaultServicePrice(pType, nType);
        std::wstring pf = toFaDigits(formatMoney(price));
        // avoid recursive EN_CHANGE loops: only set if text actually differs
        wchar_t cur[64]; GetWindowTextW(t->ePrice,cur,64);
        if(parseMoney(cur)!=price){
            t->autoPrice = true;
            SetWindowTextW(t->ePrice, pf.c_str());
            t->autoPrice = false;
        }
    }

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
// Manual RTL, plain (non-mirrored) coordinates.
//   Page layout (RTL):
//     ┌─ billing card ─┐  ┌──────────── form card (RIGHT, wide) ────────────┐
//     │ (LEFT, fixed)  │  │  patient sections, two columns                  │
//     │  print buttons │  └─────────────────────────────────────────────────┘
//     └────────────────┘
//   The billing / print card sits on the LEFT so the print actions live on the
//   left edge (per requirement); the patient form fills the wide right area.
static const int RC_OUT = 18;   // outer page padding
static const int RC_GAP = 16;   // gap between form card and billing card
static const int RC_IN  = 22;   // inner card padding
static const int BILL_W = 340;  // billing card width
static const int INFO_W = 320;  // right info-panel width (v1.6.0)

//  infoL/infoR give the right info-panel bounds (0 if it doesn't fit).
static void rcMetrics2(int W, int& cardL, int& cardR, int& billL, int& billR,
                       int& infoL, int& infoR, int& colW, int& xr, int& xl,
                       bool& stacked){
    int pad=S(RC_OUT);
    // billing/print card on the LEFT, info panel on the RIGHT, form in middle
    billL = pad;
    billR = billL + S(BILL_W);
    infoR = W - pad;
    infoL = infoR - S(INFO_W);
    cardL = billR + S(RC_GAP);
    cardR = infoL - S(RC_GAP);
    int fw = (cardR - S(RC_IN)) - (cardL + S(RC_IN));
    stacked = fw < S(320);
    if(stacked){
        // drop the side panels and let the form fill the page
        billL = billR = 0; infoL = infoR = 0;
        cardL = pad; cardR = W - pad;
        fw = (cardR-S(RC_IN))-(cardL+S(RC_IN));
    }
    colW = (fw - S(18))/2;
    xr = (cardR - S(RC_IN)) - colW;            // right column (RTL first field)
    xl = cardL + S(RC_IN);                     // left column
}
//  Back-compat wrapper for callers that don't need the info panel bounds.
static void rcMetrics(int W, int& cardL, int& cardR, int& billL, int& billR,
                      int& colW, int& xr, int& xl, bool& stacked){
    int infoL,infoR;
    rcMetrics2(W,cardL,cardR,billL,billR,infoL,infoR,colW,xr,xl,stacked);
}
//  Vertical metrics adapt to available height so the form never overflows on
//  small / low-res monitors (responsive requirement).
static void rcVMetrics(int H, int& y0, int& step, int& rh){
    // y0 must clear: card top (S16) + header (≈S46) + separator (S52) and then
    // leave room above the first row for its section caption. Start ~S124.
    // step must be tall enough that: label(18px) + gap(6) + input(rh) fit with a
    // small gap before the next label, so nothing overlaps.
    y0 = S(124); step = S(66); rh = S(34);
    int need = y0 + 8*step + S(126);
    if(H > 0 && need > H){
        step = (H - y0 - S(126)) / 8;
        if(step < S(56)) step = S(56);     // keep label+input from overlapping
        rh = step - S(30); if(rh > S(34)) rh = S(34); if(rh < S(26)) rh = S(26);
    }
}
static void tabPageLayout(HWND h, TabPage* t){
    if(!t) return;
    if(t->kind==TK_APPOINTMENT){
        if(t->appt){ RECT rc; GetClientRect(h,&rc);
            MoveWindow(t->appt,0,0,rc.right,rc.bottom,TRUE); }
        return;
    }
    if(t->kind!=TK_RECEPTION) return;   // painted pages have no controls
    RECT rc; GetClientRect(h,&rc);
    int W=rc.right, H=rc.bottom;
    if(W<=0 || H<=0) return;
    int cardL,cardR,billL,billR,infoL,infoR,colW,xr,xl; bool stacked;
    rcMetrics2(W,cardL,cardR,billL,billR,infoL,infoR,colW,xr,xl,stacked);
    int y0,step,rh; rcVMetrics(H,y0,step,rh);
    int formLeft = cardL+S(RC_IN);
    int formRight= cardR-S(RC_IN);
    int fw = formRight - formLeft;

    // Section 1: هویت بیمار (rows 0..2)
    MoveWindow(t->eFirst, xr, y0,          colW, rh, TRUE);
    MoveWindow(t->eLast,  xl, y0,          colW, rh, TRUE);
    MoveWindow(t->eNid,   xr, y0+step,     colW, rh, TRUE);
    MoveWindow(t->eFather,xl, y0+step,     colW, rh, TRUE);
    MoveWindow(t->eBirth, xr, y0+2*step,   colW, rh, TRUE);
    MoveWindow(t->cGender,xl, y0+2*step,   colW, S(200), TRUE);
    // Section 2: تماس (rows 3..4)
    MoveWindow(t->eMobile,xr, y0+3*step,   colW, rh, TRUE);
    MoveWindow(t->ePhone, xl, y0+3*step,   colW, rh, TRUE);
    MoveWindow(t->eAddr,  formLeft, y0+4*step, fw, rh, TRUE);
    // Section 3: نوبت و بیمه (rows 5..6)
    MoveWindow(t->cPType, xr, y0+5*step,   colW, S(200), TRUE);
    MoveWindow(t->cNType, xl, y0+5*step,   colW, S(200), TRUE);
    // «دارای بیمه» checkbox sits ABOVE the insurance combo (right column),
    // so unchecking it before picking insurance reads naturally top-to-bottom.
    { int chkW=S(110);
      MoveWindow(t->chkIns, xr, y0+6*step-S(20), chkW, S(18), TRUE); }
    MoveWindow(t->cIns,   xr, y0+6*step,   colW, S(240), TRUE);
    MoveWindow(t->cSupp,  xl, y0+6*step,   colW, S(240), TRUE);
    // Section 4: مبلغ (row 7)
    MoveWindow(t->ePrice, xr, y0+7*step,   colW, rh, TRUE);
    MoveWindow(t->eDiscount,xl,y0+7*step,  colW, rh, TRUE);
    // submit
    MoveWindow(t->bSubmit,formLeft, y0+8*step+S(6), fw, S(50), TRUE);

    // billing panel buttons (bottom of billing card)
    if(!stacked){
        int bx = billL + S(16), byy = H - S(214);
        int bbw = (billR-billL) - S(32);
        MoveWindow(t->bPrtIns, bx, byy,        bbw, S(40), TRUE);
        MoveWindow(t->bPrtRx,  bx, byy+S(48),  bbw, S(40), TRUE);
        MoveWindow(t->bPrtLast,bx, byy+S(96),  bbw, S(40), TRUE);
        MoveWindow(t->bClose,  bx, byy+S(152), bbw, S(42), TRUE);
        MoveWindow(t->bInquiry,bx, byy-S(48),  bbw, S(40), TRUE);
        ShowWindow(t->bPrtIns,SW_SHOW); ShowWindow(t->bPrtRx,SW_SHOW);
        ShowWindow(t->bPrtLast,SW_SHOW); ShowWindow(t->bClose,SW_SHOW);
        ShowWindow(t->bInquiry,SW_SHOW);
    } else {
        // stacked: place inquiry under the submit button
        MoveWindow(t->bInquiry,formLeft, y0+8*step+S(62), fw, S(40), TRUE);
        ShowWindow(t->bInquiry,SW_SHOW);
        ShowWindow(t->bPrtIns,SW_HIDE); ShowWindow(t->bPrtRx,SW_HIDE);
        ShowWindow(t->bPrtLast,SW_HIDE); ShowWindow(t->bClose,SW_HIDE);
    }

    // ====================== RIGHT INFO PANEL layout ======================
    HWND infoCtls[]={t->eArchive,t->bArchiveGo,t->eFile,t->bFileGo,t->eBookNo,
        t->chkBookNo,t->eValid,t->chkValidAuto,t->eRxDate,t->chkRxAuto,
        t->cSuppPanel,t->eSuppPct,t->bSuppGo,t->eDocCode,t->eDocName,t->bDocSearch};
    if(stacked || infoR<=infoL){
        for(HWND c: infoCtls) if(c) ShowWindow(c,SW_HIDE);
    } else {
        for(HWND c: infoCtls) if(c) ShowWindow(c,SW_SHOW);
        int ipad=S(16);
        int iL=infoL+ipad, iR=infoR-ipad;
        int iw=iR-iL;
        int rh2=S(28), gp=S(6);
        // y starts below: avatar(70) + identity(40) + chips(54) ≈ painted header
        int y=S(16)+S(74)+S(40)+S(40)+S(8);   // ~ after avatar + نسخه + قبض/بارکد + P:S
        // --- search keys group ---
        y += S(26);  // group title
        int btnW=S(78);
        MoveWindow(t->bArchiveGo, iL, y, btnW, rh2, TRUE);
        MoveWindow(t->eArchive,   iL+btnW+gp, y, iw-btnW-gp, rh2, TRUE);
        y += rh2+gp;
        MoveWindow(t->bFileGo,    iL, y, btnW, rh2, TRUE);
        MoveWindow(t->eFile,      iL+btnW+gp, y, iw-btnW-gp, rh2, TRUE);
        y += rh2+S(16);
        // --- insurance block group ---
        y += S(26);  // group title
        // ش دفترچه + فعال checkbox
        MoveWindow(t->chkBookNo, iL, y, S(64), rh2, TRUE);
        MoveWindow(t->eBookNo,   iL+S(70), y, iw-S(70), rh2, TRUE);
        y += rh2+gp;
        // تاریخ اعتبار + اتوماتیک
        MoveWindow(t->chkValidAuto, iL, y, S(86), rh2, TRUE);
        MoveWindow(t->eValid,       iL+S(92), y, iw-S(92), rh2, TRUE);
        y += rh2+gp;
        // تاریخ نسخه + اتوماتیک
        MoveWindow(t->chkRxAuto, iL, y, S(86), rh2, TRUE);
        MoveWindow(t->eRxDate,   iL+S(92), y, iw-S(92), rh2, TRUE);
        y += rh2+gp;
        // بیمه مکمل + درصد + استعلام
        MoveWindow(t->bSuppGo,    iL, y, btnW, rh2, TRUE);
        MoveWindow(t->eSuppPct,   iL+btnW+gp, y, S(48), rh2, TRUE);
        MoveWindow(t->cSuppPanel, iL+btnW+gp+S(54), y, iw-btnW-gp-S(54), rh2, TRUE);
        y += rh2+S(16);
        // --- پزشک معالج group ---
        y += S(26);  // group title
        MoveWindow(t->eDocCode, iL, y, iw, rh2, TRUE);
        y += rh2+gp;
        MoveWindow(t->bDocSearch, iL, y, btnW, rh2, TRUE);
        MoveWindow(t->eDocName,   iL+btnW+gp, y, iw-btnW-gp, rh2, TRUE);
    }
}

// ----- national-id insurance inquiry ---------------------------------------
//  Validate an Iranian 10-digit national code (checksum) and derive a likely
//  basic-insurance organisation.  A "real" online inquiry would POST to the
//  configured server; offline we do the local validation + deterministic
//  mapping so the workflow is identical and ready to swap for a web call.
//  validNationalId() / lookupCitizen() now live in data_ext.cpp (shared with the
//  appointment module). doInquiry() uses lookupCitizen() to auto-fill ALL the
//  patient fields from the (offline) Civil-Registry and to detect when a patient
//  carries 2 or 3 insurances — in which case it announces it and restricts the
//  insurance combo to ONLY those organisations, highlighted in a distinct colour.
static void doInquiry(TabPage* t, HWND h, bool quiet){
    wchar_t b[32]={0}; GetWindowTextW(t->eNid,b,32);
    std::wstring nid=trim(b);
    if(!validNationalId(nid)){
        if(!quiet){
            t->lastMsg=L"کد ملی نامعتبر است (۱۰ رقم و رقم کنترلی صحیح).";
            t->msgCol=g_theme.danger; InvalidateRect(h,NULL,FALSE);
        }
        return;
    }
    CitizenInfo c = lookupCitizen(nid);
    if(!c.found){
        if(!quiet){
            t->lastMsg=L"اطلاعاتی برای این کد ملی یافت نشد.";
            t->msgCol=g_theme.danger; InvalidateRect(h,NULL,FALSE);
        }
        return;
    }
    // auto-fill every empty patient field from the registry
    auto setIfEmpty=[&](HWND e, const std::wstring& v){
        if(v.empty()) return;
        wchar_t cur[256]; GetWindowTextW(e,cur,256);
        if(trim(cur).empty()) SetWindowTextW(e,v.c_str());
    };
    setIfEmpty(t->eFirst, c.firstName);
    setIfEmpty(t->eLast,  c.lastName);
    setIfEmpty(t->eFather,c.fatherName);
    setIfEmpty(t->eMobile,c.mobile);
    setIfEmpty(t->eBirth, c.birthDate);
    SendMessageW(t->cGender,CB_SETCURSEL,(c.gender==L"زن")?1:0,0);

    // ---- insurance handling -------------------------------------------------
    // remember which insurances this patient has, so WM_DRAWITEM on the combo
    // can paint them in a distinct colour, and rebuild the list to show ONLY
    // those organisations when the patient has more than one.
    t->insAllowed = c.insurances;
    SendMessageW(t->cIns,CB_RESETCONTENT,0,0);
    if(c.insurances.size()>=2){
        for(int ix : c.insurances){
            if(ix>=0 && ix<N_INSURANCES)
                SendMessageW(t->cIns,CB_ADDSTRING,0,(LPARAM)INSURANCES[ix].name);
        }
        SendMessageW(t->cIns,CB_SETCURSEL,0,0);
        wchar_t mb[200];
        swprintf(mb,200,L"این بیمار دارای %d بیمه است؛ فقط بیمه‌های او نمایش داده می‌شود.",
            (int)c.insurances.size());
        t->lastMsg=toFaDigits(mb); t->msgCol=g_theme.warn;
    } else {
        // single (or zero) — restore the full list and select the matched one
        for(int i=0;i<N_INSURANCES;i++)
            SendMessageW(t->cIns,CB_ADDSTRING,0,(LPARAM)INSURANCES[i].name);
        int idx = c.insurances.empty()?0:c.insurances[0];
        if(idx<0||idx>=N_INSURANCES) idx=0;
        SendMessageW(t->cIns,CB_SETCURSEL,idx,0);
        t->lastMsg=std::wstring(L"استعلام بیمه: ")+INSURANCES[idx].name;
        t->msgCol=g_theme.success;
    }
    recalc(t);
    InvalidateRect(h,NULL,FALSE);
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

// reset all patient fields of a tab for the NEXT reception (no new tab needed)
static void resetForm(TabPage* t){
    if(!t) return;
    HWND clr[8]={t->eFirst,t->eLast,t->eNid,t->eFather,
                 t->eBirth,t->eMobile,t->ePhone,t->eAddr};
    for(int i=0;i<8;i++) SetWindowTextW(clr[i],L"");
    SendMessageW(t->cGender,CB_SETCURSEL,0,0);
    SendMessageW(t->cPType, CB_SETCURSEL,1,0);   // سرپایی default
    SendMessageW(t->cNType, CB_SETCURSEL,0,0);   // عادی
    // next patient defaults to «دارای بیمه» again
    SendMessageW(t->chkIns, BM_SETCHECK, BST_CHECKED, 0);
    EnableWindow(t->bInquiry, TRUE);
    SetWindowTextW(t->eDiscount,L"");
    SetWindowTextW(t->ePrice,L"");               // recalc auto-fills tariff
    recalc(t);
    t->lastMsg.clear();
    if(t->page){ InvalidateRect(t->page,NULL,FALSE); }
    SetFocus(t->eFirst);
}

// ----- painted page for portal-message / empty tabs (no controls) ----------
//  A clean centred "glass" hero card with a vector icon, a title and a body
//  line. The portal page is the placeholder for future messages pushed by the
//  clinic management panel; the empty page invites the user to start a task.
// ----- the cartable (کارتابل): a real inbox of management messages ----------
static void drawCartable(HDC dc, const RECT& rc){
    gpGradRoundRect(dc,(RECT&)rc,0,g_theme.bg,g_theme.bg2,CLR_INVALID);
    SetBkMode(dc,TRANSPARENT);
    int pad=S(24);
    RECT panel={rc.left+pad, rc.top+pad, rc.right-pad, rc.bottom-pad};
    gpShadow(dc,panel,S(18),S(22),50);
    gpFillAlpha(dc,panel,S(18),g_theme.surfaceTop,235);
    gpRoundRect(dc,panel,S(18),CLR_INVALID,g_theme.border,255);

    // header
    RECT hdr={panel.left,panel.top,panel.right,panel.top+S(56)};
    gpGradRoundRect(dc,hdr,S(18),g_theme.accent2,g_theme.accent,CLR_INVALID);
    RECT hdrB={panel.left,panel.top+S(30),panel.right,panel.top+S(56)};
    gpGradRoundRect(dc,hdrB,0,g_theme.accent2,g_theme.accent,CLR_INVALID);
    RECT bi={panel.right-S(48),panel.top+S(14),panel.right-S(20),panel.top+S(42)};
    drawIcon(dc,ICO_BELL,bi,RGB(255,255,255),S(2));
    SelectObject(dc,g_fTitle); SetTextColor(dc,RGB(255,255,255));
    RECT tr={panel.left+S(20),panel.top+S(10),panel.right-S(56),panel.top+S(46)};
    DrawTextW(dc,L"کارتابل — پیام‌های مدیریت درمانگاه",-1,&tr,
        DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);

    auto msgs=loadMessages(g_session.user.username);
    int y=panel.top+S(70);
    if(msgs.empty()){
        SelectObject(dc,g_fUI); SetTextColor(dc,g_theme.textDim);
        RECT er={panel.left+S(24),y,panel.right-S(24),y+S(40)};
        DrawTextW(dc,L"در حال حاضر پیامی از مدیریت دریافت نشده است.",-1,&er,
            DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
        return;
    }
    // newest first
    for(int i=(int)msgs.size()-1;i>=0&& y<panel.bottom-S(20);i--){
        KMsg& mm=msgs[i];
        int ch=S(60);
        RECT card={panel.left+S(16),y,panel.right-S(16),y+ch};
        // severity colour: عادی=green / فوری=yellow / بحرانی=red
        COLORREF sevCol = mm.type==KMSG_CRITICAL ? g_theme.danger
                        : mm.type==KMSG_URGENT   ? g_theme.warn
                        :                          g_theme.success;
        const wchar_t* sevLbl = mm.type==KMSG_CRITICAL ? L"بحرانی"
                              : mm.type==KMSG_URGENT   ? L"فوری"
                              :                          L"عادی";
        gpRoundRect(dc,card,S(10),
            mm.seen?g_theme.surface:g_theme.surface2,
            mm.seen?g_theme.border:sevCol,255);
        // coloured severity stripe down the right edge (RTL leading edge)
        RECT stripe={card.right-S(6),card.top+S(2),card.right-S(2),card.bottom-S(2)};
        gpRoundRect(dc,stripe,S(2),sevCol,CLR_INVALID,255);
        if(!mm.seen){
            RECT dot={card.right-S(30),card.top+S(8),card.right-S(18),card.top+S(20)};
            gpRoundRect(dc,dot,S(6),sevCol,CLR_INVALID,255);
        }
        SelectObject(dc,g_fUIB); SetTextColor(dc,g_theme.text);
        RECT fr={card.left+S(14),card.top+S(6),card.right-S(40),card.top+S(28)};
        std::wstring from=L"["+std::wstring(sevLbl)+L"] از: "+
            (mm.from.empty()?std::wstring(L"مدیریت"):mm.from)+L"   •   "+mm.time;
        DrawTextW(dc,from.c_str(),-1,&fr,
            DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
        SelectObject(dc,g_fUI); SetTextColor(dc,g_theme.textDim);
        RECT br={card.left+S(14),card.top+S(28),card.right-S(14),card.bottom-S(4)};
        DrawTextW(dc,mm.text.c_str(),-1,&br,
            DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX|DT_END_ELLIPSIS);
        y+=ch+S(8);
    }
}
static void drawTabPlaceholder(HDC dc, const RECT& rc, int kind){
    if(kind==TK_PORTAL){ drawCartable(dc,rc); return; }
    // soft page gradient
    gpGradRoundRect(dc,(RECT&)rc,0,g_theme.bg,g_theme.bg2,CLR_INVALID);

    int cw = S(560); if(cw > rc.right-S(80)) cw = rc.right-S(80);
    int chh= S(300); if(chh> rc.bottom-S(80)) chh= rc.bottom-S(80);
    int cx = rc.right/2, cy = rc.bottom/2;
    RECT card={cx-cw/2, cy-chh/2, cx+cw/2, cy+chh/2};

    gpShadow(dc,card,S(22),S(26),60);
    gpFillAlpha(dc,card,S(22),g_theme.surfaceTop,235);
    gpRoundRect(dc,card,S(22),CLR_INVALID,g_theme.border,255);

    int br=S(40), bx=cx, by=card.top+S(64);
    RECT badge={bx-br,by-br,bx+br,by+br};
    gpGradRoundRect(dc,badge,br,g_theme.accent2,g_theme.accent,CLR_INVALID);
    RECT bi={badge.left+S(18),badge.top+S(18),badge.right-S(18),badge.bottom-S(18)};
    drawIcon(dc, ICO_TAB, bi, RGB(255,255,255), S(3));

    SetBkMode(dc,TRANSPARENT);
    SelectObject(dc,g_fTitle);
    SetTextColor(dc,g_theme.text);
    RECT tr={card.left+S(20), by+br+S(14), card.right-S(20), by+br+S(14)+S(40)};
    DrawTextW(dc,L"تب جدید",-1,&tr,DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);

    SelectObject(dc,g_fUI);
    SetTextColor(dc,g_theme.textDim);
    RECT br1={card.left+S(28), tr.bottom+S(8), card.right-S(28), tr.bottom+S(8)+S(28)};
    DrawTextW(dc,L"برای پذیرش بیمار، روی «پذیرش جدید» کلیک کنید.",-1,&br1,
        DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);

    SelectObject(dc,g_fSmall);
    SetTextColor(dc,g_theme.textDim);
    RECT br2={card.left+S(28), br1.bottom+S(2), card.right-S(28), br1.bottom+S(2)+S(24)};
    DrawTextW(dc,L"این تب برای کارهای آینده آماده است.",-1,&br2,
        DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
}

// ------------------------------------------------- right info-panel paint --
//  Draws a round profile avatar with a GUEST icon coloured by the patient's
//  gender (no real photo), the نسخه الکترونیک / قبض-بارکد chips, the «P : S»
//  counters and the group titles for the search-keys / insurance / doctor
//  blocks. The actual edit controls are positioned by tabPageLayout.
static void drawGuestAvatar(HDC dc, int cx, int cy, int r, bool female){
    COLORREF ring = female? RGB(0xE8,0x6A,0xA6) : RGB(0x4F,0x8E,0xF7);
    // disc
    HBRUSH br=CreateSolidBrush(g_theme.surface2);
    HPEN pn=CreatePen(PS_SOLID,S(2),ring);
    HGDIOBJ ob=SelectObject(dc,br), op=SelectObject(dc,pn);
    Ellipse(dc,cx-r,cy-r,cx+r,cy+r);
    // head + shoulders (guest silhouette)
    HBRUSH brh=CreateSolidBrush(ring);
    SelectObject(dc,brh);
    SelectObject(dc,GetStockObject(NULL_PEN));
    int hr=r*38/100;            // head radius
    int hy=cy-r*22/100;         // head center y
    Ellipse(dc,cx-hr,hy-hr,cx+hr,hy+hr);
    // shoulders arc
    int sw=r*78/100, sy=cy+r*10/100, sh=r*90/100;
    Ellipse(dc,cx-sw,sy,cx+sw,sy+sh*2);
    SelectObject(dc,op); SelectObject(dc,ob);
    DeleteObject(br); DeleteObject(pn); DeleteObject(brh);
}
static void paintInfoGroup(HDC dc, int iL, int iR, int y, const wchar_t* title, int icon){
    SelectObject(dc,g_fUIB); SetTextColor(dc,g_theme.accent);
    int icoW=S(16);
    RECT si={iR-icoW,y+S(1),iR,y+S(17)};
    drawIcon(dc,icon,si,g_theme.accent,S(2));
    RECT sr={iL,y,iR-icoW-S(6),y+S(18)};
    DrawTextW(dc,title,-1,&sr,DT_RIGHT|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);
    HPEN pn=CreatePen(PS_SOLID,1,g_theme.border);
    HGDIOBJ op=SelectObject(dc,pn);
    MoveToEx(dc,iL,y+S(20),0); LineTo(dc,iR,y+S(20));
    SelectObject(dc,op); DeleteObject(pn);
}
static void paintInfoPanel(HDC dc, TabPage* t, int infoL, int infoR, int H){
    RECT card={infoL,S(16),infoR,H-S(16)};
    fillRoundRect(dc,card,S(16),g_theme.surface,g_theme.border);
    int ipad=S(16);
    int iL=infoL+ipad, iR=infoR-ipad;
    // gender from the combo
    bool female = (SendMessageW(t->cGender,CB_GETCURSEL,0,0)==1);
    // --- avatar ---
    int cx=(iL+iR)/2, cy=card.top+S(46), r=S(34);
    drawGuestAvatar(dc,cx,cy,r,female);
    // --- نسخه الکترونیک chip ---
    int y=cy+r+S(10);
    { RECT chip={iL,y,iR,y+S(26)};
      fillRoundRect(dc,chip,S(8),g_theme.accent,CLR_INVALID);
      SelectObject(dc,g_fSmall); SetTextColor(dc,g_theme.accentText);
      DrawTextW(dc,L"نسخه الکترونیک",-1,&chip,
          DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX); }
    y+=S(32);
    // --- قبض - ماه / بارکد ---
    { RECT box={iL,y,iR,y+S(30)};
      fillRoundRect(dc,box,S(8),g_theme.inputBg,g_theme.border);
      SelectObject(dc,g_fSmall); SetTextColor(dc,g_theme.textDim);
      SYSTEMTIME st=iranNow(); int jy,jm,jd;
      gregToJalali(st.wYear,st.wMonth,st.wDay,jy,jm,jd);
      wchar_t b[64]; swprintf(b,64,L"قبض - ماه %d  /  بارکد",jm);
      RECT tr=box; tr.right-=S(8);
      DrawTextW(dc,toFaDigits(b).c_str(),-1,&tr,
          DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX); }
    y+=S(36);
    // --- P : 0   S : 0 counters ---
    { SelectObject(dc,g_fUIB); SetTextColor(dc,g_theme.text);
      int pCount=countTodayReceptions();
      wchar_t b[40]; swprintf(b,40,L"P : %d        S : %d",pCount,detectShift());
      RECT tr={iL,y,iR,y+S(20)};
      DrawTextW(dc,toFaDigits(b).c_str(),-1,&tr,
          DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_NOPREFIX); }
    y+=S(28);
    // group titles align with tabPageLayout's y progression
    paintInfoGroup(dc,iL,iR,y,L"کلیدهای جستجو",ICO_ID);
    int rh2=S(28), gp=S(6);
    y += S(26) + (rh2+gp)*2 + S(16);
    paintInfoGroup(dc,iL,iR,y,L"بیمه",ICO_SHIELD);
    y += S(26) + (rh2+gp)*4 + S(16);
    paintInfoGroup(dc,iL,iR,y,L"پزشک معالج",ICO_CROSS_MED);
    // --- انجام دهنده (current user) at the very bottom ---
    { SelectObject(dc,g_fSmall); SetTextColor(dc,g_theme.textDim);
      std::wstring who=L"انجام دهنده: "+
          (g_session.user.fullname.empty()?g_session.user.username:g_session.user.fullname);
      RECT tr={iL,card.bottom-S(26),iR,card.bottom-S(8)};
      DrawTextW(dc,who.c_str(),-1,&tr,
          DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX); }
}

// ----------------------------------------------------------- tab page proc -
static LRESULT CALLBACK tabPageProc(HWND h, UINT m, WPARAM w, LPARAM l){
    TabPage* t=(TabPage*)GetWindowLongPtrW(h,GWLP_USERDATA);
    switch(m){
    case WM_CREATE: {
        CREATESTRUCTW* cs=(CREATESTRUCTW*)l;
        t=(TabPage*)cs->lpCreateParams;
        SetWindowLongPtrW(h,GWLP_USERDATA,(LONG_PTR)t);
        t->page=h;
        // The appointment tab hosts the self-contained نوبت‌دهی page as a child
        // that fills the whole tab area.
        if(t->kind==TK_APPOINTMENT){
            t->appt = createAppointmentPage(h);
            if(t->appt) ShowWindow(t->appt, SW_SHOW);
            return 0;
        }
        // Portal-message and empty tabs are pure painted pages with no form
        // controls — they own no edit boxes, combos or buttons.
        if(t->kind!=TK_RECEPTION) return 0;
        DWORD es=WS_CHILD|WS_VISIBLE|WS_TABSTOP|ES_AUTOHSCROLL;
        DWORD cbs=WS_CHILD|WS_VISIBLE|WS_TABSTOP|CBS_DROPDOWNLIST;
        t->eFirst =CreateWindowExW(0,L"EDIT",L"",es,0,0,10,10,h,(HMENU)(ID_F_FIRST+0),g_hInst,0);
        t->eLast  =CreateWindowExW(0,L"EDIT",L"",es,0,0,10,10,h,(HMENU)(ID_F_FIRST+1),g_hInst,0);
        t->eNid   =CreateWindowExW(0,L"EDIT",L"",es|ES_NUMBER,0,0,10,10,h,(HMENU)(ID_F_FIRST+2),g_hInst,0);
        t->eFather=CreateWindowExW(0,L"EDIT",L"",es,0,0,10,10,h,(HMENU)(ID_F_FIRST+3),g_hInst,0);
        t->eBirth =CreateWindowExW(0,L"EDIT",L"",es,0,0,10,10,h,(HMENU)(ID_F_FIRST+4),g_hInst,0);
        t->cGender=createThemedCombo(h,ID_F_GENDER);
        t->eMobile=CreateWindowExW(0,L"EDIT",L"",es,0,0,10,10,h,(HMENU)(ID_F_FIRST+5),g_hInst,0);
        t->ePhone =CreateWindowExW(0,L"EDIT",L"",es,0,0,10,10,h,(HMENU)(ID_F_FIRST+6),g_hInst,0);
        t->eAddr  =CreateWindowExW(WS_EX_RTLREADING|WS_EX_RIGHT,L"EDIT",L"",es,0,0,10,10,h,(HMENU)(ID_F_FIRST+7),g_hInst,0);
        t->cPType =createThemedCombo(h,ID_F_PTYPE);
        t->cNType =createThemedCombo(h,ID_F_NTYPE);
        t->cIns   =createThemedCombo(h,ID_F_INS);
        t->cSupp  =createThemedCombo(h,ID_F_SUPP);
        (void)cbs;
        t->ePrice =CreateWindowExW(0,L"EDIT",L"",es,0,0,10,10,h,(HMENU)ID_F_PRICE,g_hInst,0);
        t->eDiscount=CreateWindowExW(0,L"EDIT",L"",es,0,0,10,10,h,(HMENU)ID_F_DISCOUNT,g_hInst,0);
        // «دارای بیمه» checkbox — placed beside the national-id field; checked by
        // default so Enter on the id field runs the validated insurance inquiry.
        // Unchecking it switches to manual insurance selection (no auto-inquiry).
        t->chkIns =CreateWindowExW(0,L"BUTTON",L"دارای بیمه",
            WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX|BS_RIGHTBUTTON|BS_RIGHT,
            0,0,10,10,h,(HMENU)ID_F_HASINS,g_hInst,0);
        SendMessageW(t->chkIns,WM_SETFONT,(WPARAM)g_fSmall,TRUE);
        SendMessageW(t->chkIns,BM_SETCHECK,BST_CHECKED,0);

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
        t->bInquiry=createFlatButton(h,ID_F_INQUIRY,L"استعلام بیمه",ICO_SHIELD,BS_OUTLINE,0,0,10,10);
        // v1.4.1: real raster icons on the print-action buttons (left card)
        setFlatButtonImage(t->bPrtIns, IMG_IC_SHIELD);
        setFlatButtonImage(t->bPrtRx,  IMG_IC_RECEIPT);
        setFlatButtonImage(t->bPrtLast,IMG_IC_LAST);
        // blend button corners: submit sits on the form card (surface); the
        // billing/print buttons sit on the page background (bg).
        setFlatButtonBg(t->bSubmit, g_theme.surface);
        setFlatButtonBg(t->bPrtIns, g_theme.surface);
        setFlatButtonBg(t->bPrtRx,  g_theme.surface);
        setFlatButtonBg(t->bPrtLast,g_theme.surface);
        setFlatButtonBg(t->bClose,  g_theme.surface);
        setFlatButtonBg(t->bInquiry,g_theme.surface);

        // birth-date uses the smart Jalali mask (digits-only, auto slashes)
        HWND eds[11]={t->eFirst,t->eLast,t->eNid,t->eFather,
                      t->eMobile,t->ePhone,t->eAddr,t->ePrice,t->eDiscount,0,0};
        for(int i=0;eds[i];i++){
            SendMessageW(eds[i],WM_SETFONT,(WPARAM)g_fUI,TRUE);
            enableEnterNavigation(eds[i]);
        }
        // auto RTL/LTR alignment on the free-text fields (names/address):
        // Persian content aligns right, Latin/digits align left.
        enableAutoDir(t->eFirst); enableAutoDir(t->eLast);
        enableAutoDir(t->eFather); enableAutoDir(t->eAddr);
        SendMessageW(t->eBirth,WM_SETFONT,(WPARAM)g_fUI,TRUE);
        SendMessageW(t->eBirth,EM_SETLIMITTEXT,10,0);
        enableDateMask(t->eBirth);
        HWND cbsArr[5]={t->cGender,t->cPType,t->cNType,t->cIns,t->cSupp};
        for(int i=0;i<5;i++)
            SendMessageW(cbsArr[i],WM_SETFONT,(WPARAM)g_fUI,TRUE);
        // default patient type = سرپایی (most common), triggers tariff auto-fill
        SendMessageW(t->cPType,CB_SETCURSEL,1,0);

        // ====================== RIGHT INFO PANEL (v1.6.0) ====================
        // search keys
        t->eArchive  =CreateWindowExW(0,L"EDIT",L"",es|ES_NUMBER,0,0,10,10,h,(HMENU)ID_F_ARCHIVE,g_hInst,0);
        t->bArchiveGo=createFlatButton(h,ID_F_ARCHIVE_GO,L"استعلام",ICO_NONE,BS_OUTLINE,0,0,10,10);
        t->eFile     =CreateWindowExW(0,L"EDIT",L"",es|ES_NUMBER,0,0,10,10,h,(HMENU)ID_F_FILE,g_hInst,0);
        t->bFileGo   =createFlatButton(h,ID_F_FILE_GO,L"استعلام",ICO_NONE,BS_OUTLINE,0,0,10,10);
        // insurance block
        t->eBookNo   =CreateWindowExW(0,L"EDIT",L"",es|ES_NUMBER,0,0,10,10,h,(HMENU)ID_F_BOOKNO,g_hInst,0);
        t->chkBookNo =CreateWindowExW(0,L"BUTTON",L"فعال",
            WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX|BS_RIGHTBUTTON|BS_RIGHT,0,0,10,10,h,(HMENU)ID_F_BOOKNO_CHK,g_hInst,0);
        t->eValid    =CreateWindowExW(0,L"EDIT",L"",es,0,0,10,10,h,(HMENU)ID_F_VALID,g_hInst,0);
        t->chkValidAuto=CreateWindowExW(0,L"BUTTON",L"اتوماتیک",
            WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX|BS_RIGHTBUTTON|BS_RIGHT,0,0,10,10,h,(HMENU)ID_F_VALID_AUTO,g_hInst,0);
        t->eRxDate   =CreateWindowExW(0,L"EDIT",L"",es,0,0,10,10,h,(HMENU)ID_F_RXDATE,g_hInst,0);
        t->chkRxAuto =CreateWindowExW(0,L"BUTTON",L"اتوماتیک",
            WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX|BS_RIGHTBUTTON|BS_RIGHT,0,0,10,10,h,(HMENU)ID_F_RXDATE_AUTO,g_hInst,0);
        t->cSuppPanel=createThemedCombo(h,ID_F_SUPP_PANEL);
        t->eSuppPct  =CreateWindowExW(0,L"EDIT",L"",es|ES_NUMBER,0,0,10,10,h,(HMENU)ID_F_SUPP_PCT,g_hInst,0);
        t->bSuppGo   =createFlatButton(h,ID_F_SUPP_GO,L"استعلام",ICO_NONE,BS_OUTLINE,0,0,10,10);
        // پزشک معالج
        t->eDocCode  =CreateWindowExW(0,L"EDIT",L"",es|ES_NUMBER,0,0,10,10,h,(HMENU)ID_F_DOCCODE,g_hInst,0);
        t->eDocName  =CreateWindowExW(0,L"EDIT",L"",es,0,0,10,10,h,(HMENU)ID_F_DOCNAME,g_hInst,0);
        t->bDocSearch=createFlatButton(h,ID_F_DOCSEARCH,L"جستجو",ICO_NONE,BS_OUTLINE,0,0,10,10);
        // fonts + behaviour
        HWND infoEds[]={t->eArchive,t->eFile,t->eBookNo,t->eValid,t->eRxDate,
                        t->eSuppPct,t->eDocCode,t->eDocName};
        for(HWND e: infoEds){ SendMessageW(e,WM_SETFONT,(WPARAM)g_fUI,TRUE); enableEnterNavigation(e); }
        enableAutoDir(t->eDocName);
        SendMessageW(t->eValid,EM_SETLIMITTEXT,10,0);  enableDateMask(t->eValid);
        SendMessageW(t->eRxDate,EM_SETLIMITTEXT,10,0); enableDateMask(t->eRxDate);
        SendMessageW(t->cSuppPanel,WM_SETFONT,(WPARAM)g_fUI,TRUE);
        for(int i=0;i<N_SUPP;i++)
            SendMessageW(t->cSuppPanel,CB_ADDSTRING,0,(LPARAM)SUPP_INSURANCES[i].name);
        SendMessageW(t->cSuppPanel,CB_SETCURSEL,0,0);
        HWND infoChks[]={t->chkBookNo,t->chkValidAuto,t->chkRxAuto};
        for(HWND c: infoChks) SendMessageW(c,WM_SETFONT,(WPARAM)g_fSmall,TRUE);
        // ش دفترچه disabled until its checkbox is ticked
        EnableWindow(t->eBookNo,FALSE);
        SendMessageW(t->chkValidAuto,BM_SETCHECK,BST_CHECKED,0);
        SendMessageW(t->chkRxAuto,BM_SETCHECK,BST_CHECKED,0);
        // اتوماتیک = current Iran date/time
        SetWindowTextW(t->eValid, jalaliDateShort(iranNow()).c_str());
        SetWindowTextW(t->eRxDate,jalaliDateShort(iranNow()).c_str());
        EnableWindow(t->eValid,FALSE); EnableWindow(t->eRxDate,FALSE);
        // doctor inquiry buttons sit on the surface (info panel) — blend corners
        setFlatButtonBg(t->bArchiveGo,g_theme.surface);
        setFlatButtonBg(t->bFileGo,   g_theme.surface);
        setFlatButtonBg(t->bSuppGo,   g_theme.surface);
        setFlatButtonBg(t->bDocSearch,g_theme.surface);

        recalc(t);
        return 0; }
    case WM_SIZE: if(t) tabPageLayout(h,t); return 0;
    case WM_APP_THEME:
        if(t && t->kind==TK_RECEPTION){
            setFlatButtonBg(t->bSubmit, g_theme.surface);
            setFlatButtonBg(t->bPrtIns, g_theme.surface);
            setFlatButtonBg(t->bPrtRx,  g_theme.surface);
            setFlatButtonBg(t->bPrtLast,g_theme.surface);
            setFlatButtonBg(t->bClose,  g_theme.surface);
            setFlatButtonBg(t->bInquiry,g_theme.surface);
            setFlatButtonBg(t->bArchiveGo,g_theme.surface);
            setFlatButtonBg(t->bFileGo,   g_theme.surface);
            setFlatButtonBg(t->bSuppGo,   g_theme.surface);
            setFlatButtonBg(t->bDocSearch,g_theme.surface);
        }
        InvalidateRect(h,NULL,TRUE);
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
        // Checkboxes sit on a card (surface) — paint their labels with surface
        // bg + normal text so they blend with the card (no white box).
        if(t && ((HWND)l==t->chkIns || (HWND)l==t->chkBookNo ||
                 (HWND)l==t->chkValidAuto || (HWND)l==t->chkRxAuto)){
            SetTextColor(dc,g_theme.text); SetBkColor(dc,g_theme.surface);
            return (LRESULT)g_brSurface;
        }
        // CBS_DROPDOWNLIST combos paint their closed display via STATIC color —
        // theme it so the selected text isn't white-on-white in dark mode.
        SetTextColor(dc,g_theme.inputText); SetBkColor(dc,g_theme.inputBg);
        return (LRESULT)g_brInput; }
    case WM_DRAWITEM: {
        // theme-aware owner-draw combobox items (fixes dark-mode dropdown)
        if(drawThemedComboItem((LPDRAWITEMSTRUCT)l)) return TRUE;
        break; }
    case WM_COMMAND: {
        if(!t) return 0;
        int id=LOWORD(w), code=HIWORD(w);
        if((id==ID_F_INS||id==ID_F_SUPP) && code==CBN_SELCHANGE){
            recalc(t); InvalidateRect(h,NULL,FALSE);
        }
        else if((id==ID_F_PTYPE||id==ID_F_NTYPE) && code==CBN_SELCHANGE){
            // patient/appointment type changed → re-derive the default tariff
            SetWindowTextW(t->ePrice, L"");     // clear so recalc auto-fills
            recalc(t); InvalidateRect(h,NULL,FALSE);
        }
        else if(id==ID_F_PRICE && code==EN_CHANGE){
            if(!t->autoPrice){ recalc(t); InvalidateRect(h,NULL,FALSE); }
        }
        else if(id==ID_F_DISCOUNT && code==EN_CHANGE){
            recalc(t); InvalidateRect(h,NULL,FALSE);
        }
        else if(id==(ID_F_FIRST+2) && code==EN_KILLFOCUS){
            // national-id field lost focus (Enter/Tab navigates away). If the
            // patient is marked as insured, run the validated inquiry and show an
            // error on an invalid id; otherwise leave the manual list untouched.
            bool insured = SendMessageW(t->chkIns,BM_GETCHECK,0,0)==BST_CHECKED;
            if(insured) doInquiry(t,h,false);   // show invalid-id error
        }
        else if(id==ID_F_INQUIRY){
            // manual «استعلام بیمه» button — only meaningful when insured
            if(SendMessageW(t->chkIns,BM_GETCHECK,0,0)==BST_CHECKED)
                doInquiry(t,h,false);
            else {
                t->lastMsg=L"برای استعلام، گزینه «دارای بیمه» را فعال کنید.";
                t->msgCol=g_theme.warn; InvalidateRect(h,NULL,FALSE);
            }
        }
        else if(id==ID_F_HASINS && code==BN_CLICKED){
            // toggling insured state: when unchecked, reset to free/«آزاد» so the
            // user picks insurance manually; recalc + redraw either way.
            bool insured = SendMessageW(t->chkIns,BM_GETCHECK,0,0)==BST_CHECKED;
            EnableWindow(t->bInquiry, insured);
            if(!insured){
                SendMessageW(t->cIns,CB_SETCURSEL,0,0);   // index 0 = آزاد/بدون بیمه
                t->lastMsg=L"حالت بدون بیمه — بیمه را به‌صورت دستی انتخاب کنید.";
                t->msgCol=g_theme.textDim;
            } else {
                t->lastMsg=L"حالت دارای بیمه — کد ملی را وارد و Enter بزنید.";
                t->msgCol=g_theme.accent;
            }
            recalc(t); InvalidateRect(h,NULL,FALSE);
        }
        else if(id==ID_F_SUBMIT){
            ReceptionRecord r; collect(t,r);
            // v1.4.0 validation: REQUIRED = first/last name, national id, mobile,
            // birth date.  OPTIONAL (may be empty) = landline, address, discount.
            if(r.firstName.empty()||r.lastName.empty()){
                t->lastMsg=L"نام و نام خانوادگی بیمار الزامی است.";
                t->msgCol=g_theme.danger;
            } else if(r.nationalId.empty()){
                t->lastMsg=L"کد ملی بیمار الزامی است.";
                t->msgCol=g_theme.danger;
            } else if(r.mobile.empty()){
                t->lastMsg=L"تلفن همراه بیمار الزامی است.";
                t->msgCol=g_theme.danger;
            } else if(r.birthDate.empty()){
                t->lastMsg=L"تاریخ تولد بیمار الزامی است.";
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
                // v1.4.0: prefer the user-designed layout for this section; fall
                // back to the classic GDI receipt if no design exists.
                auto doPrint=[&](const ReceptionRecord& rec){
                    if(!printDesignedReceipt(rec,0,h)) printReceipt(rec,2,h);
                };
                if(getSetting(L"auto_print",L"0")==L"1"){
                    doPrint(r);
                } else if(MessageBoxW(h,L"پذیرش ثبت شد. قبض چاپ شود؟",
                    L"ثبت موفق",MB_YESNO|MB_ICONQUESTION)==IDYES){
                    doPrint(r);
                }
                // reset patient fields for next reception (same tab)
                std::wstring keep = t->lastMsg; COLORREF kc = t->msgCol;
                resetForm(t);
                t->lastMsg = keep; t->msgCol = kc;   // keep success message
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
        // ---- right info-panel handlers ----
        else if(id==ID_F_BOOKNO_CHK && code==BN_CLICKED){
            bool on=SendMessageW(t->chkBookNo,BM_GETCHECK,0,0)==BST_CHECKED;
            EnableWindow(t->eBookNo,on);
            if(on) SetFocus(t->eBookNo);
        }
        else if(id==ID_F_VALID_AUTO && code==BN_CLICKED){
            bool on=SendMessageW(t->chkValidAuto,BM_GETCHECK,0,0)==BST_CHECKED;
            EnableWindow(t->eValid,!on);
            if(on) SetWindowTextW(t->eValid,jalaliDateShort(iranNow()).c_str());
        }
        else if(id==ID_F_RXDATE_AUTO && code==BN_CLICKED){
            bool on=SendMessageW(t->chkRxAuto,BM_GETCHECK,0,0)==BST_CHECKED;
            EnableWindow(t->eRxDate,!on);
            if(on) SetWindowTextW(t->eRxDate,jalaliDateShort(iranNow()).c_str());
        }
        else if(id==ID_F_ARCHIVE_GO || id==ID_F_FILE_GO){
            // archive/file-number inquiry: reuse the national-id inquiry path so a
            // found record auto-fills the patient block (placeholder mapping).
            doInquiry(t,h,true);
            t->lastMsg=L"استعلام انجام شد."; t->msgCol=g_theme.accent;
            InvalidateRect(h,NULL,FALSE);
        }
        else if(id==ID_F_SUPP_GO){
            int si=(int)SendMessageW(t->cSuppPanel,CB_GETCURSEL,0,0);
            if(si>=0 && si<N_SUPP){
                SendMessageW(t->cSupp,CB_SETCURSEL,si,0);
                wchar_t pb[16]; swprintf(pb,16,L"%d",SUPP_INSURANCES[si].pct);
                SetWindowTextW(t->eSuppPct,toFaDigits(pb).c_str());
                recalc(t); InvalidateRect(h,NULL,FALSE);
            }
        }
        else if(id==ID_F_DOCSEARCH){
            t->lastMsg=L"جستجوی پزشک معالج در دست توسعه است.";
            t->msgCol=g_theme.textDim; InvalidateRect(h,NULL,FALSE);
        }
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

        // -------- Appointment tab: a child page fills it, just clear bg -------
        if(t && t->kind==TK_APPOINTMENT){
            BitBlt(dc0,0,0,rc.right,rc.bottom,dc,0,0,SRCCOPY);
            SelectObject(dc,obm); DeleteObject(bmp); DeleteDC(dc);
            EndPaint(h,&ps);
            return 0;
        }
        // -------- Portal-message / empty tabs: a centred glass card ----------
        if(t && t->kind!=TK_RECEPTION){
            drawTabPlaceholder(dc,rc,t->kind);
            BitBlt(dc0,0,0,rc.right,rc.bottom,dc,0,0,SRCCOPY);
            SelectObject(dc,obm); DeleteObject(bmp); DeleteDC(dc);
            EndPaint(h,&ps);
            return 0;
        }

        int cardL,cardR,billL,billR,infoL,infoR,colW,xr,xl; bool stacked;
        rcMetrics2(rc.right,cardL,cardR,billL,billR,infoL,infoR,colW,xr,xl,stacked);
        int y0,step,rh2; rcVMetrics(rc.bottom,y0,step,rh2);
        int formLeft = cardL+S(RC_IN), formRight = cardR-S(RC_IN);
        int fw = formRight-formLeft;

        // ============ RIGHT INFO PANEL ============
        if(!stacked && infoR>infoL) paintInfoPanel(dc,t,infoL,infoR,rc.bottom);

        // ============ FORM CARD (left, wide) ============
        RECT fcard={cardL,S(16),cardR,rc.bottom-S(16)};
        fillRoundRect(dc,fcard,S(16),g_theme.surface,g_theme.border);
        // card header bar (vector icon + title — no emoji font dependency).
        // The icon sits flush at the right edge; the title's RIGHT edge stops a
        // clear gap to the LEFT of the icon so they never overlap (RTL layout).
        { int icoW=S(24), gap=S(10);
          RECT hi={formRight-icoW,fcard.top+S(16),formRight,fcard.top+S(40)};
          drawIcon(dc,ICO_USER,hi,g_theme.accent,S(2));
          SetTextColor(dc,g_theme.text);
          SelectObject(dc,g_fTitle);
          RECT ht={formLeft,fcard.top+S(14),formRight-icoW-gap,fcard.top+S(46)};
          DrawTextW(dc,L"مشخصات و پذیرش بیمار",-1,&ht,
              DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX); }
        // header separator
        { HPEN pn=CreatePen(PS_SOLID,1,g_theme.border);
          HGDIOBJ op=SelectObject(dc,pn);
          MoveToEx(dc,formLeft,fcard.top+S(52),0); LineTo(dc,formRight,fcard.top+S(52));
          SelectObject(dc,op); DeleteObject(pn); }

        // --- section captions (accent caption + vector icon above group) ---
        struct SEC{const wchar_t* s; int row; int icon;};
        SEC secs[4]={ {L"هویت بیمار",0,ICO_ID},
                      {L"اطلاعات تماس",3,ICO_PHONE},
                      {L"نوبت و بیمه",5,ICO_SHIELD},
                      {L"مبلغ و تخفیف",7,ICO_RECEIPT} };
        SelectObject(dc,g_fUIB);
        for(int i=0;i<4;i++){
            int sy=y0+secs[i].row*step-S(44);
            // icon flush-right, caption text stops well to its LEFT (no overlap)
            int icoW=S(18), gap=S(8);
            RECT si={formRight-icoW,sy+S(1),formRight,sy+S(19)};
            drawIcon(dc,secs[i].icon,si,g_theme.accent,S(2));
            SetTextColor(dc,g_theme.accent);
            RECT sr={formLeft,sy,formRight-icoW-gap,sy+S(20)};
            DrawTextW(dc,secs[i].s,-1,&sr,DT_RIGHT|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);
        }

        // --- framed input wells behind every field (modern flat look) ---
        {
            HWND inputs[16]={t->eFirst,t->eLast,t->eNid,t->eFather,t->eBirth,
                t->cGender,t->eMobile,t->ePhone,t->eAddr,t->cPType,t->cNType,
                t->cIns,t->cSupp,t->ePrice,t->eDiscount,0};
            HWND foc=GetFocus();
            for(int i=0;inputs[i];i++){
                RECT wr; GetWindowRect(inputs[i],&wr);
                POINT a={wr.left,wr.top}, b={wr.right,wr.bottom};
                ScreenToClient(h,&a); ScreenToClient(h,&b);
                int minH = a.y + (rh2>S(28)?rh2:S(34));
                RECT well={a.x-S(7),a.y-S(4),b.x+S(7),
                           (b.y<minH)?minH:b.y+S(4)};
                if(b.y<=a.y) continue;        // not yet laid out
                bool focused = (inputs[i]==foc);
                fillRoundRect(dc,well,S(8),g_theme.inputBg,
                    focused?g_theme.accent:g_theme.border);
            }
        }

        // field labels (above each control)
        SelectObject(dc,g_fSmall);
        SetTextColor(dc,g_theme.textDim);
        struct LBL{const wchar_t* s;int x,y,w;};
        LBL Ls[16]={
            {L"نام",xr,y0-S(20),colW},{L"نام خانوادگی",xl,y0-S(20),colW},
            {L"کد ملی",xr,y0+step-S(20),colW},{L"نام پدر",xl,y0+step-S(20),colW},
            {L"تاریخ تولد (مثال: ۱۳۴۰ ۵ ۲۰)",xr,y0+2*step-S(20),colW},{L"جنسیت",xl,y0+2*step-S(20),colW},
            {L"تلفن همراه",xr,y0+3*step-S(20),colW},{L"تلفن ثابت",xl,y0+3*step-S(20),colW},
            {L"آدرس",formLeft,y0+4*step-S(20),fw},{NULL,0,0,0},
            {L"نوع بیمار",xr,y0+5*step-S(20),colW},{L"نوع نوبت",xl,y0+5*step-S(20),colW},
            {L"بیمه اصلی",xr,y0+6*step-S(20),colW},{L"بیمه مکمل",xl,y0+6*step-S(20),colW},
            {L"مبلغ خدمت (ریال)",xr,y0+7*step-S(20),colW},{L"تخفیف (ریال)",xl,y0+7*step-S(20),colW},
        };
        for(int i=0;i<16;i++){
            if(!Ls[i].s) continue;
            RECT lr={Ls[i].x,Ls[i].y,Ls[i].x+Ls[i].w,Ls[i].y+S(18)};
            DrawTextW(dc,Ls[i].s,-1,&lr,DT_RIGHT|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);
        }
        // auto date note (chip)
        SYSTEMTIME st=iranNow();
        std::wstring dn=L"تاریخ نوبت (خودکار): "+toFaDigits(jalaliDateShort(st))
            +L"   \u2014   ساعت "+toFaDigits(iranTimeStr(st,false))
            +L"   \u2014   شیفت "+shiftName(g_session.shift);
        SetTextColor(dc,g_theme.accent);
        SelectObject(dc,g_fSmall);
        { int icoW=S(16), gap=S(8);
          RECT di={formRight-icoW,y0+8*step+S(62),formRight,y0+8*step+S(78)};
          drawIcon(dc,ICO_CAL,di,g_theme.accent,S(2));
          RECT dnr={formLeft,y0+8*step+S(62),formRight-icoW-gap,y0+8*step+S(84)};
          DrawTextW(dc,dn.c_str(),-1,&dnr,DT_RIGHT|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX); }
        // status message
        if(t && !t->lastMsg.empty()){
            SetTextColor(dc,t->msgCol);
            SelectObject(dc,g_fUIB);
            RECT mr2={formLeft,y0+8*step+S(86),formRight,y0+8*step+S(132)};
            DrawTextW(dc,t->lastMsg.c_str(),-1,&mr2,
                DT_RIGHT|DT_WORDBREAK|DT_RTLREADING|DT_NOPREFIX);
        }

        // ============ BILLING CARD (pinned to the right edge) ============
        if(!stacked && t){
            recalc(t);
            RECT card={billL,S(16),billR,rc.bottom-S(16)};
            fillRoundRect(dc,card,S(16),g_theme.surface,g_theme.border);
            // header (icon flush-right, title to its left — no overlap)
            { int icoW=S(24), gap=S(10);
              RECT bi={card.right-S(16)-icoW,card.top+S(16),card.right-S(16),card.top+S(40)};
              drawIcon(dc,ICO_RECEIPT,bi,g_theme.accent,S(2));
              SetTextColor(dc,g_theme.text);
              SelectObject(dc,g_fTitle);
              RECT bt={card.left+S(16),card.top+S(14),card.right-S(16)-icoW-gap,card.top+S(46)};
              DrawTextW(dc,L"صدور قبض",-1,&bt,
                  DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX); }
            { HPEN pn=CreatePen(PS_SOLID,1,g_theme.border);
              HGDIOBJ op=SelectObject(dc,pn);
              MoveToEx(dc,card.left+S(16),card.top+S(52),0);
              LineTo(dc,card.right-S(16),card.top+S(52));
              SelectObject(dc,op); DeleteObject(pn); }

            // helper to draw a key/value money row
            auto money=[&](int& ry,const wchar_t* k,long long v,
                           bool head,bool strong){
                if(head){
                    SetTextColor(dc,g_theme.accent);
                    SelectObject(dc,g_fUIB);
                    RECT hr={card.left+S(16),ry,card.right-S(16),ry+S(24)};
                    DrawTextW(dc,k,-1,&hr,DT_RIGHT|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);
                    ry+=S(30); return;
                }
                SetTextColor(dc, strong?g_theme.success:g_theme.textDim);
                SelectObject(dc, strong?g_fUIB:g_fUI);
                RECT kr={card.left+S(16),ry,card.right-S(16),ry+S(24)};
                DrawTextW(dc,k,-1,&kr,DT_RIGHT|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);
                SetTextColor(dc, strong?g_theme.success:g_theme.text);
                std::wstring v2=toFaDigits(formatMoney(v))+L" ریال";
                DrawTextW(dc,v2.c_str(),-1,&kr,DT_LEFT|DT_SINGLELINE|DT_NOPREFIX);
                ry+= strong?S(30):S(26);
            };
            auto sep=[&](int& ry){
                HPEN pn=CreatePen(PS_DOT,1,g_theme.border);
                HGDIOBJ op=SelectObject(dc,pn);
                MoveToEx(dc,card.left+S(16),ry+S(2),0);
                LineTo(dc,card.right-S(16),ry+S(2));
                SelectObject(dc,op); DeleteObject(pn);
                ry+=S(12);
            };

            int ry=card.top+S(62);
            money(ry,L"بیمه اصلی",0,true,false);
            money(ry,L"سهم بیمه",t->mainShare,false,false);
            money(ry,L"جمع کل",t->total,false,false);
            money(ry,L"سهم بیمار",t->patientShare,false,false);
            sep(ry);
            money(ry,L"بیمه مکمل",0,true,false);
            money(ry,L"مابه‌التفاوت پایه",t->baseDiff,false,false);
            money(ry,L"سهم سازمان",t->orgShare,false,false);
            sep(ry);
            money(ry,L"مبلغ نهایی",0,true,false);
            money(ry,L"جمع کل",t->total,false,false);
            money(ry,L"تخفیف",t->patientShare - t->paid,false,false);
            // big "payable" highlight chip
            RECT pay={card.left+S(16),ry+S(2),card.right-S(16),ry+S(46)};
            fillRoundRect(dc,pay,S(10),
                g_dark?RGB(20,52,40):RGB(232,248,240),g_theme.success);
            SetTextColor(dc,g_theme.success);
            SelectObject(dc,g_fUIB);
            RECT pk={pay.left+S(10),pay.top,pay.right-S(10),pay.bottom};
            DrawTextW(dc,L"پرداختی",-1,&pk,DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
            std::wstring pv=toFaDigits(formatMoney(t->paid))+L" ریال";
            DrawTextW(dc,pv.c_str(),-1,&pk,DT_LEFT|DT_SINGLELINE|DT_VCENTER|DT_NOPREFIX);
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
static void addTabKind(HWND h, int kind){
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
    t->kind=kind;
    std::wstring dept=g_session.user.dept;
    if(kind==TK_PORTAL)         t->title=L"کارتابل";
    else if(kind==TK_EMPTY)     t->title=L"تب جدید";
    else if(kind==TK_APPOINTMENT) t->title=L"نوبت‌دهی";
    else                        t->title=L"پذیرش بیمار";
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
    if(t->kind==TK_RECEPTION && t->eFirst) SetFocus(t->eFirst);
    else SetFocus(h);
}
static void addTab(HWND h){ addTabKind(h, TK_RECEPTION); }
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
        s_rd->bNewPat = createFlatButton(h,ID_RC_NEWPAT,L"پذیرش جدید",ICO_PLUS,BS_PRIMARY,0,0,10,10);
        s_rd->bNewTab = createFlatButton(h,ID_RC_NEWTAB,L"تب جدید",ICO_TAB,BS_OUTLINE,0,0,10,10);
        s_rd->bCalc = NULL;   // calculator moved to the frame header (left side)
        // blend the button corners into the info-bar surface (no white halo)
        setFlatButtonBg(s_rd->bNewPat,g_theme.surface2);
        setFlatButtonBg(s_rd->bNewTab,g_theme.surface2);
        s_rd->lastUnseen = unseenMessageCount(g_session.user.username);
        SetTimer(h, 77, 5000, NULL);   // poll the cartable for new messages
        return 0;
    case WM_APP_THEME:
        if(s_rd){
            setFlatButtonBg(s_rd->bNewPat,g_theme.surface2);
            setFlatButtonBg(s_rd->bNewTab,g_theme.surface2);
        }
        InvalidateRect(h,NULL,TRUE);
        return 0;
    case WM_TIMER:
        if(w==77 && s_rd){
            int n=unseenMessageCount(g_session.user.username);
            if(n>s_rd->lastUnseen){
                // new message → Windows notification sound + repaint cartable
                if(getSetting(L"notify",L"1")==L"1")
                    MessageBeep(MB_ICONASTERISK);
                InvalidateRect(h,NULL,FALSE);
                // repaint the cartable page itself if visible
                for(auto* tp : s_rd->tabs)
                    if(tp->kind==TK_PORTAL && tp->page) InvalidateRect(tp->page,NULL,FALSE);
            }
            s_rd->lastUnseen=n;
        }
        return 0;
    case WM_NCDESTROY:
        KillTimer(h,77);
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
        RECT rc; GetClientRect(h,&rc);
        int bh=S(38), y=(infoBarH()-bh)/2;
        // LAYER 2 action buttons anchored to the RIGHT edge (RTL):
        // پذیرش جدید (right-most) → تب جدید → ماشین حساب
        int x = rc.right - S(14);
        int wNew=S(150), wTab=S(120), g=S(8);
        MoveWindow(s_rd->bNewPat, x-wNew,                 y, wNew,  bh, TRUE);
        MoveWindow(s_rd->bNewTab, x-wNew-g-wTab,          y, wTab,  bh, TRUE);
        recLayoutTabs(h);
        return 0; }
    case WM_COMMAND: {
        int id=LOWORD(w);
        if(id==ID_RC_CALC) openCalculator(g_hFrame);
        else if(id==ID_RC_NEWTAB) addTabKind(h, TK_EMPTY);   // new-tab → empty
        else if(id==ID_RC_NEWPAT){
            // "پذیرش جدید" — reuse the ACTIVE tab ONLY if it is already a
            // reception form; otherwise open a fresh reception tab (so the
            // portal/empty pages are never overwritten unexpectedly).
            if(s_rd && s_rd->active>=0 && s_rd->active<(int)s_rd->tabs.size()
               && s_rd->tabs[s_rd->active]->kind==TK_RECEPTION)
                resetForm(s_rd->tabs[s_rd->active]);
            else
                addTab(h);   // open a new reception tab
        }
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
            // the cartable (پرتابل/کارتابل) tab is permanent — never closes
            if(part==1 && t->kind==TK_PORTAL){
                s_rd->active=hit; markMessagesSeen(g_session.user.username);
                recLayoutTabs(h); InvalidateRect(h,NULL,FALSE);
            }
            else if(part==1) closeTab(t);
            else if(part==2 && !t->detached && t->kind!=TK_PORTAL) detachTab(t);
            else if(!t->detached){
                s_rd->active=hit; recLayoutTabs(h);
                if(t->kind==TK_PORTAL){
                    markMessagesSeen(g_session.user.username);
                    if(s_rd) s_rd->lastUnseen=0;
                    if(t->page) InvalidateRect(t->page,NULL,FALSE);
                }
                InvalidateRect(h,NULL,FALSE);
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
        // NO username is shown here (privacy). The logged-in person's full name
        // and role already live in the frame's Layer-1 header. Here we only show
        // a friendly section title + the live shift, so the action buttons on
        // the right (Layer-2) have room.
        SYSTEMTIME st=iranNow();
        SelectObject(dc,g_fUIB);
        SetTextColor(dc,g_theme.text);
        std::wstring info = L"میز پذیرش بیمار";
        RECT ir={rc.right-S(420),0,rc.right-S(14),infoBarH()};
        DrawTextW(dc,info.c_str(),-1,&ir,
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
                // unseen badge on the cartable tab
                if(t->kind==TK_PORTAL){
                    int n=unseenMessageCount(g_session.user.username);
                    if(n>0){
                        RECT bd={r.left+S(6),r.top+S(8),r.left+S(28),r.bottom-S(8)};
                        fillRoundRect(dc,bd,S(9),g_theme.danger,CLR_INVALID);
                        SetTextColor(dc,RGB(255,255,255)); SelectObject(dc,g_fSmall);
                        wchar_t nb[8]; swprintf(nb,8,L"%d",n);
                        DrawTextW(dc,toFaDigits(nb).c_str(),-1,&bd,
                            DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_NOPREFIX);
                    }
                    continue;   // cartable: no close/detach controls
                }
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
    // Tab order (RTL, right→left as displayed):
    //   نوبت‌دهی → پذیرش بیمار → کارتابل
    // The نوبت‌دهی (appointment) tab is FIRST (right-most). A ready-to-use
    // reception tab sits beside it, and the permanent کارتابل (cartable) tab
    // holds the management messages. On login the cartable is shown first so
    // the user immediately sees any pending messages.
    addTabKind(h, TK_APPOINTMENT);
    addTabKind(h, TK_RECEPTION);
    addTabKind(h, TK_PORTAL);
    if(s_rd){ s_rd->active=2; recLayoutTabs(h); }   // focus the cartable tab
    return h;
}
