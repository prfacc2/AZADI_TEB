// ============================================================================
//  web_admission_api.inc — the C++ side of the admission IPC bridge.
//  Included into web_admission.cpp. Every verb the JS UI can send is handled
//  here against the REAL data layer (patients, services, insurances, doctors).
//  Returns a JSON string. This is the single source of truth — the UI never
//  fabricates prices or identities.
// ============================================================================

// ---- serialize a PatientRow / CitizenInfo to JSON --------------------------
static std::string patientJson(const std::wstring& nid, const std::wstring& first,
        const std::wstring& last, const std::wstring& father, const std::wstring& gender,
        const std::wstring& birth, const std::wstring& mobile, const std::vector<int>& ins){
    std::string o="{";
    o+="\"nid\":"+jstr(nid)+",";
    o+="\"first\":"+jstr(first)+",";
    o+="\"last\":"+jstr(last)+",";
    o+="\"father\":"+jstr(father)+",";
    o+="\"gender\":"+jstr(gender)+",";
    o+="\"birth\":"+jstr(birth)+",";
    o+="\"mobile\":"+jstr(mobile)+",";
    o+="\"insurances\":[";
    for(size_t i=0;i<ins.size();++i){ if(i)o+=","; o+=jnum(ins[i]); }
    o+="]}";
    return o;
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
static std::string insurancesJson(){
    std::string o="[";
    for(int i=0;i<N_INSURANCES;i++){
        if(i)o+=",";
        o+="{\"name\":"+jstr(INSURANCES[i].name)+",\"pct\":"+jnum(INSURANCES[i].pct)+"}";
    }
    o+="]"; return o;
}
static std::string suppInsurancesJson(){
    std::string o="[";
    for(int i=0;i<N_SUPP;i++){
        if(i)o+=",";
        o+="{\"name\":"+jstr(SUPP_INSURANCES[i].name)+",\"pct\":"+jnum(SUPP_INSURANCES[i].pct)+"}";
    }
    o+="]"; return o;
}

// case-insensitive contains for wide strings (Persian is unaffected)
static bool wcontains(const std::wstring& hay, const std::wstring& needle){
    if(needle.empty()) return true;
    auto lower=[](std::wstring s){ for(auto&c:s) c=(wchar_t)towlower(c); return s; };
    return lower(hay).find(lower(needle))!=std::wstring::npos;
}

// ---------------------------------------------------------------------------
//  admissionApi — route a verb + JSON payload to a JSON response.
//  `hostFrame` is g_hFrame so save/print can notify the app.
// ---------------------------------------------------------------------------
static std::string admissionApi(const std::string& verb, const std::string& body){
    // ---- init: send everything the page needs to render ----
    if(verb=="init"||verb=="ready"){
        std::string o="{";
        o+="\"ok\":true,";
        o+="\"insurances\":"+insurancesJson()+",";
        o+="\"supp\":"+suppInsurancesJson();
        o+="}";
        return o;
    }

    // ---- patient.lookup: national-id -> full record (local store first) ----
    if(verb=="patient.lookup"){
        std::string nidU; jsonGetString(body,"nid",nidU);
        std::wstring nid=normDigits(u82w(nidU));
        if(nid.empty()) return "{\"found\":false}";
        // 1) local store
        auto all=loadAllPatients();
        for(const auto& p:all){
            if(normDigits(p.nid)==nid){
                return "{\"found\":true,\"patient\":"+
                    patientJson(p.nid,p.first,p.last,p.father,p.gender,p.birth,p.mobile,p.insurances)+"}";
            }
        }
        // 2) citizen lookup (verified sources only — never fabricates)
        CitizenInfo c=lookupCitizen(nid);
        if(c.found){
            return "{\"found\":true,\"patient\":"+
                patientJson(nid,c.firstName,c.lastName,c.fatherName,c.gender,c.birthDate,c.mobile,c.insurances)+"}";
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
            o+=patientJson(p.nid,p.first,p.last,p.father,p.gender,p.birth,p.mobile,p.insurances);
            if(n>=50) break;
        }
        o+="]}"; return o;
    }

    // ---- service.search: over the Management service catalog ----
    if(verb=="service.search"){
        std::string qU; jsonGetString(body,"q",qU);
        std::wstring q=u82w(qU);
        auto svcs=loadServices();
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

    // ---- doctor.search ----
    if(verb=="doctor.search"){
        std::string qU; jsonGetString(body,"q",qU);
        std::wstring q=u82w(qU);
        auto docs=loadDoctors();
        std::string o="{\"rows\":[";
        int n=0;
        for(const auto& d:docs){
            bool hit = q.empty() || wcontains(d.name,q) || wcontains(d.specialty,q);
            if(!hit) continue;
            if(n++) o+=",";
            o+="{\"name\":"+jstr(d.name)+",\"specialty\":"+jstr(d.specialty)+",\"code\":\"\"}";
            if(n>=50) break;
        }
        o+="]}"; return o;
    }

    // ---- admission.save: persist the patient + acknowledge ----
    if(verb=="admission.save"){
        std::string patRaw; jsonGetRaw(body,"patient",patRaw);
        std::string nidU,firstU,lastU,fatherU,genderU,birthU,mobileU;
        jsonGetString(patRaw,"nid",nidU);
        jsonGetString(patRaw,"first",firstU);
        jsonGetString(patRaw,"last",lastU);
        jsonGetString(patRaw,"father",fatherU);
        jsonGetString(patRaw,"gender",genderU);
        jsonGetString(patRaw,"birth",birthU);
        jsonGetString(patRaw,"mobile",mobileU);
        std::wstring nid=normDigits(u82w(nidU));
        if(nid.empty())
            return "{\"ok\":false,\"err\":\"کد ملی خالی است\"}";
        double insMain=0; jsonGetNumber(body,"insMain",insMain);
        std::vector<int> ins; if((int)insMain>0) ins.push_back((int)insMain);
        // persist to the local patient store (dedup by national id)
        rememberPatient(nid,u82w(firstU),u82w(lastU),u82w(fatherU),
            u82w(genderU),normDigits(u82w(birthU)),normDigits(u82w(mobileU)),ins);
        logLine(L"admission saved (web): "+nid);
        // notify the app frame (updater/print path can hook this later)
        if(g_hFrame) PostMessageW(g_hFrame,WM_APP_DESIGN_PUSHED,0,0);
        return "{\"ok\":true,\"ps\":{\"P\":0,\"S\":0}}";
    }

    if(verb=="admission.clear"||verb=="admission.new"||
       verb=="ui.toggleServices"||verb=="ui.toggleQueue"){
        return "{\"ok\":true}";
    }

    // ---- print.* : acknowledge (real GDI print path can be wired here) ----
    if(verb.rfind("print.",0)==0){
        logLine(L"admission print request (web): "+u82w(verb));
        return "{\"ok\":true}";
    }

    return "{\"ok\":false,\"err\":\"verb ناشناخته\"}";
}
