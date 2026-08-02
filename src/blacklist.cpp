// ============================================================================
// blacklist.cpp — file-backed patient blacklist and emergency-override audit.
// Matching is deliberately ONLY by normalized national ID.
// ============================================================================
#include "app.h"
#include <mutex>
#include <algorithm>

namespace {
std::mutex g_blacklistMx;

std::wstring path(){ return dataDir()+L"\\patient_blacklist.dat"; }
std::wstring auditPath(){ return logsDir()+L"\\blacklist_override.log"; }

std::wstring esc(std::wstring s){
    for(auto& c:s){
        if(c==L'|') c=L'¦';
        else if(c==L'\r'||c==L'\n') c=L' ';
    }
    return s;
}
std::vector<std::wstring> split(const std::wstring& s, wchar_t d){
    std::vector<std::wstring> out; std::wstring cur;
    for(wchar_t c:s){ if(c==d){ out.push_back(cur); cur.clear(); } else cur+=c; }
    out.push_back(cur); return out;
}
std::wstring normNid(const std::wstring& s){
    std::wstring out;
    for(wchar_t c:trim(s)){
        if(c>=L'0'&&c<=L'9') out+=c;
        else if(c>=L'۰'&&c<=L'۹') out+=(wchar_t)(L'0'+(c-L'۰'));
        else if(c>=L'٠'&&c<=L'٩') out+=(wchar_t)(L'0'+(c-L'٠'));
    }
    return out;
}
std::wstring lower(std::wstring s){
    for(auto& c:s) c=(wchar_t)towlower(c);
    return s;
}
std::vector<BlacklistEntry> loadUnlocked(){
    std::vector<BlacklistEntry> out;
    std::wstring all=readFileUtf8(path());
    size_t pos=0;
    while(pos<all.size()){
        size_t e=all.find(L'\n',pos); if(e==std::wstring::npos) e=all.size();
        std::wstring line=all.substr(pos,e-pos); pos=e+1;
        while(!line.empty()&&(line.back()==L'\r'||line.back()==L' ')) line.pop_back();
        if(line.empty()) continue;
        auto f=split(line,L'|');
        if(f.size()<12) continue;
        BlacklistEntry r;
        r.nid=f[0]; r.first=f[1]; r.last=f[2]; r.father=f[3]; r.mobile=f[4];
        r.reason=f[5]; r.durationLabel=f[6];
        r.createdEpochMin=_wtoi64(f[7].c_str());
        r.expiresEpochMin=_wtoi64(f[8].c_str());
        r.createdText=f[9]; r.createdBy=f[10];
        // Field 11 is reserved for forward-compatible status metadata.
        out.push_back(r);
    }
    return out;
}
}

long long Blacklist_NowEpochMinutes(){
    FILETIME ft; GetSystemTimeAsFileTime(&ft);
    ULARGE_INTEGER u; u.LowPart=ft.dwLowDateTime; u.HighPart=ft.dwHighDateTime;
    return (long long)(u.QuadPart/600000000ULL);
}

std::vector<BlacklistEntry> Blacklist_Load(){
    std::lock_guard<std::mutex> lk(g_blacklistMx);
    auto out=loadUnlocked();
    std::reverse(out.begin(),out.end());
    return out;
}

bool Blacklist_Add(const BlacklistEntry& input, std::wstring& err){
    BlacklistEntry r=input;
    r.nid=normNid(r.nid); r.reason=trim(r.reason);
    if(r.nid.empty()){ err=L"کد ملی الزامی است."; return false; }
    if(r.reason.empty()){ err=L"علت مسدودی الزامی است."; return false; }
    if(r.createdEpochMin<=0) r.createdEpochMin=Blacklist_NowEpochMinutes();
    if(r.createdText.empty()){
        SYSTEMTIME st=iranNow(); r.createdText=jalaliDateShort(st)+L" "+iranTimeStr(st,true);
    }
    std::lock_guard<std::mutex> lk(g_blacklistMx);
    wchar_t created[32],expires[32];
    swprintf(created,32,L"%lld",r.createdEpochMin);
    swprintf(expires,32,L"%lld",r.expiresEpochMin);
    std::wstring line=esc(r.nid)+L"|"+esc(r.first)+L"|"+esc(r.last)+L"|"+
        esc(r.father)+L"|"+esc(r.mobile)+L"|"+esc(r.reason)+L"|"+
        esc(r.durationLabel)+L"|"+created+L"|"+expires+L"|"+
        esc(r.createdText)+L"|"+esc(r.createdBy)+L"|1\r\n";
    if(!writeFileUtf8(path(),line,true)){ err=L"ذخیره لیست سیاه ناموفق بود."; return false; }
    return true;
}

bool Blacklist_FindActive(const std::wstring& nationalId, BlacklistEntry& out){
    std::wstring nid=normNid(nationalId);
    if(nid.empty()) return false;
    std::lock_guard<std::mutex> lk(g_blacklistMx);
    auto rows=loadUnlocked();
    long long now=Blacklist_NowEpochMinutes();
    for(auto it=rows.rbegin();it!=rows.rend();++it){
        if(normNid(it->nid)!=nid) continue;
        if(it->expiresEpochMin==0 || it->expiresEpochMin>now){ out=*it; return true; }
    }
    return false;
}

std::vector<BlacklistEntry> Blacklist_Search(const std::wstring& query){
    std::wstring q=lower(trim(query)), qnid=normNid(query);
    auto rows=Blacklist_Load();
    if(q.empty()) return rows;
    std::vector<BlacklistEntry> out;
    for(const auto& r:rows){
        std::wstring full=lower(r.first+L" "+r.last+L" "+r.reason);
        if(full.find(q)!=std::wstring::npos ||
           (!qnid.empty() && normNid(r.nid).find(qnid)!=std::wstring::npos)) out.push_back(r);
    }
    return out;
}

void Blacklist_AuditOverride(const BlacklistEntry& r,const std::wstring& operatorName){
    SYSTEMTIME st=iranNow();
    std::wstring when=jalaliDateShort(st)+L" "+iranTimeStr(st,true);
    std::wstring line=esc(when)+L"|"+esc(normNid(r.nid))+L"|"+
        esc(r.first+L" "+r.last)+L"|"+esc(r.reason)+L"|"+
        esc(operatorName)+L"|EMERGENCY_SINGLE_ADMISSION\r\n";
    std::lock_guard<std::mutex> lk(g_blacklistMx);
    writeFileUtf8(auditPath(),line,true);
}
