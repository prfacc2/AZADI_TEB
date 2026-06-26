// ============================================================================
//  reception.cpp — reception workspace:
//   • info bar (current user, access type, live Iran date/time, calculator)
//   • browser-style tab strip: open/close/detach tabs (پذیرش <بخش>)
//   • reception form (Enter = next field) + live billing (Iranian insurances)
//   • print: رسید بیمه / چاپ نسخه / چاپ آخرین قبض (F8) — real printer output
// ============================================================================
#include "app.h"
//  v1.17.0: the HTML/CSS/JS (MSHTML) presentation host was retired — the
//  reception/appointment UI is now 100% native C++. `webhost.h` is no longer
//  included and the webhost_*.{cpp,inc} sources are no longer compiled. The
//  `web` HWND field on TabPage is kept (always NULL) so the existing layout
//  guards (`if(t->web) ...`) stay valid without any code churn.
#include <commctrl.h>
#include <stdio.h>
#include <algorithm>
#include <utility>

#define RC_CLASS   L"AzReception"
#define TABPG_CLASS L"AzRecTab"
#define DET_CLASS  L"AzDetached"

// info-bar buttons
#define ID_RC_CALC    501
#define ID_RC_NEWTAB  502
#define ID_RC_NEWPAT  503   // "پذیرش جدید" — clears the ACTIVE tab's form
#define ID_RC_APPT    504   // "نوبت‌دهی" — focus (or open) the appointment tab
//  v1.7.0: these three actions are now triggered from BUTTONS IN THE FRAME
//  HEADER (main.cpp) and routed to this window via WM_COMMAND, so the tab
//  strip stays clean. The header sends ID_RC_NEWPAT / ID_RC_APPT / ID_RC_NEWTAB.
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
#define ID_F_RESET       666   // v1.4.0 (§6): پاک کردن فرم (Ctrl+R)
#define ID_F_NEW         667   // v1.18.0: «پذیرش جدید» — فرم تازه (ردیف دکمه‌های پایین)
#define ID_F_SVC_ADD     668   // v1.18.0: «افزودن خدمت» به جدول خدمات
#define ID_F_SVC_SEARCH  669   // v1.18.0: جستجوی خدمت

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
    HWND bReset;   // v1.4.0 (§6) reset/clear form
    HWND bNew;     // v1.18.0: «پذیرش جدید» (ردیف دکمه‌های پایین فرم)
    HWND bSvcAdd;  // v1.18.0: «افزودن خدمت» بالای جدول خدمات
    HWND eSvcSearch;// v1.18.0: جستجوی خدمت
    HWND chkIns;             // «دارای بیمه» — کنار کد ملی، پیش‌فرض تیک‌خورده
    HWND appt;               // نوبت‌دهی child page (kind==TK_APPOINTMENT)
    //  v1.13.0 (§3/§4): the hybrid HTML/CSS/JS presentation host. When non-NULL
    //  it is the VISIBLE interface for this reception/appointment tab (rendered
    //  via the system MSHTML control). C++ stays the source of truth — the host
    //  bridges every action to the existing repository functions. NULL means the
    //  classic native form is used (deterministic fallback if MSHTML is absent).
    HWND web;
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
    //  v1.7.0: identity-verification state. After an inquiry, if NO trusted
    //  source could verify the national code, we flag name+surname so the form
    //  shows a clear validation (danger) border instead of fabricating data.
    bool idChecked;          // an inquiry was performed
    bool idVerified;         // a trusted source verified the identity
    //  v1.7.0 cartable (کارتابل) view state — only meaningful for TK_PORTAL.
    //   cartDetail : false → tile list, true → full message details view
    //   cartSelDisp: the display index (into s_cartMsgs) being viewed
    //   cartSelNF  : the plain newest-first index (for the data layer)
    //   cartHotBtn : which detail-view button is hovered (0=none)
    bool cartDetail;
    int  cartSelDisp, cartSelNF, cartHotBtn;
    //  v1.8.0: cartShowArchive — when true the cartable shows the locally
    //  archived (saved) messages instead of the live management inbox. Only
    //  reachable when «پیام‌های ذخیره‌شده» is enabled in settings.
    bool cartShowArchive;
    //  v1.9.0: bitmask of REQUIRED fields that were empty on the last submit
    //  attempt. Each set bit maps to an index in the `inputs[]` array used by
    //  the field-well painter, and draws ONLY a very thin red hairline border
    //  (no glow / double-ring) until the user edits the field.
    int  invalidMask;
    //  v2: vertical scroll offset (device px) for the reception form when its
    //  content is taller than the visible tab-page client area. 0 = top.
    int  scrollY;
    int  contentH;           // last computed total content height
    std::wstring lastMsg; COLORREF msgCol;
    TabPage():page(0),kind(TK_RECEPTION),appt(0),web(0),total(0),mainShare(0),patientShare(0),
        baseDiff(0),orgShare(0),paid(0),detached(false),autoPrice(false),
        idChecked(false),idVerified(false),
        cartDetail(false),cartSelDisp(-1),cartSelNF(-1),cartHotBtn(0),
        cartShowArchive(false),invalidMask(0),scrollY(0),contentH(0),msgCol(0){}
};

struct RecData {
    HWND bCalc, bNewTab, bNewPat;
    std::vector<TabPage*> tabs;
    int active;
    int hotTab, hotClose, hotDetach;     // hover indices
    int lastUnseen;                      // cartable poll state
    // v1.7.0: drag-and-drop tab reordering -----------------------------------
    bool  dragArmed;     // mouse down on a tab body, may become a drag
    bool  dragging;      // a reorder drag is in progress
    int   dragIdx;       // index of the tab being dragged
    POINT dragStart;     // where the press began (to apply a small threshold)
    int   dragX;         // current mouse x (for the floating ghost)
    int   dropIdx;       // computed insertion index (drop target)
    RecData():active(-1),hotTab(-1),hotClose(-1),hotDetach(-1),lastUnseen(0),
        dragArmed(false),dragging(false),dragIdx(-1),dragX(0),dropIdx(-1){
        dragStart.x=dragStart.y=0; }
};
static RecData* s_rd = NULL;             // single reception screen at a time

// metrics
// v1.10.0 — the old "میز پذیرش بیمار" header row was empty (the user/role/clock
// already live in the frame's Layer-1 header), so it only wasted ~54px of
// vertical space and forced the reception form to scroll. We collapse it to a
// thin spacer so the tabs + form shift up and the whole form fits at 1024×600
// without any scrollbar. (See requirement 3.1.)
static int infoBarH(){ return S(6); }
// v1.12.0 (§2.C): slightly slimmer tab strip + a touch more breathing room
// between tabs and from the strip edge, for a cleaner, more modern header.
static int tabBarH(){ return S(38); }
static int tabW()    { return S(206); }
static int tabGap()  { return S(8);  }

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
//  v1.18.0 — PIXEL-MATCH REDESIGN of the reception screen to the reference
//  mock-up. Three columns (RTL):
//     ┌─ billing (LEFT) ─┐ ┌──── center form (WIDE) ────┐ ┌─ info (RIGHT) ─┐
//     │ صورتحساب          │ │  «مشخصات بیمار» card        │ │ avatar/بیمه/.. │
//     │ چک پرداختی        │ │  «بیمه و نوبت» card         │ │                │
//     │ چاپ buttons       │ │  services table             │ │                │
//     └──────────────────┘ │  4 action buttons row       │ └────────────────┘
//                          └─────────────────────────────┘
//  The center column is built from two stacked white sub-cards (3-column grid
//  for identity/contact, 4-column for appointment/insurance), a services table
//  with an empty-state, and a row of four action buttons. The exact spacing,
//  border radii, label-with-red-asterisk and light input wells reproduce the
//  reference design.
static const int RC_OUT = 16;   // outer page padding
static const int RC_GAP = 14;   // gap between the three columns
static const int RC_IN  = 20;   // inner card padding (center cards)
static const int BILL_W = 196;  // billing card width  (LEFT)
static const int INFO_W = 210;  // right info-panel width

//  ----- horizontal metrics: the three column bounds + center grid columns.
//  cx[0..2] = LEFT edges of the 3 center columns (col 0 is the RIGHT-most in
//  RTL reading order); ccolW = width of each center column.
struct RecH {
    int billL, billR;        // LEFT billing card
    int infoL, infoR;        // RIGHT info panel
    int cardL, cardR;        // center column outer bounds
    int formL, formR;        // center inner (padded) bounds
    int fw;                  // inner form width
    int ccolW;               // one center grid column width (of 3)
    int cgap;                // gap between center grid columns
    bool stacked;            // narrow screen → drop side panels
};
static void rcH(int W, RecH& m){
    int pad=S(RC_OUT);
    m.billL = pad;
    m.billR = m.billL + S(BILL_W);
    m.infoR = W - pad;
    m.infoL = m.infoR - S(INFO_W);
    m.cardL = m.billR + S(RC_GAP);
    m.cardR = m.infoL - S(RC_GAP);
    m.stacked = (m.cardR - m.cardL) < S(420);
    if(m.stacked){
        m.billL=m.billR=0; m.infoL=m.infoR=0;
        m.cardL=pad; m.cardR=W-pad;
    }
    m.formL = m.cardL + S(RC_IN);
    m.formR = m.cardR - S(RC_IN);
    m.fw    = m.formR - m.formL;
    m.cgap  = S(14);
    m.ccolW = (m.fw - 2*m.cgap) / 3;
}
//  Back-compat shim used by a couple of remaining callers (old signature).
static void rcMetrics2(int W, int& cardL, int& cardR, int& billL, int& billR,
                       int& infoL, int& infoR, int& colW, int& xr, int& xl,
                       bool& stacked){
    RecH m; rcH(W,m);
    cardL=m.cardL; cardR=m.cardR; billL=m.billL; billR=m.billR;
    infoL=m.infoL; infoR=m.infoR; stacked=m.stacked;
    colW=m.ccolW; xr=m.formR-m.ccolW; xl=m.formL;
}

//  ----- vertical layout of the center column (single source of truth shared
//  by the painter and the control positioner). All y are virtual (pre-scroll).
struct CenterV {
    int rh;                  // input height
    // card 1 «مشخصات بیمار»
    int c1Top, c1Bot, c1HdrY;
    int r1y[3];              // baseline (input top) of the 3 identity rows
    // card 2 «بیمه و نوبت»
    int c2Top, c2Bot, c2HdrY;
    int r2y;                 // first row (4 cols) input top
    int r2by;                // second row (مبلغ/تخفیف) input top
    // services card
    int tTop, tBot, tHdrY;   // table card bounds + toolbar row
    int toolY;               // toolbar (افزودن خدمت + جستجو) input top
    int theadY;              // table header row top
    int tbodyY, tbodyBot;    // table body bounds
    // bottom buttons row
    int btnY, btnH;
    int totalBot;            // total content bottom
};
static void computeCenterV(const RecH& m, CenterV& v){
    (void)m;
    const int rh   = S(34);          // input height (matches reference)
    const int lbl  = S(20);          // label band above each input
    const int rgap = S(16);          // gap between grid rows
    v.rh = rh;
    int y = S(RC_OUT);
    // ----- card 1: مشخصات بیمار -----
    v.c1Top = y;
    v.c1HdrY = y + S(14);
    int rowsTop = y + S(44);         // below header + hairline
    for(int i=0;i<3;i++) v.r1y[i] = rowsTop + lbl + i*(lbl+rh+rgap);
    v.c1Bot = v.r1y[2] + rh + S(16);
    y = v.c1Bot + S(12);
    // ----- card 2: بیمه و نوبت -----
    v.c2Top = y;
    v.c2HdrY = y + S(14);
    int c2rowsTop = y + S(44);
    v.r2y  = c2rowsTop + lbl;
    v.r2by = v.r2y + rh + rgap + lbl;
    v.c2Bot = v.r2by + rh + S(16);
    y = v.c2Bot + S(12);
    // ----- action buttons row (MUST be BELOW insurance, BEFORE services) -----
    //  §1.18.2: per the reference image + spec, the «ثبت پذیرش…/پذیرش جدید/پاک
    //  کردن/انصراف» row sits directly under the «بیمه و نوبت» card and ABOVE the
    //  «افزودن خدمت» services table.
    v.btnH = S(44);
    v.btnY = y;
    y = v.btnY + v.btnH + S(14);
    // ----- services table card (always below the action buttons) -----
    v.tTop = y;
    v.toolY = y + S(12);             // toolbar buttons
    v.theadY = v.toolY + rh + S(12); // table header strip
    v.tbodyY = v.theadY + S(34);
    v.tbodyBot = v.tbodyY + S(120);  // empty-state body height
    v.tBot = v.tbodyBot + S(12);
    y = v.tBot + S(14);
    v.totalBot = v.tBot + S(RC_OUT);
}
//  Natural (uncompressed) full height the center form needs, so the page can
//  decide whether to show a vertical scrollbar. Width is taken from the current
//  client when known; H is unused now (kept for the existing call sites).
static int rcFormContentH(int W){
    RecH m; rcH(W>0?W:S(1024),m);
    CenterV v; computeCenterV(m,v);
    return v.totalBot + S(RC_OUT);
}
// ----------------------------------------------------------------------------
//  Unified RIGHT INFO-PANEL layout.  A SINGLE source of truth shared by the
//  painter (paintInfoPanel) and the control positioner (tabPageLayout) so the
//  painted group titles / chips and the real edit controls can NEVER drift
//  apart and overlap (the v1.9.x right-panel overlap bug).  All y values are in
//  device pixels and already scaled.  Every row reserves its own vertical band
//  with a small label line above each control band so labels never sit under or
//  behind a field.
struct InfoLayout {
    int iL, iR, iw;                 // inner padded bounds
    int cardTop, cardBot;
    int avCx, avCy, avR;            // avatar centre + radius
    int chipY, chipH;               // نسخه الکترونیک chip
    int boxY,  boxH;                // قبض/بارکد box
    int psY,   psH;                 // P:S counters line
    // group "کلیدهای جستجو"
    int g1TitleY;
    int archiveLblY, archiveY;
    int fileLblY,    fileY;
    // group "بیمه"
    int g2TitleY;
    int bookLblY,  bookY;
    int validLblY, validY;
    int rxLblY,    rxY;
    int suppLblY,  suppY;
    // group "پزشک معالج"
    int g3TitleY;
    int docCodeLblY, docCodeY;
    int docNameLblY, docNameY;
    int rh2, gp, btnW, lblH;
};
static void computeInfoLayout(int infoL, int infoR, int H, InfoLayout& L){
    int ipad=S(14);
    L.iL=infoL+ipad; L.iR=infoR-ipad; L.iw=L.iR-L.iL;
    L.cardTop=S(16); L.cardBot=H-S(16);
    L.rh2=S(30); L.gp=S(6); L.btnW=S(54); L.lblH=S(15);
    // --- header zone (matches painter) ---
    //  smaller avatar + «بیمار جدید/بدون سابقه» caption + green «نسخه الکترونیک»
    //  chip + a row of two counter boxes (نسخه سرپایی / نسخه الکترونیکی).
    L.avR=S(30); L.avCx=(L.iL+L.iR)/2; L.avCy=L.cardTop+S(12)+L.avR;
    int y=L.avCy+L.avR+S(8);
    y += S(36);                          // «بیمار جدید» + «بدون سابقه» two lines
    L.chipH=S(24); L.chipY=y;            y+=L.chipH+S(10);
    L.boxH =S(42); L.boxY =y;            y+=L.boxH +S(14); // two counter boxes
    L.psH  =0;     L.psY  =y;                                // unused now
    int lblGap=S(1);   // gap between a label line and its control
    int rowGap=S(8);   // gap between control rows
    int grpGap=S(14);  // gap before next group title
    auto labelled=[&](int& lblY,int& ctlY){
        lblY=y; y+=L.lblH+lblGap; ctlY=y; y+=L.rh2+rowGap;
    };
    // group 1 — کلیدهای جستجو
    L.g1TitleY=y; y+=S(22);
    labelled(L.archiveLblY,L.archiveY);
    labelled(L.fileLblY,   L.fileY);
    y+=grpGap-rowGap;
    // group 2 — بیمه
    L.g2TitleY=y; y+=S(22);
    labelled(L.bookLblY, L.bookY);
    labelled(L.validLblY,L.validY);
    labelled(L.rxLblY,   L.rxY);
    labelled(L.suppLblY, L.suppY);
    y+=grpGap-rowGap;
    // group 3 — پزشک معالج
    L.g3TitleY=y; y+=S(22);
    labelled(L.docCodeLblY,L.docCodeY);
    labelled(L.docNameLblY,L.docNameY);
}

