// ============================================================================
//  web_admission_api.inc — the C++ side of the admission IPC bridge.
//  Included into web_admission.cpp. Every verb the JS UI can send is handled
//  here against the REAL data layer (patients, services, insurances, doctors,
//  reception persistence + the Management print design). Returns a JSON string.
//  This is the single source of truth — the UI never fabricates prices,
//  identities or bill amounts.
// ============================================================================

// v1.43.0 forward decl (defined in web_admission_dispatch.inc): pushes the
// fresh insurance selectors to every open embedded admission view after an edit.
void WebAdmission_NotifyCatalogChanged();

// ---- serialize a PatientRow to JSON (v1.40.0 struct-based, authoritative) ---
//  The struct overload is the single source of truth for the wire format; the
//  legacy positional overload below delegates to it so the two never diverge.
static std::string patientJson(const PatientRow& p){
    std::string o="{";
    o+="\"nid\":"    +jstr(p.nid)    +",";
    o+="\"first\":"  +jstr(p.first)  +",";
    o+="\"last\":"   +jstr(p.last)   +",";
    o+="\"father\":" +jstr(p.father) +",";
    o+="\"gender\":" +jstr(p.gender) +",";
    o+="\"birth\":"  +jstr(p.birth)  +",";
    o+="\"mobile\":" +jstr(p.mobile) +",";
    o+="\"phone\":"  +jstr(p.landline)+",";   // v1.40.0 — تلفن ثابت
    o+="\"addr\":"   +jstr(p.address) +",";   // v1.40.0 — آدرس
    o+="\"suppIdx\":"+jnum(p.suppIdx) +",";   // v1.40.0 — SUPP_INSURANCES[] index
    o+="\"insurances\":[";
    for(size_t i=0;i<p.insurances.size();++i){ if(i)o+=","; o+=jnum(p.insurances[i]); }
    o+="]}";
    return o;
}
// ---- legacy positional overload — delegates to the struct one --------------
static std::string patientJson(const std::wstring& nid, const std::wstring& first,
        const std::wstring& last, const std::wstring& father, const std::wstring& gender,
        const std::wstring& birth, const std::wstring& mobile, const std::vector<int>& ins,
        const std::wstring& landline=L"", const std::wstring& address=L"", int suppIdx=-1){
    PatientRow p;
    p.nid=nid; p.first=first; p.last=last; p.father=father; p.gender=gender;
    p.birth=birth; p.mobile=mobile; p.landline=landline; p.address=address;
    p.insurances=ins; p.suppIdx=suppIdx;
    return patientJson(p);
}

static std::string serviceJson(const ServiceDef& s){
    std::string o="{";
    o+="\"code\":"+jstr(s.code)+",";
    o+="\"name\":"+jstr(s.name)+",";
    o+="\"category\":"+jstr(s.category)+",";
    o+="\"dept\":"+jstr(s.dept)+",";
    o+="\"price\":"+jnum(s.price)+",";
    o+="\"status\":"+jnum(s.status);
    o+="}";
    return o;
}

// ---- insurance lists (base + supplementary) --------------------------------
// v1.43.0: the selectors + billing now read the EDITABLE insurance store
// («مدیریت بیمه‌ها», data\insurances.dat) instead of the hardcoded arrays, so
// clinic-defined coverage percentages drive the discount. Index order here is
// exactly what the web UI's <select> uses, so JS indices stay aligned with C++.
static std::string insurancesJson(){
    auto v=insBaseList();
    std::string o="[";
    for(size_t i=0;i<v.size();i++){
        if(i)o+=",";
        o+="{\"name\":"+jstr(v[i].name)+",\"pct\":"+jnum(v[i].pct)+"}";
    }
    o+="]"; return o;
}
static std::string suppInsurancesJson(){
    auto v=insSuppList();
    std::string o="[";
    for(size_t i=0;i<v.size();i++){
        if(i)o+=",";
        o+="{\"name\":"+jstr(v[i].name)+",\"pct\":"+jnum(v[i].pct)+"}";
    }
    o+="]"; return o;
}

// case-insensitive contains for wide strings (Persian is unaffected)
static bool wcontains(const std::wstring& hay, const std::wstring& needle){
    if(needle.empty()) return true;
    auto lower=[](std::wstring s){ for(auto&c:s) c=(wchar_t)towlower(c); return s; };
    return lower(hay).find(lower(needle))!=std::wstring::npos;
}

// ---- current Jalali date + time strings (for the header clock + records) ---
static std::wstring adTodayJalali(){
    SYSTEMTIME st=iranNow();
    int jy,jm,jd; gregToJalali(st.wYear,st.wMonth,st.wDay,jy,jm,jd);
    wchar_t b[32]; swprintf(b,32,L"%04d/%02d/%02d",jy,jm,jd);
    return b;
}
static std::wstring adNowTime(){
    SYSTEMTIME st=iranNow();
    wchar_t b[16]; swprintf(b,16,L"%02d:%02d:%02d",st.wHour,st.wMinute,st.wSecond);
    return b;
}

