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

bool g_lowSpec = false;

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

    static wchar_t body[4096];
#if defined(_M_X64) || defined(__x86_64__)
    wsprintfW(body,
        L"==== Azadi-Teb crash report v%s (x64) ====\r\n"
        L"Time   : %04d-%02d-%02d %02d:%02d:%02d (local)\r\n"
        L"Code   : 0x%08X (%s)\r\n"
        L"Address: %p\r\nModule : %s\r\n"
        L"RIP=%p RSP=%p RBP=%p\r\nRAX=%p RBX=%p RCX=%p RDX=%p\r\n"
        L"CPU cores: %u   RAM: %u MB (free %u MB)\r\n",
        APP_VERSION_W, st.wYear,st.wMonth,st.wDay,st.wHour,st.wMinute,st.wSecond,
        code, excName(code), addr, exe,
        c?(void*)c->Rip:0, c?(void*)c->Rsp:0, c?(void*)c->Rbp:0,
        c?(void*)c->Rax:0, c?(void*)c->Rbx:0, c?(void*)c->Rcx:0, c?(void*)c->Rdx:0,
        si.dwNumberOfProcessors,
        (UINT)(ms.ullTotalPhys/(1024*1024)), (UINT)(ms.ullAvailPhys/(1024*1024)));
#else
    wsprintfW(body,
        L"==== Azadi-Teb crash report v%s (x86) ====\r\n"
        L"Time   : %04d-%02d-%02d %02d:%02d:%02d (local)\r\n"
        L"Code   : 0x%08X (%s)\r\n"
        L"Address: %p\r\nModule : %s\r\n"
        L"EIP=%p ESP=%p EBP=%p\r\nEAX=%p EBX=%p ECX=%p EDX=%p\r\n"
        L"CPU cores: %u   RAM: %u MB (free %u MB)\r\n",
        APP_VERSION_W, st.wYear,st.wMonth,st.wDay,st.wHour,st.wMinute,st.wSecond,
        code, excName(code), addr, exe,
        c?(void*)(UINT_PTR)c->Eip:0, c?(void*)(UINT_PTR)c->Esp:0, c?(void*)(UINT_PTR)c->Ebp:0,
        c?(void*)(UINT_PTR)c->Eax:0, c?(void*)(UINT_PTR)c->Ebx:0,
        c?(void*)(UINT_PTR)c->Ecx:0, c?(void*)(UINT_PTR)c->Edx:0,
        si.dwNumberOfProcessors,
        (UINT)(ms.ullTotalPhys/(1024*1024)), (UINT)(ms.ullAvailPhys/(1024*1024)));
#endif
    rawWrite(path, body);

    int r = MessageBoxW(NULL,
        L"متأسفانه خطای غیرمنتظره‌ای رخ داد.\n"
        L"گزارش کامل خطا در پوشه logs ذخیره شد.\n\n"
        L"برنامه دوباره اجرا شود؟",
        L"آزادی طب — خطای سیستم",
        MB_YESNO|MB_ICONERROR|MB_TOPMOST|MB_SETFOREGROUND);
    if(r == IDYES){
        STARTUPINFOW siu = { sizeof(siu) };
        PROCESS_INFORMATION pi;
        if(CreateProcessW(exe, NULL, NULL, NULL, FALSE, 0, NULL, dir, &siu, &pi)){
            CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
        }
    }
    TerminateProcess(GetCurrentProcess(), (UINT)code);
}

static LONG WINAPI crashFilter(EXCEPTION_POINTERS* ep){
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
        HANDLE h = CreateFileW(target.c_str(), GENERIC_WRITE, 0, NULL,
                               CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
        if(h != INVALID_HANDLE_VALUE){
            DWORD wr; WriteFile(h, dat, sz, &wr, NULL); CloseHandle(h);
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
