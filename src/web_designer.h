// ============================================================================
//  web_designer.h — HTML/CSS/JS print designer hosted in an app-owned window
//  (release 1.19.0). A tiny embedded HTTP server (Winsock, loopback-only)
//  serves the designer assets from RCDATA and exposes a JSON bridge under
//  /api/*. The page is shown inside a WebView2 control when the runtime is
//  present; otherwise the caller falls back to the native GDI designer so the
//  app keeps working on every Windows (7→11+) and on weak hardware.
// ============================================================================
#pragma once
#include <windows.h>
#include <string>
#include <vector>

// Start (idempotent) the loopback HTTP host that serves the designer assets +
// /api bridge. Returns the port it is listening on, or 0 on failure. The host
// runs on a single background thread and is extremely light (no busy loops).
int  WebDesigner_EnsureHost();

// Returns the loopback URL for the designer, seeded with the section to edit.
// e.g. http://127.0.0.1:<port>/index.html?section=<id>
std::wstring WebDesigner_Url(int sectionId);

// True if a WebView2 runtime is available on this machine.
bool WebDesigner_WebViewAvailable();

// Open the HTML designer for the given section(s) in an app-owned WebView
// window. Returns true if the WebView path was used; false if it could not be
// created (caller should then fall back to the native designer).
bool WebDesigner_Open(HWND owner, const std::vector<int>& sectionIds);
