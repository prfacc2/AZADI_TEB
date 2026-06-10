// ============================================================================
//  billing.cpp — Iranian insurance definitions, reception persistence,
//                last receipt, printing (real GDI printer output)
// ============================================================================
#include "app.h"
#include <stdio.h>

// ----------------------------------------------------- Iranian insurances --
//  pct = سهم سازمان بیمه‌گر (درصد پوشش پایه برای خدمات سرپایی دولتی)
const InsuranceDef INSURANCES[] = {
    { L"آزاد (بدون بیمه)",          0  },
    { L"تأمین اجتماعی",             70 },
    { L"بیمه سلامت ایرانیان",        70 },
    { L"بیمه سلامت روستایی",         90 },
    { L"بیمه سلامت کارکنان دولت",    70 },
    { L"نیروهای مسلح",              90 },
    { L"کمیته امداد امام خمینی",     100},
};
const int N_INSURANCES = sizeof(INSURANCES)/sizeof(INSURANCES[0]);

const InsuranceDef SUPP_INSURANCES[] = {
    { L"ندارد",            0  },
    { L"بیمه ایران",        70 },
    { L"بیمه آسیا",         70 },
    { L"بیمه دانا",         70 },
    { L"بیمه البرز",        70 },
    { L"بیمه پاسارگاد",     80 },
    { L"بیمه ملت",          70 },
    { L"بیمه کوثر",         80 },
    { L"بیمه دی",           90 },
    { L"بیمه SOS",          80 },
};
const int N_SUPP = sizeof(SUPP_INSURANCES)/sizeof(SUPP_INSURANCES[0]);

// ------------------------------------------------------------- tariffs -----
//  Base service tariff per visit type (Rial). Editable; can later be loaded
//  from data\tariffs.ini for server-side configuration.
const long long VISIT_TARIFF[3] = {
    2'500'000,   // عادی   (ویزیت عمومی)
    3'500'000,   // سرپایی (خدمت سرپایی)
    8'000'000,   // بستری  (خدمت بستری پایه)
};
long long applyApptTariff(long long base, int apptType){
    switch(apptType){
        case 1: return base * 150 / 100;   // اورژانس: +۵۰٪
        case 2: return base *  50 / 100;   // پرسنلی: نصف تعرفه
        default:return base;               // عادی
    }
}
long long defaultServicePrice(int patientType, int apptType){
    int p = (patientType>=0 && patientType<3) ? patientType : 0;
    return applyApptTariff(VISIT_TARIFF[p], apptType);
}

// ------------------------------------------------------------ persistence --
static std::wstring recPath(){
    SYSTEMTIME st = iranNow();
    int jy,jm,jd; gregToJalali(st.wYear,st.wMonth,st.wDay,jy,jm,jd);
    wchar_t f[64]; swprintf(f,64,L"\\receptions_%04d-%02d-%02d.csv",jy,jm,jd);
    return dataDir()+f;
}
int countTodayReceptions(){
    std::wstring all = readFileUtf8(recPath());
    int n=0;
    for(wchar_t c : all) if(c==L'\n') n++;
    return n>0 ? n-1 : 0;   // minus header
}
static std::wstring csvEsc(const std::wstring& s){
    std::wstring o = s;
    for(auto& c : o) if(c==L',') c=L'،';
    return o;
}
int saveReception(ReceptionRecord& r){
    SYSTEMTIME st = iranNow();
    r.apptDate = jalaliDateShort(st);
    r.apptTime = iranTimeStr(st, true);
    bool isNew = GetFileAttributesW(recPath().c_str())==INVALID_FILE_ATTRIBUTES;
    r.queueNo = countTodayReceptions() + 1;
    std::wstring row;
    if(isNew)
        row += L"\uFEFFنوبت,نام,نام خانوادگی,کد ملی,نام پدر,تاریخ تولد,جنسیت,تلفن,ثابت,آدرس,"
               L"نوع بیمار,بیمه,بیمه مکمل,تاریخ,ساعت,شیفت,بخش,کاربر,"
               L"جمع کل,سهم بیمه,سهم بیمار,مابه‌التفاوت,سهم سازمان,تخفیف,پرداختی\r\n";
    wchar_t nums[256];
    swprintf(nums,256,L"%lld,%lld,%lld,%lld,%lld,%lld,%lld",
        r.total,r.mainShare,r.patientShare,r.baseDiff,r.orgShare,r.discount,r.paid);
    wchar_t qn[16]; swprintf(qn,16,L"%d",r.queueNo);
    row += std::wstring(qn)+L","+csvEsc(r.firstName)+L","+csvEsc(r.lastName)+L","
        + csvEsc(r.nationalId)+L","+csvEsc(r.fatherName)+L","+csvEsc(r.birthDate)+L","
        + csvEsc(r.gender)+L","+csvEsc(r.mobile)+L","+csvEsc(r.landline)+L","
        + csvEsc(r.address)+L","+csvEsc(r.patientType)+L","+csvEsc(r.insurance)+L","
        + csvEsc(r.suppInsurance)+L","+r.apptDate+L","+r.apptTime+L","
        + csvEsc(r.shift)+L","+csvEsc(r.dept)+L","+csvEsc(r.userName)+L","+nums+L"\r\n";
    writeFileUtf8(recPath(), row, true);
    logLine(L"reception saved #" + std::wstring(qn) + L" " + r.firstName + L" " + r.lastName);
    saveLastReceipt(r);
    return r.queueNo;
}

