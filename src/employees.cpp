// ============================================================================
//  employees.cpp — department categories, extended employee profiles,
//  online-presence tracking, and the management cartable (inbox) message store.
//  All file-backed (data\*.dat) so it stays a single static EXE and is ready
//  to be swapped for a DB/REST layer later without touching the UI.
// ============================================================================
#include "app.h"
#include <stdio.h>
#include <algorithm>
#include <ctime>

static std::vector<std::wstring> splitPipe(const std::wstring& s){
    std::vector<std::wstring> out; size_t pos=0;
    while(true){ size_t e=s.find(L'|',pos);
        if(e==std::wstring::npos){ out.push_back(s.substr(pos)); break; }
        out.push_back(s.substr(pos,e-pos)); pos=e+1; }
    return out;
}
static std::wstring pipeEsc(const std::wstring& s){
    std::wstring o=s; for(auto&c:o) if(c==L'|'||c==L'\n'||c==L'\r') c=L' '; return o;
}

// ============================================================ departments ====
static std::wstring deptsPath(){ return dataDir()+L"\\depts.dat"; }
std::vector<DeptCat> loadDepts(){
    std::vector<DeptCat> out;
    std::wstring all=readFileUtf8(deptsPath());
    size_t pos=0;
    while(pos<all.size()){
        size_t e=all.find(L'\n',pos); if(e==std::wstring::npos) e=all.size();
        std::wstring line=trim(all.substr(pos,e-pos)); pos=e+1;
        if(line.empty()) continue;
        auto f=splitPipe(line);
        if(f.size()<4) continue;
        DeptCat c; c.id=f[0]; c.name=f[1]; c.manager=f[2]; c.icon=f[3];
        // §H: preserve any future extra columns (re-escaped on save below).
        for(size_t i=4;i<f.size();i++){ c.extra+=L"|"; c.extra+=pipeEsc(f[i]); }
        out.push_back(c);
    }
    return out;
}
static void saveDepts(const std::vector<DeptCat>& v){
    std::wstring out;
    for(auto&c:v) out+=pipeEsc(c.id)+L"|"+pipeEsc(c.name)+L"|"+pipeEsc(c.manager)+L"|"+pipeEsc(c.icon)+c.extra+L"\r\n";
    writeFileUtf8(deptsPath(),out,false);
}
//  v1.4.1: make sure the default «پذیرش» category always exists so the
//  management panel and the per-section print designs have a baseline section.
void seedDefaultDepts(){
    auto v=loadDepts();
    for(auto& c:v) if(c.name==L"پذیرش") return;     // already present
    DeptCat p; p.id=L"DEP_PAZIRESH"; p.name=L"پذیرش";
    p.manager=L""; p.icon=L"shield";
    v.insert(v.begin(),p);
    saveDepts(v);
    logLine(L"seeded default dept: پذیرش");
}
bool addDept(const DeptCat& c, std::wstring& err){
    if(trim(c.name).empty()){ err=L"نام بخش نمی‌تواند خالی باشد."; return false; }
    auto v=loadDepts();
    DeptCat nc=c;
    if(trim(nc.id).empty()){
        // auto id: DEP + count+1
        wchar_t b[24]; swprintf(b,24,L"DEP%03d",(int)v.size()+1); nc.id=b;
    }
    for(auto&e:v) if(e.id==nc.id){ err=L"این شناسهٔ بخش تکراری است."; return false; }
    v.push_back(nc); saveDepts(v);
    logLine(L"dept added: "+nc.id+L" "+nc.name);
    return true;
}
bool removeDept(const std::wstring& id){
    auto v=loadDepts();
    for(size_t i=0;i<v.size();i++) if(v[i].id==id){ v.erase(v.begin()+i); saveDepts(v); return true; }
    return false;
}

