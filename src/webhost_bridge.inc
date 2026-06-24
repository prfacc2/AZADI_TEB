// ============================================================================
//  webhost_bridge.inc — native <-> web bridge verbs + boot payload + embedded
//  HTML/CSS/JS documents for the reception & appointment surfaces.
//  Included into webhost_run.inc.  C++ is the source of truth here: every verb
//  validates and persists through the existing repository functions (app.h).
// ============================================================================

// ----------------------------------------------------------- tiny JSON out --
namespace whjson {
    static std::wstring esc(const std::wstring& s){
        std::wstring o; o.reserve(s.size()+8);
        for(wchar_t c : s){
            switch(c){
                case L'"':  o+=L"\\\""; break;
                case L'\\': o+=L"\\\\"; break;
                case L'\n': o+=L"\\n";  break;
                case L'\r': o+=L"\\r";  break;
                case L'\t': o+=L"\\t";  break;
                default:
                    if(c<0x20){ wchar_t b[8]; swprintf(b,8,L"\\u%04x",(unsigned)c); o+=b; }
                    else o+=c;
            }
        }
        return o;
    }
    struct Obj {
        std::wstring s; bool first=true;
        Obj(){ s=L"{"; }
        void key(const wchar_t* k){ if(!first) s+=L","; first=false; s+=L"\""; s+=k; s+=L"\":"; }
        void str(const wchar_t* k, const std::wstring& v){ key(k); s+=L"\""; s+=esc(v); s+=L"\""; }
        void num(const wchar_t* k, long long v){ key(k); wchar_t b[32]; swprintf(b,32,L"%lld",v); s+=b; }
        void boolean(const wchar_t* k, bool v){ key(k); s+= v?L"true":L"false"; }
        void raw(const wchar_t* k, const std::wstring& v){ key(k); s+=v; }
        std::wstring done(){ return s+L"}"; }
    };
    // get a string value for a top-level key from a flat JSON object (tolerant,
    // good enough for our small bridge args — values are strings or numbers).
    static std::wstring get(const std::wstring& json, const wchar_t* key){
        std::wstring pat=L"\""; pat+=key; pat+=L"\"";
        size_t p=json.find(pat); if(p==std::wstring::npos) return L"";
        p=json.find(L':',p+pat.size()); if(p==std::wstring::npos) return L"";
        ++p; while(p<json.size() && (json[p]==L' '||json[p]==L'\t')) ++p;
        if(p>=json.size()) return L"";
        if(json[p]==L'"'){
            ++p; std::wstring o;
            while(p<json.size() && json[p]!=L'"'){
                if(json[p]==L'\\' && p+1<json.size()){
                    wchar_t e=json[p+1]; p+=2;
                    switch(e){ case L'n':o+=L'\n';break; case L'r':o+=L'\r';break;
                        case L't':o+=L'\t';break; case L'"':o+=L'"';break;
                        case L'\\':o+=L'\\';break; default:o+=e; }
                } else o+=json[p++];
            }
            return o;
        }
        // bare number / literal
        size_t st=p;
        while(p<json.size() && json[p]!=L','&&json[p]!=L'}'&&json[p]!=L' '&&json[p]!=L'\n'&&json[p]!=L'\r') ++p;
        return json.substr(st,p-st);
    }
    static long long getNum(const std::wstring& json, const wchar_t* key){
        return _wtoi64(get(json,key).c_str());
    }
}