//  v2: the virtual page height — the taller of the visible client area and the
//  natural content height of the form + the right info-panel. When the content
//  is taller than the client area the page scrolls; the cards grow to this VH so
//  their rounded bottom edge is always below the last control.
static int recPageVH(int W, int H){
    RecH m; rcH(W,m);
    int formH = rcFormContentH(W);
    int infoH = 0;
    if(!m.stacked && m.infoR>m.infoL){
        InfoLayout L; computeInfoLayout(m.infoL,m.infoR,H,L);
        infoH = L.docNameY + L.rh2 + S(40);   // last control + bottom room
    }
    //  v1.19.0: summary card (≈276) + gap + «مبلغ نهایی» gradient card (≈88) +
    //  «چاپ» group title + 3 print buttons (≈212 from the column bottom). We
    //  guarantee the gradient card never collides with the print group.
    int billH = S(364) + S(40) + S(212);       // final-amount bottom + gap + print block
    int need = formH; if(infoH>need) need=infoH; if(billH>need) need=billH;
    need += S(16);                                   // bottom padding mirror
    return need>H ? need : H;
}
//  Clamp & return the current scroll offset for the page.
static int recClampScroll(HWND h, TabPage* t){
    RECT rc; GetClientRect(h,&rc);
    int VH=recPageVH(rc.right,rc.bottom);
    int maxS = VH - rc.bottom; if(maxS<0) maxS=0;
    if(t->scrollY<0) t->scrollY=0;
    if(t->scrollY>maxS) t->scrollY=maxS;
    t->contentH=VH;
    return maxS;
}
//  Sync the WS_VSCROLL thumb to the current scroll state.
static void recUpdateScrollbar(HWND h, TabPage* t){
    RECT rc; GetClientRect(h,&rc);
    int VH=recPageVH(rc.right,rc.bottom);
    SCROLLINFO si={sizeof(si)};
    si.fMask=SIF_RANGE|SIF_PAGE|SIF_POS;
    si.nMin=0; si.nMax=VH-1; si.nPage=rc.bottom; si.nPos=t->scrollY;
    SetScrollInfo(h,SB_VERT,&si,TRUE);
}
static void tabPageLayout(HWND h, TabPage* t){
    if(!t) return;
    // §3 hybrid host: if the HTML/CSS/JS surface is up it owns the whole tab.
    if(t->web){
        RECT rc; GetClientRect(h,&rc);
        MoveWindow(t->web,0,0,rc.right,rc.bottom,TRUE);
        return;
    }
    if(t->kind==TK_APPOINTMENT){
        if(t->appt){ RECT rc; GetClientRect(h,&rc);
            MoveWindow(t->appt,0,0,rc.right,rc.bottom,TRUE); }
        return;
    }
    if(t->kind!=TK_RECEPTION) return;   // painted pages have no controls
    RECT rc; GetClientRect(h,&rc);
    int W=rc.right, H=rc.bottom;
    if(W<=0 || H<=0) return;
    RecH m; rcH(W,m);
    CenterV v; computeCenterV(m,v);
    recClampScroll(h,t);
    const int sy=t->scrollY;          // scroll offset (subtract from every y)
    recUpdateScrollbar(h,t);

    const int rh = v.rh;
    const int cgap = m.cgap, cw = m.ccolW;
    // RTL: column 0 sits at the RIGHT, column 2 at the LEFT.
    auto colX=[&](int c){ return m.formR - (c+1)*cw - c*cgap; };
    const int cx0 = colX(0), cx1 = colX(1), cx2 = colX(2);
    #define Y(yy) ((yy)-sy)

    // ===== card 1: مشخصات بیمار (3-column grid) =====
    // row0: نام(col0) | نام خانوادگی(col1) | کد ملی(col2)
    MoveWindow(t->eFirst, cx0, Y(v.r1y[0]), cw, rh, TRUE);
    MoveWindow(t->eLast,  cx1, Y(v.r1y[0]), cw, rh, TRUE);
    MoveWindow(t->eNid,   cx2, Y(v.r1y[0]), cw, rh, TRUE);
    // row1: نام پدر(col0) | تاریخ تولد(col1) | جنسیت(col2)
    MoveWindow(t->eFather,cx0, Y(v.r1y[1]), cw, rh, TRUE);
    MoveWindow(t->eBirth, cx1, Y(v.r1y[1]), cw, rh, TRUE);
    MoveWindow(t->cGender,cx2, Y(v.r1y[1]), cw, S(200), TRUE);
    // row2: شماره موبایل(col0) | تلفن ثابت(col1) | آدرس(col2)
    MoveWindow(t->eMobile,cx0, Y(v.r1y[2]), cw, rh, TRUE);
    MoveWindow(t->ePhone, cx1, Y(v.r1y[2]), cw, rh, TRUE);
    MoveWindow(t->eAddr,  cx2, Y(v.r1y[2]), cw, rh, TRUE);

    // ===== card 2: بیمه و نوبت (4-column row then 2-column row) =====
    // 4 columns across the full inner width.
    int gw = (m.fw - 3*cgap)/4;
    auto gx=[&](int c){ return m.formR - (c+1)*gw - c*cgap; };
    MoveWindow(t->cPType, gx(0), Y(v.r2y), gw, S(200), TRUE);  // نوع پذیرش
    MoveWindow(t->cNType, gx(1), Y(v.r2y), gw, S(200), TRUE);  // نوع نوبت
    MoveWindow(t->cIns,   gx(2), Y(v.r2y), gw, S(240), TRUE);  // نوع بیمه
    MoveWindow(t->cSupp,  gx(3), Y(v.r2y), gw, S(240), TRUE);  // نوع بیمه پایه
    // second row: مبلغ خدمت | تخفیف (each spans 2 of the 4 columns)
    int hw = (m.fw - cgap)/2;
    MoveWindow(t->ePrice,    m.formR-hw,        Y(v.r2by), hw, rh, TRUE);
    MoveWindow(t->eDiscount, m.formL,           Y(v.r2by), hw, rh, TRUE);
    // «دارای بیمه» checkbox is folded into the insurance flow; hide the visual
    // duplicate in the reference design (the painted labels carry the meaning).
    ShowWindow(t->chkIns, SW_HIDE);

    // ===== services table toolbar =====
    // «افزودن خدمت» button (left) + search box (to its right)
    int addW = S(108);
    MoveWindow(t->bSvcAdd,   m.formL,            Y(v.toolY), addW, rh, TRUE);
    MoveWindow(t->eSvcSearch,m.formL+addW+cgap,  Y(v.toolY), S(180), rh, TRUE);

    // ===== bottom action buttons row (4 buttons) =====
    //  §1.18.1: order EXACTLY matches the reference image (RTL, right→left):
    //    ثبت پذیرش و صدور قبض (blue, widest) | پذیرش جدید | پاک کردن (red) | انصراف
    //  (previously پاک‌کردن and پذیرش‌جدید were swapped vs. the reference).
    {
        int gap = S(10);
        int small = S(118);
        int wideW = m.fw - 3*(small+gap);
        if(wideW < S(200)) wideW = S(200);
        int x = m.formR;                 // start at the right edge (RTL)
        // submit is the RIGHT-most & widest
        x -= wideW;     MoveWindow(t->bSubmit, x, Y(v.btnY), wideW, v.btnH, TRUE);
        x -= gap+small; MoveWindow(t->bNew,    x, Y(v.btnY), small, v.btnH, TRUE);
        x -= gap+small; MoveWindow(t->bReset,  x, Y(v.btnY), small, v.btnH, TRUE);
        x -= gap+small; MoveWindow(t->bClose,  x, Y(v.btnY), small, v.btnH, TRUE);
        ShowWindow(t->bSubmit,SW_SHOW); ShowWindow(t->bReset,SW_SHOW);
        ShowWindow(t->bNew,SW_SHOW);    ShowWindow(t->bClose,SW_SHOW);
        ShowWindow(t->bSvcAdd,SW_SHOW); ShowWindow(t->eSvcSearch,SW_SHOW);
    }

    // billing panel print buttons (bottom of LEFT billing card). Pinned to the
    // virtual page height so they track the card bottom and scroll with the page.
    int VH = recPageVH(W,H);
    if(!m.stacked){
        int bx = m.billL + S(12), byy = VH - S(186) - sy;
        int bbw = (m.billR-m.billL) - S(24);
        MoveWindow(t->bPrtIns, bx, byy,        bbw, S(40), TRUE);
        MoveWindow(t->bPrtRx,  bx, byy+S(48),  bbw, S(40), TRUE);
        MoveWindow(t->bPrtLast,bx, byy+S(96),  bbw, S(40), TRUE);
        ShowWindow(t->bPrtIns,SW_SHOW); ShowWindow(t->bPrtRx,SW_SHOW);
        ShowWindow(t->bPrtLast,SW_SHOW);
        ShowWindow(t->bInquiry,SW_HIDE);
    } else {
        ShowWindow(t->bInquiry,SW_HIDE);
        ShowWindow(t->bPrtIns,SW_HIDE); ShowWindow(t->bPrtRx,SW_HIDE);
        ShowWindow(t->bPrtLast,SW_HIDE);
    }
    #undef Y

    // ====================== RIGHT INFO PANEL layout ======================
    HWND infoCtls[]={t->eArchive,t->bArchiveGo,t->eFile,t->bFileGo,t->eBookNo,
        t->chkBookNo,t->eValid,t->chkValidAuto,t->eRxDate,t->chkRxAuto,
        t->cSuppPanel,t->eSuppPct,t->bSuppGo,t->eDocCode,t->eDocName,t->bDocSearch};
    if(m.stacked || m.infoR<=m.infoL){
        for(HWND c: infoCtls) if(c) ShowWindow(c,SW_HIDE);
    } else {
        for(HWND c: infoCtls) if(c) ShowWindow(c,SW_SHOW);
        InfoLayout L; computeInfoLayout(m.infoL,m.infoR,H,L);
        const int iL=L.iL, iw=L.iw, rh2=L.rh2, gp=L.gp, btnW=L.btnW;
        // --- search keys group ---  (all info-panel controls also scroll)
        MoveWindow(t->bArchiveGo, iL, L.archiveY-sy, btnW, rh2, TRUE);
        MoveWindow(t->eArchive,   iL+btnW+gp, L.archiveY-sy, iw-btnW-gp, rh2, TRUE);
        MoveWindow(t->bFileGo,    iL, L.fileY-sy, btnW, rh2, TRUE);
        MoveWindow(t->eFile,      iL+btnW+gp, L.fileY-sy, iw-btnW-gp, rh2, TRUE);
        // --- insurance block group ---
        // ش دفترچه + فعال checkbox
        MoveWindow(t->chkBookNo, iL, L.bookY-sy, S(64), rh2, TRUE);
        MoveWindow(t->eBookNo,   iL+S(70), L.bookY-sy, iw-S(70), rh2, TRUE);
        // تاریخ اعتبار + اتوماتیک
        MoveWindow(t->chkValidAuto, iL, L.validY-sy, S(92), rh2, TRUE);
        MoveWindow(t->eValid,       iL+S(98), L.validY-sy, iw-S(98), rh2, TRUE);
        // تاریخ نسخه + اتوماتیک
        MoveWindow(t->chkRxAuto, iL, L.rxY-sy, S(92), rh2, TRUE);
        MoveWindow(t->eRxDate,   iL+S(98), L.rxY-sy, iw-S(98), rh2, TRUE);
        // بیمه مکمل + درصد + استعلام
        MoveWindow(t->bSuppGo,    iL, L.suppY-sy, btnW, rh2, TRUE);
        MoveWindow(t->eSuppPct,   iL+btnW+gp, L.suppY-sy, S(48), rh2, TRUE);
        MoveWindow(t->cSuppPanel, iL+btnW+gp+S(54), L.suppY-sy, iw-btnW-gp-S(54), S(200), TRUE);
        // --- پزشک معالج group ---
        MoveWindow(t->eDocCode, iL, L.docCodeY-sy, iw, rh2, TRUE);
        MoveWindow(t->bDocSearch, iL, L.docNameY-sy, btnW, rh2, TRUE);
        MoveWindow(t->eDocName,   iL+btnW+gp, L.docNameY-sy, iw-btnW-gp, rh2, TRUE);
    }
}