// ===================================================== employee profiles ====
//  one file per user: data\emp_<username>.dat  (key=value)
static std::wstring empPath(const std::wstring& u){
    std::wstring safe=u; for(auto&c:safe) if(c==L'\\'||c==L'/'||c==L':') c=L'_';
    return dataDir()+L"\\emp_"+safe+L".dat";
}
EmpProfile loadEmpProfile(const std::wstring& username){
    EmpProfile p; p.username=username;
    std::wstring all=readFileUtf8(empPath(username));
    size_t pos=0;
    while(pos<all.size()){
        size_t e=all.find(L'\n',pos); if(e==std::wstring::npos) e=all.size();
        std::wstring line=trim(all.substr(pos,e-pos)); pos=e+1;
        size_t eq=line.find(L'='); if(eq==std::wstring::npos) continue;
        std::wstring k=trim(line.substr(0,eq)), v=trim(line.substr(eq+1));
        if(k==L"nid") p.nationalId=v; else if(k==L"father") p.fatherName=v;
        else if(k==L"address") p.address=v; else if(k==L"landline") p.landline=v;
        else if(k==L"shiftFrom") p.shiftFrom=v; else if(k==L"shiftTo") p.shiftTo=v;
        else if(k==L"photo") p.photoPath=v; else if(k==L"idcard") p.idCardPath=v;
        else if(k==L"dept") p.deptId=v;
        // v1.8.0 extended fields
        else if(k==L"empId") p.empId=v; else if(k==L"uniqueId") p.uniqueId=v;
        else if(k==L"position") p.position=v; else if(k==L"mobile") p.mobile=v;
        else if(k==L"email") p.email=v; else if(k==L"hireDate") p.hireDate=v;
        else if(k==L"workHours") p.workHours=v;
        // §H: any key we don't recognise belongs to a newer version — keep it.
        else p.extraKv += k+L"="+v+L"\r\n";
    }
    return p;
}
void saveEmpProfile(const EmpProfile& p){
    std::wstring s;
    s+=L"nid="+p.nationalId+L"\r\n";
    s+=L"father="+p.fatherName+L"\r\n";
    s+=L"address="+p.address+L"\r\n";
    s+=L"landline="+p.landline+L"\r\n";
    s+=L"shiftFrom="+p.shiftFrom+L"\r\n";
    s+=L"shiftTo="+p.shiftTo+L"\r\n";
    s+=L"photo="+p.photoPath+L"\r\n";
    s+=L"idcard="+p.idCardPath+L"\r\n";
    s+=L"dept="+p.deptId+L"\r\n";
    s+=L"empId="+p.empId+L"\r\n";
    s+=L"uniqueId="+p.uniqueId+L"\r\n";
    s+=L"position="+p.position+L"\r\n";
    s+=L"mobile="+p.mobile+L"\r\n";
    s+=L"email="+p.email+L"\r\n";
    s+=L"hireDate="+p.hireDate+L"\r\n";
    s+=L"workHours="+p.workHours+L"\r\n";
    s+=p.extraKv;                 // §H: preserve forward-compat unknown fields
    writeFileUtf8(empPath(p.username),s,false);
}

// ======================================================= online presence ====
//  §G (1.11.0): presence is now HEARTBEAT-based. data\online.dat stores one
//  record per line as «username|epochSeconds[|…]». A user counts as ONLINE only
//  if their last heartbeat is younger than ONLINE_WINDOW_SECS (90s) — so a
//  crashed / powered-off workstation drops off automatically instead of showing
//  a stale green dot forever.
//
//  Forward-compatible (§I): any extra pipe-delimited columns after the timestamp
//  are preserved verbatim on rewrite, and LEGACY rows that carry only a username
//  (no timestamp) are still honoured as online for the window so an old peer that
//  has not been upgraded is not mistakenly shown offline.
static std::wstring onlinePath(){ return dataDir()+L"\\online.dat"; }
static const long long ONLINE_WINDOW_SECS = 90;

static long long epochNow(){ return (long long)time(nullptr); }

//  Parse one record → (username, lastSeenEpoch, extraColumns). lastSeen==-1 for
//  a legacy username-only row (no timestamp present).
static void parseOnlineRow(const std::wstring& line, std::wstring& user,
                           long long& lastSeen, std::wstring& extra){
    user.clear(); lastSeen=-1; extra.clear();
    std::wstring s=trim(line);
    if(s.empty()) return;
    size_t p1=s.find(L'|');
    if(p1==std::wstring::npos){ user=trim(s); return; }   // legacy: name only
    user=trim(s.substr(0,p1));
    size_t p2=s.find(L'|',p1+1);
    std::wstring ts = (p2==std::wstring::npos) ? s.substr(p1+1)
                                               : s.substr(p1+1,p2-(p1+1));
    lastSeen=_wtoi64(trim(ts).c_str());
    if(p2!=std::wstring::npos) extra=s.substr(p2);          // includes leading '|'
}

