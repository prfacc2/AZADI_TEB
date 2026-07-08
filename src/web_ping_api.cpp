// ============================================================================
//  web_ping_api.cpp — /api/ping handler for the multi-page shell demo (v1.40.0)
//
//  Proves the whole multi-page pipeline end-to-end: the ping page (RCDATA
//  600..602) is served through the generic WebPages_* dispatcher + the worker
//  thread pool, and its dedicated «ping» verb round-trips here. The handler is
//  registered in WebPages_RegisterBuiltins() (src/web_pages.cpp).
//
//  It echoes the caller's message back, reports the running app version and the
//  live worker-pool size, and is deliberately trivial + allocation-light so it
//  is a clean template for future page APIs. C++17, no runtime deps.
// ============================================================================
#include "app.h"
#include "web_pages.h"
#include "web_thread_pool.h"

#include <string>
#include <windows.h>

namespace {

// minimal JSON string escaper for the (short, trusted) echo value.
static std::string ping_jsonEsc(const std::string& s) {
    std::string o;
    o.reserve(s.size() + 8);
    for (size_t i = 0; i < s.size(); ++i) {
        unsigned char c = (unsigned char)s[i];
        switch (c) {
            case '"':  o += "\\\""; break;
            case '\\': o += "\\\\"; break;
            case '\n': o += "\\n"; break;
            case '\r': o += "\\r"; break;
            case '\t': o += "\\t"; break;
            default:
                if (c < 0x20) { char b[8]; sprintf(b, "\\u%04x", c); o += b; }
                else o += (char)c;
        }
    }
    return o;
}

// pull the top-level string value for `key` out of a small JSON object (the
// same tolerant style used elsewhere in the codebase — good enough for the
// trusted, self-produced request bodies from common.js).
static bool ping_jsonStr(const std::string& j, const std::string& key,
                         std::string& out) {
    std::string pat = "\"" + key + "\"";
    size_t p = j.find(pat); if (p == std::string::npos) return false;
    p = j.find(':', p + pat.size()); if (p == std::string::npos) return false;
    p++; while (p < j.size() && (j[p] == ' ' || j[p] == '\t')) p++;
    if (p >= j.size() || j[p] != '"') return false;
    p++; std::string s;
    while (p < j.size()) {
        char c = j[p++];
        if (c == '\\' && p < j.size()) {
            char e = j[p++];
            switch (e) {
                case 'n': s += '\n'; break; case 'r': s += '\r'; break;
                case 't': s += '\t'; break; case '"': s += '"'; break;
                case '\\': s += '\\'; break; case '/': s += '/'; break;
                default: s += e;
            }
        } else if (c == '"') break;
        else s += c;
    }
    out = s; return true;
}

static std::string ping_appVersionUtf8() {
    // APP_VERSION_W is a wide literal (L"1.40.0"); narrow it (ASCII digits/dots).
    std::wstring w = APP_VERSION_W;
    std::string s; s.reserve(w.size());
    for (size_t i = 0; i < w.size(); ++i) s += (char)(w[i] & 0x7F);
    return s;
}

} // namespace

std::string WebPing_Handle(const std::string& body, const std::string& page) {
    std::string echo;
    ping_jsonStr(body, "echo", echo);

    // reflect the incoming X-Az-Page (falls back to the JSON "page" field).
    std::string pg = page;
    if (pg.empty()) ping_jsonStr(body, "page", pg);

    int workers = WebPool_Ready() ? WebPool_Init(nullptr) : 0;
    // WebPool_Init(nullptr) is a safe no-op once initialised and returns the
    // live worker count; when the pool is not up it returns 0 (classic path).

    std::string pong = "pong";
    if (!echo.empty()) pong = "pong: " + echo;

    std::string o = "{";
    o += "\"ok\":true";
    o += ",\"pong\":\"" + ping_jsonEsc(pong) + "\"";
    o += ",\"page\":\""  + ping_jsonEsc(pg)   + "\"";
    o += ",\"version\":\"" + ping_jsonEsc(ping_appVersionUtf8()) + "\"";
    o += ",\"workers\":" + std::to_string(workers);
    o += ",\"t\":" + std::to_string((long long)GetTickCount());
    o += "}";
    return o;
}
