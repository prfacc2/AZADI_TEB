// ============================================================================
//  data_ext.cpp  (v1.6.0)
//  Extended data layer added for the appointment (نوبت‌دهی) module and the
//  upgraded reception screen:
//    • A deterministic OFFLINE simulation of the Iranian Civil Registry
//      (ثبت احوال) + insurance enquiry — real 10-digit checksum, then a stable
//      identity derived from the code so the whole workflow runs with NO
//      external API and is trivially swapped for a real web-service later.
//    • Doctors + their services (file-backed, seeded with realistic specialties)
//    • Appointment store (CSV-like .dat, queue numbers, cancel/edit/search)
//    • Profile-change request workflow (reception → management approval)
//    • Cartable v2 actions: pin / seen-one / delete-one (each reported back)
//
//  Everything is file-backed under data\ so the program stays a single static
//  EXE and is ready to migrate to a DB/REST backend without touching the UI.
// ============================================================================
#include "app.h"
#include <stdio.h>
#include <algorithm>

// ---- shared little helpers (kept local to avoid clashing with employees.cpp)
static std::vector<std::wstring> dx_split(const std::wstring& s, wchar_t sep){
    std::vector<std::wstring> out; size_t pos=0;
    while(true){ size_t e=s.find(sep,pos);
        if(e==std::wstring::npos){ out.push_back(s.substr(pos)); break; }
        out.push_back(s.substr(pos,e-pos)); pos=e+1; }
    return out;
}
static std::wstring dx_esc(const std::wstring& s){
    std::wstring o=s; for(auto&c:o) if(c==L'|'||c==L'\n'||c==L'\r') c=L' '; return o;
}

// ============================================================================
//  NATIONAL REGISTRY (ثبت احوال) — offline deterministic simulation
// ============================================================================
bool validNationalId(const std::wstring& id){
    if(id.size()!=10) return false;
    for(wchar_t c:id) if(c<L'0'||c>L'9') return false;
    bool allSame=true; for(wchar_t c:id) if(c!=id[0]){ allSame=false; break; }
    if(allSame) return false;
    int sum=0; for(int i=0;i<9;i++) sum+=(id[i]-L'0')*(10-i);
    int rem=sum%11, chk=id[9]-L'0';
    return (rem<2)? (chk==rem) : (chk==11-rem);
}

//  pools of realistic Iranian names so the derived identity looks natural
static const wchar_t* MALE_NAMES[] = {
    L"محمد",L"علی",L"رضا",L"حسین",L"مهدی",L"امیر",L"سعید",L"حسن",L"محسن",
    L"احمد",L"ابوالفضل",L"یاسر",L"کامران",L"بهروز",L"فرهاد",L"سجاد" };
static const wchar_t* FEMALE_NAMES[] = {
    L"فاطمه",L"زهرا",L"مریم",L"لیلا",L"سمیرا",L"نرگس",L"الهام",L"سارا",
    L"مینا",L"شیما",L"نسرین",L"پریسا",L"معصومه",L"راضیه",L"آرزو",L"هانیه" };
static const wchar_t* FAMILY[] = {
    L"احمدی",L"محمدی",L"حسینی",L"رضایی",L"کریمی",L"موسوی",L"جعفری",L"اکبری",
    L"ایزدپناه",L"حسن‌پور",L"صادقی",L"نوری",L"رحیمی",L"قاسمی",L"یوسفی",
    L"باقری",L"مرادی",L"شریفی",L"کاظمی",L"عباسی" };

static int hashNid(const std::wstring& nid){
    unsigned h=2166136261u;
    for(wchar_t c:nid){ h^=(unsigned)c; h*=16777619u; }
    return (int)(h & 0x7fffffff);
}