// ---- resolve the operator's print-design SECTION (mirrors reception.cpp) ----
static int adResolveSectionId(){
    std::wstring dept=g_session.user.dept;
    std::vector<Section> all; Sections_All(all);
    if(all.empty()) return 0;
    if(!dept.empty()){
        for(const auto& s:all) if(s.is_active && s.name_fa==dept) return s.id;
        for(const auto& s:all) if(s.is_active && s.code==dept) return s.id;
        for(const auto& s:all)
            if(s.is_active && (s.name_fa.find(dept)!=std::wstring::npos ||
                               dept.find(s.name_fa)!=std::wstring::npos)) return s.id;
    }
    for(const auto& s:all) if(s.is_active && s.kind==L"reception") return s.id;
    for(const auto& s:all) if(s.is_active) return s.id;
    return 0;
}

// ============================================================================
//  «صندوق نرفته‌ها» — unpaid / not-yet-cashiered queue.
//  File-backed (data\unpaid_queue.dat) so it survives restarts and stays a
//  single static EXE. One pipe-delimited line per entry:
//    id|nid|first|last|mobile|barcode|jdate|time|epochMin|amount
// ============================================================================
static std::wstring adQueuePath(){ return dataDir()+L"\\unpaid_queue.dat"; }

static long long adEpochMinutes(){
    // minutes since a fixed epoch, from Iran-local wall clock (stable ordering)
    SYSTEMTIME st=iranNow();
    FILETIME ft; SystemTimeToFileTime(&st,&ft);
    ULARGE_INTEGER u; u.LowPart=ft.dwLowDateTime; u.HighPart=ft.dwHighDateTime;
    return (long long)(u.QuadPart/600000000ULL); // 100ns→minutes
}

struct AdQueueRow {
    std::wstring id,nid,first,last,mobile,barcode,jdate,time;
    long long epochMin, amount;
    AdQueueRow():epochMin(0),amount(0){}
};

static std::vector<std::wstring> adSplit(const std::wstring& s, wchar_t d){
    std::vector<std::wstring> out; std::wstring cur;
    for(wchar_t c:s){ if(c==d){ out.push_back(cur); cur.clear(); } else cur+=c; }
    out.push_back(cur); return out;
}
static std::wstring adEscPipe(const std::wstring& s){
    std::wstring o=s; for(auto&c:o){ if(c==L'|') c=L'¦'; if(c==L'\n'||c==L'\r') c=L' '; } return o;
}

static std::vector<AdQueueRow> adQueueLoad(){
    std::vector<AdQueueRow> out;
    std::wstring all=readFileUtf8(adQueuePath());
    size_t pos=0;
    while(pos<all.size()){
        size_t e=all.find(L'\n',pos); if(e==std::wstring::npos) e=all.size();
        std::wstring line=all.substr(pos,e-pos); pos=e+1;
        while(!line.empty()&&(line.back()==L'\r'||line.back()==L' ')) line.pop_back();
        if(line.empty()) continue;
        auto f=adSplit(line,L'|');
        if(f.size()<10) continue;
        AdQueueRow r;
        r.id=f[0]; r.nid=f[1]; r.first=f[2]; r.last=f[3]; r.mobile=f[4];
        r.barcode=f[5]; r.jdate=f[6]; r.time=f[7];
        r.epochMin=_wtoi64(f[8].c_str()); r.amount=_wtoi64(f[9].c_str());
        out.push_back(r);
    }
    return out;
}
static void adQueueSave(const std::vector<AdQueueRow>& rows){
    std::wstring all;
    for(const auto& r:rows){
        wchar_t em[32],am[32];
        swprintf(em,32,L"%lld",r.epochMin); swprintf(am,32,L"%lld",r.amount);
        all += adEscPipe(r.id)+L"|"+adEscPipe(r.nid)+L"|"+adEscPipe(r.first)+L"|"+
               adEscPipe(r.last)+L"|"+adEscPipe(r.mobile)+L"|"+adEscPipe(r.barcode)+L"|"+
               adEscPipe(r.jdate)+L"|"+adEscPipe(r.time)+L"|"+em+L"|"+am+L"\r\n";
    }
    writeFileUtf8(adQueuePath(),all,false);
}
static std::string adQueueJson(){
    auto rows=adQueueLoad();
    long long now=adEpochMinutes();
    // newest first
    std::string o="{\"rows\":[";
    for(size_t i=0;i<rows.size();++i){
        // reverse index for newest-first
        const AdQueueRow& r=rows[rows.size()-1-i];
        if(i)o+=",";
        long long mins=now-r.epochMin; if(mins<0) mins=0;
        std::wstring full=r.first+L" "+r.last;
        o+="{";
        o+="\"id\":"+jstr(r.id)+",";
        o+="\"nid\":"+jstr(r.nid)+",";
        o+="\"first\":"+jstr(r.first)+",";
        o+="\"last\":"+jstr(r.last)+",";
        o+="\"name\":"+jstr(full)+",";
        o+="\"mobile\":"+jstr(r.mobile)+",";
        o+="\"barcode\":"+jstr(r.barcode)+",";
        o+="\"date\":"+jstr(r.jdate)+",";
        o+="\"time\":"+jstr(r.time)+",";
        o+="\"minsAgo\":"+jnum(mins)+",";
        o+="\"amount\":"+jnum(r.amount);
        o+="}";
    }
    o+="]}"; return o;
}