bool isUserOnline(const std::wstring& username){
    std::wstring all=readFileUtf8(onlinePath());
    long long now=epochNow();
    size_t pos=0;
    while(pos<all.size()){
        size_t e=all.find(L'\n',pos); if(e==std::wstring::npos) e=all.size();
        std::wstring u,extra; long long seen;
        parseOnlineRow(all.substr(pos,e-pos),u,seen,extra); pos=e+1;
        if(u!=username) continue;
        if(seen<0) return true;                              // legacy → treat present
        return (now - seen) <= ONLINE_WINDOW_SECS;
    }
    return false;
}

//  Refresh / clear THIS user's heartbeat. Also prunes any record whose heartbeat
//  has expired so the file does not grow without bound.
void setUserOnline(const std::wstring& username, bool on){
    std::wstring all=readFileUtf8(onlinePath()), out;
    long long now=epochNow();
    size_t pos=0; bool wrote=false;
    while(pos<all.size()){
        size_t e=all.find(L'\n',pos); if(e==std::wstring::npos) e=all.size();
        std::wstring u,extra; long long seen;
        parseOnlineRow(all.substr(pos,e-pos),u,seen,extra); pos=e+1;
        if(u.empty()) continue;
        if(u==username){
            if(!on) continue;                                // remove on logout
            wchar_t tb[32]; swprintf(tb,32,L"%lld",now);
            out+=u+L"|"+tb+extra+L"\r\n"; wrote=true; continue;
        }
        // prune expired peers (keep legacy + fresh ones)
        if(seen>=0 && (now-seen)>ONLINE_WINDOW_SECS) continue;
        wchar_t tb[32]; swprintf(tb,32,L"%lld",seen<0?now:seen);
        out+=u+L"|"+tb+extra+L"\r\n";
    }
    if(on && !wrote){
        wchar_t tb[32]; swprintf(tb,32,L"%lld",now);
        out+=username+L"|"+tb+L"\r\n";
    }
    writeFileUtf8(onlinePath(),out,false);
}

//  §G: a workstation should call this on a short timer (e.g. every 30s) so its
//  presence stays fresh inside the 90s window. Thin wrapper over setUserOnline.
void heartbeatUser(const std::wstring& username){
    if(username.empty()) return;
    setUserOnline(username,true);
}

// ============================================================== cartable ====
//  data\messages.dat:  from|to|time|seen|type|text   (to=="*" → broadcast)
//  Legacy rows (5 fields, no type) are read as type=0 (عادی).
static std::wstring msgPath(){ return dataDir()+L"\\messages.dat"; }
std::vector<KMsg> loadMessages(const std::wstring& forUser){
    std::vector<KMsg> out;
    std::wstring all=readFileUtf8(msgPath());
    size_t pos=0;
    while(pos<all.size()){
        size_t e=all.find(L'\n',pos); if(e==std::wstring::npos) e=all.size();
        std::wstring line=all.substr(pos,e-pos); pos=e+1;
        if(trim(line).empty()) continue;
        auto f=splitPipe(line);
        if(f.size()<5) continue;
        KMsg k; k.from=f[0]; k.to=f[1]; k.time=f[2]; k.seen=(f[3]==L"1");
        if(f.size()>=6){ k.type=_wtoi(f[4].c_str()); k.text=f[5]; }
        else           { k.type=KMSG_NORMAL;        k.text=f[4]; }
        k.pinned = (f.size()>=7 && f[6]==L"1");
        if(k.to==L"*" || k.to==forUser || forUser.empty()) out.push_back(k);
    }
    return out;
}
//  v1.9.0: per-user "new message" notify flag. The recipient workstation's
//  frame timer polls this and raises the Windows notification, then clears it.
//  The SENDER never sets a flag for themselves, so management does not get its
//  own message notification back.
static std::wstring notifyFlagPath(const std::wstring& u){
    std::wstring safe=u.empty()?L"_all":u;
    for(auto&c:safe) if(c==L'\\'||c==L'/'||c==L':'||c==L'*') c=L'_';
    return dataDir()+L"\\notify_"+safe+L".flag";
}
static void setNotifyFlag(const std::wstring& forUser){
    writeFileUtf8(notifyFlagPath(forUser),L"1",false);
}
//  Called from the frame timer for the CURRENT user. If a flag is set, fire the
//  Windows notification and clear it. Managers (role>=1) never receive it.
void notifyNewMessageRecipients(){
    if(g_session.user.username.empty()) return;
    if(g_session.user.role>=1) return;            // managers don't get notified
    std::wstring p=notifyFlagPath(g_session.user.username);
    std::wstring bp=notifyFlagPath(L"*");
    bool any=false;
    if(!trim(readFileUtf8(p)).empty()){ any=true; DeleteFileW(p.c_str()); }
    // broadcast flag: consume per-user marker so it only fires once here
    if(!trim(readFileUtf8(bp)).empty()){
        std::wstring seenP=dataDir()+L"\\notify_seen_"+g_session.user.username+L".flag";
        std::wstring tag=readFileUtf8(bp);
        if(trim(readFileUtf8(seenP))!=trim(tag)){
            any=true; writeFileUtf8(seenP,tag,false);
        }
    }
    if(any) showWindowsNotification(L"آزادی طب", L"شما یک پیام جدید دارید.");
}