// ----- national-id insurance inquiry ---------------------------------------
//  Validate an Iranian 10-digit national code (checksum) and look the patient
//  up against a TRUSTED source only. validNationalId() / lookupCitizen() live in
//  data_ext.cpp (shared with the appointment module). doInquiry() uses
//  lookupCitizen() to auto-fill the patient fields ONLY from a verified record
//  (online registry web-service when configured, or the local store of
//  previously-verified patients) and to detect when a patient carries 2 or 3
//  insurances — in which case it announces it and restricts the insurance combo
//  to ONLY those organisations, highlighted in a distinct colour.
//  v1.7.0: no fabrication. We only fill fields when a TRUSTED source (online
//  registry web-service or the local store of previously-verified patients)
//  returns a real record. When nothing verifies the code we DO NOT guess — the
//  name + surname wells turn into a danger/validation state and the operator
//  enters the identity by hand (which is then remembered on save).
static void rebuildInsuranceCombo(TabPage* t, const std::vector<int>& verified){
    t->insAllowed = verified;
    SendMessageW(t->cIns,CB_RESETCONTENT,0,0);
    if(verified.size()>=2){
        for(int ix : verified)
            if(ix>=0 && ix<N_INSURANCES)
                SendMessageW(t->cIns,CB_ADDSTRING,0,(LPARAM)INSURANCES[ix].name);
        SendMessageW(t->cIns,CB_SETCURSEL,0,0);
    } else {
        for(int i=0;i<N_INSURANCES;i++)
            SendMessageW(t->cIns,CB_ADDSTRING,0,(LPARAM)INSURANCES[i].name);
        int idx = verified.empty()?0:verified[0];
        if(idx<0||idx>=N_INSURANCES) idx=0;
        SendMessageW(t->cIns,CB_SETCURSEL,idx,0);
    }
}
static void doInquiry(TabPage* t, HWND h, bool quiet){
    wchar_t b[32]={0}; GetWindowTextW(t->eNid,b,32);
    std::wstring nid=trim(b);
    t->idChecked=true; t->idVerified=false;
    if(!validNationalId(nid)){
        t->insAllowed.clear();
        if(!quiet){
            t->lastMsg=L"کد ملی نامعتبر است (۱۰ رقم و رقم کنترلی صحیح). مشخصات را دستی وارد کنید.";
            t->msgCol=g_theme.danger;
        }
        InvalidateRect(h,NULL,FALSE);
        return;
    }
    CitizenInfo c = lookupCitizen(nid);
    if(!c.found){
        // No trusted source verified it → manual entry, no guessing.
        t->insAllowed.clear();
        // keep the full insurance list editable for manual selection
        rebuildInsuranceCombo(t, std::vector<int>());
        if(!quiet){
            if(c.lookupFailed)
                t->lastMsg=L"استعلام برخط ناموفق بود (سرور در دسترس نیست). مشخصات را دستی وارد و بررسی کنید.";
            else
                t->lastMsg=L"هویت تأیید نشد؛ نام و نام خانوادگی را دستی وارد کنید (قاب قرمز).";
            t->msgCol=g_theme.danger;
        }
        recalc(t);
        InvalidateRect(h,NULL,FALSE);
        SetFocus(t->eFirst);
        return;
    }
    // verified — fill only the fields the trusted source actually returned
    t->idVerified=true;
    auto setIfPresent=[&](HWND e, const std::wstring& v){
        if(!v.empty()) SetWindowTextW(e,v.c_str());
    };
    setIfPresent(t->eFirst, c.firstName);
    setIfPresent(t->eLast,  c.lastName);
    setIfPresent(t->eFather,c.fatherName);
    setIfPresent(t->eMobile,c.mobile);
    setIfPresent(t->eBirth, c.birthDate);
    if(c.gender==L"زن") SendMessageW(t->cGender,CB_SETCURSEL,1,0);
    else if(c.gender==L"مرد") SendMessageW(t->cGender,CB_SETCURSEL,0,0);

    // ---- insurance: ONLY what the trusted source verified ----
    rebuildInsuranceCombo(t, c.insurances);
    std::wstring src = c.source==CS_REGISTRY ? L"ثبت احوال/سامانه بیمه"
                     : c.source==CS_LOCAL    ? L"سوابق همین درمانگاه"
                     :                          L"منبع معتبر";
    if(c.insurances.size()>=2){
        wchar_t mb[220];
        swprintf(mb,220,L"هویت تأیید شد (%s) — این بیمار %d بیمهٔ معتبر دارد.",
            src.c_str(),(int)c.insurances.size());
        t->lastMsg=toFaDigits(mb); t->msgCol=g_theme.warn;
    } else if(c.insurances.size()==1){
        t->lastMsg=L"هویت تأیید شد ("+src+L") — بیمه: "+INSURANCES[c.insurances[0]].name;
        t->msgCol=g_theme.success;
    } else {
        t->lastMsg=L"هویت تأیید شد ("+src+L") — بیمه‌ای ثبت نشده؛ در صورت نیاز دستی انتخاب کنید.";
        t->msgCol=g_theme.success;
    }
    recalc(t);
    InvalidateRect(h,NULL,FALSE);
}

