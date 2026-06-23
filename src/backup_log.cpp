// ============================================================================
//  backup_log.cpp  — RELEASE 1.2.0  (Section A.3 / A.4)
//  Implementation of the dedicated "Backup Log" channel. See backup_log.h.
//
//  This is the only general-purpose logging that remains in the application
//  (besides the crash dumps). It is intentionally verbose: every line carries
//  enough forensic context (Win32 / SQLite / SEH / C++ error text, stack trace,
//  identity hash, free disk space, breadcrumbs) to reconstruct exactly what was
//  happening when a backup analyze / import / create operation failed.
// ============================================================================
#include "app.h"
#include "backup_log.h"
#include <shlobj.h>
#include <dbghelp.h>
#include <stdint.h>
#include <stdio.h>
#include <mutex>
#include <deque>
#include <string>

// gzip rotation uses a tiny self-contained "stored block" deflate + gzip frame
// (vendored under src/third_party/miniz.h is overkill for log rotation; a
// stored-block gzip stream is fully spec-compliant and decompresses with any
// gzip tool). See azGzipFile() below.

// ---------------------------------------------------------------- globals ----
static std::mutex                s_logMtx;
static std::wstring              s_logDir;
static std::wstring              s_logPath;
static bool                      s_inited = false;
static std::deque<std::wstring>  s_breadcrumbs;     // last-16 progress steps
static std::wstring              s_lastPayload;     // for the inline error card

static const long long kRotateBytes = 2LL * 1024 * 1024;   // 2 MB
static const int       kKeepFiles   = 5;