void pushMessageT(const std::wstring& from, const std::wstring& to,
                  const std::wstring& text, int type){
    SYSTEMTIME st=iranNow();
    wchar_t tb[32]; swprintf(tb,32,L"%s %02d:%02d",jalaliDateShort(st).c_str(),st.wHour,st.wMinute);
    wchar_t ty[8]; swprintf(ty,8,L"%d",type<0?0:(type>2?2:type));
    std::wstring row=pipeEsc(from)+L"|"+pipeEsc(to)+L"|"+std::wstring(tb)+L"|0|"+
                     std::wstring(ty)+L"|"+pipeEsc(text)+L"\r\n";
    writeFileUtf8(msgPath(),row,true);
    logLine(L"message pushed to "+to+L" type "+ty);
    // v1.9.0: notify the recipient (never the sender). For broadcast (*) we
    // stamp a unique tag so each recipient fires it exactly once.
    if(to!=from){
        if(to==L"*"){
            wchar_t tag[32]; swprintf(tag,32,L"%lu",(unsigned long)GetTickCount());
            writeFileUtf8(notifyFlagPath(L"*"),tag,false);
        } else {
            setNotifyFlag(to);
        }
    }
}
void pushMessage(const std::wstring& from, const std::wstring& to, const std::wstring& text){
    pushMessageT(from,to,text,KMSG_NORMAL);
}
int unseenMessageCount(const std::wstring& forUser){
    int n=0; for(auto&m:loadMessages(forUser)) if(!m.seen) n++; return n;
}
void markMessagesSeen(const std::wstring& forUser){
    std::wstring all=readFileUtf8(msgPath()), out;
    size_t pos=0;
    while(pos<all.size()){
        size_t e=all.find(L'\n',pos); if(e==std::wstring::npos) e=all.size();
        std::wstring line=all.substr(pos,e-pos); pos=e+1;
        if(trim(line).empty()) continue;
        auto f=splitPipe(line);
        if(f.size()<5){ out+=line+L"\r\n"; continue; }
        if((f[1]==L"*"||f[1]==forUser) && f[3]==L"0") f[3]=L"1";
        if(f.size()>=6)
            out+=f[0]+L"|"+f[1]+L"|"+f[2]+L"|"+f[3]+L"|"+f[4]+L"|"+f[5]+L"\r\n";
        else
            out+=f[0]+L"|"+f[1]+L"|"+f[2]+L"|"+f[3]+L"|"+f[4]+L"\r\n";
    }
    writeFileUtf8(msgPath(),out,false);
}

// =============================================== settings-change requests ====
//  v1.9.0 APPROVAL workflow. data\setreq.dat (newest appended last):
//    user|system|change|profile|time|seen|status|payload|title|preview
//  Older 6-field rows are read with status=0 / empty payload (pending).
static std::wstring setReqPath(){ return dataDir()+L"\\setreq.dat"; }

std::wstring systemSourceName(){
    wchar_t name[256]={0}; DWORD sz=256;
    if(GetComputerNameW(name,&sz) && name[0]) return name;
    std::wstring s=getSetting(L"system_id",L"");
    if(s.empty()){ s=L"WS-"+std::to_wstring((unsigned)GetTickCount()%100000);
        setSetting(L"system_id",s); }
    return s;
}

