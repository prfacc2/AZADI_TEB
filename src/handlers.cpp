// ============================================================================
//  handlers.cpp — crash handler, speed handler (low-spec detect), font setup
// ============================================================================
#include "app.h"
#include <stdio.h>

bool g_lowSpec = false;

// ============================================================== CRASH ======
//  Unhandled exceptions → logs\crash_*.log  +  friendly Persian message.
static LONG WINAPI crashFilter(EXCEPTION_POINTERS* ep){
    SYSTEMTIME st = iranNow();
    wchar_t path[MAX_PATH];
    swprintf(path, MAX_PATH, L"%s\\crash_%04d%02d%02d_%02d%02d%02d.log",
        logsDir().c_str(), st.wYear,st.wMonth,st.wDay,st.wHour,st.wMinute,st.wSecond);

    wchar_t body[2048];
    DWORD code = ep ? ep->ExceptionRecord->ExceptionCode : 0;
    void* addr = ep ? ep->ExceptionRecord->ExceptionAddress : 0;
    CONTEXT* c = ep ? ep->ContextRecord : NULL;
#if defined(_M_X64) || defined(__x86_64__)
    swprintf(body, 2048,
        L"Azadi-Teb crash report v%s\r\nException: 0x%08X at %p\r\n"
        L"RIP=%p RSP=%p RBP=%p\r\nRAX=%p RBX=%p RCX=%p RDX=%p\r\n",
        APP_VERSION_W, code, addr,
        c?(void*)c->Rip:0, c?(void*)c->Rsp:0, c?(void*)c->Rbp:0,
        c?(void*)c->Rax:0, c?(void*)c->Rbx:0, c?(void*)c->Rcx:0, c?(void*)c->Rdx:0);
#else
    swprintf(body, 2048,
        L"Azadi-Teb crash report v%s\r\nException: 0x%08X at %p\r\n"
        L"EIP=%p ESP=%p EBP=%p\r\nEAX=%p EBX=%p ECX=%p EDX=%p\r\n",
        APP_VERSION_W, code, addr,
        c?(void*)(UINT_PTR)c->Eip:0, c?(void*)(UINT_PTR)c->Esp:0, c?(void*)(UINT_PTR)c->Ebp:0,
        c?(void*)(UINT_PTR)c->Eax:0, c?(void*)(UINT_PTR)c->Ebx:0, c?(void*)(UINT_PTR)c->Ecx:0,
        c?(void*)(UINT_PTR)c->Edx:0);
#endif
    writeFileUtf8(path, body, false);
    logLine(L"CRASH captured -> " + std::wstring(path));

    MessageBoxW(NULL,
        L"متأسفانه خطای غیرمنتظره‌ای رخ داد.\n"
        L"گزارش خطا در پوشه logs ذخیره شد.\n"
        L"لطفاً برنامه را دوباره اجرا کنید.",
        L"آزادی طب — خطای سیستم", MB_OK|MB_ICONERROR|MB_TOPMOST);
    return EXCEPTION_EXECUTE_HANDLER;
}
void installCrashHandler(){
    SetUnhandledExceptionFilter(crashFilter);
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
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
