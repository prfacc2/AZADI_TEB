// ============================================================================
//  web_pages.h — multi-page embedded-web PAGE REGISTRY (v1.40.0)
//
//  A tiny, dependency-free registry that lets the single loopback HTTP host
//  (src/web_admission_http.inc) serve MANY embedded pages — not just the
//  admission bundle — from RCDATA blobs, and route /api/<verb> requests to the
//  page module that owns that verb.
//
//  Two registration surfaces:
//    * WebPages_RegisterAsset(pageId, urlPath, rcId, contentType)
//        — bind a URL path (e.g. "/ping.css") to an embedded RCDATA id.
//    * WebPages_RegisterVerb(verb, handler)
//        — bind an /api verb (e.g. "ping") to a std::string(payload,page) fn.
//
//  Lookups are O(n) over small vectors (a handful of assets/verbs); no map, no
//  STL surprises across the MinGW static runtime. All C++17, no runtime deps.
//  The registry is populated once at boot from WebPages_RegisterBuiltins().
// ============================================================================
#pragma once
#include <string>
#include <functional>

// Content-Type helper for a URL path (by extension). Returns a static string
// literal; falls back to application/octet-stream for unknown extensions.
const char* WebPages_ContentTypeFor(const std::string& urlPath);

// Register an embedded asset: a URL path served verbatim from an RCDATA id.
//   pageId      — owning page (diagnostics / X-Az-Page correlation), may be "".
//   urlPath     — exact request path, e.g. "/ping.css" or "/index.html".
//   rcId        — RCDATA resource id in app.rc.
//   contentType — MIME type; pass nullptr to derive from the path extension.
void WebPages_RegisterAsset(const char* pageId, const char* urlPath,
                            int rcId, const char* contentType);

// Resolve a URL path to an RCDATA id + content-type. Returns false when the
// path is not registered (caller replies 404).
bool WebPages_ResolveAsset(const std::string& urlPath, int& rcIdOut,
                           const char*& contentTypeOut);

// An API verb handler: receives the raw JSON request body plus the X-Az-Page
// header value (may be empty), returns a JSON response body.
typedef std::function<std::string(const std::string& body,
                                  const std::string& page)> WebVerbHandler;

// Register (or replace) a handler for /api/<verb>.
void WebPages_RegisterVerb(const char* verb, WebVerbHandler handler);

// Dispatch /api/<verb>. Returns true and fills `out` when a handler exists;
// returns false when the verb is unknown (caller falls back to the legacy
// admissionApi handler for backwards compatibility).
bool WebPages_DispatchVerb(const std::string& verb, const std::string& body,
                           const std::string& page, std::string& out);

// Populate the registry with every built-in page (shell assets + ping demo)
// and built-in verb (ping, client.log, client.metrics). Idempotent.
void WebPages_RegisterBuiltins();

// ---- built-in verb handlers implemented in their own translation units ----
// src/web_ping_api.cpp
std::string WebPing_Handle(const std::string& body, const std::string& page);
