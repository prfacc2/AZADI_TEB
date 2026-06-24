// ============================================================================
//  webhost.cpp — hybrid HTML/CSS/JS presentation host (release 1.13.0, §3/§4)
//
//  Hosts the SYSTEM MSHTML / WebBrowser (Trident) OLE control inside a child
//  window. The reception + appointment screens render their VISIBLE interface
//  as HTML/CSS/JS; C++ remains host / validator / persistence / bridge.
//
//  Why MSHTML and not WebView2/CEF: the product is a SINGLE static 32-bit PE32
//  EXE, no shipped DLLs, runs offline on Windows 7 → 11 (x86 & x64). MSHTML is
//  the only HTML engine guaranteed present on every one of those targets with
//  zero runtime install and zero binary bloat. The embedded HTML/CSS/JS lives
//  in RCDATA and is written into an in-process document, so nothing touches the
//  disk or the network.
//
//  This file is intentionally dependency-free at link time beyond ole32/oleaut/
//  user32/gdi32 which are already linked. We implement the minimal OLE site
//  interfaces by hand (no ATL) so it cross-compiles cleanly under MinGW.
// ============================================================================
#include "app.h"
#include "webhost.h"
#include "sections.h"
#include "ui_kit.h"
#include <exdisp.h>      // IWebBrowser2
#include <mshtml.h>      // IHTMLDocument2, IHTMLWindow2
#include <mshtmhst.h>    // IDocHostUIHandler, DOCHOSTUIINFO
#include <ocidl.h>
#include <oleidl.h>
#include <exdispid.h>
#include <objbase.h>
#include <oleauto.h>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
//  CLSID / IID literals (defined here so we don't depend on importing the libs)
// ---------------------------------------------------------------------------
static const CLSID CLSID_WebBrowser_local =
    {0x8856F961,0x340A,0x11D0,{0xA9,0x6B,0x00,0xC0,0x4F,0xD7,0x05,0xA2}};
static const IID IID_IWebBrowser2_local =
    {0xD30C1661,0xCDAF,0x11D0,{0x8A,0x3E,0x00,0xC0,0x4F,0xC9,0xE2,0x6E}};
static const IID IID_IOleObject_local =
    {0x00000112,0,0,{0xC0,0,0,0,0,0,0,0x46}};
static const IID IID_IConnectionPointContainer_local =
    {0xB196B284,0xBAB4,0x101A,{0xB6,0x9C,0x00,0xAA,0x00,0x34,0x1D,0x07}};
static const IID DIID_DWebBrowserEvents2_local =
    {0x34A715A0,0x6587,0x11D0,{0x92,0x4A,0x00,0x20,0xAF,0xC7,0xAC,0x4D}};
// ICustomDoc — {3050F221-98B5-11CF-BB82-00AA00BDCE0B}. MinGW's import libs do
// not always export IID_ICustomDoc, so we define it locally to avoid a link
// failure across toolchain versions.
static const IID IID_ICustomDoc_local =
    {0x3050F221,0x98B5,0x11CF,{0xBB,0x82,0x00,0xAA,0x00,0xBD,0xCE,0x0B}};

// ---------------------------------------------------------------------------
//  persistent, throttled error log (logs\webhost_errors.log)
// ---------------------------------------------------------------------------
void WebHost_LogError(const wchar_t* where, const wchar_t* detail){
    // Coalesce identical messages fired within a short window so a misbehaving
    // page can never flood the disk or the UI. Storage is a tiny static cache.
    static std::wstring lastKey;
    static DWORD lastTick = 0;
    std::wstring key = std::wstring(where?where:L"?") + L"|" + (detail?detail:L"");
    DWORD now = GetTickCount();
    if(key==lastKey && (now-lastTick) < 1500) return;   // throttle duplicates
    lastKey = key; lastTick = now;

    SYSTEMTIME st = iranNow();
    wchar_t hdr[80];
    swprintf(hdr,80,L"[%04d-%02d-%02d %02d:%02d:%02d] ",
             st.wYear,st.wMonth,st.wDay,st.wHour,st.wMinute,st.wSecond);
    std::wstring line = hdr;
    line += L"webhost/"; line += (where?where:L"?");
    line += L": "; line += (detail?detail:L"");
    line += L"\r\n";
    writeFileUtf8(logsDir()+L"\\webhost_errors.log", line, true);
    Breadcrumb(L"webhost: error logged");
}

