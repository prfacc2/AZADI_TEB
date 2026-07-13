// ============================================================================
//  client_log.cpp — structured abnormal-events-ONLY client log (v1.44.0)
//  See client_log.h for the contract. This is intentionally self-contained and
//  heap-light so it can be called from crash / SEH paths without depending on
//  anything that might itself be in a bad state.
// ============================================================================
#include "app.h"
#include "client_log.h"

#include <windows.h>
#include <string>
#include <vector>

// ---- JSON string escaper (minimal, sufficient for log messages) -----------
static std::wstring clEscape(const std::wstring& s){
    std::wstring o; o.reserve(s.size()+8);
    for(wchar_t c : s){
        switch(c){
            case L'\\': o+=L"\\\\"; break;
            case L'"':  o+=L"\\\""; break;
            case L'\n': o+=L"\\n";  break;
            case L'\r': o+=L"\\r";  break;
            case L'\t': o+=L"\\t";  break;
            default:
                if(c < 0x20){ wchar_t b[8]; swprintf(b,8,L"\\u%04x",(unsigned)c); o+=b; }
                else o+=c;
        }
    }
    return o;
}

// ---- Unix epoch MILLISECONDS from the system clock -------------------------
static long long clEpochMs(){
    FILETIME ft; GetSystemTimeAsFileTime(&ft);
    ULARGE_INTEGER u; u.LowPart=ft.dwLowDateTime; u.HighPart=ft.dwHighDateTime;
    // FILETIME is 100-ns ticks since 1601-01-01; epoch delta = 11644473600 s.
    const unsigned long long EPOCH_DIFF = 116444736000000000ULL; // in 100-ns
    unsigned long long t = (u.QuadPart - EPOCH_DIFF) / 10000ULL;  // -> ms
    return (long long)t;
}

// ---- concurrent-writer-tolerant append -------------------------------------
//  Opens with FILE_SHARE_READ | FILE_SHARE_WRITE and retries ONCE on
//  ERROR_SHARING_VIOLATION after Sleep(3). Writes raw UTF-8 bytes.
static void clAppendUtf8(const std::wstring& path, const std::wstring& text){
    int n = WideCharToMultiByte(CP_UTF8,0,text.c_str(),(int)text.size(),NULL,0,NULL,NULL);
    if(n<=0) return;
    std::vector<char> buf(n);
    WideCharToMultiByte(CP_UTF8,0,text.c_str(),(int)text.size(),buf.data(),n,NULL,NULL);

    for(int attempt=0; attempt<2; ++attempt){
        HANDLE h = CreateFileW(path.c_str(), FILE_APPEND_DATA,
            FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS,
            FILE_ATTRIBUTE_NORMAL, NULL);
        if(h!=INVALID_HANDLE_VALUE){
            SetFilePointer(h, 0, NULL, FILE_END);
            DWORD wr=0; WriteFile(h, buf.data(), (DWORD)n, &wr, NULL);
            CloseHandle(h);
            return;
        }
        if(GetLastError()==ERROR_SHARING_VIOLATION && attempt==0){ Sleep(3); continue; }
        return; // give up silently — logging must never throw / crash the app
    }
}

static void clWrite(const wchar_t* level, const std::wstring& page,
                    const std::wstring& msg, const std::wstring& extraJson){
    ClientLog_Init();
    std::wstring extra = extraJson.empty() ? L"{}" : extraJson;
    std::wstring line =
        L"{\"page\":\"" + clEscape(page) + L"\","
        L"\"level\":\"" + std::wstring(level) + L"\","
        L"\"msg\":\"" + clEscape(msg) + L"\","
        L"\"extra\":" + extra + L","
        L"\"t\":" + std::to_wstring(clEpochMs()) + L"}\r\n";
    clAppendUtf8(logsDir() + L"\\client.log", line);
}

void ClientLog_Init(){
    // logsDir() already auto-creates <exe>\logs via ensureDir().
    (void)logsDir();
}

void ClientLog_Warn (const std::wstring& page, const std::wstring& msg,
                     const std::wstring& extraJson){ clWrite(L"warn",  page, msg, extraJson); }
void ClientLog_Error(const std::wstring& page, const std::wstring& msg,
                     const std::wstring& extraJson){ clWrite(L"error", page, msg, extraJson); }
void ClientLog_Crash(const std::wstring& page, const std::wstring& msg,
                     const std::wstring& extraJson){ clWrite(L"crash", page, msg, extraJson); }
