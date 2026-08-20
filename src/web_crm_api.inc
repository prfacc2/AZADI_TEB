// ============================================================================
//  web_crm_api.inc — the C++ side of the CRM management IPC bridge (v1.70.0).
//  Included into web_crm.cpp (after the UTF/JSON helpers). Every crm.* verb the
//  HTML management UI can send is handled here against the REAL data layer
//  (sections, services, doctors, patients, users/employees, messages, settings
//  + backup). Returns a JSON string. The UI never fabricates identities or
//  prices — every save round-trips through the authoritative C++ stores, so the
//  on-disk file formats (sections.dat, services.dat, doctors.dat, patients.dat,
//  users.dat, …) are preserved exactly.
// ============================================================================
#include "sections.h"

// ----------------------------------------------------------------------------
//  JSON serializers for each management entity (mirror the admission style).
// ----------------------------------------------------------------------------
static std::string crmSectionJson(const Section& s){
    std::string o="{";
    o+="\"id\":"+jnum(s.id)+",";
    o+="\"code\":"+jstr(s.code)+",";
    o+="\"name\":"+jstr(s.name_fa)+",";
    o+="\"kind\":"+jstr(s.kind)+",";
    o+="\"kindLabel\":"+jstr(Sections_KindLabel(s.kind))+",";
    o+="\"active\":";
    o+=(s.is_active?"true":"false");
    o+=",\"createdAt\":"+jstr(s.created_at)+",";
    o+="\"updatedAt\":"+jstr(s.updated_at)+"";
    o+="}";
    return o;
}

static std::string crmServiceJson(const ServiceDef& s){
    std::string o="{";
    o+="\"code\":"+jstr(s.code)+",";
    o+="\"name\":"+jstr(s.name)+",";
    o+="\"category\":"+jstr(s.category)+",";
    o+="\"dept\":"+jstr(s.dept)+",";
    o+="\"price\":"+jnum(s.price)+",";
    o+="\"insType\":"+jstr(s.insType)+",";
    o+="\"desc\":"+jstr(s.desc)+",";
    o+="\"status\":"+jnum(s.status)+",";
    o+="\"created\":"+jstr(s.created)+",";
    o+="\"modified\":"+jstr(s.modified)+"";
    o+="}";
    return o;
}

static std::string crmDoctorJson(const DoctorDef& d, int idx){
    std::string o="{";
    o+="\"idx\":"+jnum(idx)+",";
    o+="\"name\":"+jstr(d.name)+",";
    o+="\"specialty\":"+jstr(d.specialty)+",";
    o+="\"deptId\":"+jstr(d.deptId)+",";
    o+="\"docType\":"+jnum(d.docType)+",";
    o+="\"docCode\":"+jstr(d.docCode)+",";
    o+="\"active\":"; o+=(d.active?"true":"false"); o+=",";
    o+="\"medicalId\":"+jstr(d.medicalId)+",";
    o+="\"namePrefix\":"+jstr(d.namePrefix)+",";
    o+="\"firstName\":"+jstr(d.firstName)+",";
    o+="\"lastName\":"+jstr(d.lastName)+",";
    o+="\"nationalId\":"+jstr(d.nationalId)+",";
    o+="\"mobile\":"+jstr(d.mobile)+",";
    o+="\"email\":"+jstr(d.email)+",";
    o+="\"address\":"+jstr(d.address)+",";
    o+="\"franchise\":"+jstr(d.franchise)+",";
    o+="\"printOnReceipt\":"; o+=(d.printOnReceipt?"true":"false"); o+=",";
    o+="\"insSpecialty\":"+jstr(d.insSpecialty)+",";
    o+="\"degree\":"+jstr(d.degree)+",";
    o+="\"contractType\":"+jnum(d.contractType)+",";
    o+="\"emergencyContract\":"+jstr(d.emergencyContract)+",";
    o+="\"accounting\":"+jstr(d.accounting)+",";
    o+="\"services\":[";
    for(size_t i=0;i<d.services.size();++i){ if(i)o+=","; o+=jstr(d.services[i]); }
    o+="]";
    o+="}";
    return o;
}

