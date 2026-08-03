// ============================================================================
//  print_designer_templates.inc — 30 built-in A4 reception templates
//  (v1.62.0 — FULL REWRITE: SERVICES-FIRST, TEN DISTINCT LAYOUT FAMILIES)
//
//  Included into print_designer.cpp. Every template is authored in A4 mm
//  coordinates (210×297).
//
//  WHAT CHANGED IN v1.62.0 (the whole point of this rewrite)
//  --------------------------------------------------------
//  * Previously ALL 30 builtins shared ONE cookie-cutter body, so the gallery
//    was 30 near-identical pages and several of them left almost no room for
//    the services table — the operator's #1 complaint («در ۳۰ طرح خدمات درست
//    وارد نمی‌شود / جا برای خدمات نیست / چند ردیف خدمت جا نمی‌شود»).
//  * Now there are TEN genuinely different layout families (classic, colour
//    band, invoice, sidebar, line-art, card, photo-ID, financial, compact
//    tear-off, dual-block), each with three variants → 30 distinct designs.
//  * EVERY single one of the 30 carries the dynamic services table
//    (PIT_SERVICES) bound to the live ReceptionRecord.services vector, and its
//    box height is COMPUTED so the table always claims every millimetre left
//    between the patient block and the totals — typically 15–22 service rows
//    on A4, instead of the 8–10 the old cookie-cutter allowed.
//  * The table is a REAL TABLE (ruled grid, pinned header/row heights, RTL
//    column classification by caption) — never a stack of labels.
//  * Eight different, correctly-classified column presets are used across the
//    30 designs (3→7 columns: ردیف / نام خدمت / شرح خدمت / نوع خدمت / کد خدمت /
//    تعداد / مبلغ واحد / تخفیف / سهم بیمه / سهم بیمار / مبلغ کل), so a clinic
//    can pick the level of detail it wants and still get real data in it.
//
//  If the operator switches the paper size (A5/A6/receipt) the print engine
//  auto-scales the whole design proportionally (pscale), so nothing is
//  clipped. Seeded on first run; existing installs are migrated once.
// ============================================================================