CitizenInfo lookupCitizen(const std::wstring& nationalId){
    CitizenInfo c;
    if(!validNationalId(nationalId)) return c;        // c.found stays false
    c.found=true;
    int h=hashNid(nationalId);
    bool male = ((h>>3)&1)==0;
    c.gender = male ? L"مرد" : L"زن";
    if(male) c.firstName = MALE_NAMES[h % (sizeof(MALE_NAMES)/sizeof(*MALE_NAMES))];
    else     c.firstName = FEMALE_NAMES[(h/7) % (sizeof(FEMALE_NAMES)/sizeof(*FEMALE_NAMES))];
    c.lastName   = FAMILY[(h/13) % (sizeof(FAMILY)/sizeof(*FAMILY))];
    c.fatherName = MALE_NAMES[(h/29) % (sizeof(MALE_NAMES)/sizeof(*MALE_NAMES))];
    // a plausible Jalali birth date (year 1340..1399)
    int by = 1340 + (h % 60);
    int bm = 1 + ((h/61) % 12);
    int bd = 1 + ((h/733) % 28);
    wchar_t bb[16]; swprintf(bb,16,L"%04d/%02d/%02d",by,bm,bd);
    c.birthDate = bb;
    // a plausible mobile number 09xxxxxxxxx
    wchar_t mb[16];
    swprintf(mb,16,L"09%09d",(h % 1000000000));
    c.mobile = mb;
    // ---- insurance(s): MOST people carry exactly one basic insurance; some
    //      (deterministically) carry two or three so the multi-insurance UX is
    //      exercised. Index 0 (آزاد) is never returned here.
    int last = nationalId[9]-L'0';
    int primary;
    if(last<=3) primary=1;          // تأمین اجتماعی
    else if(last<=6) primary=2;     // بیمه سلامت ایرانیان
    else if(last<=7) primary=3;     // روستایی
    else if(last<=8) primary=5;     // نیروهای مسلح
    else primary=4;                 // کارکنان دولت
    c.insurances.push_back(primary);
    int extra = (h/97) % 10;        // ~30% have a second, ~10% a third
    if(extra<3){
        int second = 2 + ((h/131) % 4);   // 2..5
        if(second!=primary) c.insurances.push_back(second);
        if(extra==0){
            int third = 1 + ((h/271) % 5); // 1..5
            bool dup=false; for(int x:c.insurances) if(x==third) dup=true;
            if(!dup) c.insurances.push_back(third);
        }
    }
    return c;
}

// ============================================================================
//  DOCTORS
// ============================================================================
static std::wstring docsPath(){ return dataDir()+L"\\doctors.dat"; }
//  file format per line: name|specialty|service1;service2;service3
static void seedDoctors(){
    struct D{ const wchar_t* n; const wchar_t* sp; const wchar_t* sv; };
    static const D defs[] = {
        { L"دکتر علی رضایی",      L"رادیولوژی",   L"رادیوگرافی ساده;سونوگرافی;سی‌تی‌اسکن" },
        { L"دکتر مریم حسینی",     L"داخلی",       L"ویزیت داخلی;نوار قلب;مشاوره" },
        { L"دکتر رضا محمدی",      L"دندانپزشکی",  L"معاینه;جرم‌گیری;ترمیم;عصب‌کشی" },
        { L"دکتر زهرا کریمی",     L"زنان و زایمان",L"ویزیت;سونوگرافی;پاپ‌اسمیر" },
        { L"دکتر حسین اکبری",     L"اطفال",        L"ویزیت کودک;واکسیناسیون;مشاوره تغذیه" },
        { L"دکتر سارا موسوی",     L"پوست و مو",    L"ویزیت پوست;لیزر;مزوتراپی" },
        { L"دکتر مهدی صادقی",     L"ارتوپدی",      L"ویزیت;گچ‌گیری;تزریق مفصل" },
        { L"دکتر لیلا نوری",      L"چشم‌پزشکی",    L"معاینه بینایی;اپتومتری;لیزیک" },
    };
    std::wstring out;
    for(auto&d:defs)
        out += std::wstring(d.n)+L"|"+d.sp+L"|"+d.sv+L"\r\n";
    writeFileUtf8(docsPath(),out,false);
}
std::vector<DoctorDef> loadDoctors(){
    std::wstring all=readFileUtf8(docsPath());
    if(trim(all).empty()){ seedDoctors(); all=readFileUtf8(docsPath()); }
    std::vector<DoctorDef> out;
    size_t pos=0;
    while(pos<all.size()){
        size_t e=all.find(L'\n',pos); if(e==std::wstring::npos) e=all.size();
        std::wstring line=trim(all.substr(pos,e-pos)); pos=e+1;
        if(line.empty()) continue;
        auto f=dx_split(line,L'|');
        if(f.size()<3) continue;
        DoctorDef d; d.name=f[0]; d.specialty=f[1];
        for(auto&s:dx_split(f[2],L';')) if(!trim(s).empty()) d.services.push_back(trim(s));
        out.push_back(d);
    }
    return out;
}
std::vector<DoctorDef> todaysDoctors(){
    // deterministic subset "on shift today" based on the Jalali day number so
    // the «پزشکان امروز» button shows a stable, realistic roster each day.
    auto all=loadDoctors();
    SYSTEMTIME st=iranNow();
    int jy,jm,jd; gregToJalali(st.wYear,st.wMonth,st.wDay,jy,jm,jd);
    std::vector<DoctorDef> out;
    for(size_t i=0;i<all.size();i++)
        if(((int)i + jd) % 2 == 0) out.push_back(all[i]);
    if(out.empty() && !all.empty()) out.push_back(all[0]);
    return out;
}