static std::string crmPatientJson(const PatientRow& p){
    std::string o="{";
    o+="\"nid\":"+jstr(p.nid)+",";
    o+="\"first\":"+jstr(p.first)+",";
    o+="\"last\":"+jstr(p.last)+",";
    o+="\"father\":"+jstr(p.father)+",";
    o+="\"gender\":"+jstr(p.gender)+",";
    o+="\"birth\":"+jstr(p.birth)+",";
    o+="\"mobile\":"+jstr(p.mobile)+",";
    o+="\"phone\":"+jstr(p.landline)+",";
    o+="\"addr\":"+jstr(p.address)+",";
    o+="\"suppIdx\":"+jnum(p.suppIdx)+",";
    o+="\"insurances\":[";
    for(size_t i=0;i<p.insurances.size();++i){ if(i)o+=","; o+=jnum(p.insurances[i]); }
    o+="]";
    o+="}";
    return o;
}

static std::string crmUserJson(const User& u){
    std::string o="{";
    o+="\"id\":"+jnum(u.id)+",";
    o+="\"username\":"+jstr(u.username)+",";
    o+="\"fullname\":"+jstr(u.fullname)+",";
    o+="\"dept\":"+jstr(u.dept)+",";
    o+="\"role\":"+jnum(u.role)+",";
    o+="\"online\":"; o+=(isUserOnline(u.username)?"true":"false"); o+="";
    o+="}";
    return o;
}

static std::string crmDeptJson(const DeptCat& c){
    std::string o="{";
    o+="\"id\":"+jstr(c.id)+",";
    o+="\"name\":"+jstr(c.name)+",";
    o+="\"manager\":"+jstr(c.manager)+",";
    o+="\"icon\":"+jstr(c.icon)+"";
    o+="}";
    return o;
}

static std::string crmMsgJson(const KMsg& m, int idx){
    std::string o="{";
    o+="\"idx\":"+jnum(idx)+",";
    o+="\"from\":"+jstr(m.from)+",";
    o+="\"to\":"+jstr(m.to)+",";
    o+="\"text\":"+jstr(m.text)+",";
    o+="\"time\":"+jstr(m.time)+",";
    o+="\"seen\":"; o+=(m.seen?"true":"false"); o+=",";
    o+="\"type\":"+jnum(m.type)+",";
    o+="\"pinned\":"; o+=(m.pinned?"true":"false"); o+="";
    o+="}";
    return o;
}

// base + supplementary insurance lists (for the patient form selectors)
static std::string crmInsurancesJson(){
    std::string o="[";
    for(int i=0;i<N_INSURANCES;i++){
        if(i)o+=",";
        o+="{\"idx\":"+jnum(i)+",\"name\":"+jstr(INSURANCES[i].name)+"}";
    }
    o+="]"; return o;
}
static std::string crmSuppInsurancesJson(){
    std::string o="[";
    for(int i=0;i<N_SUPP;i++){
        if(i)o+=",";
        o+="{\"idx\":"+jnum(i)+",\"name\":"+jstr(SUPP_INSURANCES[i].name)+"}";
    }
    o+="]"; return o;
}

// current Jalali date + time strings (header clock + record stamps)
static std::wstring crmTodayJalali(){
    SYSTEMTIME st=iranNow();
    int jy,jm,jd; gregToJalali(st.wYear,st.wMonth,st.wDay,jy,jm,jd);
    wchar_t b[32]; swprintf(b,32,L"%04d/%02d/%02d",jy,jm,jd);
    return b;
}
static std::wstring crmNowTime(){
    SYSTEMTIME st=iranNow();
    wchar_t b[16]; swprintf(b,16,L"%02d:%02d:%02d",st.wHour,st.wMinute,st.wSecond);
    return b;
}

