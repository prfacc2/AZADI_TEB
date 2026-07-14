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

// LIVE SYNC from Management: call this whenever the service catalog or the
// insurance tables change (add / edit / delete). It pushes a fresh catalog +
// insurance snapshot to every open embedded Admission view so the operator sees
// the change immediately, with NO page reload. Safe to call even when no view
// is open (it becomes a no-op). Defined in web_admission.cpp.
void WebAdmission_NotifyCatalogChanged();

// Destroy the embedded view + release its WebView2 resources.
void WebAdmission_DestroyView(HWND view);

// Route a pending message (typically WM_KEYDOWN) through the embedded browser
// control BEFORE the main pump calls TranslateMessage/DispatchMessage. This is
// REQUIRED so Tab / Enter / Ctrl+A / arrow keys reach the hosted HTML page:
//   * MSHTML  → IOleInPlaceActiveObject::TranslateAccelerator (+ the site's
//               IDocHostUIHandler::TranslateAccelerator returning S_FALSE).
//   * WebView2→ handled internally via add_AcceleratorKeyPressed; this returns
//               false so the message continues normally.
// Returns true iff the browser control consumed the message (caller must then
// skip TranslateMessage/DispatchMessage). Only views whose host HWND is an
// ancestor of msg->hwnd are consulted, so unrelated windows are untouched.
bool WebAdmission_TranslateAccel(MSG* msg);