// ---- parse the JS "patient" object out of a record body -------------------
struct AdPatient { std::wstring nid,first,last,father,gender,birth,mobile,phone,addr; };
static AdPatient adReadPatient(const std::string& body){
    AdPatient p;
    std::string patRaw; if(!jsonGetRaw(body,"patient",patRaw)) patRaw=body;
    std::string s;
    if(jsonGetString(patRaw,"nid",s))    p.nid=normDigits(u82w(s));
    if(jsonGetString(patRaw,"first",s))  p.first=u82w(s);
    if(jsonGetString(patRaw,"last",s))   p.last=u82w(s);
    if(jsonGetString(patRaw,"father",s)) p.father=u82w(s);
    if(jsonGetString(patRaw,"gender",s)) p.gender=u82w(s);
    if(jsonGetString(patRaw,"birth",s))  p.birth=normDigits(u82w(s));
    if(jsonGetString(patRaw,"mobile",s)) p.mobile=normDigits(u82w(s));
    if(jsonGetString(patRaw,"phone",s))  p.phone=normDigits(u82w(s));
    if(jsonGetString(patRaw,"addr",s))   p.addr=u82w(s);
    return p;
}

// ---- compute the bill for a record body (server-authoritative) -------------
//  Mirrors the JS calc but is the trusted copy used for persistence + print.
//  آزاد (base pct 0) → patient pays the full net; insured → reduced.
struct AdBill {
    long long gross=0, disc=0, org=0, supp=0, pat=0, paid=0;
    int insIdx=0, suppIdx=0, basePct=0, suppPct=0;
};
static AdBill adComputeBill(const std::string& body){
    AdBill b;
    bool hasIns=jsonGetBool(body,"hasIns",false);
    double d;
    if(jsonGetNumber(body,"insMain",d)) b.insIdx=(int)d;
    if(jsonGetNumber(body,"insSupp",d)) b.suppIdx=(int)d;
    if(jsonGetNumber(body,"insSuppPct",d)) b.suppPct=(int)d;
    // v1.43.0: coverage percentages come from the EDITABLE insurance store.
    if(hasIns && b.insIdx>=0) b.basePct=insBasePctAt(b.insIdx);
    if(b.suppPct<=0 && b.suppIdx>=0) b.suppPct=insSuppPctAt(b.suppIdx);
    if(!hasIns){ b.basePct=0; }

    // walk the services array
    std::string arr; jsonGetRaw(body,"services",arr);
    // iterate objects inside arr
    // v1.44.0 §2.3 FREEZE FIX: hard iteration cap + guaranteed forward progress.
    // A malformed/unbalanced services payload could previously leave `p` stalled
    // (q never advancing past o) and spin the HTTP worker forever — from outside
    // this looked exactly like a total freeze (reply never arrives, _billBusy
    // stays true). The guard and the p=o+1 fallback make a stall impossible.
    size_t p=0;
    int guard=0;
    while(p<arr.size() && ++guard < 10000){
        size_t o=arr.find('{',p); if(o==std::string::npos) break;
        int depth=0; bool inStr=false; size_t q=o;
        for(;q<arr.size();++q){
            char c=arr[q];
            if(inStr){ if(c=='\\'){q++;continue;} if(c=='"') inStr=false; continue; }
            if(c=='"'){ inStr=true; continue; }
            if(c=='{') depth++;
            else if(c=='}'){ depth--; if(depth==0){ q++; break; } }
        }
        // Never let p stall: if the brace scan did not advance past o, force it.
        if(q<=o){ p=o+1; continue; }
        std::string obj=arr.substr(o,q-o);
        p=q;
        double price=0, qty=1, discount=0;
        jsonGetNumber(obj,"price",price);
        jsonGetNumber(obj,"qty",qty);
        jsonGetNumber(obj,"discount",discount);
        long long g=(long long)llround(price*(qty>0?qty:1));
        long long ds=(long long)llround(discount);
        long long net=g-ds; if(net<0) net=0;
        long long org=(long long)llround((double)net*b.basePct/100.0);
        long long afterBase=net-org;
        long long sup=(long long)llround((double)afterBase*b.suppPct/100.0);
        long long patShare=afterBase-sup;
        b.gross+=g; b.disc+=ds; b.org+=org; b.supp+=sup; b.pat+=patShare;
    }
    if(guard>=10000){
        ClientLog_Warn(L"admission", L"bill.compute parser guard tripped",
            L"{\"cap\":10000}");
    }
    bool noPay=jsonGetBool(body,"noPay",false);
    b.paid = noPay ? 0 : b.pat;
    return b;
}