std::vector<SetReq> loadSetReqs(){
    std::vector<SetReq> out;
    std::wstring all=readFileUtf8(setReqPath());
    size_t pos=0;
    while(pos<all.size()){
        size_t e=all.find(L'\n',pos); if(e==std::wstring::npos) e=all.size();
        std::wstring line=all.substr(pos,e-pos); pos=e+1;
        if(trim(line).empty()) continue;
        auto f=splitPipe(line);
        if(f.size()<6) continue;
        SetReq r; r.user=f[0]; r.system=f[1]; r.change=f[2]; r.profile=f[3];
        r.time=f[4]; r.seen=(f[5]==L"1");
        r.status =(f.size()>=7)?_wtoi(f[6].c_str()):0;
        r.payload=(f.size()>=8)?f[7]:L"";
        r.title  =(f.size()>=9)?f[8]:r.change;
        r.preview=(f.size()>=10)?f[9]:L"";
        out.push_back(r);
    }
    // newest first
    std::reverse(out.begin(),out.end());
    return out;
}
static std::wstring serializeSetReq(const SetReq& r){
    wchar_t sb[8]; swprintf(sb,8,L"%d",r.status);
    return pipeEsc(r.user)+L"|"+pipeEsc(r.system)+L"|"+pipeEsc(r.change)+L"|"+
           pipeEsc(r.profile)+L"|"+pipeEsc(r.time)+L"|"+(r.seen?L"1":L"0")+L"|"+
           std::wstring(sb)+L"|"+pipeEsc(r.payload)+L"|"+pipeEsc(r.title)+L"|"+
           pipeEsc(r.preview);
}
static void saveAllSetReqs_newestFirst(const std::vector<SetReq>& v){
    std::wstring out;
    for(int i=(int)v.size()-1;i>=0;i--) out+=serializeSetReq(v[i])+L"\r\n";
    writeFileUtf8(setReqPath(),out,false);
}
void pushSetReqEx(const std::wstring& user, const std::wstring& system,
                  const std::wstring& title, const std::wstring& change,
                  const std::wstring& payload, const std::wstring& preview){
    SYSTEMTIME st=iranNow();
    wchar_t tb[40]; swprintf(tb,40,L"%s %02d:%02d",jalaliDateShort(st).c_str(),st.wHour,st.wMinute);
    SetReq r; r.user=user; r.system=system; r.change=change; r.profile=L"";
    r.time=tb; r.seen=false; r.status=0; r.payload=payload;
    r.title=title.empty()?change:title; r.preview=preview;
    writeFileUtf8(setReqPath(),serializeSetReq(r)+L"\r\n",true);
    logLine(L"settings change request (pending approval) from "+user);
}
void pushSetReq(const std::wstring& user, const std::wstring& system,
                const std::wstring& change, const std::wstring& profile){
    SYSTEMTIME st=iranNow();
    wchar_t tb[40]; swprintf(tb,40,L"%s %02d:%02d",jalaliDateShort(st).c_str(),st.wHour,st.wMinute);
    SetReq r; r.user=user; r.system=system; r.change=change; r.profile=profile;
    r.time=tb; r.seen=false; r.status=0; r.title=change;
    writeFileUtf8(setReqPath(),serializeSetReq(r)+L"\r\n",true);
    logLine(L"settings change request from "+user);
}
//  apply "key=value;key=value" pairs to settings
static void applyPayload(const std::wstring& payload){
    size_t pos=0;
    while(pos<payload.size()){
        size_t semi=payload.find(L';',pos);
        std::wstring pair=(semi==std::wstring::npos)?payload.substr(pos):payload.substr(pos,semi-pos);
        pos=(semi==std::wstring::npos)?payload.size():semi+1;
        size_t eq=pair.find(L'=');
        if(eq==std::wstring::npos) continue;
        std::wstring k=trim(pair.substr(0,eq)), v=trim(pair.substr(eq+1));
        if(!k.empty()) setSetting(k,v);
    }
}
void setSetReqStatus(int indexNewestFirst, int status, const std::wstring& reason){
    auto v=loadSetReqs();   // newest first
    if(indexNewestFirst<0||indexNewestFirst>=(int)v.size()) return;
    SetReq& r=v[indexNewestFirst];
    if(r.status!=0) { r.seen=true; saveAllSetReqs_newestFirst(v); return; }
    r.status=status; r.seen=true;
    if(status==1){
        if(!r.payload.empty()) applyPayload(r.payload);
        pushMessageT(L"مدیریت", r.user,
            L"درخواست تغییر تنظیمات شما تأیید و اعمال شد.", KMSG_NORMAL);
    } else if(status==2){
        std::wstring t=L"درخواست شما توسط مدیریت رد شد. ✕";
        if(!reason.empty()) t+=L"  دلیل: "+reason;
        pushMessageT(L"مدیریت", r.user, t, KMSG_CRITICAL);
    }
    saveAllSetReqs_newestFirst(v);
}
void markOneSetReqSeen(int indexNewestFirst){
    auto v=loadSetReqs();
    if(indexNewestFirst<0||indexNewestFirst>=(int)v.size()) return;
    v[indexNewestFirst].seen=true;
    saveAllSetReqs_newestFirst(v);
}
void deleteSetReq(int indexNewestFirst){
    auto v=loadSetReqs();
    if(indexNewestFirst<0||indexNewestFirst>=(int)v.size()) return;
    v.erase(v.begin()+indexNewestFirst);
    saveAllSetReqs_newestFirst(v);
}
int unseenSetReqCount(){
    int n=0; for(auto&r:loadSetReqs()) if(!r.seen) n++; return n;
}
int pendingSetReqCount(){
    int n=0; for(auto&r:loadSetReqs()) if(r.status==0) n++; return n;
}
void markSetReqsSeen(){
    auto v=loadSetReqs();
    for(auto& r:v) r.seen=true;
    saveAllSetReqs_newestFirst(v);
}

