// ============================================================================
//  services.cpp — clinic Service Management data layer (v1.28.0)
//  Backing store for the «مدیریت خدمات» page in the management panel and the
//  service picker inside admission. One pipe-delimited line per service in
//  data/services.dat (UTF-8). The admission operator NEVER types a price — the
//  price always comes from this file (single source of truth).
//
//  Line format (columns):
//    code | name | category | dept | price | insType | desc | status |
//    created | modified | [future extra columns…]
// ============================================================================
#include "app.h"
#include <vector>
#include <string>
#include <mutex>

// ---- local pipe escaping helpers (mirror employees.cpp) --------------------
static std::wstring svcEsc(const std::wstring& s){
    std::wstring o=s; for(auto&c:o) if(c==L'|'||c==L'\n'||c==L'\r') c=L' '; return o;
}
static std::vector<std::wstring> svcSplit(const std::wstring& line){
    std::vector<std::wstring> f; std::wstring cur;
    for(wchar_t c:line){ if(c==L'|'){ f.push_back(cur); cur.clear(); } else cur+=c; }
    f.push_back(cur); return f;
}
static std::wstring servicesPath(){ return dataDir()+L"\\services.dat"; }

// ---- v1.42.0 performance cache ---------------------------------------------
//  loadServices() used to re-read + re-parse the WHOLE services.dat file on
//  EVERY call. The admission service picker calls it on every keystroke and the
//  billing path calls findService() per row, so with thousands of services the
//  repeated O(N) parse caused visible lag / FPS drops. We now keep an in-memory
//  cache invalidated by the file's last-write time (FILETIME) + size. When the
//  file is unchanged we hand back the cached vector in O(1); when it changes on
//  disk (another process, or our own saveServices) we re-parse exactly once.
//  A mutex guards the cache because the loopback HTTP host serves requests on a
//  worker-thread pool (concurrent service.search calls are expected).
static std::mutex               g_svcCacheMx;
static std::vector<ServiceDef>  g_svcCache;
static bool                     g_svcCacheValid = false;
static ULONGLONG                g_svcCacheStamp = 0;   // hi/lo FILETIME packed
static ULONGLONG                g_svcCacheSize  = 0;

// Cheap fingerprint of services.dat: (last-write-time, size). 0 when missing.
static void svcFileFingerprint(ULONGLONG& stampOut, ULONGLONG& sizeOut){
    stampOut=0; sizeOut=0;
    WIN32_FILE_ATTRIBUTE_DATA fad;
    if(GetFileAttributesExW(servicesPath().c_str(),GetFileExInfoStandard,&fad)){
        ULARGE_INTEGER t; t.LowPart=fad.ftLastWriteTime.dwLowDateTime;
                          t.HighPart=fad.ftLastWriteTime.dwHighDateTime;
        stampOut=t.QuadPart;
        ULARGE_INTEGER sz; sz.LowPart=fad.nFileSizeLow; sz.HighPart=fad.nFileSizeHigh;
        sizeOut=sz.QuadPart;
    }
}

// Parse services.dat from disk (no caching). Kept separate so the cache layer
// and any explicit "force reload" path can share it.
static std::vector<ServiceDef> parseServicesFromDisk(){
    std::vector<ServiceDef> out;
    std::wstring all=readFileUtf8(servicesPath());
    size_t pos=0;
    while(pos<all.size()){
        size_t e=all.find(L'\n',pos); if(e==std::wstring::npos) e=all.size();
        std::wstring line=trim(all.substr(pos,e-pos)); pos=e+1;
        if(line.empty()) continue;
        auto f=svcSplit(line);
        if(f.size()<8) continue;
        ServiceDef s;
        s.code     = trim(f[0]);
        s.name     = f[1];
        s.category = f[2];
        s.dept     = f[3];
        s.price    = _wtoi64(f[4].c_str());
        s.insType  = f[5];
        s.desc     = f[6];
        s.status   = _wtoi(f[7].c_str());
        if(f.size()>8)  s.created  = f[8];
        if(f.size()>9)  s.modified = f[9];
        // §H: keep any future extra columns verbatim (already unescaped here;
        // re-escaped on save).
        for(size_t i=10;i<f.size();i++){ s.extra+=L"|"; s.extra+=svcEsc(f[i]); }
        out.push_back(s);
    }
    return out;
}

// Invalidate the cache (called after any save so the next read re-parses).
static void invalidateServiceCache(){
    std::lock_guard<std::mutex> lk(g_svcCacheMx);
    g_svcCacheValid=false;
}

std::vector<ServiceDef> loadServices(){
    ULONGLONG stamp,size; svcFileFingerprint(stamp,size);
    {
        std::lock_guard<std::mutex> lk(g_svcCacheMx);
        if(g_svcCacheValid && g_svcCacheStamp==stamp && g_svcCacheSize==size)
            return g_svcCache;                     // O(1) cache hit
    }
    // Miss: parse OUTSIDE the lock (disk I/O), then publish under the lock.
    std::vector<ServiceDef> fresh=parseServicesFromDisk();
    {
        std::lock_guard<std::mutex> lk(g_svcCacheMx);
        g_svcCache=fresh;
        g_svcCacheStamp=stamp; g_svcCacheSize=size;
        g_svcCacheValid=true;
        return g_svcCache;
    }
}