// --------------------------------------------------------------- SHA-256 -----
namespace {
struct Sha256 {
    uint32_t h[8]; uint64_t len; uint8_t buf[64]; size_t bl;
    static uint32_t ror(uint32_t x,int n){ return (x>>n)|(x<<(32-n)); }
    void init(){
        static const uint32_t iv[8]={0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,
            0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
        for(int i=0;i<8;i++){ h[i]=iv[i]; }
        len=0; bl=0;
    }
    void block(const uint8_t* p){
        static const uint32_t k[64]={
            0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
            0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
            0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
            0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
            0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
            0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
            0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
            0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
        uint32_t w[64];
        for(int i=0;i<16;i++) w[i]=(p[i*4]<<24)|(p[i*4+1]<<16)|(p[i*4+2]<<8)|p[i*4+3];
        for(int i=16;i<64;i++){
            uint32_t s0=ror(w[i-15],7)^ror(w[i-15],18)^(w[i-15]>>3);
            uint32_t s1=ror(w[i-2],17)^ror(w[i-2],19)^(w[i-2]>>10);
            w[i]=w[i-16]+s0+w[i-7]+s1;
        }
        uint32_t a=h[0],b=h[1],c=h[2],d=h[3],e=h[4],f=h[5],g=h[6],hh=h[7];
        for(int i=0;i<64;i++){
            uint32_t S1=ror(e,6)^ror(e,11)^ror(e,25);
            uint32_t ch=(e&f)^((~e)&g);
            uint32_t t1=hh+S1+ch+k[i]+w[i];
            uint32_t S0=ror(a,2)^ror(a,13)^ror(a,22);
            uint32_t mj=(a&b)^(a&c)^(b&c);
            uint32_t t2=S0+mj;
            hh=g; g=f; f=e; e=d+t1; d=c; c=b; b=a; a=t1+t2;
        }
        h[0]+=a;h[1]+=b;h[2]+=c;h[3]+=d;h[4]+=e;h[5]+=f;h[6]+=g;h[7]+=hh;
    }
    void update(const void* data, size_t n){
        const uint8_t* p=(const uint8_t*)data; len+=n;
        while(n){ size_t take=64-bl; if(take>n) take=n;
            memcpy(buf+bl,p,take); bl+=take; p+=take; n-=take;
            if(bl==64){ block(buf); bl=0; } }
    }
    void final(uint8_t out[32]){
        uint64_t bits=len*8; uint8_t pad=0x80; update(&pad,1);
        uint8_t z=0; while(bl!=56) update(&z,1);
        uint8_t lb[8]; for(int i=0;i<8;i++) lb[i]=(uint8_t)(bits>>(56-i*8));
        update(lb,8);
        for(int i=0;i<8;i++){ out[i*4]=(uint8_t)(h[i]>>24); out[i*4+1]=(uint8_t)(h[i]>>16);
            out[i*4+2]=(uint8_t)(h[i]>>8); out[i*4+3]=(uint8_t)h[i]; }
    }
};
static std::wstring hex32(const uint8_t b[32]){
    static const wchar_t* hx=L"0123456789abcdef";
    std::wstring s; s.reserve(64);
    for(int i=0;i<32;i++){ s+=hx[b[i]>>4]; s+=hx[b[i]&0xF]; }
    return s;
}
} // anon

// -------------------------------------------- sha-256 of first 1 MiB of file -
static std::wstring sha256FileHead(const std::wstring& path){
    if(path.empty()) return L"-";
    HANDLE h=CreateFileW(path.c_str(),GENERIC_READ,
        FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,
        OPEN_EXISTING,FILE_FLAG_SEQUENTIAL_SCAN,NULL);
    if(h==INVALID_HANDLE_VALUE) return L"-";
    Sha256 s; s.init();
    std::vector<uint8_t> buf(64*1024);
    long long remaining = 1024*1024;
    DWORD rd=0;
    while(remaining>0 && ReadFile(h,buf.data(),
            (DWORD)((remaining<(long long)buf.size())?remaining:(long long)buf.size()),&rd,NULL) && rd>0){
        s.update(buf.data(),rd); remaining-=rd;
    }
    CloseHandle(h);
    uint8_t dig[32]; s.final(dig);
    return hex32(dig);
}

// --------------------------------------------------------------- helpers -----
static long long fileSizeOf(const std::wstring& path){
    if(path.empty()) return -1;
    WIN32_FILE_ATTRIBUTE_DATA fa;
    if(!GetFileAttributesExW(path.c_str(),GetFileExInfoStandard,&fa)) return -1;
    LARGE_INTEGER li; li.HighPart=fa.nFileSizeHigh; li.LowPart=fa.nFileSizeLow;
    return li.QuadPart;
}

static std::wstring win32ErrText(DWORD e){
    if(e==0) return L"";
    wchar_t* msg=nullptr;
    DWORD n=FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM|
        FORMAT_MESSAGE_IGNORE_INSERTS,NULL,e,
        MAKELANGID(LANG_NEUTRAL,SUBLANG_DEFAULT),(LPWSTR)&msg,0,NULL);
    std::wstring out;
    if(n && msg){ out.assign(msg,n); while(!out.empty()&&(out.back()==L'\r'||out.back()==L'\n')) out.pop_back(); }
    if(msg) LocalFree(msg);
    return out;
}

static long long freeDiskBytes(const std::wstring& nearPath){
    std::wstring root = nearPath;
    if(root.size()>=2 && root[1]==L':') root = root.substr(0,3);  // "C:\"
    else root.clear();
    ULARGE_INTEGER freeAvail{}; freeAvail.QuadPart=0;
    if(GetDiskFreeSpaceExW(root.empty()?NULL:root.c_str(),&freeAvail,NULL,NULL))
        return (long long)freeAvail.QuadPart;
    return -1;
}

// ----------------------------------------------------- capture a stack trace -
static std::wstring captureStack(){
    std::wstring out;
    void* frames[24];
    USHORT n = CaptureStackBackTrace(1, 24, frames, NULL);
    HANDLE proc = GetCurrentProcess();
    static bool symInit=false;
    if(!symInit){ SymSetOptions(SYMOPT_DEFERRED_LOADS|SYMOPT_UNDNAME);
                  SymInitialize(proc,NULL,TRUE); symInit=true; }
    for(USHORT i=0;i<n;i++){
        DWORD64 addr=(DWORD64)(uintptr_t)frames[i];
        char symbuf[sizeof(SYMBOL_INFO)+256]={0};
        SYMBOL_INFO* sym=(SYMBOL_INFO*)symbuf;
        sym->SizeOfStruct=sizeof(SYMBOL_INFO); sym->MaxNameLen=255;
        DWORD64 disp=0;
        wchar_t line[320];
        if(SymFromAddr(proc,addr,&disp,sym)){
            wchar_t wname[260]={0};
            MultiByteToWideChar(CP_ACP,0,sym->Name,-1,wname,260);
            swprintf(line,320,L"    #%02u 0x%p %s+0x%llx",
                     (unsigned)i,(void*)(uintptr_t)addr,wname,(unsigned long long)disp);
        } else {
            swprintf(line,320,L"    #%02u 0x%p",(unsigned)i,(void*)(uintptr_t)addr);
        }
        out+=line; out+=L"\r\n";
    }
    if(out.empty()) out=L"    (no frames)\r\n";
    return out;
}

// ----------------------------------------------------- ISO-8601 Tehran time --
static std::wstring isoTehranNow(){
    SYSTEMTIME st=iranNow();
    // milliseconds: iranNow() may not carry ms; pull from a high-res counter.
    SYSTEMTIME lt; GetLocalTime(&lt);
    wchar_t b[64];
    swprintf(b,64,L"%04d-%02d-%02dT%02d:%02d:%02d.%03d+03:30",
        st.wYear,st.wMonth,st.wDay,st.wHour,st.wMinute,st.wSecond,lt.wMilliseconds);
    return b;
}

// ----------------------------------------------------------------- gzip ------
//  CRC-32 (IEEE 802.3) for the gzip trailer.
static uint32_t crc32_buf(const uint8_t* p, size_t n, uint32_t crc){
    static uint32_t tab[256]; static bool init=false;
    if(!init){
        for(uint32_t i=0;i<256;i++){ uint32_t c=i;
            for(int k=0;k<8;k++){ c=(c&1)?(0xEDB88320u^(c>>1)):(c>>1); }
            tab[i]=c; }
        init=true; }
    crc^=0xFFFFFFFFu;
    for(size_t i=0;i<n;i++) crc=tab[(crc^p[i])&0xFF]^(crc>>8);
    return crc^0xFFFFFFFFu;
}

//  Write a spec-compliant gzip file using DEFLATE *stored* (uncompressed)
//  blocks. Decompresses with gzip/7-Zip/zlib everywhere; no external lib.
static bool azGzipFile(const std::wstring& src, const std::wstring& dst){
    HANDLE hi=CreateFileW(src.c_str(),GENERIC_READ,FILE_SHARE_READ,NULL,
        OPEN_EXISTING,FILE_FLAG_SEQUENTIAL_SCAN,NULL);
    if(hi==INVALID_HANDLE_VALUE) return false;
    HANDLE ho=CreateFileW(dst.c_str(),GENERIC_WRITE,0,NULL,
        CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
    if(ho==INVALID_HANDLE_VALUE){ CloseHandle(hi); return false; }

    auto w=[&](const void* d, DWORD n){ DWORD wr=0; return WriteFile(ho,d,n,&wr,NULL)&&wr==n; };
    // gzip header: magic, deflate, flags, mtime, xfl, os
    const uint8_t hdr[10]={0x1f,0x8b,0x08,0,0,0,0,0,0,0xff};
    bool ok=w(hdr,10);

    std::vector<uint8_t> buf(64*1024);
    uint32_t crc=0; uint32_t isize=0; DWORD rd=0;
    // We must know if a block is the final one; read all, emit stored blocks.
    // Stored block: 1 byte (BFINAL+BTYPE=00), LEN(2 LE), NLEN(2 LE), data.
    // Max LEN per stored block is 65535.
    std::vector<uint8_t> all;
    while(ok && ReadFile(hi,buf.data(),(DWORD)buf.size(),&rd,NULL) && rd>0){
        all.insert(all.end(),buf.begin(),buf.begin()+rd);
        crc=crc32_buf(buf.data(),rd,crc); isize+=rd;
    }
    size_t off=0;
    if(all.empty()){
        uint8_t b0=0x01; uint16_t len=0,nlen=0xFFFF;
        ok=ok&&w(&b0,1)&&w(&len,2)&&w(&nlen,2);
    } else {
        while(ok && off<all.size()){
            size_t chunk=all.size()-off; if(chunk>65535) chunk=65535;
            bool last=(off+chunk>=all.size());
            uint8_t b0=last?0x01:0x00;
            uint16_t len=(uint16_t)chunk, nlen=(uint16_t)(~len);
            ok=ok&&w(&b0,1)&&w(&len,2)&&w(&nlen,2)&&w(all.data()+off,(DWORD)chunk);
            off+=chunk;
        }
    }
    // trailer: CRC32 (LE), ISIZE (LE)
    ok=ok&&w(&crc,4)&&w(&isize,4);
    CloseHandle(hi); CloseHandle(ho);
    return ok;
}

// ---------------------------------------------------------- rotation logic ---
static void rotateIfNeeded_locked(){
    if(fileSizeOf(s_logPath) < kRotateBytes) return;
    // shift backup.4.gz -> backup.5.gz ... drop the oldest
    for(int i=kKeepFiles; i>=1; i--){
        wchar_t from[MAX_PATH], to[MAX_PATH];
        swprintf(from,MAX_PATH,L"%s\\backup.%d.gz",s_logDir.c_str(),i-1);
        swprintf(to,  MAX_PATH,L"%s\\backup.%d.gz",s_logDir.c_str(),i);
        if(i==kKeepFiles){ DeleteFileW(to); }
        if(i-1==0){
            // gzip the current backup.log -> backup.1.gz
            if(azGzipFile(s_logPath, to)) DeleteFileW(s_logPath.c_str());
        } else {
            MoveFileExW(from,to,MOVEFILE_REPLACE_EXISTING);
        }
    }
}

// ------------------------------------------------------------ append a line --
static void appendUtf8_locked(const std::wstring& text){
    HANDLE h=CreateFileW(s_logPath.c_str(),FILE_APPEND_DATA,
        FILE_SHARE_READ,NULL,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
    if(h==INVALID_HANDLE_VALUE) return;
    SetFilePointer(h,0,NULL,FILE_END);
    int n=WideCharToMultiByte(CP_UTF8,0,text.c_str(),(int)text.size(),NULL,0,NULL,NULL);
    if(n>0){ std::vector<char> u8(n); 
        WideCharToMultiByte(CP_UTF8,0,text.c_str(),(int)text.size(),u8.data(),n,NULL,NULL);
        DWORD wr=0; WriteFile(h,u8.data(),(DWORD)n,&wr,NULL); }
    CloseHandle(h);
}

// =============================================================== public API ==
void BackupLog_Init(){
    std::lock_guard<std::mutex> lk(s_logMtx);
    if(s_inited) return;
    wchar_t local[MAX_PATH]={0};
    if(SHGetFolderPathW(NULL,CSIDL_LOCAL_APPDATA,NULL,0,local)!=S_OK){
        // fall back to %TEMP%
        GetTempPathW(MAX_PATH,local);
    }
    s_logDir = std::wstring(local)+L"\\AzadiTeb\\backup_logs";
    SHCreateDirectoryExW(NULL,s_logDir.c_str(),NULL);
    s_logPath = s_logDir+L"\\backup.log";
    s_inited=true;
}

void BackupLog_Shutdown(){
    std::lock_guard<std::mutex> lk(s_logMtx);
    s_inited=false;
}

void BackupLog_Breadcrumb(const wchar_t* step){
    if(!step) return;
    std::lock_guard<std::mutex> lk(s_logMtx);
    s_breadcrumbs.push_back(isoTehranNow()+L"  "+step);
    while(s_breadcrumbs.size()>16) s_breadcrumbs.pop_front();
}

void BackupLog_Event(const wchar_t* phase, const wchar_t* file, const wchar_t* detail){
    DWORD gle=GetLastError();   // capture immediately
    if(!s_inited) BackupLog_Init();
    std::lock_guard<std::mutex> lk(s_logMtx);
    if(!s_inited) return;

    const std::wstring ph = phase?phase:L"-";
    const std::wstring fl = file?file:L"";
    const std::wstring dt = detail?detail:L"";
    const bool isFail = (ph.find(L"FAIL")!=std::wstring::npos);

    long long fsz = fileSizeOf(fl);
    std::wstring identity = sha256FileHead(fl);
    long long freeb = freeDiskBytes(fl.empty()?s_logDir:fl);
    std::wstring w32 = win32ErrText(gle);

    wchar_t hdr[256];
    swprintf(hdr,256,
        L"[%s] pid=%lu tid=%lu phase=%s\r\n",
        isoTehranNow().c_str(),
        (unsigned long)GetCurrentProcessId(),
        (unsigned long)GetCurrentThreadId(),
        ph.c_str());

    std::wstring line = hdr;
    line += L"  file      : "+(fl.empty()?std::wstring(L"-"):fl)+L"\r\n";
    {   wchar_t b[64]; swprintf(b,64,L"%lld",fsz); line+=L"  file_size : "+std::wstring(b)+L" bytes\r\n"; }
    line += L"  identity  : sha256(file[:1MiB])="+identity+L"\r\n";
    {   wchar_t b[64]; swprintf(b,64,L"%lld",freeb); line+=L"  free_disk : "+std::wstring(b)+L" bytes\r\n"; }
    {   wchar_t b[96]; swprintf(b,96,L"%lu",(unsigned long)gle);
        line+=L"  win32_err : "+std::wstring(b)+(w32.empty()?L"":(L" ("+w32+L")"))+L"\r\n"; }

    if(isFail){
        // dump the breadcrumb ring buffer so we can see what was happening.
        line += L"  breadcrumbs (last 16):\r\n";
        if(s_breadcrumbs.empty()) line+=L"    (none)\r\n";
        for(const auto& b : s_breadcrumbs) line += L"    "+b+L"\r\n";
        line += L"  stack:\r\n"+captureStack();
    }
    if(!dt.empty()) line += L"  detail    : "+dt+L"\r\n";
    line += L"----\r\n";

    rotateIfNeeded_locked();
    appendUtf8_locked(line);
    s_lastPayload = line;
}

std::wstring BackupLog_LastPayload(){
    std::lock_guard<std::mutex> lk(s_logMtx);
    return s_lastPayload;
}