// ================================================= local saved notes (v1.9.0) =
//  Strictly LOCAL to this machine/user (never sent over the network).
//  data\local_notes_<user>.dat:  time|attachPath|text  (newest appended last)
static std::wstring localNotesPath(const std::wstring& u){
    std::wstring safe=u.empty()?L"_local":u;
    for(auto&c:safe) if(c==L'\\'||c==L'/'||c==L':') c=L'_';
    return dataDir()+L"\\local_notes_"+safe+L".dat";
}
std::vector<LocalNote> loadLocalNotes(const std::wstring& forUser){
    std::vector<LocalNote> out;
    std::wstring all=readFileUtf8(localNotesPath(forUser));
    size_t pos=0;
    while(pos<all.size()){
        size_t e=all.find(L'\n',pos); if(e==std::wstring::npos) e=all.size();
        std::wstring line=all.substr(pos,e-pos); pos=e+1;
        if(trim(line).empty()) continue;
        auto f=splitPipe(line);
        if(f.size()<3) continue;
        LocalNote n; n.time=f[0]; n.attachPath=f[1]; n.text=f[2];
        out.push_back(n);
    }
    std::reverse(out.begin(),out.end());   // newest first
    return out;
}
void pushLocalNote(const std::wstring& forUser, const std::wstring& text,
                   const std::wstring& attachPath){
    SYSTEMTIME st=iranNow();
    wchar_t tb[40]; swprintf(tb,40,L"%s %02d:%02d",jalaliDateShort(st).c_str(),st.wHour,st.wMinute);
    std::wstring row=std::wstring(tb)+L"|"+pipeEsc(attachPath)+L"|"+pipeEsc(text)+L"\r\n";
    writeFileUtf8(localNotesPath(forUser),row,true);
}
void deleteLocalNote(const std::wstring& forUser, int indexNewestFirst){
    auto v=loadLocalNotes(forUser);   // newest first
    if(indexNewestFirst<0||indexNewestFirst>=(int)v.size()) return;
    v.erase(v.begin()+indexNewestFirst);
    std::wstring out;
    for(int i=(int)v.size()-1;i>=0;i--)
        out+=v[i].time+L"|"+pipeEsc(v[i].attachPath)+L"|"+pipeEsc(v[i].text)+L"\r\n";
    writeFileUtf8(localNotesPath(forUser),out,false);
}
int localNoteCount(const std::wstring& forUser){ return (int)loadLocalNotes(forUser).size(); }

