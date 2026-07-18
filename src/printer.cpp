// ============================================================================
//  printer.cpp — v1.4.0 print system
//   • Print SECTIONS (each prints differently): پذیرش / تزریقات / آزمایشگاه …
//   • A printer-SETTINGS dialog: default printer (persisted independently of the
//     Windows default), test connection, A4/A5 paper, fit/fill, advanced.
//   • A full visual print DESIGNER (in printer_designer.inc): draggable
//     labels / lines / borders / logo, field bindings, per-item font & style,
//     templates, backup/restore, zoom, snapping, "چاپ توسط پذیرش".
//   • printDesignedReceipt(): renders a saved design onto a real printer DC.
//
//  Everything is file-backed (data\design_<sec>.txt + settings.ini) so the EXE
//  stays single & static.
// ============================================================================
#include "app.h"
#include "print_designer.h"   // §3: new vector designer (PrintDesigner_Open)
#include <stdio.h>

// ----------------------------------------------------------------------------
//  Record a "settings-change request" when a RECEPTION user (role 0) alters a
//  printer / design setting, so management sees who/what/when on the red-badge
//  panel. Manager/admin (role 1/2) edits are applied silently (they own it).
// ----------------------------------------------------------------------------
static std::wstring currentSystemName(){
    wchar_t buf[256]={0}; DWORD n=255;
    if(GetComputerNameW(buf,&n) && n>0) return std::wstring(buf,n);
    return L"—";
}
static void logSettingsChange(const std::wstring& change){
    if(g_session.user.username.empty()) return;
    if(g_session.user.role!=0) return;   // only reception edits are "requests"
    std::wstring who = g_session.user.fullname.empty()
                     ? g_session.user.username : g_session.user.fullname;
    std::wstring prof = g_session.user.dept.empty() ? L"پذیرش" : g_session.user.dept;
    pushSetReq(who, currentSystemName(), change, prof);
}
// v1.9.0: printer settings change gate. Managers (role>=1) apply directly; for
// reception/staff the change is NOT applied — it is confirmed, then queued for
// management approval with a key=value payload + a preview string, exactly like
// the settings.cpp workflow. Returns true if the caller may apply directly.
static bool printerRequestGate(HWND h, const std::wstring& title,
                               const std::wstring& change,
                               const std::wstring& payload,
                               const std::wstring& preview){
    if(g_session.user.username.empty()) return true;   // not signed in → local
    if(g_session.user.role>=1) return true;            // manager/admin → direct
    if(MessageBoxW(h,L"آیا از ذخیرهٔ این تنظیمات اطمینان دارید؟",
        L"تأیید ذخیره", MB_ICONQUESTION|MB_YESNO|MB_DEFBUTTON2)!=IDYES)
        return false;
    std::wstring who = g_session.user.fullname.empty()
                     ? g_session.user.username : g_session.user.fullname;
    pushSetReqEx(who, systemSourceName(), title, change, payload, preview);
    MessageBoxW(h,L"این تنظیمات برای مدیریت ارسال شد. پس از تأیید اعمال خواهد شد.",
        L"ارسال برای تأیید", MB_OK|MB_ICONINFORMATION);
    return false;
}

// ------------------------------------------------------------- sections -----
const wchar_t* PRINT_SECTIONS[] = {
    L"پذیرش درمانگاه",
    L"نوبت‌دهی",
    L"قبض / صورتحساب",
    L"بیمه",
    L"بیمه مکمل",
    L"مبلغ نهایی",
    L"نسخه پزشک",
    L"تزریقات",
    L"آزمایشگاه",
    L"داروخانه",
    L"رادیولوژی",
};
const int N_PRINT_SECTIONS = sizeof(PRINT_SECTIONS)/sizeof(PRINT_SECTIONS[0]);

// ----------------------------------------------------- bindable fields ------
//  token  →  shown label (Persian).  Tokens are written {first} etc.
struct FieldDef { const wchar_t* token; const wchar_t* label; };
static const FieldDef FIELDS[] = {
    { L"{first}",     L"نام بیمار" },
    { L"{last}",      L"نام خانوادگی" },
    { L"{full}",      L"نام و نام خانوادگی" },
    { L"{father}",    L"نام پدر" },
    { L"{nid}",       L"کد ملی" },
    { L"{birth}",     L"تاریخ تولد" },
    { L"{gender}",    L"جنسیت" },
    { L"{mobile}",    L"تلفن همراه" },
    { L"{landline}",  L"تلفن ثابت" },
    { L"{address}",   L"آدرس" },
    { L"{ptype}",     L"نوع بیمار" },
    { L"{ins}",       L"بیمه اصلی" },
    { L"{supp}",      L"بیمه مکمل" },
    { L"{queue}",     L"شماره نوبت" },
    { L"{date}",      L"تاریخ" },
    { L"{time}",      L"ساعت" },
    { L"{shift}",     L"شیفت" },
    { L"{dept}",      L"بخش" },
    { L"{user}",      L"کاربر پذیرش" },
    { L"{total}",     L"جمع کل" },
    { L"{discount}",  L"تخفیف" },
    { L"{paid}",      L"مبلغ پرداختی" },
    { L"{issued}",    L"چاپ توسط پذیرش" },
};
static const int N_FIELDS = sizeof(FIELDS)/sizeof(FIELDS[0]);

// ------------------------------------------------------------ design model --
enum ItemKind { IT_LABEL=0, IT_LINE_H, IT_LINE_V, IT_BORDER, IT_LOGO };

struct DItem {
    int  kind;          // ItemKind
    double x, y, w, h;  // millimetres on the page
    std::wstring field; // bound token, or empty for free text
    std::wstring text;  // literal text (label) or caption prefix
    int  fontSize;      // pt
    bool bold, italic, underline, strike;
    std::wstring fontName;
    COLORREF color;
    double lineW;       // mm thickness for lines / borders
    int  lineStyle;     // 0 solid, 1 dashed, 2 dotted
    COLORREF borderColor;
    std::wstring logoPath;
    COLORREF bgColor;   // label background fill; CLR_INVALID = transparent
    int  align;         // 0 right, 1 center, 2 left (for labels)
    DItem():kind(IT_LABEL),x(10),y(10),w(50),h(8),
        fontSize(11),bold(false),italic(false),underline(false),strike(false),
        fontName(L"Vazirmatn"),color(RGB(0,0,0)),lineW(0.3),lineStyle(0),
        borderColor(RGB(0,0,0)),bgColor(CLR_INVALID),align(0){}
};

struct Design {
    int paper;          // 0 = A4, 1 = A5
    std::wstring name;  // template/backup display name
    std::vector<DItem> items;
    Design():paper(0),name(L""){}
};

static void paperMM(int paper, double& w, double& h){
    if(paper==1){ w=148.0; h=210.0; }     // A5 portrait
    else        { w=210.0; h=297.0; }     // A4 portrait
}

// --------------------------------------------------------- serialization ----
//  data\design_<sec>.txt  — line 0: "paper\tname"; then one item per line,
//  tab-delimited fields. Logo paths & text are pipe-escaped of tabs/newlines.
static std::wstring escTab(const std::wstring& s){
    std::wstring o=s; for(auto&c:o) if(c==L'\t'||c==L'\n'||c==L'\r') c=L' '; return o;
}
static std::wstring designPath(int sec){
    wchar_t b[32]; swprintf(b,32,L"\\design_%d.txt",sec);
    return dataDir()+b;
}
static std::wstring designBackupPath(int sec){
    wchar_t b[40]; swprintf(b,40,L"\\design_%d_saved.txt",sec);
    return dataDir()+b;
}

static std::vector<std::wstring> splitTab(const std::wstring& s){
    std::vector<std::wstring> out; size_t pos=0;
    while(true){ size_t e=s.find(L'\t',pos);
        if(e==std::wstring::npos){ out.push_back(s.substr(pos)); break; }
        out.push_back(s.substr(pos,e-pos)); pos=e+1; }
    return out;
}

static std::wstring serializeDesign(const Design& d){
    std::wstring out;
    wchar_t hb[64]; swprintf(hb,64,L"%d\t",d.paper);
    out += std::wstring(hb)+escTab(d.name)+L"\r\n";
    for(const DItem& it: d.items){
        wchar_t b[512];
        swprintf(b,512,
            L"%d\t%.2f\t%.2f\t%.2f\t%.2f\t%d\t%d%d%d%d\t%u\t%.2f\t%d\t%u\t",
            it.kind,it.x,it.y,it.w,it.h,it.fontSize,
            it.bold?1:0,it.italic?1:0,it.underline?1:0,it.strike?1:0,
            (unsigned)it.color,it.lineW,it.lineStyle,(unsigned)it.borderColor);
        wchar_t b2[64]; swprintf(b2,64,L"\t%u\t%d",(unsigned)it.bgColor,it.align);
        out += std::wstring(b)+escTab(it.field)+L"\t"+escTab(it.text)+L"\t"
            +escTab(it.fontName)+L"\t"+escTab(it.logoPath)+std::wstring(b2)+L"\r\n";
    }
    return out;
}
static bool parseDesign(const std::wstring& all, Design& d){
    d.items.clear();
    size_t pos=0; bool first=true;
    while(pos<all.size()){
        size_t e=all.find(L'\n',pos); if(e==std::wstring::npos) e=all.size();
        std::wstring line=all.substr(pos,e-pos); pos=e+1;
        while(!line.empty() && (line.back()==L'\r'||line.back()==L'\n')) line.pop_back();
        if(line.empty()) continue;
        auto f=splitTab(line);
        if(first){
            first=false;
            d.paper = f.size()>0 ? _wtoi(f[0].c_str()) : 0;
            d.name  = f.size()>1 ? f[1] : L"";
            continue;
        }
        if(f.size()<16) continue;
        DItem it;
        it.kind=_wtoi(f[0].c_str());
        it.x=_wtof(f[1].c_str()); it.y=_wtof(f[2].c_str());
        it.w=_wtof(f[3].c_str()); it.h=_wtof(f[4].c_str());
        it.fontSize=_wtoi(f[5].c_str());
        std::wstring st=f[6];
        it.bold     = st.size()>0 && st[0]==L'1';
        it.italic   = st.size()>1 && st[1]==L'1';
        it.underline= st.size()>2 && st[2]==L'1';
        it.strike   = st.size()>3 && st[3]==L'1';
        it.color=(COLORREF)wcstoul(f[7].c_str(),NULL,10);
        it.lineW=_wtof(f[8].c_str());
        it.lineStyle=_wtoi(f[9].c_str());
        it.borderColor=(COLORREF)wcstoul(f[10].c_str(),NULL,10);
        it.field=f[11]; it.text=f[12]; it.fontName=f[13]; it.logoPath=f[14];
        if(trim(it.fontName).empty()) it.fontName=L"Vazirmatn";
        // optional newer fields: bgColor, align
        if(f.size()>15 && !trim(f[15]).empty())
            it.bgColor=(COLORREF)wcstoul(f[15].c_str(),NULL,10);
        else it.bgColor=CLR_INVALID;
        if(f.size()>16) it.align=_wtoi(f[16].c_str());
        d.items.push_back(it);
    }
    return !first;
}
static void saveDesignFile(int sec, const Design& d){
    writeFileUtf8(designPath(sec), serializeDesign(d), false);
}
static bool loadDesignFile(int sec, Design& d){
    std::wstring all=readFileUtf8(designPath(sec));
    if(all.empty()) return false;
    return parseDesign(all,d);
}

// ------------------------------------------------------ default templates ---
//  A clean professional default per section so the program ships with usable
//  layouts. The designer lets the user pick / tweak / backup these.
static Design defaultDesign(int sec){
    Design d; d.paper=1;   // A5 is friendliest for a reception slip
    double cw; double ch; paperMM(d.paper,cw,ch);
    auto addLabel=[&](double x,double y,const wchar_t* token,const wchar_t* text,
                      int sz,bool bold,COLORREF col){
        DItem it; it.kind=IT_LABEL; it.x=x; it.y=y; it.w=cw-2*x; it.h=sz*0.45;
        it.field=token?token:L""; it.text=text?text:L"";
        it.fontSize=sz; it.bold=bold; it.color=col;
        d.items.push_back(it);
    };
    auto addLine=[&](double y){
        DItem it; it.kind=IT_LINE_H; it.x=8; it.y=y; it.w=cw-16; it.h=0;
        it.lineW=0.3; it.lineStyle=2; it.color=RGB(60,60,60); d.items.push_back(it);
    };
    // outer border
    { DItem b; b.kind=IT_BORDER; b.x=5; b.y=5; b.w=cw-10; b.h=ch-10;
      b.lineW=0.5; b.borderColor=RGB(40,70,120); d.items.push_back(b); }
    // header
    addLabel(8,10,NULL,(std::wstring(L"درمانگاه ")+APP_NAME_W).c_str(),18,true,RGB(20,50,100));
    addLabel(8,20,NULL,PRINT_SECTIONS[sec<N_PRINT_SECTIONS?sec:0],13,true,RGB(40,70,120));
    addLine(30);
    // top info row
    addLabel(8,34,L"{queue}",L"شماره نوبت: ",13,true,RGB(0,0,0));
    addLabel(8,42,L"{date}", L"تاریخ: ",11,false,RGB(0,0,0));
    addLabel(70,42,L"{time}",L"ساعت: ",11,false,RGB(0,0,0));
    addLabel(8,50,L"{shift}",L"شیفت: ",11,false,RGB(0,0,0));
    addLabel(70,50,L"{dept}",L"بخش: ",11,false,RGB(0,0,0));
    addLine(58);
    // patient
    addLabel(8,62,L"{full}",  L"بیمار: ",13,true,RGB(0,0,0));
    addLabel(8,70,L"{nid}",   L"کد ملی: ",11,false,RGB(0,0,0));
    addLabel(70,70,L"{father}",L"نام پدر: ",11,false,RGB(0,0,0));
    addLabel(8,78,L"{birth}", L"تاریخ تولد: ",11,false,RGB(0,0,0));
    addLabel(70,78,L"{gender}",L"جنسیت: ",11,false,RGB(0,0,0));
    addLabel(8,86,L"{mobile}",L"تلفن: ",11,false,RGB(0,0,0));
    addLine(94);
    addLabel(8,98, L"{ins}", L"بیمه: ",11,false,RGB(0,0,0));
    addLabel(8,106,L"{paid}",L"مبلغ پرداختی: ",13,true,RGB(150,20,20));
    addLine(116);
    addLabel(8,120,L"{issued}",L"",10,false,RGB(90,90,90));
    return d;
}

//  Pre-made templates offered in the designer dropdown — 10 modern Iranian
//  clinic-reception layouts. Index 0 = section default (پذیرش-friendly).
static const wchar_t* TEMPLATE_NAMES[] = {
    L"۱) پیش‌فرض حرفه‌ای",
    L"۲) ساده و سریع",
    L"۳) رسمی با کادر آبی",
    L"۴) فشرده A5",
    L"۵) سربرگ رنگی مدرن",
    L"۶) نوبت‌دهی درشت",
    L"۷) رسید پرداخت",
    L"۸) دو ستونه شیک",
    L"۹) مینیمال خط‌دار",
    L"۱۰) کارت بیمار",
};
static const int N_TEMPLATES = sizeof(TEMPLATE_NAMES)/sizeof(TEMPLATE_NAMES[0]);