// -------------------------------------------------------- last receipt -----
static std::wstring lastPath(){ return dataDir()+L"\\last_receipt.dat"; }
void saveLastReceipt(const ReceptionRecord& r){
    wchar_t nums[512];
    swprintf(nums,512,L"%lld\n%lld\n%lld\n%lld\n%lld\n%lld\n%lld\n%lld\n%d",
        r.total,r.mainShare,r.patientShare,r.baseDiff,r.orgShare,
        r.finalTotal,r.discount,r.paid,r.queueNo);
    std::wstring s = r.firstName+L"\n"+r.lastName+L"\n"+r.nationalId+L"\n"
        +r.fatherName+L"\n"+r.birthDate+L"\n"+r.gender+L"\n"+r.mobile+L"\n"
        +r.landline+L"\n"+r.address+L"\n"+r.patientType+L"\n"+r.insurance+L"\n"
        +r.suppInsurance+L"\n"+r.apptDate+L"\n"+r.apptTime+L"\n"+r.shift+L"\n"
        +r.dept+L"\n"+r.userName+L"\n"+nums;
    writeFileUtf8(lastPath(), s, false);
}
bool loadLastReceipt(ReceptionRecord& r){
    std::wstring all = readFileUtf8(lastPath());
    if(all.empty()) return false;
    std::vector<std::wstring> f; size_t pos=0;
    while(pos <= all.size()){
        size_t e = all.find(L'\n',pos);
        if(e==std::wstring::npos){ f.push_back(trim(all.substr(pos))); break; }
        f.push_back(trim(all.substr(pos,e-pos))); pos=e+1;
    }
    if(f.size() < 26) return false;
    r.firstName=f[0]; r.lastName=f[1]; r.nationalId=f[2]; r.fatherName=f[3];
    r.birthDate=f[4]; r.gender=f[5]; r.mobile=f[6]; r.landline=f[7];
    r.address=f[8]; r.patientType=f[9]; r.insurance=f[10]; r.suppInsurance=f[11];
    r.apptDate=f[12]; r.apptTime=f[13]; r.shift=f[14]; r.dept=f[15]; r.userName=f[16];
    r.total=_wtoi64(f[17].c_str()); r.mainShare=_wtoi64(f[18].c_str());
    r.patientShare=_wtoi64(f[19].c_str()); r.baseDiff=_wtoi64(f[20].c_str());
    r.orgShare=_wtoi64(f[21].c_str()); r.finalTotal=_wtoi64(f[22].c_str());
    r.discount=_wtoi64(f[23].c_str()); r.paid=_wtoi64(f[24].c_str());
    r.queueNo=_wtoi(f[25].c_str());
    return true;
}