// ============================================================================
//  APPOINTMENTS
// ============================================================================
static std::wstring apptPath(){ return dataDir()+L"\\appointments.dat"; }
//  per line: nid|first|last|mobile|doctor|service|date|time|day|shift|kind|user|queue|cancelled
static Appointment parseAppt(const std::wstring& line){
    Appointment a;
    auto f=dx_split(line,L'|');
    if(f.size()<14) return a;
    a.nationalId=f[0]; a.firstName=f[1]; a.lastName=f[2]; a.mobile=f[3];
    a.doctor=f[4]; a.service=f[5]; a.apptDate=f[6]; a.apptTime=f[7];
    a.day=f[8]; a.shift=f[9]; a.kind=f[10]; a.user=f[11];
    a.queueNo=_wtoi(f[12].c_str()); a.cancelled=(f[13]==L"1");
    return a;
}
static std::wstring serializeAppt(const Appointment& a){
    wchar_t qn[16]; swprintf(qn,16,L"%d",a.queueNo);
    return dx_esc(a.nationalId)+L"|"+dx_esc(a.firstName)+L"|"+dx_esc(a.lastName)+L"|"
        + dx_esc(a.mobile)+L"|"+dx_esc(a.doctor)+L"|"+dx_esc(a.service)+L"|"
        + dx_esc(a.apptDate)+L"|"+dx_esc(a.apptTime)+L"|"+dx_esc(a.day)+L"|"
        + dx_esc(a.shift)+L"|"+dx_esc(a.kind)+L"|"+dx_esc(a.user)+L"|"
        + std::wstring(qn)+L"|"+(a.cancelled?L"1":L"0");
}
std::vector<Appointment> loadAppointments(bool includeCancelled){
    std::vector<Appointment> out;
    std::wstring all=readFileUtf8(apptPath());
    size_t pos=0;
    while(pos<all.size()){
        size_t e=all.find(L'\n',pos); if(e==std::wstring::npos) e=all.size();
        std::wstring line=trim(all.substr(pos,e-pos)); pos=e+1;
        if(line.empty()) continue;
        Appointment a=parseAppt(line);
        if(a.queueNo==0 && a.nationalId.empty() && a.lastName.empty()) continue;
        if(!includeCancelled && a.cancelled) continue;
        out.push_back(a);
    }
    return out;
}
static void saveAllAppts(const std::vector<Appointment>& v){
    std::wstring out;
    for(auto&a:v) out+=serializeAppt(a)+L"\r\n";
    writeFileUtf8(apptPath(),out,false);
}
static const wchar_t* weekdayName(const SYSTEMTIME& st){
    static const wchar_t* days[7]={L"یکشنبه",L"دوشنبه",L"سه‌شنبه",
        L"چهارشنبه",L"پنجشنبه",L"جمعه",L"شنبه"};
    return days[st.wDayOfWeek];   // 0=Sunday
}
int saveAppointment(Appointment& a){
    auto v=loadAppointments(true);
    int maxQ=0; for(auto&x:v) if(!x.cancelled && x.queueNo>maxQ) maxQ=x.queueNo;
    a.queueNo=maxQ+1;
    if(a.apptDate.empty()){ SYSTEMTIME st=iranNow(); a.apptDate=jalaliDateShort(st); }
    if(a.apptTime.empty()){ SYSTEMTIME st=iranNow(); a.apptTime=iranTimeStr(st,false); }
    if(a.day.empty()){ SYSTEMTIME st=iranNow(); a.day=weekdayName(st); }
    if(a.shift.empty()) a.shift=shiftName(g_session.shift);
    if(a.kind.empty()) a.kind=L"حضوری";
    if(a.user.empty()) a.user=g_session.user.username;
    v.push_back(a);
    saveAllAppts(v);
    logLine(L"appointment saved q="+std::to_wstring(a.queueNo));
    return a.queueNo;
}
bool cancelAppointment(int index){
    auto v=loadAppointments(true);
    if(index<0||index>=(int)v.size()) return false;
    v[index].cancelled=true; saveAllAppts(v); return true;
}
bool updateAppointment(int index, const Appointment& a){
    auto v=loadAppointments(true);
    if(index<0||index>=(int)v.size()) return false;
    int q=v[index].queueNo; v[index]=a; v[index].queueNo=q;
    saveAllAppts(v); return true;
}
static bool icontains(const std::wstring& hay, const std::wstring& needle){
    if(needle.empty()) return true;
    return hay.find(needle)!=std::wstring::npos;
}
std::vector<Appointment> searchAppointments(const std::wstring& nid,
        const std::wstring& mobile, const std::wstring& fn,
        const std::wstring& ln, bool includeCancelled){
    std::vector<Appointment> out;
    for(auto&a:loadAppointments(includeCancelled)){
        if(!nid.empty()    && !icontains(a.nationalId,nid)) continue;
        if(!mobile.empty() && !icontains(a.mobile,mobile))  continue;
        if(!fn.empty()     && !icontains(a.firstName,fn))   continue;
        if(!ln.empty()     && !icontains(a.lastName,ln))    continue;
        out.push_back(a);
    }
    return out;
}