// parse a JSON array of strings (["a","b"]) into a vector<wstring>. Tolerant.
static std::vector<std::wstring> crmParseStrArray(const std::string& raw){
    std::vector<std::wstring> out;
    if(raw.empty()) return out;
    std::string s=raw;
    size_t p=s.find('['); if(p==std::string::npos) return out;
    size_t e=s.find(']',p); if(e==std::string::npos) e=s.size();
    for(size_t i=p+1;i<e;){
        while(i<e&&(s[i]==' '||s[i]==','||s[i]=='\t'||s[i]=='\n'||s[i]=='\r')) i++;
        if(i>=e||s[i]!='"') break;
        i++;
        std::string item;
        while(i<e){
            char c=s[i++];
            if(c=='\\'&&i<e){ char x=s[i++];
                switch(x){ case 'n':item+='\n';break; case 'r':item+='\r';break;
                    case 't':item+='\t';break; case '"':item+='"';break;
                    case '\\':item+='\\';break; case '/':item+='/';break;
                    case 'u':{ if(i+4<=e){ int cp=(int)strtol(s.substr(i,4).c_str(),NULL,16); i+=4;
                        if(cp<0x80) item+=(char)cp;
                        else if(cp<0x800){ item+=(char)(0xC0|(cp>>6)); item+=(char)(0x80|(cp&0x3F)); }
                        else { item+=(char)(0xE0|(cp>>12)); item+=(char)(0x80|((cp>>6)&0x3F)); item+=(char)(0x80|(cp&0x3F)); } } break; }
                    default: item+=x; }
            } else if(c=='"') break;
            else item+=c;
        }
        out.push_back(u82w(item));
    }
    return out;
}