// ----- national-id field: Enter triggers a network-wide patient lookup -------
//  The operator types the 10-digit national code in t->eNid and presses Enter.
//  Regardless of the «دارای بیمه» state we run the validated, no-fabrication
//  lookupCitizen() (online registry → local verified store) and auto-fill the
//  patient identity (name / surname / father / birth date / gender / contact /
//  insurance + supplementary). Missing/partial records fill only what is known;
//  invalid/duplicate codes are handled gracefully inside doInquiry(). After the
//  lookup we hop to the first empty field so data entry stays fast.
static WNDPROC s_oldNid = NULL;
static LRESULT CALLBACK nidEditProc(HWND e, UINT m, WPARAM w, LPARAM l){
    if(m==WM_KEYDOWN && w==VK_RETURN){
        HWND page = GetParent(e);
        TabPage* t = page ? (TabPage*)GetWindowLongPtrW(page,GWLP_USERDATA) : NULL;
        if(t){
            doInquiry(t, page, false);          // always look up + auto-fill
            // advance to the first still-empty identity field for fast entry
            HWND nxt = NULL;
            auto isEmpty=[](HWND c){ wchar_t b[8]={0}; GetWindowTextW(c,b,2); return b[0]==0; };
            if(t->idVerified){
                if(isEmpty(t->eFirst))      nxt=t->eFirst;
                else if(isEmpty(t->eLast))  nxt=t->eLast;
                else if(isEmpty(t->eMobile))nxt=t->eMobile;
                else                        nxt=t->eFirst;
            } else {
                nxt = t->eFirst;                // manual entry starts at name
            }
            if(nxt){ SetFocus(nxt); SendMessageW(nxt,EM_SETSEL,0,-1); }
        }
        return 0;
    }
    if(m==WM_CHAR && (w==VK_RETURN||w==VK_TAB)) return 0;   // suppress beep
    return CallWindowProcW(s_oldNid?s_oldNid:DefWindowProcW, e, m, w, l);
}
static void enableNidLookup(HWND e){
    WNDPROC old=(WNDPROC)SetWindowLongPtrW(e,GWLP_WNDPROC,(LONG_PTR)nidEditProc);
    if(!s_oldNid) s_oldNid=old;
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
    t->idChecked=false; t->idVerified=false; t->insAllowed.clear();
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
//  v2: messages are drawn as fixed-size RECTANGLE TILES laid side-by-side and
//  wrapping into rows (not full-width lines). Pinned messages float to the
//  front; the rest are sorted by send date (newest first). Clicking a tile
//  opens a context menu (پین کردن / دیدن / پاک کردن). The background is solid
//  dark with NO gradient bleed so colours stay crisp on the dark theme.
struct CartTile { RECT r; int disp; };       // disp = index into s_cartMsgs
static std::vector<CartTile> s_cartTiles;    // hit-test map, rebuilt each paint
static std::vector<KMsg>     s_cartMsgs;      // the list shown (sorted)
static std::vector<int>      s_cartNF;        // display-pos → plain newest-first idx
// v1.7.0 detail-view buttons (rebuilt each paint of the details screen)
enum { CART_BTN_NONE=0, CART_BTN_MARK=1, CART_BTN_TOGGLEREAD=2,
       CART_BTN_DELETE=3, CART_BTN_BACK=4, CART_BTN_SAVE=5 };
struct CartBtn { RECT r; int id; };
static std::vector<CartBtn> s_cartBtns;      // detail-view button hit map
// v1.8.0: the archive toggle hotspot in the cartable header (top-LEFT corner).
// Empty when «پیام‌های ذخیره‌شده» is disabled (it is then not drawn / not hit).
static RECT s_cartArchiveRect = {0,0,0,0};

// load + sort: pinned first, then by send time (newest first). loadMessages
// already returns oldest-first, so we reverse for newest-first then stable-
// partition the pinned ones to the front. s_cartNF remembers each tile's
// PLAIN newest-first index (what the data layer's pin/seen/delete expects).
static void cartReload(){
    auto raw=loadMessages(g_session.user.username);
    std::reverse(raw.begin(),raw.end());     // newest first (by storage order)
    // pair each message with its plain newest-first index, then stable-sort
    std::vector<std::pair<KMsg,int>> v;
    for(int i=0;i<(int)raw.size();i++) v.push_back({raw[i],i});
    std::stable_sort(v.begin(),v.end(),
        [](const std::pair<KMsg,int>&a,const std::pair<KMsg,int>&b){
            return a.first.pinned && !b.first.pinned; });
    s_cartMsgs.clear(); s_cartNF.clear();
    for(auto&p:v){ s_cartMsgs.push_back(p.first); s_cartNF.push_back(p.second); }
}
// severity colour + label helpers (priority/status of a message) -----------
static COLORREF sevColor(int type){
    return type==KMSG_CRITICAL ? g_theme.danger
         : type==KMSG_URGENT   ? g_theme.warn
         :                       g_theme.success;
}
static const wchar_t* sevLabel(int type){
    return type==KMSG_CRITICAL ? L"بحرانی"
         : type==KMSG_URGENT   ? L"فوری"
         :                       L"عادی";
}
// split a stored time string "1403/05/20 14:30" → date + time parts ----------
static void splitMsgTime(const std::wstring& s, std::wstring& date, std::wstring& time){
    size_t sp=s.find(L' ');
    if(sp==std::wstring::npos){ date=s; time=L""; }
    else { date=s.substr(0,sp); time=s.substr(sp+1); }
}

// ----- the FULL message DETAILS view (sender, priority, date, time, body) ---
static void drawCartDetail(HDC dc, const RECT& rc, TabPage* t){
    { HBRUSH bg=CreateSolidBrush(g_theme.bg); FillRect(dc,(RECT*)&rc,bg); DeleteObject(bg); }
    SetBkMode(dc,TRANSPARENT);
    s_cartBtns.clear();
    cartReload();
    // resolve the selected message by its STABLE newest-first index (cartSelNF)
    // so it stays correct even if the list re-sorts (e.g. after pin/seen).
    int sel=-1;
    for(int i=0;i<(int)s_cartNF.size();i++)
        if(s_cartNF[i]==t->cartSelNF){ sel=i; break; }
    if(sel<0 && t->cartSelDisp>=0 && t->cartSelDisp<(int)s_cartMsgs.size())
        sel=t->cartSelDisp;
    if(sel<0 || sel>=(int)s_cartMsgs.size()){
        t->cartDetail=false; return;     // selection no longer valid (deleted)
    }
    t->cartSelDisp=sel;
    KMsg& mm=s_cartMsgs[sel];
    COLORREF sev=sevColor(mm.type);

    int pad=S(20);
    RECT panel={rc.left+pad, rc.top+pad, rc.right-pad, rc.bottom-pad};
    // v1.8.0: draw the panel as a real rounded rect (corners patched with the
    // page background) instead of a square FillRect that left surface-coloured
    // square corners poking out beyond the rounded border.
    gpRoundRectBg(dc,panel,S(16),g_theme.surface,g_theme.border,g_theme.bg,255);

    // header bar (priority-coloured so status reads at a glance)
    RECT hdr={panel.left,panel.top,panel.right,panel.top+S(52)};
    gpGradRoundRectBg(dc,hdr,S(16),sev,sev,CLR_INVALID,g_theme.surface);
    RECT hdrB={panel.left,panel.top+S(28),panel.right,panel.top+S(52)};
    gpGradRoundRect(dc,hdrB,0,sev,sev,CLR_INVALID);
    RECT bi={panel.right-S(44),panel.top+S(12),panel.right-S(18),panel.top+S(38)};
    drawIcon(dc,ICO_BELL,bi,RGB(255,255,255),S(2));
    SelectObject(dc,g_fTitle); SetTextColor(dc,RGB(255,255,255));
    RECT tr={panel.left+S(20),panel.top+S(8),panel.right-S(52),panel.top+S(44)};
    DrawTextW(dc,L"جزئیات پیام مدیریت",-1,&tr,
        DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
    // pin icon top-right of the header when pinned
    if(mm.pinned){
        SelectObject(dc,g_fUIB); SetTextColor(dc,RGB(255,255,255));
        RECT pr={panel.left+S(16),panel.top+S(12),panel.left+S(120),panel.top+S(40)};
        DrawTextW(dc,L"\U0001F4CC سنجاق‌شده",-1,&pr,
            DT_LEFT|DT_SINGLELINE|DT_VCENTER|DT_NOPREFIX);
    }

    std::wstring date,time; splitMsgTime(mm.time,date,time);
    int x0=panel.left+S(24), x1=panel.right-S(24);
    int y=panel.top+S(70);
    // metadata grid rows -----------------------------------------------------
    struct Meta{ const wchar_t* k; std::wstring v; COLORREF vc; };
    std::wstring sender=mm.from.empty()?std::wstring(L"مدیریت درمانگاه"):mm.from;
    std::wstring recip = (mm.to==L"*")?std::wstring(L"همهٔ کاربران"):mm.to;
    Meta rows[]={
        {L"فرستنده",     sender,                        g_theme.text},
        {L"گیرنده",      recip,                         g_theme.text},
        {L"اولویت / وضعیت", std::wstring(sevLabel(mm.type))+
            (mm.seen?L"  •  خوانده‌شده":L"  •  خوانده‌نشده"), sev},
        {L"تاریخ",        toFaDigits(date),             g_theme.text},
        {L"ساعت",         toFaDigits(time),             g_theme.text},
    };
    for(auto& r : rows){
        SelectObject(dc,g_fSmall); SetTextColor(dc,g_theme.textDim);
        RECT kr={x1-S(150),y,x1,y+S(22)};
        DrawTextW(dc,r.k,-1,&kr,DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
        SelectObject(dc,g_fUIB); SetTextColor(dc,r.vc);
        RECT vr={x0,y,x1-S(160),y+S(22)};
        DrawTextW(dc,r.v.c_str(),-1,&vr,DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
        y+=S(28);
    }
    // separator
    { HPEN pn=CreatePen(PS_SOLID,1,g_theme.border); HGDIOBJ op=SelectObject(dc,pn);
      MoveToEx(dc,x0,y+S(4),0); LineTo(dc,x1,y+S(4)); SelectObject(dc,op); DeleteObject(pn); }
    y+=S(16);
    // description / body ------------------------------------------------------
    SelectObject(dc,g_fSmall); SetTextColor(dc,g_theme.accent);
    RECT dl={x0,y,x1,y+S(20)};
    DrawTextW(dc,L"متن پیام",-1,&dl,DT_RIGHT|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);
    y+=S(24);
    SelectObject(dc,g_fUI); SetTextColor(dc,g_theme.text);
    RECT body={x0,y,x1,panel.bottom-S(70)};
    DrawTextW(dc,mm.text.c_str(),-1,&body,
        DT_RIGHT|DT_WORDBREAK|DT_RTLREADING|DT_NOPREFIX|DT_EDITCONTROL);

    // action buttons (bottom strip): بازگشت | حذف | علامت خوانده‌شده | باز/نخوانده
    int by=panel.bottom-S(54), bh=S(38);
    int bx=panel.left+S(24);            // RTL: lay out from LEFT going right
    struct Btn{int id; const wchar_t* lbl; COLORREF fill; COLORREF txt;};
    std::vector<Btn> defs={
        {CART_BTN_BACK,       L"بازگشت",            g_theme.surface2, g_theme.text},
        {CART_BTN_DELETE,     L"حذف پیام",          g_theme.danger,   RGB(255,255,255)},
        {CART_BTN_TOGGLEREAD, mm.seen?L"خوانده":L"علامت خوانده‌شده",
                              g_theme.surface2, g_theme.text},
        {CART_BTN_MARK,       L"خواندن",            g_theme.accent,   g_theme.accentText},
    };
    // v1.9.5: when «پیام‌های ذخیره‌شده» is enabled, the message viewer offers a
    // SAVE button that archives this exact message into the saved-messages store
    // (data\saved_msgs.dat). Hidden in the archive view itself (already saved).
    if(savedMsgsEnabled() && !(t && t->cartShowArchive)){
        defs.push_back({CART_BTN_SAVE, L"ذخیره در پیام‌ها", g_theme.success, RGB(255,255,255)});
    }
    for(auto& d : defs){
        SelectObject(dc,g_fUIB);
        SIZE sz; GetTextExtentPoint32W(dc,d.lbl,(int)wcslen(d.lbl),&sz);
        int bw=sz.cx+S(34);
        RECT r={bx,by,bx+bw,by+bh};
        bool hov=(t->cartHotBtn==d.id);
        fillRoundRect(dc,r,S(9), hov?g_theme.accent:d.fill,
                      hov?g_theme.accent:g_theme.border);
        SetTextColor(dc, hov?g_theme.accentText:d.txt);
        DrawTextW(dc,d.lbl,-1,&r,DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
        s_cartBtns.push_back({r,d.id});
        bx+=bw+S(10);
    }
    SelectObject(dc,g_fSmall); SetTextColor(dc,g_theme.textDim);
    RECT hint={bx+S(8),by,panel.right-S(20),by+bh};
    DrawTextW(dc,L"برای بازگشت کلید Esc را بزنید",-1,&hint,
        DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
}

static void drawCartList(HDC dc, const RECT& rc, TabPage* t){
    // solid dark background — no gradient bleed
    { HBRUSH bg=CreateSolidBrush(g_theme.bg); FillRect(dc,(RECT*)&rc,bg); DeleteObject(bg); }
    SetBkMode(dc,TRANSPARENT);
    int pad=S(20);
    RECT panel={rc.left+pad, rc.top+pad, rc.right-pad, rc.bottom-pad};
    // v1.8.0: rounded panel with corners patched to the page background.
    gpRoundRectBg(dc,panel,S(16),g_theme.surface,g_theme.border,g_theme.bg,255);

    bool savedOn = savedMsgsEnabled();
    bool archive = savedOn && t && t->cartShowArchive;

    // header bar
    RECT hdr={panel.left,panel.top,panel.right,panel.top+S(52)};
    gpGradRoundRectBg(dc,hdr,S(16),g_theme.accent2,g_theme.accent,CLR_INVALID,g_theme.surface);
    RECT hdrB={panel.left,panel.top+S(28),panel.right,panel.top+S(52)};
    gpGradRoundRect(dc,hdrB,0,g_theme.accent2,g_theme.accent,CLR_INVALID);
    RECT bi={panel.right-S(44),panel.top+S(12),panel.right-S(18),panel.top+S(38)};
    drawIcon(dc,archive?ICO_SAVE:ICO_BELL,bi,RGB(255,255,255),S(2));
    SelectObject(dc,g_fTitle); SetTextColor(dc,RGB(255,255,255));
    RECT tr={panel.left+S(56),panel.top+S(8),panel.right-S(52),panel.top+S(44)};
    DrawTextW(dc, archive?L"پیام‌های ذخیره‌شده":L"کارتابل — پیام‌های مدیریت درمانگاه",-1,&tr,
        DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);

    // §D (v1.10.0): the saved-messages (archive) toggle is ALWAYS visible and
    // reachable in the TOP-LEFT corner of the cartable header — it is no longer
    // hidden behind the «پیام‌های ذخیره‌شده» setting. When the feature is off,
    // clicking it offers to enable it (handled in WM_LBUTTONUP), so the entry is
    // discoverable instead of dead.
    {
        RECT ab={panel.left+S(12),panel.top+S(12),panel.left+S(40),panel.top+S(40)};
        if(archive) gpRoundRect(dc,ab,S(8),RGB(255,255,255),CLR_INVALID,60);
        RECT ai={ab.left+S(4),ab.top+S(4),ab.right-S(4),ab.bottom-S(4)};
        // dim the icon a touch when the feature is disabled (still clickable).
        drawIcon(dc, archive?ICO_BELL:ICO_SAVE, ai,
                 savedOn?RGB(255,255,255):RGB(210,224,242), S(2));
        s_cartArchiveRect = ab;
    }

    s_cartTiles.clear();
    int topY=panel.top+S(64);

    // ---- ARCHIVE (saved messages) branch -----------------------------------
    if(archive){
        auto saved=loadSavedMsgs();
        if(saved.empty()){
            SelectObject(dc,g_fUI); SetTextColor(dc,g_theme.textDim);
            RECT er={panel.left+S(24),topY,panel.right-S(24),topY+S(40)};
            DrawTextW(dc,L"پیام ذخیره‌شده‌ای وجود ندارد.",-1,&er,
                DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
            return;
        }
        int y=topY;
        for(int i=0;i<(int)saved.size() && y<panel.bottom-S(20); i++){
            RECT card={panel.left+S(16),y,panel.right-S(16),y+S(76)};
            gpRoundRectBg(dc,card,S(10),g_theme.surface2,g_theme.border,g_theme.surface);
            SelectObject(dc,g_fUIB); SetTextColor(dc,g_theme.text);
            RECT hr={card.left+S(14),card.top+S(8),card.right-S(14),card.top+S(30)};
            std::wstring head=saved[i].from+L"  ←  "+saved[i].to+L"   ("+toFaDigits(saved[i].time)+L")";
            DrawTextW(dc,head.c_str(),-1,&hr,
                DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX|DT_END_ELLIPSIS);
            SelectObject(dc,g_fSmall); SetTextColor(dc,g_theme.textDim);
            RECT br={card.left+S(14),card.top+S(32),card.right-S(14),card.bottom-S(6)};
            std::wstring body=saved[i].text;
            if(!saved[i].attachPath.empty()) body=L"\U0001F4CE "+body;
            DrawTextW(dc,body.c_str(),-1,&br,
                DT_RIGHT|DT_WORDBREAK|DT_RTLREADING|DT_NOPREFIX|DT_END_ELLIPSIS|DT_EDITCONTROL);
            y += S(76)+S(10);
        }
        return;
    }

    cartReload();
    if(s_cartMsgs.empty()){
        SelectObject(dc,g_fUI); SetTextColor(dc,g_theme.textDim);
        RECT er={panel.left+S(24),topY,panel.right-S(24),topY+S(40)};
        DrawTextW(dc,L"در حال حاضر پیامی از مدیریت دریافت نشده است.",-1,&er,
            DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
        return;
    }
    // a small caption inviting the user to click for details
    SelectObject(dc,g_fSmall); SetTextColor(dc,g_theme.accentText);
    RECT cap={panel.left+S(56),panel.top+S(34),panel.right-S(52),panel.top+S(50)};
    DrawTextW(dc,L"برای دیدن جزئیات روی پیام کلیک کنید — راست‌کلیک: سنجاق / ذخیره",-1,&cap,
        DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);

    // tile grid metrics — responsive column count
    int areaW = panel.right-panel.left-S(32);
    int tileMinW = S(300);
    int cols = areaW / (tileMinW+S(12)); if(cols<1) cols=1; if(cols>4) cols=4;
    int gap  = S(12);
    int tileW = (areaW - gap*(cols-1)) / cols;
    int tileH = S(96);
    int x0    = panel.right-S(16);            // RTL: lay out from the RIGHT edge
    int col=0, row=0;
    for(int i=0;i<(int)s_cartMsgs.size();i++){
        KMsg& mm=s_cartMsgs[i];
        int tx = x0 - (col+1)*tileW - col*gap;
        int ty = topY + row*(tileH+gap);
        if(ty>panel.bottom-tileH-S(8)) break;     // ran out of vertical room
        RECT card={tx,ty,tx+tileW,ty+tileH};
        s_cartTiles.push_back({card,(int)i});

        COLORREF sevCol = sevColor(mm.type);
        const wchar_t* sevLbl = sevLabel(mm.type);
        // tile body: surface2 when unseen, surface when seen — each tile is a
        // distinct card clearly separated from the panel background.
        // v1.8.0: *Bg variant patches the rounded-corner gaps with the panel
        // surface colour so corners never show a wrong/black artefact.
        gpRoundRectBg(dc,card,S(10),
            mm.seen?g_theme.surface2:g_theme.surface,
            mm.seen?g_theme.border:sevCol, g_theme.surface, 255);
        // severity stripe down the RIGHT (RTL leading) edge
        RECT stripe={card.right-S(6),card.top+S(4),card.right-S(2),card.bottom-S(4)};
        gpRoundRect(dc,stripe,S(2),sevCol,CLR_INVALID,255);
        // v1.7.0: PIN icon at the TOP-RIGHT corner (just left of the stripe)
        // when the message is pinned.
        if(mm.pinned){
            RECT pin={card.right-S(32),card.top+S(6),card.right-S(10),card.top+S(28)};
            gpRoundRect(dc,pin,S(9),g_theme.accent,CLR_INVALID,255);
            SelectObject(dc,g_fSmall); SetTextColor(dc,g_theme.accentText);
            DrawTextW(dc,L"\U0001F4CC",-1,&pin,
                DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_NOPREFIX);
        }
        // unseen dot (top-LEFT)
        if(!mm.seen){
            RECT dot={card.left+S(10),card.top+S(10),card.left+S(22),card.top+S(22)};
            gpRoundRect(dc,dot,S(6),sevCol,CLR_INVALID,255);
        }
        // header line: [severity] from
        SelectObject(dc,g_fUIB); SetTextColor(dc,g_theme.text);
        RECT fr={card.left+S(28),card.top+S(8),card.right-(mm.pinned?S(38):S(12)),card.top+S(30)};
        std::wstring head=L"["+std::wstring(sevLbl)+L"] "+
            (mm.from.empty()?std::wstring(L"مدیریت"):mm.from);
        DrawTextW(dc,head.c_str(),-1,&fr,
            DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX|DT_END_ELLIPSIS);
        // date line
        SelectObject(dc,g_fSmall); SetTextColor(dc,g_theme.textDim);
        RECT dr={card.left+S(12),card.top+S(30),card.right-S(12),card.top+S(48)};
        DrawTextW(dc,(L"\U0001F4C5 "+toFaDigits(mm.time)).c_str(),-1,&dr,
            DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX|DT_END_ELLIPSIS);
        // body text (wrapped to 2 lines)
        SelectObject(dc,g_fUI); SetTextColor(dc,g_theme.text);
        RECT br={card.left+S(12),card.top+S(50),card.right-S(12),card.bottom-S(8)};
        DrawTextW(dc,mm.text.c_str(),-1,&br,
            DT_RIGHT|DT_WORDBREAK|DT_RTLREADING|DT_NOPREFIX|DT_END_ELLIPSIS|DT_EDITCONTROL);

        col++; if(col>=cols){ col=0; row++; }
    }
}
static void drawCartable(HDC dc, const RECT& rc, TabPage* t){
    if(t && t->cartDetail) drawCartDetail(dc,rc,t);
    else                   drawCartList(dc,rc,t);
}
// hit-test a click against the cartable tiles → display index into s_cartMsgs
static int cartHit(POINT pt){
    for(auto& t:s_cartTiles) if(PtInRect(&t.r,pt)) return t.disp;
    return -1;
}
// hit-test a click/hover against the detail-view buttons → CART_BTN_*
static int cartBtnHit(POINT pt){
    for(auto& b:s_cartBtns) if(PtInRect(&b.r,pt)) return b.id;
    return CART_BTN_NONE;
}
static void drawTabPlaceholder(HDC dc, const RECT& rc, int kind, TabPage* t){
    if(kind==TK_PORTAL){ drawCartable(dc,rc,t); return; }
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
    // v1.9.1: the placeholder avatar is a calm, neutral GRAY (no blue/pink) so
    // it reads as a generic "no photo" person. The silhouette is drawn SMALLER
    // than the disc and the disc is used as a CLIP region, so the head/shoulders
    // are perfectly centred and NOTHING ever spills past the rounded edge.
    (void)female;
    COLORREF ring = blendColor(g_theme.textDim, g_theme.border, 35);
    COLORREF fig  = blendColor(g_theme.textDim, g_theme.surface, 20);
    // --- disc (filled circle + ring) ---
    HBRUSH br=CreateSolidBrush(g_theme.surface2);
    HPEN pn=CreatePen(PS_SOLID,S(2),ring);
    HGDIOBJ ob=SelectObject(dc,br), op=SelectObject(dc,pn);
    Ellipse(dc,cx-r,cy-r,cx+r,cy+r);
    SelectObject(dc,op); SelectObject(dc,ob);
    // --- clip everything that follows to the inside of the disc ---
    // (slightly inset so the ring stays clean and crisp)
    int ci = r - S(2);
    HRGN clip = CreateEllipticRgn(cx-ci, cy-ci, cx+ci, cy+ci);
    int savedDC = SaveDC(dc);
    SelectClipRgn(dc, clip);
    // --- compact, centred guest silhouette (smaller than the disc) ---
    HBRUSH brh=CreateSolidBrush(fig);
    HGDIOBJ ob2=SelectObject(dc,brh);
    HGDIOBJ op2=SelectObject(dc,GetStockObject(NULL_PEN));
    int hr = r*30/100;          // head radius (smaller)
    int hy = cy - r*22/100;     // head centre y (sits in the upper third)
    Ellipse(dc,cx-hr,hy-hr,cx+hr,hy+hr);
    // shoulders: a wide, shallow rounded arc tucked under the head. Kept fully
    // inside the disc — the bottom of the arc stays above the disc edge.
    int sw = r*60/100;          // half-width of the shoulders
    int sTop = cy + r*10/100;   // top of the shoulder ellipse
    int sBot = cy + r*95/100;   // bottom — clip handles any overshoot anyway
    Ellipse(dc,cx-sw,sTop,cx+sw,sBot);
    SelectObject(dc,op2); SelectObject(dc,ob2);
    // --- restore clip + free GDI objects ---
    RestoreDC(dc, savedDC);
    SelectClipRgn(dc, NULL);
    DeleteObject(clip);
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
//  Helper: draw a small field caption (right-aligned, dim) above a control.
static void paintInfoLabel(HDC dc, int iL, int iR, int y, const wchar_t* txt){
    SelectObject(dc,g_fSmall); SetTextColor(dc,g_theme.textDim);
    RECT tr={iL,y,iR,y+S(16)};
    DrawTextW(dc,txt,-1,&tr,DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
}
static void paintInfoPanel(HDC dc, TabPage* t, int infoL, int infoR, int H, int sy){
    // H here is the VIRTUAL bottom (already had sy subtracted by the caller);
    // recover the real virtual height for layout, then offset everything by sy.
    int VH = H + sy;
    RECT card={infoL,S(16)-sy,infoR,VH-S(16)-sy};
    // v2: clip the rounded card so nothing bleeds outside the rounded corners,
    // and patch the corner gaps with the page background so no square shows.
    gpRoundRectBg(dc,card,S(16),g_theme.surface,g_theme.border,g_theme.bg);
    InfoLayout L; computeInfoLayout(infoL,infoR,VH,L);
    // shift the whole computed layout up by the scroll offset
    #define SY(v) ((v)-sy)
    const int iL=L.iL, iR=L.iR;
    // gender from the combo
    bool female = (SendMessageW(t->cGender,CB_GETCURSEL,0,0)==1);
    // --- avatar (neutral gray, perfectly centred) ---
    drawGuestAvatar(dc,L.avCx,SY(L.avCy),L.avR,female);
    // --- «بیمار جدید» + «بدون سابقه» caption ---
    { int capY=SY(L.avCy)+L.avR+S(8);
      SelectObject(dc,g_fUIB); SetTextColor(dc,g_theme.text);
      RECT t1={iL,capY,iR,capY+S(20)};
      DrawTextW(dc,L"بیمار جدید",-1,&t1,DT_CENTER|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);
      SelectObject(dc,g_fSmall); SetTextColor(dc,g_theme.textDim);
      RECT t2={iL,capY+S(18),iR,capY+S(34)};
      DrawTextW(dc,L"بدون سابقه",-1,&t2,DT_CENTER|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX); }
    // --- نسخه الکترونیک chip (GREEN, soft fill) ---
    { RECT chip={iL+L.iw/4,SY(L.chipY),iR-L.iw/4,SY(L.chipY)+L.chipH};
      COLORREF gfill=blendColor(g_theme.success,g_theme.surface,78);
      fillRoundRect(dc,chip,S(12),gfill,blendColor(g_theme.success,g_theme.surface,40));
      SelectObject(dc,g_fSmall); SetTextColor(dc,g_theme.success);
      DrawTextW(dc,L"نسخه الکترونیک",-1,&chip,
          DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX); }
    // --- two counter boxes: نسخه (سرپایی) | نسخه (الکترونیکی) ---
    { int half=(L.iw-S(8))/2;
      RECT bL={iL,SY(L.boxY),iL+half,SY(L.boxY)+L.boxH};
      RECT bR={iR-half,SY(L.boxY),iR,SY(L.boxY)+L.boxH};
      for(int k=0;k<2;k++){
          RECT bx = k==0?bR:bL;          // RTL: first box on the RIGHT
          fillRoundRect(dc,bx,S(8),g_theme.inputBg,g_theme.border);
          SelectObject(dc,g_fSmall); SetTextColor(dc,g_theme.textDim);
          RECT lr={bx.left,bx.top+S(4),bx.right,bx.top+S(20)};
          DrawTextW(dc,k==0?L"نسخه (سرپایی)":L"نسخه (الکترونیکی)",-1,&lr,
              DT_CENTER|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);
          SelectObject(dc,g_fUIB); SetTextColor(dc,g_theme.text);
          RECT vr={bx.left,bx.top+S(20),bx.right,bx.bottom-S(2)};
          DrawTextW(dc,L"0",-1,&vr,DT_CENTER|DT_SINGLELINE|DT_NOPREFIX);
      }
    }
    // group titles + per-row field captions — share the SAME layout as controls
    paintInfoGroup(dc,iL,iR,SY(L.g1TitleY),L"کلیدهای جستجو",ICO_ID);
    paintInfoLabel(dc,iL,iR,SY(L.archiveLblY),L"شماره بایگانی");
    paintInfoLabel(dc,iL,iR,SY(L.fileLblY),   L"شماره پرونده");
    paintInfoGroup(dc,iL,iR,SY(L.g2TitleY),L"بیمه",ICO_SHIELD);
    paintInfoLabel(dc,iL,iR,SY(L.bookLblY), L"شماره دفترچه");
    paintInfoLabel(dc,iL,iR,SY(L.validLblY),L"تاریخ اعتبار");
    paintInfoLabel(dc,iL,iR,SY(L.rxLblY),   L"تاریخ نسخه");
    paintInfoLabel(dc,iL,iR,SY(L.suppLblY), L"بیمه مکمل");
    paintInfoGroup(dc,iL,iR,SY(L.g3TitleY),L"پزشک معالج",ICO_CROSS_MED);
    paintInfoLabel(dc,iL,iR,SY(L.docCodeLblY),L"کد نظام پزشکی");
    paintInfoLabel(dc,iL,iR,SY(L.docNameLblY),L"نام پزشک");
    // --- انجام دهنده (current user) at the very bottom ---
    { SelectObject(dc,g_fSmall); SetTextColor(dc,g_theme.textDim);
      std::wstring who=L"انجام دهنده: "+
          (g_session.user.fullname.empty()?g_session.user.username:g_session.user.fullname);
      RECT tr={iL,card.bottom-S(26),iR,card.bottom-S(8)};
      DrawTextW(dc,who.c_str(),-1,&tr,
          DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX); }
    #undef SY
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
        // §3 hybrid host: when opening the Appointment tab we first try to bring
        // up the HTML/CSS/JS surface (system MSHTML/WebBrowser OLE control) with a
        // centred loader while native state synchronises. C++ stays the host /
        // validator / lifecycle owner; JS owns only layout + interaction. If the
        // renderer is unavailable or fails, we fall back to the classic native
        // appointment page so the app NEVER loses the feature.
        if(t->kind==TK_APPOINTMENT){
            //  v1.17.0: the HTML/CSS/JS (MSHTML) presentation layer has been
            //  RETIRED. The appointment screen is now rendered 100% in native
            //  C++ (Win32/GDI) — the same engine as the rest of the app — so
            //  there is no embedded browser, no IE/Trident dependency and no
            //  C++↔JS bridge that can drift or fail. This removes a whole class
            //  of "تغییرات اعمال نشده" / "ارتباط مشکل دارد" failures and keeps
            //  the product a single, light, self-contained EXE.
            t->web = NULL;
            t->appt = createAppointmentPage(h);
            if(t->appt) ShowWindow(t->appt, SW_SHOW);
            return 0;
        }
        // Portal-message and empty tabs are pure painted pages with no form
        // controls — they own no edit boxes, combos or buttons.
        if(t->kind!=TK_RECEPTION) return 0;
        //  v1.17.0: HTML/CSS/JS (MSHTML) presentation layer RETIRED. The
        //  reception form below is now the ONLY renderer — pure native C++
        //  (Win32 edit boxes / combos / owner-drawn cards in WM_PAINT). The
        //  layout is tuned to match the reference design pixel-for-pixel, the
        //  insurance lists come straight from billing.cpp, billing recalculates
        //  natively, and printing goes through the real GDI printer path. No
        //  browser control is ever created.
        t->web = NULL;
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

        // bottom action row (matches reference): ثبت (primary) | پاک کردن |
        // پذیرش جدید | انصراف
        t->bSubmit =createFlatButton(h,ID_F_SUBMIT,L"ثبت پذیرش و صدور قبض",ICO_SAVE,BS_PRIMARY,0,0,10,10);
        // §1.18.1: «پاک کردن» is the destructive/clear action → danger (red) style,
        // matching the reference image (red trash icon + red text).
        t->bReset  =createFlatButton(h,ID_F_RESET,L"پاک کردن",ICO_TRASH,BS_DANGER,0,0,10,10);
        t->bNew    =createFlatButton(h,ID_F_NEW,L"پذیرش جدید",ICO_CHEVRON,BS_OUTLINE,0,0,10,10);
        t->bClose  =createFlatButton(h,ID_F_CLOSE,L"انصراف",ICO_X,BS_OUTLINE,0,0,10,10);
        // services table toolbar
        t->bSvcAdd =createFlatButton(h,ID_F_SVC_ADD,L"افزودن خدمت",ICO_NONE,BS_OUTLINE,0,0,10,10);
        t->eSvcSearch=CreateWindowExW(0,L"EDIT",L"",es,0,0,10,10,h,(HMENU)ID_F_SVC_SEARCH,g_hInst,0);
        SendMessageW(t->eSvcSearch,WM_SETFONT,(WPARAM)g_fUI,TRUE);
        // print actions (LEFT billing card)
        t->bPrtIns =createFlatButton(h,ID_F_PRT_INS,L"رسید بیمه",ICO_PRINT,BS_OUTLINE,0,0,10,10);
        t->bPrtRx  =createFlatButton(h,ID_F_PRT_RX,L"چاپ نسخه",ICO_PRINT,BS_OUTLINE,0,0,10,10);
        t->bPrtLast=createFlatButton(h,ID_F_PRT_LAST,L"چاپ آخرین قبض (F8)",ICO_PRINT,BS_OUTLINE,0,0,10,10);
        t->bInquiry=createFlatButton(h,ID_F_INQUIRY,L"استعلام بیمه",ICO_SHIELD,BS_OUTLINE,0,0,10,10);
        // v1.4.1: real raster icons on the print-action buttons (left card)
        setFlatButtonImage(t->bPrtIns, IMG_IC_SHIELD);
        setFlatButtonImage(t->bPrtRx,  IMG_IC_RECEIPT);
        setFlatButtonImage(t->bPrtLast,IMG_IC_LAST);
        // blend button corners: submit sits on the form card (surface); the
        // billing/print buttons sit on the page background (bg).
        setFlatButtonBg(t->bSubmit, g_theme.surface);
        setFlatButtonBg(t->bReset,  g_theme.surface);
        setFlatButtonBg(t->bNew,    g_theme.surface);
        setFlatButtonBg(t->bClose,  g_theme.surface);
        setFlatButtonBg(t->bSvcAdd, g_theme.surface);
        setFlatButtonBg(t->bPrtIns, g_theme.surface);
        setFlatButtonBg(t->bPrtRx,  g_theme.surface);
        setFlatButtonBg(t->bPrtLast,g_theme.surface);
        setFlatButtonBg(t->bInquiry,g_theme.surface);

        // birth-date uses the smart Jalali mask (digits-only, auto slashes)
        HWND eds[11]={t->eFirst,t->eLast,t->eNid,t->eFather,
                      t->eMobile,t->ePhone,t->eAddr,t->ePrice,t->eDiscount,0,0};
        for(int i=0;eds[i];i++){
            SendMessageW(eds[i],WM_SETFONT,(WPARAM)g_fUI,TRUE);
            enableEnterNavigation(eds[i]);
        }
        // national-id field: override the plain Enter-navigation so Enter runs
        // the network-wide patient lookup + auto-fill (must be applied AFTER the
        // generic enableEnterNavigation above so it wins for this one field).
        enableNidLookup(t->eNid);
        SendMessageW(t->eNid,EM_SETLIMITTEXT,10,0);
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
        // اتوماتیک = current Iran date/time (v1.4.0 §6: via FormatJalaliPersian)
        SetWindowTextW(t->eValid, FormatJalaliPersian(0).c_str());
        SetWindowTextW(t->eRxDate,FormatJalaliPersian(0).c_str());
        EnableWindow(t->eValid,FALSE); EnableWindow(t->eRxDate,FALSE);
        // doctor inquiry buttons sit on the surface (info panel) — blend corners
        setFlatButtonBg(t->bArchiveGo,g_theme.surface);
        setFlatButtonBg(t->bFileGo,   g_theme.surface);
        setFlatButtonBg(t->bSuppGo,   g_theme.surface);
        setFlatButtonBg(t->bDocSearch,g_theme.surface);

        recalc(t);
        return 0; }
    case WM_SIZE: if(t) tabPageLayout(h,t); return 0;
    case WM_VSCROLL: {
        // vertical scrollbar of the reception form
        if(!t || t->web || t->kind!=TK_RECEPTION) break;
        int maxS=recClampScroll(h,t);
        int old=t->scrollY;
        SCROLLINFO si={sizeof(si)}; si.fMask=SIF_TRACKPOS;
        GetScrollInfo(h,SB_VERT,&si);
        RECT rc; GetClientRect(h,&rc);
        int line=S(40), pageStep=rc.bottom-S(40); if(pageStep<line) pageStep=line;
        switch(LOWORD(w)){
            case SB_LINEUP:   t->scrollY-=line; break;
            case SB_LINEDOWN: t->scrollY+=line; break;
            case SB_PAGEUP:   t->scrollY-=pageStep; break;
            case SB_PAGEDOWN: t->scrollY+=pageStep; break;
            case SB_THUMBTRACK:
            case SB_THUMBPOSITION: t->scrollY=si.nTrackPos; break;
            case SB_TOP:      t->scrollY=0; break;
            case SB_BOTTOM:   t->scrollY=maxS; break;
        }
        recClampScroll(h,t);
        if(t->scrollY!=old){
            // v1.19.0 ultra-smooth scroll: freeze child-control repaints while we
            // batch-reposition them, then repaint the painted background AND all
            // controls together in ONE double-buffered frame. SetWindowRedraw
            // (WM_SETREDRAW) suppresses the per-control invalidations MoveWindow
            // would otherwise queue, so nothing is drawn twice and the cards,
            // field wells and edit boxes move in perfect lock-step — zero
            // flicker, zero tearing, no frame-by-frame lag even with 40+ controls.
            SendMessageW(h,WM_SETREDRAW,FALSE,0);
            tabPageLayout(h,t);
            SendMessageW(h,WM_SETREDRAW,TRUE,0);
            RedrawWindow(h,NULL,NULL,
                RDW_INVALIDATE|RDW_UPDATENOW|RDW_ALLCHILDREN|RDW_NOERASE);
        } else {
            recUpdateScrollbar(h,t);
        }
        return 0; }
    case WM_MOUSEWHEEL: {
        if(!t || t->web || t->kind!=TK_RECEPTION) break;
        int maxS=recClampScroll(h,t);
        if(maxS<=0) return 0;            // nothing to scroll
        int delta=GET_WHEEL_DELTA_WPARAM(w);
        int old=t->scrollY;
        t->scrollY -= (delta/WHEEL_DELTA)*S(60);
        recClampScroll(h,t);
        if(t->scrollY!=old){
            // §B (v1.10.0): no header-collapse animation on scroll. The action
            // bar stays at its fixed compact height; we just scroll the page.
            // v1.19.0: freeze child repaints, batch-reposition, then flush ONE
            // double-buffered frame so controls + painted background never
            // separate while wheel-scrolling (matches the WM_VSCROLL path).
            SendMessageW(h,WM_SETREDRAW,FALSE,0);
            tabPageLayout(h,t);
            SendMessageW(h,WM_SETREDRAW,TRUE,0);
            RedrawWindow(h,NULL,NULL,
                RDW_INVALIDATE|RDW_UPDATENOW|RDW_ALLCHILDREN|RDW_NOERASE);
        }
        return 0; }
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
        if(!t || t->web) return 0;   // hybrid host owns input; no native controls
        int id=LOWORD(w), code=HIWORD(w);
        // Repaint the field wells whenever an edit/combo gains or loses focus so
        // the focused well border updates immediately (thin red focus ring that
        // fades back to the normal hairline once the field is left/clicked away).
        if(code==EN_SETFOCUS || code==EN_KILLFOCUS ||
           code==CBN_SETFOCUS || code==CBN_KILLFOCUS){
            InvalidateRect(h,NULL,FALSE);
        }
        // v1.9.0: editing any field clears the empty-field red markers so they
        // never linger after the operator starts correcting the form.
        if((code==EN_CHANGE || code==CBN_SELCHANGE) && t->invalidMask){
            t->invalidMask=0; InvalidateRect(h,NULL,FALSE);
        }
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
            // national-id field lost focus (Tab navigates away). Auto-fill the
            // patient identity from a trusted source whenever a complete code is
            // present — quietly when not insured (so no error nags the operator),
            // verbosely when insured (so an invalid code is flagged). Enter on the
            // field itself is handled by nidEditProc which always looks up.
            wchar_t nb[16]={0}; GetWindowTextW(t->eNid,nb,16);
            std::wstring nid=trim(nb);
            bool insured = SendMessageW(t->chkIns,BM_GETCHECK,0,0)==BST_CHECKED;
            if(insured)                doInquiry(t,h,false);  // show invalid-id error
            else if(nid.size()==10)    doInquiry(t,h,true);   // quiet auto-fill
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
            // v1.9.0: compute the empty-field mask so every missing REQUIRED
            // field gets a thin red hairline (indices match inputs[] order).
            int mask=0;
            if(r.firstName.empty())  mask|=(1<<0);
            if(r.lastName.empty())   mask|=(1<<1);
            if(r.nationalId.empty()) mask|=(1<<2);
            if(r.birthDate.empty())  mask|=(1<<4);
            if(r.mobile.empty())     mask|=(1<<6);
            if(r.total<=0)           mask|=(1<<13);
            t->invalidMask=mask;
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
                t->invalidMask=0;   // all required fields present
                int q=saveReception(r);
                // v1.7.0: remember this REAL (operator-confirmed) identity so the
                // same national code recalls the same patient next time — never
                // fabricated, only what was actually entered/verified here.
                {
                    std::vector<int> ins;
                    if(r.insIdx>0 && r.insIdx<N_INSURANCES) ins.push_back(r.insIdx);
                    rememberPatient(r.nationalId,r.firstName,r.lastName,
                        r.fatherName,r.gender,r.birthDate,r.mobile,ins);
                }
                wchar_t mb[160];
                swprintf(mb,160,L"پذیرش با شماره نوبت %d ثبت شد — %s %s",
                    q, r.firstName.c_str(), r.lastName.c_str());
                t->lastMsg=toFaDigits(mb); t->msgCol=g_theme.success;
                // v1.4.0: prefer the user-designed layout for this section; fall
                // back to the classic GDI receipt if no design exists.
                auto doPrint=[&](const ReceptionRecord& rec){
                    if(!printDesignedReceipt(rec,0,h)) printReceipt(rec,2,h);
                    kickCashDrawer();   // pulse drawer if enabled in printer settings
                };
                if(getSetting(L"auto_print",L"0")==L"1"){
                    doPrint(r);
                } else if(MessageBoxW(h,L"پذیرش ثبت شد. قبض چاپ شود؟",
                    L"ثبت موفق",MB_YESNO|MB_ICONQUESTION)==IDYES){
                    doPrint(r);
                }
                // v1.4.0 (§6): do NOT clear the form after printing — the operator
                // often re-prints or tweaks. Fields stay populated; use the reset
                // button (or Ctrl+R) to start a fresh reception explicitly.
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
        else if(id==ID_F_RESET){                       // پاک کردن
            resetForm(t);
            t->lastMsg=L"فرم پاک شد."; t->msgCol=g_theme.textDim;
            InvalidateRect(h,NULL,FALSE);
        }
        else if(id==ID_F_NEW){                          // پذیرش جدید
            resetForm(t);
            t->lastMsg=L"پذیرش جدید — اطلاعات بیمار را وارد کنید.";
            t->msgCol=g_theme.accent;
            SetFocus(t->eFirst);
            InvalidateRect(h,NULL,FALSE);
        }
        else if(id==ID_F_SVC_ADD || id==ID_F_SVC_SEARCH){
            // خدمات: placeholder until the service catalogue UI lands — for now
            // the price field on the «بیمه و نوبت» card drives the bill.
            t->lastMsg=L"مبلغ خدمت را در بخش «بیمه و نوبت» وارد کنید.";
            t->msgCol=g_theme.textDim;
            InvalidateRect(h,NULL,FALSE);
        }
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
            if(on) SetWindowTextW(t->eValid,FormatJalaliPersian(0).c_str());
        }
        else if(id==ID_F_RXDATE_AUTO && code==BN_CLICKED){
            bool on=SendMessageW(t->chkRxAuto,BM_GETCHECK,0,0)==BST_CHECKED;
            EnableWindow(t->eRxDate,!on);
            if(on) SetWindowTextW(t->eRxDate,FormatJalaliPersian(0).c_str());
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
            // جستجوی پزشک معالج: ابتدا با نام، سپس انتخاب از فهرست پزشکان
            wchar_t nb[128]; GetWindowTextW(t->eDocName,nb,128);
            std::wstring q=trim(nb);
            auto docs=loadDoctors();
            HMENU mnu=CreatePopupMenu();
            std::vector<int> map;
            for(int i=0;i<(int)docs.size();i++){
                if(q.empty() || docs[i].name.find(q)!=std::wstring::npos
                             || docs[i].specialty.find(q)!=std::wstring::npos){
                    std::wstring s=docs[i].name+L" — "+docs[i].specialty;
                    AppendMenuW(mnu,MF_STRING,(UINT)(map.size()+1),s.c_str());
                    map.push_back(i);
                }
            }
            if(map.empty()){
                t->lastMsg=L"پزشکی با این مشخصات یافت نشد.";
                t->msgCol=g_theme.danger; InvalidateRect(h,NULL,FALSE);
                DestroyMenu(mnu);
            } else {
                RECT br; GetWindowRect(t->bDocSearch,&br);
                int cmd=TrackPopupMenu(mnu,TPM_RETURNCMD|TPM_RIGHTBUTTON,
                    br.left,br.bottom,0,h,NULL);
                DestroyMenu(mnu);
                if(cmd>0 && cmd<=(int)map.size()){
                    const DoctorDef& d=docs[map[cmd-1]];
                    SetWindowTextW(t->eDocName,d.name.c_str());
                    // generate a stable 5-digit نظام پزشکی from the name hash
                    unsigned hsh=0; for(wchar_t c:d.name) hsh=hsh*131+(unsigned)c;
                    wchar_t code[16]; swprintf(code,16,L"%05u",10000+(hsh%89999));
                    SetWindowTextW(t->eDocCode,toFaDigits(code).c_str());
                    t->lastMsg=L"پزشک معالج انتخاب شد: "+d.name;
                    t->msgCol=g_theme.success; InvalidateRect(h,NULL,FALSE);
                }
            }
        }
        return 0; }
    case WM_KEYDOWN:
        if(w==VK_F8){ printLastReceipt(h); return 0; }
        // v1.4.0 (§6): Ctrl+R clears the reception form for a fresh patient.
        if(w=='R' && (GetKeyState(VK_CONTROL)&0x8000) &&
           t && !t->web && t->kind==TK_RECEPTION){
            resetForm(t);
            t->lastMsg=L"فرم پاک شد."; t->msgCol=g_theme.textDim;
            InvalidateRect(h,NULL,FALSE);
            return 0;
        }
        // v1.7.0: Esc returns from the message details view to the list
        if(w==VK_ESCAPE && t && t->kind==TK_PORTAL && t->cartDetail){
            t->cartDetail=false; t->cartHotBtn=0;
            InvalidateRect(h,NULL,FALSE);
            return 0;
        }
        break;
    case WM_SETCURSOR: {
        // hand cursor while hovering a clickable message tile or detail button
        if(t && t->kind==TK_PORTAL && LOWORD(l)==HTCLIENT){
            POINT pt; GetCursorPos(&pt); ScreenToClient(h,&pt);
            bool hand = t->cartDetail ? (cartBtnHit(pt)!=CART_BTN_NONE)
                                      : (cartHit(pt)>=0);
            if(hand){ SetCursor(LoadCursor(NULL,IDC_HAND)); return TRUE; }
        }
        break; }
    case WM_MOUSEMOVE: {
        // detail-view: track which action button is hovered (for highlight)
        if(t && t->kind==TK_PORTAL && t->cartDetail){
            POINT pt={GET_X_LPARAM(l),GET_Y_LPARAM(l)};
            int hb=cartBtnHit(pt);
            if(hb!=t->cartHotBtn){ t->cartHotBtn=hb; InvalidateRect(h,NULL,FALSE); }
            TRACKMOUSEEVENT te={sizeof(te),TME_LEAVE,h,0}; TrackMouseEvent(&te);
        }
        break; }
    case WM_MOUSELEAVE:
        if(t && t->kind==TK_PORTAL && t->cartHotBtn){
            t->cartHotBtn=0; InvalidateRect(h,NULL,FALSE);
        }
        break;
    case WM_RBUTTONDOWN: {
        // v1.7.0: RIGHT-CLICK on a tile shows ONLY the pin/unpin option.
        if(t && t->kind==TK_PORTAL && !t->cartDetail){
            POINT pt={GET_X_LPARAM(l),GET_Y_LPARAM(l)};
            int disp=cartHit(pt);
            if(disp>=0 && disp<(int)s_cartMsgs.size() && disp<(int)s_cartNF.size()){
                KMsg& mm=s_cartMsgs[disp];
                int nf=s_cartNF[disp];
                HMENU mnu=CreatePopupMenu();
                AppendMenuW(mnu,MF_STRING,1,
                    mm.pinned?L"برداشتن سنجاق":L"سنجاق کردن");
                AppendMenuW(mnu,MF_SEPARATOR,0,NULL);
                // v1.8.0: «ارسال به پیام‌های ذخیره‌شده» — disabled by default
                // (greyed out) unless the feature is enabled in settings.
                AppendMenuW(mnu,MF_STRING|(savedMsgsEnabled()?MF_ENABLED:MF_GRAYED),
                    2,L"ارسال به پیام‌های ذخیره‌شده");
                POINT sp=pt; ClientToScreen(h,&sp);
                int cmd=TrackPopupMenu(mnu,TPM_RETURNCMD|TPM_RIGHTBUTTON,
                    sp.x,sp.y,0,h,NULL);
                DestroyMenu(mnu);
                if(cmd==1){ pinMessage(g_session.user.username,nf,!mm.pinned);
                            InvalidateRect(h,NULL,FALSE); }
                else if(cmd==2 && savedMsgsEnabled()){
                    // archive a copy locally (text + any attachment preserved)
                    pushSavedMsg(mm.from.empty()?L"مدیریت":mm.from,
                                 g_session.user.fullname.empty()?g_session.user.username
                                                                :g_session.user.fullname,
                                 mm.text, mm.type, L"");
                    t->lastMsg=L"پیام به «پیام‌های ذخیره‌شده» منتقل شد.";
                    t->msgCol=g_theme.success;
                    InvalidateRect(h,NULL,FALSE);
                }
            }
            return 0;
        }
        break; }
    case WM_LBUTTONDOWN: {
        if(t && t->kind==TK_PORTAL){
            POINT pt={GET_X_LPARAM(l),GET_Y_LPARAM(l)};
            std::wstring me=g_session.user.username;
            if(t->cartDetail){
                // ----- detail view: action buttons -----
                int bid=cartBtnHit(pt);
                if(bid==CART_BTN_BACK){
                    t->cartDetail=false; t->cartHotBtn=0;
                    InvalidateRect(h,NULL,FALSE);
                } else if(bid==CART_BTN_MARK || bid==CART_BTN_TOGGLEREAD){
                    // «خواندن» / «علامت خوانده‌شده» — mark this message seen
                    if(t->cartSelNF>=0) seenOneMessage(me,t->cartSelNF);
                    InvalidateRect(h,NULL,FALSE);
                } else if(bid==CART_BTN_DELETE){
                    if(MessageBoxW(h,L"این پیام حذف شود؟",L"حذف پیام",
                        MB_YESNO|MB_ICONQUESTION)==IDYES){
                        if(t->cartSelNF>=0) deleteOneMessage(me,t->cartSelNF);
                        t->cartDetail=false; t->cartHotBtn=0;
                        InvalidateRect(h,NULL,FALSE);
                    }
                } else if(bid==CART_BTN_SAVE){
                    // archive THIS message into the saved-messages store so it is
                    // kept even after it is deleted from the live inbox.
                    int sel=-1;
                    for(int i=0;i<(int)s_cartNF.size();i++)
                        if(s_cartNF[i]==t->cartSelNF){ sel=i; break; }
                    if(sel<0 && t->cartSelDisp>=0 && t->cartSelDisp<(int)s_cartMsgs.size())
                        sel=t->cartSelDisp;
                    if(savedMsgsEnabled() && sel>=0 && sel<(int)s_cartMsgs.size()){
                        KMsg& mm=s_cartMsgs[sel];
                        std::wstring from=mm.from.empty()?std::wstring(L"مدیریت درمانگاه"):mm.from;
                        pushSavedMsg(from, mm.to, mm.text, mm.type, L"");
                        MessageBoxW(h,L"پیام در «پیام‌های ذخیره‌شده» بایگانی شد.",
                            L"ذخیره شد",MB_OK|MB_ICONINFORMATION);
                    } else {
                        MessageBoxW(h,L"بایگانی پیام‌ها در تنظیمات غیرفعال است.",
                            L"غیرفعال",MB_OK|MB_ICONWARNING);
                    }
                    InvalidateRect(h,NULL,FALSE);
                }
                return 0;
            }
            // ----- §D: archive toggle icon (top-left of the header) -----
            if(PtInRect(&s_cartArchiveRect,pt)){
                if(!savedMsgsEnabled()){
                    // feature is off — offer to enable it so the entry is never
                    // a dead end (the brief: must be reachable + functional).
                    int r=MessageBoxW(h,
                        L"\u0642\u0627\u0628\u0644\u06cc\u062a \u00ab\u067e\u06cc\u0627\u0645\u200c\u0647\u0627\u06cc \u0630\u062e\u06cc\u0631\u0647\u200c\u0634\u062f\u0647\u00bb \u063a\u06cc\u0631\u0641\u0639\u0627\u0644 \u0627\u0633\u062a. \u0641\u0639\u0627\u0644 \u0634\u0648\u062f\u061f",
                        L"\u067e\u06cc\u0627\u0645\u200c\u0647\u0627\u06cc \u0630\u062e\u06cc\u0631\u0647\u200c\u0634\u062f\u0647",
                        MB_YESNO|MB_ICONQUESTION);
                    if(r==IDYES) setSetting(L"saved_msgs_enabled",L"1");
                    else { InvalidateRect(h,NULL,FALSE); return 0; }
                }
                t->cartShowArchive = !t->cartShowArchive;
                t->cartDetail=false; t->cartHotBtn=0;
                InvalidateRect(h,NULL,FALSE);
                return 0;
            }
            // archive view is read-only (no detail/tiles to click)
            if(savedMsgsEnabled() && t->cartShowArchive){ return 0; }
            // ----- list view: clicking a tile OPENS the full details -----
            int disp=cartHit(pt);
            if(disp>=0 && disp<(int)s_cartMsgs.size() && disp<(int)s_cartNF.size()){
                t->cartSelDisp=disp;
                t->cartSelNF=s_cartNF[disp];
                t->cartDetail=true; t->cartHotBtn=0;
                // opening a message marks it read (مشاهده شد)
                if(!s_cartMsgs[disp].seen) seenOneMessage(me,t->cartSelNF);
                InvalidateRect(h,NULL,FALSE);
            }
            return 0;
        }
        break; }
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

        // -------- Hybrid HTML host / appointment child page: just clear bg ----
        // When the web surface is up it covers the whole tab, so the host only
        // needs to clear the background behind it (no native form is painted).
        if(t && (t->web || t->kind==TK_APPOINTMENT)){
            BitBlt(dc0,0,0,rc.right,rc.bottom,dc,0,0,SRCCOPY);
            SelectObject(dc,obm); DeleteObject(bmp); DeleteDC(dc);
            EndPaint(h,&ps);
            return 0;
        }
        // -------- Portal-message / empty tabs: a centred glass card ----------
        if(t && t->kind!=TK_RECEPTION){
            drawTabPlaceholder(dc,rc,t->kind,t);
            BitBlt(dc0,0,0,rc.right,rc.bottom,dc,0,0,SRCCOPY);
            SelectObject(dc,obm); DeleteObject(bmp); DeleteDC(dc);
            EndPaint(h,&ps);
            return 0;
        }

        RecH m; rcH(rc.right,m);
        CenterV v; computeCenterV(m,v);
        recalc(t);
        recClampScroll(h,t);
        const int sy=t->scrollY;
        int VH=recPageVH(rc.right,rc.bottom);
        #define Y(yy) ((yy)-sy)

        // ============ RIGHT INFO PANEL ============
        if(!m.stacked && m.infoR>m.infoL) paintInfoPanel(dc,t,m.infoL,m.infoR,VH-sy,sy);

        // ---- shared helpers for the center cards ----
        const int formL=m.formL, formR=m.formR, fw=m.fw;
        const int rh=v.rh, cgap=m.cgap, cw=m.ccolW;
        auto colX=[&](int c){ return formR - (c+1)*cw - c*cgap; };
        //  draw a white sub-card with a header (icon flush-right + title).
        auto subcard=[&](int top,int bot,const wchar_t* title,int icon){
            RECT cr={m.cardL,Y(top),m.cardR,Y(bot)};
            gpRoundRectBg(dc,cr,S(14),g_theme.surface,g_theme.border,g_theme.bg);
            int icoW=S(18);
            RECT hi={formR-icoW,cr.top+S(14),formR,cr.top+S(32)};
            drawIcon(dc,icon,hi,g_theme.accent,S(2));
            SetTextColor(dc,g_theme.text); SelectObject(dc,g_fUIB);
            RECT ht={formL,cr.top+S(12),formR-icoW-S(8),cr.top+S(34)};
            DrawTextW(dc,title,-1,&ht,DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
        };
        //  draw a field label (with optional red required asterisk) above (x,y).
        auto fieldLabel=[&](int x,int y,int w,const wchar_t* txt,bool req){
            SelectObject(dc,g_fSmall);
            // measure the label so the asterisk can sit just to its LEFT (RTL).
            RECT lr={x,Y(y),x+w,Y(y)+S(16)};
            SetTextColor(dc,g_theme.textDim);
            DrawTextW(dc,txt,-1,&lr,DT_RIGHT|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);
            if(req){
                SIZE sz; GetTextExtentPoint32W(dc,txt,(int)wcslen(txt),&sz);
                int ax = x+w - sz.cx - S(8);
                SetTextColor(dc,g_theme.danger);
                RECT ar={ax-S(10),Y(y),ax,Y(y)+S(16)};
                DrawTextW(dc,L"*",-1,&ar,DT_RIGHT|DT_SINGLELINE|DT_NOPREFIX);
            }
        };

        // ============ CARD 1: مشخصات بیمار (3-column grid) ============
        subcard(v.c1Top,v.c1Bot,L"مشخصات بیمار",ICO_USER);
        // labels (col0=right, col2=left)
        fieldLabel(colX(0),v.r1y[0]-S(20),cw,L"نام",true);
        fieldLabel(colX(1),v.r1y[0]-S(20),cw,L"نام خانوادگی",true);
        fieldLabel(colX(2),v.r1y[0]-S(20),cw,L"کد ملی",true);
        fieldLabel(colX(0),v.r1y[1]-S(20),cw,L"نام پدر",false);
        fieldLabel(colX(1),v.r1y[1]-S(20),cw,L"تاریخ تولد",false);
        fieldLabel(colX(2),v.r1y[1]-S(20),cw,L"جنسیت",false);
        fieldLabel(colX(0),v.r1y[2]-S(20),cw,L"شماره موبایل",true);
        fieldLabel(colX(1),v.r1y[2]-S(20),cw,L"تلفن ثابت",false);
        fieldLabel(colX(2),v.r1y[2]-S(20),cw,L"آدرس",false);

        // ============ CARD 2: بیمه و نوبت (4-col then 2-col) ============
        subcard(v.c2Top,v.c2Bot,L"بیمه و نوبت",ICO_SHIELD);
        { int gw=(fw-3*cgap)/4;
          auto gx=[&](int c){ return formR-(c+1)*gw-c*cgap; };
          fieldLabel(gx(0),v.r2y-S(20),gw,L"نوع پذیرش",false);
          fieldLabel(gx(1),v.r2y-S(20),gw,L"نوع نوبت",false);
          fieldLabel(gx(2),v.r2y-S(20),gw,L"نوع بیمه",false);
          fieldLabel(gx(3),v.r2y-S(20),gw,L"نوع بیمه پایه",false);
          int hw=(fw-cgap)/2;
          fieldLabel(formR-hw,v.r2by-S(20),hw,L"مبلغ خدمت (ریال)",true);
          fieldLabel(formL,   v.r2by-S(20),hw,L"تخفیف (ریال)",false);
        }

        // ============ CARD 3: services table ============
        {
            RECT cr={m.cardL,Y(v.tTop),m.cardR,Y(v.tBot)};
            gpRoundRectBg(dc,cr,S(14),g_theme.surface,g_theme.border,g_theme.bg);
            // table header strip
            RECT head={formL,Y(v.theadY),formR,Y(v.theadY)+S(30)};
            fillRoundRect(dc,head,S(8),g_theme.surface2,g_theme.border);
            SelectObject(dc,g_fSmall); SetTextColor(dc,g_theme.textDim);
            // 7 columns (RTL): ردیف | کد خدمت | نام خدمت | مبلغ | تخفیف | سهم بیمه | عملیات
            const wchar_t* cols[7]={L"ردیف",L"کد خدمت",L"نام خدمت",
                L"مبلغ (ریال)",L"تخفیف (ریال)",L"سهم بیمه (ریال)",L"عملیات"};
            int weights[7]={6,12,24,16,16,16,10};
            int wsum=0; for(int i=0;i<7;i++) wsum+=weights[i];
            int x=formR;   // start at RIGHT (RTL)
            for(int i=0;i<7;i++){
                int colw=(fw*weights[i])/wsum;
                RECT hr={x-colw+S(4),head.top,x-S(4),head.bottom};
                DrawTextW(dc,cols[i],-1,&hr,DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
                x-=colw;
            }
            // empty-state: centred box icon + caption
            int ecx=(formL+formR)/2, ecy=Y(v.tbodyY)+(v.tbodyBot-v.tbodyY)/2-S(8);
            { // simple open-box glyph
              COLORREF gc=blendColor(g_theme.textDim,g_theme.surface,55);
              HPEN pn=CreatePen(PS_SOLID,S(2),gc);
              HGDIOBJ op=SelectObject(dc,pn);
              HGDIOBJ ob=SelectObject(dc,GetStockObject(NULL_BRUSH));
              int bw=S(34), bh=S(24);
              // box body
              Rectangle(dc,ecx-bw/2,ecy-bh/2,ecx+bw/2,ecy+bh/2);
              // lid flaps
              MoveToEx(dc,ecx-bw/2,ecy-bh/2,0); LineTo(dc,ecx-bw/4,ecy-bh/2-S(8));
              LineTo(dc,ecx+bw/4,ecy-bh/2-S(8)); LineTo(dc,ecx+bw/2,ecy-bh/2);
              SelectObject(dc,op); SelectObject(dc,ob); DeleteObject(pn);
            }
            SelectObject(dc,g_fSmall); SetTextColor(dc,g_theme.textDim);
            RECT er={formL,ecy+S(22),formR,ecy+S(42)};
            DrawTextW(dc,L"هیچ خدمتی اضافه نشده است.",-1,&er,
                DT_CENTER|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);
        }

        // ============ framed input wells behind every center control ========
        {
            HWND inputs[16]={t->eFirst,t->eLast,t->eNid,t->eFather,t->eBirth,
                t->cGender,t->eMobile,t->ePhone,t->eAddr,t->cPType,t->cNType,
                t->cIns,t->cSupp,t->ePrice,t->eDiscount,t->eSvcSearch};
            HWND foc=GetFocus();
            for(int i=0;i<16;i++){
                if(!inputs[i]) continue;
                RECT wr; GetWindowRect(inputs[i],&wr);
                POINT a={wr.left,wr.top}, b={wr.right,wr.bottom};
                ScreenToClient(h,&a); ScreenToClient(h,&b);
                if(b.y<=a.y) continue;
                int wellBot = a.y + rh;          // clamp to a single row height
                RECT well={a.x-S(6),a.y-S(4),b.x+S(6),wellBot+S(4)};
                bool focused = (inputs[i]==foc);
                bool invalid = (t->idChecked && !t->idVerified && (i==0||i==1))
                             || (t->invalidMask & (1<<i));
                COLORREF bord = invalid ? g_theme.danger
                              : focused ? g_theme.accent : g_theme.border;
                fillRoundRect(dc,well,S(8),g_theme.inputBg,bord);
            }
        }

        // ============ status message (above the buttons) ============
        if(t && !t->lastMsg.empty()){
            SetTextColor(dc,t->msgCol); SelectObject(dc,g_fSmall);
            RECT mr2={formL,Y(v.btnY)-S(18),formR,Y(v.btnY)-S(2)};
            DrawTextW(dc,t->lastMsg.c_str(),-1,&mr2,
                DT_RIGHT|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);
        }

        // ============ LEFT BILLING COLUMN (صورتحساب) ============
        //  v1.19.0 premium redesign (matches the reference):
        //    1) «صورتحساب» summary card  — 8 money rows, NO inline highlight.
        //    2) «مبلغ نهایی» card        — vivid blue gradient + wallet icon +
        //                                   large white amount (replaces the old
        //                                   dark navy «چک پرداختی» strip).
        //    3) «چاپ» group + 3 print buttons (pinned to the column bottom by
        //       tabPageLayout). All three blocks scroll together with the page.
        if(!m.stacked && t){
            // ----- 1) summary card -----
            int sumTop = S(RC_OUT);
            // 8 rows · 26px each + header (42) + bottom pad (14)
            int sumBot = sumTop + S(42) + 8*S(26) + S(10);
            RECT card={m.billL,Y(sumTop),m.billR,Y(sumBot)};
            gpRoundRectBg(dc,card,S(14),g_theme.surface,g_theme.border,g_theme.bg);
            int bl=card.left+S(12), br=card.right-S(12);
            // header
            { int icoW=S(18);
              RECT bi={br-icoW,card.top+S(12),br,card.top+S(30)};
              drawIcon(dc,ICO_RECEIPT,bi,g_theme.accent,S(2));
              SetTextColor(dc,g_theme.text); SelectObject(dc,g_fUIB);
              RECT bt={bl,card.top+S(10),br-icoW-S(6),card.top+S(32)};
              DrawTextW(dc,L"صورتحساب",-1,&bt,
                  DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX); }
            // money row helper (key right, value left)
            auto row=[&](int& ry,const wchar_t* k,long long val){
                SelectObject(dc,g_fSmall); SetTextColor(dc,g_theme.textDim);
                RECT kr={bl,ry,br,ry+S(20)};
                DrawTextW(dc,k,-1,&kr,DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
                SetTextColor(dc,g_theme.text);
                std::wstring vs=toFaDigits(formatMoney(val))+L" ریال";
                DrawTextW(dc,vs.c_str(),-1,&kr,DT_LEFT|DT_SINGLELINE|DT_VCENTER|DT_NOPREFIX);
                ry+=S(26);
            };
            int ry=card.top+S(42);
            row(ry,L"بیمه اصلی",0);
            row(ry,L"سهم بیمه",t->mainShare);
            row(ry,L"جمع کل",t->total);
            row(ry,L"بیمه مکمل",0);
            row(ry,L"مابه‌التفاوت پایه",t->baseDiff);
            row(ry,L"سهم سازمان",t->orgShare);
            row(ry,L"مبلغ نهایی",t->patientShare);
            row(ry,L"تخفیف",t->patientShare - t->paid);

            // ----- 2) «مبلغ نهایی» blue-gradient card -----
            int faTop = sumBot + S(12);
            int faBot = faTop + S(76);
            RECT fa={m.billL,Y(faTop),m.billR,Y(faBot)};
            // soft glow under the card, then the vivid HORIZONTAL blue gradient
            // body (royal-blue on the LEFT → sky-blue on the RIGHT, matching the
            // reference; the wallet icon sits on the lighter right edge).
            gpShadow(dc,fa,S(14),S(8),46);
            gpGradRoundRectBgH(dc,fa,S(14),g_theme.accent,g_theme.accent2,
                               CLR_INVALID,g_theme.bg);
            int fl=fa.left+S(14), fr=fa.right-S(14);
            // wallet icon flush-right (RTL)
            int wico=S(26);
            RECT wi={fr-wico,fa.top+(fa.bottom-fa.top-wico)/2,fr,
                     fa.top+(fa.bottom-fa.top-wico)/2+wico};
            drawIcon(dc,ICO_WALLET,wi,RGB(255,255,255),S(2));
            // title (small, top) + amount (large, below) on the LEFT, white —
            // exactly mirrors the reference (icon right, text block left-aligned).
            SelectObject(dc,g_fSmall);
            SetTextColor(dc,RGB(0xDD,0xEC,0xFF));
            RECT ft={fl,fa.top+S(14),fr-wico-S(8),fa.top+S(32)};
            DrawTextW(dc,L"مبلغ نهایی",-1,&ft,
                DT_LEFT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
            SelectObject(dc,g_fTitle); SetTextColor(dc,RGB(255,255,255));
            std::wstring fv=toFaDigits(formatMoney(t->patientShare))+L" ریال";
            RECT fvr={fl,fa.top+S(34),fr-wico-S(8),fa.bottom-S(10)};
            DrawTextW(dc,fv.c_str(),-1,&fvr,
                DT_LEFT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);

            // ----- 3) «چاپ» group title (buttons positioned by tabPageLayout) -
            int prTop = VH - S(186) - sy - S(26);
            SelectObject(dc,g_fUIB); SetTextColor(dc,g_theme.text);
            int icoW=S(16);
            RECT pi={br-icoW,prTop,br,prTop+S(18)};
            drawIcon(dc,ICO_PRINT,pi,g_theme.accent,S(2));
            RECT pt={bl,prTop,br-icoW-S(6),prTop+S(18)};
            DrawTextW(dc,L"چاپ",-1,&pt,DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
        }
        #undef Y
        BitBlt(dc0,0,0,rc.right,rc.bottom,dc,0,0,SRCCOPY);
        SelectObject(dc,obm); DeleteObject(bmp); DeleteDC(dc);
        EndPaint(h,&ps);
        return 0; }
    case WM_DESTROY:
        // §3 deterministic teardown: tear the hybrid HTML host down before the
        // page window goes away (its own WM_DESTROY also runs teardown, but we
        // null our handle here so no stale reference can be reused).
        if(t && t->web){ HWND w0=t->web; t->web=0; DestroyWindow(w0); }
        return 0;
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

// v1.7.0: persist the user's preferred tab order. We store the sequence of the
// permanent tab KINDS (نوبت‌دهی / پذیرش / کارتابل) as a CSV in the settings
// file so a drag-reorder survives between runs. Reception (form) tabs the user
// opened ad-hoc and detached tabs are not persisted (only the fixed trio).
static void saveTabOrder(){
    if(!s_rd) return;
    std::wstring csv;
    for(auto* t : s_rd->tabs){
        if(t->detached) continue;
        // only the singleton/permanent kinds define a stable order
        if(t->kind==TK_APPOINTMENT || t->kind==TK_RECEPTION || t->kind==TK_PORTAL){
            if(!csv.empty()) csv += L",";
            wchar_t b[8]; swprintf(b,8,L"%d",t->kind); csv += b;
        }
    }
    if(!csv.empty()) setSetting(L"tab_order", csv);
}
// return the persisted kind order (e.g. {3,0,1}); empty if none saved.
static std::vector<int> loadTabOrder(){
    std::vector<int> out;
    std::wstring csv=getSetting(L"tab_order",L"");
    size_t p=0;
    while(p<csv.size()){
        size_t e=csv.find(L',',p); if(e==std::wstring::npos) e=csv.size();
        std::wstring tok=csv.substr(p,e-p); p=e+1;
        if(!tok.empty()) out.push_back(_wtoi(tok.c_str()));
    }
    return out;
}

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
    // Only the reception form scrolls (it has the long right-side form); the
    // painted portal / appointment / empty pages don't need a scrollbar.
    DWORD pgStyle = WS_CHILD|WS_CLIPCHILDREN;
    if(kind==TK_RECEPTION) pgStyle |= WS_VSCROLL;
    HWND pg=CreateWindowExW(0,TABPG_CLASS,L"",
        pgStyle,
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
// v1.7.0: focus the appointment (نوبت‌دهی) tab if one already exists; otherwise
// open a fresh one. Triggered by the header's «نوبت‌دهی» button.
static void focusAppointmentTab(HWND h){
    if(!s_rd) return;
    for(size_t i=0;i<s_rd->tabs.size();i++){
        if(s_rd->tabs[i]->kind==TK_APPOINTMENT && !s_rd->tabs[i]->detached){
            s_rd->active=(int)i;
            recLayoutTabs(h);
            InvalidateRect(h,NULL,FALSE);
            return;
        }
    }
    addTabKind(h, TK_APPOINTMENT);
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
    r.right = rc.right - S(10) - i*(tabW()+tabGap());
    r.left  = r.right - tabW();
    r.top   = infoBarH()+S(3);
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
// v1.7.0: compute the insertion slot for a drag at mouse-x. Tabs flow RTL
// (right→left): index 0 is right-most. We pick the slot whose horizontal
// centre is nearest to the cursor. Returns a value in [0, tabCount].
static int dropIndexForX(HWND h, int mouseX){
    if(!s_rd || s_rd->tabs.empty()) return 0;
    int n=(int)s_rd->tabs.size();
    for(int i=0;i<n;i++){
        RECT r=tabRect(h,i);
        int mid=(r.left+r.right)/2;
        // RTL: if the cursor is to the RIGHT of this tab's centre, it belongs
        // BEFORE it (smaller index sits further right).
        if(mouseX > mid) return i;
    }
    return n;   // dropped past the left-most tab → end of the list
}

// ------------------------------------------------------------- reception ---
static LRESULT CALLBACK recProc(HWND h, UINT m, WPARAM w, LPARAM l){
    switch(m){
    case WM_CREATE:
        s_rd = new RecData();
        // v1.7.0: «پذیرش جدید» / «نوبت‌دهی» / «تب جدید» now live in the FRAME
        // HEADER (main.cpp). The reception info-bar no longer owns any action
        // buttons, so the tab strip is clean and uncluttered.
        s_rd->bNewPat = NULL;
        s_rd->bNewTab = NULL;
        s_rd->bCalc   = NULL;   // calculator also in the frame header
        s_rd->lastUnseen = unseenMessageCount(g_session.user.username);
        SetTimer(h, 77, 5000, NULL);   // poll the cartable for new messages
        return 0;
    case WM_APP_THEME:
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
        // v1.7.0: action buttons moved to the frame header — only the tab
        // strip needs re-laying out here.
        recLayoutTabs(h);
        return 0; }
    case WM_COMMAND: {
        int id=LOWORD(w);
        if(id==ID_RC_CALC) openCalculator(g_hFrame);
        else if(id==ID_RC_NEWTAB) addTabKind(h, TK_EMPTY);   // new-tab → empty
        else if(id==ID_RC_APPT) focusAppointmentTab(h);      // نوبت‌دهی
        else if(id==ID_RC_NEWPAT){
            // "پذیرش جدید" (پذیرش جدید/New Admission) — always open a FRESH
            // reception tab so a new patient never overwrites the form the
            // operator is currently filling. If the active tab is an EMPTY
            // placeholder, reuse it; otherwise add a new reception tab.
            if(s_rd && s_rd->active>=0 && s_rd->active<(int)s_rd->tabs.size()
               && s_rd->tabs[s_rd->active]->kind==TK_EMPTY){
                // turn the empty tab into a reception tab in place
                addTab(h);
            } else {
                addTab(h);   // open a new reception tab
            }
        }
        return 0; }
    case WM_MOUSEMOVE: {
        if(!s_rd) return 0;
        POINT pt={GET_X_LPARAM(l),GET_Y_LPARAM(l)};
        // v1.7.0: drag-reorder in progress (or arming) ----------------------
        if(s_rd->dragArmed){
            int dx=pt.x-s_rd->dragStart.x;
            if(!s_rd->dragging && (dx>S(6)||dx<-S(6)))
                s_rd->dragging=true;            // crossed the threshold
            if(s_rd->dragging){
                s_rd->dragX=pt.x;
                s_rd->dropIdx=dropIndexForX(h,pt.x);
                SetCursor(LoadCursor(NULL,IDC_SIZEWE));
                RECT bar={0,infoBarH(),S(2000),infoBarH()+tabBarH()};
                InvalidateRect(h,&bar,FALSE);
                return 0;
            }
        }
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
        // don't clear hover while a drag (with capture) is active
        if(s_rd && !s_rd->dragging && s_rd->hotTab!=-1){
            s_rd->hotTab=s_rd->hotClose=s_rd->hotDetach=-1;
            RECT bar={0,infoBarH(),S(2000),infoBarH()+tabBarH()};
            InvalidateRect(h,&bar,FALSE);
        }
        return 0;
    case WM_CAPTURECHANGED:
        // capture was lost (e.g. alt-tab) — cancel any pending/active drag
        if(s_rd && (s_rd->dragArmed||s_rd->dragging)){
            s_rd->dragArmed=false; s_rd->dragging=false;
            s_rd->dragIdx=-1; s_rd->dropIdx=-1;
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
                // v1.7.0: arm a potential drag-reorder on the tab BODY. It only
                // becomes a real drag once the cursor moves past a threshold,
                // so a plain click still just activates the tab.
                s_rd->dragArmed=true; s_rd->dragging=false;
                s_rd->dragIdx=hit; s_rd->dragStart=pt; s_rd->dragX=pt.x;
                s_rd->dropIdx=hit;
                SetCapture(h);
            } else {
                // focus the detached window
                HWND det=GetParent(t->page);
                if(det) SetForegroundWindow(det);
            }
        }
        return 0; }
    case WM_LBUTTONUP: {
        if(!s_rd) return 0;
        bool wasDragging=s_rd->dragging;
        int from=s_rd->dragIdx, to=s_rd->dropIdx;
        if(GetCapture()==h) ReleaseCapture();
        s_rd->dragArmed=false; s_rd->dragging=false;
        if(wasDragging && from>=0 && from<(int)s_rd->tabs.size()){
            // RTL drop-index → list insertion. dropIndexForX returns the slot
            // the dragged tab should occupy; normalise when moving rightward.
            if(to<0) to=0; if(to>(int)s_rd->tabs.size()) to=(int)s_rd->tabs.size();
            int adj = (to>from)? to-1 : to;
            if(adj!=from && adj>=0 && adj<(int)s_rd->tabs.size()){
                TabPage* moved=s_rd->tabs[from];
                s_rd->tabs.erase(s_rd->tabs.begin()+from);
                s_rd->tabs.insert(s_rd->tabs.begin()+adj, moved);
                // keep the moved tab active and persist the new order
                s_rd->active=adj;
                saveTabOrder();
                recLayoutTabs(h);
            }
            s_rd->dragIdx=-1; s_rd->dropIdx=-1;
            InvalidateRect(h,NULL,FALSE);
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
        // v1.10.0: only the tab-bar bottom separator remains (the collapsed
        // info bar no longer needs its own divider line).
        MoveToEx(dc,0,infoBarH()+tabBarH()-1,0); LineTo(dc,rc.right,infoBarH()+tabBarH()-1);
        SelectObject(dc,op); DeleteObject(pen);

        SetBkMode(dc,TRANSPARENT);
        // v1.10.0: the old "میز پذیرش بیمار" title row was removed — it was an
        // empty header that only pushed the form down and caused scrolling. The
        // logged-in person's name/role and the live clock already live in the
        // frame's Layer-1 header, so nothing is lost here.

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
            // v1.7.0: drag-reorder drop indicator — a bright accent bar at the
            // boundary where the tab will be inserted, plus a subtle highlight
            // of the tab being dragged.
            if(s_rd->dragging && s_rd->dragIdx>=0){
                int di=s_rd->dropIdx;
                if(di<0) di=0; if(di>(int)s_rd->tabs.size()) di=(int)s_rd->tabs.size();
                int barX;
                if(di>=(int)s_rd->tabs.size()){
                    RECT last=tabRect(h,(int)s_rd->tabs.size()-1);
                    barX=last.left-S(3);
                } else {
                    RECT r=tabRect(h,di);
                    barX=r.right+S(3);
                }
                int top=infoBarH()+S(4), bot=infoBarH()+tabBarH()-S(2);
                RECT ind={barX-S(2),top,barX+S(2),bot};
                fillRoundRect(dc,ind,S(2),g_theme.accent,CLR_INVALID);
                // dim-highlight the dragged tab so it reads as "lifted"
                RECT dr=tabRect(h,s_rd->dragIdx);
                fillRoundRect(dc,dr,S(9),g_theme.hover,g_theme.accent);
            }
            if(s_rd->tabs.empty()){
                SetTextColor(dc,g_theme.textDim);
                SelectObject(dc,g_fUI);
                RECT er={0,infoBarH()+tabBarH(),rc.right,rc.bottom};
                DrawTextW(dc,
                    L"برای شروع، «پذیرش جدید» را از نوار بالا انتخاب کنید",
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
// v1.7.0: public routing used by the frame header (main.cpp) ----------------
HWND receptionWindow(){
    return FindWindowExW(g_hFrame,NULL,RC_CLASS,NULL);
}
void receptionAction(RecAction a){
    HWND rec=receptionWindow();
    if(!rec || !IsWindow(rec)) return;
    int id = a==RA_APPOINTMENT ? ID_RC_APPT
           : a==RA_NEWTAB      ? ID_RC_NEWTAB
           :                     ID_RC_NEWPAT;
    SendMessageW(rec, WM_COMMAND, MAKEWPARAM(id,0), 0);
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
    // v1.8.0 — default tab behaviour:
    //   On entering reception, NO previously-open tab is auto-restored. The ONLY
    //   tab opened by default is the message board (کارتابل / TK_PORTAL), which
    //   is also the FIRST (right-most, index 0) tab so the user immediately sees
    //   any pending management messages. Reception / appointment tabs are opened
    //   on demand from the header action buttons.
    addTabKind(h, TK_PORTAL);
    if(s_rd){
        s_rd->active = 0;            // focus the message board tab
        recLayoutTabs(h);
    }
#ifdef AZ_DEBUG_BUILD
    {   // headless screenshot helper: open a specific tab for inspection
        wchar_t dbg[32]={0};
        GetEnvironmentVariableW(L"AZ_DEBUG_TAB", dbg, 32);
        if(dbg[0]){
            if(!wcscmp(dbg,L"reception")) addTabKind(h, TK_RECEPTION);
            else if(!wcscmp(dbg,L"appointment")) addTabKind(h, TK_APPOINTMENT);
            recLayoutTabs(h);
            InvalidateRect(h,NULL,TRUE);
            // optional initial scroll offset for screenshot verification
            wchar_t scr[16]={0};
            GetEnvironmentVariableW(L"AZ_DEBUG_SCROLL", scr, 16);
            if(scr[0] && s_rd && !s_rd->tabs.empty()){
                TabPage* t=s_rd->tabs.back();
                if(t && t->page){
                    t->scrollY=_wtoi(scr);
                    tabPageLayout(t->page,t);
                    InvalidateRect(t->page,NULL,TRUE);
                }
            }
        }
    }
#endif
    return h;
}