// ============================================================================
//  PROFILE-CHANGE REQUESTS  (reception → management)
// ============================================================================
static std::wstring profReqPath(){ return dataDir()+L"\\profreq.dat"; }
//  user|oldName|newName|oldPhoto|newPhoto|time|status|reason
std::vector<ProfReq> loadProfReqs(){
    std::vector<ProfReq> out;
    std::wstring all=readFileUtf8(profReqPath());
    size_t pos=0;
    while(pos<all.size()){
        size_t e=all.find(L'\n',pos); if(e==std::wstring::npos) e=all.size();
        std::wstring line=trim(all.substr(pos,e-pos)); pos=e+1;
        if(line.empty()) continue;
        auto f=dx_split(line,L'|');
        if(f.size()<7) continue;
        ProfReq r; r.user=f[0]; r.oldName=f[1]; r.newName=f[2];
        r.oldPhoto=f[3]; r.newPhoto=f[4]; r.time=f[5]; r.status=_wtoi(f[6].c_str());
        if(f.size()>=8) r.reason=f[7];
        out.push_back(r);
    }
    std::reverse(out.begin(),out.end());     // newest first
    return out;
}
static void saveAllProfReqs_newestFirst(const std::vector<ProfReq>& v){
    // stored oldest-first on disk
    std::wstring out;
    for(int i=(int)v.size()-1;i>=0;i--){
        const ProfReq& r=v[i];
        wchar_t sb[8]; swprintf(sb,8,L"%d",r.status);
        out += dx_esc(r.user)+L"|"+dx_esc(r.oldName)+L"|"+dx_esc(r.newName)+L"|"
            + dx_esc(r.oldPhoto)+L"|"+dx_esc(r.newPhoto)+L"|"+dx_esc(r.time)+L"|"
            + std::wstring(sb)+L"|"+dx_esc(r.reason)+L"\r\n";
    }
    writeFileUtf8(profReqPath(),out,false);
}
void pushProfReq(const ProfReq& r){
    SYSTEMTIME st=iranNow();
    wchar_t tb[32]; swprintf(tb,32,L"%s %02d:%02d",jalaliDateShort(st).c_str(),st.wHour,st.wMinute);
    ProfReq x=r; x.time=tb; x.status=0;
    std::wstring row=dx_esc(x.user)+L"|"+dx_esc(x.oldName)+L"|"+dx_esc(x.newName)+L"|"
        + dx_esc(x.oldPhoto)+L"|"+dx_esc(x.newPhoto)+L"|"+dx_esc(x.time)+L"|0|"+L"\r\n";
    writeFileUtf8(profReqPath(),row,true);
    logLine(L"profile change request from "+x.user);
}
void setProfReqStatus(int indexNewestFirst, int status, const std::wstring& reason){
    auto v=loadProfReqs();   // newest first
    if(indexNewestFirst<0||indexNewestFirst>=(int)v.size()) return;
    ProfReq& r=v[indexNewestFirst];
    r.status=status; r.reason=reason;
    if(status==1){
        // apply: persist the new display name as the user's fullname override
        if(!r.newName.empty()) setSetting(L"name_override_"+r.user, r.newName);
        if(!r.newPhoto.empty()) setSetting(L"photo_"+r.user, r.newPhoto);
        pushMessageT(L"مدیریت", r.user,
            L"درخواست تغییر پروفایل شما تأیید شد.", KMSG_NORMAL);
    } else if(status==2){
        std::wstring t=L"درخواست تغییر پروفایل شما رد شد.";
        if(!reason.empty()) t+=L" دلیل: "+reason;
        pushMessageT(L"مدیریت", r.user, t, KMSG_CRITICAL);
    }
    saveAllProfReqs_newestFirst(v);
}
int unseenProfReqCount(){
    int n=0; for(auto&r:loadProfReqs()) if(r.status==0) n++; return n;
}