// --------------------------------------------------------- shared json bits --
static std::wstring wh_sectionsJson(){
    std::vector<Section> secs; Sections_All(secs);
    std::wstring a=L"[";
    bool first=true;
    for(const auto& s : secs){
        if(!s.is_active) continue;
        if(!first) a+=L","; first=false;
        whjson::Obj o;
        o.num(L"id",s.id);
        o.str(L"code",s.code);
        o.str(L"cat",Sections_CategoryCode(s.kind));   // §7 stable category key
        o.str(L"prefix",Sections_CodePrefix(s));        // §7 durable code prefix
        o.str(L"name",s.name_fa);
        o.str(L"kind",s.kind);
        o.str(L"kindLabel",Sections_KindLabel(s.kind));
        a+=o.done();
    }
    return a+L"]";
}
static std::wstring wh_insurancesJson(){
    std::wstring a=L"[";
    for(int i=0;i<N_INSURANCES;i++){ if(i)a+=L",";
        whjson::Obj o; o.num(L"idx",i); o.str(L"name",INSURANCES[i].name); o.num(L"pct",INSURANCES[i].pct);
        a+=o.done(); }
    return a+L"]";
}
static std::wstring wh_suppJson(){
    std::wstring a=L"[";
    for(int i=0;i<N_SUPP;i++){ if(i)a+=L",";
        whjson::Obj o; o.num(L"idx",i); o.str(L"name",SUPP_INSURANCES[i].name); o.num(L"pct",SUPP_INSURANCES[i].pct);
        a+=o.done(); }
    return a+L"]";
}
static std::wstring wh_themeJson(){
    auto hex=[&](COLORREF c){ wchar_t b[12]; swprintf(b,12,L"#%02x%02x%02x",
        GetRValue(c),GetGValue(c),GetBValue(c)); return std::wstring(b); };
    whjson::Obj o;
    o.str(L"bg",hex(g_theme.bg));
    o.str(L"surface",hex(g_theme.surface));
    o.str(L"surfaceTop",hex(g_theme.surfaceTop));
    o.str(L"surface2",hex(g_theme.surface2));
    o.str(L"border",hex(g_theme.border));
    o.str(L"text",hex(g_theme.text));
    o.str(L"textDim",hex(g_theme.textDim));
    o.str(L"accent",hex(g_theme.accent));
    o.str(L"accent2",hex(g_theme.accent2));
    o.str(L"accentText",hex(g_theme.accentText));
    o.str(L"danger",hex(g_theme.danger));
    o.str(L"success",hex(g_theme.success));
    o.str(L"warn",hex(g_theme.warn));
    o.str(L"inputBg",hex(g_theme.inputBg));
    o.str(L"inputText",hex(g_theme.inputText));
    o.boolean(L"dark",g_dark);
    return o.done();
}

// ---------------------------------------------------------- boot payload -----
std::wstring WebHostBridge_BootPayload(int kind){
    whjson::Obj o;
    o.str(L"event",L"boot");
    o.num(L"kind",kind);
    o.str(L"app",APP_VERSION_W);
    o.str(L"userName", g_session.user.fullname.empty()?g_session.user.username:g_session.user.fullname);
    o.str(L"userDept", g_session.user.dept);
    o.str(L"shift", shiftName(g_session.shift));
    o.str(L"today", FormatJalaliPersian(0));
    o.raw(L"theme", wh_themeJson());
    o.raw(L"sections", wh_sectionsJson());
    o.raw(L"insurances", wh_insurancesJson());
    o.raw(L"supp", wh_suppJson());
    if(kind==WH_APPOINTMENT){
        // doctors on shift today
        std::vector<DoctorDef> docs=todaysDoctors();
        std::wstring da=L"[";
        for(size_t i=0;i<docs.size();++i){ if(i)da+=L",";
            whjson::Obj d; d.str(L"name",docs[i].name); d.str(L"specialty",docs[i].specialty);
            std::wstring sa=L"[";
            for(size_t j=0;j<docs[i].services.size();++j){ if(j)sa+=L",";
                sa+=L"\""+whjson::esc(docs[i].services[j])+L"\""; }
            sa+=L"]";
            d.raw(L"services",sa);
            da+=d.done(); }
        da+=L"]";
        o.raw(L"doctors",da);
    }
    return o.done();
}

// --------------------------------------------------------- verb dispatch -----
//  Every verb returns a JSON string. C++ owns all validation + persistence.
static std::wstring wh_apptListJson(){
    std::vector<Appointment> ap=loadAppointments(true);
    std::wstring a=L"[";
    // newest first
    for(size_t i=ap.size(); i-- > 0; ){
        const Appointment& x=ap[i];
        if(a.size()>1) a+=L",";
        whjson::Obj o;
        o.num(L"idx",(long long)i);
        o.str(L"nid",x.nationalId);
        o.str(L"first",x.firstName);
        o.str(L"last",x.lastName);
        o.str(L"mobile",x.mobile);
        o.str(L"doctor",x.doctor);
        o.str(L"service",x.service);
        o.str(L"date",x.apptDate);
        o.str(L"time",x.apptTime);
        o.str(L"day",x.day);
        o.str(L"kind",x.kind);
        o.str(L"user",x.user);
        o.num(L"queue",x.queueNo);
        o.boolean(L"cancelled",x.cancelled);
        a+=o.done();
    }
    return a+L"]";
}