// ----------------------------------------------------------------------------
//  crmApi — handle every management verb. Returns a JSON response string.
// ----------------------------------------------------------------------------
static std::string crmApi(const std::string& verb, const std::string& body){
    using std::wstring;

    // ---- crm.init: everything the page needs on first render ----
    if(verb=="crm.init"||verb=="crm.ready"){
        std::string o="{\"ok\":true,";
        o+="\"date\":"+jstr(crmTodayJalali())+",";
        o+="\"time\":"+jstr(crmNowTime())+",";
        o+="\"user\":"+jstr(g_session.user.fullname.empty()?g_session.user.username:g_session.user.fullname)+",";
        o+="\"username\":"+jstr(g_session.user.username)+",";
        o+="\"role\":"+jnum(g_session.user.role)+",";
        o+="\"theme\":"+jstr(getSetting(L"theme",L"light"))+",";
        o+="\"version\":\"1.70.0\",";
        std::vector<Section> secs; Sections_All(secs);
        o+="\"sections\":"+jnum((long long)secs.size())+",";
        o+="\"patients\":"+jnum((long long)loadAllPatients().size())+",";
        o+="\"doctors\":"+jnum((long long)loadDoctors().size())+",";
        o+="\"services\":"+jnum((long long)loadServices().size())+",";
        o+="\"employees\":"+jnum((long long)loadUsers().size())+",";
        o+="\"today\":"+jnum((long long)countTodayReceptions())+",";
        o+="\"messages\":"+jnum((long long)unseenMessageCount(g_session.user.username))+",";
        o+="\"insurances\":"+crmInsurancesJson()+",";
        o+="\"supp\":"+crmSuppInsurancesJson()+"";
        o+="}";
        return o;
    }

    // ---- clock poll (keeps the Jalali date fresh) ----
    if(verb=="crm.clock"){
        return "{\"time\":"+jstr(crmNowTime())+",\"date\":"+jstr(crmTodayJalali())+"}";
    }

    // ---- dashboard KPIs ----
    if(verb=="crm.dashboard"){
        std::string o="{";
        o+="\"patients\":"+jnum((long long)loadAllPatients().size())+",";
        o+="\"doctors\":"+jnum((long long)loadDoctors().size())+",";
        o+="\"services\":"+jnum((long long)loadServices().size())+",";
        std::vector<Section> secs; Sections_All(secs);
        o+="\"sections\":"+jnum((long long)secs.size())+",";
        o+="\"employees\":"+jnum((long long)loadUsers().size())+",";
        o+="\"today\":"+jnum((long long)countTodayReceptions())+",";
        o+="\"messages\":"+jnum((long long)unseenMessageCount(g_session.user.username))+",";
        o+="\"pendingReqs\":"+jnum((long long)(pendingSetReqCount()+unseenProfReqCount()))+",";
        o+="\"date\":"+jstr(crmTodayJalali())+",";
        o+="\"time\":"+jstr(crmNowTime())+"";
        o+="}";
        return o;
    }

    // ===================================================== SECTIONS (بخش) ====
    if(verb=="crm.sections.list"){
        std::vector<Section> all; Sections_All(all);
        std::string o="{\"rows\":[";
        for(size_t i=0;i<all.size();++i){ if(i)o+=","; o+=crmSectionJson(all[i]); }
        o+="]}"; return o;
    }
    if(verb=="crm.sections.save"){
        Section s;                       // default ctor: id=0, is_active=1
        double id=0; jsonGetNumber(body,"id",id); s.id=(int)id;
        std::string t;
        if(jsonGetString(body,"code",t))    s.code=u82w(t);
        if(jsonGetString(body,"name",t))    s.name_fa=u82w(t);
        if(jsonGetString(body,"kind",t))    s.kind=u82w(t);
        s.is_active=jsonGetBool(body,"active",true)?1:0;
        s.updated_at=crmTodayJalali();
        if(s.id<=0) s.created_at=s.updated_at;
        int rid=Sections_Upsert(s);
        if(rid<=0) return "{\"ok\":false,\"err\":\"ذخیره بخش ناموفق بود.\"}";
        return "{\"ok\":true,\"id\":"+jnum(rid)+"}";
    }
    if(verb=="crm.sections.delete"){
        double id=0; jsonGetNumber(body,"id",id);
        if(id<=0) return "{\"ok\":false,\"err\":\"شناسه نامعتبر است.\"}";
        return Sections_Delete((int)id)>0 ? "{\"ok\":true}"
                                          : "{\"ok\":false,\"err\":\"حذف ناموفق بود.\"}";
    }

    // ===================================================== SERVICES (خدمات) ===
    if(verb=="crm.services.list"){
        std::string qU; jsonGetString(body,"q",qU);
        wstring q=u82w(qU);
        auto svcs=loadServices();
        std::string o="{\"rows\":[";
        int n=0;
        for(const auto& s:svcs){
            bool hit=q.empty()||wcontains(s.name,q)||wcontains(s.code,q)||
                    wcontains(s.category,q);
            if(!hit) continue;
            if(n++) o+=",";
            o+=crmServiceJson(s);
            if(n>=200) break;
        }
        o+="]}"; return o;
    }
    if(verb=="crm.services.save"){
        ServiceDef s;
        std::string t; double n=0;
        if(jsonGetString(body,"code",t))     s.code=u82w(t);
        if(jsonGetString(body,"name",t))     s.name=u82w(t);
        if(jsonGetString(body,"category",t)) s.category=u82w(t);
        if(jsonGetString(body,"dept",t))     s.dept=u82w(t);
        if(jsonGetNumber(body,"price",n))    s.price=(long long)llround(n);
        if(jsonGetString(body,"insType",t))  s.insType=u82w(t);
        if(jsonGetString(body,"desc",t))     s.desc=u82w(t);
        s.status=jsonGetBool(body,"active",true)?1:0;
        std::string orig; jsonGetString(body,"originalCode",orig);
        std::wstring oc=u82w(orig);
        std::wstring err;
        bool ok;
        if(oc.empty()){
            // brand-new service
            ok=addService(s,err);
        } else if(s.code==oc){
            // edit in place (code unchanged) -> updateService matches by code
            ok=updateService(s,err);
        } else {
            // rename: capture the original, remove the old row, add the new one.
            // On add failure, roll back the EXACT original so no data is lost.
            ServiceDef original; bool haveOrig=false;
            {
                auto all=loadServices();
                for(const auto& e:all) if(e.code==oc){ original=e; haveOrig=true; break; }
            }
            if(!haveOrig){
                err=L"خدمت با این کد یافت نشد."; ok=false;
            } else if(!removeService(oc)){
                err=L"خدمت با این کد یافت نشد."; ok=false;
            } else {
                ok=addService(s,err);
                if(!ok && haveOrig){ std::wstring e2; addService(original,e2); }
            }
        }
        if(!ok) return std::string("{\"ok\":false,\"err\":")+jstr(err)+"}";
        return "{\"ok\":true}";
    }
    if(verb=="crm.services.delete"){
        std::string cU; jsonGetString(body,"code",cU);
        wstring code=u82w(cU);
        if(code.empty()) return "{\"ok\":false,\"err\":\"کد خدمت خالی است.\"}";
        return removeService(code) ? "{\"ok\":true}"
                                   : "{\"ok\":false,\"err\":\"حذف ناموفق بود.\"}";
    }

    // ===================================================== DOCTORS (پزشک/پرستار)
    if(verb=="crm.doctors.list"){
        std::string qU; jsonGetString(body,"q",qU);
        wstring q=u82w(qU);
        auto docs=loadDoctors();
        std::string o="{\"rows\":[";
        int n=0;
        for(size_t i=0;i<docs.size();++i){
            const auto& d=docs[i];
            bool hit=q.empty()||wcontains(d.name,q)||wcontains(d.specialty,q)||
                    wcontains(d.medicalId,q)||wcontains(d.docCode,q);
            if(!hit) continue;
            if(n++) o+=",";
            o+=crmDoctorJson(d,(int)i);
            if(n>=200) break;
        }
        o+="]}"; return o;
    }
    if(verb=="crm.doctors.save"){
        DoctorDef d;
        std::string t; double n=0;
        if(jsonGetString(body,"name",t))             d.name=u82w(t);
        if(jsonGetString(body,"specialty",t))        d.specialty=u82w(t);
        if(jsonGetString(body,"deptId",t))           d.deptId=u82w(t);
        if(jsonGetNumber(body,"docType",n))          d.docType=(int)n;
        if(jsonGetString(body,"docCode",t))          d.docCode=u82w(t);
        d.active=jsonGetBool(body,"active",true);
        if(jsonGetString(body,"medicalId",t))        d.medicalId=u82w(t);
        if(jsonGetString(body,"namePrefix",t))       d.namePrefix=u82w(t);
        if(jsonGetString(body,"firstName",t))        d.firstName=u82w(t);
        if(jsonGetString(body,"lastName",t))         d.lastName=u82w(t);
        if(jsonGetString(body,"nationalId",t))       d.nationalId=u82w(t);
        if(jsonGetString(body,"mobile",t))           d.mobile=u82w(t);
        if(jsonGetString(body,"email",t))            d.email=u82w(t);
        if(jsonGetString(body,"address",t))          d.address=u82w(t);
        if(jsonGetString(body,"franchise",t))        d.franchise=u82w(t);
        d.printOnReceipt=jsonGetBool(body,"printOnReceipt",true);
        if(jsonGetString(body,"insSpecialty",t))     d.insSpecialty=u82w(t);
        if(jsonGetString(body,"degree",t))           d.degree=u82w(t);
        if(jsonGetNumber(body,"contractType",n))     d.contractType=(int)n;
        if(jsonGetString(body,"emergencyContract",t))d.emergencyContract=u82w(t);
        if(jsonGetString(body,"accounting",t))       d.accounting=u82w(t);
        std::string svcRaw; if(jsonGetRaw(body,"services",svcRaw)) d.services=crmParseStrArray(svcRaw);
        if(trim(d.name).empty()) return "{\"ok\":false,\"err\":\"نام پزشک نمی‌تواند خالی باشد.\"}";
        double idx=-1; jsonGetNumber(body,"idx",idx);
        auto docs=loadDoctors();
        if(idx>=0 && idx<(double)docs.size()){
            docs[(size_t)idx]=d;
        } else {
            // avoid exact-duplicate names on append
            for(const auto& e:docs) if(e.name==d.name)
                return "{\"ok\":false,\"err\":\"این پزشک قبلاً ثبت شده است.\"}";
            docs.push_back(d);
        }
        if(!saveDoctors(docs)) return "{\"ok\":false,\"err\":\"ذخیره پزشک ناموفق بود.\"}";
        logLine(L"crm doctor saved: "+d.name);
        return "{\"ok\":true}";
    }
    if(verb=="crm.doctors.delete"){
        double idx=-1; jsonGetNumber(body,"idx",idx);
        if(idx<0) return "{\"ok\":false,\"err\":\"پزشکی انتخاب نشده است.\"}";
        auto docs=loadDoctors();
        if(idx>=(double)docs.size()) return "{\"ok\":false,\"err\":\"پزشک یافت نشد.\"}";
        std::wstring nm=docs[(size_t)idx].name;
        docs.erase(docs.begin()+(size_t)idx);
        if(!saveDoctors(docs)) return "{\"ok\":false,\"err\":\"حذف ناموفق بود.\"}";
        logLine(L"crm doctor removed: "+nm);
        return "{\"ok\":true}";
    }

    // ===================================================== PATIENTS (بیمار) ===
    if(verb=="crm.patients.list"){
        std::string qU; jsonGetString(body,"q",qU);
        wstring q=u82w(qU); wstring qd=normDigits(q);
        auto all=loadAllPatients();
        std::string o="{\"rows\":[";
        int n=0;
        for(const auto& p:all){
            wstring full=p.first+L" "+p.last;
            bool hit=q.empty()||wcontains(full,q)||
                    normDigits(p.nid).find(qd)!=wstring::npos||
                    normDigits(p.mobile).find(qd)!=wstring::npos;
            if(!hit) continue;
            if(n++) o+=",";
            o+=crmPatientJson(p);
            if(n>=200) break;
        }
        o+="]}"; return o;
    }
    if(verb=="crm.patients.save"){
        std::string t; double si=-1;
        std::wstring nid,first,last,father,gender,birth,mobile,landline,address;
        if(jsonGetString(body,"nid",t))      nid=u82w(t);
        if(jsonGetString(body,"first",t))    first=u82w(t);
        if(jsonGetString(body,"last",t))     last=u82w(t);
        if(jsonGetString(body,"father",t))   father=u82w(t);
        if(jsonGetString(body,"gender",t))   gender=u82w(t);
        if(jsonGetString(body,"birth",t))    birth=u82w(t);
        if(jsonGetString(body,"mobile",t))   mobile=u82w(t);
        if(jsonGetString(body,"phone",t))    landline=u82w(t);
        if(jsonGetString(body,"addr",t))     address=u82w(t);
        if(jsonGetNumber(body,"suppIdx",si)) /*keep*/; else si=-1;
        nid=normDigits(trim(nid));
        if(nid.empty()) return "{\"ok\":false,\"err\":\"کد ملی خالی است.\"}";
        std::vector<int> ins;
        std::string insRaw; if(jsonGetRaw(body,"insurances",insRaw)){
            // parse [n,n,...]
            size_t p=insRaw.find('[');
            if(p!=std::string::npos){
                for(size_t i=p+1;i<insRaw.size();){
                    while(i<insRaw.size()&&(insRaw[i]==' '||insRaw[i]==',')) i++;
                    if(i>=insRaw.size()||insRaw[i]==']') break;
                    char* end=NULL; double v=strtod(insRaw.c_str()+i,&end);
                    if(end==insRaw.c_str()+i) break;
                    ins.push_back((int)v); i=(size_t)(end-insRaw.c_str());
                }
            }
        }
        rememberPatient(nid,first,last,father,gender,birth,mobile,
                        landline,address,ins,(int)si);
        logLine(L"crm patient saved: "+nid);
        return "{\"ok\":true}";
    }
    if(verb=="crm.patients.delete"){
        std::string nU; jsonGetString(body,"nid",nU);
        std::wstring nid=normDigits(trim(u82w(nU)));
        if(nid.empty()) return "{\"ok\":false,\"err\":\"کد ملی خالی است.\"}";
        return deletePatient(nid) ? "{\"ok\":true}"
                                  : "{\"ok\":false,\"err\":\"بیماری با این کد ملی یافت نشد.\"}";
    }

    // ===================================================== EMPLOYEES (کاربر) ==
    if(verb=="crm.employees.list"){
        auto us=loadUsers();
        std::string o="{\"rows\":[";
        for(size_t i=0;i<us.size();++i){ if(i)o+=","; o+=crmUserJson(us[i]); }
        o+="],\"depts\":[";
        auto ds=loadDepts();
        for(size_t i=0;i<ds.size();++i){ if(i)o+=","; o+=crmDeptJson(ds[i]); }
        o+="]}"; return o;
    }
    if(verb=="crm.employees.save"){
        std::string t; double r=0;
        std::wstring username,fullname,dept,pass;
        if(jsonGetString(body,"username",t)) username=u82w(t);
        if(jsonGetString(body,"fullname",t)) fullname=u82w(t);
        if(jsonGetString(body,"dept",t))     dept=u82w(t);
        if(jsonGetString(body,"password",t)) pass=u82w(t);
        if(jsonGetNumber(body,"role",r))     /*keep*/; else r=0;
        username=trim(username);
        if(username.empty()) return "{\"ok\":false,\"err\":\"نام کاربری خالی است.\"}";
        // existing user -> update fields (+ optional password reset)
        auto us=loadUsers(); bool exists=false;
        for(const auto& u:us) if(u.username==username){ exists=true; break; }
        if(exists){
            std::wstring err;
            if(!updateUserAccount(username,fullname,dept,(int)r,pass,err))
                return std::string("{\"ok\":false,\"err\":")+jstr(err)+"}";
            return "{\"ok\":true}";
        }
        // new user -> add (requires a password)
        if(pass.empty()) return "{\"ok\":false,\"err\":\"رمز عبور الزامی است.\"}";
        User u; u.username=username; u.fullname=fullname; u.dept=dept;
        u.role=(int)r; u.hash=hashPassword(pass);
        std::wstring err;
        if(!addUser(u,err)) return std::string("{\"ok\":false,\"err\":")+jstr(err)+"}";
        return "{\"ok\":true}";
    }
    if(verb=="crm.employees.delete"){
        std::string uU; jsonGetString(body,"username",uU);
        std::wstring username=u82w(uU);
        if(username.empty()||username==L"prf")
            return "{\"ok\":false,\"err\":\"این کاربر قابل حذف نیست.\"}";
        return removeUser(username) ? "{\"ok\":true}"
                                    : "{\"ok\":false,\"err\":\"حذف ناموفق بود.\"}";
    }
    if(verb=="crm.depts.save"){
        DeptCat c;
        std::string t;
        if(jsonGetString(body,"id",t))      c.id=u82w(t);
        if(jsonGetString(body,"name",t))    c.name=u82w(t);
        if(jsonGetString(body,"manager",t)) c.manager=u82w(t);
        if(jsonGetString(body,"icon",t))    c.icon=u82w(t);
        std::wstring err;
        if(!addDept(c,err)) return std::string("{\"ok\":false,\"err\":")+jstr(err)+"}";
        return "{\"ok\":true}";
    }
    if(verb=="crm.depts.delete"){
        std::string idU; jsonGetString(body,"id",idU);
        std::wstring id=u82w(idU);
        return removeDept(id) ? "{\"ok\":true}"
                              : "{\"ok\":false,\"err\":\"حذف ناموفق بود.\"}";
    }

    // ===================================================== MESSAGES (کارتابل)
    if(verb=="crm.messages.list"){
        std::string forU; jsonGetString(body,"forUser",forU);
        std::wstring who=forU.empty()?g_session.user.username:u82w(forU);
        if(who.empty()) who=L"*";
        auto msgs=loadMessages(who);       // oldest-first (file order)
        // newest-first so idx == indexNewestFirst used by pin/seen/delete.
        std::reverse(msgs.begin(),msgs.end());
        std::string o="{\"rows\":[";
        for(size_t i=0;i<msgs.size();++i){ if(i)o+=","; o+=crmMsgJson(msgs[i],(int)i); }
        o+="],\"unseen\":"+jnum((long long)unseenMessageCount(who))+"}";
        return o;
    }
    if(verb=="crm.messages.send"){
        std::string fU,tU,txt; double tp=0;
        jsonGetString(body,"from",fU);
        jsonGetString(body,"to",tU);
        jsonGetString(body,"text",txt);
        jsonGetNumber(body,"type",tp);
        std::wstring from=fU.empty()?g_session.user.username:u82w(fU);
        std::wstring to=u82w(tU); if(to.empty()) to=L"*";
        std::wstring text=u82w(txt);
        if(trim(text).empty()) return "{\"ok\":false,\"err\":\"متن پیام خالی است.\"}";
        pushMessageT(from,to,text,(int)tp);
        return "{\"ok\":true}";
    }
    if(verb=="crm.messages.seen"){
        std::string forU; jsonGetString(body,"forUser",forU);
        std::wstring who=forU.empty()?g_session.user.username:u82w(forU);
        markMessagesSeen(who);
        return "{\"ok\":true}";
    }
    if(verb=="crm.messages.delete"){
        std::string forU; double idx=-1;
        jsonGetString(body,"forUser",forU);
        jsonGetNumber(body,"idx",idx);
        std::wstring who=forU.empty()?g_session.user.username:u82w(forU);
        if(idx<0) return "{\"ok\":false,\"err\":\"پیامی انتخاب نشده است.\"}";
        deleteOneMessage(who,(int)idx);
        return "{\"ok\":true}";
    }
    if(verb=="crm.messages.pin"){
        std::string forU; double idx=-1; bool pin=true;
        jsonGetString(body,"forUser",forU);
        jsonGetNumber(body,"idx",idx);
        pin=jsonGetBool(body,"pin",true);
        std::wstring who=forU.empty()?g_session.user.username:u82w(forU);
        if(idx<0) return "{\"ok\":false,\"err\":\"پیامی انتخاب نشده است.\"}";
        pinMessage(who,(int)idx,pin);
        return "{\"ok\":true}";
    }

    // ===================================================== SETTINGS / BACKUP ==
    if(verb=="crm.settings.get"){
        std::string o="{";
        o+="\"theme\":"+jstr(getSetting(L"theme",L"light"))+",";
        o+="\"palette\":"+jstr(getSetting(L"theme.palette",L"blue"))+",";
        o+="\"clinicName\":"+jstr(getSetting(L"clinic.name",L"درمان پلاس"))+",";
        o+="\"receptionMode\":"+jstr(getSetting(L"reception.mode",L"simple"))+",";
        o+="\"printCopies\":"+jstr(getSetting(L"print.copies",L"1"))+",";
        o+="\"savedMsgs\":"+jstr(getSetting(L"saved_msgs.enabled",L"0"))+"";
        o+="}";
        return o;
    }
    if(verb=="crm.settings.save"){
        std::string t;
        if(jsonGetString(body,"theme",t) && !t.empty())       setSetting(L"theme",u82w(t));
        if(jsonGetString(body,"palette",t) && !t.empty())     setSetting(L"theme.palette",u82w(t));
        if(jsonGetString(body,"clinicName",t))                setSetting(L"clinic.name",u82w(t));
        if(jsonGetString(body,"receptionMode",t) && !t.empty())setSetting(L"reception.mode",u82w(t));
        if(jsonGetString(body,"printCopies",t))               setSetting(L"print.copies",u82w(t));
        if(jsonGetString(body,"savedMsgs",t))                 setSetting(L"saved_msgs.enabled",u82w(t));
        logLine(L"crm settings saved");
        return "{\"ok\":true}";
    }
    if(verb=="crm.backup"){
        // open the management backup/restore manager (modal) on the UI thread.
        HWND owner=g_hFrame;
        RunOnUiThreadSync([&](){ openBackupManager(owner); });
        return "{\"ok\":true}";
    }
    if(verb=="crm.printConfig"){
        // open the print-settings modal (تنظیمات چاپ) on the UI thread.
        HWND owner=g_hFrame;
        RunOnUiThreadSync([&](){ PrintCfg_Open(owner); });
        return "{\"ok\":true}";
    }

    return "{\"ok\":false,\"err\":\"verb ناشناخته\"}";
}