// ============================================================================
//  CARTABLE v2 — pin / seen-one / delete-one (per-user, reported to manager)
// ============================================================================
//  Messages are stored in employees.cpp's data\messages.dat with the format:
//      from|to|time|seen|type|text
//  We add an OPTIONAL 7th field "pin" (0/1). Older 6-field rows read pin=0.
//  Acting on "the i-th message newest-first for this user" means we must map
//  back to the matching disk row. We re-load, filter by recipient, and toggle.
static std::wstring msgPath2(){ return dataDir()+L"\\messages.dat"; }
struct RawMsg { std::vector<std::wstring> f; };
static std::vector<RawMsg> loadRaw(){
    std::vector<RawMsg> out;
    std::wstring all=readFileUtf8(msgPath2());
    size_t pos=0;
    while(pos<all.size()){
        size_t e=all.find(L'\n',pos); if(e==std::wstring::npos) e=all.size();
        std::wstring line=all.substr(pos,e-pos); pos=e+1;
        if(trim(line).empty()) continue;
        RawMsg r; r.f=dx_split(line,L'|');
        if(r.f.size()<5) continue;
        while(r.f.size()<7) r.f.push_back(r.f.size()==4?L"":L"0"); // pad type/text/pin
        out.push_back(r);
    }
    return out;
}
static void saveRaw(const std::vector<RawMsg>& v){
    std::wstring out;
    for(auto&r:v){
        std::wstring line;
        for(size_t i=0;i<r.f.size();i++){ if(i) line+=L"|"; line+=r.f[i]; }
        out+=line+L"\r\n";
    }
    writeFileUtf8(msgPath2(),out,false);
}
//  map "i-th newest-first for forUser" → raw row index
static int rawIndexForUserMsg(const std::vector<RawMsg>& raw,
                              const std::wstring& forUser, int indexNewestFirst){
    // collect raw indices that belong to this user, in file (oldest-first) order
    std::vector<int> mine;
    for(size_t i=0;i<raw.size();i++){
        const std::wstring& to=raw[i].f[1];
        if(to==L"*" || to==forUser || forUser.empty()) mine.push_back((int)i);
    }
    // newest-first: reverse
    int n=(int)mine.size();
    int wantOldestFirst = n-1-indexNewestFirst;
    if(wantOldestFirst<0||wantOldestFirst>=n) return -1;
    return mine[wantOldestFirst];
}
void pinMessage(const std::wstring& forUser, int indexNewestFirst, bool pin){
    auto raw=loadRaw();
    int ri=rawIndexForUserMsg(raw,forUser,indexNewestFirst);
    if(ri<0) return;
    while(raw[ri].f.size()<7) raw[ri].f.push_back(L"0");
    raw[ri].f[6]= pin?L"1":L"0";
    saveRaw(raw);
}
void seenOneMessage(const std::wstring& forUser, int indexNewestFirst){
    auto raw=loadRaw();
    int ri=rawIndexForUserMsg(raw,forUser,indexNewestFirst);
    if(ri<0) return;
    if(raw[ri].f[3]!=L"1"){
        raw[ri].f[3]=L"1";
        saveRaw(raw);
        pushMessageT(forUser, L"مدیریت",
            L"کاربر «"+forUser+L"» پیام را مشاهده کرد.", KMSG_NORMAL);
    }
}
void deleteOneMessage(const std::wstring& forUser, int indexNewestFirst){
    auto raw=loadRaw();
    int ri=rawIndexForUserMsg(raw,forUser,indexNewestFirst);
    if(ri<0) return;
    std::wstring txt = raw[ri].f.size()>5?raw[ri].f[5]:L"";
    raw.erase(raw.begin()+ri);
    saveRaw(raw);
    pushMessageT(forUser, L"مدیریت",
        L"کاربر «"+forUser+L"» پیامی را حذف کرد.", KMSG_NORMAL);
}
