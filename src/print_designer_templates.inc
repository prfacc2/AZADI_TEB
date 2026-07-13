// ============================================================================
//  print_designer_templates.inc — 30 built-in reception templates (v1.44.0 §5;
//  was 20 in release 1.4.0 §3.8). RECEPTION_TEMPLATES[] + static_assert==30.
//  Included into print_designer.cpp. Each template is built programmatically so
//  the layout is balanced and looks good out of the box. Seeded on first run.
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
// v1.44.0 §4/§5: build a PIT_TABLE whose model JSON matches what pdParseTable
// (printer.cpp) and the JS designer both understand. `json` is the serialized
// table model. `kind` is "static" or "services".
static PrintItem mkTableJson(double x,double y,double w,double h,
                             const std::wstring& json,double pt){
    PrintItem it; it.id=nid(); it.type=PIT_TABLE; it.x=x; it.y=y; it.w=w; it.h=h;
    it.text=json; it.fontPt=pt; it.borderColor=0x333333; it.borderWidth=1; return it;
}
// A ready-made services table (repeating one template body row per service) with
// header, zebra stripe and per-column alignment — uses the v1.44 svc.row.* tokens.
static std::wstring servicesTableJson(){
    return
      L"{\"cols\":5,\"rows\":2,\"header\":true,\"kind\":\"services\",\"stripe\":true,"
      L"\"headerBg\":\"#eef2fb\",\"padding\":6,\"border\":{\"w\":1,\"color\":\"#333333\"},"
      L"\"widths\":[1,4,1,2,2],\"colAlign\":[\"center\",\"right\",\"center\",\"left\",\"left\"],"
      L"\"cells\":[["
      L"\"\\u0631\\u062f\\u06cc\\u0641\",\"\\u0634\\u0631\\u062d \\u062e\\u062f\\u0645\\u062a\","
      L"\"\\u062a\\u0639\\u062f\\u0627\\u062f\",\"\\u0633\\u0647\\u0645 \\u0628\\u06cc\\u0645\\u0647\","
      L"\"\\u0633\\u0647\\u0645 \\u0628\\u06cc\\u0645\\u0627\\u0631\"],["
      L"\"{{svc.row.idx}}\",\"{{svc.row.name}}\",\"{{svc.row.qty}}\","
      L"\"{{svc.row.insShare}}\",\"{{svc.row.patShare}}\"]]}";
}
// A static insurance/billing summary table using the v1.44 bill.* + ins.* tokens.
static std::wstring billSummaryTableJson(){
    return
      L"{\"cols\":2,\"rows\":6,\"header\":false,\"kind\":\"static\",\"stripe\":true,"
      L"\"headerBg\":\"#eef2fb\",\"padding\":5,\"border\":{\"w\":1,\"color\":\"#333333\"},"
      L"\"widths\":[3,2],\"colAlign\":[\"right\",\"left\"],\"cells\":[["
      L"\"\\u062c\\u0645\\u0639 \\u06a9\\u0644\",\"{{bill.gross}}\"],["
      L"\"\\u062a\\u062e\\u0641\\u06cc\\u0641\",\"{{bill.disc}}\"],["
      L"\"\\u0628\\u06cc\\u0645\\u0647 \\u067e\\u0627\\u06cc\\u0647 ({{ins.base.pct}})\",\"{{bill.org}}\"],["
      L"\"\\u0628\\u06cc\\u0645\\u0647 \\u062a\\u06a9\\u0645\\u06cc\\u0644\\u06cc ({{ins.supp.pct}})\",\"{{bill.supp}}\"],["
      L"\"\\u0633\\u0647\\u0645 \\u0628\\u06cc\\u0645\\u0627\\u0631\",\"{{bill.pat}}\"],["
      L"\"\\u062a\\u0627\\u0631\\u06cc\\u062e\",\"{{appt.date}}\"]]}";
}

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

// A generic "reception receipt" body added to many templates.
static void addReceptionBody(PrintDesign& d,double margin,double top,
                             double contentW,double pt){
    double x=margin, y=top, rowH=7.5;
    d.items.push_back(mkField(x,y,contentW,rowH,L"firstName",FA_NAME,pt,0)); y+=rowH;
    d.items.push_back(mkField(x,y,contentW,rowH,L"lastName",FA_FAMILY,pt,0)); y+=rowH;
    d.items.push_back(mkField(x,y,contentW,rowH,L"nationalCode",FA_NID,pt,0)); y+=rowH;
    d.items.push_back(mkField(x,y,contentW,rowH,L"insurance",FA_INS,pt,0)); y+=rowH;
    d.items.push_back(mkField(x,y,contentW,rowH,L"doctor",FA_DOCTOR,pt,0)); y+=rowH;
    d.items.push_back(mkHLine(x,y+1,contentW,0.3)); y+=rowH;
    d.items.push_back(mkField(x,y,contentW,rowH,L"total",FA_TOTAL,pt,0)); y+=rowH;
    d.items.push_back(mkField(x,y,contentW,rowH,L"paid",FA_PAID,pt,0)); y+=rowH;
}

