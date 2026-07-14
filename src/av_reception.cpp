// ============================================================================
//  av_reception.cpp — Avalonia (.NET) Patient-Admission surface host.
//
//  Launches the embedded self-contained Avalonia reception exe and embeds it
//  (reparents its top window) inside the reception tab. The UI drives the SAME
//  loopback /api bridge the retired HTML admission page used (web_admission_*),
//  so the entire reception stays synced with the C++ data layer with ZERO
//  changes to the business logic. Graceful fallback: if the exe is missing or
//  cannot run, AvReception_CreateView returns NULL and the caller keeps the
//  existing WebView2/MSHTML HTML engine (and finally the native GDI form).
// ============================================================================
#include "app.h"
#include "av_reception.h"
#include "web_admission.h"     // WebAdmission_EnsureHost()  (loopback /api port)
#include <shlobj.h>
#include <string>
#include <map>
#include <mutex>
#include <cstdio>

extern HINSTANCE g_hInst;

namespace {

struct AvView {
    HWND   host   = nullptr;   // child host window inside the reception tab
    HWND   child  = nullptr;   // Avalonia top window, once reparented in
    HANDLE proc   = nullptr;   // Avalonia process handle
    DWORD  pid    = 0;
};
static std::map<HWND,AvView*> g_avViews;
static std::mutex             g_avMx;

static AvView* avViewFor(HWND h){
    std::lock_guard<std::mutex> lk(g_avMx);
    auto it=g_avViews.find(h); return it==g_avViews.end()?nullptr:it->second;
}

// ---- paths ---------------------------------------------------------------
static std::wstring avDir(){
    wchar_t buf[MAX_PATH]={0};
    if(SUCCEEDED(SHGetFolderPathW(NULL,CSIDL_LOCAL_APPDATA,NULL,0,buf))){
        std::wstring p=buf; p+=L"\\AzadiTeb\\reception";
        return p;
    }
    return L"";
}
static std::wstring avExePath(){
    std::wstring d=avDir(); if(d.empty()) return L"";
    return d+L"\\AzadiTeb.Reception.exe";
}
static std::wstring exeSiblingPath(){
    // A copy shipped next to AzadiTeb.exe takes priority (dev / portable).
    wchar_t path[MAX_PATH]; GetModuleFileNameW(NULL,path,MAX_PATH);
    std::wstring p=path; size_t s=p.find_last_of(L"\\/");
    if(s!=std::wstring::npos) p=p.substr(0,s+1);
    return p+L"AzadiTeb.Reception.exe";
}

static bool fileExistsNonTrivial(const std::wstring& path){
    WIN32_FILE_ATTRIBUTE_DATA fad;
    if(!GetFileAttributesExW(path.c_str(),GetFileExInfoStandard,&fad)) return false;
    if(fad.dwFileAttributes==INVALID_FILE_ATTRIBUTES) return false;
    // the RCDATA placeholder is a single byte; a real exe is many MB.
    ULONGLONG sz=((ULONGLONG)fad.nFileSizeHigh<<32)|fad.nFileSizeLow;
    return sz>65536ULL;
}

// Extract embedded RCDATA(700) → %LOCALAPPDATA%\AzadiTeb\reception\...exe once.
static bool avEnsureExtracted(std::wstring& outPath){
    // 1) sibling copy wins if present + real.
    std::wstring sib=exeSiblingPath();
    if(fileExistsNonTrivial(sib)){ outPath=sib; return true; }

    std::wstring target=avExePath();
    if(target.empty()) return false;

    HRSRC h=FindResourceW(g_hInst,MAKEINTRESOURCEW(700),RT_RCDATA);
    if(!h) return false;
    DWORD sz=SizeofResource(g_hInst,h);
    if(sz<=65536){ // placeholder → no embedded exe in this build
        // still allow a real sibling that appeared meanwhile
        if(fileExistsNonTrivial(sib)){ outPath=sib; return true; }
        return false;
    }
    HGLOBAL g=LoadResource(g_hInst,h); if(!g) return false;
    void* p=LockResource(g); if(!p) return false;

    // If an up-to-date extraction already exists (same size), reuse it.
    if(fileExistsNonTrivial(target)){
        WIN32_FILE_ATTRIBUTE_DATA fad;
        if(GetFileAttributesExW(target.c_str(),GetFileExInfoStandard,&fad)){
            ULONGLONG have=((ULONGLONG)fad.nFileSizeHigh<<32)|fad.nFileSizeLow;
            if(have==(ULONGLONG)sz){ outPath=target; return true; }
        }
    }

    // make the folder
    std::wstring dir=avDir();
    SHCreateDirectoryExW(NULL,dir.c_str(),NULL);

    // write atomically via a temp then move-replace.
    std::wstring tmp=target+L".tmp";
    HANDLE f=CreateFileW(tmp.c_str(),GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
    if(f==INVALID_HANDLE_VALUE) return false;
    DWORD written=0; BOOL ok=WriteFile(f,p,sz,&written,NULL);
    CloseHandle(f);
    if(!ok||written!=sz){ DeleteFileW(tmp.c_str()); return false; }
    MoveFileExW(tmp.c_str(),target.c_str(),MOVEFILE_REPLACE_EXISTING);

    outPath=target;
    return fileExistsNonTrivial(target);
}

// Find the Avalonia top-level window that belongs to `pid` and has been
// reparented under `host` (the child self-reparents at startup).
struct FindCtx { DWORD pid; HWND host; HWND found; };
static BOOL CALLBACK enumChildProc(HWND hwnd, LPARAM lp){
    FindCtx* c=(FindCtx*)lp;
    DWORD wpid=0; GetWindowThreadProcessId(hwnd,&wpid);
    if(wpid==c->pid){ c->found=hwnd; return FALSE; }
    return TRUE;
}
static HWND avFindChild(HWND host, DWORD pid){
    FindCtx c{pid,host,nullptr};
    EnumChildWindows(host,enumChildProc,(LPARAM)&c);
    return c.found;
}

} // namespace

// ============================================================================
//  Public API
// ============================================================================
bool AvReception_Available(){
    std::wstring dummy;
    return avEnsureExtracted(dummy);
}

HWND AvReception_CreateView(HWND parent){
    if(!parent) return nullptr;

    std::wstring exe;
    if(!avEnsureExtracted(exe)) return nullptr;

    // Bring up the same loopback /api host the HTML page used.
    int port=WebAdmission_EnsureHost();
    if(!port) return nullptr;

    // Host child window that fills the reception tab.
    static bool cls=false;
    static const wchar_t* CLS=L"AzAvReceptionHost";
    if(!cls){
        WNDCLASSW wc={}; wc.lpfnWndProc=DefWindowProcW; wc.hInstance=g_hInst;
        wc.hCursor=LoadCursor(NULL,IDC_ARROW);
        wc.hbrBackground=(HBRUSH)(COLOR_WINDOW+1);
        wc.lpszClassName=CLS;
        RegisterClassW(&wc); cls=true;
    }
    RECT rc; GetClientRect(parent,&rc);
    HWND host=CreateWindowExW(0,CLS,L"",WS_CHILD|WS_VISIBLE|WS_CLIPCHILDREN,
        0,0,rc.right,rc.bottom,parent,NULL,g_hInst,NULL);
    if(!host) return nullptr;

    // Launch the Avalonia exe:  --port <port> --parent <hostHwnd>
    wchar_t cmd[512];
    swprintf(cmd,512,L"\"%s\" --port %d --parent %lld",
             exe.c_str(), port, (long long)(INT_PTR)host);

    STARTUPINFOW si={sizeof(si)};
    PROCESS_INFORMATION pi={0};
    // Pass the port/parent via env too, as a belt-and-braces fallback.
    std::wstring env;
    {
        wchar_t buf[128];
        swprintf(buf,128,L"AZ_API_PORT=%d",port); env.append(buf); env.push_back(L'\0');
        swprintf(buf,128,L"AZ_PARENT_HWND=%lld",(long long)(INT_PTR)host); env.append(buf); env.push_back(L'\0');
        env.push_back(L'\0');
    }
    BOOL launched=CreateProcessW(exe.c_str(),cmd,NULL,NULL,FALSE,
        CREATE_UNICODE_ENVIRONMENT|CREATE_NO_WINDOW,(LPVOID)env.data(),NULL,&si,&pi);
    if(!launched){
        DestroyWindow(host);
        return nullptr;
    }
    if(pi.hThread) CloseHandle(pi.hThread);

    AvView* v=new AvView();
    v->host=host; v->proc=pi.hProcess; v->pid=pi.dwProcessId;
    { std::lock_guard<std::mutex> lk(g_avMx); g_avViews[host]=v; }

    // Pump + wait (bounded ~12s) until the Avalonia window reparents into host.
    DWORD start=GetTickCount();
    HWND child=nullptr;
    while(GetTickCount()-start<12000){
        // If the process died early, bail so the caller can fall back.
        if(WaitForSingleObject(pi.hProcess,0)==WAIT_OBJECT_0) break;
        child=avFindChild(host,pi.dwProcessId);
        if(child) break;
        MSG msg;
        while(PeekMessageW(&msg,NULL,0,0,PM_REMOVE)){ TranslateMessage(&msg); DispatchMessageW(&msg); }
        Sleep(20);
    }

    if(!child){
        // failed to embed → tear down and let the caller fall back.
        TerminateProcess(pi.hProcess,0);
        CloseHandle(pi.hProcess);
        DestroyWindow(host);
        { std::lock_guard<std::mutex> lk(g_avMx); g_avViews.erase(host); }
        delete v;
        return nullptr;
    }

    v->child=child;
    // size the child to fill the host now that it's in.
    MoveWindow(child,0,0,rc.right,rc.bottom,TRUE);
    return host;
}

void AvReception_Resize(HWND view, int w, int h){
    AvView* v=avViewFor(view); if(!v) return;
    MoveWindow(view,0,0,w,h,TRUE);
    if(v->child && IsWindow(v->child)) MoveWindow(v->child,0,0,w,h,TRUE);
}

bool AvReception_Owns(HWND view){
    std::lock_guard<std::mutex> lk(g_avMx);
    return g_avViews.find(view)!=g_avViews.end();
}

bool AvReception_DestroyView(HWND view){
    AvView* v=nullptr;
    { std::lock_guard<std::mutex> lk(g_avMx);
      auto it=g_avViews.find(view); if(it!=g_avViews.end()){ v=it->second; g_avViews.erase(it); } }
    if(!v) return false;
    if(v->proc){
        // ask nicely first (WM_CLOSE to the child), then force after a moment.
        if(v->child && IsWindow(v->child)) PostMessageW(v->child,WM_CLOSE,0,0);
        if(WaitForSingleObject(v->proc,1500)!=WAIT_OBJECT_0)
            TerminateProcess(v->proc,0);
        CloseHandle(v->proc);
    }
    if(IsWindow(view)) DestroyWindow(view);
    delete v;
    return true;
}
