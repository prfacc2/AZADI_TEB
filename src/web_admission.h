// ============================================================================
//  web_admission.h — embedded HTML/CSS/JS Patient-Admission surface (v1.33.0).
//
//  The «پذیرش بیمار» screen is rendered by a modern Chromium engine (Microsoft
//  WebView2) embedded *inside* the reception tab — NOT in an external browser.
//  A tiny loopback (127.0.0.1) HTTP host serves the bundled admission assets
//  from RCDATA; a two-way JSON IPC bridge keeps C++ and the page fully synced:
//
//    JS -> C++ :  patient.lookup / patient.search / service.search /
//                 admission.save / print.* / ui.toggle* / doctor.search …
//    C++ -> JS :  patient.load / services.update / queue.update / ps.update …
//
//  If the WebView2 runtime is NOT present the caller keeps the proven native
//  GDI reception form, so the app works on every Windows (7 → 11+) offline.
// ============================================================================
#pragma once
#include <windows.h>
#include <string>

// True when a WebView2 runtime (Evergreen or fixed) is detected on this box.
bool WebAdmission_Available();

// Start (idempotent) the loopback asset/API host. Returns the port, or 0.
int  WebAdmission_EnsureHost();

// Create the embedded WebView2 view as a child of `parent`, sized to fill it,
// and load the admission page. Returns the WebView host HWND on success, or
// NULL if the WebView could not be created (caller falls back to native form).
// The view is fully wired to the C++ bridge before this returns control.
HWND WebAdmission_CreateView(HWND parent);

// Resize the embedded view to the parent's client rect (call on WM_SIZE).
void WebAdmission_Resize(HWND view, int w, int h);

// Push a C++ -> JS event with a JSON payload (e.g. "patient.load").
void WebAdmission_PushEvent(const char* eventName, const std::string& jsonData);

// Destroy the embedded view + release its WebView2 resources.
void WebAdmission_DestroyView(HWND view);
