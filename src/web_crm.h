// ============================================================================
//  web_crm.h — embedded HTML/CSS/JS CRM Management surface (v1.70.0).
//
//  The «مدیریت درمانگاه» (management) screen is rendered by a modern Chromium
//  engine (Microsoft WebView2) embedded *inside* the manage tab — NOT in an
//  external browser — replacing the legacy native C++/GDI manage panel. The
//  page is fully INLINED from RCDATA (assets/crm) and handed directly to the
//  engine (NavigateToString / document.write): NO local server, NO loopback,
//  NO ports. A two-way JSON IPC bridge keeps C++ and the page fully synced:
//
//    JS -> C++ :  crm.init / crm.sections.list / crm.sections.save /
//                 crm.patients.list / crm.patients.save / crm.doctors.list /
//                 crm.doctors.save / crm.services.list / crm.services.save /
//                 crm.employees.list / crm.employees.save / crm.messages.list /
//                 crm.backup / crm.settings …
//    C++ -> JS :  crm.refresh (push a fresh snapshot to an open view)
//
//  If the WebView2 runtime is NOT present the universal MSHTML/Trident fallback
//  (which ships with every Windows) renders the same page inside the app, so
//  the management surface works on every Windows (7 → 11+) offline.
//
//  The native GDI manage panel (src/manage.inc) is DISABLED, not deleted — its
//  createManageScreen() body is preserved under #if 0 so it can be re-enabled.
// ============================================================================
#pragma once
#include <windows.h>
#include <string>

// True whenever ANY embedded engine can host the page (always true in practice,
// since the MSHTML WebBrowser control is present on every Windows).
bool WebCrm_Available();

// SERVERLESS bootstrap (idempotent): pre-builds the fully-inlined CRM page
// variants so the first manage-screen open is instant. No sockets/ports.
void WebCrm_Prepare();

// Create the management SCREEN host window as a child of `frame` (this is what
// SC_MANAGE switches to), sized to the frame content rect, and host the embedded
// CRM web view inside it. Returns the host HWND on success, or NULL on failure.
// The host owns the web-view lifecycle (created on WM_CREATE, resized on
// WM_SIZE, destroyed on WM_DESTROY).
HWND WebCrm_CreateScreen(HWND frame);

// Push a C++ -> JS event with a JSON payload to every open CRM view (e.g.
// "crm.refresh"). Safe to call even when no view is open (becomes a no-op).
void WebCrm_PushEvent(const char* eventName, const std::string& jsonData);

// Route a pending message (typically WM_KEYDOWN) through the embedded CRM
// browser control BEFORE the main pump calls TranslateMessage/DispatchMessage,
// so Tab / Enter / Ctrl+A / arrow keys reach the hosted HTML page. Returns true
// iff the browser control consumed the message (caller must then skip
// TranslateMessage/DispatchMessage). Only views whose host HWND is an ancestor
// of msg->hwnd are consulted, so unrelated windows are untouched.
bool WebCrm_TranslateAccel(MSG* msg);