static void saveServices(const std::vector<ServiceDef>& v){
    std::wstring out;
    for(auto&s:v){
        wchar_t pb[32]; swprintf(pb,32,L"%lld",s.price);
        wchar_t sb[8];  swprintf(sb,8,L"%d",s.status);
        out += svcEsc(s.code)+L"|"+svcEsc(s.name)+L"|"+svcEsc(s.category)+L"|"+
               svcEsc(s.dept)+L"|"+pb+L"|"+svcEsc(s.insType)+L"|"+svcEsc(s.desc)+L"|"+
               sb+L"|"+svcEsc(s.created)+L"|"+svcEsc(s.modified)+s.extra+L"\r\n";
    }
    writeFileUtf8(servicesPath(),out,false);
    invalidateServiceCache();   // v1.42.0: next read re-parses the new file
}

// v1.44.0 §2.4: snapshotServices() ALWAYS returns a full copy taken UNDER the
// cache mutex. This is the only sanctioned way for a caller to iterate the
// catalog: it can never hand out a reference to the shared g_svcCache that a
// concurrent saveServices()/invalidateServiceCache() on another thread could
// replace under the caller's feet. loadServices() (which also returns by value)
// stays for compatibility, but iteration should go through this snapshot.
std::vector<ServiceDef> snapshotServices(){
    ULONGLONG stamp,size; svcFileFingerprint(stamp,size);
    {
        std::lock_guard<std::mutex> lk(g_svcCacheMx);
        if(g_svcCacheValid && g_svcCacheStamp==stamp && g_svcCacheSize==size)
            return g_svcCache;                     // full copy under the lock
    }
    std::vector<ServiceDef> fresh=parseServicesFromDisk();
    {
        std::lock_guard<std::mutex> lk(g_svcCacheMx);
        g_svcCache=fresh;
        g_svcCacheStamp=stamp; g_svcCacheSize=size;
        g_svcCacheValid=true;
        return g_svcCache;                         // full copy under the lock
    }
}

// v1.42.0: findService now returns by value into a thread-local buffer so it is
// safe against the shared cache being replaced by another thread.
// v1.44.0 §2.4: iterate the LOCAL snapshot, never the shared g_svcCache.
const ServiceDef* findService(const std::wstring& code){
    static thread_local ServiceDef tls;
    std::wstring c=trim(code);
    std::vector<ServiceDef> v=snapshotServices();   // full copy under the lock
    for(auto&s:v) if(s.code==c){ tls=s; return &tls; }
    return nullptr;
}

bool addService(const ServiceDef& in, std::wstring& err){
    ServiceDef s=in;
    s.code=trim(s.code); s.name=trim(s.name);
    if(s.name.empty()){ err=L"نام خدمت نمی‌تواند خالی باشد."; return false; }
    auto v=loadServices();
    if(s.code.empty()){
        // auto code: SRV + (count+1)
        wchar_t b[24]; swprintf(b,24,L"SRV%04d",(int)v.size()+1); s.code=b;
        // ensure uniqueness even if a gap exists
        int n=(int)v.size()+1;
        auto exists=[&](const std::wstring& c){ for(auto&e:v) if(e.code==c) return true; return false; };
        while(exists(s.code)){ n++; swprintf(b,24,L"SRV%04d",n); s.code=b; }
    }
    for(auto&e:v) if(e.code==s.code){ err=L"این کد خدمت تکراری است."; return false; }
    std::wstring today=JalaliTodayKey();
    if(s.created.empty())  s.created=today;
    s.modified=today;
    v.push_back(s); saveServices(v);
    logLine(L"service added: "+s.code+L" "+s.name);
    return true;
}

bool updateService(const ServiceDef& in, std::wstring& err){
    ServiceDef s=in; s.code=trim(s.code); s.name=trim(s.name);
    if(s.code.empty()){ err=L"کد خدمت نامعتبر است."; return false; }
    if(s.name.empty()){ err=L"نام خدمت نمی‌تواند خالی باشد."; return false; }
    auto v=loadServices();
    for(auto&e:v) if(e.code==s.code){
        if(s.created.empty()) s.created=e.created;
        s.extra=e.extra;                 // preserve future columns
        s.modified=JalaliTodayKey();
        e=s; saveServices(v);
        logLine(L"service updated: "+s.code);
        return true;
    }
    err=L"خدمت با این کد یافت نشد."; return false;
}

bool removeService(const std::wstring& code){
    auto v=loadServices();
    for(size_t i=0;i<v.size();i++) if(v[i].code==trim(code)){
        v.erase(v.begin()+i); saveServices(v);
        logLine(L"service removed: "+code);
        return true;
    }
    return false;
}