// ---------------------------------------------------------------------------
//  small UTF helpers
// ---------------------------------------------------------------------------
static std::string W2U8(const std::wstring& w){
    if(w.empty()) return std::string();
    int n=WideCharToMultiByte(CP_UTF8,0,w.c_str(),(int)w.size(),NULL,0,NULL,NULL);
    std::string s(n,0);
    if(n) WideCharToMultiByte(CP_UTF8,0,w.c_str(),(int)w.size(),&s[0],n,NULL,NULL);
    return s;
}
static std::wstring U82W(const std::string& s){
    if(s.empty()) return std::wstring();
    int n=MultiByteToWideChar(CP_UTF8,0,s.c_str(),(int)s.size(),NULL,0);
    std::wstring w(n,0);
    if(n) MultiByteToWideChar(CP_UTF8,0,s.c_str(),(int)s.size(),&w[0],n);
    return w;
}

// The bridge verbs live in a separate translation unit (webhost_bridge.inc) so
// this file stays focused on OLE hosting. It returns a JSON string for a verb.
std::wstring WebHostBridge_Call(int kind, const std::wstring& verb,
                                const std::wstring& argsJson);
// Build the boot payload (sections, form config, identity, theme) for a kind.
std::wstring WebHostBridge_BootPayload(int kind);
// The embedded HTML document (full page) for a kind.
std::wstring WebHostBridge_Html(int kind);

bool WebHost_Available(){
    // MSHTML is present whenever the WebBrowser CLSID resolves. Probe without
    // creating a window so callers can fall back deterministically.
    IUnknown* p=NULL;
    HRESULT hr=CoCreateInstance(CLSID_WebBrowser_local,NULL,
        CLSCTX_INPROC_SERVER,IID_IUnknown,(void**)&p);
    if(SUCCEEDED(hr) && p){ p->Release(); return true; }
    return false;
}

// ---------------------------------------------------------------------------
//  Browser emulation (FEATURE_BROWSER_EMULATION)
//
//  A hosted WebBrowser control defaults to IE7 *quirks* mode regardless of any
//  <meta http-equiv='X-UA-Compatible'> tag — that is why modern CSS (flexbox /
//  grid / custom properties) and modern JS (JSON, querySelectorAll, …) silently
//  fail with "Object doesn't support this property or method". The only reliable
//  fix is to register our EXE under FEATURE_BROWSER_EMULATION so Trident runs in
//  the newest standards mode installed on the machine.
//
//  We write to HKCU (per-user, no admin needed) for the bare EXE name, which is
//  what the WebBrowser control keys off. Value 11001 = IE11 edge/standards mode
//  (falls back gracefully to whatever Trident is actually installed). Done once,
//  early, before any browser is created. Idempotent and failure-tolerant.
// ---------------------------------------------------------------------------
void WebHost_SetBrowserEmulation(){
    wchar_t path[MAX_PATH]={0};
    if(!GetModuleFileNameW(NULL,path,MAX_PATH)) return;
    const wchar_t* exe=path;
    for(const wchar_t* p=path; *p; ++p) if(*p==L'\\'||*p==L'/') exe=p+1;
    if(!exe || !*exe) return;

    const wchar_t* SUBKEY =
        L"Software\\Microsoft\\Internet Explorer\\Main\\FeatureControl\\FEATURE_BROWSER_EMULATION";
    HKEY hk=NULL;
    if(RegCreateKeyExW(HKEY_CURRENT_USER,SUBKEY,0,NULL,REG_OPTION_NON_VOLATILE,
                       KEY_READ|KEY_WRITE,NULL,&hk,NULL)!=ERROR_SUCCESS)
        return;
    DWORD desired=11001;                 // IE11 edge mode (standards)
    DWORD cur=0, cb=sizeof(cur), type=0;
    bool need=true;
    if(RegQueryValueExW(hk,exe,NULL,&type,(LPBYTE)&cur,&cb)==ERROR_SUCCESS
       && type==REG_DWORD && cur==desired)
        need=false;                      // already set — idempotent
    if(need)
        RegSetValueExW(hk,exe,0,REG_DWORD,(const BYTE*)&desired,sizeof(desired));
    RegCloseKey(hk);

    // also register under the WOW6432 view is unnecessary for a 32-bit EXE on
    // x64: the control reads the native HKCU view, which is what we wrote.
}

#include "webhost_host.inc"