// --- small builder helpers --------------------------------------------------
namespace {

static int g_tplItemId = 1;
static int nid(){ return g_tplItemId++; }

// A4 page geometry shared by all 30 designs.
static const double PG_W   = 210.0;
static const double PG_H   = 297.0;
static const double PG_M   = 12.0;                 // page margin
static const double PG_CW  = PG_W - 2*PG_M;        // 186 mm content width
static const double FOOT_Y = PG_H - 34.0;          // 263 mm — footer band top

static PrintItem mkLabel(double x,double y,double w,double h,
                         const std::wstring& text,double pt,bool bold,int align){
    PrintItem it; it.id=nid(); it.type=PIT_LABEL; it.x=x; it.y=y; it.w=w; it.h=h;
    it.text=text; it.fontPt=pt; it.bold=bold; it.align=align; return it;
}
static PrintItem mkLabelC(double x,double y,double w,double h,
                          const std::wstring& text,double pt,bool bold,int align,
                          unsigned colour){
    PrintItem it=mkLabel(x,y,w,h,text,pt,bold,align); it.textColor=colour; return it;
}
static PrintItem mkField(double x,double y,double w,double h,
                         const std::wstring& field,const std::wstring& prefix,
                         double pt,int align){
    PrintItem it; it.id=nid(); it.type=PIT_FIELD; it.x=x; it.y=y; it.w=w; it.h=h;
    it.field=field; it.prefix=prefix; it.fontPt=pt; it.align=align; return it;
}
static PrintItem mkFieldB(double x,double y,double w,double h,
                          const std::wstring& field,const std::wstring& prefix,
                          double pt,int align){
    PrintItem it=mkField(x,y,w,h,field,prefix,pt,align); it.bold=true; return it;
}
static PrintItem mkHLine(double x,double y,double w,double bw){
    PrintItem it; it.id=nid(); it.type=PIT_HLINE; it.x=x; it.y=y; it.w=w; it.h=0.2;
    it.borderWidth=bw; return it;
}
static PrintItem mkHLineC(double x,double y,double w,double bw,unsigned col){
    PrintItem it=mkHLine(x,y,w,bw); it.borderColor=col; return it;
}
static PrintItem mkVLine(double x,double y,double h,double bw){
    PrintItem it; it.id=nid(); it.type=PIT_VLINE; it.x=x; it.y=y; it.w=0.2; it.h=h;
    it.borderWidth=bw; return it;
}
static PrintItem mkRect(double x,double y,double w,double h,double bw,double corner){
    PrintItem it; it.id=nid(); it.type=PIT_RECT; it.x=x; it.y=y; it.w=w; it.h=h;
    it.borderWidth=bw; it.corner=corner; it.fillTransparent=true; return it;
}
static PrintItem mkRectC(double x,double y,double w,double h,double bw,double corner,
                         unsigned border){
    PrintItem it=mkRect(x,y,w,h,bw,corner); it.borderColor=border; return it;
}
static PrintItem mkBand(double x,double y,double w,double h,unsigned fill,double bw){
    PrintItem it; it.id=nid(); it.type=PIT_RECT; it.x=x; it.y=y; it.w=w; it.h=h;
    it.fillColor=fill; it.fillTransparent=false; it.borderWidth=bw;
    it.borderColor=fill; return it;
}
static PrintItem mkTint(double x,double y,double w,double h,unsigned fill,
                        unsigned border,double bw,double corner){
    PrintItem it; it.id=nid(); it.type=PIT_RECT; it.x=x; it.y=y; it.w=w; it.h=h;
    it.fillColor=fill; it.fillTransparent=false;
    it.borderColor=border; it.borderWidth=bw; it.corner=corner; return it;
}
static PrintItem mkFrame(double pw,double ph,double margin,unsigned col,double bw){
    PrintItem it; it.id=nid(); it.type=PIT_FRAME; it.is_frame=true;
    it.x=margin; it.y=margin; it.w=pw-2*margin; it.h=ph-2*margin;
    it.borderColor=col; it.borderWidth=bw; it.fillTransparent=true; return it;
}
static PrintItem mkLogo(double x,double y,double w,double h){
    PrintItem it; it.id=nid(); it.type=PIT_LOGO; it.x=x; it.y=y; it.w=w; it.h=h; return it;
}
static PrintItem mkQR(double x,double y,double s){
    PrintItem it; it.id=nid(); it.type=PIT_QR; it.x=x; it.y=y; it.w=s; it.h=s;
    it.field=L"receiptNo"; return it;
}
// REAL scannable 1-D barcode (Code128-B) bound to the deterministic,
// patient-linked {receiptbarcode} token — reprints are identical.
static PrintItem mkBarcode(double x,double y,double w,double h){
    PrintItem it; it.id=nid(); it.type=PIT_BARCODE; it.x=x; it.y=y; it.w=w; it.h=h;
    it.field=L"receiptbarcode";
    it.text=L"{\"sym\":\"code128\",\"hri\":true,\"quiet\":2}";
    it.fontPt=8.0; it.align=1; it.textColor=0x000000; return it;
}
static PrintItem mkPhoto(double x,double y,double w,double h){
    PrintItem it; it.id=nid(); it.type=PIT_PHOTO; it.x=x; it.y=y; it.w=w; it.h=h;
    it.borderWidth=0.3; return it;
}

// ---------------------------------------------------------------------------
//  THE CORE: dynamic services table CONNECTED to the live reception record.
//
//  The model JSON describes the header (cols / labels / widths); at print time
//  printer.cpp::pdDrawServices fills ONE data row per entered service from
//  ReceptionRecord.services — real نام/شرح/تعداد/مبلغ، never test text. The
//  caption of each column decides its meaning (pdSvcColOf), so a clinic may
//  reorder or delete columns in the designer and the data still lands right.
//
//  v1.62.0 — EIGHT column presets, every caption verified against
//  printer.cpp::pdSvcColOf so no column is ever mis-classified:
//     SVC3       نام خدمت | شرح خدمت | تعداد
//     SVC4_ROW   ردیف | نام خدمت | شرح خدمت | تعداد
//     SVC4_CAT   نام خدمت | نوع خدمت | تعداد | مبلغ کل
//     SVC5       ردیف | نام خدمت | شرح خدمت | تعداد | مبلغ کل
//     SVC5_CODE  ردیف | کد خدمت | نام خدمت | تعداد | مبلغ کل
//     SVC6_FIN   ردیف | نام خدمت | تعداد | مبلغ واحد | تخفیف | مبلغ کل
//     SVC6_INS   ردیف | نام خدمت | تعداد | مبلغ کل | سهم بیمه | سهم بیمار
//     SVC7       ردیف | کد خدمت | نام خدمت | شرح خدمت | تعداد | مبلغ واحد | مبلغ کل
// ---------------------------------------------------------------------------
enum SvcPreset {
    SVC3 = 0, SVC4_ROW, SVC4_CAT, SVC5, SVC5_CODE, SVC6_FIN, SVC6_INS, SVC7,
    SVC_PRESET_COUNT
};

static std::wstring svcModelJson(int preset){
    switch(preset){
    case SVC4_ROW:
        return L"{\"cols\":4,\"header\":true,\"widths\":[0.07,0.48,0.30,0.15],"
               L"\"labels\":[\"ردیف\",\"نام خدمت\",\"شرح خدمت\",\"تعداد\"]}";
    case SVC4_CAT:
        return L"{\"cols\":4,\"header\":true,\"widths\":[0.40,0.24,0.11,0.25],"
               L"\"labels\":[\"نام خدمت\",\"نوع خدمت\",\"تعداد\",\"مبلغ کل\"]}";
    case SVC5:
        return L"{\"cols\":5,\"header\":true,\"widths\":[0.07,0.36,0.25,0.10,0.22],"
               L"\"labels\":[\"ردیف\",\"نام خدمت\",\"شرح خدمت\",\"تعداد\",\"مبلغ کل\"]}";
    case SVC5_CODE:
        return L"{\"cols\":5,\"header\":true,\"widths\":[0.07,0.15,0.44,0.10,0.24],"
               L"\"labels\":[\"ردیف\",\"کد خدمت\",\"نام خدمت\",\"تعداد\",\"مبلغ کل\"]}";
    case SVC6_FIN:
        return L"{\"cols\":6,\"header\":true,\"widths\":[0.06,0.34,0.09,0.17,0.14,0.20],"
               L"\"labels\":[\"ردیف\",\"نام خدمت\",\"تعداد\",\"مبلغ واحد\",\"تخفیف\",\"مبلغ کل\"]}";
    case SVC6_INS:
        return L"{\"cols\":6,\"header\":true,\"widths\":[0.06,0.32,0.09,0.18,0.17,0.18],"
               L"\"labels\":[\"ردیف\",\"نام خدمت\",\"تعداد\",\"مبلغ کل\",\"سهم بیمه\",\"سهم بیمار\"]}";
    case SVC7:
        return L"{\"cols\":7,\"header\":true,\"widths\":[0.05,0.11,0.27,0.20,0.08,0.14,0.15],"
               L"\"labels\":[\"ردیف\",\"کد خدمت\",\"نام خدمت\",\"شرح خدمت\",\"تعداد\","
               L"\"مبلغ واحد\",\"مبلغ کل\"]}";
    case SVC3:
    default:
        return L"{\"cols\":3,\"header\":true,\"widths\":[0.55,0.30,0.15],"
               L"\"labels\":[\"نام خدمت\",\"شرح خدمت\",\"تعداد\"]}";
    }
}

// Build the live services table. `headFill` == 0 → no fill (pure line-art, so a
// monochrome design prints monochrome). rowH/headerH are pinned in mm, which is
// what guarantees a clean, evenly-ruled table for ANY number of services: the
// print engine keeps the pinned pitch and, when a very long bill needs more
// rows than the box has, shrinks the whole table uniformly instead of clipping.
static PrintItem mkServices(double x,double y,double w,double h,double pt,
                            int preset,unsigned headFill,double bw,
                            double rowH,double headerH){
    PrintItem it; it.id=nid(); it.type=PIT_SERVICES; it.x=x; it.y=y; it.w=w; it.h=h;
    it.text=svcModelJson(preset);
    it.fontPt=pt; it.bold=false; it.align=1;
    it.borderColor=0x000000; it.borderWidth=bw;
    it.textColor=0x000000;
    if(headFill){ it.fillColor=headFill; it.fillTransparent=false; }
    else        { it.fillColor=0xFFFFFF; it.fillTransparent=true;  }
    it.padding=0.8;
    it.headerH=headerH; it.rowH=rowH;
    return it;
}

// localized captions reused across templates
#define FA_CLINIC   L"درمانگاه شبانه‌روزی آزادی طب"
#define FA_SUBTITLE L"سامانه پذیرش و مدیریت درمانگاه"
#define FA_SVCLIST  L"لیست خدمات ارائه‌شده"
#define FA_NAME     L"نام: "
#define FA_FAMILY   L"نام خانوادگی: "
#define FA_FULL     L"نام بیمار: "
#define FA_NID      L"کد ملی: "
#define FA_FATHER   L"نام پدر: "
#define FA_BIRTH    L"تاریخ تولد: "
#define FA_AGE      L"سن: "
#define FA_GENDER   L"جنسیت: "
#define FA_DATE     L"تاریخ: "
#define FA_TIME     L"ساعت: "
#define FA_RECEIPT  L"شماره قبض: "
#define FA_TOTAL    L"جمع کل: "
#define FA_PAID     L"پرداختی: "
#define FA_PATIENT  L"سهم بیمار: "
#define FA_INSSHARE L"سهم بیمه: "
#define FA_DISCOUNT L"تخفیف: "
#define FA_FINAL    L"مبلغ نهایی: "
#define FA_INS      L"بیمه: "
#define FA_SUPP     L"بیمه مکمل: "
#define FA_DOCTOR   L"پزشک: "
#define FA_PERF     L"انجام‌دهنده: "
#define FA_PHONE    L"تلفن: "
#define FA_SHIFT    L"شیفت: "
#define FA_DEPT     L"بخش: "
#define FA_PTYPE    L"نوع بیمار: "
#define FA_USER     L"کاربر: "
#define FA_ROWS     L"تعداد ردیف: "
#define FA_BARCODE_CAP L"بارکد رسید"
#define FA_SIGN     L"امضا و مهر پذیرش"
#define FA_SIGNPAT  L"امضای بیمار"
#define FA_KEEP     L"این رسید را تا پایان درمان نزد خود نگه دارید."

// ===========================================================================
//  SHARED BLOCK BUILDERS
//  Each appends to d.items and returns the next free y (mm).
// ===========================================================================

// ---- header variants ------------------------------------------------------
static double hdrPlain(PrintDesign& d,const std::wstring& title,double titlePt,
                       bool subtitle,double rule){
    double y=PG_M;
    d.items.push_back(mkLabel(PG_M,y,PG_CW,11,title,titlePt,true,1)); y+=13;
    if(subtitle){ d.items.push_back(mkLabel(PG_M,y,PG_CW,6,FA_SUBTITLE,9,false,1)); y+=7; }
    if(rule>0){ d.items.push_back(mkHLine(PG_M,y,PG_CW,rule)); y+=3.5; }
    return y;
}
static double hdrLogoRight(PrintDesign& d,const std::wstring& title,double titlePt,
                           bool subtitle,double rule){
    d.items.push_back(mkLogo(PG_M+PG_CW-21,PG_M,21,21));
    d.items.push_back(mkLabel(PG_M,PG_M+1,PG_CW-25,11,title,titlePt,true,0));
    if(subtitle) d.items.push_back(mkLabel(PG_M,PG_M+12,PG_CW-25,6,FA_SUBTITLE,9,false,0));
    double y=PG_M+23;
    if(rule>0){ d.items.push_back(mkHLine(PG_M,y,PG_CW,rule)); y+=3.5; }
    return y;
}
static double hdrCenterLogo(PrintDesign& d,const std::wstring& title,double titlePt,
                            bool subtitle,double rule){
    d.items.push_back(mkLogo((PG_W-22)/2,PG_M,22,22));
    double y=PG_M+24;
    d.items.push_back(mkLabel(PG_M,y,PG_CW,11,title,titlePt,true,1)); y+=12.5;
    if(subtitle){ d.items.push_back(mkLabel(PG_M,y,PG_CW,6,FA_SUBTITLE,9,false,1)); y+=7; }
    if(rule>0){ d.items.push_back(mkHLine(PG_M,y,PG_CW,rule)); y+=3.5; }
    return y;
}
// Full-width colour band with reversed (white) type. Corporate look.
static double hdrBand(PrintDesign& d,const std::wstring& title,unsigned accent,
                      bool logo,const std::wstring& docLabel){
    double bh=22;
    d.items.push_back(mkBand(PG_M,PG_M,PG_CW,bh,accent,0));
    if(logo) d.items.push_back(mkLogo(PG_M+2.5,PG_M+2.5,17,17));
    double tx = logo ? PG_M+22 : PG_M+4;
    double tw = PG_CW - (tx-PG_M) - 4;
    d.items.push_back(mkLabelC(tx,PG_M+3.0,tw,9,title,16,true,0,0xFFFFFF));
    d.items.push_back(mkLabelC(tx,PG_M+12.5,tw,6,FA_SUBTITLE,8.5,false,0,0xFFFFFF));
    double y=PG_M+bh+3;
    if(!docLabel.empty()){
        d.items.push_back(mkLabel(PG_M,y,PG_CW,7,docLabel,12.5,true,1)); y+=9;
    }
    return y;
}
// Split header: clinic identity right, document identity left, thick rule.
static double hdrSplit(PrintDesign& d,const std::wstring& title,unsigned accent,
                       const std::wstring& docLabel){
    d.items.push_back(mkLabel(PG_M+PG_CW*0.40,PG_M,PG_CW*0.60,10,title,15.5,true,0));
    d.items.push_back(mkLabel(PG_M+PG_CW*0.40,PG_M+10,PG_CW*0.60,5.5,FA_SUBTITLE,8.5,false,0));
    d.items.push_back(mkLabelC(PG_M,PG_M+1,PG_CW*0.38,8,docLabel,13,true,2,accent));
    d.items.push_back(mkField(PG_M,PG_M+10,PG_CW*0.38,6,L"receiptNo",FA_RECEIPT,9,2));
    double y=PG_M+19;
    d.items.push_back(mkHLineC(PG_M,y,PG_CW,1.2,accent)); y+=4;
    return y;
}

// ---- meta strips ----------------------------------------------------------
static double metaStrip4(PrintDesign& d,double y,double pt){
    double c=PG_CW/4;
    d.items.push_back(mkFieldB(PG_M+3*c,y,c,7,L"receiptNo",FA_RECEIPT,pt,0));
    d.items.push_back(mkField (PG_M+2*c,y,c,7,L"date",     FA_DATE,   pt,1));
    d.items.push_back(mkField (PG_M+  c,y,c,7,L"time",     FA_TIME,   pt,1));
    d.items.push_back(mkField (PG_M    ,y,c,7,L"shift",    FA_SHIFT,  pt,2));
    return y+9;
}
static double metaStripBoxed(PrintDesign& d,double y,double pt,unsigned tint,
                             unsigned border){
    double h=11, c=PG_CW/4;
    d.items.push_back(mkTint(PG_M,y,PG_CW,h,tint,border,0.3,1.2));
    d.items.push_back(mkFieldB(PG_M+3*c+1,y+2,c-2,7,L"receiptNo",FA_RECEIPT,pt,1));
    d.items.push_back(mkField (PG_M+2*c+1,y+2,c-2,7,L"date",     FA_DATE,   pt,1));
    d.items.push_back(mkField (PG_M+  c+1,y+2,c-2,7,L"time",     FA_TIME,   pt,1));
    d.items.push_back(mkField (PG_M    +1,y+2,c-2,7,L"shift",    FA_SHIFT,  pt,1));
    for(int i=1;i<4;i++) d.items.push_back(mkVLine(PG_M+i*c,y,h,0.25));
    return y+h+3;
}
static double metaGrid6(PrintDesign& d,double y,double pt,unsigned border){
    // 3 columns × 2 rows of boxed meta, an invoice-style masthead grid.
    double h=9, c=PG_CW/3;
    for(int r=0;r<2;r++) d.items.push_back(mkRectC(PG_M,y+r*h,PG_CW,h,0.3,0,border));
    for(int i=1;i<3;i++) d.items.push_back(mkVLine(PG_M+i*c,y,2*h,0.25));
    d.items.push_back(mkFieldB(PG_M+2*c+1.5,y+1,c-3,6.5,L"receiptNo",FA_RECEIPT,pt,0));
    d.items.push_back(mkField (PG_M+  c+1.5,y+1,c-3,6.5,L"date",     FA_DATE,   pt,0));
    d.items.push_back(mkField (PG_M    +1.5,y+1,c-3,6.5,L"time",     FA_TIME,   pt,0));
    d.items.push_back(mkField (PG_M+2*c+1.5,y+h+1,c-3,6.5,L"shift",  FA_SHIFT,  pt,0));
    d.items.push_back(mkField (PG_M+  c+1.5,y+h+1,c-3,6.5,L"dept",   FA_DEPT,   pt,0));
    d.items.push_back(mkField (PG_M    +1.5,y+h+1,c-3,6.5,L"userName",FA_USER,  pt,0));
    return y+2*h+3;
}

// ---- patient blocks -------------------------------------------------------
// 2-column, 3-row grid — the compact classic.
static double patient2x3(PrintDesign& d,double y,double pt){
    double col=PG_CW/2-3;
    d.items.push_back(mkFieldB(PG_M+col+6,y,col,7.5,L"firstName",FA_NAME,pt,0));
    d.items.push_back(mkFieldB(PG_M,y,col,7.5,L"lastName",FA_FAMILY,pt,0)); y+=8.5;
    d.items.push_back(mkField(PG_M+col+6,y,col,7.5,L"nationalCode",FA_NID,pt,0));
    d.items.push_back(mkField(PG_M,y,col,7.5,L"insurance",FA_INS,pt,0)); y+=8.5;
    d.items.push_back(mkField(PG_M+col+6,y,col,7.5,L"doctor",FA_DOCTOR,pt,0));
    d.items.push_back(mkField(PG_M,y,col,7.5,L"mobile",FA_PHONE,pt,0)); y+=8.5;
    return y+2;
}
// 3-column, 3-row grid — a fuller identity block.
static double patient3x3(PrintDesign& d,double y,double pt){
    double c=PG_CW/3;
    d.items.push_back(mkFieldB(PG_M+2*c,y,c-2,7,L"fullName",FA_FULL,pt,0));
    d.items.push_back(mkField (PG_M+  c,y,c-2,7,L"nationalCode",FA_NID,pt,0));
    d.items.push_back(mkField (PG_M    ,y,c-2,7,L"fatherName",FA_FATHER,pt,0)); y+=8;
    d.items.push_back(mkField (PG_M+2*c,y,c-2,7,L"birthDate",FA_BIRTH,pt,0));
    d.items.push_back(mkField (PG_M+  c,y,c-2,7,L"gender",FA_GENDER,pt,0));
    d.items.push_back(mkField (PG_M    ,y,c-2,7,L"mobile",FA_PHONE,pt,0)); y+=8;
    d.items.push_back(mkField (PG_M+2*c,y,c-2,7,L"insurance",FA_INS,pt,0));
    d.items.push_back(mkField (PG_M+  c,y,c-2,7,L"suppInsurance",FA_SUPP,pt,0));
    d.items.push_back(mkField (PG_M    ,y,c-2,7,L"doctor",FA_DOCTOR,pt,0)); y+=8;
    return y+2;
}
// Boxed / tinted patient card with a caption strip.
static double patientCard(PrintDesign& d,double y,double pt,unsigned tint,
                          unsigned border,unsigned accent){
    double h=32;
    d.items.push_back(mkTint(PG_M,y,PG_CW,h,tint,border,0.35,1.5));
    d.items.push_back(mkBand(PG_M,y,PG_CW,7,accent,0));
    d.items.push_back(mkLabelC(PG_M+3,y+0.8,PG_CW-6,5.5,L"مشخصات بیمار",9.5,true,0,0xFFFFFF));
    double iy=y+8.5, c=PG_CW/3;
    d.items.push_back(mkFieldB(PG_M+2*c+1.5,iy,c-3,7,L"fullName",FA_FULL,pt,0));
    d.items.push_back(mkField (PG_M+  c+1.5,iy,c-3,7,L"nationalCode",FA_NID,pt,0));
    d.items.push_back(mkField (PG_M    +1.5,iy,c-3,7,L"mobile",FA_PHONE,pt,0)); iy+=7.6;
    d.items.push_back(mkField (PG_M+2*c+1.5,iy,c-3,7,L"insurance",FA_INS,pt,0));
    d.items.push_back(mkField (PG_M+  c+1.5,iy,c-3,7,L"suppInsurance",FA_SUPP,pt,0));
    d.items.push_back(mkField (PG_M    +1.5,iy,c-3,7,L"patientType",FA_PTYPE,pt,0)); iy+=7.6;
    d.items.push_back(mkField (PG_M+2*c+1.5,iy,c-3,7,L"doctor",FA_DOCTOR,pt,0));
    d.items.push_back(mkField (PG_M+  c+1.5,iy,c-3,7,L"performer",FA_PERF,pt,0));
    d.items.push_back(mkField (PG_M    +1.5,iy,c-3,7,L"dept",FA_DEPT,pt,0));
    return y+h+3;
}
// Patient block with a photo box on the LEFT and the identity grid on the right.
static double patientPhoto(PrintDesign& d,double y,double pt,bool qr){
    double pw=24, ph=30;
    d.items.push_back(mkPhoto(PG_M,y,pw,ph));
    d.items.push_back(mkLabel(PG_M,y+ph+0.5,pw,4,L"عکس بیمار",7.5,false,1));
    double gx=PG_M+pw+4, gw=PG_CW-pw-4;
    if(qr){ d.items.push_back(mkQR(PG_M+PG_CW-22,y,22)); gw-=25; }
    double c=gw/2;
    d.items.push_back(mkFieldB(gx+c,y,c-2,7,L"fullName",FA_FULL,pt,0));
    d.items.push_back(mkField (gx  ,y,c-2,7,L"nationalCode",FA_NID,pt,0));
    d.items.push_back(mkField (gx+c,y+7.8,c-2,7,L"fatherName",FA_FATHER,pt,0));
    d.items.push_back(mkField (gx  ,y+7.8,c-2,7,L"birthDate",FA_BIRTH,pt,0));
    d.items.push_back(mkField (gx+c,y+15.6,c-2,7,L"insurance",FA_INS,pt,0));
    d.items.push_back(mkField (gx  ,y+15.6,c-2,7,L"mobile",FA_PHONE,pt,0));
    d.items.push_back(mkField (gx+c,y+23.4,c-2,7,L"doctor",FA_DOCTOR,pt,0));
    d.items.push_back(mkField (gx  ,y+23.4,c-2,7,L"dept",FA_DEPT,pt,0));
    return y+ph+6;
}

// ---- SERVICES BLOCK — the centrepiece -------------------------------------
//  `reserve` = millimetres that must stay free BELOW the table (totals etc.).
//  The table height is whatever is left, so services always get the maximum
//  possible room on the page — never a cramped 8-row box again.
static double servicesBlock(PrintDesign& d,double y,double pt,int preset,
                            unsigned headFill,double bw,double rowH,
                            double reserve,bool caption,double footY){
    if(caption){
        d.items.push_back(mkLabel(PG_M,y,PG_CW*0.60,6.5,FA_SVCLIST,pt+1,true,0));
        d.items.push_back(mkField(PG_M+PG_CW*0.60,y,PG_CW*0.40,6.5,
                                  L"servicescount",FA_ROWS,pt-0.5,2));
        y+=8;
    }
    double h = footY - reserve - y;
    if(h < 40) h = 40;                       // never collapse the table
    if(h > 200) h = 200;
    double headerH = rowH + 0.8;
    d.items.push_back(mkServices(PG_M,y,PG_CW,h,pt-0.5,preset,headFill,bw,rowH,headerH));
    return y+h+3;
}
// Same, but inside a narrower column (sidebar family).
static double servicesBlockAt(PrintDesign& d,double x,double w,double y,double pt,
                              int preset,unsigned headFill,double bw,double rowH,
                              double reserve,bool caption,double footY){
    if(caption){
        d.items.push_back(mkLabel(x,y,w*0.60,6.5,FA_SVCLIST,pt+1,true,0));
        d.items.push_back(mkField(x+w*0.60,y,w*0.40,6.5,L"servicescount",FA_ROWS,pt-0.5,2));
        y+=8;
    }
    double h = footY - reserve - y;
    if(h < 40) h = 40;
    if(h > 200) h = 200;
    double headerH = rowH + 0.8;
    PrintItem it=mkServices(x,y,w,h,pt-0.5,preset,headFill,bw,rowH,headerH);
    d.items.push_back(it);
    return y+h+3;
}

// ---- totals variants ------------------------------------------------------
// 3 fields in one row + a rule above.  Height ≈ 13 mm.
static double totalsRow(PrintDesign& d,double y,double pt){
    d.items.push_back(mkHLine(PG_M,y,PG_CW,0.35)); y+=2.5;
    double c=PG_CW/3;
    d.items.push_back(mkField (PG_M+2*c,y,c,8,L"total",FA_TOTAL,pt+0.5,0));
    d.items.push_back(mkField (PG_M+  c,y,c,8,L"patientshare",FA_PATIENT,pt+0.5,1));
    d.items.push_back(mkFieldB(PG_M    ,y,c,8,L"paid",FA_PAID,pt+1.0,2));
    return y+10.5;
}
// Boxed ladder on the LEFT half + a note on the right.  Height ≈ 30 mm.
static double totalsBox(PrintDesign& d,double y,double pt,unsigned tint,
                        unsigned border,unsigned accent){
    double bw_=PG_CW*0.46, bx=PG_M, h=28;
    d.items.push_back(mkTint(bx,y,bw_,h,tint,border,0.35,1.5));
    double iy=y+1.5;
    d.items.push_back(mkField (bx+1.5,iy,bw_-3,6,L"total",FA_TOTAL,pt,0));            iy+=6.2;
    d.items.push_back(mkField (bx+1.5,iy,bw_-3,6,L"discount",FA_DISCOUNT,pt,0));      iy+=6.2;
    d.items.push_back(mkField (bx+1.5,iy,bw_-3,6,L"insshare",FA_INSSHARE,pt,0));      iy+=6.2;
    d.items.push_back(mkHLineC(bx+1.5,iy-0.6,bw_-3,0.4,border));
    d.items.push_back(mkFieldB(bx+1.5,iy,bw_-3,7,L"paid",FA_FINAL,pt+1.5,0));
    // right side: signature + keep-this-receipt note
    double sx=PG_M+PG_CW*0.54, sw=PG_CW*0.46;
    d.items.push_back(mkLabelC(sx,y+1,sw,6,FA_KEEP,8.5,false,2,accent));
    d.items.push_back(mkHLine(sx+sw*0.30,y+18,sw*0.70,0.3));
    d.items.push_back(mkLabel(sx+sw*0.30,y+18.5,sw*0.70,5,FA_SIGN,8.5,false,1));
    return y+h+3;
}
// Full financial ladder in a right-aligned narrow column.  Height ≈ 34 mm.
static double totalsLadder(PrintDesign& d,double y,double pt,unsigned accent){
    d.items.push_back(mkHLineC(PG_M,y,PG_CW,0.8,accent)); y+=2.5;
    double lw=PG_CW*0.44, lx=PG_M+PG_CW-lw;
    struct Row { const wchar_t* cap; const wchar_t* fld; };
    static const Row R[5]={
        {FA_TOTAL,      L"total"},
        {FA_DISCOUNT,   L"discount"},
        {FA_INSSHARE,   L"insshare"},
        {FA_PATIENT,    L"patientshare"},
        {FA_PAID,       L"paid"},
    };
    for(int i=0;i<5;i++){
        bool last=(i==4);
        if(last) d.items.push_back(mkHLineC(lx,y-0.5,lw,0.5,accent));
        PrintItem f=mkField(lx,y,lw,6.4,R[i].fld,R[i].cap,last?pt+1.5:pt,0);
        f.bold=last; if(last) f.textColor=accent;
        d.items.push_back(f);
        y+=6.4;
    }
    // left side: notes + signature
    double sx=PG_M, sw=PG_CW-lw-6;
    d.items.push_back(mkLabel(sx,y-31,sw,5.5,FA_KEEP,8.5,false,0));
    d.items.push_back(mkHLine(sx,y-9,sw*0.62,0.3));
    d.items.push_back(mkLabel(sx,y-8.5,sw*0.62,5,FA_SIGNPAT,8.5,false,1));
    return y+3;
}
// Wide banded totals bar (reversed type).  Height ≈ 20 mm.
static double totalsBar(PrintDesign& d,double y,double pt,unsigned accent){
    double c=PG_CW/4;
    d.items.push_back(mkBand(PG_M,y,PG_CW,11,accent,0));
    PrintItem f0=mkField(PG_M+3*c+1.5,y+2,c-3,7,L"total",FA_TOTAL,pt,0);
    PrintItem f1=mkField(PG_M+2*c+1.5,y+2,c-3,7,L"discount",FA_DISCOUNT,pt,0);
    PrintItem f2=mkField(PG_M+  c+1.5,y+2,c-3,7,L"insshare",FA_INSSHARE,pt,0);
    PrintItem f3=mkField(PG_M    +1.5,y+2,c-3,7,L"paid",FA_FINAL,pt+1,0);
    f0.textColor=f1.textColor=f2.textColor=f3.textColor=0xFFFFFF; f3.bold=true;
    d.items.push_back(f0); d.items.push_back(f1);
    d.items.push_back(f2); d.items.push_back(f3);
    y+=13.5;
    d.items.push_back(mkHLine(PG_M+PG_CW*0.58,y+3,PG_CW*0.42,0.3));
    d.items.push_back(mkLabel(PG_M+PG_CW*0.58,y+3.5,PG_CW*0.42,5,FA_SIGN,8.5,false,1));
    return y+9;
}

// ---- footers --------------------------------------------------------------
// Barcode on the right, signature line on the left.
static void footBarcode(PrintDesign& d,double footY){
    double y=footY;
    d.items.push_back(mkHLine(PG_M,y-3,PG_CW,0.3));
    double bcW=PG_CW*0.40, bcH=15, bcX=PG_M+PG_CW-bcW;
    d.items.push_back(mkBarcode(bcX,y,bcW,bcH));
    d.items.push_back(mkLabel(bcX,y+bcH+0.5,bcW,4,FA_BARCODE_CAP,8,false,1));
    d.items.push_back(mkField(PG_M,y+1,PG_CW*0.52,6,L"clinicphone",FA_PHONE,8.5,0));
    d.items.push_back(mkField(PG_M,y+7.5,PG_CW*0.52,6,L"clinicaddr",L"نشانی: ",8.5,0));
}
// Centred barcode with the clinic address underneath.
static void footCentered(PrintDesign& d,double footY){
    double y=footY;
    d.items.push_back(mkHLine(PG_M,y-3,PG_CW,0.3));
    double bcW=PG_CW*0.46, bcX=PG_M+(PG_CW-bcW)/2;
    d.items.push_back(mkBarcode(bcX,y,bcW,14));
    d.items.push_back(mkField(PG_M,y+15,PG_CW,5.5,L"clinicaddr",L"",8.5,1));
    d.items.push_back(mkField(PG_M,y+20.5,PG_CW,5.5,L"clinicphone",FA_PHONE,8.5,1));
}
// Barcode + QR pair on a tinted strip.
static void footDual(PrintDesign& d,double footY,unsigned tint,unsigned border){
    double y=footY-1;
    d.items.push_back(mkTint(PG_M,y,PG_CW,30,tint,border,0.3,1.5));
    d.items.push_back(mkQR(PG_M+2.5,y+2.5,25));
    double bcW=PG_CW*0.42, bcX=PG_M+PG_CW-bcW-2.5;
    d.items.push_back(mkBarcode(bcX,y+3,bcW,15));
    d.items.push_back(mkLabel(bcX,y+18.5,bcW,4,FA_BARCODE_CAP,8,false,1));
    d.items.push_back(mkField(PG_M+31,y+3,PG_CW-bcW-36,6,L"clinicphone",FA_PHONE,8.5,0));
    d.items.push_back(mkField(PG_M+31,y+10,PG_CW-bcW-36,6,L"clinicaddr",L"نشانی: ",8.5,0));
    d.items.push_back(mkLabel(PG_M+31,y+18,PG_CW-bcW-36,5,FA_KEEP,8,false,0));
}
// Minimal: a hairline plus a small right-hand barcode only.
static void footMinimal(PrintDesign& d,double footY){
    double y=footY+4;
    d.items.push_back(mkHLine(PG_M,y-3,PG_CW,0.25));
    double bcW=PG_CW*0.32, bcX=PG_M+PG_CW-bcW;
    d.items.push_back(mkBarcode(bcX,y,bcW,13));
    d.items.push_back(mkLabel(PG_M,y+3,PG_CW*0.60,5.5,FA_KEEP,8.5,false,0));
}

// ===========================================================================
//  buildTemplate — 30 designs across TEN layout families.
//  Family list (3 variants each):
//     0  کلاسیک        classic centred title + 2×3 patient + tall table
//     1  باند رنگی      corporate colour band header + tinted patient card
//     2  فاکتور        invoice masthead grid + very tall detailed table
//     3  ستون کناری     left sidebar (logo/QR/barcode) + wide main column
//     4  خط‌کشی        pure monochrome line-art, no fills at all
//     5  کارتی         rounded tinted cards for every block
//     6  عکس و هویت     patient photo + identity grid + table
//     7  مالی          full financial columns + complete totals ladder
//     8  فشرده         compact upper page + dotted tear-off + patient copy
//     9  دو بلوکی       split meta/patient blocks + banded totals bar
//  EVERY variant calls servicesBlock*(), so all 30 print the live services.
// ===========================================================================
struct TplSpec {
    int      family;      // 0..9
    int      variant;     // 0..2
    int      svc;         // SvcPreset
    unsigned accent;      // brand colour
    unsigned tint;        // light fill
    unsigned headFill;    // services header fill (0 = line-art)
    double   bw;          // table border width
    double   rowH;        // pinned service row height (mm)
    bool     frame;       // page frame
};

static const TplSpec TPL[30] = {
  /* 01 */ {0,0, SVC4_ROW , 0x1F6FEB, 0xF2F6FC, 0xE8EDF4, 0.40, 7.2, false},
  /* 02 */ {0,1, SVC5     , 0x0B5ED7, 0xF1F5FB, 0xE4EBF5, 0.40, 7.0, false},
  /* 03 */ {0,2, SVC3     , 0x2F6F4E, 0xF1F7F3, 0x000000, 0.30, 7.6, false},
  /* 04 */ {1,0, SVC5     , 0x0B5ED7, 0xF3F7FD, 0xDDE7F6, 0.40, 7.0, false},
  /* 05 */ {1,1, SVC6_INS , 0x1F6FEB, 0xF2F6FC, 0xE1E9F5, 0.40, 6.8, false},
  /* 06 */ {1,2, SVC4_CAT , 0x6A0DAD, 0xF6F2FA, 0xEAE1F4, 0.40, 7.2, false},
  /* 07 */ {2,0, SVC7     , 0x14532D, 0xF2F7F3, 0xE6EDE8, 0.35, 6.6, true },
  /* 08 */ {2,1, SVC6_FIN , 0x0B5ED7, 0xF2F6FC, 0xE4EBF5, 0.35, 6.6, true },
  /* 09 */ {2,2, SVC5_CODE, 0x8A2E2E, 0xFAF2F2, 0xF0E3E3, 0.35, 6.8, true },
  /* 10 */ {3,0, SVC5     , 0x1F6FEB, 0xEFF4FB, 0xE2EAF5, 0.35, 7.0, false},
  /* 11 */ {3,1, SVC4_ROW , 0x0F766E, 0xEFF7F6, 0xE0EEEC, 0.35, 7.2, false},
  /* 12 */ {3,2, SVC6_FIN , 0x6A0DAD, 0xF5F1FA, 0xE9E0F3, 0.35, 6.8, false},
  /* 13 */ {4,0, SVC4_ROW , 0x000000, 0xFFFFFF, 0x000000, 0.30, 7.4, false},
  /* 14 */ {4,1, SVC5     , 0x000000, 0xFFFFFF, 0x000000, 0.45, 7.2, true },
  /* 15 */ {4,2, SVC7     , 0x000000, 0xFFFFFF, 0x000000, 0.30, 6.6, false},
  /* 16 */ {5,0, SVC5     , 0x1F6FEB, 0xF3F7FD, 0xE4ECF7, 0.35, 7.0, false},
  /* 17 */ {5,1, SVC4_CAT , 0x0F766E, 0xF0F7F6, 0xE1EFEC, 0.35, 7.2, false},
  /* 18 */ {5,2, SVC6_INS , 0xB45309, 0xFDF6EC, 0xF6E9D2, 0.35, 6.8, false},
  /* 19 */ {6,0, SVC4_ROW , 0x1F6FEB, 0xF2F6FC, 0xE6EDF6, 0.40, 7.2, false},
  /* 20 */ {6,1, SVC5     , 0x14532D, 0xF1F6F2, 0xE4EDE7, 0.40, 7.0, false},
  /* 21 */ {6,2, SVC3     , 0x6A0DAD, 0xF6F2FA, 0x000000, 0.30, 7.6, false},
  /* 22 */ {7,0, SVC6_FIN , 0x0B5ED7, 0xF2F6FC, 0xE2EAF5, 0.35, 6.8, false},
  /* 23 */ {7,1, SVC6_INS , 0x8A2E2E, 0xFAF2F2, 0xEFE2E2, 0.35, 6.8, false},
  /* 24 */ {7,2, SVC7     , 0x14532D, 0xF1F6F2, 0xE3ECE6, 0.30, 6.4, true },
  /* 25 */ {8,0, SVC4_ROW , 0x1F6FEB, 0xF2F6FC, 0xE6EDF6, 0.40, 7.0, false},
  /* 26 */ {8,1, SVC5     , 0x0F766E, 0xEFF7F6, 0xDEEDEA, 0.40, 6.8, false},
  /* 27 */ {8,2, SVC3     , 0x6A0DAD, 0xF6F2FA, 0xE9E0F3, 0.40, 7.2, false},
  /* 28 */ {9,0, SVC5     , 0x1F6FEB, 0xF2F6FC, 0xE4ECF7, 0.40, 7.0, false},
  /* 29 */ {9,1, SVC6_FIN , 0xB45309, 0xFDF6EC, 0xF6E9D2, 0.40, 6.8, false},
  /* 30 */ {9,2, SVC5_CODE, 0x6A0DAD, 0xF6F2FA, 0xE9E0F3, 0.40, 7.0, true },
};

// v1.62.0 — the 30 Persian gallery names. Previously these lived only in
// buildTemplateName() which NOTHING ever called, so all 30 seeded designs were
// stored NAMELESS and «طراحی چاپ» showed a gallery of blank cards. The name is
// now baked into the design by buildTemplate() itself.
static const wchar_t* const TPL_NAMES[30]={
    L"۰۱) کلاسیک — جدول خدمات بلند",
    L"۰۲) کلاسیک — لوگو کنار عنوان + مبلغ سطری",
    L"۰۳) کلاسیک — تک‌رنگ وسط‌چین",
    L"۰۴) باند رنگی — قبض پذیرش آبی",
    L"۰۵) باند رنگی — سهم بیمه و بیمار",
    L"۰۶) باند رنگی — بنفش با نوع خدمت",
    L"۰۷) فاکتور — هفت‌ستونه کامل",
    L"۰۸) فاکتور — ریز مالی با تخفیف",
    L"۰۹) فاکتور — کد خدمت و مبلغ",
    L"۱۰) ستون کناری — QR و بارکد",
    L"۱۱) ستون کناری — سبز آبی",
    L"۱۲) ستون کناری — ریز مالی",
    L"۱۳) خط‌کشی — تک‌رنگ چهارستونه",
    L"۱۴) خط‌کشی — قاب‌دار پنج‌ستونه",
    L"۱۵) خط‌کشی — هفت‌ستونه فشرده",
    L"۱۶) کارتی — گرد آبی",
    L"۱۷) کارتی — فیروزه‌ای نوع خدمت",
    L"۱۸) کارتی — کهربایی سهم بیمه",
    L"۱۹) عکس بیمار — چهارستونه",
    L"۲۰) عکس بیمار — باند سبز",
    L"۲۱) عکس بیمار — QR و تک‌رنگ",
    L"۲۲) مالی — تخفیف و مبلغ واحد",
    L"۲۳) مالی — سهم بیمه و بیمار",
    L"۲۴) مالی — هفت‌ستونه قاب‌دار",
    L"۲۵) فشرده — ته‌برگ نسخهٔ بیمار",
    L"۲۶) فشرده — ته‌برگ فیروزه‌ای",
    L"۲۷) فشرده — ته‌برگ سه‌ستونه",
    L"۲۸) دو بلوکی — باند آبی",
    L"۲۹) دو بلوکی — ریز مالی کهربایی",
    L"۳۰) دو بلوکی — بنفش قاب‌دار",
};

static PrintDesign buildTemplate(int idx){
    g_tplItemId = 1;
    PrintDesign d; d.kind=L"builtin";
    d.paper=L"A4"; d.orientation=0; d.paperW=PG_W; d.paperH=PG_H;
    if(idx<0||idx>=30) idx=0;
    d.name = TPL_NAMES[idx];
    const TplSpec& sp = TPL[idx];
    const double pt = 10.5;
    double y = PG_M;
    double footY = FOOT_Y;

    if(sp.frame) d.items.push_back(mkFrame(PG_W,PG_H,8,sp.accent,0.8));

    switch(sp.family){

    // ================= 0 — کلاسیک ==========================================
    case 0: {
        if(sp.variant==0)      y=hdrPlain(d,FA_CLINIC,18,true,0.7);
        else if(sp.variant==1) y=hdrLogoRight(d,FA_CLINIC,17,true,0.7);
        else                   y=hdrCenterLogo(d,FA_CLINIC,18,false,0.5);
        y=metaStrip4(d,y,pt-0.5);
        y=patient2x3(d,y,pt);
        d.items.push_back(mkHLine(PG_M,y-1,PG_CW,0.25));
        y=servicesBlock(d,y+1.5,pt,sp.svc,sp.headFill,sp.bw,sp.rowH,15,true,footY);
        totalsRow(d,footY-13,pt);
        footBarcode(d,footY+2);
    } break;

    // ================= 1 — باند رنگی =======================================
    case 1: {
        const wchar_t* doc = (sp.variant==0) ? L"قبض پذیرش و صورت‌حساب خدمات"
                            :(sp.variant==1) ? L"صورت‌حساب خدمات درمانی"
                                             : L"رسید پذیرش بیمار";
        y=hdrBand(d,FA_CLINIC,sp.accent,sp.variant!=2,doc);
        y=metaStripBoxed(d,y,pt-0.5,sp.tint,sp.accent);
        y=patientCard(d,y,pt-0.5,sp.tint,sp.accent,sp.accent);
        y=servicesBlock(d,y,pt,sp.svc,sp.headFill,sp.bw,sp.rowH,24,true,footY);
        totalsBar(d,footY-22,pt,sp.accent);
        footCentered(d,footY+3);
    } break;

    // ================= 2 — فاکتور ==========================================
    case 2: {
        const wchar_t* doc = (sp.variant==0) ? L"فاکتور خدمات درمانی"
                            :(sp.variant==1) ? L"فاکتور رسمی خدمات"
                                             : L"صورت‌حساب تفصیلی خدمات";
        y=hdrSplit(d,FA_CLINIC,sp.accent,doc);
        y=metaGrid6(d,y,pt-1,sp.accent);
        // one-line patient strip: the invoice family gives the table the most room
        {
            double c=PG_CW/3;
            d.items.push_back(mkFieldB(PG_M+2*c,y,c-2,7,L"fullName",FA_FULL,pt,0));
            d.items.push_back(mkField (PG_M+  c,y,c-2,7,L"nationalCode",FA_NID,pt,0));
            d.items.push_back(mkField (PG_M    ,y,c-2,7,L"insurance",FA_INS,pt,0)); y+=8;
            d.items.push_back(mkFieldB(PG_M+2*c,y,c-2,7,L"doctor",FA_DOCTOR,pt,0));
            d.items.push_back(mkField (PG_M+  c,y,c-2,7,L"performer",FA_PERF,pt,0));
            d.items.push_back(mkField (PG_M    ,y,c-2,7,L"patientType",FA_PTYPE,pt,0)); y+=10;
        }
        y=servicesBlock(d,y,pt,sp.svc,sp.headFill,sp.bw,sp.rowH,36,true,footY);
        totalsLadder(d,footY-34,pt,sp.accent);
        footMinimal(d,footY+2);
    } break;

    // ================= 3 — ستون کناری =====================================
    case 3: {
        double sbW=44, sbX=PG_M, mainX=PG_M+sbW+5, mainW=PG_CW-sbW-5;
        // sidebar panel spans header → footer
        d.items.push_back(mkTint(sbX,PG_M,sbW,footY-PG_M-2,sp.tint,sp.accent,0.35,2.0));
        d.items.push_back(mkLogo(sbX+(sbW-26)/2,PG_M+4,26,26));
        d.items.push_back(mkLabelC(sbX+2,PG_M+33,sbW-4,10,FA_CLINIC,10,true,1,sp.accent));
        d.items.push_back(mkHLineC(sbX+3,PG_M+45,sbW-6,0.35,sp.accent));
        d.items.push_back(mkQR(sbX+(sbW-30)/2,PG_M+48,30));
        d.items.push_back(mkField(sbX+2,PG_M+80,sbW-4,6,L"receiptNo",FA_RECEIPT,9,1));
        d.items.push_back(mkField(sbX+2,PG_M+86.5,sbW-4,6,L"date",FA_DATE,9,1));
        d.items.push_back(mkField(sbX+2,PG_M+93,sbW-4,6,L"time",FA_TIME,9,1));
        d.items.push_back(mkField(sbX+2,PG_M+99.5,sbW-4,6,L"shift",FA_SHIFT,9,1));
        d.items.push_back(mkHLineC(sbX+3,PG_M+108,sbW-6,0.3,sp.accent));
        d.items.push_back(mkField(sbX+2,PG_M+111,sbW-4,6,L"dept",FA_DEPT,9,1));
        d.items.push_back(mkField(sbX+2,PG_M+117.5,sbW-4,6,L"userName",FA_USER,9,1));
        d.items.push_back(mkBarcode(sbX+2,footY-PG_M-2+PG_M-46,sbW-4,26));
        d.items.push_back(mkLabel(sbX+2,footY-19,sbW-4,4,FA_BARCODE_CAP,7.5,false,1));
        // main column
        double my=PG_M+1;
        const wchar_t* doc = (sp.variant==0) ? L"قبض پذیرش و خدمات"
                            :(sp.variant==1) ? L"صورت‌حساب خدمات"
                                             : L"فاکتور خدمات درمانی";
        d.items.push_back(mkLabelC(mainX,my,mainW,9,doc,15,true,0,sp.accent));
        my+=11;
        d.items.push_back(mkHLineC(mainX,my,mainW,0.7,sp.accent)); my+=3.5;
        {
            double c=mainW/2;
            d.items.push_back(mkFieldB(mainX+c,my,c-2,7,L"fullName",FA_FULL,pt,0));
            d.items.push_back(mkField (mainX  ,my,c-2,7,L"nationalCode",FA_NID,pt,0)); my+=7.8;
            d.items.push_back(mkField (mainX+c,my,c-2,7,L"fatherName",FA_FATHER,pt,0));
            d.items.push_back(mkField (mainX  ,my,c-2,7,L"birthDate",FA_BIRTH,pt,0)); my+=7.8;
            d.items.push_back(mkField (mainX+c,my,c-2,7,L"insurance",FA_INS,pt,0));
            d.items.push_back(mkField (mainX  ,my,c-2,7,L"suppInsurance",FA_SUPP,pt,0)); my+=7.8;
            d.items.push_back(mkField (mainX+c,my,c-2,7,L"doctor",FA_DOCTOR,pt,0));
            d.items.push_back(mkField (mainX  ,my,c-2,7,L"mobile",FA_PHONE,pt,0)); my+=10;
        }
        my=servicesBlockAt(d,mainX,mainW,my,pt,sp.svc,sp.headFill,sp.bw,sp.rowH,
                           30,true,footY-2);
        // compact totals ladder inside the main column
        {
            double ly=footY-30, lw=mainW*0.60, lx=mainX+mainW-lw;
            d.items.push_back(mkHLineC(mainX,ly,mainW,0.6,sp.accent)); ly+=2;
            d.items.push_back(mkField (lx,ly,lw,6.2,L"total",FA_TOTAL,pt,0));        ly+=6.2;
            d.items.push_back(mkField (lx,ly,lw,6.2,L"insshare",FA_INSSHARE,pt,0));  ly+=6.2;
            d.items.push_back(mkField (lx,ly,lw,6.2,L"discount",FA_DISCOUNT,pt,0));  ly+=6.2;
            d.items.push_back(mkHLineC(lx,ly-0.5,lw,0.5,sp.accent));
            PrintItem f=mkFieldB(lx,ly,lw,7,L"paid",FA_FINAL,pt+1.5,0);
            f.textColor=sp.accent; d.items.push_back(f);
            d.items.push_back(mkHLine(mainX,footY-10,mainW*0.34,0.3));
            d.items.push_back(mkLabel(mainX,footY-9.5,mainW*0.34,5,FA_SIGN,8.5,false,1));
        }
    } break;

    // ================= 4 — خط‌کشی (mono line-art) =========================
    case 4: {
        if(sp.variant==0)      y=hdrPlain(d,FA_CLINIC,17,false,0.9);
        else if(sp.variant==1) y=hdrPlain(d,FA_CLINIC,19,true,1.2);
        else                   y=hdrLogoRight(d,FA_CLINIC,16,false,0.9);
        {   // ruled meta line
            double c=PG_CW/4;
            d.items.push_back(mkFieldB(PG_M+3*c,y,c,6.5,L"receiptNo",FA_RECEIPT,pt-0.5,0));
            d.items.push_back(mkField (PG_M+2*c,y,c,6.5,L"date",FA_DATE,pt-0.5,1));
            d.items.push_back(mkField (PG_M+  c,y,c,6.5,L"time",FA_TIME,pt-0.5,1));
            d.items.push_back(mkField (PG_M    ,y,c,6.5,L"shift",FA_SHIFT,pt-0.5,2));
            y+=7.5;
            d.items.push_back(mkHLine(PG_M,y,PG_CW,0.25)); y+=3;
        }
        y=patient3x3(d,y,pt);
        d.items.push_back(mkHLine(PG_M,y-1,PG_CW,0.25)); y+=2;
        y=servicesBlock(d,y,pt,sp.svc,0,sp.bw,sp.rowH,16,true,footY);
        totalsRow(d,footY-14,pt);
        footMinimal(d,footY+1);
    } break;

    // ================= 5 — کارتی ==========================================
    case 5: {
        // rounded card shell around the whole page content
        d.items.push_back(mkRectC(PG_M-2,PG_M-2,PG_CW+4,PG_H-2*PG_M+4,0.5,4.0,sp.accent));
        y=PG_M+2;
        if(sp.variant==0){
            d.items.push_back(mkLogo(PG_M+PG_CW-22,y,20,20));
            d.items.push_back(mkLabelC(PG_M+2,y+1,PG_CW-26,10,FA_CLINIC,16.5,true,0,sp.accent));
            d.items.push_back(mkLabel(PG_M+2,y+11.5,PG_CW-26,6,FA_SUBTITLE,8.5,false,0));
            y+=23;
        } else if(sp.variant==1){
            d.items.push_back(mkLabelC(PG_M+2,y,PG_CW-4,10,FA_CLINIC,17,true,1,sp.accent));
            d.items.push_back(mkLabel(PG_M+2,y+11,PG_CW-4,6,FA_SUBTITLE,8.5,false,1));
            y+=20;
        } else {
            d.items.push_back(mkBand(PG_M,y,PG_CW,15,sp.accent,0));
            d.items.push_back(mkLabelC(PG_M+3,y+3.2,PG_CW-6,8,FA_CLINIC,15,true,0,0xFFFFFF));
            y+=18;
        }
        y=metaStripBoxed(d,y,pt-0.5,sp.tint,sp.accent);
        y=patientCard(d,y,pt-0.5,sp.tint,sp.accent,sp.accent);
        y=servicesBlock(d,y,pt,sp.svc,sp.headFill,sp.bw,sp.rowH,32,true,footY);
        totalsBox(d,footY-31,pt,sp.tint,sp.accent,sp.accent);
        footDual(d,footY+1,sp.tint,sp.accent);
    } break;

    // ================= 6 — عکس و هویت =====================================
    case 6: {
        if(sp.variant==0)      y=hdrLogoRight(d,FA_CLINIC,17,true,0.7);
        else if(sp.variant==1) y=hdrBand(d,FA_CLINIC,sp.accent,true,L"پروندهٔ پذیرش بیمار");
        else                   y=hdrCenterLogo(d,FA_CLINIC,17,true,0.5);
        y=metaStrip4(d,y,pt-0.5);
        y=patientPhoto(d,y,pt-0.5,sp.variant==2);
        d.items.push_back(mkHLineC(PG_M,y-1,PG_CW,0.4,sp.accent)); y+=2;
        y=servicesBlock(d,y,pt,sp.svc,sp.headFill,sp.bw,sp.rowH,32,true,footY);
        totalsBox(d,footY-31,pt,sp.tint,sp.accent,sp.accent);
        footBarcode(d,footY+2);
    } break;

    // ================= 7 — مالی ===========================================
    case 7: {
        const wchar_t* doc = (sp.variant==0) ? L"صورت‌حساب مالی خدمات"
                            :(sp.variant==1) ? L"ریز صورت‌حساب و سهم بیمه"
                                             : L"صورت‌حساب تفصیلی مالی";
        y=hdrSplit(d,FA_CLINIC,sp.accent,doc);
        y=metaStripBoxed(d,y,pt-0.5,sp.tint,sp.accent);
        {
            double c=PG_CW/3;
            d.items.push_back(mkFieldB(PG_M+2*c,y,c-2,7,L"fullName",FA_FULL,pt,0));
            d.items.push_back(mkField (PG_M+  c,y,c-2,7,L"nationalCode",FA_NID,pt,0));
            d.items.push_back(mkField (PG_M    ,y,c-2,7,L"patientType",FA_PTYPE,pt,0)); y+=8;
            d.items.push_back(mkField (PG_M+2*c,y,c-2,7,L"insurance",FA_INS,pt,0));
            d.items.push_back(mkField (PG_M+  c,y,c-2,7,L"ins_percent",L"درصد پایه: ",pt,0));
            d.items.push_back(mkField (PG_M    ,y,c-2,7,L"insNo",L"شماره بیمه: ",pt,0)); y+=8;
            d.items.push_back(mkField (PG_M+2*c,y,c-2,7,L"suppInsurance",FA_SUPP,pt,0));
            d.items.push_back(mkField (PG_M+  c,y,c-2,7,L"supp_percent",L"درصد مکمل: ",pt,0));
            d.items.push_back(mkField (PG_M    ,y,c-2,7,L"doctor",FA_DOCTOR,pt,0)); y+=10;
        }
        y=servicesBlock(d,y,pt,sp.svc,sp.headFill,sp.bw,sp.rowH,36,true,footY);
        totalsLadder(d,footY-34,pt,sp.accent);
        footBarcode(d,footY+2);
    } break;

    // ================= 8 — فشرده + ته‌برگ =================================
    case 8: {
        // Main receipt occupies the top ~62% of the page; a dotted tear line and
        // a short «نسخهٔ بیمار» stub follow, so one A4 sheet gives two copies.
        double tear = 196.0;
        if(sp.variant==0)      y=hdrPlain(d,FA_CLINIC,16.5,false,0.6);
        else if(sp.variant==1) y=hdrLogoRight(d,FA_CLINIC,16,false,0.6);
        else                   y=hdrBand(d,FA_CLINIC,sp.accent,true,L"");
        y=metaStrip4(d,y,pt-1);
        y=patient2x3(d,y,pt-0.5);
        y=servicesBlock(d,y,pt-0.5,sp.svc,sp.headFill,sp.bw,sp.rowH,15,true,tear-4);
        totalsRow(d,tear-17,pt-0.5);
        // tear-off rule
        d.items.push_back(mkHLine(PG_M,tear,PG_CW,0.3));
        d.items.push_back(mkLabel(PG_M,tear+0.8,PG_CW,5,L"— — — محل جدا کردن — — —",8,false,1));
        // stub
        double sy=tear+8;
        d.items.push_back(mkLabelC(PG_M,sy,PG_CW,8,L"نسخهٔ بیمار — رسید پرداخت",12.5,true,1,sp.accent));
        sy+=10;
        d.items.push_back(mkTint(PG_M,sy,PG_CW,26,sp.tint,sp.accent,0.35,1.5));
        {
            double c=PG_CW/3;
            d.items.push_back(mkFieldB(PG_M+2*c+1.5,sy+2,c-3,6.5,L"fullName",FA_FULL,pt-0.5,0));
            d.items.push_back(mkField (PG_M+  c+1.5,sy+2,c-3,6.5,L"receiptNo",FA_RECEIPT,pt-0.5,0));
            d.items.push_back(mkField (PG_M    +1.5,sy+2,c-3,6.5,L"date",FA_DATE,pt-0.5,0));
            d.items.push_back(mkField (PG_M+2*c+1.5,sy+9.5,c-3,6.5,L"servicescount",FA_ROWS,pt-0.5,0));
            d.items.push_back(mkField (PG_M+  c+1.5,sy+9.5,c-3,6.5,L"total",FA_TOTAL,pt-0.5,0));
            d.items.push_back(mkField (PG_M    +1.5,sy+9.5,c-3,6.5,L"insshare",FA_INSSHARE,pt-0.5,0));
            PrintItem f=mkFieldB(PG_M+1.5,sy+17,PG_CW-3,7,L"paid",FA_FINAL,pt+1,0);
            f.textColor=sp.accent; d.items.push_back(f);
        }
        footBarcode(d,footY+4);
    } break;

    // ================= 9 — دو بلوکی =======================================
    default: {
        if(sp.variant==0)      y=hdrBand(d,FA_CLINIC,sp.accent,true,L"");
        else if(sp.variant==1) y=hdrPlain(d,FA_CLINIC,18,true,0.8);
        else                   y=hdrCenterLogo(d,FA_CLINIC,17,true,0.6);
        // two side-by-side blocks: reception meta (left) + patient (right)
        {
            double gw=PG_CW/2-3, lx=PG_M, rx=PG_M+gw+6, bh=38;
            d.items.push_back(mkTint(rx,y,gw,bh,sp.tint,sp.accent,0.35,1.5));
            d.items.push_back(mkBand(rx,y,gw,6.5,sp.accent,0));
            d.items.push_back(mkLabelC(rx+2,y+0.6,gw-4,5,L"مشخصات بیمار",9,true,0,0xFFFFFF));
            double ry=y+8;
            d.items.push_back(mkFieldB(rx+2,ry,gw-4,6.5,L"fullName",FA_FULL,pt-0.5,0)); ry+=7;
            d.items.push_back(mkField (rx+2,ry,gw-4,6.5,L"nationalCode",FA_NID,pt-0.5,0)); ry+=7;
            d.items.push_back(mkField (rx+2,ry,gw-4,6.5,L"birthDate",FA_BIRTH,pt-0.5,0)); ry+=7;
            d.items.push_back(mkField (rx+2,ry,gw-4,6.5,L"mobile",FA_PHONE,pt-0.5,0));

            d.items.push_back(mkTint(lx,y,gw,bh,sp.tint,sp.accent,0.35,1.5));
            d.items.push_back(mkBand(lx,y,gw,6.5,sp.accent,0));
            d.items.push_back(mkLabelC(lx+2,y+0.6,gw-4,5,L"اطلاعات پذیرش",9,true,0,0xFFFFFF));
            double ly=y+8;
            d.items.push_back(mkFieldB(lx+2,ly,gw-4,6.5,L"receiptNo",FA_RECEIPT,pt-0.5,0)); ly+=7;
            d.items.push_back(mkField (lx+2,ly,gw-4,6.5,L"date",FA_DATE,pt-0.5,0)); ly+=7;
            d.items.push_back(mkField (lx+2,ly,gw-4,6.5,L"shift",FA_SHIFT,pt-0.5,0)); ly+=7;
            d.items.push_back(mkField (lx+2,ly,gw-4,6.5,L"doctor",FA_DOCTOR,pt-0.5,0));
            y+=bh+4;
        }
        // insurance line
        {
            double c=PG_CW/3;
            d.items.push_back(mkField(PG_M+2*c,y,c-2,7,L"insurance",FA_INS,pt,0));
            d.items.push_back(mkField(PG_M+  c,y,c-2,7,L"suppInsurance",FA_SUPP,pt,0));
            d.items.push_back(mkField(PG_M    ,y,c-2,7,L"patientType",FA_PTYPE,pt,0));
            y+=9;
        }
        y=servicesBlock(d,y,pt,sp.svc,sp.headFill,sp.bw,sp.rowH,24,true,footY);
        totalsBar(d,footY-22,pt,sp.accent);
        footBarcode(d,footY+3);
    } break;
    }
    return d;
}

} // namespace

