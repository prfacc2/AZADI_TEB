// ============================================================================
//  print_designer_templates.inc — 30 built-in A4 reception templates
//  (v1.60.0 — FULL REBUILD around the LIVE SERVICES TABLE)
//  Included into print_designer.cpp. Every template is authored in A4 mm
//  coordinates (210×297) and EVERY template carries the dynamic services
//  table (PIT_SERVICES) bound to the live ReceptionRecord.services vector —
//  نام خدمت / شرح خدمت / تعداد، per-row, straight from Management → خدمات.
//  If the operator switches the paper size (A5/A6/receipt) the print engine
//  auto-scales the whole design proportionally (pscale), so nothing is
//  clipped. Seeded on first run; existing installs are migrated once.
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
static PrintItem mkVLine(double x,double y,double h,double bw){
    PrintItem it; it.id=nid(); it.type=PIT_VLINE; it.x=x; it.y=y; it.w=0.2; it.h=h;
    it.borderWidth=bw; return it;
}
static PrintItem mkRect(double x,double y,double w,double h,double bw,double corner){
    PrintItem it; it.id=nid(); it.type=PIT_RECT; it.x=x; it.y=y; it.w=w; it.h=h;
    it.borderWidth=bw; it.corner=corner; it.fillTransparent=true; return it;
}
static PrintItem mkBand(double x,double y,double w,double h,unsigned fill,double bw){
    PrintItem it; it.id=nid(); it.type=PIT_RECT; it.x=x; it.y=y; it.w=w; it.h=h;
    it.fillColor=fill; it.fillTransparent=false; it.borderWidth=bw;
    it.borderColor=fill; return it;
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
static PrintItem mkApptNo(double x,double y,double w,double h,double pt){
    PrintItem it; it.id=nid(); it.type=PIT_APPTNO; it.x=x; it.y=y; it.w=w; it.h=h;
    it.fontPt=pt; it.bold=true; it.align=1; it.startValue=1; it.step=1; return it;
}

// ---------------------------------------------------------------------------
//  THE CORE: dynamic services table CONNECTED to the live reception record.
//  The model JSON describes the header (cols/labels/widths); at print time
//  printer.cpp::pdDrawServices fills ONE data row per entered service from
//  ReceptionRecord.services — real نام/شرح/تعداد، never test text.
//  Columns (RTL, col0 = RIGHTMOST):
//      col 0 = نام خدمت  (ServiceLine.name)
//      col 1 = شرح خدمت  (ServiceLine.desc / category)
//      col 2 = تعداد     (ServiceLine.qty)
//  headerH + rowH are pinned (mm) so ~10 services print cleanly; the table
//  auto-shrinks uniformly if more rows must fit its box.
// ---------------------------------------------------------------------------
static PrintItem mkServices(double x,double y,double w,double h,double pt,
                            bool banded,double bw){
    PrintItem it; it.id=nid(); it.type=PIT_SERVICES; it.x=x; it.y=y; it.w=w; it.h=h;
    // نام خدمت=نام خدمت  شرح خدمت=شرح خدمت  تعداد=تعداد
    it.text=L"{\"cols\":3,\"header\":true,\"widths\":[0.55,0.30,0.15],"
            L"\"labels\":[\"\u0646\u0627\u0645 \u062e\u062f\u0645\u062a\","
            L"\"\u0634\u0631\u062d \u062e\u062f\u0645\u062a\","
            L"\"\u062a\u0639\u062f\u0627\u062f\"]}";
    it.fontPt=pt; it.bold=false; it.align=1;
    it.borderColor=0x000000; it.borderWidth=bw;
    it.textColor=0x000000;
    it.fillColor=banded? 0xE9E9E9 : 0xFFFFFF;
    it.fillTransparent=!banded;
    it.padding=0.8;
    it.headerH=7.5; it.rowH=7.0;   // pinned heights → room for many services
    return it;
}

// localized captions reused across templates
#define FA_CLINIC   L"درمانگاه شبانه‌روزی آزادی طب"
#define FA_SVCLIST  L"لیست خدمات"
#define FA_NAME     L"نام: "
#define FA_FAMILY   L"نام خانوادگی: "
#define FA_FULL     L"نام بیمار: "
#define FA_NID      L"کد ملی: "
#define FA_DATE     L"تاریخ: "
#define FA_TIME     L"ساعت: "
#define FA_RECEIPT  L"شماره قبض: "
#define FA_APPT     L"شماره نوبت"
#define FA_TOTAL    L"جمع کل: "
#define FA_PAID     L"پرداختی: "
#define FA_PATIENT  L"سهم بیمار: "
#define FA_INS      L"بیمه: "
#define FA_DOCTOR   L"پزشک: "
#define FA_PHONE    L"تلفن: "
#define FA_SHIFT    L"شیفت: "
#define FA_BARCODE_CAP L"بارکد رسید"

// ===========================================================================
//  BLOCK BUILDERS — each appends to d.items and returns the next free y.
//  Every body block ends in the services table, the centrepiece of the page.
// ===========================================================================

// Header: clinic title + optional logo + rule. Returns y below the header.
static double addHeader(PrintDesign& d,double m,double cw,int style){
    double y=m;
    if(style==1){           // logo at top-right, title beside it
        d.items.push_back(mkLogo(m+cw-20,m,20,20));
        d.items.push_back(mkLabel(m,m+3,cw-24,12,FA_CLINIC,17,true,0));
        y=m+24;
    } else if(style==2){    // centred logo above centred title
        d.items.push_back(mkLogo((d.paperW-22)/2,m,22,22));
        d.items.push_back(mkLabel(m,m+24,cw,12,FA_CLINIC,18,true,1));
        y=m+40;
    } else {                // plain centred title
        d.items.push_back(mkLabel(m,m,cw,12,FA_CLINIC,18,true,1));
        y=m+16;
    }
    d.items.push_back(mkHLine(m,y,cw,0.6));
    return y+4;
}

// Receipt/date/shift strip. Returns y below it.
static double addMetaStrip(PrintDesign& d,double m,double y,double cw,double pt){
    d.items.push_back(mkField(m,y,cw/3,7,L"receiptNo",FA_RECEIPT,pt,2));
    d.items.push_back(mkField(m+cw/3,y,cw/3,7,L"date",FA_DATE,pt,1));
    d.items.push_back(mkField(m+2*cw/3,y,cw/3,7,L"shift",FA_SHIFT,pt,0));
    return y+9;
}

// Patient info block (2-col RTL grid). Returns y below it.
static double addPatient(PrintDesign& d,double m,double y,double cw,double pt){
    double col=cw/2-3;
    d.items.push_back(mkField(m+col+6,y,col,7.5,L"firstName",FA_NAME,pt,0));
    d.items.push_back(mkField(m,y,col,7.5,L"lastName",FA_FAMILY,pt,0)); y+=8.5;
    d.items.push_back(mkField(m+col+6,y,col,7.5,L"nationalCode",FA_NID,pt,0));
    d.items.push_back(mkField(m,y,col,7.5,L"insurance",FA_INS,pt,0)); y+=8.5;
    d.items.push_back(mkField(m+col+6,y,col,7.5,L"doctor",FA_DOCTOR,pt,0));
    d.items.push_back(mkField(m,y,col,7.5,L"mobile",FA_PHONE,pt,0)); y+=8.5;
    return y+2;
}

// Services block: caption + live table. THE section the whole redesign is for.
// Returns y below it. svcH is the table box height (mm).
static double addServices(PrintDesign& d,double m,double y,double cw,double pt,
                          double svcH,bool banded,double bw){
    d.items.push_back(mkLabel(m,y,cw*0.6,6.5,FA_SVCLIST,pt+1,true,0));
    d.items.push_back(mkField(m+cw*0.6,y,cw*0.4,6.5,L"servicescount",
        L"تعداد ردیف: ",pt-0.5,2));
    y+=8;
    d.items.push_back(mkServices(m,y,cw,svcH,pt-0.5,banded,bw));
    return y+svcH+4;
}

// Totals row: جمع کل / سهم بیمار / پرداختی. Returns y below it.
static double addTotals(PrintDesign& d,double m,double y,double cw,double pt){
    d.items.push_back(mkHLine(m,y,cw,0.3)); y+=2.5;
    d.items.push_back(mkField(m+2*cw/3,y,cw/3,8,L"total",FA_TOTAL,pt+0.5,0));
    d.items.push_back(mkField(m+cw/3,y,cw/3,8,L"patientshare",FA_PATIENT,pt+0.5,1));
    d.items.push_back(mkField(m,y,cw/3,8,L"paid",FA_PAID,pt+0.5,2));
    return y+10;
}

// Footer: rule + barcode on the RIGHT + optional appointment number on LEFT.
static void addFooter(PrintDesign& d,double m,double cw,double paperH,bool withAppt){
    double y = paperH - 32;
    d.items.push_back(mkHLine(m,y-3,cw,0.3));
    double bcW = cw*0.42, bcH = 16;
    double bcX = m + cw - bcW;
    d.items.push_back(mkBarcode(bcX,y,bcW,bcH));
    d.items.push_back(mkLabel(bcX,y+bcH+0.5,bcW,4,FA_BARCODE_CAP,8,false,1));
    if(withAppt){
        d.items.push_back(mkApptNo(m,y-1,cw*0.4,12,22));
        d.items.push_back(mkLabel(m,y+13,cw*0.4,5,FA_APPT,9,false,1));
    }
}

// ===========================================================================
//  buildTemplate — 30 DISTINCT layouts. `style` picks a header style, `svcH`
//  the services-table height, `banded` the table shading, `extra` decorations.
//  Every single case places the services table via addServices().
// ===========================================================================
static PrintDesign buildTemplate(int idx){
    g_tplItemId = 1;
    PrintDesign d; d.kind=L"builtin";
    d.paper=L"A4"; d.orientation=0; d.paperW=210; d.paperH=297;
    double m=14, cw=d.paperW-2*m;   // 182mm content width
    //            header, meta, svcH, banded, bw, frameCol(0=none), withAppt, photo, qr
    struct Spec { int header; double svcH; bool banded; double bw;
                  unsigned frame; bool appt; bool photo; bool qr; };
    static const Spec SP[30] = {
        {0, 96, true ,0.40,0        ,true ,false,false}, // 01 standard
        {1, 92, true ,0.40,0        ,true ,false,false}, // 02 logo right
        {2, 84, false,0.30,0        ,true ,false,false}, // 03 centred logo
        {0, 90, false,0.30,0x1F6FEB ,true ,false,false}, // 04 blue frame
        {1, 78, true ,0.40,0        ,true ,true ,false}, // 05 patient photo
        {0, 88, false,0.35,0        ,false,false,true }, // 06 QR corner
        {2, 72, true ,0.45,0x0B5ED7 ,true ,false,false}, // 07 logo+frame
        {0,100, true ,0.50,0        ,true ,false,false}, // 08 tall table
        {1, 86, false,0.30,0        ,false,false,false}, // 09 clean lines
        {0, 82, true ,0.40,0x6A0DAD ,true ,false,false}, // 10 violet frame
        {1, 90, true ,0.35,0        ,true ,false,false}, // 11 logo light band
        {0, 74, false,0.30,0        ,true ,true ,false}, // 12 photo+lines
        {2, 80, true ,0.40,0        ,false,false,false}, // 13 centred, no appt
        {0, 94, false,0.45,0        ,true ,false,false}, // 14 bold lines
        {1, 68, true ,0.50,0x0B5ED7 ,true ,true ,false}, // 15 compact photo
        {0, 98, true ,0.40,0        ,true ,false,false}, // 16 tall banded
        {2, 76, false,0.35,0        ,true ,false,true }, // 17 centred QR
        {1, 88, false,0.30,0x1F6FEB ,false,false,false}, // 18 logo blue frame
        {0, 84, true ,0.45,0        ,true ,false,false}, // 19 balanced
        {1, 92, false,0.35,0        ,true ,false,false}, // 20 logo lines
        {0, 70, true ,0.40,0        ,true ,true ,true }, // 21 photo+QR
        {2, 90, true ,0.50,0x6A0DAD ,true ,false,false}, // 22 centred framed
        {1, 78, false,0.30,0        ,false,false,false}, // 23 minimal
        {0, 96, true ,0.40,0        ,true ,false,false}, // 24 standard banded
        {1, 82, true ,0.45,0x0B5ED7 ,true ,false,false}, // 25 logo framed
        {0, 86, false,0.35,0        ,true ,false,false}, // 26 clean
        {2, 74, true ,0.40,0        ,true ,true ,false}, // 27 centred photo
        {1, 90, false,0.30,0x1F6FEB ,true ,false,false}, // 28 logo blue
        {0, 94, true ,0.45,0        ,true ,false,false}, // 29 bold banded
        {1, 88, true ,0.40,0x6A0DAD ,true ,false,false}, // 30 logo violet
    };
    const Spec& sp=SP[idx];
    double pt=11.0;
    if(sp.frame) d.items.push_back(mkFrame(d.paperW,d.paperH,9,sp.frame,0.9));
    double y=addHeader(d,m,cw,sp.header);
    y=addMetaStrip(d,m,y,cw,pt-0.5);
    if(sp.photo){ d.items.push_back(mkPhoto(d.paperW-m-24,y,24,30)); }
    if(sp.qr){    d.items.push_back(mkQR(d.paperW-m-24,y,24)); }
    y=addPatient(d,m,y,cw,pt);
    y=addServices(d,m,y,cw,pt,sp.svcH,sp.banded,sp.bw);
    y=addTotals(d,m,y,cw,pt);
    addFooter(d,m,cw,d.paperH,sp.appt);
    return d;
}

} // namespace