// --- small builder helpers shared by the templates --------------------------
namespace tpl {
    static DItem label(double x,double y,double w,const wchar_t* tok,
                       const wchar_t* t,int sz,bool b,COLORREF col,int align=0){
        DItem it; it.kind=IT_LABEL; it.x=x; it.y=y; it.w=w; it.h=sz*0.45;
        it.field=tok?tok:L""; it.text=t?t:L""; it.fontSize=sz; it.bold=b;
        it.color=col; it.align=align; return it;
    }
    static DItem hline(double x,double y,double w,double th,int style,COLORREF c){
        DItem it; it.kind=IT_LINE_H; it.x=x; it.y=y; it.w=w; it.h=0;
        it.lineW=th; it.lineStyle=style; it.color=c; return it;
    }
    static DItem border(double x,double y,double w,double h,double th,COLORREF c){
        DItem it; it.kind=IT_BORDER; it.x=x; it.y=y; it.w=w; it.h=h;
        it.lineW=th; it.borderColor=c; return it;
    }
    static DItem bar(double x,double y,double w,const wchar_t* tok,
                     const wchar_t* t,int sz,COLORREF bg,COLORREF fg,int align=1){
        DItem it=label(x,y,w,tok,t,sz,true,fg,align); it.bgColor=bg; return it;
    }
}

static Design templateByIndex(int sec, int idx){
    using namespace tpl;
    Design d;
    double cw,ch;
    const wchar_t* secName=PRINT_SECTIONS[sec<N_PRINT_SECTIONS?sec:0];
    std::wstring clinic=std::wstring(L"درمانگاه ")+APP_NAME_W;

    switch(idx){
    case 1: { // simple & fast (A5)
        d.paper=1; paperMM(d.paper,cw,ch);
        d.items.push_back(label(8,10,cw-16,NULL,clinic.c_str(),16,true,RGB(20,50,100),1));
        d.items.push_back(label(8,20,cw-16,NULL,secName,12,true,RGB(40,70,120),1));
        d.items.push_back(label(8,34,cw-16,L"{queue}",L"نوبت: ",13,true,RGB(0,0,0)));
        d.items.push_back(label(8,44,cw-16,L"{full}",L"بیمار: ",12,true,RGB(0,0,0)));
        d.items.push_back(label(8,52,cw-16,L"{nid}",L"کد ملی: ",11,false,RGB(0,0,0)));
        d.items.push_back(label(8,60,cw-16,L"{date}",L"تاریخ: ",11,false,RGB(0,0,0)));
        d.items.push_back(label(8,68,cw-16,L"{paid}",L"پرداختی: ",12,true,RGB(150,20,20)));
        d.items.push_back(label(8,80,cw-16,L"{issued}",L"",10,false,RGB(120,120,120)));
        return d; }
    case 2: { // formal w/ blue frame (A4)
        d = defaultDesign(sec); d.paper=0;
        for(auto& it:d.items){ it.y*=1.18; it.x*=1.25; }
        return d; }
    case 3: { // compact A5
        d.paper=1; paperMM(d.paper,cw,ch);
        d.items.push_back(label(6,8,cw-12,NULL,clinic.c_str(),13,true,RGB(20,50,100)));
        d.items.push_back(label(6,18,80,L"{queue}",L"نوبت: ",12,true,RGB(0,0,0)));
        d.items.push_back(label(cw-80,18,74,L"{date}",L"تاریخ: ",10,false,RGB(0,0,0),2));
        d.items.push_back(label(6,26,cw-12,L"{full}",L"",12,true,RGB(0,0,0)));
        d.items.push_back(label(6,34,cw-12,L"{nid}",L"کد ملی: ",10,false,RGB(0,0,0)));
        d.items.push_back(label(6,42,cw-12,L"{paid}",L"پرداختی: ",11,true,RGB(150,20,20)));
        d.items.push_back(label(6,52,cw-12,L"{issued}",L"",9,false,RGB(120,120,120)));
        return d; }
    case 4: { // modern colored header (A5)
        d.paper=1; paperMM(d.paper,cw,ch);
        d.items.push_back(bar(5,6,cw-10,NULL,clinic.c_str(),16,RGB(15,90,160),RGB(255,255,255),1));
        d.items.push_back(bar(5,18,cw-10,NULL,secName,12,RGB(30,130,200),RGB(255,255,255),1));
        d.items.push_back(label(8,32,70,L"{queue}",L"نوبت: ",14,true,RGB(15,90,160)));
        d.items.push_back(label(cw-78,32,70,L"{time}",L"ساعت: ",11,false,RGB(0,0,0),2));
        d.items.push_back(hline(8,40,cw-16,0.3,2,RGB(150,150,150)));
        d.items.push_back(label(8,44,cw-16,L"{full}",L"بیمار: ",13,true,RGB(0,0,0)));
        d.items.push_back(label(8,52,cw-16,L"{nid}",L"کد ملی: ",11,false,RGB(0,0,0)));
        d.items.push_back(label(8,60,cw-16,L"{ins}",L"بیمه: ",11,false,RGB(0,0,0)));
        d.items.push_back(bar(8,70,cw-16,L"{paid}",L"مبلغ پرداختی: ",14,RGB(235,245,255),RGB(150,20,20),0));
        d.items.push_back(label(8,84,cw-16,L"{issued}",L"",9,false,RGB(120,120,120)));
        return d; }
    case 5: { // big queue number (A5)
        d.paper=1; paperMM(d.paper,cw,ch);
        d.items.push_back(label(6,8,cw-12,NULL,clinic.c_str(),13,true,RGB(20,50,100),1));
        d.items.push_back(label(6,18,cw-12,NULL,L"شماره نوبت شما",12,false,RGB(80,80,80),1));
        d.items.push_back(label(6,26,cw-12,L"{queue}",L"",40,true,RGB(15,90,160),1));
        d.items.push_back(hline(8,60,cw-16,0.4,0,RGB(15,90,160)));
        d.items.push_back(label(6,64,cw-12,L"{full}",L"",13,true,RGB(0,0,0),1));
        d.items.push_back(label(6,74,cw-12,L"{dept}",L"بخش: ",11,false,RGB(0,0,0),1));
        d.items.push_back(label(6,82,cw-12,L"{date}",L"",10,false,RGB(120,120,120),1));
        return d; }
    case 6: { // payment receipt (A5)
        d.paper=1; paperMM(d.paper,cw,ch);
        d.items.push_back(border(5,5,cw-10,ch-10,0.4,RGB(120,120,120)));
        d.items.push_back(bar(6,7,cw-12,NULL,L"رسید پرداخت",15,RGB(40,120,60),RGB(255,255,255),1));
        d.items.push_back(label(8,20,cw-16,NULL,clinic.c_str(),11,false,RGB(0,0,0),1));
        d.items.push_back(label(8,30,cw-16,L"{full}",L"بیمار: ",12,true,RGB(0,0,0)));
        d.items.push_back(label(8,38,cw-16,L"{date}",L"تاریخ: ",10,false,RGB(0,0,0)));
        d.items.push_back(label(cw/2,38,cw/2-8,L"{time}",L"ساعت: ",10,false,RGB(0,0,0)));
        d.items.push_back(hline(8,46,cw-16,0.3,2,RGB(150,150,150)));
        d.items.push_back(label(8,50,cw-16,L"{total}",L"جمع کل: ",11,false,RGB(0,0,0)));
        d.items.push_back(label(8,58,cw-16,L"{discount}",L"تخفیف: ",11,false,RGB(0,0,0)));
        d.items.push_back(bar(8,68,cw-16,L"{paid}",L"پرداختی: ",14,RGB(235,250,238),RGB(40,120,60),0));
        d.items.push_back(label(8,82,cw-16,L"{issued}",L"",9,false,RGB(120,120,120)));
        return d; }
    case 7: { // two-column elegant (A4)
        d.paper=0; paperMM(d.paper,cw,ch);
        d.items.push_back(border(8,8,cw-16,ch-16,0.5,RGB(40,70,120)));
        d.items.push_back(bar(10,10,cw-20,NULL,clinic.c_str(),20,RGB(25,60,110),RGB(255,255,255),1));
        d.items.push_back(label(10,26,cw-20,NULL,secName,14,true,RGB(40,70,120),1));
        d.items.push_back(hline(12,40,cw-24,0.4,0,RGB(40,70,120)));
        double cR=cw/2+4, cL=12, colW=cw/2-16;
        d.items.push_back(label(cR,46,colW,L"{queue}",L"نوبت: ",14,true,RGB(0,0,0)));
        d.items.push_back(label(cL,46,colW,L"{date}",L"تاریخ: ",12,false,RGB(0,0,0)));
        d.items.push_back(label(cR,56,colW,L"{full}",L"بیمار: ",14,true,RGB(0,0,0)));
        d.items.push_back(label(cL,56,colW,L"{time}",L"ساعت: ",12,false,RGB(0,0,0)));
        d.items.push_back(label(cR,66,colW,L"{nid}",L"کد ملی: ",12,false,RGB(0,0,0)));
        d.items.push_back(label(cL,66,colW,L"{father}",L"نام پدر: ",12,false,RGB(0,0,0)));
        d.items.push_back(label(cR,76,colW,L"{mobile}",L"تلفن: ",12,false,RGB(0,0,0)));
        d.items.push_back(label(cL,76,colW,L"{gender}",L"جنسیت: ",12,false,RGB(0,0,0)));
        d.items.push_back(hline(12,88,cw-24,0.3,2,RGB(150,150,150)));
        d.items.push_back(label(cR,92,colW,L"{ins}",L"بیمه: ",12,false,RGB(0,0,0)));
        d.items.push_back(bar(cL,92,colW,L"{paid}",L"پرداختی: ",14,RGB(245,245,250),RGB(150,20,20),0));
        d.items.push_back(label(12,108,cw-24,L"{issued}",L"",10,false,RGB(120,120,120)));
        return d; }
    case 8: { // minimal lined (A5)
        d.paper=1; paperMM(d.paper,cw,ch);
        d.items.push_back(label(8,8,cw-16,NULL,clinic.c_str(),15,true,RGB(0,0,0),1));
        d.items.push_back(hline(8,18,cw-16,0.5,0,RGB(0,0,0)));
        d.items.push_back(label(8,22,cw-16,L"{queue}",L"نوبت: ",13,true,RGB(0,0,0)));
        d.items.push_back(hline(8,30,cw-16,0.2,1,RGB(180,180,180)));
        d.items.push_back(label(8,33,cw-16,L"{full}",L"بیمار: ",12,true,RGB(0,0,0)));
        d.items.push_back(hline(8,41,cw-16,0.2,1,RGB(180,180,180)));
        d.items.push_back(label(8,44,cw-16,L"{nid}",L"کد ملی: ",11,false,RGB(0,0,0)));
        d.items.push_back(hline(8,52,cw-16,0.2,1,RGB(180,180,180)));
        d.items.push_back(label(8,55,cw-16,L"{date}",L"تاریخ: ",11,false,RGB(0,0,0)));
        d.items.push_back(hline(8,63,cw-16,0.2,1,RGB(180,180,180)));
        d.items.push_back(label(8,66,cw-16,L"{paid}",L"پرداختی: ",13,true,RGB(0,0,0)));
        d.items.push_back(hline(8,75,cw-16,0.5,0,RGB(0,0,0)));
        d.items.push_back(label(8,78,cw-16,L"{issued}",L"",9,false,RGB(120,120,120)));
        return d; }
    case 9: { // patient card (A5 landscape-ish compact)
        d.paper=1; paperMM(d.paper,cw,ch);
        d.items.push_back(border(6,6,cw-12,70,0.5,RGB(25,60,110)));
        d.items.push_back(bar(8,8,cw-16,NULL,clinic.c_str(),14,RGB(25,60,110),RGB(255,255,255),1));
        d.items.push_back(label(10,22,cw-20,L"{full}",L"",15,true,RGB(0,0,0),1));
        d.items.push_back(label(10,34,cw-20,L"{nid}",L"کد ملی: ",11,false,RGB(0,0,0)));
        d.items.push_back(label(10,42,cw-20,L"{birth}",L"تاریخ تولد: ",11,false,RGB(0,0,0)));
        d.items.push_back(label(10,50,cw-20,L"{mobile}",L"تلفن: ",11,false,RGB(0,0,0)));
        d.items.push_back(label(10,58,cw-20,L"{ins}",L"بیمه: ",11,false,RGB(0,0,0)));
        d.items.push_back(bar(8,80,cw-16,L"{queue}",L"شماره نوبت: ",13,RGB(235,245,255),RGB(25,60,110),0));
        d.items.push_back(label(8,92,cw-16,L"{date}",L"تاریخ مراجعه: ",10,false,RGB(120,120,120)));
        return d; }
    default:
        return defaultDesign(sec);
    }
}

// ---------------------------------------------------------- printer list ----
static std::vector<std::wstring> enumPrinters(){
    std::vector<std::wstring> out;
    DWORD needed=0, returned=0;
    EnumPrintersW(PRINTER_ENUM_LOCAL|PRINTER_ENUM_CONNECTIONS,NULL,4,
        NULL,0,&needed,&returned);
    if(needed==0) return out;
    std::vector<BYTE> buf(needed);
    if(EnumPrintersW(PRINTER_ENUM_LOCAL|PRINTER_ENUM_CONNECTIONS,NULL,4,
        buf.data(),needed,&needed,&returned)){
        PRINTER_INFO_4W* pi=(PRINTER_INFO_4W*)buf.data();
        for(DWORD i=0;i<returned;i++)
            if(pi[i].pPrinterName) out.push_back(pi[i].pPrinterName);
    }
    return out;
}
static std::wstring currentPrinter(){
    std::wstring p=getSetting(L"printer_name",L"");
    if(!p.empty()) return p;
    wchar_t def[256]={0}; DWORD sz=256;
    if(GetDefaultPrinterW(def,&sz)) return def;
    return L"";
}

// Send the standard ESC/POS «kick drawer» pulse (ESC p m t1 t2) straight to the
// configured printer via the spooler RAW data type. Only fires when the cash
// drawer option is enabled in the printer settings; silently no-ops otherwise.
void kickCashDrawer(){
    if(getSetting(L"cash_drawer",L"0")!=L"1") return;
    std::wstring prn=currentPrinter();
    if(prn.empty()) return;
    HANDLE hp=NULL;
    if(!OpenPrinterW((LPWSTR)prn.c_str(),&hp,NULL) || !hp) return;
    DOC_INFO_1W di; di.pDocName=(LPWSTR)L"AzadiTeb-Drawer";
    di.pOutputFile=NULL; di.pDatatype=(LPWSTR)L"RAW";
    if(StartDocPrinterW(hp,1,(LPBYTE)&di)){
        StartPagePrinter(hp);
        // ESC p 0 25 250  — pulse pin 2 (most common); a 2nd kick for pin 5.
        BYTE kick[]={0x1B,0x70,0x00,0x19,0xFA, 0x1B,0x70,0x01,0x19,0xFA};
        DWORD wr=0; WritePrinter(hp,kick,sizeof(kick),&wr);
        EndPagePrinter(hp); EndDocPrinter(hp);
    }
    ClosePrinter(hp);
}

// =================================================== printer settings dialog =
#define PS_CLASS L"AzPrinterCfg"
enum {
    PSB_CLOSE=1, PSB_TEST, PSB_ADV, PSB_DESIGN, PSB_A4, PSB_A5,
    PSB_P80, PSB_P58,                 // 80mm / 58mm thermal roll
    PSB_FIT, PSB_FILL, PSB_SEC_PREV, PSB_SEC_NEXT,
    PSB_COPIES_DN, PSB_COPIES_UP,     // copies − / +
    PSB_SECEN,                        // per-section enable toggle
    PSB_AUTOPRINT,                    // auto-print receipt on save
    PSB_DRAWER,                       // open cash drawer after print
    PSB_LOGO,                         // print clinic logo/header
    PSB_PRINTER_BASE=200
};

struct PrnState {
    HWND owner;
    int  hot;
    std::vector<std::wstring> printers;
    std::wstring sel;       // selected printer
    int  paper;             // 0 A4 / 1 A5 / 2 80mm / 3 58mm
    int  mode;              // 0 fit / 1 fill
    int  section;           // section being edited (for the design button)
    int  copies;           // number of copies per print (1..5)
    bool autoPrint;        // auto-print receipt right after admission/save
    bool drawer;           // pulse the cash drawer after a successful print
    bool logo;             // print the clinic header/logo band
};
static HWND s_prn=NULL;
static PrnState* s_ps=NULL;