// Resolve just the NAME of template idx (migration matches by name).
// Shares the single TPL_NAMES table baked into buildTemplate(), so a design's
// stored name and its gallery caption can never drift apart again.
static std::wstring buildTemplateName(int idx){
    if(idx<0||idx>=30) return L"";
    return TPL_NAMES[idx];
}

// ----------------------------------------------------------------- seeding ---
// Seed the 30 built-in A4 templates AND migrate any previously-seeded
// builtins. The migration is idempotent: it ALWAYS rebuilds each builtin
// from buildTemplate(idx) on every version bump — so an existing install
// picks up the rebuilt services-table layouts automatically. A per-version
// one-shot setting flag guards re-runs. Stored design ids (and therefore
// section<->design bindings) are preserved.
void Designs_Init(){
    std::vector<PrintDesign> existing; Designs_Builtins(existing);
    const int N = 30;
    auto stamp=[](){
        setSetting(L"tpl_migration_1_62", L"1");
        setSetting(L"tpl_migration_1_61", L"1");
        setSetting(L"tpl_migration_1_60", L"1");
        setSetting(L"tpl_migration_1_59", L"1");
        setSetting(L"tpl_migration_1_58", L"1");
        setSetting(L"tpl_migration_1_53", L"1");
        setSetting(L"tpl_migration_1_52", L"1");
    };
    if(existing.empty()){
        for(int i=0;i<N;i++){ PrintDesign d=buildTemplate(i); Designs_Insert(d); }
        stamp();
        return;
    }
    if(getSetting(L"tpl_migration_1_62", L"0") != L"1"){
        // Overwrite the first N stored builtins in order (preserving their
        // ids/bindings), insert any missing, DELETE surplus beyond N.
        for(int i=0;i<N;i++){
            PrintDesign fresh=buildTemplate(i);
            if(i<(int)existing.size()){ fresh.id=existing[i].id; Designs_Update(fresh); }
            else Designs_Insert(fresh);
        }
        for(size_t i=N;i<existing.size();++i) Designs_Delete(existing[i].id);
        stamp();
    }
}