// ---- build a ReceptionRecord from a record body ---------------------------
static ReceptionRecord adBuildRecord(const std::string& body){
    AdPatient p=adReadPatient(body);
    AdBill b=adComputeBill(body);
    ReceptionRecord r;
    r.firstName=p.first; r.lastName=p.last; r.nationalId=p.nid;
    r.fatherName=p.father; r.birthDate=p.birth; r.gender=p.gender;
    r.mobile=p.mobile; r.landline=p.phone; r.address=p.addr;
    std::string s;
    if(jsonGetString(body,"ptype",s)) r.patientType=u82w(s);
    if(jsonGetString(body,"apptDate",s)) r.apptDate=normDigits(u82w(s));
    if(jsonGetString(body,"apptShift",s)) r.shift=u82w(s);
    if(jsonGetString(body,"doc2name",s)) r.dept=u82w(s); // treating doctor label
    r.apptTime=adNowTime();
    r.userName=g_session.user.username;
    r.insIdx=b.insIdx; r.suppIdx=b.suppIdx;
    // v1.43.0: names come from the editable insurance store (index-aligned).
    if(r.insIdx>=0) r.insurance=insBaseNameAt(r.insIdx);
    if(r.suppIdx>=0) r.suppInsurance=insSuppNameAt(r.suppIdx);
    r.total=b.gross; r.discount=b.disc; r.orgShare=b.org+b.supp;
    r.mainShare=b.org; r.patientShare=b.pat; r.finalTotal=b.gross;
    r.paid=b.paid;
    r.basePct=b.basePct; r.suppPct=b.suppPct;

    // v1.44.0 §4.3: capture per-line items so the print designer's "services"
    // table subtype can render one row per service. Same tolerant brace-walk as
    // adComputeBill (with the same hard iteration cap + forward-progress guard).
    {
        std::string arr; jsonGetRaw(body,"services",arr);
        size_t p=0; int guard=0;
        while(p<arr.size() && ++guard < 10000){
            size_t o=arr.find('{',p); if(o==std::string::npos) break;
            int depth=0; bool inStr=false; size_t q=o;
            for(;q<arr.size();++q){ char c=arr[q];
                if(inStr){ if(c=='\\'){q++;continue;} if(c=='"') inStr=false; continue; }
                if(c=='"'){ inStr=true; continue; }
                if(c=='{') depth++; else if(c=='}'){ depth--; if(depth==0){ q++; break; } } }
            if(q<=o){ p=o+1; continue; }
            std::string obj=arr.substr(o,q-o); p=q;
            ReceptionLine ln;
            std::string sv; if(jsonGetString(obj,"code",sv)) ln.code=u82w(sv);
            if(jsonGetString(obj,"name",sv)) ln.name=u82w(sv);
            double price=0, qty=1, discount=0;
            jsonGetNumber(obj,"price",price);
            jsonGetNumber(obj,"qty",qty);
            jsonGetNumber(obj,"discount",discount);
            ln.qty=(int)(qty>0?qty:1);
            ln.price=(long long)llround(price);
            ln.discount=(long long)llround(discount);
            long long g=(long long)llround(price*ln.qty);
            long long net=g-ln.discount; if(net<0) net=0;
            ln.insShare=(long long)llround((double)net*b.basePct/100.0);
            long long afterBase=net-ln.insShare;
            long long sup=(long long)llround((double)afterBase*b.suppPct/100.0);
            ln.insShare+=sup;              // total insurer share (base + supp)
            ln.patShare=afterBase-sup;
            r.services.push_back(ln);
        }
    }
    return r;
}