// Resolve just the NAME of template idx (migration matches by name).
static std::wstring buildTemplateName(int idx){
    static const wchar_t* NAMES[30]={
        L"۰۱) رسید پذیرش — استاندارد",
        L"۰۲) رسید پذیرش — لوگو کنار عنوان",
        L"۰۳) رسید پذیرش — لوگوی وسط",
        L"۰۴) رسید پذیرش — قاب آبی",
        L"۰۵) رسید پذیرش — عکس بیمار",
        L"۰۶) رسید پذیرش — QR گوشه",
        L"۰۷) رسید پذیرش — لوگو + قاب",
        L"۰۸) رسید پذیرش — جدول بلند",
        L"۰۹) رسید پذیرش — خط‌کشی ساده",
        L"۱۰) رسید پذیرش — قاب بنفش",
        L"۱۱) رسید پذیرش — لوگو باند روشن",
        L"۱۲) رسید پذیرش — عکس + خط",
        L"۱۳) رسید پذیرش — وسط‌چین",
        L"۱۴) رسید پذیرش — خط ضخیم",
        L"۱۵) رسید پذیرش — فشرده عکس‌دار",
        L"۱۶) رسید پذیرش — جدول باند بلند",
        L"۱۷) رسید پذیرش — وسط + QR",
        L"۱۸) رسید پذیرش — قاب آبی لوگو",
        L"۱۹) رسید پذیرش — متعادل",
        L"۲۰) رسید پذیرش — لوگو خطی",
        L"۲۱) رسید پذیرش — عکس + QR",
        L"۲۲) رسید پذیرش — وسط قاب بنفش",
        L"۲۳) رسید پذیرش — مینیمال",
        L"۲۴) رسید پذیرش — استاندارد باند",
        L"۲۵) رسید پذیرش — لوگو قاب‌دار",
        L"۲۶) رسید پذیرش — تمیز",
        L"۲۷) رسید پذیرش — وسط عکس‌دار",
        L"۲۸) رسید پذیرش — لوگو آبی",
        L"۲۹) رسید پذیرش — باند ضخیم",
        L"۳۰) رسید پذیرش — لوگو بنفش",
    };
    if(idx<0||idx>=30) return L"";
    return NAMES[idx];
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
    if(existing.empty()){
        for(int i=0;i<N;i++){ PrintDesign d=buildTemplate(i); Designs_Insert(d); }
        setSetting(L"tpl_migration_1_60", L"1");
        setSetting(L"tpl_migration_1_59", L"1");
        setSetting(L"tpl_migration_1_58", L"1");
        setSetting(L"tpl_migration_1_53", L"1");
        setSetting(L"tpl_migration_1_52", L"1");
        return;
    }
    if(getSetting(L"tpl_migration_1_60", L"0") != L"1"){
        // Overwrite the first N stored builtins in order (preserving their
        // ids/bindings), insert any missing, DELETE surplus beyond N.
        for(int i=0;i<N;i++){
            PrintDesign fresh=buildTemplate(i);
            if(i<(int)existing.size()){ fresh.id=existing[i].id; Designs_Update(fresh); }
            else Designs_Insert(fresh);
        }
        for(size_t i=N;i<existing.size();++i) Designs_Delete(existing[i].id);
        setSetting(L"tpl_migration_1_60", L"1");
        setSetting(L"tpl_migration_1_59", L"1");
        setSetting(L"tpl_migration_1_58", L"1");
        setSetting(L"tpl_migration_1_53", L"1");
        setSetting(L"tpl_migration_1_52", L"1");
    }
}
