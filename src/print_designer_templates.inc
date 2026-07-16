// ============================================================================
//  print_designer_templates.inc — 30 built-in A4 reception templates (§1.52.0)
//  Included into print_designer.cpp. Every template is authored in A4 mm
//  coordinates (210×297) and includes the dynamic services table. If the
//  operator switches the paper size to A5, printPrintDesign / pcPaintDesign
//  auto-scale the whole design proportionally (see the pscale logic), so the
//  layout relocates and shrinks to fit A5 perfectly — no per-template work.
//  Seeded on first run; existing installs are migrated once.
// ============================================================================

// --- small builder helpers --------------------------------------------------
namespace {

static int g_tplItemId = 1;
static int nid(){ return g_tplItemId++; }

static PrintItem mkLabel(double x,double y,double w,double h,
                         const std::wstring& text,double pt,bool bold,int align){
    PrintItem it; it.id=nid(); it.type=PIT_LABEL; it.x=x; it.y=y; it.w=w; it.h=h;
    it.text=text; it.fontPt=pt; it.bold=bold; it.align=align; return it;
}
static PrintItem mkField(double x,double y,double w,double h,
                         const std::wstring& field,const std::wstring& prefix,
                         double pt,int align){
    PrintItem it; it.id=nid(); it.type=PIT_FIELD; it.x=x; it.y=y; it.w=w; it.h=h;
    it.field=field; it.prefix=prefix; it.fontPt=pt; it.align=align; return it;
}
static PrintItem mkHLine(double x,double y,double w,double bw){
    PrintItem it; it.id=nid(); it.type=PIT_HLINE; it.x=x; it.y=y; it.w=w; it.h=0.2;
    it.borderWidth=bw; return it;
}
static PrintItem mkRect(double x,double y,double w,double h,double bw,double corner){
    PrintItem it; it.id=nid(); it.type=PIT_RECT; it.x=x; it.y=y; it.w=w; it.h=h;
    it.borderWidth=bw; it.corner=corner; it.fillTransparent=true; return it;
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
static PrintItem mkPhoto(double x,double y,double w,double h){
    PrintItem it; it.id=nid(); it.type=PIT_PHOTO; it.x=x; it.y=y; it.w=w; it.h=h;
    it.borderWidth=0.3; return it;
}
static PrintItem mkApptNo(double x,double y,double w,double h,double pt){
    PrintItem it; it.id=nid(); it.type=PIT_APPTNO; it.x=x; it.y=y; it.w=w; it.h=h;
    it.fontPt=pt; it.bold=true; it.align=1; it.startValue=1; it.step=1; return it;
}

// §1.52.0: dynamic services-list table. The model JSON describes the header
// (cols/labels/widths); rows are filled from the live ReceptionRecord.services
// vector at print time. RTL column order: col 0 = rightmost.
//   col 0 = ردیف (row #), col 1 = نام خدمت (name), col 2 = کد (code), col 3 = مبلغ (line total)
static PrintItem mkServices(double x,double y,double w,double h,double pt){
    PrintItem it; it.id=nid(); it.type=PIT_SERVICES; it.x=x; it.y=y; it.w=w; it.h=h;
    // ردیف = \u0631\u062f\u06cc\u0641, نام خدمت = \u0646\u0627\u0645 \u062e\u062f\u0645\u062a,
    // کد = \u06a9\u062f, مبلغ = \u0645\u0628\u0644\u063a
    it.text=L"{\"cols\":4,\"header\":true,\"widths\":[0.55,3.2,1.1,2.15],"
            L"\"labels\":[\"\u0631\u062f\u06cc\u0641\",\"\u0646\u0627\u0645 \u062e\u062f\u0645\u062a\","
            L"\"\u06a9\u062f\",\"\u0645\u0628\u0644\u063a\"]}";
    it.fontPt=pt; it.bold=false; it.align=1;
    it.borderColor=0x1F5FD6; it.borderWidth=0.35;
    it.textColor=0x0D2144; it.fillTransparent=true;
    it.padding=0.8;
    return it;
}

// localized caption for the services section
#define FA_SVCLIST  L"\u0644\u06cc\u0633\u062a \u062e\u062f\u0645\u0627\u062a"   // لیست خدمات

// localized field captions reused across templates
#define FA_CLINIC   L"\u062f\u0631\u0645\u0627\u0646\u06af\u0627\u0647 \u0622\u0632\u0627\u062f\u06cc \u0637\u0628"
#define FA_NAME     L"\u0646\u0627\u0645: "
#define FA_FAMILY   L"\u0646\u0627\u0645 \u062e\u0627\u0646\u0648\u0627\u062f\u06af\u06cc: "
#define FA_NID      L"\u06a9\u062f \u0645\u0644\u06cc: "
#define FA_DATE     L"\u062a\u0627\u0631\u06cc\u062e: "
#define FA_TIME     L"\u0633\u0627\u0639\u062a: "
#define FA_RECEIPT  L"\u0634\u0645\u0627\u0631\u0647 \u0642\u0628\u0636: "
#define FA_APPT     L"\u0634\u0645\u0627\u0631\u0647 \u0646\u0648\u0628\u062a"
#define FA_TOTAL    L"\u062c\u0645\u0639 \u06a9\u0644: "
#define FA_PAID     L"\u067e\u0631\u062f\u0627\u062e\u062a\u06cc: "
#define FA_INS      L"\u0628\u06cc\u0645\u0647: "
#define FA_DOCTOR   L"\u067e\u0632\u0634\u06a9: "
#define FA_PHONE    L"\u062a\u0644\u0641\u0646: "

// section titles (Persian, \u-escaped for source safety)
#define FA_INJECT   L"\u0628\u062e\u0634 \u062a\u0632\u0631\u06cc\u0642\u0627\u062a \u0648 \u067e\u0627\u0646\u0633\u0645\u0627\u0646"        // بخش تزریقات و پانسمان
#define FA_LAB      L"\u0622\u0632\u0645\u0627\u06cc\u0634\u06af\u0627\u0647 \u062a\u0634\u062e\u06cc\u0635 \u0637\u0628\u06cc"               // آزمایشگاه تشخیص طبی
#define FA_RADIO    L"\u0628\u062e\u0634 \u0631\u0627\u062f\u06cc\u0648\u0644\u0648\u0698\u06cc"                                              // بخش رادیولوژی
#define FA_PHYSIO   L"\u0628\u062e\u0634 \u0641\u06cc\u0632\u06cc\u0648\u062a\u0631\u0627\u067e\u06cc"                                       // بخش فیزیوتراپی
#define FA_DENTAL   L"\u0628\u062e\u0634 \u062f\u0646\u062f\u0627\u0646\u067e\u0632\u0634\u06a9\u06cc"                                       // بخش دندانپزشکی
#define FA_SURG     L"\u0628\u062e\u0634 \u062c\u0631\u0627\u062d\u06cc"                                                                     // بخش جراحی
#define FA_CARDIO   L"\u0628\u062e\u0634 \u0642\u0644\u0628 \u0648 \u0639\u0631\u0648\u0642"                                                 // بخش قلب و عروق
#define FA_PED      L"\u0628\u062e\u0634 \u0627\u0637\u0641\u0627\u0644"                                                                     // بخش اطفال
#define FA_GYN      L"\u0628\u062e\u0634 \u0632\u0646\u0627\u0646"                                                                           // بخش زنان
#define FA_ORTHO    L"\u0628\u062e\u0634 \u0627\u0631\u062a\u0648\u067e\u062f\u06cc"                                                         // بخش ارتوپدی
#define FA_EYE      L"\u0628\u062e\u0634 \u0686\u0634\u0645"                                                                                 // بخش چشم
#define FA_ENT      L"\u0628\u062e\u0634 \u06af\u0648\u0634 \u06af\u0644\u0648 \u0628\u06cc\u0646\u06cc"                                    // بخش گوش گلو بینی
#define FA_SKIN     L"\u0628\u062e\u0634 \u067e\u0648\u0633\u062a"                                                                           // بخش پوست

// §1.53.0 — the canonical A4 reception body. Lays out a 2-column patient grid,
// a centered "لیست خدمات" caption + dynamic services table, and a totals row.
// RTL column order is fixed: the RIGHT cell (x) carries «نام» (firstName) and the
// LEFT cell (x+col+gap) carries «نام خانوادگی» (lastName) — exactly how a Persian
// reader scans a row, and how the HTML admission form orders them.
// Returns the y-coordinate just below the totals so callers can place the
// appointment-number counter / footer without overlap. svcH defaults to 42mm so
// up to ~6 services print without compressing; it scales down cleanly to A5 via
// the responsive pscale.
static double addA4Body(PrintDesign& d,double m,double top,
                        double cw,double pt,double svcH=42.0){
    double x=m, y=top, rowH=8, col=cw/2-3;
    // patient info grid (2 columns, RTL). §1.53.0 FIX: the RIGHT cell (x+col+6)
    // carries the FIRST field of each pair so a Persian reader scans نام → نام
    // خانوادگی → کد ملی → بیمه → پزشک → تلفن right-to-left, exactly like the
    // admission form. The LEFT cell (x) carries the second field.
    d.items.push_back(mkField(x+col+6,y,col,rowH,L"firstName",FA_NAME,pt,0));
    d.items.push_back(mkField(x,y,col,rowH,L"lastName",FA_FAMILY,pt,0)); y+=rowH;
    d.items.push_back(mkField(x+col+6,y,col,rowH,L"nationalCode",FA_NID,pt,0));
    d.items.push_back(mkField(x,y,col,rowH,L"insurance",FA_INS,pt,0)); y+=rowH;
    d.items.push_back(mkField(x+col+6,y,col,rowH,L"doctor",FA_DOCTOR,pt,0));
    d.items.push_back(mkField(x,y,col,rowH,L"mobile",FA_PHONE,pt,0)); y+=rowH+2;
    // services list — the centerpiece (present in EVERY template). The header band
    // + first data row are always drawn even when the record has no services, so
    // the table is never an invisible blank rectangle on the printed page.
    d.items.push_back(mkLabel(x,y,cw,7,FA_SVCLIST,pt+1,true,1)); y+=8;
    d.items.push_back(mkServices(x,y,cw,svcH,pt-0.5)); y+=svcH+3;
    // totals row — RTL: «جمع کل» on the RIGHT (x+col+6), «پرداختی» on the LEFT (x)
    d.items.push_back(mkHLine(x,y,cw,0.3)); y+=2;
    d.items.push_back(mkField(x+col+6,y,col,rowH,L"total",FA_TOTAL,pt+0.5,0));
    d.items.push_back(mkField(x,y,col,rowH,L"paid",FA_PAID,pt+0.5,0)); y+=rowH;
    return y;
}

// Add ONLY the services block (caption + table) at a given position; returns
// the y just below it. The services block always renders (header + at least one
// placeholder row) so it is never an invisible blank rectangle.
static double addServicesBlock(PrintDesign& d,double x,double y,double w,double pt,double svcH){
    d.items.push_back(mkLabel(x,y,w,7,FA_SVCLIST,pt+1,true,1)); y+=8;
    d.items.push_back(mkServices(x,y,w,svcH,pt-0.5)); y += svcH + 2;
    return y;
}

// Build one template by index 0..29 (30 templates); returns the finished design.
// All templates are A4 (210×297) and include the services table.
static PrintDesign buildTemplate(int idx){
    g_tplItemId = 1;
    PrintDesign d; d.kind=L"builtin";
    auto setPaper=[&](const wchar_t* p,int orient){
        d.paper=p; d.orientation=orient; double w,h;
        if(Paper_Dims(p,w,h)){ d.paperW=w; d.paperH=h; }
        if(orient==1){ std::swap(d.paperW,d.paperH); }
    };
    // A4 portrait default geometry
    auto a4p=[&](){ setPaper(L"A4",0); };   // 210×297
    double m=14, cw;   // set per case after paper
    switch(idx){
    case 0: { // T01 پذیرش استاندارد A4 عمودی
        a4p(); cw=d.paperW-2*m;
        d.name=L"\u067e\u0630\u06cc\u0631\u0634 \u0627\u0633\u062a\u0627\u0646\u062f\u0627\u0631\u062f A4 \u0639\u0645\u0648\u062f\u06cc";
        d.items.push_back(mkLabel(m,m,cw,12,FA_CLINIC,18,true,1));
        d.items.push_back(mkHLine(m,m+14,cw,0.6));
        d.items.push_back(mkField(m,m+18,cw/2,8,L"receiptNo",FA_RECEIPT,11,2));
        d.items.push_back(mkField(m+cw/2,m+18,cw/2,8,L"date",FA_DATE,11,0));
        addA4Body(d,m,m+30,cw,11);
        d.items.push_back(mkApptNo(m,d.paperH-34,cw,14,26));
        d.items.push_back(mkLabel(m,d.paperH-18,cw,7,FA_APPT,10,false,1));
        break; }
    case 1: { // T02 پذیرش استاندارد A4 افقی
        setPaper(L"A4",1); cw=d.paperW-2*m;
        d.name=L"\u067e\u0630\u06cc\u0631\u0634 \u0627\u0633\u062a\u0627\u0646\u062f\u0627\u0631\u062f A4 \u0627\u0641\u0642\u06cc";
        d.items.push_back(mkLabel(m,m,cw,12,FA_CLINIC,18,true,1));
        d.items.push_back(mkHLine(m,m+14,cw,0.6));
        addA4Body(d,m,m+18,cw,11);
        d.items.push_back(mkApptNo(m,d.paperH-30,cw,14,26));
        d.items.push_back(mkLabel(m,d.paperH-14,cw,7,FA_APPT,10,false,1));
        break; }
    case 2: { // T03 پذیرش با لوگو در بالا
        a4p(); cw=d.paperW-2*m;
        d.name=L"\u067e\u0630\u06cc\u0631\u0634 \u0628\u0627 \u0644\u0648\u06af\u0648 \u062f\u0631 \u0628\u0627\u0644\u0627";
        d.items.push_back(mkLogo((d.paperW-28)/2,m,28,28));
        d.items.push_back(mkLabel(m,m+30,cw,12,FA_CLINIC,18,true,1));
        d.items.push_back(mkHLine(m,m+43,cw,0.6));
        d.items.push_back(mkField(m,m+47,cw/2,8,L"receiptNo",FA_RECEIPT,11,2));
        d.items.push_back(mkField(m+cw/2,m+47,cw/2,8,L"date",FA_DATE,11,0));
        addA4Body(d,m,m+59,cw,11);
        d.items.push_back(mkApptNo(m,d.paperH-30,cw,13,24));
        break; }
    case 3: { // T04 پذیرش با QR کنار شماره قبض
        a4p(); cw=d.paperW-2*m;
        d.name=L"\u067e\u0630\u06cc\u0631\u0634 \u0628\u0627 QR \u06a9\u0646\u0627\u0631 \u0634\u0645\u0627\u0631\u0647 \u0642\u0628\u0636";
        d.items.push_back(mkLabel(m,m,cw,12,FA_CLINIC,18,true,1));
        d.items.push_back(mkHLine(m,m+14,cw,0.6));
        d.items.push_back(mkQR(m,m+18,26));
        d.items.push_back(mkField(m+30,m+18,cw-30,8,L"receiptNo",FA_RECEIPT,11,0));
        d.items.push_back(mkField(m+30,m+28,cw-30,8,L"date",FA_DATE,11,0));
        addA4Body(d,m,m+50,cw,11);
        d.items.push_back(mkApptNo(m,d.paperH-30,cw,13,24));
        break; }
    case 4: { // T05 پذیرش با عکس پرسنلی
        a4p(); cw=d.paperW-2*m;
        d.name=L"\u067e\u0630\u06cc\u0631\u0634 \u0628\u0627 \u0639\u06a9\u0633 \u067e\u0631\u0633\u0646\u0644\u06cc";
        d.items.push_back(mkLabel(m,m,cw-30,12,FA_CLINIC,18,true,1));
        d.items.push_back(mkPhoto(d.paperW-m-26,m+2,26,32));
        d.items.push_back(mkHLine(m,m+14,cw,0.6));
        d.items.push_back(mkField(m,m+18,cw/2,8,L"receiptNo",FA_RECEIPT,11,2));
        d.items.push_back(mkField(m+cw/2,m+18,cw/2,8,L"date",FA_DATE,11,0));
        addA4Body(d,m,m+30,cw,11);
        d.items.push_back(mkApptNo(m,d.paperH-30,cw,13,24));
        break; }
    case 5: { // T06 پذیرش لاکچری با حاشیه طلایی
        a4p(); cw=d.paperW-2*m;
        d.name=L"\u067e\u0630\u06cc\u0631\u0634 \u0644\u0627\u06a8\u0686\u0631\u06cc \u0628\u0627 \u062d\u0627\u0634\u06cc\u0647 \u0637\u0644\u0627\u06cc\u06cc";
        d.items.push_back(mkFrame(d.paperW,d.paperH,9,0xC9A227,1.4));
        d.items.push_back(mkLabel(m,m,cw,13,FA_CLINIC,20,true,1));
        d.items.push_back(mkHLine(m,m+16,cw,0.7));
        d.items.push_back(mkField(m,m+20,cw/2,8,L"receiptNo",FA_RECEIPT,12,2));
        d.items.push_back(mkField(m+cw/2,m+20,cw/2,8,L"date",FA_DATE,12,0));
        addA4Body(d,m,m+32,cw,12);
        d.items.push_back(mkApptNo(m,d.paperH-32,cw,14,26));
        d.items.push_back(mkLabel(m,d.paperH-16,cw,7,FA_APPT,10,false,1));
        break; }
    case 6: { // T07 پذیرش مدرن مینیمال
        a4p(); cw=d.paperW-2*m;
        d.name=L"\u067e\u0630\u06cc\u0631\u0634 \u0645\u062f\u0631\u0646 \u0645\u06cc\u0646\u06cc\u0645\u0627\u0644";
        d.items.push_back(mkLabel(m,m,cw,13,FA_CLINIC,19,true,2));
        d.items.push_back(mkHLine(m,m+17,cw,1.1));
        d.items.push_back(mkField(m,m+21,cw/2,8,L"receiptNo",FA_RECEIPT,11,2));
        d.items.push_back(mkField(m+cw/2,m+21,cw/2,8,L"date",FA_DATE,11,0));
        addA4Body(d,m,m+33,cw,11);
        d.items.push_back(mkApptNo(m,d.paperH-30,cw,13,24));
        break; }
    case 7: { // T08 پذیرش بیمه پایه + تکمیلی دو ستونه
        a4p(); cw=d.paperW-2*m; double col=cw/2-3;
        d.name=L"\u067e\u0630\u06cc\u0631\u0634 \u0628\u06cc\u0645\u0647 \u067e\u0627\u06cc\u0647 + \u062a\u06a9\u0645\u06cc\u0644\u06cc";
        d.items.push_back(mkLabel(m,m,cw,12,FA_CLINIC,18,true,1));
        d.items.push_back(mkHLine(m,m+14,cw,0.6));
        d.items.push_back(mkField(m+col+6,m+18,col,8,L"firstName",FA_NAME,11,0));
        d.items.push_back(mkField(m,m+18,col,8,L"lastName",FA_FAMILY,11,0));
        d.items.push_back(mkRect(m,m+28,col,22,0.3,2));
        d.items.push_back(mkLabel(m+2,m+29,col,7,L"\u0628\u06cc\u0645\u0647 \u067e\u0627\u06cc\u0647",11,true,1));
        d.items.push_back(mkField(m+2,m+38,col-4,7,L"insurance",L"",10,2));
        d.items.push_back(mkRect(m+col+6,m+28,col,22,0.3,2));
        d.items.push_back(mkLabel(m+col+8,m+29,col,7,L"\u0628\u06cc\u0645\u0647 \u062a\u06a9\u0645\u06cc\u0644\u06cc",11,true,1));
        d.items.push_back(mkField(m+col+8,m+38,col-4,7,L"suppInsurance",L"",10,2));
        addServicesBlock(d,m,m+54,cw,11,42);
        d.items.push_back(mkHLine(m,m+106,cw,0.3));
        d.items.push_back(mkField(m+col+6,m+108,col,8,L"total",FA_TOTAL,12,0));
        d.items.push_back(mkField(m,m+108,col,8,L"paid",FA_PAID,12,0));
        d.items.push_back(mkApptNo(m,d.paperH-30,cw,13,24));
        break; }
    case 8: { // T09 تزریقات استاندارد
        a4p(); cw=d.paperW-2*m;
        d.name=L"\u062a\u0632\u0631\u06cc\u0642\u0627\u062a \u0627\u0633\u062a\u0627\u0646\u062f\u0627\u0631\u062f A4";
        d.items.push_back(mkLabel(m,m,cw,12,FA_INJECT,18,true,1));
        d.items.push_back(mkHLine(m,m+14,cw,0.6));
        d.items.push_back(mkField(m,m+18,cw/2,8,L"receiptNo",FA_RECEIPT,11,2));
        d.items.push_back(mkField(m+cw/2,m+18,cw/2,8,L"date",FA_DATE,11,0));
        addA4Body(d,m,m+30,cw,11);
        d.items.push_back(mkApptNo(m,d.paperH-30,cw,13,24));
        break; }
    case 9: { // T10 تزریقات با چک‌لیست دارو
        a4p(); cw=d.paperW-2*m;
        d.name=L"\u062a\u0632\u0631\u06cc\u0642\u0627\u062a \u0628\u0627 \u0686\u06a9\u200c\u0644\u06cc\u0633\u062a \u062f\u0627\u0631\u0648";
        d.items.push_back(mkLabel(m,m,cw,12,FA_INJECT,18,true,1));
        d.items.push_back(mkHLine(m,m+14,cw,0.6));
        d.items.push_back(mkField(m+cw/2,m+18,cw/2,8,L"firstName",FA_NAME,11,0));
        d.items.push_back(mkField(m,m+18,cw/2,8,L"lastName",FA_FAMILY,11,0));
        d.items.push_back(mkField(m+cw/2,m+28,cw/2,8,L"nationalCode",FA_NID,11,0));
        d.items.push_back(mkField(m,m+28,cw/2,8,L"insurance",FA_INS,11,0));
        addServicesBlock(d,m,m+40,cw,11,42);
        d.items.push_back(mkRect(m,m+94,cw,40,0.3,2));
        d.items.push_back(mkLabel(m+2,m+89,cw-4,7,L"\u0686\u06a9\u200c\u0644\u06cc\u0633\u062a \u062f\u0627\u0631\u0648\u0647\u0627",11,true,2));
        for(int i=0;i<5;i++) d.items.push_back(mkHLine(m+4,m+98+i*6,cw-8,0.2));
        d.items.push_back(mkApptNo(m,d.paperH-30,cw,13,24));
        break; }
    case 10: { // T11 آزمایشگاه رسید پذیرش
        a4p(); cw=d.paperW-2*m;
        d.name=L"\u0622\u0632\u0645\u0627\u06cc\u0634\u06af\u0627\u0647 \u0631\u0633\u06cc\u062f \u067e\u0630\u06cc\u0631\u0634 A4";
        d.items.push_back(mkLabel(m,m,cw,12,FA_LAB,18,true,1));
        d.items.push_back(mkHLine(m,m+14,cw,0.6));
        d.items.push_back(mkField(m,m+18,cw/2,8,L"receiptNo",FA_RECEIPT,11,2));
        d.items.push_back(mkField(m+cw/2,m+18,cw/2,8,L"date",FA_DATE,11,0));
        addA4Body(d,m,m+30,cw,11);
        d.items.push_back(mkApptNo(m,d.paperH-30,cw,13,24));
        break; }
    case 11: { // T12 آزمایشگاه با QR و کد ملی
        a4p(); cw=d.paperW-2*m;
        d.name=L"\u0622\u0632\u0645\u0627\u06cc\u0634\u06af\u0627\u0647 \u0628\u0627 QR \u0648 \u06a9\u062f \u0645\u0644\u06cc";
        d.items.push_back(mkLabel(m,m,cw-30,12,FA_LAB,18,true,1));
        d.items.push_back(mkQR(d.paperW-m-26,m+2,26));
        d.items.push_back(mkHLine(m,m+14,cw,0.6));
        d.items.push_back(mkField(m,m+18,cw/2,8,L"nationalCode",FA_NID,11,0));
        d.items.push_back(mkField(m+cw/2,m+18,cw/2,8,L"receiptNo",FA_RECEIPT,11,2));
        addA4Body(d,m,m+30,cw,11);
        d.items.push_back(mkApptNo(m,d.paperH-30,cw,13,24));
        break; }
    case 12: { // T13 رادیولوژی برگه پذیرش
        a4p(); cw=d.paperW-2*m;
        d.name=L"\u0631\u0627\u062f\u06cc\u0648\u0644\u0648\u0698\u06cc \u0628\u0631\u06af\u0647 \u067e\u0630\u06cc\u0631\u0634 A4";
        d.items.push_back(mkLabel(m,m,cw,12,FA_RADIO,18,true,1));
        d.items.push_back(mkHLine(m,m+14,cw,0.6));
        d.items.push_back(mkField(m,m+18,cw/2,8,L"receiptNo",FA_RECEIPT,11,2));
        d.items.push_back(mkField(m+cw/2,m+18,cw/2,8,L"date",FA_DATE,11,0));
        addA4Body(d,m,m+30,cw,11);
        d.items.push_back(mkApptNo(m,d.paperH-30,cw,13,24));
        break; }
    case 13: { // T14 رادیولوژی با عکس پرسنلی
        a4p(); cw=d.paperW-2*m;
        d.name=L"\u0631\u0627\u062f\u06cc\u0648\u0644\u0648\u0698\u06cc \u0628\u0627 \u0639\u06a9\u0633 \u067e\u0631\u0633\u0646\u0644\u06cc";
        d.items.push_back(mkLabel(m,m,cw-30,12,FA_RADIO,18,true,1));
        d.items.push_back(mkPhoto(d.paperW-m-26,m+2,26,32));
        d.items.push_back(mkHLine(m,m+14,cw,0.6));
        d.items.push_back(mkField(m,m+18,cw/2,8,L"receiptNo",FA_RECEIPT,11,2));
        d.items.push_back(mkField(m+cw/2,m+18,cw/2,8,L"date",FA_DATE,11,0));
        addA4Body(d,m,m+30,cw,11);
        d.items.push_back(mkApptNo(m,d.paperH-30,cw,13,24));
        break; }
    case 14: { // T15 فیزیوتراپی رسید جلسه
        a4p(); cw=d.paperW-2*m;
        d.name=L"\u0641\u06cc\u0632\u06cc\u0648\u062a\u0631\u0627\u067e\u06cc \u0631\u0633\u06cc\u062f \u062c\u0644\u0633\u0647 A4";
        d.items.push_back(mkLabel(m,m,cw,12,FA_PHYSIO,18,true,1));
        d.items.push_back(mkHLine(m,m+14,cw,0.6));
        d.items.push_back(mkField(m,m+18,cw/2,8,L"receiptNo",FA_RECEIPT,11,2));
        d.items.push_back(mkField(m+cw/2,m+18,cw/2,8,L"date",FA_DATE,11,0));
        addA4Body(d,m,m+30,cw,11);
        d.items.push_back(mkApptNo(m,d.paperH-30,cw,13,24));
        break; }
    case 15: { // T16 فیزیوتراپی کارت ۱۰ جلسه
        a4p(); cw=d.paperW-2*m;
        d.name=L"\u0641\u06cc\u0632\u06cc\u0648\u062a\u0631\u0627\u067e\u06cc \u06a9\u0627\u0631\u062a \u06f1\u06f0 \u062c\u0644\u0633\u0647";
        d.items.push_back(mkFrame(d.paperW,d.paperH,9,0x1F6FEB,0.8));
        d.items.push_back(mkLabel(m,m,cw,11,FA_PHYSIO,17,true,1));
        d.items.push_back(mkField(m+cw/2,m+14,cw/2,8,L"firstName",FA_NAME,11,0));
        d.items.push_back(mkField(m,m+14,cw/2,8,L"lastName",FA_FAMILY,11,0));
        addServicesBlock(d,m,m+26,cw,11,34);
        for(int i=0;i<10;i++){
            double bx=m+(i%5)*((cw)/5);
            double by=m+74+(i/5)*22;
            d.items.push_back(mkRect(bx+1,by,((cw)/5)-2,16,0.3,1));
            wchar_t b[8]; swprintf(b,8,L"%d",i+1);
            d.items.push_back(mkLabel(bx+1,by+4,((cw)/5)-2,7,b,11,true,1));
        }
        d.items.push_back(mkApptNo(m,d.paperH-28,cw,12,22));
        break; }
    case 16: { // T17 دندانپزشکی پذیرش
        a4p(); cw=d.paperW-2*m;
        d.name=L"\u062f\u0646\u062f\u0627\u0646\u067e\u0632\u0634\u06a9\u06cc \u067e\u0630\u06cc\u0631\u0634 A4";
        d.items.push_back(mkLabel(m,m,cw,12,FA_DENTAL,18,true,1));
        d.items.push_back(mkHLine(m,m+14,cw,0.6));
        d.items.push_back(mkField(m,m+18,cw/2,8,L"receiptNo",FA_RECEIPT,11,2));
        d.items.push_back(mkField(m+cw/2,m+18,cw/2,8,L"date",FA_DATE,11,0));
        addA4Body(d,m,m+30,cw,11);
        d.items.push_back(mkApptNo(m,d.paperH-30,cw,13,24));
        break; }
    case 17: { // T18 جراحی پذیرش
        a4p(); cw=d.paperW-2*m;
        d.name=L"\u062c\u0631\u0627\u062d\u06cc \u067e\u0630\u06cc\u0631\u0634 A4";
        d.items.push_back(mkLabel(m,m,cw,12,FA_SURG,18,true,1));
        d.items.push_back(mkHLine(m,m+14,cw,0.6));
        d.items.push_back(mkField(m,m+18,cw/2,8,L"receiptNo",FA_RECEIPT,11,2));
        d.items.push_back(mkField(m+cw/2,m+18,cw/2,8,L"date",FA_DATE,11,0));
        addA4Body(d,m,m+30,cw,11);
        d.items.push_back(mkApptNo(m,d.paperH-30,cw,13,24));
        break; }
    case 18: { // T19 قلب و عروق پذیرش
        a4p(); cw=d.paperW-2*m;
        d.name=L"\u0642\u0644\u0628 \u0648 \u0639\u0631\u0648\u0642 \u067e\u0630\u06cc\u0631\u0634 A4";
        d.items.push_back(mkLabel(m,m,cw,12,FA_CARDIO,18,true,1));
        d.items.push_back(mkHLine(m,m+14,cw,0.6));
        d.items.push_back(mkField(m,m+18,cw/2,8,L"receiptNo",FA_RECEIPT,11,2));
        d.items.push_back(mkField(m+cw/2,m+18,cw/2,8,L"date",FA_DATE,11,0));
        addA4Body(d,m,m+30,cw,11);
        d.items.push_back(mkApptNo(m,d.paperH-30,cw,13,24));
        break; }
    case 19: { // T20 اطفال پذیرش
        a4p(); cw=d.paperW-2*m;
        d.name=L"\u0627\u0637\u0641\u0627\u0644 \u067e\u0630\u06cc\u0631\u0634 A4";
        d.items.push_back(mkLabel(m,m,cw,12,FA_PED,18,true,1));
        d.items.push_back(mkHLine(m,m+14,cw,0.6));
        d.items.push_back(mkField(m,m+18,cw/2,8,L"receiptNo",FA_RECEIPT,11,2));
        d.items.push_back(mkField(m+cw/2,m+18,cw/2,8,L"date",FA_DATE,11,0));
        addA4Body(d,m,m+30,cw,11);
        d.items.push_back(mkApptNo(m,d.paperH-30,cw,13,24));
        break; }
    case 20: { // T21 زنان پذیرش
        a4p(); cw=d.paperW-2*m;
        d.name=L"\u0632\u0646\u0627\u0646 \u067e\u0630\u06cc\u0631\u0634 A4";
        d.items.push_back(mkLabel(m,m,cw,12,FA_GYN,18,true,1));
        d.items.push_back(mkHLine(m,m+14,cw,0.6));
        d.items.push_back(mkField(m,m+18,cw/2,8,L"receiptNo",FA_RECEIPT,11,2));
        d.items.push_back(mkField(m+cw/2,m+18,cw/2,8,L"date",FA_DATE,11,0));
        addA4Body(d,m,m+30,cw,11);
        d.items.push_back(mkApptNo(m,d.paperH-30,cw,13,24));
        break; }
    case 21: { // T22 ارتوپدی پذیرش
        a4p(); cw=d.paperW-2*m;
        d.name=L"\u0627\u0631\u062a\u0648\u067e\u062f\u06cc \u067e\u0630\u06cc\u0631\u0634 A4";
        d.items.push_back(mkLabel(m,m,cw,12,FA_ORTHO,18,true,1));
        d.items.push_back(mkHLine(m,m+14,cw,0.6));
        d.items.push_back(mkField(m,m+18,cw/2,8,L"receiptNo",FA_RECEIPT,11,2));
        d.items.push_back(mkField(m+cw/2,m+18,cw/2,8,L"date",FA_DATE,11,0));
        addA4Body(d,m,m+30,cw,11);
        d.items.push_back(mkApptNo(m,d.paperH-30,cw,13,24));
        break; }
    case 22: { // T23 چشم پذیرش
        a4p(); cw=d.paperW-2*m;
        d.name=L"\u0686\u0634\u0645 \u067e\u0630\u06cc\u0631\u0634 A4";
        d.items.push_back(mkLabel(m,m,cw,12,FA_EYE,18,true,1));
        d.items.push_back(mkHLine(m,m+14,cw,0.6));
        d.items.push_back(mkField(m,m+18,cw/2,8,L"receiptNo",FA_RECEIPT,11,2));
        d.items.push_back(mkField(m+cw/2,m+18,cw/2,8,L"date",FA_DATE,11,0));
        addA4Body(d,m,m+30,cw,11);
        d.items.push_back(mkApptNo(m,d.paperH-30,cw,13,24));
        break; }
    case 23: { // T24 گوش گلو بینی پذیرش
        a4p(); cw=d.paperW-2*m;
        d.name=L"\u06af\u0648\u0634 \u06af\u0644\u0648 \u0628\u06cc\u0646\u06cc \u067e\u0630\u06cc\u0631\u0634 A4";
        d.items.push_back(mkLabel(m,m,cw,12,FA_ENT,18,true,1));
        d.items.push_back(mkHLine(m,m+14,cw,0.6));
        d.items.push_back(mkField(m,m+18,cw/2,8,L"receiptNo",FA_RECEIPT,11,2));
        d.items.push_back(mkField(m+cw/2,m+18,cw/2,8,L"date",FA_DATE,11,0));
        addA4Body(d,m,m+30,cw,11);
        d.items.push_back(mkApptNo(m,d.paperH-30,cw,13,24));
        break; }
    case 24: { // T25 پوست پذیرش
        a4p(); cw=d.paperW-2*m;
        d.name=L"\u067e\u0648\u0633\u062a \u067e\u0630\u06cc\u0631\u0634 A4";
        d.items.push_back(mkLabel(m,m,cw,12,FA_SKIN,18,true,1));
        d.items.push_back(mkHLine(m,m+14,cw,0.6));
        d.items.push_back(mkField(m,m+18,cw/2,8,L"receiptNo",FA_RECEIPT,11,2));
        d.items.push_back(mkField(m+cw/2,m+18,cw/2,8,L"date",FA_DATE,11,0));
        addA4Body(d,m,m+30,cw,11);
        d.items.push_back(mkApptNo(m,d.paperH-30,cw,13,24));
        break; }
    case 25: { // T26 پذیرش با لوگو و QR
        a4p(); cw=d.paperW-2*m;
        d.name=L"\u067e\u0630\u06cc\u0631\u0634 \u0628\u0627 \u0644\u0648\u06af\u0648 \u0648 QR";
        d.items.push_back(mkLogo(m,m,26,26));
        d.items.push_back(mkLabel(m+30,m+4,cw-58,12,FA_CLINIC,17,true,1));
        d.items.push_back(mkQR(d.paperW-m-26,m,26));
        d.items.push_back(mkHLine(m,m+30,cw,0.6));
        d.items.push_back(mkField(m,m+34,cw/2,8,L"receiptNo",FA_RECEIPT,11,2));
        d.items.push_back(mkField(m+cw/2,m+34,cw/2,8,L"date",FA_DATE,11,0));
        addA4Body(d,m,m+46,cw,11);
        d.items.push_back(mkApptNo(m,d.paperH-30,cw,13,24));
        break; }
    case 26: { // T27 پذیرش با قاب آبی
        a4p(); cw=d.paperW-2*m;
        d.name=L"\u067e\u0630\u06cc\u0631\u0634 \u0628\u0627 \u0642\u0627\u0628 \u0622\u0628\u06cc";
        d.items.push_back(mkFrame(d.paperW,d.paperH,9,0x1F5FD6,1.0));
        d.items.push_back(mkLabel(m,m,cw,12,FA_CLINIC,18,true,1));
        d.items.push_back(mkHLine(m,m+14,cw,0.6));
        d.items.push_back(mkField(m,m+18,cw/2,8,L"receiptNo",FA_RECEIPT,11,2));
        d.items.push_back(mkField(m+cw/2,m+18,cw/2,8,L"date",FA_DATE,11,0));
        addA4Body(d,m,m+30,cw,11);
        d.items.push_back(mkApptNo(m,d.paperH-30,cw,13,24));
        break; }
    case 27: { // T28 پذیرش فشرده همه‌چیز در یک نگاه
        a4p(); cw=d.paperW-2*m; double col=cw/3-2;
        d.name=L"\u067e\u0630\u06cc\u0631\u0634 \u0641\u0634\u0631\u062f\u0647 \u0633\u0647 \u0633\u062a\u0648\u0646\u06cc";
        d.items.push_back(mkLabel(m,m,cw,11,FA_CLINIC,17,true,1));
        d.items.push_back(mkHLine(m,m+13,cw,0.5));
        d.items.push_back(mkField(m+2*(col+3),m+16,col,7,L"receiptNo",FA_RECEIPT,10,2));
        d.items.push_back(mkField(m+col+3,m+16,col,7,L"date",FA_DATE,10,2));
        d.items.push_back(mkField(m,m+16,col,7,L"insurance",FA_INS,10,2));
        d.items.push_back(mkField(m+2*(col+3),m+24,col,7,L"firstName",FA_NAME,10,2));
        d.items.push_back(mkField(m+col+3,m+24,col,7,L"lastName",FA_FAMILY,10,2));
        d.items.push_back(mkField(m,m+24,col,7,L"nationalCode",FA_NID,10,2));
        d.items.push_back(mkField(m+2*(col+3),m+32,col,7,L"doctor",FA_DOCTOR,10,2));
        d.items.push_back(mkField(m+col+3,m+32,col,7,L"mobile",FA_PHONE,10,2));
        d.items.push_back(mkField(m,m+32,col,7,L"paid",FA_PAID,10,2));
        addServicesBlock(d,m,m+42,cw,11,42);
        d.items.push_back(mkHLine(m,m+98,cw,0.3));
        d.items.push_back(mkField(m+cw/2,m+100,cw/2,8,L"total",FA_TOTAL,12,0));
        d.items.push_back(mkField(m,m+100,cw/2,8,L"paid",FA_PAID,12,0));
        d.items.push_back(mkApptNo(m,d.paperH-28,cw,12,22));
        break; }
    case 28: { // T29 پذیرش با عنوان رنگی
        a4p(); cw=d.paperW-2*m;
        d.name=L"\u067e\u0630\u06cc\u0631\u0634 \u0628\u0627 \u0639\u0646\u0648\u0627\u0646 \u0631\u0646\u06af\u06cc";
        PrintItem hd=mkRect(m,m,cw,16,0,0); hd.fillTransparent=false; hd.fillColor=0x1F5FD6;
        d.items.push_back(hd);
        PrintItem ht=mkLabel(m,m+2,cw,12,FA_CLINIC,19,true,1); ht.textColor=0xFFFFFF;
        d.items.push_back(ht);
        d.items.push_back(mkField(m,m+20,cw/2,8,L"receiptNo",FA_RECEIPT,11,2));
        d.items.push_back(mkField(m+cw/2,m+20,cw/2,8,L"date",FA_DATE,11,0));
        addA4Body(d,m,m+32,cw,11);
        d.items.push_back(mkApptNo(m,d.paperH-30,cw,13,24));
        break; }
    case 29: { // T30 پذیرش جامع با همه فیلدها
        a4p(); cw=d.paperW-2*m; double col=cw/2-3;
        d.name=L"\u067e\u0630\u06cc\u0631\u0634 \u062c\u0627\u0645\u0639 \u0628\u0627 \u0647\u0645\u0647 \u0641\u06cc\u0644\u062f\u0647\u0627";
        d.items.push_back(mkLabel(m,m,cw,13,FA_CLINIC,20,true,1));
        d.items.push_back(mkHLine(m,m+16,cw,0.7));
        d.items.push_back(mkField(m+col+6,m+20,col,8,L"receiptNo",FA_RECEIPT,11,2));
        d.items.push_back(mkField(m,m+20,col,8,L"date",FA_DATE,11,0));
        d.items.push_back(mkField(m+col+6,m+29,col,8,L"firstName",FA_NAME,11,0));
        d.items.push_back(mkField(m,m+29,col,8,L"lastName",FA_FAMILY,11,0));
        d.items.push_back(mkField(m+col+6,m+38,col,8,L"nationalCode",FA_NID,11,0));
        d.items.push_back(mkField(m,m+38,col,8,L"insurance",FA_INS,11,0));
        d.items.push_back(mkField(m+col+6,m+47,col,8,L"doctor",FA_DOCTOR,11,0));
        d.items.push_back(mkField(m,m+47,col,8,L"mobile",FA_PHONE,11,0));
        addServicesBlock(d,m,m+58,cw,11,42);
        d.items.push_back(mkHLine(m,m+114,cw,0.3));
        d.items.push_back(mkField(m+col+6,m+116,col,8,L"total",FA_TOTAL,12,0));
        d.items.push_back(mkField(m,m+116,col,8,L"paid",FA_PAID,12,0));
        d.items.push_back(mkApptNo(m,d.paperH-32,cw,14,26));
        d.items.push_back(mkLabel(m,d.paperH-16,cw,7,FA_APPT,10,false,1));
        break; }
    }
    return d;
}

} // namespace

// Resolve just the NAME of template idx (so migration can match by name without
// rebuilding the whole design). Mirrors the names hardcoded in buildTemplate().
static std::wstring buildTemplateName(int idx){
    switch(idx){
    case 0: return L"\u067e\u0630\u06cc\u0631\u0634 \u0627\u0633\u062a\u0627\u0646\u062f\u0627\u0631\u062f A4 \u0639\u0645\u0648\u062f\u06cc";
    case 1: return L"\u067e\u0630\u06cc\u0631\u0634 \u0627\u0633\u062a\u0627\u0646\u062f\u0627\u0631\u062f A4 \u0627\u0641\u0642\u06cc";
    case 2: return L"\u067e\u0630\u06cc\u0631\u0634 \u0628\u0627 \u0644\u0648\u06af\u0648 \u062f\u0631 \u0628\u0627\u0644\u0627";
    case 3: return L"\u067e\u0630\u06cc\u0631\u0634 \u0628\u0627 QR \u06a9\u0646\u0627\u0631 \u0634\u0645\u0627\u0631\u0647 \u0642\u0628\u0636";
    case 4: return L"\u067e\u0630\u06cc\u0631\u0634 \u0628\u0627 \u0639\u06a9\u0633 \u067e\u0631\u0633\u0646\u0644\u06cc";
    case 5: return L"\u067e\u0630\u06cc\u0631\u0634 \u0644\u0627\u06a8\u0686\u0631\u06cc \u0628\u0627 \u062d\u0627\u0634\u06cc\u0647 \u0637\u0644\u0627\u06cc\u06cc";
    case 6: return L"\u067e\u0630\u06cc\u0631\u0634 \u0645\u062f\u0631\u0646 \u0645\u06cc\u0646\u06cc\u0645\u0627\u0644";
    case 7: return L"\u067e\u0630\u06cc\u0631\u0634 \u0628\u06cc\u0645\u0647 \u067e\u0627\u06cc\u0647 + \u062a\u06a9\u0645\u06cc\u0644\u06cc";
    case 8: return L"\u062a\u0632\u0631\u06cc\u0642\u0627\u062a \u0627\u0633\u062a\u0627\u0646\u062f\u0627\u0631\u062f A4";
    case 9: return L"\u062a\u0632\u0631\u06cc\u0642\u0627\u062a \u0628\u0627 \u0686\u06a9\u200c\u0644\u06cc\u0633\u062a \u062f\u0627\u0631\u0648";
    case 10: return L"\u0622\u0632\u0645\u0627\u06cc\u0634\u06af\u0627\u0647 \u0631\u0633\u06cc\u062f \u067e\u0630\u06cc\u0631\u0634 A4";
    case 11: return L"\u0622\u0632\u0645\u0627\u06cc\u0634\u06af\u0627\u0647 \u0628\u0627 QR \u0648 \u06a9\u062f \u0645\u0644\u06cc";
    case 12: return L"\u0631\u0627\u062f\u06cc\u0648\u0644\u0648\u0698\u06cc \u0628\u0631\u06af\u0647 \u067e\u0630\u06cc\u0631\u0634 A4";
    case 13: return L"\u0631\u0627\u062f\u06cc\u0648\u0644\u0648\u0698\u06cc \u0628\u0627 \u0639\u06a9\u0633 \u067e\u0631\u0633\u0646\u0644\u06cc";
    case 14: return L"\u0641\u06cc\u0632\u06cc\u0648\u062a\u0631\u0627\u067e\u06cc \u0631\u0633\u06cc\u062f \u062c\u0644\u0633\u0647 A4";
    case 15: return L"\u0641\u06cc\u0632\u06cc\u0648\u062a\u0631\u0627\u067e\u06cc \u06a9\u0627\u0631\u062a \u06f1\u06f0 \u062c\u0644\u0633\u0647";
    case 16: return L"\u062f\u0646\u062f\u0627\u0646\u067e\u0632\u0634\u06a9\u06cc \u067e\u0630\u06cc\u0631\u0634 A4";
    case 17: return L"\u062c\u0631\u0627\u062d\u06cc \u067e\u0630\u06cc\u0631\u0634 A4";
    case 18: return L"\u0642\u0644\u0628 \u0648 \u0639\u0631\u0648\u0642 \u067e\u0630\u06cc\u0631\u0634 A4";
    case 19: return L"\u0627\u0637\u0641\u0627\u0644 \u067e\u0630\u06cc\u0631\u0634 A4";
    case 20: return L"\u0632\u0646\u0627\u0646 \u067e\u0630\u06cc\u0631\u0634 A4";
    case 21: return L"\u0627\u0631\u062a\u0648\u067e\u062f\u06cc \u067e\u0630\u06cc\u0631\u0634 A4";
    case 22: return L"\u0686\u0634\u0645 \u067e\u0630\u06cc\u0631\u0634 A4";
    case 23: return L"\u06af\u0648\u0634 \u06af\u0644\u0648 \u0628\u06cc\u0646\u06cc \u067e\u0630\u06cc\u0631\u0634 A4";
    case 24: return L"\u067e\u0648\u0633\u062a \u067e\u0630\u06cc\u0631\u0634 A4";
    case 25: return L"\u067e\u0630\u06cc\u0631\u0634 \u0628\u0627 \u0644\u0648\u06af\u0648 \u0648 QR";
    case 26: return L"\u067e\u0630\u06cc\u0631\u0634 \u0628\u0627 \u0642\u0627\u0628 \u0622\u0628\u06cc";
    case 27: return L"\u067e\u0630\u06cc\u0631\u0634 \u0641\u0634\u0631\u062f\u0647 \u0633\u0647 \u0633\u062a\u0648\u0646\u06cc";
    case 28: return L"\u067e\u0630\u06cc\u0631\u0634 \u0628\u0627 \u0639\u0646\u0648\u0627\u0646 \u0631\u0646\u06af\u06cc";
    case 29: return L"\u067e\u0630\u06cc\u0631\u0634 \u062c\u0627\u0645\u0639 \u0628\u0627 \u0647\u0645\u0647 \u0641\u06cc\u0644\u062f\u0647\u0627";
    default: return L"";
    }
}

// ----------------------------------------------------------------- seeding ---
// §1.53.0: seed the 30 built-in A4 templates AND migrate any previously-seeded
// builtins. The migration is idempotent: it ALWAYS rebuilds each builtin from
// buildTemplate(idx) on every version bump — so an existing install picks up
// the fixed RTL name/lastName order, taller services table, and prettier
// layouts automatically. A per-version one-shot setting flag guards re-runs.
// IMPORTANT: we preserve the stored design id and any section<->design bindings
// (bindings are stored separately by id, so overwriting the design content but
// keeping the id keeps every section's chosen template connected).
void Designs_Init(){
    std::vector<PrintDesign> existing; Designs_Builtins(existing);
    const int N = 30;
    // fresh install → seed all 30 A4 templates
    if(existing.empty()){
        for(int i=0;i<N;i++){
            PrintDesign d = buildTemplate(i);
            Designs_Insert(d);
        }
        setSetting(L"tpl_migration_1_53", L"1");
        setSetting(L"tpl_migration_1_52", L"1");
        return;
    }
    // already seeded → one-time migration to the 30 A4 templates (v1.53 fixes)
    if(getSetting(L"tpl_migration_1_53", L"0") != L"1"){
        for(int i=0;i<N;i++){
            // find the stored builtin whose name matches template idx i
            std::wstring wantName = buildTemplateName(i);
            bool found=false;
            for(auto& d : existing){
                if(d.name == wantName){
                    // §1.53.0: always rebuild so RTL fixes + taller services table
                    // + prettier layouts propagate to existing installs.
                    PrintDesign fresh = buildTemplate(i);
                    fresh.id = d.id;   // keep the same stored id / bindings
                    Designs_Update(fresh);
                    found=true; break;
                }
            }
            if(!found){
                // template idx not present yet (e.g. growing from 20→30) → insert it
                PrintDesign fresh = buildTemplate(i);
                Designs_Insert(fresh);
            }
        }
        setSetting(L"tpl_migration_1_53", L"1");
        setSetting(L"tpl_migration_1_52", L"1");
    }
}
