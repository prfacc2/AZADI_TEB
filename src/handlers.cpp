// ============================================================================
//  handlers.cpp — crash handler, speed handler (low-spec detect), font setup
//  v1.1.0: crash handler completely rebuilt:
//   • zero heap allocation inside the crash path (static buffers + raw WinAPI)
//   • covers SEH exceptions + std::terminate + abort/SIGSEGV/SIGFPE/SIGILL
//   • recursion guard (a crash inside the handler can't loop forever)
//   • records exception code (named), address, registers, RAM/CPU info
//   • offers one-click automatic RESTART of the application
// ============================================================================
#include "app.h"
#include <stdio.h>
#include <signal.h>
#include <exception>
#include <shlobj.h>
#include <dbghelp.h>
#include <psapi.h>

bool g_lowSpec = false;

// ============================================================== breadcrumbs ==
//  §J (1.11.0): a lock-light ring buffer of the last 32 UI/flow breadcrumbs
//  (up from the previous 16). Code calls Breadcrumb(L"...") at meaningful steps
//  ("open settings", "analyze backup start", "print designer save", …). When a
//  crash occurs the newest-first trail is appended to the crash report so we can
//  see WHAT THE USER WAS DOING in the moments before the fault. Storage is a
//  fixed static array of fixed-size wide buffers — NO heap, NO CRT streams — so
//  it is safe to read from inside the crash handler.
#define CRUMB_COUNT 32
#define CRUMB_LEN   120
static wchar_t  s_crumbs[CRUMB_COUNT][CRUMB_LEN];
static DWORD    s_crumbTick[CRUMB_COUNT];     // GetTickCount() at record time
static volatile LONG s_crumbHead = 0;         // next write slot (monotonic)

void Breadcrumb(const wchar_t* what){
    if(!what) return;
    // reserve a slot atomically; modulo into the ring on read/write.
    LONG idx = InterlockedIncrement(&s_crumbHead) - 1;
    int slot = (int)(((ULONG)idx) % CRUMB_COUNT);
    lstrcpynW(s_crumbs[slot], what, CRUMB_LEN);
    s_crumbTick[slot] = GetTickCount();
}

// ---------------------------------------------------------- crash dumps ----
//  RELEASE 1.2.0 (F.4): write a MiniDumpWriteDump alongside the existing crash
//  log. Dumps go to %LOCALAPPDATA%/AzadiTeb/crashdumps/azaditeb-<utc>.dmp and
//  the last 5 are kept (crash-only — exempt from the "no user logs" rule).
static void crashRotateDumps(const wchar_t* dir){
    // collect *.dmp, delete all but the newest 4 (so adding 1 keeps 5)
    wchar_t pat[MAX_PATH]; wsprintfW(pat,L"%s\\azaditeb-*.dmp",dir);
    WIN32_FIND_DATAW fd; HANDLE h=FindFirstFileW(pat,&fd);
    if(h==INVALID_HANDLE_VALUE) return;
    // simple: gather names + write-times, keep newest 4
    struct E { wchar_t name[MAX_PATH]; FILETIME ft; };
    static E ents[256]; int n=0;
    do {
        if(n<256){ lstrcpynW(ents[n].name,fd.cFileName,MAX_PATH);
                   ents[n].ft=fd.ftLastWriteTime; n++; }
    } while(FindNextFileW(h,&fd));
    FindClose(h);
    // bubble-sort by time ascending (oldest first) — n is tiny
    for(int i=0;i<n;i++) for(int j=i+1;j<n;j++)
        if(CompareFileTime(&ents[j].ft,&ents[i].ft)<0){ E t=ents[i]; ents[i]=ents[j]; ents[j]=t; }
    for(int i=0;i<n-4;i++){
        wchar_t full[MAX_PATH]; wsprintfW(full,L"%s\\%s",dir,ents[i].name);
        DeleteFileW(full);
    }
}
static void crashWriteMiniDump(EXCEPTION_POINTERS* ep){
    wchar_t local[MAX_PATH]={0};
    if(SHGetFolderPathW(NULL,CSIDL_LOCAL_APPDATA,NULL,0,local)!=S_OK) return;
    wchar_t dir[MAX_PATH]; wsprintfW(dir,L"%s\\AzadiTeb\\crashdumps",local);
    SHCreateDirectoryExW(NULL,dir,NULL);
    crashRotateDumps(dir);
    SYSTEMTIME ut; GetSystemTime(&ut);
    wchar_t path[MAX_PATH];
    wsprintfW(path,L"%s\\azaditeb-%04d%02d%02dT%02d%02d%02dZ.dmp",
        dir,ut.wYear,ut.wMonth,ut.wDay,ut.wHour,ut.wMinute,ut.wSecond);
    HANDLE hf=CreateFileW(path,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,
                          FILE_ATTRIBUTE_NORMAL,NULL);
    if(hf==INVALID_HANDLE_VALUE) return;
    MINIDUMP_EXCEPTION_INFORMATION mei; ZeroMemory(&mei,sizeof(mei));
    mei.ThreadId=GetCurrentThreadId(); mei.ExceptionPointers=ep; mei.ClientPointers=FALSE;
    MiniDumpWriteDump(GetCurrentProcess(),GetCurrentProcessId(),hf,
        (MINIDUMP_TYPE)(MiniDumpWithIndirectlyReferencedMemory|MiniDumpScanMemory),
        ep?&mei:NULL,NULL,NULL);
    CloseHandle(hf);
}

