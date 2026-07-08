// ============================================================================
//  web_pages.cpp — multi-page embedded-web PAGE REGISTRY (v1.40.0)
//
//  Implements web_pages.h: a small, thread-safe registry of embedded assets
//  (URL path -> RCDATA id) and /api verb handlers, plus the built-in verbs
//  every page can rely on:
//     * ping           -> src/web_ping_api.cpp (demo round-trip)
//     * client.log     -> append a structured line to logs\client.log
//     * client.metrics -> append a structured line to logs\client.metrics
//
//  Registration happens once at boot (WebPages_RegisterBuiltins), before the
//  loopback host starts serving. Lookups run on the per-connection worker
//  threads, so the registry is guarded by a critical section. C++17, no deps.
// ============================================================================
#include "app.h"
#include "web_pages.h"

#include <vector>
#include <string>
#include <windows.h>

// ---------------------------------------------------------------------------
//  Registry storage (guarded).
// ---------------------------------------------------------------------------
namespace {

struct AssetEntry {
    std::string page;
    std::string path;
    int         rcId;
    const char* ctype;
};
struct VerbEntry {
    std::string    verb;
    WebVerbHandler handler;
};

static std::vector<AssetEntry> g_assets;
static std::vector<VerbEntry>  g_verbs;

static CRITICAL_SECTION g_regCs;
static bool g_regCsInit = false;
static bool g_builtinsDone = false;

static void regLock()   { if (g_regCsInit) EnterCriticalSection(&g_regCs); }
static void regUnlock() { if (g_regCsInit) LeaveCriticalSection(&g_regCs); }

static void ensureCs() {
    if (!g_regCsInit) { InitializeCriticalSection(&g_regCs); g_regCsInit = true; }
}

// tiny UTF-8 -> wide for the log sinks (self-contained; does not depend on the
// web_admission.cpp-local helpers which are in an anonymous namespace there).
static std::wstring pgU82W(const std::string& s) {
    if (s.empty()) return L"";
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), NULL, 0);
    std::wstring w(n, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &w[0], n);
    return w;
}

} // namespace

// ---------------------------------------------------------------------------
//  Content-Type by extension.
// ---------------------------------------------------------------------------
const char* WebPages_ContentTypeFor(const std::string& urlPath) {
    std::string p = urlPath;
    size_t q = p.find('?'); if (q != std::string::npos) p = p.substr(0, q);
    auto ends = [&](const char* ext) {
        size_t n = strlen(ext);
        return p.size() >= n && p.compare(p.size() - n, n, ext) == 0;
    };
    if (p == "/" || ends(".html") || ends(".htm")) return "text/html; charset=utf-8";
    if (ends(".css"))  return "text/css; charset=utf-8";
    if (ends(".js"))   return "application/javascript; charset=utf-8";
    if (ends(".json")) return "application/json; charset=utf-8";
    if (ends(".ttf"))  return "font/ttf";
    if (ends(".woff2"))return "font/woff2";
    if (ends(".woff")) return "font/woff";
    if (ends(".svg"))  return "image/svg+xml";
    if (ends(".png"))  return "image/png";
    if (ends(".jpg") || ends(".jpeg")) return "image/jpeg";
    if (ends(".ico"))  return "image/x-icon";
    return "application/octet-stream";
}

// ---------------------------------------------------------------------------
//  Asset registration + resolution.
// ---------------------------------------------------------------------------
void WebPages_RegisterAsset(const char* pageId, const char* urlPath,
                            int rcId, const char* contentType) {
    ensureCs();
    if (!urlPath) return;
    AssetEntry e;
    e.page  = pageId ? pageId : "";
    e.path  = urlPath;
    e.rcId  = rcId;
    e.ctype = contentType ? contentType : WebPages_ContentTypeFor(urlPath);
    regLock();
    // replace an existing binding for the same path (last registration wins).
    bool replaced = false;
    for (size_t i = 0; i < g_assets.size(); ++i) {
        if (g_assets[i].path == e.path) { g_assets[i] = e; replaced = true; break; }
    }
    if (!replaced) g_assets.push_back(e);
    regUnlock();
}

