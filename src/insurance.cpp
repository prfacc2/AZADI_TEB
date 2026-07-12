// ============================================================================
//  insurance.cpp — EDITABLE clinic insurance store (v1.43.0)
//  Backing store for the «مدیریت بیمه‌ها» management page. One pipe-delimited
//  line per insurer in data\insurances.dat (UTF-8). The admission billing reads
//  the coverage percentages from HERE (single source of truth) so the clinic can
//  decide exactly how much share each base / supplementary insurer covers.
//
//  Line format (columns):
//    name | pct | supp(0=base,1=supp) | status(1/0) | [future extra columns…]
//
//  On first run (no file yet) the store is SEEDED from the built-in
//  INSURANCES[] / SUPP_INSURANCES[] tables in billing.cpp, so behaviour is
//  identical to previous versions until the operator edits it.
// ============================================================================
#include "app.h"
#include <vector>
#include <string>
#include <mutex>

// ---- local pipe escaping helpers (mirror services.cpp) --------------------
static std::wstring insEsc(const std::wstring& s){
    std::wstring o=s; for(auto&c:o) if(c==L'|'||c==L'\n'||c==L'\r') c=L' '; return o;
}
static std::vector<std::wstring> insSplit(const std::wstring& line){
    std::vector<std::wstring> f; std::wstring cur;
    for(wchar_t c:line){ if(c==L'|'){ f.push_back(cur); cur.clear(); } else cur+=c; }
    f.push_back(cur); return f;
}
static std::wstring insurancesPath(){ return dataDir()+L"\\insurances.dat"; }

// ---- in-memory cache (fingerprint = last-write-time + size) ----------------
static std::mutex                g_insMx;
static std::vector<InsuranceRow> g_insCache;
static bool                      g_insValid = false;
static ULONGLONG                 g_insStamp = 0;
static ULONGLONG                 g_insSize  = 0;

static void insFingerprint(ULONGLONG& stampOut, ULONGLONG& sizeOut){
    stampOut=0; sizeOut=0;
    WIN32_FILE_ATTRIBUTE_DATA fad;
    if(GetFileAttributesExW(insurancesPath().c_str(),GetFileExInfoStandard,&fad)){
        ULARGE_INTEGER t; t.LowPart=fad.ftLastWriteTime.dwLowDateTime;
                          t.HighPart=fad.ftLastWriteTime.dwHighDateTime;
        stampOut=t.QuadPart;
        ULARGE_INTEGER sz; sz.LowPart=fad.nFileSizeLow; sz.HighPart=fad.nFileSizeHigh;
        sizeOut=sz.QuadPart;
    }
}

static int clampPct(int p){ if(p<0) return 0; if(p>100) return 100; return p; }

// Build the built-in seed (base + supplementary) from billing.cpp tables.
static std::vector<InsuranceRow> seedRows(){
    std::vector<InsuranceRow> v;
    for(int i=0;i<N_INSURANCES;i++){
        InsuranceRow r; r.name=INSURANCES[i].name; r.pct=clampPct(INSURANCES[i].pct);
        r.supp=0; r.status=1; v.push_back(r);
    }
    for(int i=0;i<N_SUPP;i++){
        InsuranceRow r; r.name=SUPP_INSURANCES[i].name; r.pct=clampPct(SUPP_INSURANCES[i].pct);
        r.supp=1; r.status=1; v.push_back(r);
    }
    return v;
}