// ============================================================== CRASH ======
static volatile LONG s_inCrash = 0;          // recursion / double-fault guard

static const wchar_t* excName(DWORD c){
    switch(c){
    case EXCEPTION_ACCESS_VIOLATION:      return L"ACCESS_VIOLATION";
    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED: return L"ARRAY_BOUNDS_EXCEEDED";
    case EXCEPTION_DATATYPE_MISALIGNMENT: return L"DATATYPE_MISALIGNMENT";
    case EXCEPTION_FLT_DIVIDE_BY_ZERO:    return L"FLT_DIVIDE_BY_ZERO";
    case EXCEPTION_FLT_OVERFLOW:          return L"FLT_OVERFLOW";
    case EXCEPTION_ILLEGAL_INSTRUCTION:   return L"ILLEGAL_INSTRUCTION";
    case EXCEPTION_INT_DIVIDE_BY_ZERO:    return L"INT_DIVIDE_BY_ZERO";
    case EXCEPTION_INT_OVERFLOW:          return L"INT_OVERFLOW";
    case EXCEPTION_STACK_OVERFLOW:        return L"STACK_OVERFLOW";
    case EXCEPTION_PRIV_INSTRUCTION:      return L"PRIV_INSTRUCTION";
    case 0xE06D7363:                      return L"CPP_EXCEPTION";
    default:                              return L"UNKNOWN";
    }
}