// ============================================== saved (archived) messages ====
//  data\saved_msgs.dat:  from|to|time|type|attachPath|text
static std::wstring savedMsgPath(){ return dataDir()+L"\\saved_msgs.dat"; }
static std::wstring attachDir(){
    std::wstring d=dataDir()+L"\\attachments";
    CreateDirectoryW(d.c_str(),NULL);
    return d;
}
bool savedMsgsEnabled(){
    return getSetting(L"saved_msgs_enabled",L"0")==L"1";
}
std::vector<SavedMsg> loadSavedMsgs(){
    std::vector<SavedMsg> out;
    std::wstring all=readFileUtf8(savedMsgPath());
    size_t pos=0;
    while(pos<all.size()){
        size_t e=all.find(L'\n',pos); if(e==std::wstring::npos) e=all.size();
        std::wstring line=all.substr(pos,e-pos); pos=e+1;
        if(trim(line).empty()) continue;
        auto f=splitPipe(line);
        if(f.size()<6) continue;
        SavedMsg m; m.from=f[0]; m.to=f[1]; m.time=f[2];
        m.type=_wtoi(f[3].c_str()); m.attachPath=f[4]; m.text=f[5];
        out.push_back(m);
    }
    std::reverse(out.begin(),out.end());   // newest first
    return out;
}
void pushSavedMsg(const std::wstring& from, const std::wstring& to,
                  const std::wstring& text, int type,
                  const std::wstring& attachPath){
    SYSTEMTIME st=iranNow();
    wchar_t tb[32]; swprintf(tb,32,L"%s %02d:%02d",jalaliDateShort(st).c_str(),st.wHour,st.wMinute);
    wchar_t ty[8]; swprintf(ty,8,L"%d",type<0?0:(type>2?2:type));
    std::wstring row=pipeEsc(from)+L"|"+pipeEsc(to)+L"|"+std::wstring(tb)+L"|"+
                     std::wstring(ty)+L"|"+pipeEsc(attachPath)+L"|"+pipeEsc(text)+L"\r\n";
    writeFileUtf8(savedMsgPath(),row,true);
    logLine(L"message archived to saved store");
}
int savedMsgCount(){ return (int)loadSavedMsgs().size(); }

//  Copy an attachment into data\attachments\ with a unique name so it survives
//  even if the original source file is later moved/deleted. Returns the stored
//  path (which the recipient can download/open) or L"" on failure.
std::wstring copyAttachmentLocal(const std::wstring& srcPath){
    if(trim(srcPath).empty()) return L"";
    std::wstring dir=attachDir();
    // derive filename
    size_t slash=srcPath.find_last_of(L"\\/");
    std::wstring base=(slash==std::wstring::npos)?srcPath:srcPath.substr(slash+1);
    SYSTEMTIME st=iranNow();
    wchar_t pre[32]; swprintf(pre,32,L"%02d%02d%02d%02d%02d_",
        st.wYear%100,st.wMonth,st.wDay,st.wHour,st.wMinute);
    std::wstring dst=dir+L"\\"+std::wstring(pre)+base;
    if(CopyFileW(srcPath.c_str(),dst.c_str(),FALSE)) return dst;
    return L"";
}

// ============================================== Windows notification (v1.9.0) =
//  A lightweight tray balloon notification. Only the message RECIPIENTS see
//  «شما یک پیام جدید دارید.» — the sending manager never gets it back.
void showWindowsNotification(const std::wstring& title, const std::wstring& body){
    if(getSetting(L"notify",L"1")!=L"1") return;     // user disabled notifications
    NOTIFYICONDATAW nid={0};
    nid.cbSize=sizeof(nid);
    nid.hWnd=g_hFrame;
    nid.uID=0xA2A2;
    nid.uFlags=NIF_INFO|NIF_ICON;
    nid.hIcon=LoadIconW(g_hInst,MAKEINTRESOURCEW(1));
    if(!nid.hIcon) nid.hIcon=LoadIconW(NULL,IDI_APPLICATION);
    nid.dwInfoFlags=NIIF_INFO;
    wcsncpy(nid.szInfoTitle,title.c_str(),63); nid.szInfoTitle[63]=0;
    wcsncpy(nid.szInfo,body.c_str(),255);       nid.szInfo[255]=0;
    // add (idempotent) then modify to fire the balloon, leave it resident briefly
    Shell_NotifyIconW(NIM_ADD,&nid);
    Shell_NotifyIconW(NIM_MODIFY,&nid);
}