static std::vector<InsuranceRow> parseFromDisk(){
    std::vector<InsuranceRow> out;
    std::wstring txt=readFileUtf8(insurancesPath());
    if(txt.empty()) return out;   // caller seeds when empty
    std::wstring line;
    auto flush=[&](){
        if(line.empty()){ return; }
        auto f=insSplit(line);
        if(f.size()<1 || trim(f[0]).empty()){ line.clear(); return; }
        InsuranceRow r;
        r.name = trim(f[0]);
        r.pct  = (f.size()>1)? clampPct(_wtoi(trim(f[1]).c_str())) : 0;
        r.supp = (f.size()>2)? (_wtoi(trim(f[2]).c_str())?1:0) : 0;
        r.status=(f.size()>3)? (_wtoi(trim(f[3]).c_str())?1:0) : 1;
        // preserve any extra future columns verbatim (forward-compat).
        if(f.size()>4){
            std::wstring ex;
            for(size_t i=4;i<f.size();++i){ ex+=L"|"; ex+=f[i]; }
            r.extra=ex;
        }
        out.push_back(r);
        line.clear();
    };
    for(wchar_t c:txt){
        if(c==L'\n'){ flush(); }
        else if(c==L'\r'){ /* skip */ }
        else line+=c;
    }
    flush();
    return out;
}

std::vector<InsuranceRow> loadInsuranceRows(){
    ULONGLONG stamp,size; insFingerprint(stamp,size);
    {
        std::lock_guard<std::mutex> lk(g_insMx);
        if(g_insValid && g_insStamp==stamp && g_insSize==size)
            return g_insCache;   // O(1) cache hit
    }
    std::vector<InsuranceRow> fresh=parseFromDisk();
    if(fresh.empty()){
        // First run: seed from the built-in tables and persist so the file
        // exists for the management page to edit.
        fresh=seedRows();
        saveInsuranceRows(fresh);   // writes + invalidates; re-fingerprint below
        insFingerprint(stamp,size);
    }
    {
        std::lock_guard<std::mutex> lk(g_insMx);
        g_insCache=fresh; g_insStamp=stamp; g_insSize=size; g_insValid=true;
        return g_insCache;
    }
}

void insuranceStoreInvalidate(){
    std::lock_guard<std::mutex> lk(g_insMx);
    g_insValid=false;
}

void saveInsuranceRows(const std::vector<InsuranceRow>& v){
    std::wstring out;
    for(auto& r:v){
        wchar_t pb[8]; swprintf(pb,8,L"%d",clampPct(r.pct));
        wchar_t sb[4]; swprintf(sb,4,L"%d",r.supp?1:0);
        wchar_t st[4]; swprintf(st,4,L"%d",r.status?1:0);
        out += insEsc(r.name)+L"|"+pb+L"|"+sb+L"|"+st+r.extra+L"\r\n";
    }
    writeFileUtf8(insurancesPath(),out,false);
    insuranceStoreInvalidate();
}

// ---- derived lists (guarantee a stable index-0 "none" option) --------------
std::vector<InsuranceRow> insBaseList(){
    auto all=loadInsuranceRows();
    std::vector<InsuranceRow> v;
    for(auto& r:all) if(r.supp==0 && r.status!=0) v.push_back(r);
    if(v.empty()){ InsuranceRow z; z.name=L"آزاد (بدون بیمه)"; z.pct=0; z.supp=0; z.status=1; v.push_back(z); }
    return v;
}
std::vector<InsuranceRow> insSuppList(){
    auto all=loadInsuranceRows();
    std::vector<InsuranceRow> v;
    for(auto& r:all) if(r.supp==1 && r.status!=0) v.push_back(r);
    if(v.empty()){ InsuranceRow z; z.name=L"ندارد"; z.pct=0; z.supp=1; z.status=1; v.push_back(z); }
    return v;
}
int insBasePctAt(int idx){
    auto v=insBaseList();
    if(idx>=0 && idx<(int)v.size()) return clampPct(v[idx].pct);
    return 0;
}
int insSuppPctAt(int idx){
    auto v=insSuppList();
    if(idx>=0 && idx<(int)v.size()) return clampPct(v[idx].pct);
    return 0;
}
std::wstring insBaseNameAt(int idx){
    auto v=insBaseList();
    if(idx>=0 && idx<(int)v.size()) return v[idx].name;
    return L"";
}
std::wstring insSuppNameAt(int idx){
    auto v=insSuppList();
    if(idx>=0 && idx<(int)v.size()) return v[idx].name;
    return L"";
}
