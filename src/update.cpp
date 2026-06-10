// ============================================================================
//  update.cpp — remote update over HTTPS (WinINet, works on Win7+)
//  Checks   data\update_url  (default: GitHub raw)  for  version.txt
//  Format:  line1 = latest version,  line2 = download URL of new exe
//  If newer → downloads to AzadiTeb_new.exe + writes update.bat that swaps
//  the exe on next run. Fully offline-safe (silent fail when no internet).
// ============================================================================
#include "app.h"
#include <wininet.h>
#include <stdio.h>

static const wchar_t* DEFAULT_UPDATE_URL =
    L"https://raw.githubusercontent.com/perofesor/Azadi-Teb/main/update/version.txt";

static std::string httpGet(const std::wstring& url, bool* okOut){
    *okOut = false;
    std::string body;
    HINTERNET net = InternetOpenW(L"AzadiTeb/1.0", INTERNET_OPEN_TYPE_PRECONFIG,
                                  NULL, NULL, 0);
    if(!net) return body;
    HINTERNET f = InternetOpenUrlW(net, url.c_str(), NULL, 0,
        INTERNET_FLAG_RELOAD | INTERNET_FLAG_SECURE | INTERNET_FLAG_NO_CACHE_WRITE, 0);
    if(f){
        char buf[4096]; DWORD rd;
        while(InternetReadFile(f, buf, sizeof(buf), &rd) && rd)
            body.append(buf, rd);
        InternetCloseHandle(f);
        *okOut = true;
    }
    InternetCloseHandle(net);
    return body;
}

static bool downloadTo(const std::wstring& url, const std::wstring& path){
    bool ok=false;
    std::string body = httpGet(url, &ok);
    if(!ok || body.empty()) return false;
    HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, NULL,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if(h==INVALID_HANDLE_VALUE) return false;
    DWORD wr; WriteFile(h, body.data(), (DWORD)body.size(), &wr, NULL);
    CloseHandle(h);
    return wr == body.size();
}

static int cmpVersion(const std::wstring& a, const std::wstring& b){
    int a1=0,a2=0,a3=0,b1=0,b2=0,b3=0;
    swscanf(a.c_str(), L"%d.%d.%d",&a1,&a2,&a3);
    swscanf(b.c_str(), L"%d.%d.%d",&b1,&b2,&b3);
    if(a1!=b1) return a1-b1;
    if(a2!=b2) return a2-b2;
    return a3-b3;
}

void checkRemoteUpdate(HWND owner){
    std::wstring url = getSetting(L"update_url", DEFAULT_UPDATE_URL);
    logLine(L"update check: " + url);

    bool ok=false;
    std::string resp = httpGet(url, &ok);
    if(!ok || resp.empty()){
        MessageBoxW(owner,
            L"امکان بررسی به‌روزرسانی وجود ندارد.\n"
            L"اتصال اینترنت را بررسی کنید یا آدرس سرور به‌روزرسانی را\n"
            L"در data\\settings.ini (کلید update_url) تنظیم کنید.",
            L"به‌روزرسانی از راه دور", MB_OK|MB_ICONINFORMATION);
        return;
    }
    // parse: line1 version, line2 exe url
    int n = MultiByteToWideChar(CP_UTF8,0,resp.c_str(),(int)resp.size(),NULL,0);
    std::wstring w(n,0);
    MultiByteToWideChar(CP_UTF8,0,resp.c_str(),(int)resp.size(),&w[0],n);
    size_t nl = w.find(L'\n');
    std::wstring ver = trim(nl==std::wstring::npos ? w : w.substr(0,nl));
    std::wstring dlUrl = nl==std::wstring::npos ? L"" : trim(w.substr(nl+1));
    size_t nl2 = dlUrl.find(L'\n');
    if(nl2!=std::wstring::npos) dlUrl = trim(dlUrl.substr(0,nl2));

    if(ver.empty() || cmpVersion(ver, APP_VERSION_W) <= 0){
        MessageBoxW(owner,
            (L"شما از آخرین نسخه استفاده می‌کنید.\nنسخه فعلی: " +
             toFaDigits(APP_VERSION_W)).c_str(),
            L"به‌روزرسانی از راه دور", MB_OK|MB_ICONINFORMATION);
        return;
    }
    std::wstring msg = L"نسخه جدید " + toFaDigits(ver) + L" در دسترس است.\n"
        L"نسخه فعلی: " + toFaDigits(APP_VERSION_W) + L"\n\nدانلود و نصب شود؟";
    if(MessageBoxW(owner, msg.c_str(), L"به‌روزرسانی از راه دور",
        MB_YESNO|MB_ICONQUESTION) != IDYES) return;

    if(dlUrl.empty()){
        MessageBoxW(owner, L"آدرس دانلود در سرور تعریف نشده است.",
            L"به‌روزرسانی", MB_OK|MB_ICONWARNING);
        return;
    }
    std::wstring newExe = exeDir() + L"\\AzadiTeb_new.exe";
    SetCursor(LoadCursor(NULL, IDC_WAIT));
    bool dl = downloadTo(dlUrl, newExe);
    SetCursor(LoadCursor(NULL, IDC_ARROW));
    if(!dl){
        MessageBoxW(owner, L"دانلود نسخه جدید ناموفق بود.",
            L"به‌روزرسانی", MB_OK|MB_ICONERROR);
        return;
    }
    // swap script — runs after we exit
    wchar_t self[MAX_PATH]; GetModuleFileNameW(NULL,self,MAX_PATH);
    std::wstring bat = exeDir() + L"\\update.bat";
    std::wstring script =
        L"@echo off\r\nchcp 65001>nul\r\n:wait\r\n"
        L"timeout /t 1 /nobreak >nul\r\n"
        L"del \"" + std::wstring(self) + L"\" 2>nul\r\n"
        L"if exist \"" + std::wstring(self) + L"\" goto wait\r\n"
        L"move /y \"" + newExe + L"\" \"" + std::wstring(self) + L"\" >nul\r\n"
        L"start \"\" \"" + std::wstring(self) + L"\"\r\n"
        L"del \"%~f0\"\r\n";
    writeFileUtf8(bat, script, false);
    logLine(L"update downloaded, restarting via update.bat");
    ShellExecuteW(NULL, L"open", bat.c_str(), NULL, exeDir().c_str(), SW_HIDE);
    PostMessageW(g_hFrame, WM_CLOSE, 0, 0);
}
