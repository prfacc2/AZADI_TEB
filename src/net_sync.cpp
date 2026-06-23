// ============================================================================
//  net_sync.cpp — implementation of the LAN-sync layer (release 1.4.0, §9).
//  HTTP via WinHTTP; file fallback via the SMB share. No modal dialogs ever.
// ============================================================================
#include "app.h"
#include "net_sync.h"
#include <winhttp.h>
#include <objbase.h>
#include <string>
#include <vector>

// ------------------------------------------------------------------ config --
NetSyncCfg NetSync_Config(){
    NetSyncCfg c;
    c.enabled    = getSetting(L"net_sync.enabled", L"1") != L"0";
    c.admin_host = getSetting(L"net_sync.admin_host", L"");
    c.share_path = getSetting(L"net_sync.share_path", L"");
    return c;
}

// ---------------------------------------------------------- url splitting ---
static bool splitUrl(const std::wstring& url, std::wstring& host, INTERNET_PORT& port,
                     std::wstring& path, bool& https){
    URL_COMPONENTS uc; ZeroMemory(&uc, sizeof(uc));
    uc.dwStructSize = sizeof(uc);
    wchar_t hostB[256]={0}, pathB[1024]={0};
    uc.lpszHostName = hostB; uc.dwHostNameLength = 255;
    uc.lpszUrlPath  = pathB; uc.dwUrlPathLength  = 1023;
    if(!WinHttpCrackUrl(url.c_str(), 0, 0, &uc)) return false;
    host  = hostB;
    port  = uc.nPort;
    path  = pathB[0] ? pathB : L"/";
    https = (uc.nScheme == INTERNET_SCHEME_HTTPS);
    return !host.empty();
}