// Build one template by index 0..19; returns the finished design.
static PrintDesign buildTemplate(int idx){
    g_tplItemId = 1;
    PrintDesign d; d.kind=L"builtin";
    auto setPaper=[&](const wchar_t* p,int orient){
        d.paper=p; d.orientation=orient; double w,h;
        if(Paper_Dims(p,w,h)){ d.paperW=w; d.paperH=h; }
        if(orient==1){ std::swap(d.paperW,d.paperH); }
    };
    switch(idx){
    case 0: { // T01 پذیرش استاندارد A5 عمودی
        setPaper(L"A5",0); d.name=L"\u067e\u0630\u06cc\u0631\u0634 \u0627\u0633\u062a\u0627\u0646\u062f\u0627\u0631\u062f A5 \u0639\u0645\u0648\u062f\u06cc";
        double m=10; double cw=d.paperW-2*m;
        d.items.push_back(mkLabel(m,m,cw,10,FA_CLINIC,16,true,1));
        d.items.push_back(mkHLine(m,m+12,cw,0.5));
        d.items.push_back(mkField(m,m+15,cw/2,7,L"receiptNo",FA_RECEIPT,10,2));
        d.items.push_back(mkField(m+cw/2,m+15,cw/2,7,L"date",FA_DATE,10,0));
        addReceptionBody(d,m,m+26,cw,11);
        d.items.push_back(mkApptNo(m,d.paperH-26,cw,12,22));
        d.items.push_back(mkLabel(m,d.paperH-14,cw,6,FA_APPT,9,false,1));
        break; }
    case 1: { // T02 پذیرش استاندارد A5 افقی
        setPaper(L"A5",1); d.name=L"\u067e\u0630\u06cc\u0631\u0634 \u0627\u0633\u062a\u0627\u0646\u062f\u0627\u0631\u062f A5 \u0627\u0641\u0642\u06cc";
        double m=10; double cw=d.paperW-2*m;
        d.items.push_back(mkLabel(m,m,cw,10,FA_CLINIC,16,true,1));
        d.items.push_back(mkHLine(m,m+12,cw,0.5));
        addReceptionBody(d,m,m+16,cw/2-2,11);
        d.items.push_back(mkApptNo(d.paperW-m-50,m+16,50,16,28));
        d.items.push_back(mkLabel(d.paperW-m-50,m+34,50,6,FA_APPT,9,false,1));
        break; }
    case 2: { // T03 پذیرش فشرده A6
        setPaper(L"A6",0); d.name=L"\u067e\u0630\u06cc\u0631\u0634 \u0641\u0634\u0631\u062f\u0647 A6";
        double m=6; double cw=d.paperW-2*m;
        d.items.push_back(mkLabel(m,m,cw,8,FA_CLINIC,12,true,1));
        d.items.push_back(mkHLine(m,m+9,cw,0.4));
        addReceptionBody(d,m,m+11,cw,9);
        d.items.push_back(mkApptNo(m,d.paperH-20,cw,10,18));
        break; }
    case 3: { // T04 رسید کوتاه 80mm حرارتی
        setPaper(L"R80",0); d.name=L"\u067e\u0630\u06cc\u0631\u0634 \u0631\u0633\u06cc\u062f \u06a9\u0648\u062a\u0627\u0647 \u06f8\u06f0mm \u062d\u0631\u0627\u0631\u062a\u06cc";
        double m=4; double cw=d.paperW-2*m;
        d.items.push_back(mkLabel(m,m,cw,8,FA_CLINIC,13,true,1));
        d.items.push_back(mkHLine(m,m+9,cw,0.3));
        addReceptionBody(d,m,m+11,cw,10);
        d.items.push_back(mkApptNo(m,d.paperH-22,cw,12,20));
        break; }
    case 4: { // T05 رسید 58mm حرارتی
        setPaper(L"R58",0); d.name=L"\u067e\u0630\u06cc\u0631\u0634 \u0631\u0633\u06cc\u062f \u06a9\u0648\u062a\u0627\u0647 \u06f5\u06f8mm \u062d\u0631\u0627\u0631\u062a\u06cc";
        double m=3; double cw=d.paperW-2*m;
        d.items.push_back(mkLabel(m,m,cw,7,FA_CLINIC,11,true,1));
        d.items.push_back(mkHLine(m,m+8,cw,0.3));
        addReceptionBody(d,m,m+10,cw,8);
        d.items.push_back(mkApptNo(m,d.paperH-20,cw,10,16));
        break; }
    case 5: { // T06 پذیرش با لوگو در بالا
        setPaper(L"A5",0); d.name=L"\u067e\u0630\u06cc\u0631\u0634 \u0628\u0627 \u0644\u0648\u06af\u0648 \u062f\u0631 \u0628\u0627\u0644\u0627";
        double m=10; double cw=d.paperW-2*m;
        d.items.push_back(mkLogo((d.paperW-24)/2,m,24,24));
        d.items.push_back(mkLabel(m,m+26,cw,10,FA_CLINIC,16,true,1));
        d.items.push_back(mkHLine(m,m+38,cw,0.5));
        addReceptionBody(d,m,m+42,cw,11);
        d.items.push_back(mkApptNo(m,d.paperH-24,cw,12,22));
        break; }
    case 6: { // T07 پذیرش با QR کنار شماره قبض
        setPaper(L"A5",0); d.name=L"\u067e\u0630\u06cc\u0631\u0634 \u0628\u0627 QR \u06a9\u0646\u0627\u0631 \u0634\u0645\u0627\u0631\u0647 \u0642\u0628\u0636";
        double m=10; double cw=d.paperW-2*m;
        d.items.push_back(mkLabel(m,m,cw,10,FA_CLINIC,16,true,1));
        d.items.push_back(mkHLine(m,m+12,cw,0.5));
        d.items.push_back(mkQR(m,m+15,22));
        d.items.push_back(mkField(m+26,m+15,cw-26,7,L"receiptNo",FA_RECEIPT,11,0));
        d.items.push_back(mkField(m+26,m+24,cw-26,7,L"date",FA_DATE,11,0));
        addReceptionBody(d,m,m+40,cw,11);
        d.items.push_back(mkApptNo(m,d.paperH-22,cw,12,22));
        break; }
    case 7: { // T08 بیمه پایه + تکمیلی دو ستونه
        setPaper(L"A5",0); d.name=L"\u067e\u0630\u06cc\u0631\u0634 \u0628\u06cc\u0645\u0647 \u067e\u0627\u06cc\u0647 + \u062a\u06a9\u0645\u06cc\u0644\u06cc (\u062f\u0648 \u0633\u062a\u0648\u0646\u0647)";
        double m=10; double cw=d.paperW-2*m; double col=cw/2-2;
        d.items.push_back(mkLabel(m,m,cw,10,FA_CLINIC,15,true,1));
        d.items.push_back(mkHLine(m,m+12,cw,0.5));
        d.items.push_back(mkField(m,m+16,cw,7,L"firstName",FA_NAME,11,0));
        d.items.push_back(mkField(m,m+24,cw,7,L"lastName",FA_FAMILY,11,0));
        d.items.push_back(mkRect(m,m+34,col,30,0.3,2));
        d.items.push_back(mkLabel(m,m+35,col,6,L"\u0628\u06cc\u0645\u0647 \u067e\u0627\u06cc\u0647",10,true,1));
        d.items.push_back(mkField(m+2,m+42,col-4,6,L"insurance",L"",9,2));
        d.items.push_back(mkRect(m+col+4,m+34,col,30,0.3,2));
        d.items.push_back(mkLabel(m+col+4,m+35,col,6,L"\u0628\u06cc\u0645\u0647 \u062a\u06a9\u0645\u06cc\u0644\u06cc",10,true,1));
        d.items.push_back(mkField(m+col+6,m+42,col-4,6,L"suppInsurance",L"",9,2));
        d.items.push_back(mkApptNo(m,d.paperH-24,cw,12,22));
        break; }
    case 8: { // T09 تزریقات استاندارد A5
        setPaper(L"A5",0); d.name=L"\u062a\u0632\u0631\u06cc\u0642\u0627\u062a \u0627\u0633\u062a\u0627\u0646\u062f\u0627\u0631\u062f A5";
        double m=10; double cw=d.paperW-2*m;
        d.items.push_back(mkLabel(m,m,cw,10,L"\u0628\u062e\u0634 \u062a\u0632\u0631\u06cc\u0642\u0627\u062a \u0648 \u067e\u0627\u0646\u0633\u0645\u0627\u0646",15,true,1));
        d.items.push_back(mkHLine(m,m+12,cw,0.5));
        addReceptionBody(d,m,m+16,cw,11);
        d.items.push_back(mkApptNo(m,d.paperH-24,cw,12,22));
        break; }
    case 9: { // T10 تزریقات با چک‌لیست دارو
        setPaper(L"A5",0); d.name=L"\u062a\u0632\u0631\u06cc\u0642\u0627\u062a \u0628\u0627 \u0686\u06a9\u200c\u0644\u06cc\u0633\u062a \u062f\u0627\u0631\u0648 (A5)";
        double m=10; double cw=d.paperW-2*m;
        d.items.push_back(mkLabel(m,m,cw,10,L"\u0628\u0631\u06af\u0647 \u062a\u0632\u0631\u06cc\u0642\u0627\u062a",15,true,1));
        d.items.push_back(mkHLine(m,m+12,cw,0.5));
        d.items.push_back(mkField(m,m+16,cw,7,L"firstName",FA_NAME,11,0));
        d.items.push_back(mkField(m,m+24,cw,7,L"lastName",FA_FAMILY,11,0));
        d.items.push_back(mkRect(m,m+34,cw,50,0.3,2));
        d.items.push_back(mkLabel(m+2,m+35,cw-4,6,L"\u0686\u06a9\u200c\u0644\u06cc\u0633\u062a \u062f\u0627\u0631\u0648\u0647\u0627",10,true,2));
        for(int i=0;i<5;i++)
            d.items.push_back(mkHLine(m+4,m+44+i*7,cw-8,0.2));
        break; }
    case 10: { // T11 تزریقات رسید 80mm
        setPaper(L"R80",0); d.name=L"\u062a\u0632\u0631\u06cc\u0642\u0627\u062a \u0631\u0633\u06cc\u062f \u06f8\u06f0mm";
        double m=4; double cw=d.paperW-2*m;
        d.items.push_back(mkLabel(m,m,cw,8,L"\u062a\u0632\u0631\u06cc\u0642\u0627\u062a",13,true,1));
        d.items.push_back(mkHLine(m,m+9,cw,0.3));
        addReceptionBody(d,m,m+11,cw,10);
        d.items.push_back(mkApptNo(m,d.paperH-22,cw,12,20));
        break; }
    case 11: { // T12 آزمایشگاه برچسب نمونه 50x30mm
        d.paper=L"custom"; d.orientation=0; d.paperW=50; d.paperH=30;
        d.name=L"\u0622\u0632\u0645\u0627\u06cc\u0634\u06af\u0627\u0647 \u2014 \u0628\u0631\u0686\u0633\u0628 \u0646\u0645\u0648\u0646\u0647 (50\u00d730mm)";
        double m=2; double cw=d.paperW-2*m;
        d.items.push_back(mkField(m,m,cw,5,L"lastName",L"",9,0));
        d.items.push_back(mkField(m,m+6,cw,5,L"firstName",L"",8,0));
        d.items.push_back(mkField(m,m+12,cw-12,5,L"nationalCode",L"",8,0));
        d.items.push_back(mkQR(d.paperW-m-10,m+10,10));
        d.items.push_back(mkField(m,m+22,cw,5,L"date",L"",7,0));
        break; }
    case 12: { // T13 آزمایشگاه رسید پذیرش A5
        setPaper(L"A5",0); d.name=L"\u0622\u0632\u0645\u0627\u06cc\u0634\u06af\u0627\u0647 \u2014 \u0631\u0633\u06cc\u062f \u067e\u0630\u06cc\u0631\u0634 A5";
        double m=10; double cw=d.paperW-2*m;
        d.items.push_back(mkLabel(m,m,cw,10,L"\u0622\u0632\u0645\u0627\u06cc\u0634\u06af\u0627\u0647 \u062a\u0634\u062e\u06cc\u0635 \u0637\u0628\u06cc",15,true,1));
        d.items.push_back(mkHLine(m,m+12,cw,0.5));
        addReceptionBody(d,m,m+16,cw,11);
        d.items.push_back(mkApptNo(m,d.paperH-24,cw,12,22));
        break; }
    case 13: { // T14 آزمایشگاه با QR و کد ملی
        setPaper(L"A5",0); d.name=L"\u0622\u0632\u0645\u0627\u06cc\u0634\u06af\u0627\u0647 \u2014 \u0631\u0633\u06cc\u062f \u0628\u0627 QR \u0648 \u06a9\u062f \u0645\u0644\u06cc";
        double m=10; double cw=d.paperW-2*m;
        d.items.push_back(mkLabel(m,m,cw,10,L"\u0622\u0632\u0645\u0627\u06cc\u0634\u06af\u0627\u0647",15,true,1));
        d.items.push_back(mkHLine(m,m+12,cw,0.5));
        d.items.push_back(mkQR(d.paperW-m-24,m+15,24));
        d.items.push_back(mkField(m,m+16,cw-28,7,L"nationalCode",FA_NID,11,0));
        addReceptionBody(d,m,m+44,cw,11);
        break; }
    case 14: { // T15 رادیولوژی برگه پذیرش A5
        setPaper(L"A5",0); d.name=L"\u0631\u0627\u062f\u06cc\u0648\u0644\u0648\u0698\u06cc \u2014 \u0628\u0631\u06af\u0647 \u067e\u0630\u06cc\u0631\u0634 A5";
        double m=10; double cw=d.paperW-2*m;
        d.items.push_back(mkLabel(m,m,cw,10,L"\u0628\u062e\u0634 \u0631\u0627\u062f\u06cc\u0648\u0644\u0648\u0698\u06cc",15,true,1));
        d.items.push_back(mkHLine(m,m+12,cw,0.5));
        addReceptionBody(d,m,m+16,cw,11);
        d.items.push_back(mkApptNo(m,d.paperH-24,cw,12,22));
        break; }
    case 15: { // T16 رادیولوژی رسید با عکس پرسنلی
        setPaper(L"A5",0); d.name=L"\u0631\u0627\u062f\u06cc\u0648\u0644\u0648\u0698\u06cc \u2014 \u0631\u0633\u06cc\u062f \u0628\u0627 \u0639\u06a9\u0633 \u067e\u0631\u0633\u0646\u0644\u06cc";
        double m=10; double cw=d.paperW-2*m;
        d.items.push_back(mkLabel(m,m,cw,10,L"\u0631\u0627\u062f\u06cc\u0648\u0644\u0648\u0698\u06cc",15,true,1));
        d.items.push_back(mkHLine(m,m+12,cw,0.5));
        d.items.push_back(mkPhoto(d.paperW-m-22,m+15,22,28));
        addReceptionBody(d,m,m+16,cw-26,11);
        break; }
    case 16: { // T17 فیزیوتراپی رسید جلسه
        setPaper(L"A5",0); d.name=L"\u0641\u06cc\u0632\u06cc\u0648\u062a\u0631\u0627\u067e\u06cc \u2014 \u0631\u0633\u06cc\u062f \u062c\u0644\u0633\u0647 (A5)";
        double m=10; double cw=d.paperW-2*m;
        d.items.push_back(mkLabel(m,m,cw,10,L"\u0641\u06cc\u0632\u06cc\u0648\u062a\u0631\u0627\u067e\u06cc",15,true,1));
        d.items.push_back(mkHLine(m,m+12,cw,0.5));
        addReceptionBody(d,m,m+16,cw,11);
        d.items.push_back(mkApptNo(m,d.paperH-24,cw,12,22));
        break; }
    case 17: { // T18 فیزیوتراپی کارت 10 جلسه
        d.paper=L"custom"; d.orientation=1; d.paperW=120; d.paperH=80;
        d.name=L"\u0641\u06cc\u0632\u06cc\u0648\u062a\u0631\u0627\u067e\u06cc \u2014 \u06a9\u0627\u0631\u062a \u06f1\u06f0 \u062c\u0644\u0633\u0647";
        double m=5; double cw=d.paperW-2*m;
        d.items.push_back(mkFrame(d.paperW,d.paperH,3,0x1F6FEB,0.6));
        d.items.push_back(mkLabel(m,m,cw,8,L"\u06a9\u0627\u0631\u062a \u06f1\u06f0 \u062c\u0644\u0633\u0647 \u0641\u06cc\u0632\u06cc\u0648\u062a\u0631\u0627\u067e\u06cc",13,true,1));
        d.items.push_back(mkField(m,m+10,cw,6,L"firstName",FA_NAME,10,0));
        for(int i=0;i<10;i++){
            double bx=m+ (i%5)*((cw)/5);
            double by=m+24+ (i/5)*16;
            d.items.push_back(mkRect(bx+1,by,((cw)/5)-2,12,0.3,1));
            wchar_t b[8]; swprintf(b,8,L"%d",i+1);
            d.items.push_back(mkLabel(bx+1,by+3,((cw)/5)-2,6,b,9,true,1));
        }
        break; }
    case 18: { // T19 لاکچری با حاشیه طلایی
        setPaper(L"A5",0); d.name=L"\u067e\u0630\u06cc\u0631\u0634 \u0644\u0627\u06a9\u0686\u0631\u06cc/\u06a9\u0644\u06cc\u0646\u06cc\u06a9 \u062e\u0635\u0648\u0635\u06cc (A5 \u0628\u0627 \u062d\u0627\u0634\u06cc\u0647 \u0637\u0644\u0627\u06cc\u06cc)";
        double m=12; double cw=d.paperW-2*m;
        d.items.push_back(mkFrame(d.paperW,d.paperH,8,0xC9A227,1.2));
        d.items.push_back(mkLabel(m,m,cw,12,FA_CLINIC,18,true,1));
        d.items.push_back(mkHLine(m,m+15,cw,0.6));
        addReceptionBody(d,m,m+20,cw,12);
        d.items.push_back(mkApptNo(m,d.paperH-26,cw,12,24));
        break; }
    case 19: { // T20 مدرن مینیمال
        setPaper(L"A5",0); d.name=L"\u067e\u0630\u06cc\u0631\u0634 \u0645\u062f\u0631\u0646 \u0645\u06cc\u0646\u06cc\u0645\u0627\u0644 (A5\u060c \u062a\u06a9 \u0631\u0646\u06af)";
        double m=12; double cw=d.paperW-2*m;
        d.items.push_back(mkLabel(m,m,cw,12,FA_CLINIC,17,true,2));
        d.items.push_back(mkHLine(m,m+16,cw,1.0));
        addReceptionBody(d,m,m+22,cw,12);
        d.items.push_back(mkApptNo(m,d.paperH-24,cw,12,22));
        break; }
    // ---------------------------------------------------------------- v1.44.0 §5
    // Ten new reception templates that showcase the professional TABLE element
    // and the new billing/insurance tokens. Total reception templates → 30.
    case 20: { // T21 قبض پذیرش با جدول خدمات A5
        setPaper(L"A5",0); d.name=L"\u0642\u0628\u0636 \u067e\u0630\u06cc\u0631\u0634 \u0628\u0627 \u062c\u062f\u0648\u0644 \u062e\u062f\u0645\u0627\u062a (A5)";
        double m=10; double cw=d.paperW-2*m;
        d.items.push_back(mkLabel(m,m,cw,10,FA_CLINIC,16,true,1));
        d.items.push_back(mkHLine(m,m+12,cw,0.5));
        d.items.push_back(mkField(m,m+15,cw/2,7,L"receiptNo",FA_RECEIPT,10,2));
        d.items.push_back(mkField(m+cw/2,m+15,cw/2,7,L"date",FA_DATE,10,0));
        d.items.push_back(mkField(m,m+23,cw,7,L"firstName",FA_NAME,10,0));
        d.items.push_back(mkField(m,m+31,cw,7,L"lastName",FA_FAMILY,10,0));
        d.items.push_back(mkTableJson(m,m+40,cw,44,servicesTableJson(),9));
        d.items.push_back(mkTableJson(m,d.paperH-52,cw,42,billSummaryTableJson(),9));
        break; }
    case 21: { // T22 صورتحساب کامل بیمه پایه + تکمیلی A5
        setPaper(L"A5",0); d.name=L"\u0635\u0648\u0631\u062a\u062d\u0633\u0627\u0628 \u06a9\u0627\u0645\u0644 \u0628\u06cc\u0645\u0647 \u067e\u0627\u06cc\u0647 + \u062a\u06a9\u0645\u06cc\u0644\u06cc (A5)";
        double m=10; double cw=d.paperW-2*m;
        d.items.push_back(mkLabel(m,m,cw,10,FA_CLINIC,15,true,1));
        d.items.push_back(mkHLine(m,m+12,cw,0.5));
        d.items.push_back(mkField(m,m+15,cw,7,L"{{patient.fullname}}",L"\u0628\u06cc\u0645\u0627\u0631: ",10,0));
        d.items.push_back(mkLabel(m,m+24,cw/2,6,L"\u0628\u06cc\u0645\u0647 \u067e\u0627\u06cc\u0647: {{ins.base.name}} ({{ins.base.pct}})",9,false,0));
        d.items.push_back(mkLabel(m+cw/2,m+24,cw/2,6,L"\u062a\u06a9\u0645\u06cc\u0644\u06cc: {{ins.supp.name}} ({{ins.supp.pct}})",9,false,0));
        d.items.push_back(mkTableJson(m,m+34,cw,50,servicesTableJson(),9));
        d.items.push_back(mkTableJson(m,d.paperH-50,cw,40,billSummaryTableJson(),9));
        break; }
    case 22: { // T23 قبض حرارتی 80mm با جدول خدمات
        setPaper(L"R80",0); d.name=L"\u0642\u0628\u0636 \u062d\u0631\u0627\u0631\u062a\u06cc \u06f8\u06f0mm \u0628\u0627 \u062c\u062f\u0648\u0644 \u062e\u062f\u0645\u0627\u062a";
        double m=4; double cw=d.paperW-2*m;
        d.items.push_back(mkLabel(m,m,cw,8,FA_CLINIC,12,true,1));
        d.items.push_back(mkHLine(m,m+9,cw,0.3));
        d.items.push_back(mkField(m,m+11,cw,6,L"{{patient.fullname}}",L"",10,1));
        d.items.push_back(mkTableJson(m,m+20,cw,40,servicesTableJson(),8));
        d.items.push_back(mkField(m,d.paperH-24,cw,6,L"{{bill.pat}}",FA_PAID,10,0));
        break; }
    case 23: { // T24 قبض پذیرش با QR و جدول خدمات
        setPaper(L"A5",0); d.name=L"\u0642\u0628\u0636 \u067e\u0630\u06cc\u0631\u0634 \u0628\u0627 QR \u0648 \u062c\u062f\u0648\u0644 \u062e\u062f\u0645\u0627\u062a";
        double m=10; double cw=d.paperW-2*m;
        d.items.push_back(mkLabel(m,m,cw,10,FA_CLINIC,16,true,1));
        d.items.push_back(mkHLine(m,m+12,cw,0.5));
        d.items.push_back(mkQR(m,m+15,22));
        d.items.push_back(mkField(m+26,m+16,cw-26,7,L"{{appt.no}}",FA_APPT L": ",11,0));
        d.items.push_back(mkField(m+26,m+24,cw-26,7,L"{{appt.date}}",FA_DATE,11,0));
        d.items.push_back(mkTableJson(m,m+42,cw,42,servicesTableJson(),9));
        d.items.push_back(mkTableJson(m,d.paperH-46,cw,38,billSummaryTableJson(),9));
        break; }
    case 24: { // T25 برگه پذیرش لوکس با حاشیه طلایی و جدول
        setPaper(L"A5",0); d.name=L"\u0628\u0631\u06af\u0647 \u067e\u0630\u06cc\u0631\u0634 \u0644\u0648\u06a9\u0633 \u0628\u0627 \u062c\u062f\u0648\u0644 \u0635\u0648\u0631\u062a\u062d\u0633\u0627\u0628";
        double m=12; double cw=d.paperW-2*m;
        d.items.push_back(mkFrame(d.paperW,d.paperH,8,0xC9A227,1.2));
        d.items.push_back(mkLabel(m,m,cw,12,FA_CLINIC,18,true,1));
        d.items.push_back(mkHLine(m,m+15,cw,0.6));
        d.items.push_back(mkField(m,m+18,cw,7,L"{{patient.fullname}}",L"\u0628\u06cc\u0645\u0627\u0631: ",11,0));
        d.items.push_back(mkTableJson(m,m+28,cw,46,servicesTableJson(),9));
        d.items.push_back(mkTableJson(m,d.paperH-52,cw,40,billSummaryTableJson(),9));
        break; }
    case 25: { // T26 پذیرش A4 با جدول خدمات بزرگ
        setPaper(L"A4",0); d.name=L"\u067e\u0630\u06cc\u0631\u0634 A4 \u0628\u0627 \u062c\u062f\u0648\u0644 \u062e\u062f\u0645\u0627\u062a \u0628\u0632\u0631\u06af";
        double m=15; double cw=d.paperW-2*m;
        d.items.push_back(mkLabel(m,m,cw,12,FA_CLINIC,18,true,1));
        d.items.push_back(mkHLine(m,m+15,cw,0.6));
        d.items.push_back(mkField(m,m+18,cw/2,7,L"{{patient.fullname}}",L"\u0628\u06cc\u0645\u0627\u0631: ",11,0));
        d.items.push_back(mkField(m+cw/2,m+18,cw/2,7,L"{{patient.nid}}",FA_NID,11,0));
        d.items.push_back(mkTableJson(m,m+30,cw,90,servicesTableJson(),10));
        d.items.push_back(mkTableJson(m,d.paperH-60,cw,45,billSummaryTableJson(),10));
        break; }
    case 26: { // T27 پذیرش دو زبانه شماره نوبت بزرگ + جدول
        setPaper(L"A5",1); d.name=L"\u067e\u0630\u06cc\u0631\u0634 \u0627\u0641\u0642\u06cc \u2014 \u0634\u0645\u0627\u0631\u0647 \u0646\u0648\u0628\u062a \u0628\u0632\u0631\u06af + \u062c\u062f\u0648\u0644";
        double m=10; double cw=d.paperW-2*m;
        d.items.push_back(mkLabel(m,m,cw,9,FA_CLINIC,15,true,1));
        d.items.push_back(mkHLine(m,m+11,cw,0.5));
        d.items.push_back(mkApptNo(d.paperW-m-46,m+14,46,16,28));
        d.items.push_back(mkLabel(d.paperW-m-46,m+32,46,6,FA_APPT,9,false,1));
        d.items.push_back(mkTableJson(m,m+14,cw-52,50,servicesTableJson(),9));
        break; }
    case 27: { // T28 قبض جمع‌وجور A6 با سهم بیمار
        setPaper(L"A6",0); d.name=L"\u0642\u0628\u0636 \u062c\u0645\u0639\u200c\u0648\u062c\u0648\u0631 A6 \u0628\u0627 \u0633\u0647\u0645 \u0628\u06cc\u0645\u0627\u0631";
        double m=6; double cw=d.paperW-2*m;
        d.items.push_back(mkLabel(m,m,cw,8,FA_CLINIC,12,true,1));
        d.items.push_back(mkHLine(m,m+9,cw,0.4));
        d.items.push_back(mkField(m,m+11,cw,6,L"{{patient.fullname}}",L"",10,1));
        d.items.push_back(mkTableJson(m,m+19,cw,34,billSummaryTableJson(),8));
        break; }
    case 28: { // T29 پذیرش با خلاصهٔ بیمه (بدون جدول خدمات)
        setPaper(L"A5",0); d.name=L"\u067e\u0630\u06cc\u0631\u0634 \u0628\u0627 \u062e\u0644\u0627\u0635\u0647\u0654 \u0628\u06cc\u0645\u0647 (A5)";
        double m=10; double cw=d.paperW-2*m;
        d.items.push_back(mkLabel(m,m,cw,10,FA_CLINIC,16,true,1));
        d.items.push_back(mkHLine(m,m+12,cw,0.5));
        d.items.push_back(mkField(m,m+15,cw,7,L"{{patient.fullname}}",L"\u0628\u06cc\u0645\u0627\u0631: ",11,0));
        d.items.push_back(mkField(m,m+24,cw,6,L"{{ins.base.name}} ({{ins.base.pct}})",L"\u0628\u06cc\u0645\u0647 \u067e\u0627\u06cc\u0647: ",10,0));
        d.items.push_back(mkTableJson(m,m+34,cw,50,billSummaryTableJson(),10));
        d.items.push_back(mkApptNo(m,d.paperH-24,cw,12,22));
        break; }
    case 29: default: { // T30 صورتحساب حرفه‌ای کامل A5 (جدول خدمات + خلاصهٔ بیمه)
        setPaper(L"A5",0); d.name=L"\u0635\u0648\u0631\u062a\u062d\u0633\u0627\u0628 \u062d\u0631\u0641\u0647\u200c\u0627\u06cc \u06a9\u0627\u0645\u0644 (A5)";
        double m=10; double cw=d.paperW-2*m;
        d.items.push_back(mkLabel(m,m,cw,10,FA_CLINIC,16,true,1));
        d.items.push_back(mkLabel(m,m+11,cw,6,L"\u0635\u0648\u0631\u062a\u062d\u0633\u0627\u0628 \u067e\u0630\u06cc\u0631\u0634",11,false,1));
        d.items.push_back(mkHLine(m,m+18,cw,0.5));
        d.items.push_back(mkField(m,m+21,cw/2,6,L"{{patient.fullname}}",L"\u0628\u06cc\u0645\u0627\u0631: ",10,0));
        d.items.push_back(mkField(m+cw/2,m+21,cw/2,6,L"{{appt.no}}",FA_APPT L": ",10,0));
        d.items.push_back(mkTableJson(m,m+30,cw,48,servicesTableJson(),9));
        d.items.push_back(mkTableJson(m,d.paperH-52,cw,42,billSummaryTableJson(),9));
        break; }
    }
    return d;
}