// ---------------------------------------------------------------------------
//  admissionApi — route a verb + JSON payload to a JSON response.
// ---------------------------------------------------------------------------
static std::string admissionApi(const std::string& verb, const std::string& body){
    // ---- init: send everything the page needs to render ----
    if(verb=="init"||verb=="ready"){
        extern bool g_dark;   // theme.cpp — current palette (dark/light)
        std::string o="{";
        o+="\"ok\":true,";
        o+="\"insurances\":"+insurancesJson()+",";
        o+="\"supp\":"+suppInsurancesJson()+",";
        o+="\"date\":"+jstr(adTodayJalali())+",";
        o+="\"time\":"+jstr(adNowTime())+",";
        // v1.43.0: hand the current theme to the web UI so it opens already in
        // sync with the native shell (dark ⇄ light) — no flash of the wrong theme.
        o+="\"theme\":"+std::string(g_dark?"\"dark\"":"\"light\"")+",";
        o+="\"ps\":{\"P\":0,\"S\":0}";
        o+="}";
        return o;
    }

    // ---- theme.get: current palette (used by JS on demand) -----------------
    if(verb=="theme.get"){
        extern bool g_dark;
        return std::string("{\"theme\":")+(g_dark?"\"dark\"":"\"light\"")+"}";
    }

    // ---- insurance.list: full editable store («مدیریت بیمه‌ها») -------------
    if(verb=="insurance.list"){
        auto all=loadInsuranceRows();
        std::string o="{\"rows\":[";
        for(size_t i=0;i<all.size();i++){
            if(i)o+=",";
            o+="{\"name\":"+jstr(all[i].name)+",\"pct\":"+jnum(all[i].pct)+
               ",\"supp\":"+jnum(all[i].supp)+",\"status\":"+jnum(all[i].status)+"}";
        }
        o+="]}"; return o;
    }
    // ---- insurance.save: replace the whole store with the posted rows ------
    if(verb=="insurance.save"){
        std::string arr; if(!jsonGetRaw(body,"rows",arr)) arr="[]";
        std::vector<InsuranceRow> rows;
        size_t p=0;
        while(p<arr.size()){
            size_t o=arr.find('{',p); if(o==std::string::npos) break;
            int depth=0; bool inStr=false; size_t q=o;
            for(;q<arr.size();++q){ char c=arr[q];
                if(inStr){ if(c=='\\'){q++;continue;} if(c=='"') inStr=false; continue; }
                if(c=='"'){ inStr=true; continue; }
                if(c=='{') depth++; else if(c=='}'){ depth--; if(depth==0){ q++; break; } } }
            std::string obj=arr.substr(o,q-o); p=q;
            InsuranceRow r; std::string s; double d;
            if(jsonGetString(obj,"name",s)) r.name=u82w(s);
            if(r.name.empty()) continue;
            if(jsonGetNumber(obj,"pct",d)) r.pct=(int)d;
            if(jsonGetNumber(obj,"supp",d)) r.supp=((int)d)?1:0;
            if(jsonGetNumber(obj,"status",d)) r.status=((int)d)?1:0; else r.status=1;
            rows.push_back(r);
        }
        saveInsuranceRows(rows);
        WebAdmission_NotifyCatalogChanged();   // push fresh selectors to open views
        return "{\"ok\":true}";
    }

    // ---- clock poll (JS ticks locally; this keeps Jalali date fresh) ----
    if(verb=="clock"){
        return "{\"time\":"+jstr(adNowTime())+",\"date\":"+jstr(adTodayJalali())+"}";
    }

    // ---- patient.lookup: national-id -> full record (local store first) ----
    //  v1.40.0: build a PatientRow so landline/address/suppIdx round-trip too.
    if(verb=="patient.lookup"){
        std::string nidU; jsonGetString(body,"nid",nidU);
        std::wstring nid=normDigits(u82w(nidU));
        if(nid.empty()) return "{\"found\":false}";
        auto all=loadAllPatients();
        for(const auto& p:all){
            if(normDigits(p.nid)==nid){
                { std::lock_guard<std::mutex> lk(g_adLastNidMx); g_adLastFilledNid=w2u8(nid); }
                return "{\"found\":true,\"patient\":"+patientJson(p)+"}";
            }
        }
        CitizenInfo c=lookupCitizen(nid);
        if(c.found){
            PatientRow p;
            p.nid=nid; p.first=c.firstName; p.last=c.lastName; p.father=c.fatherName;
            p.gender=c.gender; p.birth=c.birthDate; p.mobile=c.mobile;
            p.landline=c.landline; p.address=c.address;   // v1.40.0
            p.insurances=c.insurances; p.suppIdx=c.suppIdx;
            { std::lock_guard<std::mutex> lk(g_adLastNidMx); g_adLastFilledNid=w2u8(nid); }
            return "{\"found\":true,\"patient\":"+patientJson(p)+"}";
        }
        return "{\"found\":false}";
    }

    // ---- patient.search: free-text over the local store ----
    if(verb=="patient.search"){
        std::string qU; jsonGetString(body,"q",qU);
        std::wstring q=u82w(qU);
        std::wstring qd=normDigits(q);
        auto all=loadAllPatients();
        std::string o="{\"rows\":[";
        int n=0;
        for(const auto& p:all){
            std::wstring full=p.first+L" "+p.last;
            bool hit = q.empty() || wcontains(full,q) ||
                       normDigits(p.nid).find(qd)!=std::wstring::npos ||
                       normDigits(p.mobile).find(qd)!=std::wstring::npos;
            if(!hit) continue;
            if(n++) o+=",";
            o+=patientJson(p);   // v1.40.0: struct overload → carries phone/addr/suppIdx
            if(n>=50) break;
        }
        o+="]}"; return o;
    }

    // ---- service.search: over the Management service catalog ----
    if(verb=="service.search"){
        std::string qU; jsonGetString(body,"q",qU);
        std::wstring q=u82w(qU);
        auto svcs=snapshotServices();   // v1.44.0 §2.4: safe local snapshot
        std::string o="{\"rows\":[";
        int n=0;
        for(const auto& s:svcs){
            if(s.status==0) continue;
            bool hit = q.empty() || wcontains(s.name,q) ||
                       normDigits(s.code).find(normDigits(q))!=std::wstring::npos;
            if(!hit) continue;
            if(n++) o+=",";
            o+=serviceJson(s);
            if(n>=50) break;
        }
        o+="]}"; return o;
    }

    // ---- service.resolve: code -> {name, price} (management-driven) ----
    if(verb=="service.resolve"){
        std::string codeU; jsonGetString(body,"code",codeU);
        const ServiceDef* sd=findService(u82w(codeU));
        if(!sd) sd=findService(normDigits(u82w(codeU)));
        if(sd && sd->status!=0)
            return "{\"found\":true,\"service\":"+serviceJson(*sd)+"}";
        return "{\"found\":false}";
    }

    // ---- doctor.search (also serves doctor-by-code: index+1 acts as code) --
    if(verb=="doctor.search"){
        std::string qU; jsonGetString(body,"q",qU);
        std::string codeU; jsonGetString(body,"code",codeU);
        std::wstring q=u82w(qU);
        std::wstring code=normDigits(u82w(codeU));
        auto docs=loadDoctors();
        std::string o="{\"rows\":[";
        int n=0;
        for(size_t i=0;i<docs.size();++i){
            const auto& d=docs[i];
            wchar_t cb[16]; swprintf(cb,16,L"%d",(int)i+1);
            std::wstring dcode=cb;
            bool hit;
            if(!code.empty()) hit = (dcode==code) || (normDigits(q).empty()?false:false);
            else hit = q.empty() || wcontains(d.name,q) || wcontains(d.specialty,q);
            if(!hit) continue;
            if(n++) o+=",";
            o+="{\"name\":"+jstr(d.name)+",\"specialty\":"+jstr(d.specialty)+
               ",\"code\":"+jstr(dcode)+"}";
            if(n>=50) break;
        }
        o+="]}"; return o;
    }

    // ---- bill.compute: server-authoritative recompute (optional) -----------
    if(verb=="bill.compute"){
        AdBill b=adComputeBill(body);
        std::string o="{";
        o+="\"gross\":"+jnum(b.gross)+",";
        o+="\"disc\":"+jnum(b.disc)+",";
        o+="\"org\":"+jnum(b.org)+",";
        o+="\"supp\":"+jnum(b.supp)+",";
        o+="\"pat\":"+jnum(b.pat)+",";
        o+="\"paid\":"+jnum(b.paid);
        o+="}";
        return o;
    }

    // ---- queue.list: صندوق نرفته‌ها -----------------------------------------
    if(verb=="queue.list"){
        return adQueueJson();
    }

    // ---- queue.add: push current admission into صندوق نرفته‌ها --------------
    if(verb=="queue.add"){
        AdPatient p=adReadPatient(body);
        if(p.nid.empty()) return "{\"ok\":false,\"err\":\"کد ملی خالی است\"}";
        AdBill b=adComputeBill(body);
        auto rows=adQueueLoad();
        AdQueueRow r;
        wchar_t idb[32]; swprintf(idb,32,L"%lld",adEpochMinutes());
        r.id=idb; r.nid=p.nid; r.first=p.first; r.last=p.last; r.mobile=p.mobile;
        std::string bc; jsonGetString(body,"insBooklet",bc);
        r.barcode=normDigits(u82w(bc)); if(r.barcode.empty()) r.barcode=p.nid;
        r.jdate=adTodayJalali(); r.time=adNowTime();
        r.epochMin=adEpochMinutes(); r.amount=b.pat;
        rows.push_back(r);
        adQueueSave(rows);
        logLine(L"unpaid-queue add: "+p.nid);
        return "{\"ok\":true}";
    }

    // ---- queue.remove -------------------------------------------------------
    if(verb=="queue.remove"){
        std::string idU; jsonGetString(body,"id",idU);
        std::wstring id=u82w(idU);
        auto rows=adQueueLoad();
        std::vector<AdQueueRow> keep;
        for(const auto& r:rows) if(r.id!=id) keep.push_back(r);
        adQueueSave(keep);
        return "{\"ok\":true}";
    }

    // ---- admission.save: persist + PRINT per the Management print design ----
    if(verb=="admission.save"){
        AdPatient p=adReadPatient(body);
        if(p.nid.empty())
            return "{\"ok\":false,\"err\":\"کد ملی خالی است\"}";
        ReceptionRecord r=adBuildRecord(body);
        if(r.total<=0)
            return "{\"ok\":false,\"err\":\"حداقل یک خدمت با مبلغ لازم است\"}";

        int q=saveReception(r);   // assigns queue no + persists CSV

        // remember this operator-confirmed identity (never fabricated).
        // v1.40.0: also persist landline «تلفن ثابت», address «آدرس» and the
        // supplementary-insurance index so the same code recalls every field.
        std::vector<int> ins;
        if(r.insIdx>0 && r.insIdx<N_INSURANCES) ins.push_back(r.insIdx);
        rememberPatient(r.nationalId,r.firstName,r.lastName,r.fatherName,
            r.gender,r.birthDate,r.mobile,
            r.landline,r.address,   // v1.40.0
            ins,r.suppIdx);         // v1.40.0
        saveLastReceipt(r);

        // PRINT per the Management «دیزاین چاپ» template, same priority order
        // reception.cpp uses: 1) new print-design bound to the operator SECTION
        //                     2) legacy per-section designed receipt (idx 0)
        //                     3) classic GDI receipt
        //
        // CRITICAL (v1.41.0 freeze fix): printing opens a printer DC and MAY
        // show a modal PrintDlgW. This handler runs SYNCHRONOUSLY inside the
        // WebView2 WebMessageReceived callback (UI thread) or on an HTTP worker
        // thread. Doing GDI/modal work there deadlocks the WebView (the classic
        // "everything is frozen, no button responds" symptom) or touches HWNDs
        // from a worker thread. So we DEFER the print + cash-drawer to the GUI
        // thread via RunOnUiThread (async PostMessage) and reply to JS instantly.
        int  sid=adResolveSectionId();
        {
            ReceptionRecord rCopy=r;      // capture by value for the deferred task
            RunOnUiThread([rCopy,sid](){
                HWND owner=g_hFrame;
                bool done=false;
                std::wstring printMode=L"none";
                if(sid>0){
                    done=printPrintDesign(rCopy,sid,owner);
                    if(done) printMode=L"print-design section="+std::to_wstring(sid);
                }
                if(!done){
                    done=printDesignedReceipt(rCopy,0,owner);
                    if(done) printMode=L"designed-receipt idx=0";
                }
                if(!done){
                    printReceipt(rCopy,2,owner);
                    printMode=L"classic-gdi";
                }
                kickCashDrawer();
                logLine(L"admission printed via: "+printMode+L"  nid="+rCopy.nationalId);
                if(g_hFrame) PostMessageW(g_hFrame,WM_APP_DESIGN_PUSHED,0,0);
            });
        }

        // Reply immediately — the UI stays responsive; printing happens next.
        std::string o="{\"ok\":true,\"queueNo\":"+jnum(q)+
            ",\"printMode\":"+jstr(L"deferred")+
            ",\"ps\":{\"P\":"+jnum(r.paid)+
            ",\"S\":"+jnum(r.patientShare-r.paid)+"}}";
        return o;
    }

    if(verb=="admission.clear"||verb=="admission.new"||
       verb=="ui.toggleServices"||verb=="ui.toggleQueue"||
       verb=="ui.settings"){
        return "{\"ok\":true}";
    }

    // ---- print.* : route to the real GDI print pipeline --------------------
    //
    // CRITICAL (v1.42.0 freeze fix): identical rationale to admission.save.
    // printLastReceipt / printReceipt / printPrintDesign open a printer DC and
    // MAY show a modal PrintDlgW. This handler runs SYNCHRONOUSLY inside the
    // WebView2 WebMessageReceived callback (UI thread) or on an HTTP worker
    // thread — doing GDI/modal work there deadlocks the WebView (frozen UI) or
    // touches HWNDs off-thread. So we DEFER every print variant to the GUI
    // thread via RunOnUiThread and reply to JS instantly.
    if(verb.rfind("print.",0)==0){
        std::string v=verb;
        if(v=="print.last"){
            RunOnUiThread([](){
                printLastReceipt(g_hFrame);
                logLine(L"admission print (web): print.last");
            });
            return "{\"ok\":true,\"printMode\":\"deferred\"}";
        }
        ReceptionRecord r;
        bool have=false;
        if(!body.empty() && body!="{}"){ r=adBuildRecord(body); have=(r.total>0)||!r.nationalId.empty(); }
        if(!have) loadLastReceipt(r);
        int sid=adResolveSectionId();
        {
            ReceptionRecord rCopy=r;   // capture by value for the deferred task
            RunOnUiThread([rCopy,sid,v](){
                HWND owner=g_hFrame;
                if(v=="print.insurance"){ printReceipt(rCopy,0,owner); }
                else if(v=="print.rx"){   printReceipt(rCopy,1,owner); }
                else if(v=="print.receipt"){
                    bool done=false;
                    if(sid>0) done=printPrintDesign(rCopy,sid,owner);
                    if(!done) done=printDesignedReceipt(rCopy,0,owner);
                    if(!done) printReceipt(rCopy,2,owner);
                }
                logLine(L"admission print (web): "+u82w(v));
            });
        }
        return "{\"ok\":true,\"printMode\":\"deferred\"}";
    }

    if(verb=="rx.electronic"){
        logLine(L"electronic prescription request (web)");
        return "{\"ok\":true}";
    }

    return "{\"ok\":false,\"err\":\"verb ناشناخته\"}";
}

