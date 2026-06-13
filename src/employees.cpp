// ============================================================================
//  employees.cpp — department categories, extended employee profiles,
//  online-presence tracking, and the management cartable (inbox) message store.
//  All file-backed (data\*.dat) so it stays a single static EXE and is ready
//  to be swapped for a DB/REST layer later without touching the UI.
// ============================================================================
#include "app.h"
#include <stdio.h>

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
        out.push_back(c);
    }
    return out;
}
static void saveDepts(const std::vector<DeptCat>& v){
    std::wstring out;
    for(auto&c:v) out+=pipeEsc(c.id)+L"|"+pipeEsc(c.name)+L"|"+pipeEsc(c.manager)+L"|"+pipeEsc(c.icon)+L"\r\n";
    writeFileUtf8(deptsPath(),out,false);
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
    writeFileUtf8(empPath(p.username),s,false);
}

// ======================================================= online presence ====
//  data\online.dat holds one username per line for currently-active sessions.
static std::wstring onlinePath(){ return dataDir()+L"\\online.dat"; }
bool isUserOnline(const std::wstring& username){
    std::wstring all=readFileUtf8(onlinePath());
    size_t pos=0;
    while(pos<all.size()){
        size_t e=all.find(L'\n',pos); if(e==std::wstring::npos) e=all.size();
        if(trim(all.substr(pos,e-pos))==username) return true;
        pos=e+1;
    }
    return false;
}
void setUserOnline(const std::wstring& username, bool on){
    std::wstring all=readFileUtf8(onlinePath()), out;
    size_t pos=0; bool present=false;
    while(pos<all.size()){
        size_t e=all.find(L'\n',pos); if(e==std::wstring::npos) e=all.size();
        std::wstring u=trim(all.substr(pos,e-pos)); pos=e+1;
        if(u.empty()) continue;
        if(u==username){ present=true; if(!on) continue; }
        out+=u+L"\r\n";
    }
    if(on && !present) out+=username+L"\r\n";
    writeFileUtf8(onlinePath(),out,false);
}

// ============================================================== cartable ====
//  data\messages.dat:  from|to|time|seen|text     (to=="*" → broadcast)
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
        k.text=f[4];
        if(k.to==L"*" || k.to==forUser || forUser.empty()) out.push_back(k);
    }
    return out;
}
void pushMessage(const std::wstring& from, const std::wstring& to, const std::wstring& text){
    SYSTEMTIME st=iranNow();
    wchar_t tb[32]; swprintf(tb,32,L"%s %02d:%02d",jalaliDateShort(st).c_str(),st.wHour,st.wMinute);
    std::wstring row=pipeEsc(from)+L"|"+pipeEsc(to)+L"|"+std::wstring(tb)+L"|0|"+pipeEsc(text)+L"\r\n";
    writeFileUtf8(msgPath(),row,true);
    logLine(L"message pushed to "+to);
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
        out+=f[0]+L"|"+f[1]+L"|"+f[2]+L"|"+f[3]+L"|"+f[4]+L"\r\n";
    }
    writeFileUtf8(msgPath(),out,false);
}