// v1.44.0 §5: explicit reception-template registry. Exactly 30 entries — the
// static_assert below is the contractual guarantee the work-order requires.
struct ReceptionTemplateInfo {
    int          idx;          // index passed to buildTemplate()
    const char*  section;      // owning section — all are "reception" here
    const wchar_t* code;       // stable T-code
};
static const ReceptionTemplateInfo RECEPTION_TEMPLATES[] = {
    { 0,"reception",L"T01"}, { 1,"reception",L"T02"}, { 2,"reception",L"T03"},
    { 3,"reception",L"T04"}, { 4,"reception",L"T05"}, { 5,"reception",L"T06"},
    { 6,"reception",L"T07"}, { 7,"reception",L"T08"}, { 8,"reception",L"T09"},
    { 9,"reception",L"T10"}, {10,"reception",L"T11"}, {11,"reception",L"T12"},
    {12,"reception",L"T13"}, {13,"reception",L"T14"}, {14,"reception",L"T15"},
    {15,"reception",L"T16"}, {16,"reception",L"T17"}, {17,"reception",L"T18"},
    {18,"reception",L"T19"}, {19,"reception",L"T20"}, {20,"reception",L"T21"},
    {21,"reception",L"T22"}, {22,"reception",L"T23"}, {23,"reception",L"T24"},
    {24,"reception",L"T25"}, {25,"reception",L"T26"}, {26,"reception",L"T27"},
    {27,"reception",L"T28"}, {28,"reception",L"T29"}, {29,"reception",L"T30"},
};
static const int RECEPTION_TEMPLATES_COUNT =
    (int)(sizeof(RECEPTION_TEMPLATES)/sizeof(RECEPTION_TEMPLATES[0]));
static_assert(sizeof(RECEPTION_TEMPLATES)/sizeof(RECEPTION_TEMPLATES[0]) == 30,
    "v1.44.0 \u00a75: RECEPTION_TEMPLATES must contain exactly 30 templates");

} // namespace

// ----------------------------------------------------------------- seeding ---
void Designs_Init(){
    std::vector<PrintDesign> existing; Designs_Builtins(existing);
    // v1.44.0 §5: 30 built-in reception templates (was 20). Re-seed the 10 new
    // ones for installs that already had the original 20.
    if((int)existing.size() >= RECEPTION_TEMPLATES_COUNT) return;   // already fully seeded
    int have = (int)existing.size();
    for(int i=have;i<RECEPTION_TEMPLATES_COUNT;i++){
        PrintDesign d = buildTemplate(i);
        Designs_Insert(d);
    }
}