// =============================================================== PRINTING ==
//  Real GDI printing to the system default / chosen printer.
static void pLine(HDC dc, int& y, int x, int w, const std::wstring& s,
                  HFONT f, bool center=false){
    HGDIOBJ of = SelectObject(dc, f);
    RECT rc = {x, y, x+w, y+1000};
    DrawTextW(dc, s.c_str(), -1, &rc,
        DT_CALCRECT|DT_WORDBREAK|DT_RTLREADING|DT_NOPREFIX);
    int hgt = rc.bottom - rc.top;
    rc = {x, y, x+w, y+hgt};
    DrawTextW(dc, s.c_str(), -1, &rc,
        (center?DT_CENTER:DT_RIGHT)|DT_WORDBREAK|DT_RTLREADING|DT_NOPREFIX);
    y += hgt + 8;
    SelectObject(dc, of);
}
static void pSep(HDC dc, int& y, int x, int w){
    HPEN p = CreatePen(PS_DOT,1,RGB(0,0,0));
    HGDIOBJ o = SelectObject(dc,p);
    MoveToEx(dc,x,y,0); LineTo(dc,x+w,y);
    SelectObject(dc,o); DeleteObject(p);
    y += 14;
}
bool printReceipt(const ReceptionRecord& r, int kind, HWND owner){
    PRINTDLGW pd = {0};
    pd.lStructSize = sizeof(pd);
    pd.hwndOwner   = owner;
    pd.Flags = PD_RETURNDC | PD_NOPAGENUMS | PD_NOSELECTION | PD_USEDEVMODECOPIES;
    if(!PrintDlgW(&pd)){
        // fallback: default printer without dialog
        wchar_t prn[256]; DWORD sz=256;
        if(!GetDefaultPrinterW(prn,&sz)){
            MessageBoxW(owner, L"هیچ پرینتری روی سیستم پیدا نشد.",
                L"چاپ", MB_OK|MB_ICONWARNING);
            return false;
        }
        pd.hDC = CreateDCW(L"WINSPOOL", prn, NULL, NULL);
        if(!pd.hDC) return false;
    }
    HDC dc = pd.hDC;

    int dpiY = GetDeviceCaps(dc, LOGPIXELSY);
    int pw   = GetDeviceCaps(dc, HORZRES);
    int marg = dpiY/2;
    int w    = pw - 2*marg;

    HFONT fT = CreateFontW(-(dpiY*16/72),0,0,0,FW_BOLD,0,0,0,DEFAULT_CHARSET,
        0,0,CLEARTYPE_QUALITY,0,L"Vazirmatn");
    HFONT fN = CreateFontW(-(dpiY*11/72),0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,
        0,0,CLEARTYPE_QUALITY,0,L"Vazirmatn");
    HFONT fB = CreateFontW(-(dpiY*12/72),0,0,0,FW_BOLD,0,0,0,DEFAULT_CHARSET,
        0,0,CLEARTYPE_QUALITY,0,L"Vazirmatn");

    DOCINFOW di = { sizeof(di) };
    const wchar_t* names[3] = { L"رسید بیمه", L"نسخه پزشک", L"قبض پذیرش" };
    std::wstring docName = std::wstring(APP_NAME_W) + L" — " + names[kind];
    di.lpszDocName = docName.c_str();

    if(StartDocW(dc,&di) <= 0){ DeleteDC(dc); return false; }
    StartPage(dc);
    SetBkMode(dc, TRANSPARENT);
    SetTextAlign(dc, TA_RIGHT|TA_RTLREADING);

    int y = marg, x = marg;
    wchar_t buf[512];

    pLine(dc,y,x,w, std::wstring(L"درمانگاه ") + APP_NAME_W, fT, true);
    pLine(dc,y,x,w, names[kind], fB, true);
    pSep(dc,y,x,w);
    swprintf(buf,512,L"شماره نوبت: %d        تاریخ: %s        ساعت: %s",
        r.queueNo, toFaDigits(r.apptDate).c_str(), toFaDigits(r.apptTime).c_str());
    pLine(dc,y,x,w, buf, fN);
    swprintf(buf,512,L"شیفت: %s        بخش: %s        کاربر: %s",
        r.shift.c_str(), r.dept.c_str(), r.userName.c_str());
    pLine(dc,y,x,w, buf, fN);
    pSep(dc,y,x,w);
    swprintf(buf,512,L"بیمار: %s %s        کد ملی: %s",
        r.firstName.c_str(), r.lastName.c_str(), toFaDigits(r.nationalId).c_str());
    pLine(dc,y,x,w, buf, fB);
    swprintf(buf,512,L"نام پدر: %s        تاریخ تولد: %s        جنسیت: %s",
        r.fatherName.c_str(), toFaDigits(r.birthDate).c_str(), r.gender.c_str());
    pLine(dc,y,x,w, buf, fN);
    swprintf(buf,512,L"تلفن: %s        ثابت: %s",
        toFaDigits(r.mobile).c_str(), toFaDigits(r.landline).c_str());
    pLine(dc,y,x,w, buf, fN);
    if(!r.address.empty())
        pLine(dc,y,x,w, L"آدرس: " + r.address, fN);
    pSep(dc,y,x,w);
    swprintf(buf,512,L"نوع بیمار: %s        نوع نوبت ثبت‌شده", r.patientType.c_str());
    pLine(dc,y,x,w, L"نوع بیمار: " + r.patientType, fN);
    pLine(dc,y,x,w, L"بیمه اصلی: " + r.insurance +
                    L"        بیمه مکمل: " + r.suppInsurance, fN);
    pSep(dc,y,x,w);
    if(kind != 1){  // مالی — رسید بیمه و قبض
        pLine(dc,y,x,w, L"جمع کل: " + toFaDigits(formatMoney(r.total)) + L" ریال", fN);
        pLine(dc,y,x,w, L"سهم بیمه اصلی: " + toFaDigits(formatMoney(r.mainShare)) + L" ریال", fN);
        pLine(dc,y,x,w, L"مابه‌التفاوت پایه: " + toFaDigits(formatMoney(r.baseDiff)) + L" ریال", fN);
        pLine(dc,y,x,w, L"سهم سازمان (مکمل): " + toFaDigits(formatMoney(r.orgShare)) + L" ریال", fN);
        pLine(dc,y,x,w, L"سهم بیمار: " + toFaDigits(formatMoney(r.patientShare)) + L" ریال", fN);
        pLine(dc,y,x,w, L"تخفیف: " + toFaDigits(formatMoney(r.discount)) + L" ریال", fN);
        pSep(dc,y,x,w);
        pLine(dc,y,x,w, L"مبلغ قابل پرداخت: " + toFaDigits(formatMoney(r.paid)) + L" ریال", fB);
    } else {        // نسخه — فضای نسخه‌نویسی
        pLine(dc,y,x,w, L"شرح نسخه / دستور پزشک:", fB);
        for(int i=0;i<8;i++){ y += dpiY/3; pSep(dc,y,x,w); }
        pLine(dc,y,x,w, L"امضا و مهر پزشک", fN);
    }
    y += 10;
    pLine(dc,y,x,w, L"نرم‌افزار آزادی طب — این رسید را نزد خود نگه دارید", fN, true);

    EndPage(dc);
    EndDoc(dc);
    DeleteObject(fT); DeleteObject(fN); DeleteObject(fB);
    DeleteDC(dc);
    logLine(L"printed: " + std::wstring(names[kind]));
    return true;
}
bool printLastReceipt(HWND owner){
    ReceptionRecord r;
    if(!loadLastReceipt(r)){
        MessageBoxW(owner, L"هنوز قبضی ثبت نشده است.", L"چاپ آخرین قبض",
            MB_OK|MB_ICONINFORMATION);
        return false;
    }
    return printReceipt(r, 2, owner);
}
