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
#include <stdio.h>

// ------------------------------------------------------------- sections -----
const wchar_t* PRINT_SECTIONS[] = {
    L"پذیرش درمانگاه",
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
    DItem():kind(IT_LABEL),x(10),y(10),w(50),h(8),
        fontSize(11),bold(false),italic(false),underline(false),strike(false),
        fontName(L"Vazirmatn"),color(RGB(0,0,0)),lineW(0.3),lineStyle(0),
        borderColor(RGB(0,0,0)){}
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
        out += std::wstring(b)+escTab(it.field)+L"\t"+escTab(it.text)+L"\t"
            +escTab(it.fontName)+L"\t"+escTab(it.logoPath)+L"\r\n";
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

//  Pre-made templates offered in the designer dropdown.
static const wchar_t* TEMPLATE_NAMES[] = {
    L"پیش‌فرض حرفه‌ای",
    L"ساده (فقط متن)",
    L"رسمی با کادر",
    L"فشرده A5",
};
static const int N_TEMPLATES = sizeof(TEMPLATE_NAMES)/sizeof(TEMPLATE_NAMES[0]);

static Design templateByIndex(int sec, int idx){
    Design d;
    double cw,ch;
    switch(idx){
    case 1: { // simple text only
        d.paper=1; paperMM(d.paper,cw,ch);
        auto add=[&](double y,const wchar_t* tok,const wchar_t* t,int sz,bool b){
            DItem it; it.kind=IT_LABEL; it.x=8; it.y=y; it.w=cw-16; it.h=sz*0.45;
            it.field=tok?tok:L""; it.text=t?t:L""; it.fontSize=sz; it.bold=b;
            d.items.push_back(it); };
        add(10,NULL,(std::wstring(L"درمانگاه ")+APP_NAME_W).c_str(),16,true);
        add(20,NULL,PRINT_SECTIONS[sec<N_PRINT_SECTIONS?sec:0],12,true);
        add(34,L"{queue}",L"نوبت: ",13,true);
        add(44,L"{full}",L"بیمار: ",12,true);
        add(52,L"{nid}",L"کد ملی: ",11,false);
        add(60,L"{date}",L"تاریخ: ",11,false);
        add(68,L"{paid}",L"پرداختی: ",12,true);
        add(80,L"{issued}",L"",10,false);
        return d; }
    case 2: { // formal with border
        d = defaultDesign(sec); d.paper=0; // A4
        for(auto& it:d.items){ it.y*=1.2; it.x*=1.3; }
        return d; }
    case 3: { // compact A5
        d.paper=1; paperMM(d.paper,cw,ch);
        auto add=[&](double x,double y,const wchar_t* tok,const wchar_t* t,int sz,bool b){
            DItem it; it.kind=IT_LABEL; it.x=x; it.y=y; it.w=cw-2*x; it.h=sz*0.45;
            it.field=tok?tok:L""; it.text=t?t:L""; it.fontSize=sz; it.bold=b;
            d.items.push_back(it); };
        add(6,8,NULL,(std::wstring(L"درمانگاه ")+APP_NAME_W).c_str(),13,true);
        add(6,18,L"{queue}",L"نوبت: ",12,true);
        add(60,18,L"{date}",L"تاریخ: ",10,false);
        add(6,26,L"{full}",L"",12,true);
        add(6,34,L"{nid}",L"کد ملی: ",10,false);
        add(6,42,L"{paid}",L"پرداختی: ",11,true);
        add(6,52,L"{issued}",L"",9,false);
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

// =================================================== printer settings dialog =
#define PS_CLASS L"AzPrinterCfg"
enum {
    PSB_CLOSE=1, PSB_TEST, PSB_ADV, PSB_DESIGN, PSB_A4, PSB_A5,
    PSB_FIT, PSB_FILL, PSB_PRINTER_BASE=200
};

struct PrnState {
    HWND owner;
    int  hot;
    std::vector<std::wstring> printers;
    std::wstring sel;       // selected printer
    int  paper;             // 0 A4 / 1 A5
    int  mode;              // 0 fit / 1 fill
    int  section;           // section being edited (for the design button)
};
static HWND s_prn=NULL;
static PrnState* s_ps=NULL;

static int prnCardW(){ return S(560); }
static int prnCardH(){ return S(560); }
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
    // paper toggles
    SetTextColor(dc,g_theme.textDim); SelectObject(dc,g_fSmall);
    RECT pl={c.left+S(20),by,c.right-S(20),by+S(20)};
    DrawTextW(dc,L"اندازهٔ کاغذ و حالت تطبیق:",-1,&pl,
        DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
    by+=S(24);
    auto chip=[&](int id,const wchar_t* t,RECT r,bool on){
        bool hov=(s_ps->hot==id);
        gpRoundRect(dc,r,S(9),on?g_theme.accent:(hov?g_theme.hover:g_theme.surface2),
            on?g_theme.accent:g_theme.border,255);
        SetTextColor(dc,on?g_theme.accentText:g_theme.text);
        SelectObject(dc,g_fUIB);
        DrawTextW(dc,t,-1,&r,DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
    };
    int cw4=(c.right-c.left-S(52))/4, cx=c.right-S(20)-cw4;
    RECT rA4={cx,by,cx+cw4,by+S(34)};            chip(PSB_A4,L"A4",rA4,s_ps->paper==0);
    cx-=cw4+S(4);
    RECT rA5={cx,by,cx+cw4,by+S(34)};            chip(PSB_A5,L"A5",rA5,s_ps->paper==1);
    cx-=cw4+S(4);
    RECT rFit={cx,by,cx+cw4,by+S(34)};           chip(PSB_FIT,L"متناسب",rFit,s_ps->mode==0);
    cx-=cw4+S(4);
    RECT rFill={cx,by,cx+cw4,by+S(34)};          chip(PSB_FILL,L"پرکردن",rFill,s_ps->mode==1);
    by+=S(46);

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
    int bw=(c.right-c.left-S(52))/2;
    RECT rTest={c.right-S(20)-bw,by,c.right-S(20),by+S(44)};
    btn(PSB_TEST,L"تست اتصال و چاپ",ICO_PRINT,rTest,false);
    RECT rAdv={c.left+S(20),by,c.left+S(20)+bw,by+S(44)};
    btn(PSB_ADV,L"تنظیمات پیشرفتهٔ درایور",ICO_GEAR,rAdv,false);
    by+=S(54);
    RECT rDes={c.left+S(20),by,c.right-S(20),by+S(48)};
    std::wstring dl=std::wstring(L"طراحی و تنظیم چاپ بخش: ")+
        PRINT_SECTIONS[s_ps->section<N_PRINT_SECTIONS?s_ps->section:0];
    btn(PSB_DESIGN,dl.c_str(),ICO_RECEIPT,rDes,true);
    by+=S(58);

    // hint
    SetTextColor(dc,g_theme.textDim); SelectObject(dc,g_fSmall);
    RECT hr={c.left+S(20),by,c.right-S(20),by+S(40)};
    DrawTextW(dc,L"چاپگر انتخاب‌شده حتی اگر پیش‌فرض ویندوز تغییر کند، ثابت می‌ماند.",
        -1,&hr,DT_RIGHT|DT_WORDBREAK|DT_RTLREADING|DT_NOPREFIX);

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
    int by=ly+maxRows*lh+S(8)+S(24);
    int cw4=(c.right-c.left-S(52))/4, cx=c.right-S(20)-cw4;
    RECT rA4={cx,by,cx+cw4,by+S(34)};   if(PtInRect(&rA4,pt)) return PSB_A4;
    cx-=cw4+S(4); RECT rA5={cx,by,cx+cw4,by+S(34)}; if(PtInRect(&rA5,pt)) return PSB_A5;
    cx-=cw4+S(4); RECT rFit={cx,by,cx+cw4,by+S(34)}; if(PtInRect(&rFit,pt)) return PSB_FIT;
    cx-=cw4+S(4); RECT rFill={cx,by,cx+cw4,by+S(34)}; if(PtInRect(&rFill,pt)) return PSB_FILL;
    by+=S(46);
    int bw=(c.right-c.left-S(52))/2;
    RECT rTest={c.right-S(20)-bw,by,c.right-S(20),by+S(44)}; if(PtInRect(&rTest,pt)) return PSB_TEST;
    RECT rAdv={c.left+S(20),by,c.left+S(20)+bw,by+S(44)};    if(PtInRect(&rAdv,pt)) return PSB_ADV;
    by+=S(54);
    RECT rDes={c.left+S(20),by,c.right-S(20),by+S(48)};       if(PtInRect(&rDes,pt)) return PSB_DESIGN;
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
                s_ps->sel=s_ps->printers[i];
                setSetting(L"printer_name",s_ps->sel);
                InvalidateRect(h,NULL,FALSE);
            }
            return 0;
        }
        switch(id){
        case PSB_A4: s_ps->paper=0; setSetting(L"paper_size",L"A4"); InvalidateRect(h,NULL,FALSE); break;
        case PSB_A5: s_ps->paper=1; setSetting(L"paper_size",L"A5"); InvalidateRect(h,NULL,FALSE); break;
        case PSB_FIT: s_ps->mode=0; setSetting(L"print_mode",L"fit"); InvalidateRect(h,NULL,FALSE); break;
        case PSB_FILL:s_ps->mode=1; setSetting(L"print_mode",L"fill");InvalidateRect(h,NULL,FALSE); break;
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
    s_ps->paper=(getSetting(L"paper_size",L"A5")==L"A4")?0:1;
    s_ps->mode =(getSetting(L"print_mode",L"fit")==L"fill")?1:0;
    s_ps->section=0;
    s_prn=CreateWindowExW(WS_EX_TOPMOST,PS_CLASS,L"",
        WS_POPUP|WS_VISIBLE|WS_CLIPCHILDREN,
        org.x,org.y,rc.right,rc.bottom,owner,NULL,g_hInst,NULL);
    BringWindowToTop(s_prn); SetFocus(s_prn);
}

// ============================================================================
//  PRINT DESIGNER  (separate file for clarity)
// ============================================================================
#include "printer_designer.inc"

// ============================================================================
//  RENDER A SAVED DESIGN ONTO A PRINTER DC
// ============================================================================
static std::wstring fieldValue(const ReceptionRecord& r, const std::wstring& tok){
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
    StartPage(dc);
    SetBkMode(dc,TRANSPARENT);

    for(const DItem& it: d.items){
        if(it.kind==IT_LABEL){
            std::wstring s=itemText(r,it);
            if(s.empty()) continue;
            int px=mmX(it.x), py=mmY(it.y);
            int lf=-(int)(it.fontSize*dpiY/72.0);
            HFONT f=CreateFontW(lf,0,0,0,it.bold?FW_BOLD:FW_NORMAL,
                it.italic?1:0,it.underline?1:0,it.strike?1:0,DEFAULT_CHARSET,
                0,0,CLEARTYPE_QUALITY,0,
                it.fontName.empty()?L"Vazirmatn":it.fontName.c_str());
            HGDIOBJ of=SelectObject(dc,f);
            SetTextColor(dc,it.color);
            int wpx=(int)(it.w*sx); if(wpx<10) wpx=horz;
            RECT rr={px,py,px+wpx,py+lf*-3};
            DrawTextW(dc,s.c_str(),-1,&rr,
                DT_RIGHT|DT_TOP|DT_WORDBREAK|DT_RTLREADING|DT_NOPREFIX);
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

    EndPage(dc); EndDoc(dc); DeleteDC(dc);
    logLine(L"designed receipt printed for section "+std::to_wstring(sectionIdx));
    return true;
}