//  Raw UTF-8 file write — safe inside a crash (no CRT streams, no heap).
static void rawWrite(const wchar_t* path, const wchar_t* text){
    HANDLE h = CreateFileW(path, GENERIC_WRITE, FILE_SHARE_READ, NULL,
                           CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if(h == INVALID_HANDLE_VALUE) return;
    static char u8[8192];
    int n = WideCharToMultiByte(CP_UTF8,0,text,-1,u8,sizeof(u8),NULL,NULL);
    if(n > 0){ DWORD wr; WriteFile(h, u8, n-1, &wr, NULL); }
    CloseHandle(h);
}

//  §J: resolve a faulting address to "module.dll+0xOFFSET" by walking the loaded
//  modules (EnumProcessModules / GetModuleInformation) and finding the module
//  whose [base, base+size) range contains the address. Crash-safe: only static
//  buffers, no heap. Writes "<module>+0x<offset>" (or "<unknown>") into `out`.
static void resolveModuleOffset(void* addr, wchar_t* out, int outLen){
    if(out && outLen>0) out[0]=0;
    if(!addr || !out || outLen<=0){ if(out&&outLen>0) lstrcpynW(out,L"<unknown>",outLen); return; }
    HMODULE mods[512]; DWORD needed=0;
    HANDLE proc=GetCurrentProcess();
    if(!EnumProcessModules(proc, mods, sizeof(mods), &needed)){
        lstrcpynW(out,L"<unknown>",outLen); return;
    }
    int count=(int)(needed/sizeof(HMODULE));
    if(count>512) count=512;
    UINT_PTR a=(UINT_PTR)addr;
    for(int i=0;i<count;i++){
        MODULEINFO mi; 
        if(!GetModuleInformation(proc, mods[i], &mi, sizeof(mi))) continue;
        UINT_PTR base=(UINT_PTR)mi.lpBaseOfDll;
        if(a>=base && a < base+mi.SizeOfImage){
            wchar_t name[MAX_PATH]={0};
            GetModuleFileNameW(mods[i], name, MAX_PATH);
            // keep just the file name (strip the directory)
            const wchar_t* leaf=name; 
            for(const wchar_t* p=name; *p; ++p) if(*p==L'\\'||*p==L'/') leaf=p+1;
            wsprintfW(out, L"%s+0x%X", leaf, (unsigned)(a-base));
            return;
        }
    }
    lstrcpynW(out,L"<unknown>",outLen);
}

static void crashCore(DWORD code, void* addr, CONTEXT* c){
    // recursion guard — if the handler itself faults, terminate hard
    if(InterlockedExchange(&s_inCrash, 1))
        TerminateProcess(GetCurrentProcess(), 0xDEAD);

    static wchar_t exe[MAX_PATH];  GetModuleFileNameW(NULL, exe, MAX_PATH);
    static wchar_t dir[MAX_PATH];  lstrcpynW(dir, exe, MAX_PATH);
    { wchar_t* p = wcsrchr(dir, L'\\'); if(p) *p = 0; }
    static wchar_t logs[MAX_PATH];
    wsprintfW(logs, L"%s\\logs", dir);
    CreateDirectoryW(logs, NULL);

    SYSTEMTIME st; GetLocalTime(&st);
    static wchar_t path[MAX_PATH];
    wsprintfW(path, L"%s\\crash_%04d%02d%02d_%02d%02d%02d.log",
        logs, st.wYear,st.wMonth,st.wDay,st.wHour,st.wMinute,st.wSecond);

    MEMORYSTATUSEX ms; ms.dwLength = sizeof(ms); GlobalMemoryStatusEx(&ms);
    SYSTEM_INFO si; GetSystemInfo(&si);

    // §J: resolve the faulting address to module+offset for a meaningful name.
    static wchar_t modoff[MAX_PATH+32];
    resolveModuleOffset(addr, modoff, MAX_PATH+32);

    static wchar_t body[4096];
#if defined(_M_X64) || defined(__x86_64__)
    wsprintfW(body,
        L"==== Azadi-Teb crash report v%s (x64) ====\r\n"
        L"Time   : %04d-%02d-%02d %02d:%02d:%02d (local)\r\n"
        L"Code   : 0x%08X (%s)\r\n"
        L"Address: %p\r\nModule : %s\r\nFault  : %s\r\n"
        L"RIP=%p RSP=%p RBP=%p\r\nRAX=%p RBX=%p RCX=%p RDX=%p\r\n"
        L"CPU cores: %u   RAM: %u MB (free %u MB)\r\n",
        APP_VERSION_W, st.wYear,st.wMonth,st.wDay,st.wHour,st.wMinute,st.wSecond,
        code, excName(code), addr, exe, modoff,
        c?(void*)c->Rip:0, c?(void*)c->Rsp:0, c?(void*)c->Rbp:0,
        c?(void*)c->Rax:0, c?(void*)c->Rbx:0, c?(void*)c->Rcx:0, c?(void*)c->Rdx:0,
        si.dwNumberOfProcessors,
        (UINT)(ms.ullTotalPhys/(1024*1024)), (UINT)(ms.ullAvailPhys/(1024*1024)));
#else
    wsprintfW(body,
        L"==== Azadi-Teb crash report v%s (x86) ====\r\n"
        L"Time   : %04d-%02d-%02d %02d:%02d:%02d (local)\r\n"
        L"Code   : 0x%08X (%s)\r\n"
        L"Address: %p\r\nModule : %s\r\nFault  : %s\r\n"
        L"EIP=%p ESP=%p EBP=%p\r\nEAX=%p EBX=%p ECX=%p EDX=%p\r\n"
        L"CPU cores: %u   RAM: %u MB (free %u MB)\r\n",
        APP_VERSION_W, st.wYear,st.wMonth,st.wDay,st.wHour,st.wMinute,st.wSecond,
        code, excName(code), addr, exe, modoff,
        c?(void*)(UINT_PTR)c->Eip:0, c?(void*)(UINT_PTR)c->Esp:0, c?(void*)(UINT_PTR)c->Ebp:0,
        c?(void*)(UINT_PTR)c->Eax:0, c?(void*)(UINT_PTR)c->Ebx:0,
        c?(void*)(UINT_PTR)c->Ecx:0, c?(void*)(UINT_PTR)c->Edx:0,
        si.dwNumberOfProcessors,
        (UINT)(ms.ullTotalPhys/(1024*1024)), (UINT)(ms.ullAvailPhys/(1024*1024)));
#endif
    // §J: append the last 32 breadcrumbs (newest first) so the report shows the
    // flow leading up to the fault. Built into its own static buffer, then
    // appended to the body before writing — still heap-free.
    {
        static wchar_t crumbs[4096];
        int off = wsprintfW(crumbs, L"\r\n---- breadcrumbs (newest first) ----\r\n");
        LONG head = s_crumbHead;        // snapshot
        int shown = 0;
        for(int i=1; i<=CRUMB_COUNT && shown<CRUMB_COUNT; ++i){
            LONG idx = head - i;
            if(idx < 0) break;          // fewer than CRUMB_COUNT recorded so far
            int slot = (int)(((ULONG)idx) % CRUMB_COUNT);
            if(s_crumbs[slot][0]==0) continue;
            // guard the static buffer
            if(off > (int)(sizeof(crumbs)/sizeof(wchar_t)) - CRUMB_LEN - 32) break;
            off += wsprintfW(crumbs+off, L"[%2d] +%lums  %s\r\n",
                shown+1, (unsigned long)(GetTickCount()-s_crumbTick[slot]),
                s_crumbs[slot]);
            shown++;
        }
        if(shown==0) off += wsprintfW(crumbs+off, L"(none recorded)\r\n");
        // append to body (truncate-safe) then write the combined report
        int blen = lstrlenW(body);
        int room = (int)(sizeof(body)/sizeof(wchar_t)) - blen - 1;
        if(room>0) lstrcpynW(body+blen, crumbs, room);
    }
    rawWrite(path, body);

    // §J: NO auto-restart. We inform the user (Persian) that a report was saved,
    // then exit cleanly — never relaunch the process (the work order forbids it,
    // because an immediate relaunch can crash-loop on a corrupt state).
    MessageBoxW(NULL,
        L"متأسفانه خطای غیرمنتظره‌ای رخ داد و برنامه بسته می‌شود.\n"
        L"گزارش کامل خطا در پوشهٔ logs ذخیره شد.\n\n"
        L"لطفاً برنامه را به‌صورت دستی دوباره اجرا کنید.",
        L"آزادی طب — خطای سیستم",
        MB_OK|MB_ICONERROR|MB_TOPMOST|MB_SETFOREGROUND);
    (void)dir; (void)exe;
    TerminateProcess(GetCurrentProcess(), (UINT)code);
}

static LONG WINAPI crashFilter(EXCEPTION_POINTERS* ep){
    crashWriteMiniDump(ep);             // F.4: full MiniDumpWriteDump first
    crashCore(ep ? ep->ExceptionRecord->ExceptionCode : 0,
              ep ? ep->ExceptionRecord->ExceptionAddress : 0,
              ep ? ep->ContextRecord : NULL);
    return EXCEPTION_EXECUTE_HANDLER;   // unreachable
}
static void onSignal(int sig){
    crashCore(0x80000000u | (DWORD)sig, NULL, NULL);
}
static void onTerminate(){
    crashCore(0xE06D7363, NULL, NULL);  // unhandled C++ exception
}
void installCrashHandler(){
    SetUnhandledExceptionFilter(crashFilter);
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    signal(SIGABRT, onSignal);
    signal(SIGSEGV, onSignal);
    signal(SIGILL,  onSignal);
    signal(SIGFPE,  onSignal);
    std::set_terminate(onTerminate);
}

// ============================================================== SPEED ======
//  Detect weak hardware (≤2 cores or ≤2GB RAM) → reduce timers/effects.
void detectSpec(){
    SYSTEM_INFO si; GetSystemInfo(&si);
    MEMORYSTATUSEX ms; ms.dwLength = sizeof(ms);
    GlobalMemoryStatusEx(&ms);
    DWORDLONG totalMB = ms.ullTotalPhys / (1024*1024);
    g_lowSpec = (si.dwNumberOfProcessors <= 2) || (totalMB <= 2200);
    wchar_t b[128];
    swprintf(b,128,L"Spec: %u cores, %llu MB RAM -> lowSpec=%d",
        si.dwNumberOfProcessors, totalMB, (int)g_lowSpec);
    logLine(b);
    if(g_lowSpec)   // lower own priority footprint, keep UI responsive
        SetPriorityClass(GetCurrentProcess(), NORMAL_PRIORITY_CLASS);
}

// =============================================================== FONT ======
//  Vazirmatn is embedded as RCDATA. Strategy:
//   1. AddFontMemResourceEx  → always available for this process (even if
//      the user never installs anything).
//   2. If "Vazirmatn" is not installed system-wide, silently install it to
//      %LOCALAPPDATA%\Microsoft\Windows\Fonts (per-user, no admin needed,
//      Win10+) or drop next to exe + AddFontResourceEx for older Windows.
static int CALLBACK enumProc(const LOGFONTW*, const TEXTMETRICW*, DWORD, LPARAM lp){
    *(bool*)lp = true; return 0;
}
static bool fontInstalled(const wchar_t* face){
    bool found = false;
    LOGFONTW lf = {0}; lf.lfCharSet = DEFAULT_CHARSET;
    wcsncpy(lf.lfFaceName, face, LF_FACESIZE-1);
    HDC dc = GetDC(NULL);
    EnumFontFamiliesExW(dc, &lf, enumProc, (LPARAM)&found, 0);
    ReleaseDC(NULL, dc);
    return found;
}
static void loadEmbedded(int resId, const wchar_t* fileName, bool installSystem){
    HRSRC hr = FindResourceW(g_hInst, MAKEINTRESOURCEW(resId), RT_RCDATA);
    if(!hr) return;
    HGLOBAL hg = LoadResource(g_hInst, hr);
    DWORD   sz = SizeofResource(g_hInst, hr);
    void*  dat = LockResource(hg);
    if(!dat || !sz) return;

    DWORD n = 0;
    AddFontMemResourceEx(dat, sz, NULL, &n);    // (1) in-memory, this process

    if(!installSystem) return;
    // (2) per-user install
    wchar_t lad[MAX_PATH] = {0};
    DWORD got = GetEnvironmentVariableW(L"LOCALAPPDATA", lad, MAX_PATH);
    std::wstring target;
    if(got){
        std::wstring d = std::wstring(lad) + L"\\Microsoft\\Windows\\Fonts";
        CreateDirectoryW(d.c_str(), NULL);
        target = d + L"\\" + fileName;
    } else target = exeDir() + L"\\" + fileName;

    if(GetFileAttributesW(target.c_str()) == INVALID_FILE_ATTRIBUTES){
        // §C.3: the font cache file is genuinely replaceable — use CREATE_ALWAYS
        // so a stale/partial copy from a previous run (or a TOCTOU race between
        // the GetFileAttributesW check above and this open) can never surface
        // win32_err:183 (ERROR_ALREADY_EXISTS). We do NOT silently swallow a
        // real failure: it is logged with the precise GetLastError.
        HANDLE h = CreateFileW(target.c_str(), GENERIC_WRITE, 0, NULL,
                               CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if(h != INVALID_HANDLE_VALUE){
            DWORD wr; WriteFile(h, dat, sz, &wr, NULL); CloseHandle(h);
        } else {
            DWORD e=GetLastError();
            wchar_t lb[160]; swprintf(lb,160,L"font write failed win32_err=%lu file=%s",
                                      (unsigned long)e,fileName);
            logLine(lb);
        }
    }
    AddFontResourceExW(target.c_str(), 0, 0);
    // register per-user (HKCU works without admin on Win10/11)
    HKEY k;
    if(RegCreateKeyExW(HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows NT\\CurrentVersion\\Fonts",
        0, NULL, 0, KEY_SET_VALUE, NULL, &k, NULL) == ERROR_SUCCESS){
        std::wstring valName = std::wstring(L"Vazirmatn") +
            (wcsstr(fileName, L"Bold") ? L" Bold" : L"") + L" (TrueType)";
        RegSetValueExW(k, valName.c_str(), 0, REG_SZ,
            (const BYTE*)target.c_str(), (DWORD)((target.size()+1)*2));
        RegCloseKey(k);
    }
}
void installVazirFont(){
    bool have = fontInstalled(L"Vazirmatn");
    loadEmbedded(101, L"Vazirmatn-Regular.ttf", !have);
    loadEmbedded(102, L"Vazirmatn-Bold.ttf",    !have);
    if(!have) logLine(L"Vazirmatn font installed for current user");
}

// =========================================================================
//  §B (v1.10.0) — header layout state. The frame-by-frame collapse ANIMATION
//  has been REMOVED entirely. There is no per-tick easing, no 16ms timer and
//  no intermediate factor: the action bar is shown at its FULL compact height
//  immediately on tab entry and the factor is always a discrete 0.0 / 1.0.
//
//  Why the API is kept: main.cpp / reception.cpp still call HeaderCollapse_*
//  to query the state, so preserving the contract avoids touching unrelated
//  layout code while guaranteeing the animation can never come back. Set()
//  snaps instantly and never starts a timer; Tick() is a no-op kept only so
//  any stray HEADER_COLLAPSE_TIMER message is harmless.
// =========================================================================
static bool s_hcCollapsed = false;

// Discrete factor: 1.0 expanded, 0.0 collapsed (compact). No tweening.
float HeaderCollapse_Factor(){ return s_hcCollapsed ? 0.0f : 1.0f; }
bool  HeaderCollapse_Collapsed(){ return s_hcCollapsed; }

void HeaderCollapse_Set(HWND frame, bool collapsed){
    // Instant snap — no animation, no timer. The caller re-lays-out the frame
    // right after this call, so the new compact/expanded geometry appears in a
    // single paint with no intermediate frames.
    s_hcCollapsed = collapsed;
    if(frame) KillTimer(frame, HEADER_COLLAPSE_TIMER);   // belt & braces
}

// Retained as a no-op so a stale timer (should never fire now) does nothing.
bool HeaderCollapse_Tick(HWND frame){
    if(frame) KillTimer(frame, HEADER_COLLAPSE_TIMER);
    return false;   // never "more" — there is nothing to animate
}