// ============================================================================
//  admissionApiSafe (v1.43.0 FREEZE HARDENING)
//  ----------------------------------------------------------------------------
//  Every /api call — whether served on an HTTP worker thread (MSHTML) or
//  synchronously inside the WebView2 WebMessageReceived callback (UI thread) —
//  goes through this wrapper. It guarantees the handler can NEVER take down the
//  transport with an unhandled C++ exception (which would silently kill the
//  worker / re-enter the WebView message pump and manifest as the "everything
//  is frozen" symptom the user reported). A thrown handler now returns a clean
//  JSON error instead, so the UI always gets a reply and stays responsive.
// ============================================================================
// ----------------------------------------------------------------------------
// v1.44.0 §2.2 — Structured-exception hardening for the admission verbs.
//
// The MinGW-w64 32-bit toolchain this project MUST keep (guardrail: no
// toolchain switch) is built --with-dwarf2 / --disable-sjlj-exceptions, so the
// MSVC-style `__try/__except` keywords are NOT available and a Windows
// structured exception (AV, int-div-0, stack overflow, …) is NOT routed through
// C++ `catch(...)`. Without a net, an AV inside a verb kills the loopback HTTP
// worker silently: no reply is ever sent, the JS promise hangs forever, and the
// operator sees a TOTAL freeze (the reported symptom).
//
// Portable fix that compiles on DWARF GCC: a thread-scoped Vectored Exception
// Handler that, on a genuine fault code, logs ONE structured client.log "crash"
// line (verb + code + fault address + top-8 stack RVAs) and then longjmp()s
// back to a setjmp target established just before the handler runs.
// admissionApiSafe therefore ALWAYS returns a clean JSON reply, so the transport
// is never taken down by a structured exception. The g_sehArmed thread-local
// gates the VEH so it only ever redirects faults raised inside an admission
// verb — everything else falls through to the normal crash-dump chain.
// ----------------------------------------------------------------------------
#include <csetjmp>