// Perform a WinHTTP request. method = L"GET"/L"POST"/L"HEAD".
// On success fills status (HTTP code) and body (UTF-8). Returns false on
// transport failure (no connection). timeoutMs bounds the whole exchange.
static bool httpDo(const std::wstring& fullUrl, const wchar_t* method,
                   const std::string* postBody, DWORD timeoutMs,
                   DWORD& status, std::string& body){
    status = 0; body.clear();
    std::wstring host, path; INTERNET_PORT port=80; bool https=false;
    std::wstring base = getSetting(L"net_sync.admin_host", L"");
    std::wstring url = fullUrl;
    if(!splitUrl(url, host, port, path, https)) return false;

    HINTERNET hSession = WinHttpOpen(L"AzadiTeb-NetSync/1.4",
        WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS, 0);
    if(!hSession) return false;
    WinHttpSetTimeouts(hSession, (int)timeoutMs, (int)timeoutMs,
                       (int)timeoutMs, (int)timeoutMs);
    bool ok=false;
    HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(), port, 0);
    if(hConnect){
        DWORD flags = https ? WINHTTP_FLAG_SECURE : 0;
        HINTERNET hReq = WinHttpOpenRequest(hConnect, method, path.c_str(),
            NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
        if(hReq){
            const wchar_t* hdr = L"Content-Type: application/json; charset=utf-8\r\n";
            BOOL sent;
            if(postBody){
                sent = WinHttpSendRequest(hReq, hdr, (DWORD)-1L,
                    (LPVOID)postBody->data(), (DWORD)postBody->size(),
                    (DWORD)postBody->size(), 0);
            } else {
                sent = WinHttpSendRequest(hReq, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                    WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
            }
            if(sent && WinHttpReceiveResponse(hReq, NULL)){
                DWORD code=0, len=sizeof(code);
                WinHttpQueryHeaders(hReq,
                    WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,
                    WINHTTP_HEADER_NAME_BY_INDEX, &code, &len, WINHTTP_NO_HEADER_INDEX);
                status = code;
                // read body
                DWORD avail=0;
                do {
                    avail=0;
                    if(!WinHttpQueryDataAvailable(hReq, &avail)) break;
                    if(!avail) break;
                    std::vector<char> buf(avail);
                    DWORD got=0;
                    if(!WinHttpReadData(hReq, buf.data(), avail, &got) || !got) break;
                    body.append(buf.data(), got);
                } while(avail>0);
                ok=true;
            }
            WinHttpCloseHandle(hReq);
        }
        WinHttpCloseHandle(hConnect);
    }
    WinHttpCloseHandle(hSession);
    (void)base;
    return ok;
}

// ----------------------------------------------------------- reachability ---
bool NetSync_HeadOk(const wchar_t* path){
    NetSyncCfg c = NetSync_Config();
    if(!c.enabled || c.admin_host.empty()) return false;
    std::wstring url = c.admin_host;
    if(!url.empty() && url.back()==L'/') url.pop_back();
    url += (path && *path) ? path : L"/";
    DWORD status=0; std::string body;
    // two quick attempts within ~2s
    for(int i=0;i<2;i++){
        if(httpDo(url, L"HEAD", nullptr, 1000, status, body))
            return status>=200 && status<400;
    }
    return false;
}

bool NetSync_HostReachable(){
    static DWORD lastTick=0; static bool lastOk=false;
    DWORD now=GetTickCount();
    if(now - lastTick < 5000) return lastOk;       // cache 5s
    lastTick = now;
    lastOk = NetSync_HeadOk(L"/api/ping");
    return lastOk;
}

// ----------------------------------------------------------- file fallback --
static std::wstring shareDir(const wchar_t* sub){
    NetSyncCfg c = NetSync_Config();
    if(c.share_path.empty()) return L"";
    std::wstring root = c.share_path;
    if(!root.empty() && root.back()==L'\\') root.pop_back();
    CreateDirectoryW(root.c_str(), NULL);
    std::wstring p1 = root + L"\\AzadiTeb";
    CreateDirectoryW(p1.c_str(), NULL);
    std::wstring base = p1 + L"\\" + sub;
    CreateDirectoryW(base.c_str(), NULL);
    return base;
}

static std::wstring uuidName(){
    GUID g; CoCreateGuid(&g);
    wchar_t b[64];
    swprintf(b,64,L"%08lx%04x%04x%02x%02x%02x%02x%02x%02x%02x%02x",
        g.Data1,g.Data2,g.Data3,
        g.Data4[0],g.Data4[1],g.Data4[2],g.Data4[3],
        g.Data4[4],g.Data4[5],g.Data4[6],g.Data4[7]);
    return b;
}

static bool writeRawFile(const std::wstring& path, const std::string& bytes){
    HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, NULL,
                           CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if(h==INVALID_HANDLE_VALUE) return false;
    DWORD wr=0; BOOL ok = WriteFile(h, bytes.data(), (DWORD)bytes.size(), &wr, NULL);
    CloseHandle(h);
    return ok && wr==bytes.size();
}

// ---------------------------------------------------------------- public ----
bool NetSync_PostJson(const wchar_t* path, const std::string& json){
    NetSyncCfg c = NetSync_Config();
    if(!c.enabled) return false;
    // Prefer HTTP.
    if(!c.admin_host.empty() && NetSync_HostReachable()){
        std::wstring url = c.admin_host;
        if(!url.empty() && url.back()==L'/') url.pop_back();
        url += (path && *path) ? path : L"/";
        DWORD status=0; std::string body;
        if(httpDo(url, L"POST", &json, 4000, status, body) && status>=200 && status<300)
            return true;
    }
    // File fallback: queue to outbox.
    std::wstring dir = shareDir(L"outbox");
    if(dir.empty()){
        // local outbox so we can retry later
        dir = dataDir()+L"\\profile_requests_outbox";
        CreateDirectoryW(dir.c_str(), NULL);
        dir += L"\\";
    } else {
        if(dir.back()!=L'\\') dir += L"\\";
    }
    std::wstring file = dir + uuidName() + L".json";
    return writeRawFile(file, json);
}

bool NetSync_GetJson(const wchar_t* path, std::string& out){
    out.clear();
    NetSyncCfg c = NetSync_Config();
    if(!c.enabled) return false;
    if(!c.admin_host.empty() && NetSync_HostReachable()){
        std::wstring url = c.admin_host;
        if(!url.empty() && url.back()==L'/') url.pop_back();
        url += (path && *path) ? path : L"/";
        DWORD status=0;
        if(httpDo(url, L"GET", nullptr, 4000, status, out) && status>=200 && status<300)
            return true;
    }
    // File fallback: read+consume the next inbox file.
    std::wstring dir = shareDir(L"inbox");
    if(dir.empty()) return false;
    if(dir.back()!=L'\\') dir += L"\\";
    WIN32_FIND_DATAW fd; std::wstring pat = dir + L"*.json";
    HANDLE hf = FindFirstFileW(pat.c_str(), &fd);
    if(hf==INVALID_HANDLE_VALUE) return false;
    std::wstring file = dir + fd.cFileName;
    FindClose(hf);
    HANDLE h = CreateFileW(file.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if(h==INVALID_HANDLE_VALUE) return false;
    DWORD sz = GetFileSize(h, NULL);
    if(sz!=INVALID_FILE_SIZE && sz>0){
        out.resize(sz);
        DWORD rd=0; ReadFile(h, &out[0], sz, &rd, NULL);
        out.resize(rd);
    }
    CloseHandle(h);
    return !out.empty();
}