static int prnCardW(){ return S(580); }
static int prnCardH(){ return S(820); }
static RECT prnCard(HWND h){
    RECT rc; GetClientRect(h,&rc);
    int w=prnCardW(), hh=prnCardH();
    RECT c={(rc.right-w)/2,(rc.bottom-hh)/2,(rc.right+w)/2,(rc.bottom+hh)/2};
    return c;
}

static void doTestPrint(HWND h){
    if(!s_ps || s_ps->sel.empty()){
        MessageBoxW(h,L"ابتدا یک چاپگر را انتخاب کنید.",L"تست چاپگر",
            MB_OK|MB_ICONWARNING); return;
    }
    HDC dc=CreateDCW(L"WINSPOOL",s_ps->sel.c_str(),NULL,NULL);
    if(!dc){
        MessageBoxW(h,L"اتصال به چاپگر برقرار نشد.\nبررسی کنید چاپگر روشن و متصل باشد.",
            L"تست چاپگر",MB_OK|MB_ICONERROR); return;
    }
    DOCINFOW di={sizeof(di)}; di.lpszDocName=L"آزادی طب — تست چاپ";
    if(StartDocW(dc,&di)>0){
        StartPage(dc);
        int dpiY=GetDeviceCaps(dc,LOGPIXELSY);
        HFONT f=CreateFontW(-(dpiY*18/72),0,0,0,FW_BOLD,0,0,0,DEFAULT_CHARSET,
            0,0,CLEARTYPE_QUALITY,0,L"Vazirmatn");
        HGDIOBJ of=SelectObject(dc,f);
        SetBkMode(dc,TRANSPARENT);
        SetTextAlign(dc,TA_RIGHT|TA_RTLREADING);
        RECT r={dpiY/2,dpiY/2,GetDeviceCaps(dc,HORZRES)-dpiY/2,dpiY*3};
        DrawTextW(dc,L"تست چاپ موفق بود — آزادی طب\nچاپگر به‌درستی کار می‌کند.",-1,&r,
            DT_RIGHT|DT_WORDBREAK|DT_RTLREADING|DT_NOPREFIX);
        SelectObject(dc,of); DeleteObject(f);
        EndPage(dc); EndDoc(dc);
        MessageBoxW(h,L"صفحهٔ تست به چاپگر ارسال شد.",L"تست چاپگر",
            MB_OK|MB_ICONINFORMATION);
    } else {
        MessageBoxW(h,L"ارسال سند به چاپگر ناموفق بود.",L"تست چاپگر",
            MB_OK|MB_ICONERROR);
    }
    DeleteDC(dc);
}

static void doAdvanced(HWND h){
    if(!s_ps || s_ps->sel.empty()){
        MessageBoxW(h,L"ابتدا یک چاپگر را انتخاب کنید.",L"تنظیمات پیشرفته",
            MB_OK|MB_ICONWARNING); return;
    }
    HANDLE hp=NULL;
    if(!OpenPrinterW((LPWSTR)s_ps->sel.c_str(),&hp,NULL) || !hp){
        MessageBoxW(h,L"دسترسی به چاپگر ممکن نشد.",L"تنظیمات پیشرفته",
            MB_OK|MB_ICONERROR); return;
    }
    LONG sz=DocumentPropertiesW(h,hp,(LPWSTR)s_ps->sel.c_str(),NULL,NULL,0);
    if(sz>0){
        std::vector<BYTE> buf(sz);
        DEVMODEW* dm=(DEVMODEW*)buf.data();
        DocumentPropertiesW(h,hp,(LPWSTR)s_ps->sel.c_str(),dm,NULL,DM_OUT_BUFFER);
        DocumentPropertiesW(h,hp,(LPWSTR)s_ps->sel.c_str(),dm,dm,
            DM_IN_BUFFER|DM_OUT_BUFFER|DM_IN_PROMPT);
    }
    ClosePrinter(hp);
}