static thread_local jmp_buf     g_sehJmp;
static thread_local bool        g_sehArmed = false;
static thread_local std::string g_sehVerb;

static LONG CALLBACK adSehVeh(EXCEPTION_POINTERS* ep){
    if(!g_sehArmed || !ep || !ep->ExceptionRecord) return EXCEPTION_CONTINUE_SEARCH;
    DWORD code = ep->ExceptionRecord->ExceptionCode;
    bool fatal =
        code==EXCEPTION_ACCESS_VIOLATION ||
        code==EXCEPTION_INT_DIVIDE_BY_ZERO ||
        code==EXCEPTION_ILLEGAL_INSTRUCTION ||
        code==EXCEPTION_ARRAY_BOUNDS_EXCEEDED ||
        code==EXCEPTION_STACK_OVERFLOW ||
        code==EXCEPTION_PRIV_INSTRUCTION;
    if(!fatal) return EXCEPTION_CONTINUE_SEARCH;

    void* addr = ep->ExceptionRecord->ExceptionAddress;
    void* frames[8]={0};
    USHORT nf = CaptureStackBackTrace(0, 8, frames, NULL);
    HMODULE hExe = GetModuleHandleW(NULL);
    std::wstring rvas=L"[";
    for(USHORT i=0;i<nf;i++){
        if(i) rvas+=L",";
        unsigned long long rva=(unsigned long long)((BYTE*)frames[i]-(BYTE*)hExe);
        wchar_t b[32]; swprintf(b,32,L"%llu",rva); rvas+=b;
    }
    rvas+=L"]";
    wchar_t codeHex[16]; swprintf(codeHex,16,L"0x%08X",(unsigned)code);
    wchar_t addrB[32];   swprintf(addrB,32,L"%p",addr);
    std::wstring verbW = u82w(g_sehVerb);
    std::wstring extra = L"{\"verb\":\"" + verbW + L"\",\"code\":\"" +
        std::wstring(codeHex) + L"\",\"addr\":\"" + std::wstring(addrB) +
        L"\",\"frames\":" + rvas + L"}";
    ClientLog_Crash(L"admission", L"admissionApi SEH", extra);

    g_sehArmed = false;
    longjmp(g_sehJmp, 1);              // unwind back to admissionApiSafe
    return EXCEPTION_EXECUTE_HANDLER;  // unreachable
}

