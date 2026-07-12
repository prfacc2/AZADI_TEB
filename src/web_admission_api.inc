// ============================================================================
//  web_admission_api.inc — the C++ side of the admission IPC bridge.
//  Included into web_admission.cpp. Every verb the JS UI can send is handled
//  here against the REAL data layer (patients, services, insurances, doctors,
//  reception persistence + the Management print design). Returns a JSON string.
//  This is the single source of truth — the UI never fabricates prices,
//  identities or bill amounts.
// ============================================================================

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
    if(b.insIdx>=0 && b.insIdx<N_INSURANCES && hasIns) b.basePct=INSURANCES[b.insIdx].pct;
    if(b.suppPct<=0 && b.suppIdx>=0 && b.suppIdx<N_SUPP) b.suppPct=SUPP_INSURANCES[b.suppIdx].pct;
    if(!hasIns){ b.basePct=0; }

    // walk the services array
    std::string arr; jsonGetRaw(body,"services",arr);
    // iterate objects inside arr
    size_t p=0;
    while(p<arr.size()){
        size_t o=arr.find('{',p); if(o==std::string::npos) break;
        int depth=0; bool inStr=false; size_t q=o;
        for(;q<arr.size();++q){
            char c=arr[q];
            if(inStr){ if(c=='\\'){q++;continue;} if(c=='"') inStr=false; continue; }
            if(c=='"'){ inStr=true; continue; }
            if(c=='{') depth++;
            else if(c=='}'){ depth--; if(depth==0){ q++; break; } }
        }
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
    if(r.insIdx>=0 && r.insIdx<N_INSURANCES) r.insurance=INSURANCES[r.insIdx].name;
    if(r.suppIdx>=0 && r.suppIdx<N_SUPP) r.suppInsurance=SUPP_INSURANCES[r.suppIdx].name;
    r.total=b.gross; r.discount=b.disc; r.orgShare=b.org+b.supp;
    r.mainShare=b.org; r.patientShare=b.pat; r.finalTotal=b.gross;
    r.paid=b.paid;
    return r;
}

// ---------------------------------------------------------------------------
//  admissionApi — route a verb + JSON payload to a JSON response.
// ---------------------------------------------------------------------------
static std::string admissionApi(const std::string& verb, const std::string& body){
    // ---- init: send everything the page needs to render ----
    if(verb=="init"||verb=="ready"){
        std::string o="{";
        o+="\"ok\":true,";
        o+="\"insurances\":"+insurancesJson()+",";
        o+="\"supp\":"+suppInsurancesJson()+",";
        o+="\"date\":"+jstr(adTodayJalali())+",";
        o+="\"time\":"+jstr(adNowTime())+",";
        o+="\"ps\":{\"P\":0,\"S\":0}";
        o+="}";
        return o;
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