static int prnHit(HWND h, POINT pt);
static void prnPaint(HWND h, HDC dc0){
    RECT rc; GetClientRect(h,&rc);
    HDC dc=CreateCompatibleDC(dc0);
    HBITMAP bmp=CreateCompatibleBitmap(dc0,rc.right,rc.bottom);
    HGDIOBJ obm=SelectObject(dc,bmp);
    { HBRUSH sb=CreateSolidBrush(g_dark?RGB(6,9,14):RGB(28,36,48));
      FillRect(dc,&rc,sb); DeleteObject(sb); }
    gpFillAlpha(dc,rc,0,g_dark?RGB(0,0,0):RGB(20,28,40),120);
    RECT c=prnCard(h);
    gpShadow(dc,c,S(20),S(22),80);
    { HBRUSH pb=CreateSolidBrush(g_theme.surface); FillRect(dc,&c,pb); DeleteObject(pb); }
    gpGradRoundRect(dc,c,S(20),g_theme.surfaceTop,g_theme.surface,g_theme.border);
    SetBkMode(dc,TRANSPARENT);

    // title
    SelectObject(dc,g_fTitle); SetTextColor(dc,g_theme.text);
    RECT tr={c.left+S(20),c.top+S(18),c.right-S(20),c.top+S(54)};
    DrawTextW(dc,L"تنظیمات چاپگر و چاپ",-1,&tr,
        DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
    // close
    { RECT cb={c.left+S(16),c.top+S(18),c.left+S(42),c.top+S(44)};
      if(s_ps&&s_ps->hot==PSB_CLOSE) gpRoundRect(dc,cb,S(8),g_theme.hover,CLR_INVALID,255);
      RECT ci={cb.left+S(5),cb.top+S(5),cb.right-S(5),cb.bottom-S(5)};
      drawIcon(dc,ICO_X,ci,g_theme.text,S(2)); }

    // printers list
    SelectObject(dc,g_fSmall); SetTextColor(dc,g_theme.textDim);
    RECT lb={c.left+S(20),c.top+S(64),c.right-S(20),c.top+S(84)};
    DrawTextW(dc,L"چاپگر پیش‌فرض برنامه (مستقل از پیش‌فرض ویندوز):",-1,&lb,
        DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
    int ly=c.top+S(88);
    int lh=S(34), maxRows=5;
    for(int i=0;i<(int)s_ps->printers.size() && i<maxRows;i++){
        RECT r={c.left+S(20),ly+i*lh,c.right-S(20),ly+i*lh+lh-S(6)};
        bool selrow=(s_ps->printers[i]==s_ps->sel);
        bool hov=(s_ps->hot==PSB_PRINTER_BASE+i);
        gpRoundRect(dc,r,S(9),
            selrow?g_theme.accent:(hov?g_theme.hover:g_theme.surface2),
            selrow?g_theme.accent:g_theme.border,255);
        SetTextColor(dc,selrow?g_theme.accentText:g_theme.text);
        SelectObject(dc,g_fUI);
        RECT ir={r.right-S(34),r.top+S(6),r.right-S(12),r.bottom-S(6)};
        drawIcon(dc,ICO_PRINT,ir,selrow?g_theme.accentText:g_theme.accent,S(2));
        RECT nr={r.left+S(12),r.top,r.right-S(40),r.bottom};
        DrawTextW(dc,s_ps->printers[i].c_str(),-1,&nr,
            DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
    }
    if(s_ps->printers.empty()){
        RECT r={c.left+S(20),ly,c.right-S(20),ly+lh};
        SetTextColor(dc,g_theme.danger); SelectObject(dc,g_fUI);
        DrawTextW(dc,L"هیچ چاپگری روی این سیستم پیدا نشد.",-1,&r,
            DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
    }

    int by=ly+maxRows*lh+S(8);
    // shared chip drawer used across all option rows
    auto chip=[&](int id,const wchar_t* t,RECT r,bool on){
        bool hov=(s_ps->hot==id);
        gpRoundRect(dc,r,S(9),on?g_theme.accent:(hov?g_theme.hover:g_theme.surface2),
            on?g_theme.accent:g_theme.border,255);
        SetTextColor(dc,on?g_theme.accentText:g_theme.text);
        SelectObject(dc,g_fUIB);
        DrawTextW(dc,t,-1,&r,DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
    };
    // a labelled on/off pill toggle (RTL: label on the right, pill on the left)
    auto toggleRow=[&](int id,const wchar_t* label,bool on,int yy){
        SetTextColor(dc,g_theme.text); SelectObject(dc,g_fUI);
        RECT lr={c.left+S(96),yy,c.right-S(20),yy+S(30)};
        DrawTextW(dc,label,-1,&lr,DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
        RECT pill={c.left+S(20),yy+S(2),c.left+S(86),yy+S(28)};
        bool hov=(s_ps->hot==id);
        gpRoundRect(dc,pill,(pill.bottom-pill.top)/2,
            on?g_theme.success:(hov?g_theme.hover:g_theme.surface2),
            on?g_theme.success:g_theme.border,255);
        int kr=(pill.bottom-pill.top)/2-S(3);
        int kcx= on? (pill.right-S(3)-kr) : (pill.left+S(3)+kr);
        int kcy=(pill.top+pill.bottom)/2;
        RECT kn={kcx-kr,kcy-kr,kcx+kr,kcy+kr};
        gpRoundRect(dc,kn,kr,RGB(255,255,255),CLR_INVALID,255);
        SetTextColor(dc,on?g_theme.accentText:g_theme.textDim);
        SelectObject(dc,g_fSmall);
        RECT tt={pill.left+(on?S(6):S(20)),pill.top,pill.right-(on?S(20):S(6)),pill.bottom};
        DrawTextW(dc,on?L"روشن":L"خاموش",-1,&tt,
            DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
    };

    // ---- section selector (each section keeps its OWN print design) ----
    SetTextColor(dc,g_theme.textDim); SelectObject(dc,g_fSmall);
    { RECT sl={c.left+S(20),by,c.right-S(20),by+S(20)};
      DrawTextW(dc,L"بخش/دپارتمان چاپ (هر بخش طراحی مستقل دارد):",-1,&sl,
          DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX); }
    by+=S(24);
    {
        int navW=S(40);
        RECT rPrev={c.left+S(20),by,c.left+S(20)+navW,by+S(34)};
        RECT rNext={c.right-S(20)-navW,by,c.right-S(20),by+S(34)};
        RECT rMid ={rPrev.right+S(6),by,rNext.left-S(6),by+S(34)};
        bool hp=(s_ps->hot==PSB_SEC_PREV), hn=(s_ps->hot==PSB_SEC_NEXT);
        gpRoundRect(dc,rPrev,S(9),hp?g_theme.hover:g_theme.surface2,g_theme.border,255);
        gpRoundRect(dc,rNext,S(9),hn?g_theme.hover:g_theme.surface2,g_theme.border,255);
        SetTextColor(dc,g_theme.text); SelectObject(dc,g_fUIB);
        DrawTextW(dc,L"‹",-1,&rPrev,DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_NOPREFIX);
        DrawTextW(dc,L"›",-1,&rNext,DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_NOPREFIX);
        gpRoundRect(dc,rMid,S(9),g_theme.inputBg,g_theme.accent,255);
        SetTextColor(dc,g_theme.accent); SelectObject(dc,g_fUIB);
        int sc=s_ps->section; if(sc<0||sc>=N_PRINT_SECTIONS) sc=0;
        DrawTextW(dc,PRINT_SECTIONS[sc],-1,&rMid,
            DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
    }
    by+=S(44);

    // ---- paper sizes (A4 / A5 / 80mm / 58mm thermal) ----
    SetTextColor(dc,g_theme.textDim); SelectObject(dc,g_fSmall);
    { RECT pl={c.left+S(20),by,c.right-S(20),by+S(20)};
      DrawTextW(dc,L"اندازهٔ کاغذ:",-1,&pl,
        DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX); }
    by+=S(24);
    {
        int n=4, gap=S(4);
        int cwN=(c.right-c.left-S(40)-gap*(n-1))/n;
        int cx=c.right-S(20)-cwN;
        RECT rA4 ={cx,by,cx+cwN,by+S(34)}; chip(PSB_A4 ,L"A4",rA4 ,s_ps->paper==0); cx-=cwN+gap;
        RECT rA5 ={cx,by,cx+cwN,by+S(34)}; chip(PSB_A5 ,L"A5",rA5 ,s_ps->paper==1); cx-=cwN+gap;
        RECT r80 ={cx,by,cx+cwN,by+S(34)}; chip(PSB_P80,L"رول ۸۰",r80,s_ps->paper==2); cx-=cwN+gap;
        RECT r58 ={cx,by,cx+cwN,by+S(34)}; chip(PSB_P58,L"رول ۵۸",r58,s_ps->paper==3);
    }
    by+=S(44);

    // ---- fit / fill ----
    SetTextColor(dc,g_theme.textDim); SelectObject(dc,g_fSmall);
    { RECT pl={c.left+S(20),by,c.right-S(20),by+S(20)};
      DrawTextW(dc,L"حالت تطبیق با کاغذ:",-1,&pl,
        DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX); }
    by+=S(24);
    {
        int cwH=(c.right-c.left-S(44))/2;
        int cx=c.right-S(20)-cwH;
        RECT rFit ={cx,by,cx+cwH,by+S(34)}; chip(PSB_FIT ,L"متناسب",rFit ,s_ps->mode==0); cx-=cwH+S(4);
        RECT rFill={cx,by,cx+cwH,by+S(34)}; chip(PSB_FILL,L"پرکردن",rFill,s_ps->mode==1);
    }
    by+=S(44);

    // ---- number of copies (− N +) ----
    SetTextColor(dc,g_theme.text); SelectObject(dc,g_fUI);
    { RECT lr={c.left+S(150),by,c.right-S(20),by+S(34)};
      DrawTextW(dc,L"تعداد نسخهٔ چاپ:",-1,&lr,
        DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX); }
    {
        int bwn=S(40);
        RECT rDn={c.left+S(20),by,c.left+S(20)+bwn,by+S(34)};
        RECT rUp={c.left+S(20)+bwn+S(46)+S(4),by,c.left+S(20)+bwn+S(46)+S(4)+bwn,by+S(34)};
        RECT rNum={rDn.right+S(2),by,rUp.left-S(2),by+S(34)};
        bool hd=(s_ps->hot==PSB_COPIES_DN), hu=(s_ps->hot==PSB_COPIES_UP);
        gpRoundRect(dc,rDn,S(9),hd?g_theme.hover:g_theme.surface2,g_theme.border,255);
        gpRoundRect(dc,rUp,S(9),hu?g_theme.hover:g_theme.surface2,g_theme.border,255);
        gpRoundRect(dc,rNum,S(9),g_theme.inputBg,g_theme.accent,255);
        SetTextColor(dc,g_theme.text); SelectObject(dc,g_fUIB);
        DrawTextW(dc,L"−",-1,&rDn,DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_NOPREFIX);
        DrawTextW(dc,L"+",-1,&rUp,DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_NOPREFIX);
        wchar_t nb[8]; swprintf(nb,8,L"%d",s_ps->copies);
        SetTextColor(dc,g_theme.accent);
        DrawTextW(dc,toFaDigits(nb).c_str(),-1,&rNum,
            DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
    }
    by+=S(46);

    // ---- toggles: section-enabled / auto-print / cash drawer / logo ----
    bool secOn = getSetting(L"sec_enabled_"+std::to_wstring(s_ps->section),L"1")!=L"0";
    toggleRow(PSB_SECEN,    L"چاپ این بخش فعال باشد",            secOn,         by); by+=S(36);
    toggleRow(PSB_AUTOPRINT,L"چاپ خودکار قبض پس از ثبت",         s_ps->autoPrint, by); by+=S(36);
    toggleRow(PSB_DRAWER,   L"باز کردن کشوی پول پس از چاپ",       s_ps->drawer,  by); by+=S(36);
    toggleRow(PSB_LOGO,     L"چاپ سربرگ/لوگوی درمانگاه",         s_ps->logo,    by); by+=S(42);

    // action buttons
    auto btn=[&](int id,const wchar_t* t,int icon,RECT r,bool primary){
        bool hov=(s_ps->hot==id);
        COLORREF bg = primary? (hov?g_theme.accentHover:g_theme.accent)
                             : (hov?g_theme.hover:g_theme.surface2);
        gpRoundRect(dc,r,S(10),bg,primary?bg:g_theme.border,255);
        COLORREF tc=primary?g_theme.accentText:g_theme.text;
        SetTextColor(dc,tc); SelectObject(dc,g_fUIB);
        RECT ir={r.right-S(36),r.top+S(8),r.right-S(14),r.bottom-S(8)};
        drawIcon(dc,icon,ir,tc,S(2));
        RECT nr={r.left+S(10),r.top,r.right-S(40),r.bottom};
        DrawTextW(dc,t,-1,&nr,DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
    };
    int bw=(c.right-c.left-S(44))/2;
    RECT rTest={c.right-S(20)-bw,by,c.right-S(20),by+S(44)};
    btn(PSB_TEST,L"تست اتصال و چاپ",ICO_PRINT,rTest,false);
    RECT rAdv={c.left+S(20),by,c.left+S(20)+bw,by+S(44)};
    btn(PSB_ADV,L"تنظیمات پیشرفتهٔ درایور",ICO_GEAR,rAdv,false);
    by+=S(52);
    RECT rDes={c.left+S(20),by,c.right-S(20),by+S(46)};
    std::wstring dl=std::wstring(L"طراحی و تنظیم چاپ بخش: ")+
        PRINT_SECTIONS[s_ps->section<N_PRINT_SECTIONS?s_ps->section:0];
    btn(PSB_DESIGN,dl.c_str(),ICO_RECEIPT,rDes,true);

    BitBlt(dc0,0,0,rc.right,rc.bottom,dc,0,0,SRCCOPY);
    SelectObject(dc,obm); DeleteObject(bmp); DeleteDC(dc);
}

static int prnHit(HWND h, POINT pt){
    if(!s_ps) return 0;
    RECT c=prnCard(h);
    if(!PtInRect(&c,pt)) return -1;   // scrim
    RECT cb={c.left+S(16),c.top+S(18),c.left+S(42),c.top+S(44)};
    if(PtInRect(&cb,pt)) return PSB_CLOSE;
    int ly=c.top+S(88), lh=S(34), maxRows=5;
    for(int i=0;i<(int)s_ps->printers.size() && i<maxRows;i++){
        RECT r={c.left+S(20),ly+i*lh,c.right-S(20),ly+i*lh+lh-S(6)};
        if(PtInRect(&r,pt)) return PSB_PRINTER_BASE+i;
    }
    int by=ly+maxRows*lh+S(8);
    // section selector row (label +24, controls 34h)
    by+=S(24);
    { int navW=S(40);
      RECT rPrev={c.left+S(20),by,c.left+S(20)+navW,by+S(34)};
      RECT rNext={c.right-S(20)-navW,by,c.right-S(20),by+S(34)};
      if(PtInRect(&rPrev,pt)) return PSB_SEC_PREV;
      if(PtInRect(&rNext,pt)) return PSB_SEC_NEXT; }
    by+=S(44);
    // paper sizes (label +24, then 4 chips)
    by+=S(24);
    {
        int n=4, gap=S(4);
        int cwN=(c.right-c.left-S(40)-gap*(n-1))/n;
        int cx=c.right-S(20)-cwN;
        RECT rA4 ={cx,by,cx+cwN,by+S(34)}; if(PtInRect(&rA4 ,pt)) return PSB_A4;  cx-=cwN+gap;
        RECT rA5 ={cx,by,cx+cwN,by+S(34)}; if(PtInRect(&rA5 ,pt)) return PSB_A5;  cx-=cwN+gap;
        RECT r80 ={cx,by,cx+cwN,by+S(34)}; if(PtInRect(&r80 ,pt)) return PSB_P80; cx-=cwN+gap;
        RECT r58 ={cx,by,cx+cwN,by+S(34)}; if(PtInRect(&r58 ,pt)) return PSB_P58;
    }
    by+=S(44);
    // fit / fill (label +24, then 2 chips)
    by+=S(24);
    {
        int cwH=(c.right-c.left-S(44))/2;
        int cx=c.right-S(20)-cwH;
        RECT rFit ={cx,by,cx+cwH,by+S(34)}; if(PtInRect(&rFit ,pt)) return PSB_FIT;  cx-=cwH+S(4);
        RECT rFill={cx,by,cx+cwH,by+S(34)}; if(PtInRect(&rFill,pt)) return PSB_FILL;
    }
    by+=S(44);
    // copies − N +
    {
        int bwn=S(40);
        RECT rDn={c.left+S(20),by,c.left+S(20)+bwn,by+S(34)};
        RECT rUp={c.left+S(20)+bwn+S(46)+S(4),by,c.left+S(20)+bwn+S(46)+S(4)+bwn,by+S(34)};
        if(PtInRect(&rDn,pt)) return PSB_COPIES_DN;
        if(PtInRect(&rUp,pt)) return PSB_COPIES_UP;
    }
    by+=S(46);
    // four toggle rows (each pill is c.left+20 .. c.left+86, 28h, +36 step)
    { RECT p1={c.left+S(20),by+S(2),c.left+S(86),by+S(28)};   if(PtInRect(&p1,pt)) return PSB_SECEN;     by+=S(36);
      RECT p2={c.left+S(20),by+S(2),c.left+S(86),by+S(28)};   if(PtInRect(&p2,pt)) return PSB_AUTOPRINT; by+=S(36);
      RECT p3={c.left+S(20),by+S(2),c.left+S(86),by+S(28)};   if(PtInRect(&p3,pt)) return PSB_DRAWER;    by+=S(36);
      RECT p4={c.left+S(20),by+S(2),c.left+S(86),by+S(28)};   if(PtInRect(&p4,pt)) return PSB_LOGO;      by+=S(42); }
    int bw=(c.right-c.left-S(44))/2;
    RECT rTest={c.right-S(20)-bw,by,c.right-S(20),by+S(44)}; if(PtInRect(&rTest,pt)) return PSB_TEST;
    RECT rAdv={c.left+S(20),by,c.left+S(20)+bw,by+S(44)};    if(PtInRect(&rAdv,pt)) return PSB_ADV;
    by+=S(52);
    RECT rDes={c.left+S(20),by,c.right-S(20),by+S(46)};       if(PtInRect(&rDes,pt)) return PSB_DESIGN;
    return 0;
}

static void prnClose(){
    if(s_prn && IsWindow(s_prn)){ HWND v=s_prn; s_prn=NULL; DestroyWindow(v); }
}
static LRESULT CALLBACK prnProc(HWND h, UINT m, WPARAM w, LPARAM l){
    switch(m){
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: { PAINTSTRUCT ps; HDC dc=BeginPaint(h,&ps);
        prnPaint(h,dc); EndPaint(h,&ps); return 0; }
    case WM_APP_THEME: InvalidateRect(h,NULL,FALSE); return 0;
    case WM_MOUSEMOVE: {
        POINT pt={GET_X_LPARAM(l),GET_Y_LPARAM(l)};
        int hr=prnHit(h,pt); if(hr<0) hr=0;
        if(s_ps && hr!=s_ps->hot){ s_ps->hot=hr; InvalidateRect(h,NULL,FALSE); }
        TRACKMOUSEEVENT te={sizeof(te),TME_LEAVE,h,0}; TrackMouseEvent(&te);
        return 0; }
    case WM_MOUSELEAVE:
        if(s_ps && s_ps->hot){ s_ps->hot=0; InvalidateRect(h,NULL,FALSE); }
        return 0;
    case WM_LBUTTONDOWN: {
        POINT pt={GET_X_LPARAM(l),GET_Y_LPARAM(l)};
        int id=prnHit(h,pt);
        if(id==-1 || id==PSB_CLOSE){ prnClose(); return 0; }
        if(id>=PSB_PRINTER_BASE){
            int i=id-PSB_PRINTER_BASE;
            if(i<(int)s_ps->printers.size()){
                std::wstring want=s_ps->printers[i];
                std::wstring chg=L"تغییر چاپگر پیش‌فرض به «"+want+L"»";
                if(printerRequestGate(h,L"تغییر نوع/چاپگر پیش‌فرض",chg,
                        L"printer_name="+want, L"چاپگر: "+want)){
                    s_ps->sel=want; setSetting(L"printer_name",want);
                    InvalidateRect(h,NULL,FALSE);
                }
            }
            return 0;
        }
        switch(id){
        case PSB_A4:
            if(printerRequestGate(h,L"تغییر اندازهٔ کاغذ",L"تغییر اندازه کاغذ به A4",
                    L"paper_size=A4",L"اندازهٔ کاغذ: A4")){
                s_ps->paper=0; setSetting(L"paper_size",L"A4"); InvalidateRect(h,NULL,FALSE); }
            break;
        case PSB_A5:
            if(printerRequestGate(h,L"تغییر اندازهٔ کاغذ",L"تغییر اندازه کاغذ به A5",
                    L"paper_size=A5",L"اندازهٔ کاغذ: A5")){
                s_ps->paper=1; setSetting(L"paper_size",L"A5"); InvalidateRect(h,NULL,FALSE); }
            break;
        case PSB_FIT:
            if(printerRequestGate(h,L"تغییر حالت چاپ",L"تغییر حالت چاپ به «متناسب»",
                    L"print_mode=fit",L"حالت چاپ: متناسب")){
                s_ps->mode=0; setSetting(L"print_mode",L"fit"); InvalidateRect(h,NULL,FALSE); }
            break;
        case PSB_FILL:
            if(printerRequestGate(h,L"تغییر حالت چاپ",L"تغییر حالت چاپ به «پرکننده»",
                    L"print_mode=fill",L"حالت چاپ: پرکننده")){
                s_ps->mode=1; setSetting(L"print_mode",L"fill"); InvalidateRect(h,NULL,FALSE); }
            break;
        case PSB_P80:
            if(printerRequestGate(h,L"تغییر اندازهٔ کاغذ",L"تغییر اندازه کاغذ به رول حرارتی ۸۰ میلی‌متر",
                    L"paper_size=80MM",L"اندازهٔ کاغذ: رول ۸۰")){
                s_ps->paper=2; setSetting(L"paper_size",L"80MM"); InvalidateRect(h,NULL,FALSE); }
            break;
        case PSB_P58:
            if(printerRequestGate(h,L"تغییر اندازهٔ کاغذ",L"تغییر اندازه کاغذ به رول حرارتی ۵۸ میلی‌متر",
                    L"paper_size=58MM",L"اندازهٔ کاغذ: رول ۵۸")){
                s_ps->paper=3; setSetting(L"paper_size",L"58MM"); InvalidateRect(h,NULL,FALSE); }
            break;
        case PSB_SEC_PREV:
            s_ps->section=(s_ps->section+N_PRINT_SECTIONS-1)%N_PRINT_SECTIONS;
            InvalidateRect(h,NULL,FALSE); break;
        case PSB_SEC_NEXT:
            s_ps->section=(s_ps->section+1)%N_PRINT_SECTIONS;
            InvalidateRect(h,NULL,FALSE); break;
        case PSB_COPIES_DN:
            if(s_ps->copies>1){ s_ps->copies--;
                setSetting(L"print_copies",std::to_wstring(s_ps->copies));
                InvalidateRect(h,NULL,FALSE); }
            break;
        case PSB_COPIES_UP:
            if(s_ps->copies<5){ s_ps->copies++;
                setSetting(L"print_copies",std::to_wstring(s_ps->copies));
                InvalidateRect(h,NULL,FALSE); }
            break;
        case PSB_SECEN: {
            std::wstring key=L"sec_enabled_"+std::to_wstring(s_ps->section);
            bool now=getSetting(key,L"1")!=L"0";
            setSetting(key, now?L"0":L"1");
            InvalidateRect(h,NULL,FALSE); break; }
        case PSB_AUTOPRINT:
            s_ps->autoPrint=!s_ps->autoPrint;
            setSetting(L"auto_print",s_ps->autoPrint?L"1":L"0");
            InvalidateRect(h,NULL,FALSE); break;
        case PSB_DRAWER:
            s_ps->drawer=!s_ps->drawer;
            setSetting(L"cash_drawer",s_ps->drawer?L"1":L"0");
            InvalidateRect(h,NULL,FALSE); break;
        case PSB_LOGO:
            s_ps->logo=!s_ps->logo;
            setSetting(L"print_logo",s_ps->logo?L"1":L"0");
            InvalidateRect(h,NULL,FALSE); break;
        case PSB_TEST: doTestPrint(h); break;
        case PSB_ADV:  doAdvanced(h); break;
        case PSB_DESIGN:{ int sec=s_ps->section; prnClose();
            openPrintDesigner(g_hFrame,sec); break; }
        }
        return 0; }
    case WM_KEYDOWN: if(w==VK_ESCAPE){ prnClose(); return 0; } break;
    case WM_DESTROY:
        if(s_ps){ delete s_ps; s_ps=NULL; } s_prn=NULL;
        if(g_hFrame) InvalidateRect(g_hFrame,NULL,TRUE);
        return 0;
    }
    return DefWindowProcW(h,m,w,l);
}

void openPrinterSettings(HWND owner){
    if(s_prn && IsWindow(s_prn)){ prnClose(); return; }
    static bool reg=false;
    if(!reg){ WNDCLASSW wc={0}; wc.lpfnWndProc=prnProc; wc.hInstance=g_hInst;
        wc.hCursor=LoadCursor(NULL,IDC_ARROW); wc.lpszClassName=PS_CLASS;
        RegisterClassW(&wc); reg=true; }
    RECT rc; GetClientRect(owner,&rc);
    POINT org={0,0}; ClientToScreen(owner,&org);
    s_ps=new PrnState();
    s_ps->owner=owner; s_ps->hot=0;
    s_ps->printers=enumPrinters();
    s_ps->sel=currentPrinter();
    { std::wstring ps=getSetting(L"paper_size",L"A5");
      s_ps->paper = ps==L"A4"?0 : ps==L"80MM"?2 : ps==L"58MM"?3 : 1; }
    s_ps->mode =(getSetting(L"print_mode",L"fit")==L"fill")?1:0;
    s_ps->section=0;
    { int cp=_wtoi(getSetting(L"print_copies",L"1").c_str());
      if(cp<1)cp=1; if(cp>5)cp=5; s_ps->copies=cp; }
    s_ps->autoPrint=getSetting(L"auto_print",L"0")==L"1";
    s_ps->drawer   =getSetting(L"cash_drawer",L"0")==L"1";
    s_ps->logo     =getSetting(L"print_logo",L"1")!=L"0";
    s_prn=CreateWindowExW(WS_EX_TOPMOST,PS_CLASS,L"",
        WS_POPUP|WS_VISIBLE|WS_CLIPCHILDREN,
        org.x,org.y,rc.right,rc.bottom,owner,NULL,g_hInst,NULL);
    BringWindowToTop(s_prn); SetFocus(s_prn);
}

// ============================================================================
//  PRINT DESIGNER  (separate file for clarity)
// ============================================================================
#include "printer_designer.inc"

// §1.52.0 — forward declaration so the legacy fieldValue resolver can reuse the
// canonical bare-name → {token} normalizer defined further below (pdNormalizeField).
static std::wstring pdNormalizeField(const std::wstring& f);

// ============================================================================
//  RENDER A SAVED DESIGN ONTO A PRINTER DC
// ============================================================================
static std::wstring fieldValue(const ReceptionRecord& r, const std::wstring& tokIn){
    // §1.52.0 — accept BOTH the canonical {token} form and the bare field name
    // (e.g. "firstName"). The new ready-made templates and the Print Designer
    // field picker store the *bare* human-readable key; the classic resolver only
    // matched the {token} form, which is why every field printed blank. Normalize
    // once up-front so every comparison below works for both shapes.
    std::wstring tok = pdNormalizeField(tokIn);
    if(tok==L"{first}")    return r.firstName;
    if(tok==L"{last}")     return r.lastName;
    if(tok==L"{full}")     return r.firstName+L" "+r.lastName;
    if(tok==L"{father}")   return r.fatherName;
    if(tok==L"{nid}")      return toFaDigits(r.nationalId);
    if(tok==L"{birth}")    return toFaDigits(r.birthDate);
    if(tok==L"{gender}")   return r.gender;
    if(tok==L"{mobile}")   return toFaDigits(r.mobile);
    if(tok==L"{landline}") return toFaDigits(r.landline);
    if(tok==L"{address}")  return r.address;
    if(tok==L"{ptype}")    return r.patientType;
    if(tok==L"{ins}")      return r.insurance;
    if(tok==L"{supp}")     return r.suppInsurance;
    if(tok==L"{queue}"){ wchar_t b[16]; swprintf(b,16,L"%d",r.queueNo); return toFaDigits(b); }
    if(tok==L"{date}")     return toFaDigits(r.apptDate);
    if(tok==L"{time}")     return toFaDigits(r.apptTime);
    if(tok==L"{shift}")    return r.shift;
    if(tok==L"{dept}")     return r.dept;
    if(tok==L"{doctor}")   return r.treatingDoctor.empty()? r.dept : r.treatingDoctor; // §1.53.0
    if(tok==L"{user}")     return r.userName;
    if(tok==L"{total}")    return toFaDigits(formatMoney(r.total))+L" ریال";
    if(tok==L"{discount}") return toFaDigits(formatMoney(r.discount))+L" ریال";
    if(tok==L"{paid}")     return toFaDigits(formatMoney(r.paid))+L" ریال";
    if(tok==L"{issued}")   return L"چاپ توسط پذیرش: "+
        (r.userName.empty()?g_session.user.fullname:r.userName);
    return L"";
}
static std::wstring itemText(const ReceptionRecord& r, const DItem& it){
    std::wstring s=it.text;
    if(!it.field.empty()) s += fieldValue(r,it.field);
    return s;
}

bool printDesignedReceipt(const ReceptionRecord& r, int sectionIdx, HWND owner){
    // honour the per-section enable toggle from the printer-settings dialog
    if(getSetting(L"sec_enabled_"+std::to_wstring(sectionIdx),L"1")==L"0")
        return false;
    Design d;
    if(!loadDesignFile(sectionIdx,d)){
        // try the per-section default if the user never saved one
        d=defaultDesign(sectionIdx);
        if(d.items.empty()) return false;
    }
    std::wstring prn=currentPrinter();
    HDC dc = prn.empty()? NULL : CreateDCW(L"WINSPOOL",prn.c_str(),NULL,NULL);
    if(!dc){
        PRINTDLGW pd={0}; pd.lStructSize=sizeof(pd); pd.hwndOwner=owner;
        pd.Flags=PD_RETURNDC|PD_NOPAGENUMS|PD_NOSELECTION|PD_USEDEVMODECOPIES;
        if(!PrintDlgW(&pd)) return false;
        dc=pd.hDC;
    }
    if(!dc) return false;

    double pageW,pageH; paperMM(d.paper,pageW,pageH);
    int dpiX=GetDeviceCaps(dc,LOGPIXELSX), dpiY=GetDeviceCaps(dc,LOGPIXELSY);
    int horz=GetDeviceCaps(dc,HORZRES),  vert=GetDeviceCaps(dc,VERTRES);
    int offX=GetDeviceCaps(dc,PHYSICALOFFSETX);
    int offY=GetDeviceCaps(dc,PHYSICALOFFSETY);
    // scale: device pixels per mm. 1 inch = 25.4 mm.
    double sx=dpiX/25.4, sy=dpiY/25.4;
    int mode=(getSetting(L"print_mode",L"fit")==L"fill")?1:0;
    if(mode==1){ // fill: stretch design to printable area
        double pw=horz/sx, ph=vert/sy;
        if(pw>1 && ph>1){ sx*=pageW/pw; sy*=pageH/ph; }
    }
    auto mmX=[&](double mm){ return (int)(mm*sx)-offX; };
    auto mmY=[&](double mm){ return (int)(mm*sy)-offY; };

    DOCINFOW di={sizeof(di)};
    std::wstring docName=std::wstring(APP_NAME_W)+L" — "+
        PRINT_SECTIONS[sectionIdx<N_PRINT_SECTIONS?sectionIdx:0];
    di.lpszDocName=docName.c_str();
    if(StartDocW(dc,&di)<=0){ DeleteDC(dc); return false; }

    // number of copies (1..5) configured in the printer-settings dialog
    int copies=_wtoi(getSetting(L"print_copies",L"1").c_str());
    if(copies<1) copies=1; if(copies>5) copies=5;

    for(int copy=0; copy<copies; ++copy){
    StartPage(dc);
    SetBkMode(dc,TRANSPARENT);

    for(const DItem& it: d.items){
        if(it.kind==IT_LABEL){
            std::wstring s=itemText(r,it);
            if(s.empty()) continue;
            int px=mmX(it.x), py=mmY(it.y);
            int lf=-(int)(it.fontSize*dpiY/72.0);
            int wpx=(int)(it.w*sx); if(wpx<10) wpx=horz;
            RECT rr={px,py,px+wpx,py+lf*-3};
            if(it.bgColor!=CLR_INVALID){
                RECT bg={px,py,px+wpx,py+(int)(it.fontSize*dpiY/72.0*1.3)};
                HBRUSH bb=CreateSolidBrush(it.bgColor); FillRect(dc,&bg,bb); DeleteObject(bb);
            }
            HFONT f=CreateFontW(lf,0,0,0,it.bold?FW_BOLD:FW_NORMAL,
                it.italic?1:0,it.underline?1:0,it.strike?1:0,DEFAULT_CHARSET,
                0,0,CLEARTYPE_QUALITY,0,
                it.fontName.empty()?L"Vazirmatn":it.fontName.c_str());
            HGDIOBJ of=SelectObject(dc,f);
            SetTextColor(dc,it.color);
            UINT al=(it.align==1)?DT_CENTER:(it.align==2)?DT_LEFT:DT_RIGHT;
            DrawTextW(dc,s.c_str(),-1,&rr,
                al|DT_TOP|DT_WORDBREAK|DT_RTLREADING|DT_NOPREFIX);
            SelectObject(dc,of); DeleteObject(f);
        } else if(it.kind==IT_LINE_H || it.kind==IT_LINE_V){
            int style=it.lineStyle==1?PS_DASH:it.lineStyle==2?PS_DOT:PS_SOLID;
            int wpx=(int)(it.lineW*sx); if(wpx<1) wpx=1;
            HPEN p=CreatePen(style,wpx,it.color);
            HGDIOBJ o=SelectObject(dc,p);
            if(it.kind==IT_LINE_H){
                MoveToEx(dc,mmX(it.x),mmY(it.y),0); LineTo(dc,mmX(it.x+it.w),mmY(it.y));
            } else {
                MoveToEx(dc,mmX(it.x),mmY(it.y),0); LineTo(dc,mmX(it.x),mmY(it.y+it.h));
            }
            SelectObject(dc,o); DeleteObject(p);
        } else if(it.kind==IT_BORDER){
            int style=it.lineStyle==1?PS_DASH:it.lineStyle==2?PS_DOT:PS_SOLID;
            int wpx=(int)(it.lineW*sx); if(wpx<1) wpx=1;
            HPEN p=CreatePen(style,wpx,it.borderColor);
            HGDIOBJ o=SelectObject(dc,p);
            HGDIOBJ ob=SelectObject(dc,GetStockObject(NULL_BRUSH));
            Rectangle(dc,mmX(it.x),mmY(it.y),mmX(it.x+it.w),mmY(it.y+it.h));
            SelectObject(dc,ob); SelectObject(dc,o); DeleteObject(p);
        } else if(it.kind==IT_LOGO){
            // logo rendering handled in the designer via GDI+; for the printer
            // we draw a placeholder box if no image could be loaded.
            HPEN p=CreatePen(PS_DOT,1,RGB(120,120,120));
            HGDIOBJ o=SelectObject(dc,p);
            HGDIOBJ ob=SelectObject(dc,GetStockObject(NULL_BRUSH));
            Rectangle(dc,mmX(it.x),mmY(it.y),mmX(it.x+it.w),mmY(it.y+it.h));
            SelectObject(dc,ob); SelectObject(dc,o); DeleteObject(p);
        }
    }
    EndPage(dc);
    }  // end copies loop
    EndDoc(dc); DeleteDC(dc);
    logLine(L"designed receipt printed for section "+std::to_wstring(sectionIdx)
            +L" ×"+std::to_wstring(copies));
    return true;
}

// ============================================================================
//  §1.19.0 — RENDER A NEW (print_designer JSON) DESIGN ONTO A PRINTER DC
//  The HTML/CSS/JS designer + native designer both persist a `PrintDesign`
//  (print_designer.h) bound to a section id. This renderer resolves that design
//  for the given section and prints it on the connected printer. The first time
//  (per session) it asks A4/A5 via the standard print dialog so the operator can
//  pick paper + printer; afterwards it reuses the saved default printer.
// ============================================================================
// ----------------------------------------------------------------------------
//  §1.21.0 — table model. A PIT_TABLE item stores its grid as JSON inside
//  `it.text`: {"cols":n,"rows":n,"header":bool,"widths":[..],"cells":[[..]]}.
//  We parse it here with a tiny tolerant reader so print + preview render the
//  EXACT same grid the designer shows (true WYSIWYG). Cells may contain {field}
//  tokens which are substituted with live data at print time.
// ----------------------------------------------------------------------------
struct PdTable {
    int cols=0, rows=0; bool header=false;
    std::vector<double> widths;
    std::vector<std::vector<std::wstring>> cells;
};
static std::wstring pdU8toW(const std::string& s){
    int n=MultiByteToWideChar(CP_UTF8,0,s.c_str(),(int)s.size(),NULL,0);
    std::wstring w(n,0); if(n) MultiByteToWideChar(CP_UTF8,0,s.c_str(),(int)s.size(),&w[0],n);
    return w;
}
// PrintItem colours are stored as 0x00RRGGBB (web/CSS order). GDI COLORREF is
// 0x00BBGGRR, so we must swap R<->B to print the *exact* colour designed.
static inline COLORREF pdCR(unsigned int rgb){
    return RGB((rgb>>16)&0xFF, (rgb>>8)&0xFF, rgb&0xFF);
}
static bool pdParseTable(const std::wstring& jsonW, PdTable& t){
    // convert to utf8 for byte parsing, but keep strings as wstring
    int n=WideCharToMultiByte(CP_UTF8,0,jsonW.c_str(),(int)jsonW.size(),NULL,0,NULL,NULL);
    std::string s(n,0); if(n) WideCharToMultiByte(CP_UTF8,0,jsonW.c_str(),(int)jsonW.size(),&s[0],n,NULL,NULL);
    size_t p=0; auto ws=[&]{ while(p<s.size()&&(s[p]==' '||s[p]=='\t'||s[p]=='\n'||s[p]=='\r'))++p; };
    auto rdStr=[&](std::string& out)->bool{ ws(); if(p>=s.size()||s[p]!='"')return false; ++p;
        while(p<s.size()&&s[p]!='"'){ char c=s[p++]; if(c=='\\'&&p<s.size()){ char e=s[p++];
            switch(e){case 'n':out+='\n';break;case 'r':out+='\r';break;case 't':out+='\t';break;
                case '"':out+='"';break;case '\\':out+='\\';break;case '/':out+='/';break;
                case 'u':{ if(p+4<=s.size()){ unsigned v=(unsigned)strtoul(s.substr(p,4).c_str(),NULL,16); p+=4;
                    if(v<0x80)out+=(char)v; else if(v<0x800){out+=(char)(0xC0|(v>>6));out+=(char)(0x80|(v&0x3F));}
                    else{out+=(char)(0xE0|(v>>12));out+=(char)(0x80|((v>>6)&0x3F));out+=(char)(0x80|(v&0x3F));} } break; }
                default:out+=e; } } else out+=c; }
        if(p<s.size()&&s[p]=='"')++p; return true; };
    auto rdNum=[&]()->double{ ws(); size_t st=p; while(p<s.size()&&(isdigit((unsigned char)s[p])||s[p]=='-'||s[p]=='+'||s[p]=='.'||s[p]=='e'||s[p]=='E'))++p; return atof(s.substr(st,p-st).c_str()); };
    ws(); if(p>=s.size()||s[p]!='{') return false; ++p;
    while(true){ ws(); if(p<s.size()&&s[p]=='}'){++p;break;} if(p>=s.size())break;
        std::string key; if(!rdStr(key))break; ws(); if(p<s.size()&&s[p]==':')++p;
        if(key=="cols") t.cols=(int)rdNum();
        else if(key=="rows") t.rows=(int)rdNum();
        else if(key=="header"){ ws(); if(s.compare(p,4,"true")==0){t.header=true;p+=4;} else if(s.compare(p,5,"false")==0){t.header=false;p+=5;} else rdNum(); }
        else if(key=="widths"){ ws(); if(p<s.size()&&s[p]=='['){++p; while(true){ ws(); if(p<s.size()&&s[p]==']'){++p;break;} t.widths.push_back(rdNum()); ws(); if(p<s.size()&&s[p]==','){++p;continue;} if(p<s.size()&&s[p]==']'){++p;break;} break; } } }
        else if(key=="cells"){ ws(); if(p<s.size()&&s[p]=='['){++p;
            while(true){ ws(); if(p<s.size()&&s[p]==']'){++p;break;}
                if(p<s.size()&&s[p]=='['){++p; std::vector<std::wstring> rowv;
                    while(true){ ws(); if(p<s.size()&&s[p]==']'){++p;break;} std::string cv; if(rdStr(cv)) rowv.push_back(pdU8toW(cv)); else { rdNum(); rowv.push_back(L""); }
                        ws(); if(p<s.size()&&s[p]==','){++p;continue;} if(p<s.size()&&s[p]==']'){++p;break;} break; }
                    t.cells.push_back(rowv); }
                ws(); if(p<s.size()&&s[p]==','){++p;continue;} if(p<s.size()&&s[p]==']'){++p;break;} break; } } }
        else { // skip unknown value
            ws(); if(p<s.size()&&s[p]=='"'){ std::string tmp; rdStr(tmp); }
            else if(p<s.size()&&s[p]=='{'){ int d=0; do{ if(s[p]=='{')d++; else if(s[p]=='}')d--; ++p; }while(p<s.size()&&d>0); }
            else if(p<s.size()&&s[p]=='['){ int d=0; do{ if(s[p]=='[')d++; else if(s[p]==']')d--; ++p; }while(p<s.size()&&d>0); }
            else rdNum();
        }
        ws(); if(p<s.size()&&s[p]==','){++p;continue;} ws(); if(p<s.size()&&s[p]=='}'){++p;break;}
        if(p>=s.size())break;
    }
    if(t.cols<=0||t.rows<=0||t.cells.empty()) return false;
    if((int)t.widths.size()!=t.cols){ t.widths.assign(t.cols,1.0); }
    return true;
}
// substitute {field} tokens inside an arbitrary string with live record data.
static std::wstring pdSubstFields(const ReceptionRecord& r, const std::wstring& in,
                                  std::wstring (*resolver)(const ReceptionRecord&, const std::wstring&)){
    std::wstring out; size_t i=0;
    while(i<in.size()){
        if(in[i]==L'{'){ size_t e=in.find(L'}',i);
            if(e!=std::wstring::npos){ std::wstring tok=in.substr(i,e-i+1);
                std::wstring v=resolver(r,tok);
                if(!v.empty() || tok.size()>2) { out+=v; i=e+1; continue; } } }
        out+=in[i++];
    }
    return out;
}

// §1.52.0 — normalize a bare field name (as stored by the ready-made templates
// and the Print Designer field picker, e.g. "firstName") into the canonical
// {token} vocabulary that pdFieldValue understands ("{first}"). Templates and
// user-created designs store the *bare* human-readable key in PrintItem.field;
// pdFieldValue historically only matched the {token} form, which is why every
// field printed blank. This single mapping fixes that root cause for ALL
// existing and future designs without touching their stored JSON.
static std::wstring pdNormalizeField(const std::wstring& f){
    if(f.empty()) return f;
    if(f.size()>=2 && f.front()==L'{' && f.back()==L'}') return f; // already a token
    // lowercase compare helper (ASCII only — field names are ASCII).
    // §1.52.0 — accept const char* literals so callers can pass plain
    // narrow strings; widen each byte on the fly.
    auto eq=[&](const char* a)->bool{
        size_t i=0; for(; i<f.size() && a[i]; ++i){
            wchar_t c=f[i]; if(c>=L'A'&&c<=L'Z') c=(wchar_t)(c-L'A'+L'a');
            wchar_t d=(wchar_t)(unsigned char)a[i];
            if(d>=L'A'&&d<=L'Z') d=(wchar_t)(d-L'A'+L'a');
            if(c!=d) return false; }
        return a[i]==0 && i==f.size();
    };
    // identity / general clinic meta
    if(eq("firstName")||eq("firstname")||eq("fname")||eq("name"))  return L"{first}";
    if(eq("lastName") ||eq("lastname") ||eq("lname")||eq("surname")) return L"{last}";
    if(eq("fullName") ||eq("fullname") ||eq("fullname"))            return L"{full}";
    if(eq("fatherName")||eq("fathername")||eq("father"))             return L"{father}";
    if(eq("nationalCode")||eq("nationalcode")||eq("nationalId")||eq("nationalid")||eq("nid")||eq("nationalNo")||eq("id")) return L"{nid}";
    if(eq("birthDate") ||eq("birthdate") ||eq("birth")||eq("dob"))   return L"{birth}";
    if(eq("gender")    ||eq("sex"))                                  return L"{gender}";
    if(eq("mobile")    ||eq("cellphone")||eq("cell")||eq("phone2"))  return L"{mobile}";
    if(eq("landline")  ||eq("phone")||eq("tel")||eq("telephone"))    return L"{landline}";
    if(eq("address"))                                                return L"{address}";
    if(eq("patientType")||eq("patienttype")||eq("ptype")||eq("visitType")||eq("visittype")) return L"{ptype}";
    if(eq("insurance") ||eq("ins")||eq("insName")||eq("insurer"))    return L"{ins}";
    if(eq("suppInsurance")||eq("suppinsurance")||eq("supplementary")||eq("supp")||eq("suppIns")) return L"{supp}";
    if(eq("queueNo")   ||eq("queueno")||eq("queue")||eq("turn")||eq("turnNo")) return L"{queue}";
    if(eq("apptDate")  ||eq("apptdate")||eq("date")||eq("regDate")||eq("regdate")||eq("appointmentDate")) return L"{date}";
    if(eq("apptTime")  ||eq("appttime")||eq("time")||eq("regTime")||eq("regtime")||eq("appointmentTime")) return L"{time}";
    if(eq("shift"))                                                  return L"{shift}";
    if(eq("dept")     ||eq("department")||eq("section")||eq("unit")) return L"{dept}";
    if(eq("doctor")   ||eq("physician")||eq("doc"))                  return L"{doctor}";
    if(eq("userName") ||eq("username")||eq("user")||eq("operator")||eq("receptionist")) return L"{user}";
    if(eq("clinic")   ||eq("clinicName")||eq("clinicname")||eq("center")) return L"{clinic}";
    if(eq("receiptNo")||eq("receiptno")||eq("receipt")||eq("invoiceNo")||eq("invoiceno")) return L"{receiptNo}";
    // money
    if(eq("total")    ||eq("gross")||eq("totalPrice")||eq("billTotal"))  return L"{total}";
    if(eq("mainShare")||eq("mainshare")||eq("insShare")||eq("insshare")||eq("insuranceShare")) return L"{insshare}";
    if(eq("discount"))                                                return L"{discount}";
    if(eq("paid")     ||eq("amountPaid")||eq("amountpaid"))          return L"{paid}";
    if(eq("patientShare")||eq("patientshare")||eq("patShare")||eq("patshare")||eq("share")) return L"{patientshare}";
    if(eq("finalTotal")||eq("finaltotal")||eq("final")||eq("net")||eq("netTotal")) return L"{finaltotal}";
    if(eq("visitFee") ||eq("visitfee")||eq("fee"))                   return L"{visitfee}";
    if(eq("payType")  ||eq("paytype")||eq("paymentType")||eq("paymenttype")) return L"{paytype}";
    if(eq("cashier")  ||eq("issued")||eq("issuer"))                  return L"{cashier}";
    // services aggregates
    if(eq("servicesCount")||eq("servicescount")||eq("serviceCount")||eq("servicecount")) return L"{servicescount}";
    if(eq("servicesTotal")||eq("servicestotal")||eq("serviceTotal")||eq("servicetotal")) return L"{servicestotal}";
    // clinic meta
    if(eq("clinicAddress")||eq("clinicaddress")||eq("address_clinic")) return L"{clinicaddr}";
    if(eq("clinicPhone") ||eq("clinicphone")||eq("phone_clinic"))     return L"{clinicphone}";
    if(eq("clinicManager")||eq("clinicmanager")||eq("manager"))       return L"{clinicmgr}";
    if(eq("clinicLicense")||eq("cliniclicense")||eq("license")||eq("licence")) return L"{cliniclic}";
    if(eq("age"))                                                    return L"{age}";
    if(eq("refDoctor")||eq("refdoctor")||eq("referringDoctor")||eq("referring")) return L"{refdoctor}";
    if(eq("room"))                                                   return L"{room}";
    if(eq("service"))                                                return L"{service}";
    // unknown bare name → wrap as {name} so the token pass can still decide
    return L"{"+f+L"}";
}

static std::wstring pdFieldValue(const ReceptionRecord& r, const std::wstring& tokIn){
    // §1.52.0 — accept BOTH the canonical {token} form and the bare field name
    // (e.g. "firstName") that ready-made templates / the designer store. We
    // normalize once up-front so every comparison below works for both shapes.
    std::wstring tok = pdNormalizeField(tokIn);
    // The new designer's field keys mirror the legacy {token} vocabulary, plus
    // a few extras. Reuse the classic resolver, then handle the new ones.
    if(tok==L"{first}")    return r.firstName;
    if(tok==L"{last}")     return r.lastName;
    if(tok==L"{full}")     return r.firstName+L" "+r.lastName;
    if(tok==L"{father}")   return r.fatherName;
    if(tok==L"{nid}")      return toFaDigits(r.nationalId);
    if(tok==L"{birth}")    return toFaDigits(r.birthDate);
    if(tok==L"{gender}")   return r.gender;
    if(tok==L"{mobile}")   return toFaDigits(r.mobile);
    if(tok==L"{landline}") return toFaDigits(r.landline);
    if(tok==L"{address}")  return r.address;
    if(tok==L"{ptype}")    return r.patientType;
    if(tok==L"{ins}")      return r.insurance;
    if(tok==L"{supp}")     return r.suppInsurance;
    if(tok==L"{insno}")    return toFaDigits(r.insNo);    // §1.53.0 (Bug D)
    if(tok==L"{insexp}")   return toFaDigits(r.insExp);
    if(tok==L"{queue}"){ wchar_t b[16]; swprintf(b,16,L"%d",r.queueNo); return toFaDigits(b); }
    if(tok==L"{date}")     return toFaDigits(r.apptDate);
    if(tok==L"{time}")     return toFaDigits(r.apptTime);
    if(tok==L"{datetime}") return toFaDigits(r.apptDate+L" - "+r.apptTime);
    if(tok==L"{shift}")    return r.shift;
    if(tok==L"{dept}")     return r.dept;
    // §1.53.0 (Bug D): prefer the dedicated treating-doctor name; fall back to
    // r.dept (the §1.52.0 behaviour) when the operator left the doctor blank.
    if(tok==L"{doctor}")   return r.treatingDoctor.empty() ? r.dept : r.treatingDoctor;
    if(tok==L"{apptdate}") return toFaDigits(r.apptDate);
    if(tok==L"{appttime}") return toFaDigits(r.apptTime);
    if(tok==L"{appttype}") return r.patientType;
    if(tok==L"{user}")     return r.userName;
    if(tok==L"{clinic}")   return L"درمانگاه آزادی طب";
    if(tok==L"{receiptNo}"){ wchar_t b[16]; swprintf(b,16,L"%d",r.queueNo); return toFaDigits(b); }
    if(tok==L"{total}")    return toFaDigits(formatMoney(r.total))+L" ریال";
    if(tok==L"{insshare}") return toFaDigits(formatMoney(r.mainShare))+L" ریال";
    if(tok==L"{discount}") return toFaDigits(formatMoney(r.discount))+L" ریال";
    if(tok==L"{paid}")     return toFaDigits(formatMoney(r.paid))+L" ریال";
    if(tok==L"{service}")  return L"ویزیت";
    if(tok==L"{issued}")   return L"چاپ توسط پذیرش: "+
        (r.userName.empty()?g_session.user.fullname:r.userName);
    // v1.22.0 — extra fields modelled on real Iranian clinic forms.
    if(tok==L"{clinicaddr}")  return getSetting(L"clinic_address",L"");
    if(tok==L"{clinicphone}") return toFaDigits(getSetting(L"clinic_phone",L""));
    if(tok==L"{clinicmgr}")   return getSetting(L"clinic_manager",L"");
    if(tok==L"{cliniclic}")   return toFaDigits(getSetting(L"clinic_license",L""));
    if(tok==L"{age}"){
        // derive age from birthDate (Jalali "YYYY/MM/DD") roughly.
        std::wstring bd=r.birthDate; if(bd.size()>=4){
            int by=_wtoi(bd.substr(0,4).c_str());
            if(by>1200 && by<1500){ SYSTEMTIME st; GetLocalTime(&st);
                int jy=st.wYear-621; int age=jy-by;
                if(age>0&&age<150){ wchar_t b[16]; swprintf(b,16,L"%d",age); return toFaDigits(b)+L" سال"; } } }
        return L"";
    }
    if(tok==L"{patientshare}") return toFaDigits(formatMoney(r.patientShare))+L" ریال";
    if(tok==L"{finaltotal}")   return toFaDigits(formatMoney(r.finalTotal))+L" ریال";
    if(tok==L"{visittype}")    return r.patientType;
    if(tok==L"{insidx}")     { wchar_t b[16]; swprintf(b,16,L"%d",r.insIdx); return toFaDigits(b); }
    if(tok==L"{shiftuser}")    return r.shift+L" — "+(r.userName.empty()?g_session.user.fullname:r.userName);
    if(tok==L"{barcode}")      return toFaDigits(r.nationalId);   // barcode payload = NID
    if(tok==L"{nationalcard}") return toFaDigits(r.nationalId);
    if(tok==L"{regdate}")      return toFaDigits(r.apptDate);
    if(tok==L"{regtime}")      return toFaDigits(r.apptTime);
    if(tok==L"{insshareonly}") return toFaDigits(formatMoney(r.mainShare));
    if(tok==L"{paidonly}")     return toFaDigits(formatMoney(r.paid));
    if(tok==L"{totalonly}")    return toFaDigits(formatMoney(r.total));
    // v1.24.0 — additional fields used by the new professional templates.
    // Not (yet) captured at reception → resolve to empty/sensible defaults so
    // a design that references them still prints cleanly (the field simply
    // shows blank, or is hidden when visibility==1).
    // §1.53.0 (Bug D): resolve the optional clinical fields from the record.
    // They stay empty by default so a design that references them prints
    // cleanly (and hides the row when visibility==1).
    if(tok==L"{refdoctor}")    return r.refDoctor;
    if(tok==L"{room}")         return r.dept;            // unit/room ≈ section
    if(tok==L"{nextvisit}")    return toFaDigits(r.nextVisit);
    if(tok==L"{weight}")       return toFaDigits(r.weight);
    if(tok==L"{height}")       return toFaDigits(r.height);
    if(tok==L"{bp}")           return toFaDigits(r.bp);
    if(tok==L"{temp}")         return toFaDigits(r.temp);
    if(tok==L"{pulse}")        return toFaDigits(r.pulse);
    if(tok==L"{allergy}")      return r.allergy;
    if(tok==L"{diagnosis}")    return r.diagnosis;
    if(tok==L"{servicecode}")  return r.services.empty()? L"" : toFaDigits(r.services[0].code);
    if(tok==L"{visitfee}")     return toFaDigits(formatMoney(r.total))+L" ریال";
    if(tok==L"{paytype}")      return L"نقدی";
    if(tok==L"{cashier}")      return r.userName.empty()?g_session.user.fullname:r.userName;
    // §1.51.0 — services-list aggregate tokens (the table itself is PIT_SERVICES;
    // these let a design print a count or a one-line summary outside the table).
    if(tok==L"{servicescount}"){ wchar_t b[16]; swprintf(b,16,L"%d",(int)r.services.size()); return toFaDigits(b); }
    if(tok==L"{servicestotal}"){ return toFaDigits(formatMoney(r.total))+L" ریال"; }
    return L"";
}

// Draw a PIT_TABLE grid inside `box` (device px). RTL: column 0 is the rightmost.
//   pxPerMmX/Y : device pixels per millimetre (for border width scaling)
//   live       : when non-NULL, substitutes {field} tokens with record data;
//                otherwise (preview with no record) shows the raw cell text.
static void pdDrawTable(HDC dc, const PrintItem& it, const RECT& box,
                        double pxPerMmX, double pxPerMmY,
                        double fontPxPerPt, const ReceptionRecord* live){
    PdTable t; if(!pdParseTable(it.text,t)) return;
    int X0=box.left, Y0=box.top, X1=box.right, Y1=box.bottom;
    int W=X1-X0, H=Y1-Y0; if(W<=0||H<=0) return;

    // column x-boundaries (RTL: col index 0 starts at the right edge)
    double sumw=0; for(double w:t.widths) sumw+=w; if(sumw<=0) sumw=t.cols;
    std::vector<int> cx; cx.reserve(t.cols+1);
    cx.push_back(X1);
    double acc=0;
    for(int c=0;c<t.cols;++c){ acc+=t.widths[c]; cx.push_back(X1-(int)(W*(acc/sumw))); }
    // row y-boundaries (top → bottom), equal height
    std::vector<int> ry; ry.reserve(t.rows+1);
    for(int rr=0;rr<=t.rows;++rr) ry.push_back(Y0+(int)((double)H*rr/t.rows));

    // fill header row background
    if(t.header && t.rows>0){
        RECT hr={cx[t.cols], ry[0], cx[0], ry[1]};
        HBRUSH hb=CreateSolidBrush(RGB(238,242,251));
        FillRect(dc,&hr,hb); DeleteObject(hb);
    }

    // cell text
    double fontPt = it.fontPt>0 ? it.fontPt : 9.0;
    int lf=-(int)(fontPt*fontPxPerPt);
    HFONT fNorm=CreateFontW(lf,0,0,0,FW_NORMAL,it.italic?1:0,0,0,DEFAULT_CHARSET,0,0,
        CLEARTYPE_QUALITY,0,it.fontName.empty()?L"Vazirmatn":it.fontName.c_str());
    HFONT fHead=CreateFontW(lf,0,0,0,FW_BOLD,0,0,0,DEFAULT_CHARSET,0,0,
        CLEARTYPE_QUALITY,0,it.fontName.empty()?L"Vazirmatn":it.fontName.c_str());
    int pad=(int)(1.2*pxPerMmX); if(pad<2)pad=2;
    SetTextColor(dc,pdCR(it.textColor));
    for(int rr=0;rr<t.rows;++rr){
        bool isHead=(t.header && rr==0);
        HGDIOBJ of=SelectObject(dc, isHead?fHead:fNorm);
        for(int c=0;c<t.cols;++c){
            std::wstring cell;
            if(rr<(int)t.cells.size() && c<(int)t.cells[rr].size()) cell=t.cells[rr][c];
            if(live) cell=pdSubstFields(*live,cell,pdFieldValue);
            // RTL: visual column c occupies [cx[c+1] .. cx[c]]
            RECT cr={cx[c+1]+pad, ry[rr]+pad/2, cx[c]-pad, ry[rr+1]-pad/2};
            if(cr.right<=cr.left||cr.bottom<=cr.top) continue;
            DrawTextW(dc,cell.c_str(),-1,&cr,
                DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_RTLREADING|DT_END_ELLIPSIS|DT_NOPREFIX);
        }
        SelectObject(dc,of);
    }
    DeleteObject(fNorm); DeleteObject(fHead);

    // grid lines
    int bw=(int)(it.borderWidth*pxPerMmX); if(bw<1)bw=1;
    HPEN pen=CreatePen(PS_SOLID,bw,pdCR(it.borderColor));
    HGDIOBJ op=SelectObject(dc,pen);
    for(int c=0;c<=t.cols;++c){ MoveToEx(dc,cx[c],ry[0],0); LineTo(dc,cx[c],ry[t.rows]); }
    for(int rr=0;rr<=t.rows;++rr){ MoveToEx(dc,cx[t.cols],ry[rr],0); LineTo(dc,cx[0],ry[rr]); }
    SelectObject(dc,op); DeleteObject(pen);
}

// §1.51.0: parse the PIT_SERVICES model JSON:
//   {"cols":n,"header":bool,"widths":[..],"labels":[..]}
// `cols`/`labels`/`widths` describe the table header; rows are filled from the
// live ReceptionRecord.services vector (variable count) at render time.
struct PdServicesModel {
    int cols=4;
    bool header=true;
    std::vector<double> widths;
    std::vector<std::wstring> labels;   // header captions (RTL order, col0=right)
};
static bool pdParseServicesModel(const std::wstring& jsonW, PdServicesModel& m){
    int n=WideCharToMultiByte(CP_UTF8,0,jsonW.c_str(),(int)jsonW.size(),NULL,0,NULL,NULL);
    std::string s(n,0); if(n) WideCharToMultiByte(CP_UTF8,0,jsonW.c_str(),(int)jsonW.size(),&s[0],n,NULL,NULL);
    size_t p=0; auto ws=[&]{ while(p<s.size()&&(s[p]==' '||s[p]=='\t'||s[p]=='\n'||s[p]=='\r'))++p; };
    auto rdStr=[&](std::string& out)->bool{ ws(); if(p>=s.size()||s[p]!='"')return false; ++p;
        while(p<s.size()&&s[p]!='"'){ char c=s[p++]; if(c=='\\'&&p<s.size()){ char e=s[p++];
            switch(e){case 'n':out+='\n';break;case 'r':out+='\r';break;case 't':out+='\t';break;
                case '"':out+='"';break;case '\\':out+='\\';break;case '/':out+='/';break;
                case 'u':{ if(p+4<=s.size()){ unsigned v=(unsigned)strtoul(s.substr(p,4).c_str(),NULL,16); p+=4;
                    if(v<0x80)out+=(char)v; else if(v<0x800){out+=(char)(0xC0|(v>>6));out+=(char)(0x80|(v&0x3F));}
                    else{out+=(char)(0xE0|(v>>12));out+=(char)(0x80|((v>>6)&0x3F));out+=(char)(0x80|(v&0x3F));} } break; }
                default:out+=e; } } else out+=c; }
        if(p<s.size()&&s[p]=='"')++p; return true; };
    auto rdNum=[&]()->double{ ws(); size_t st=p; while(p<s.size()&&(isdigit((unsigned char)s[p])||s[p]=='-'||s[p]=='+'||s[p]=='.'||s[p]=='e'||s[p]=='E'))++p; return atof(s.substr(st,p-st).c_str()); };
    ws(); if(p>=s.size()||s[p]!='{') return false; ++p;
    while(true){ ws(); if(p<s.size()&&s[p]=='}'){++p;break;} if(p>=s.size())break;
        std::string key; if(!rdStr(key))break; ws(); if(p<s.size()&&s[p]==':')++p;
        if(key=="cols") m.cols=(int)rdNum();
        else if(key=="header"){ ws(); if(s.compare(p,4,"true")==0){m.header=true;p+=4;} else if(s.compare(p,5,"false")==0){m.header=false;p+=5;} else rdNum(); }
        else if(key=="widths"){ ws(); if(p<s.size()&&s[p]=='['){++p; while(true){ ws(); if(p<s.size()&&s[p]==']'){++p;break;} m.widths.push_back(rdNum()); ws(); if(p<s.size()&&s[p]==','){++p;continue;} if(p<s.size()&&s[p]==']'){++p;break;} break; } } }
        else if(key=="labels"){ ws(); if(p<s.size()&&s[p]=='['){++p; while(true){ ws(); if(p<s.size()&&s[p]==']'){++p;break;}
                std::string cv; if(rdStr(cv)) m.labels.push_back(pdU8toW(cv));
                ws(); if(p<s.size()&&s[p]==','){++p;continue;} if(p<s.size()&&s[p]==']'){++p;break;} break; } } }
        else { ws(); if(p<s.size()&&s[p]=='"'){ std::string tmp; rdStr(tmp); }
            else if(p<s.size()&&s[p]=='{'){ int d=0; do{ if(s[p]=='{')d++; else if(s[p]=='}')d--; ++p; }while(p<s.size()&&d>0); }
            else if(p<s.size()&&s[p]=='['){ int d=0; do{ if(s[p]=='[')d++; else if(s[p]==']')d--; ++p; }while(p<s.size()&&d>0); }
            else rdNum();
        }
        ws(); if(p<s.size()&&s[p]==','){++p;continue;} ws(); if(p<s.size()&&s[p]=='}'){++p;break;}
        if(p>=s.size())break;
    }
    if(m.cols<1) m.cols=4;
    if((int)m.widths.size()!=m.cols) m.widths.assign(m.cols,1.0);
    if(m.labels.empty()){
        // sensible default Persian header (RTL order: col0=right → ردیف، نام، کد، مبلغ)
        m.labels.clear();
        const wchar_t* def[4]={L"ردیف",L"نام خدمت",L"کد",L"مبلغ"};
        int nc=m.cols>4?m.cols:4;
        for(int i=0;i<nc;++i) m.labels.push_back(i<4?std::wstring(def[i]):L"");
    }
    return true;
}

// §1.51.0: draw a dynamic services table inside `box` (device px).
// Renders one header row (optional) + one row per ReceptionRecord.services entry.
// Columns (RTL, col0=rightmost):
//   0 = ردیف (row number), 1 = نام خدمت (name), 2 = کد (code), 3 = مبلغ (line total)
// If cols>4, extra columns render empty. If cols<4, only the first `cols` render.
// `live` is the record whose services vector we render; if NULL or empty we show
// a placeholder row so the designer preview still looks like a real table.
static void pdDrawServices(HDC dc, const PrintItem& it, const RECT& box,
                           double pxPerMmX, double pxPerMmY,
                           double fontPxPerPt, const ReceptionRecord* live){
    PdServicesModel m; pdParseServicesModel(it.text, m);
    int X0=box.left, Y0=box.top, X1=box.right, Y1=box.bottom;
    int W=X1-X0, H=Y1-Y0; if(W<=0||H<=0) return;

    // §1.53.0 (Bug E): force a consistent RTL text-alignment baseline so mixed
    // Persian/number content in the services table never flips visually.
    SetTextAlign(dc, TA_RTLREADING|TA_TOP|TA_LEFT);

    int nDataRows = live ? (int)live->services.size() : 0;
    if(nDataRows==0) nDataRows=1;   // keep at least one (placeholder) row
    int totalRows = (m.header?1:0) + nDataRows;
    if(totalRows<1) return;

    // column x-boundaries (RTL: col 0 starts at the right edge)
    double sumw=0; for(double w:m.widths) sumw+=w; if(sumw<=0) sumw=m.cols;
    std::vector<int> cx; cx.reserve(m.cols+1);
    cx.push_back(X1);
    double acc=0;
    for(int c=0;c<m.cols;++c){ acc+=m.widths[c]; cx.push_back(X1-(int)(W*(acc/sumw))); }
    // row y-boundaries (top → bottom). Header gets a slightly taller band.
    std::vector<int> ry; ry.reserve(totalRows+1);
    if(m.header){
        int headH=(int)(H*0.16); if(headH<14) headH=14;
        ry.push_back(Y0); ry.push_back(Y0+headH);
        for(int rr=1; rr<=nDataRows; ++rr)
            ry.push_back(ry.back() + (int)((double)(H-headH)*rr/nDataRows));
        // fix last
        ry.back()=Y1;
    } else {
        for(int rr=0; rr<=nDataRows; ++rr) ry.push_back(Y0+(int)((double)H*rr/nDataRows));
    }

    // fonts
    double fontPt = it.fontPt>0 ? it.fontPt : 8.5;
    int lf=-(int)(fontPt*fontPxPerPt);
    HFONT fNorm=CreateFontW(lf,0,0,0,FW_NORMAL,it.italic?1:0,0,0,DEFAULT_CHARSET,0,0,
        CLEARTYPE_QUALITY,0,it.fontName.empty()?L"Vazirmatn":it.fontName.c_str());
    HFONT fHead=CreateFontW(lf,0,0,0,FW_BOLD,0,0,0,DEFAULT_CHARSET,0,0,
        CLEARTYPE_QUALITY,0,it.fontName.empty()?L"Vazirmatn":it.fontName.c_str());
    int pad=(int)(1.2*pxPerMmX); if(pad<2)pad=2;

    // header background
    if(m.header){
        RECT hr={cx[m.cols], ry[0], cx[0], ry[1]};
        HBRUSH hb=CreateSolidBrush(RGB(31,95,214));   // deep blue header band
        FillRect(dc,&hr,hb); DeleteObject(hb);
    }
    // alternating row banding for readability
    for(int rr=0; rr<nDataRows; ++rr){
        if((rr&1)==0) continue;       // band every other data row
        int y0r = m.header ? ry[1+rr] : ry[rr];
        int y1r = m.header ? ry[2+rr] : ry[rr+1];
        RECT br={cx[m.cols], y0r, cx[0], y1r};
        HBRUSH hb=CreateSolidBrush(RGB(243,247,254));
        FillRect(dc,&br,hb); DeleteObject(hb);
    }

    SetTextColor(dc, m.header ? RGB(255,255,255) : pdCR(it.textColor));
    // header cells (white text on blue)
    if(m.header){
        HGDIOBJ of=SelectObject(dc,fHead);
        for(int c=0;c<m.cols;++c){
            std::wstring cell = (c<(int)m.labels.size()) ? m.labels[c] : L"";
            // §1.53.0 (Bug E): convert any ASCII digits the user typed in a
            // header label to Persian digits so the receipt reads uniformly.
            cell = toFaDigits(cell);
            RECT cr={cx[c+1]+pad, ry[0]+pad/2, cx[c]-pad, ry[1]-pad/2};
            if(cr.right<=cr.left||cr.bottom<=cr.top) continue;
            DrawTextW(dc,cell.c_str(),-1,&cr,
                DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_RTLREADING|DT_END_ELLIPSIS|DT_NOPREFIX);
        }
        SelectObject(dc,of);
        SetTextColor(dc, pdCR(it.textColor));
    }
    // data rows
    {
        HGDIOBJ of=SelectObject(dc,fNorm);
        for(int rr=0; rr<nDataRows; ++rr){
            const ServiceLine* sln = (live && rr<(int)live->services.size()) ? &live->services[rr] : NULL;
            int ry0 = m.header ? ry[1+rr] : ry[rr];
            int ry1 = m.header ? ry[2+rr] : ry[rr+1];
            for(int c=0;c<m.cols;++c){
                std::wstring cell;
                if(sln){
                    if(c==0){ wchar_t b[8]; swprintf(b,8,L"%d",rr+1); cell=toFaDigits(b); }
                    else if(c==1) cell=sln->name;
                    else if(c==2) cell=toFaDigits(sln->code);
                    else if(c==3) cell=toFaDigits(formatMoney(sln->price*(long long)sln->qty - sln->discount))+L" ریال";
                    else cell=L"";
                } else {
                    // placeholder (designer preview with no record)
                    const wchar_t* ph[4]={L"۱",L"نمونهٔ خدمت",L"۰۰۱",L"۱۰۰٬۰۰۰ ریال"};
                    cell = (c<4)? std::wstring(ph[c]) : L"";
                }
                RECT cr={cx[c+1]+pad, ry0+pad/2, cx[c]-pad, ry1-pad/2};
                if(cr.right<=cr.left||cr.bottom<=cr.top) continue;
                // §1.53.0 (Bug E): the name column (c==1) is Persian prose — force
                // DT_RIGHT|DT_RTLREADING so it hugs the right edge and never flips
                // when the name mixes Persian words with Latin/number fragments.
                // All other columns (row #, code, money) are centered.
                UINT al = (c==1)
                    ? (DT_RIGHT |DT_VCENTER|DT_SINGLELINE|DT_RTLREADING|DT_END_ELLIPSIS|DT_NOPREFIX)
                    : (DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_RTLREADING|DT_END_ELLIPSIS|DT_NOPREFIX);
                DrawTextW(dc,cell.c_str(),-1,&cr,al);
            }
        }
        SelectObject(dc,of);
    }
    DeleteObject(fNorm); DeleteObject(fHead);

    // grid lines
    int bw=(int)(it.borderWidth*pxPerMmX); if(bw<1)bw=1;
    HPEN pen=CreatePen(PS_SOLID,bw,pdCR(it.borderColor));
    HGDIOBJ op=SelectObject(dc,pen);
    for(int c=0;c<=m.cols;++c){ MoveToEx(dc,cx[c],ry[0],0); LineTo(dc,cx[c],ry[totalRows]); }
    for(int rr=0;rr<=totalRows;++rr){ MoveToEx(dc,cx[m.cols],ry[rr],0); LineTo(dc,cx[0],ry[rr]); }
    SelectObject(dc,op); DeleteObject(pen);
}

// Map a design paper name to a Windows DMPAPER_* code. Returns 0 for custom.
static short pdPaperCode(const std::wstring& name){
    if(name==L"A3")     return DMPAPER_A3;       // 8
    if(name==L"A4")     return DMPAPER_A4;       // 9
    if(name==L"A5")     return DMPAPER_A5;       // 11
    if(name==L"A6")     return DMPAPER_A6;       // 70
    if(name==L"B5")     return DMPAPER_B5;       // 13
    if(name==L"Letter") return DMPAPER_LETTER;   // 1
    if(name==L"Legal")  return DMPAPER_LEGAL;    // 5
    return 0; // R80/R58/L90/L100/custom → set explicit dimensions instead
}
// Create a printer DC whose DEVMODE paper size & orientation match the design,
// so an A5 design actually prints on A5 (not the printer's default A4). Falls
// back to a plain DC if the printer doesn't expose its properties.
static HDC pdCreatePrinterDC(const std::wstring& prn, const PrintDesign& d){
    if(prn.empty()) return NULL;
    HANDLE hp=NULL;
    if(!OpenPrinterW((LPWSTR)prn.c_str(),&hp,NULL) || !hp)
        return CreateDCW(L"WINSPOOL",prn.c_str(),NULL,NULL);
    LONG need=DocumentPropertiesW(NULL,hp,(LPWSTR)prn.c_str(),NULL,NULL,0);
    HDC dc=NULL;
    if(need>0){
        std::vector<BYTE> buf(need);
        DEVMODEW* dm=(DEVMODEW*)buf.data();
        if(DocumentPropertiesW(NULL,hp,(LPWSTR)prn.c_str(),dm,NULL,DM_OUT_BUFFER)==IDOK){
            short code=pdPaperCode(d.paper);
            // orientation
            dm->dmFields |= DM_ORIENTATION;
            dm->dmOrientation = (d.orientation==1)?DMORIENT_LANDSCAPE:DMORIENT_PORTRAIT;
            if(code>0){
                dm->dmFields |= DM_PAPERSIZE;
                dm->dmPaperSize = code;
                dm->dmFields &= ~(DM_PAPERWIDTH|DM_PAPERLENGTH);
            } else {
                // custom / receipt / small-laser: set explicit size in 0.1 mm.
                double wmm=d.paperW, hmm=d.paperH;
                if(d.orientation==1 && wmm<hmm) std::swap(wmm,hmm);
                dm->dmFields |= (DM_PAPERSIZE|DM_PAPERWIDTH|DM_PAPERLENGTH);
                dm->dmPaperSize   = DMPAPER_USER;       // 256
                dm->dmPaperWidth  = (short)(wmm*10.0);   // tenths of a millimetre
                dm->dmPaperLength = (short)(hmm*10.0);
            }
            // let the driver validate/merge our changes
            DocumentPropertiesW(NULL,hp,(LPWSTR)prn.c_str(),dm,dm,DM_IN_BUFFER|DM_OUT_BUFFER);
            dc=CreateDCW(L"WINSPOOL",prn.c_str(),NULL,dm);
        }
    }
    ClosePrinter(hp);
    if(!dc) dc=CreateDCW(L"WINSPOOL",prn.c_str(),NULL,NULL);
    return dc;
}

bool printPrintDesign(const ReceptionRecord& r, int sectionId, HWND owner){
    PrintDesign d;
    if(!SectionDesign_Resolve(sectionId, d)) return false;   // no design → caller falls back
    if(d.items.empty()) return false;
    if(d.paperW<=0 || d.paperH<=0){
        double pw,ph; if(Paper_Dims(d.paper,pw,ph)){ d.paperW=pw; d.paperH=ph;
            if(d.orientation==1) std::swap(d.paperW,d.paperH); }
        else { d.paperW=148; d.paperH=210; }
    }

    // Resolve a printer DC whose paper size matches the DESIGN (so an A5 design
    // prints on A5, an 80mm receipt design on the receipt roll, etc.). First
    // time (no saved printer) → standard dialog so the operator picks a printer.
    std::wstring prn=currentPrinter();
    HDC dc = pdCreatePrinterDC(prn, d);
    if(!dc){
        PRINTDLGW pd={0}; pd.lStructSize=sizeof(pd); pd.hwndOwner=owner;
        pd.Flags=PD_RETURNDC|PD_NOPAGENUMS|PD_NOSELECTION|PD_USEDEVMODECOPIES;
        if(!PrintDlgW(&pd)) return true;   // user cancelled → treat as handled
        dc=pd.hDC;
    }
    if(!dc) return false;

    // §1.52.0 — RESPONSIVE A4→A5 AUTO-SCALE. All built-in templates are authored
    // in A4 mm coordinates (210×297). If the operator later switches the paper
    // size to A5 (148×210) — or any smaller sheet — the item coordinates would
    // overflow the page. We auto-detect the "authored space" from the bounding
    // box of all items (max x+w, max y+h) and compute a uniform scale factor so
    // the whole design shrinks proportionally and relocates to fit the current
    // paper perfectly. When paper == authored size (normal A4 case) scale == 1
    // and nothing changes. No schema change needed: the authored size is inferred
    // from the item extents, so this also fixes designs the user already saved.
    double authW=d.paperW, authH=d.paperH;
    for(const auto& it:d.items){ double ex=it.x+it.w; if(ex>authW)authW=ex;
                                  double ey=it.y+it.h; if(ey>authH)authH=ey; }
    double pscale=1.0;
    if(authW>d.paperW+0.01 || authH>d.paperH+0.01){
        double fx=d.paperW/authW, fy=d.paperH/authH;
        pscale = fx<fy ? fx : fy;
        if(pscale>1.0) pscale=1.0;
    }

    int dpiX=GetDeviceCaps(dc,LOGPIXELSX), dpiY=GetDeviceCaps(dc,LOGPIXELSY);
    int offX=GetDeviceCaps(dc,PHYSICALOFFSETX), offY=GetDeviceCaps(dc,PHYSICALOFFSETY);
    double sx=dpiX/25.4, sy=dpiY/25.4;
    // mm→device-px, applying the responsive A4→A5 scale so an A4-authored design
    // relocates and shrinks proportionally onto whatever paper size is active.
    auto mmX=[&](double mm){ return (int)(mm*pscale*sx)-offX; };
    auto mmY=[&](double mm){ return (int)(mm*pscale*sy)-offY; };

    DOCINFOW di={sizeof(di)};
    std::wstring docName=std::wstring(APP_NAME_W)+L" — print";
    di.lpszDocName=docName.c_str();
    if(StartDocW(dc,&di)<=0){
        // The stored/default printer rejected the job (common with virtual
        // "app" printers e.g. some PDF/photo apps → "doesn't support print
        // preview"). Fall back to the standard dialog so the operator picks a
        // working printer, then re-resolve DPI/offsets for the new DC.
        DeleteDC(dc); dc=NULL;
        PRINTDLGW pd={0}; pd.lStructSize=sizeof(pd); pd.hwndOwner=owner;
        pd.Flags=PD_RETURNDC|PD_NOPAGENUMS|PD_NOSELECTION|PD_USEDEVMODECOPIES;
        if(!PrintDlgW(&pd)) return true;        // cancelled → handled
        dc=pd.hDC; if(!dc) return false;
        dpiX=GetDeviceCaps(dc,LOGPIXELSX); dpiY=GetDeviceCaps(dc,LOGPIXELSY);
        offX=GetDeviceCaps(dc,PHYSICALOFFSETX); offY=GetDeviceCaps(dc,PHYSICALOFFSETY);
        sx=dpiX/25.4; sy=dpiY/25.4;
        if(StartDocW(dc,&di)<=0){
            MessageBoxW(owner,L"چاپگر انتخاب‌شده از چاپ این سند پشتیبانی نمی‌کند.\n"
                L"لطفاً یک چاپگر واقعی (نه «Microsoft Print to PDF» یا برنامهٔ عکس) انتخاب کنید.",
                L"چاپ طرح",MB_OK|MB_ICONWARNING);
            DeleteDC(dc); return false; }
    }
    StartPage(dc);
    SetBkMode(dc,TRANSPARENT);

    // paint items in z-order
    std::vector<const PrintItem*> ord;
    for(const auto& it:d.items) ord.push_back(&it);
    // simple insertion sort by z (avoids pulling in <algorithm>; item counts are tiny)
    for(size_t i=1;i<ord.size();++i){ const PrintItem* k=ord[i]; size_t j=i;
        while(j>0 && ord[j-1]->z > k->z){ ord[j]=ord[j-1]; --j; } ord[j]=k; }

    for(const PrintItem* pit : ord){
        const PrintItem& it=*pit;
        int x0=mmX(it.x), y0=mmY(it.y), x1=mmX(it.x+it.w), y1=mmY(it.y+it.h);
        if(it.type==PIT_TABLE){
            RECT rr={x0,y0,x1,y1};
            // §1.52.0: scale the internal font/cell px-per-mm so an A4-authored
            // table shrinks correctly onto an A5 sheet (WYSIWYG with preview).
            pdDrawTable(dc, it, rr, sx*pscale, sy*pscale, (dpiY/72.0)*pscale, &r);
        } else if(it.type==PIT_SERVICES){
            // §1.51.0: dynamic services list rendered from the live record.
            RECT rr={x0,y0,x1,y1};
            pdDrawServices(dc, it, rr, sx*pscale, sy*pscale, (dpiY/72.0)*pscale, &r);
        } else if(it.type==PIT_HLINE){
            int wpx=(int)(it.borderWidth*sx*pscale); if(wpx<1)wpx=1;
            HPEN p=CreatePen(PS_SOLID,wpx,pdCR(it.borderColor)); HGDIOBJ o=SelectObject(dc,p);
            MoveToEx(dc,x0,y0,0); LineTo(dc,x1,y0); SelectObject(dc,o); DeleteObject(p);
        } else if(it.type==PIT_VLINE){
            int wpx=(int)(it.borderWidth*sx*pscale); if(wpx<1)wpx=1;
            HPEN p=CreatePen(PS_SOLID,wpx,pdCR(it.borderColor)); HGDIOBJ o=SelectObject(dc,p);
            MoveToEx(dc,x0,y0,0); LineTo(dc,x0,y1); SelectObject(dc,o); DeleteObject(p);
        } else if(it.type==PIT_RECT||it.type==PIT_FRAME||it.type==PIT_LOGO||
                  it.type==PIT_PHOTO||it.type==PIT_QR||it.type==PIT_IMAGE){
            int wpx=(int)(it.borderWidth*sx*pscale); if(wpx<1)wpx=1;
            bool isImgItem=(it.type==PIT_LOGO||it.type==PIT_PHOTO||it.type==PIT_IMAGE);
            // v1.21.0: fill rect/frame background when not transparent (WYSIWYG).
            if((it.type==PIT_RECT||it.type==PIT_FRAME) && !it.fillTransparent){
                RECT fr={x0,y0,x1,y1}; HBRUSH fb=CreateSolidBrush(pdCR(it.fillColor));
                FillRect(dc,&fr,fb); DeleteObject(fb);
            }
            HPEN p=CreatePen(PS_SOLID,wpx,pdCR(it.borderColor)); HGDIOBJ o=SelectObject(dc,p);
            HGDIOBJ ob=SelectObject(dc,GetStockObject(NULL_BRUSH));
            // v1.20.0: PIT_IMAGE with a real picture draws no border box.
            if(!(isImgItem && !it.imgPath.empty()))
                Rectangle(dc,x0,y0,x1,y1);
            SelectObject(dc,ob); SelectObject(dc,o); DeleteObject(p);
            if(it.type==PIT_LOGO||it.type==PIT_QR||it.type==PIT_PHOTO||it.type==PIT_IMAGE){
                RECT rr={x0,y0,x1,y1};
                // v1.20.0: if the item carries an actual image (uploaded logo /
                // patient photo / image, stored as a file path or data:base64 URI),
                // render the picture; otherwise fall back to a labelled box.
                bool drawn=false;
                if(isImgItem && !it.imgPath.empty()){
                    // v1.23.0: honour the designed object-fit + padding so the
                    // logo/photo respects its rectangle exactly (no overflow,
                    // no stretch) and matches the designer preview 1:1.
                    int padPx=(int)(it.padding*sx); if(padPx<0)padPx=0;
                    drawn=gpDrawImageRectFit(dc,it.imgPath,rr,it.objectFit,padPx);
                }
                if(!drawn){
                    std::wstring ph=(it.type==PIT_LOGO)?L"لوگو":(it.type==PIT_QR?L"QR":(it.type==PIT_IMAGE?L"تصویر":L"عکس"));
                    HFONT f=CreateFontW(-(int)(9*dpiY/72.0),0,0,0,FW_NORMAL,0,0,0,
                        DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Vazirmatn");
                    HGDIOBJ of=SelectObject(dc,f); SetTextColor(dc,RGB(150,150,150));
                    DrawTextW(dc,ph.c_str(),-1,&rr,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
                    SelectObject(dc,of); DeleteObject(f);
                }
            }
        } else { // text-bearing: LABEL / FIELD / APPTNO
            std::wstring s=it.text;
            if(it.type==PIT_FIELD && !it.field.empty()) s=it.prefix+pdFieldValue(r,it.field)+it.suffix;
            else if(it.type==PIT_APPTNO){ wchar_t b[16]; swprintf(b,16,L"%d",r.queueNo>0?r.queueNo:it.startValue); s=it.prefix+toFaDigits(b); }
            else s=it.prefix+it.text+it.suffix;
            if(it.visibility==1 && (it.type==PIT_FIELD) && pdFieldValue(r,it.field).empty()) continue;
            if(s.empty()) continue;
            // v1.23.0: apply inner padding so text never touches the box edge,
            // matching the designer preview exactly.  §1.52.0: scale padding by
            // pscale so A4→A5 stays proportional.
            int padPx=(int)(it.padding*sx*pscale); if(padPx<0)padPx=0;
            RECT rr={x0+padPx,y0+padPx,x1-padPx,y1-padPx};
            if(rr.right<=rr.left){ rr.left=x0; rr.right=x1; }
            if(rr.bottom<=rr.top){ rr.top=y0; rr.bottom=y1; }
            int boxW=rr.right-rr.left, boxH=rr.bottom-rr.top;
            // horizontal alignment: 0=right 1=center 2=left (RTL)
            UINT al=(it.align==1)?DT_CENTER:(it.align==2)?DT_LEFT:DT_RIGHT;
            // v1.22.0: per-item text direction. dir 0=RTL, 1=LTR, 2=center.
            UINT dirf=(it.dir==1)?0:DT_RTLREADING;
            if(it.dir==2) al=DT_CENTER;
            // v1.24.0: AUTO-FIT so text is NEVER clipped / cut in half. We start
            // at the designed point size and shrink (down to a sensible floor)
            // until the wrapped block fits inside the box height AND each line
            // fits the width. This kills the "half-cut letters / missing ر"
            // problem the operator reported on real prints.  §1.52.0: start the
            // auto-fit from the responsively-scaled point size (A4→A5).
            double basePt = it.fontPt>0 ? it.fontPt : 10.0;
            basePt *= pscale;
            UINT base = al|DT_WORDBREAK|dirf|DT_NOPREFIX;
            HFONT f=NULL; HGDIOBJ of=NULL; RECT meas; int th=0;
            double pt=basePt; double floorPt = (basePt<7.0)? basePt*0.7 : 6.0;
            for(int tries=0; tries<14; ++tries){
                int lf=-(int)(pt*dpiY/72.0+0.5);
                f=CreateFontW(lf,0,0,0,it.bold?FW_BOLD:FW_NORMAL,it.italic?1:0,0,0,
                    DEFAULT_CHARSET,OUT_TT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,
                    DEFAULT_PITCH|FF_DONTCARE,
                    it.fontName.empty()?L"Vazirmatn":it.fontName.c_str());
                of=SelectObject(dc,f);
                meas=rr; meas.bottom=rr.top+30000;     // unbounded height for measure
                DrawTextW(dc,s.c_str(),-1,&meas,base|DT_TOP|DT_CALCRECT);
                th=meas.bottom-meas.top;
                int mw=meas.right-meas.left;
                if(th<=boxH && mw<=boxW+1) break;       // fits → done
                if(pt<=floorPt){ break; }               // can't shrink further
                SelectObject(dc,of); DeleteObject(f); f=NULL;
                pt = pt*0.92; if(pt<floorPt) pt=floorPt;
            }
            SetTextColor(dc,pdCR(it.textColor));
            // v1.23.0: vertical alignment (0=top 1=middle 2=bottom).
            RECT dr=rr;
            int bh=boxH;
            if(it.valign==1){ int off=(bh-th)/2; if(off>0) dr.top+=off; }
            else if(it.valign==2){ int off=(bh-th); if(off>0) dr.top+=off; }
            // DT_NOCLIP guarantees the LAST line is fully drawn even if the box is
            // a hair short, so descenders/letters are never sheared.
            DrawTextW(dc,s.c_str(),-1,&dr,base|DT_TOP|DT_NOCLIP);
            if(of) SelectObject(dc,of); if(f) DeleteObject(f);
        }
    }
    EndPage(dc); EndDoc(dc); DeleteDC(dc);
    logLine(L"print_designer design printed for section "+std::to_wstring(sectionId));
    return true;
}