// Inner worker: owns the C++ objects (destructors OK). Runs the existing
// std::exception-hardened path.
static std::string admissionApiInner(const std::string& verb, const std::string& body){
    try {
        return admissionApi(verb,body);
    } catch(const std::exception& ex){
        logLine(L"admissionApi exception: "+u82w(std::string(ex.what()))+L" verb="+u82w(verb));
        ClientLog_Error(L"admission", L"admissionApi std::exception",
            L"{\"verb\":\""+u82w(verb)+L"\"}");
        return "{\"ok\":false,\"err\":\"خطای داخلی سرور\"}";
    } catch(...){
        logLine(L"admissionApi unknown exception verb="+u82w(verb));
        ClientLog_Error(L"admission", L"admissionApi unknown exception",
            L"{\"verb\":\""+u82w(verb)+L"\"}");
        return "{\"ok\":false,\"err\":\"خطای ناشناخته سرور\"}";
    }
}

static std::string admissionApiSafe(const std::string& verb, const std::string& body){
    static volatile LONG s_vehInstalled=0;
    if(InterlockedCompareExchange(&s_vehInstalled,1,0)==0){
        // FIRST handler in the chain so we see the fault before the process-wide
        // crashFilter; the g_sehArmed thread-local gates it to admission calls.
        AddVectoredExceptionHandler(1, adSehVeh);
    }
    g_sehVerb = verb;
    if(setjmp(g_sehJmp)!=0){
        // Longjmp'd here from the VEH after a structured exception (already
        // logged inside adSehVeh). Return a clean JSON error so the JS side
        // always gets a reply and never wedges on a pending promise.
        g_sehArmed=false;
        return "{\"ok\":false,\"err\":\"SEH\"}";
    }
    g_sehArmed = true;
    std::string r = admissionApiInner(verb, body);
    g_sehArmed = false;
    return r;
}