bool WebPages_ResolveAsset(const std::string& urlPath, int& rcIdOut,
                           const char*& contentTypeOut) {
    std::string p = urlPath;
    size_t q = p.find('?'); if (q != std::string::npos) p = p.substr(0, q);
    bool hit = false;
    regLock();
    for (size_t i = 0; i < g_assets.size(); ++i) {
        if (g_assets[i].path == p) {
            rcIdOut = g_assets[i].rcId;
            contentTypeOut = g_assets[i].ctype;
            hit = true; break;
        }
    }
    regUnlock();
    return hit;
}

// ---------------------------------------------------------------------------
//  Verb registration + dispatch.
// ---------------------------------------------------------------------------
void WebPages_RegisterVerb(const char* verb, WebVerbHandler handler) {
    ensureCs();
    if (!verb || !handler) return;
    VerbEntry e; e.verb = verb; e.handler = handler;
    regLock();
    bool replaced = false;
    for (size_t i = 0; i < g_verbs.size(); ++i) {
        if (g_verbs[i].verb == e.verb) { g_verbs[i] = e; replaced = true; break; }
    }
    if (!replaced) g_verbs.push_back(e);
    regUnlock();
}

bool WebPages_DispatchVerb(const std::string& verb, const std::string& body,
                           const std::string& page, std::string& out) {
    WebVerbHandler h = nullptr;
    regLock();
    for (size_t i = 0; i < g_verbs.size(); ++i) {
        if (g_verbs[i].verb == verb) { h = g_verbs[i].handler; break; }
    }
    regUnlock();
    if (!h) return false;
    out = h(body, page);
    return true;
}

// ---------------------------------------------------------------------------
//  Built-in verbs: client.log / client.metrics (persist to logs\).
//  The body is written verbatim (already-valid JSON from common.js) so no
//  parsing is needed; each line is a self-describing record.
// ---------------------------------------------------------------------------
static std::string wp_clientLog(const std::string& body, const std::string& page) {
    (void)page;
    std::wstring path = logsDir() + L"\\client.log";
    writeFileUtf8(path, pgU82W(body) + L"\r\n", /*append*/true);
    return "{\"ok\":true}";
}
static std::string wp_clientMetrics(const std::string& body, const std::string& page) {
    (void)page;
    std::wstring path = logsDir() + L"\\client.metrics";
    writeFileUtf8(path, pgU82W(body) + L"\r\n", /*append*/true);
    return "{\"ok\":true}";
}

// ---------------------------------------------------------------------------
//  Boot: register every built-in page + verb. Idempotent.
//  RCDATA ids MUST match app.rc:
//     shell : 500 common.css · 501 common.js · 502 vazir.ttf
//     ping  : 600 index.html · 601 ping.css   · 602 ping.js
//  (admission 400..405 stays owned by the legacy hard-coded switch and is NOT
//   re-registered here, so the two paths never collide.)
// ---------------------------------------------------------------------------
void WebPages_RegisterBuiltins() {
    ensureCs();
    regLock();
    bool done = g_builtinsDone;
    g_builtinsDone = true;
    regUnlock();
    if (done) return;

    // shared shell assets — one copy served to every page.
    WebPages_RegisterAsset("shell", "/common.css", 500, "text/css; charset=utf-8");
    WebPages_RegisterAsset("shell", "/common.js",  501, "application/javascript; charset=utf-8");
    WebPages_RegisterAsset("shell", "/vazir.ttf",  502, "font/ttf");

    // ping demo page. NOTE: this page owns "/" and "/index.html" ONLY when the
    // host is asked for the ping page; the admission bundle keeps its own "/"
    // via the legacy switch which runs FIRST. The explicit ping asset paths
    // below are what the ping page actually requests.
    WebPages_RegisterAsset("ping", "/ping/index.html", 600, "text/html; charset=utf-8");
    WebPages_RegisterAsset("ping", "/ping.css",        601, "text/css; charset=utf-8");
    WebPages_RegisterAsset("ping", "/ping.js",         602, "application/javascript; charset=utf-8");

    // built-in verbs.
    WebPages_RegisterVerb("ping",           &WebPing_Handle);
    WebPages_RegisterVerb("client.log",     &wp_clientLog);
    WebPages_RegisterVerb("client.metrics", &wp_clientMetrics);
}