std::wstring WebHostBridge_Call(int kind, const std::wstring& verb,
                                const std::wstring& argsJson){
    // ---- lookup a national id (verified source only; never fabricated) ----
    if(verb==L"lookup"){
        std::wstring nid=whjson::get(argsJson,L"nid");
        whjson::Obj o;
        bool idOk=validNationalId(nid);
        o.boolean(L"idValid",idOk);
        CitizenInfo c=lookupCitizen(nid);
        o.boolean(L"found",c.found);
        if(c.found){
            o.str(L"first",c.firstName); o.str(L"last",c.lastName);
            o.str(L"father",c.fatherName); o.str(L"gender",c.gender);
            o.str(L"birth",c.birthDate); o.str(L"mobile",c.mobile);
        }
        return o.done();
    }
    // ---- recompute the bill (mirrors reception.cpp recalc exactly) ----
    if(verb==L"bill"){
        long long price = whjson::getNum(argsJson,L"price");
        long long disc  = whjson::getNum(argsJson,L"discount");
        int pType = (int)whjson::getNum(argsJson,L"ptype");
        int nType = (int)whjson::getNum(argsJson,L"ntype");
        int insIdx= (int)whjson::getNum(argsJson,L"ins");
        int suppIdx=(int)whjson::getNum(argsJson,L"supp");
        if(insIdx<0||insIdx>=N_INSURANCES) insIdx=0;
        if(suppIdx<0||suppIdx>=N_SUPP) suppIdx=0;
        if(price<=0) price=defaultServicePrice(pType,nType);
        long long mainShare = price*INSURANCES[insIdx].pct/100;
        long long baseDiff  = price-mainShare;
        long long orgShare  = baseDiff*SUPP_INSURANCES[suppIdx].pct/100;
        long long patShare  = baseDiff-orgShare;
        long long pay = patShare-disc; if(pay<0) pay=0;
        whjson::Obj o;
        o.num(L"price",price); o.num(L"mainShare",mainShare);
        o.num(L"baseDiff",baseDiff); o.num(L"orgShare",orgShare);
        o.num(L"patientShare",patShare); o.num(L"paid",pay);
        return o.done();
    }
    // ---- save a reception (validate + persist + remember patient) ----
    if(verb==L"saveReception"){
        ReceptionRecord r;
        r.firstName  = whjson::get(argsJson,L"first");
        r.lastName   = whjson::get(argsJson,L"last");
        r.nationalId = whjson::get(argsJson,L"nid");
        r.fatherName = whjson::get(argsJson,L"father");
        r.birthDate  = whjson::get(argsJson,L"birth");
        r.gender     = whjson::get(argsJson,L"gender");
        r.mobile     = whjson::get(argsJson,L"mobile");
        r.landline   = whjson::get(argsJson,L"phone");
        r.address    = whjson::get(argsJson,L"address");
        r.patientType= whjson::get(argsJson,L"ptypeName");
        r.insurance  = whjson::get(argsJson,L"insName");
        r.suppInsurance=whjson::get(argsJson,L"suppName");
        r.apptDate   = FormatJalaliPersian(0);
        { SYSTEMTIME st=iranNow(); wchar_t b[16]; swprintf(b,16,L"%02d:%02d",st.wHour,st.wMinute); r.apptTime=b; }
        r.shift      = shiftName(g_session.shift);
        r.dept       = g_session.user.dept;
        r.userName   = g_session.user.username;
        r.insIdx     = (int)whjson::getNum(argsJson,L"ins");
        r.suppIdx    = (int)whjson::getNum(argsJson,L"supp");
        r.total      = whjson::getNum(argsJson,L"price");
        r.discount   = whjson::getNum(argsJson,L"discount");
        // validation: a name is required.
        if(trim(r.firstName).empty() && trim(r.lastName).empty()){
            whjson::Obj o; o.boolean(L"ok",false);
            o.str(L"error",L"\u0646\u0627\u0645 \u0628\u06cc\u0645\u0627\u0631 \u0627\u0644\u0632\u0627\u0645\u06cc \u0627\u0633\u062a.");
            return o.done();
        }
        // recompute the bill server-side (never trust the client totals).
        int insIdx=r.insIdx, suppIdx=r.suppIdx;
        if(insIdx<0||insIdx>=N_INSURANCES) insIdx=0;
        if(suppIdx<0||suppIdx>=N_SUPP) suppIdx=0;
        // mirror the live preview: when the amount field is empty fall back to the
        // tariff for the chosen patient/visit type (not a hardcoded عادی tariff).
        int pTypeI=(int)whjson::getNum(argsJson,L"ptype");
        int nTypeI=(int)whjson::getNum(argsJson,L"ntype");
        long long price=r.total; if(price<=0) price=defaultServicePrice(pTypeI,nTypeI);
        r.total=price;
        r.mainShare=price*INSURANCES[insIdx].pct/100;
        r.baseDiff=price-r.mainShare;
        r.orgShare=r.baseDiff*SUPP_INSURANCES[suppIdx].pct/100;
        r.patientShare=r.baseDiff-r.orgShare;
        r.paid=r.patientShare-r.discount; if(r.paid<0) r.paid=0;
        int q=saveReception(r);
        if(q>0 && validNationalId(r.nationalId)){
            std::vector<int> ins; if(insIdx>0) ins.push_back(insIdx);
            rememberPatient(r.nationalId,r.firstName,r.lastName,r.fatherName,
                            r.gender,r.birthDate,r.mobile,ins);
        }
        saveLastReceipt(r);
        whjson::Obj o; o.boolean(L"ok",q>0); o.num(L"queue",q); o.num(L"paid",r.paid);
        return o.done();
    }
    // ---- print the last receipt / a kind ----
    if(verb==L"print"){
        int kindp=(int)whjson::getNum(argsJson,L"kind");   // 0 ins / 1 rx / 2 receipt
        ReceptionRecord r;
        if(loadLastReceipt(r)) printReceipt(r,kindp,g_hFrame);
        whjson::Obj o; o.boolean(L"ok",true); return o.done();
    }
    if(verb==L"printLast"){
        printLastReceipt(g_hFrame);
        whjson::Obj o; o.boolean(L"ok",true); return o.done();
    }
    // ---- appointment: list ----
    if(verb==L"apptList"){
        return wh_apptListJson();
    }
    // ---- appointment: next queue number for a section ----
    if(verb==L"apptNext"){
        whjson::Obj o; o.num(L"next",(long long)(countTodayReceptions()+1)); return o.done();
    }
    // ---- appointment: save ----
    if(verb==L"saveAppointment"){
        Appointment a;
        a.nationalId=whjson::get(argsJson,L"nid");
        a.firstName =whjson::get(argsJson,L"first");
        a.lastName  =whjson::get(argsJson,L"last");
        a.mobile    =whjson::get(argsJson,L"mobile");
        a.doctor    =whjson::get(argsJson,L"doctor");
        a.service   =whjson::get(argsJson,L"service");
        a.apptDate  =whjson::get(argsJson,L"date"); if(a.apptDate.empty()) a.apptDate=FormatJalaliPersian(0);
        a.apptTime  =whjson::get(argsJson,L"time");
        a.kind      =whjson::get(argsJson,L"kind"); if(a.kind.empty()) a.kind=L"\u062d\u0636\u0648\u0631\u06cc";
        a.shift     =shiftName(g_session.shift);
        a.user      =g_session.user.username;
        if(trim(a.firstName).empty() && trim(a.lastName).empty()){
            whjson::Obj o; o.boolean(L"ok",false);
            o.str(L"error",L"\u0646\u0627\u0645 \u0628\u06cc\u0645\u0627\u0631 \u0627\u0644\u0632\u0627\u0645\u06cc \u0627\u0633\u062a.");
            return o.done();
        }
        int q=saveAppointment(a);
        if(q>0 && validNationalId(a.nationalId)){
            rememberPatient(a.nationalId,a.firstName,a.lastName,L"",L"",L"",a.mobile,{});
        }
        whjson::Obj o; o.boolean(L"ok",q>0); o.num(L"queue",q); return o.done();
    }
    // ---- appointment: cancel by index ----
    if(verb==L"cancelAppointment"){
        int idx=(int)whjson::getNum(argsJson,L"idx");
        bool ok=cancelAppointment(idx);
        whjson::Obj o; o.boolean(L"ok",ok); return o.done();
    }
    // ---- theme toggle from the web UI ----
    if(verb==L"setTheme"){
        bool dark = whjson::get(argsJson,L"dark")==L"true" || whjson::getNum(argsJson,L"dark")!=0;
        setSetting(L"theme", dark?L"dark":L"light");
        applyTheme(dark); broadcastThemeChange();
        if(g_hFrame) PostMessageW(g_hFrame,WM_APP_THEME_CHANGED,1,0);
        return wh_themeJson();
    }
    if(verb==L"theme"){ return wh_themeJson(); }
    if(verb==L"sections"){ return wh_sectionsJson(); }
    // ---- explicit boot pull: lets the page request its hydration payload if a
    //      pushed boot was missed (timing/watchdog). Returns the full boot obj. --
    if(verb==L"boot"){ return WebHostBridge_BootPayload(kind); }
    // ---- insurance lists on demand (belt-and-braces for an empty dropdown) ----
    if(verb==L"insurances"){ return wh_insurancesJson(); }
    if(verb==L"supp"){ return wh_suppJson(); }
    if(verb==L"ping"){ whjson::Obj o; o.boolean(L"ok",true); return o.done(); }

    // unknown verb — log once, return an empty object so JS never breaks.
    WebHost_LogError(L"verb", verb.c_str());
    return L"{}";
}

#include "webhost_assets.inc"
