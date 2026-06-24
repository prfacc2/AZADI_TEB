// ============================================================================
//  webhost.h — hybrid HTML/CSS/JS presentation host (release 1.13.0, §3/§4)
//
//  The Reception and Appointment screens render their VISIBLE interface as
//  HTML/CSS/JS inside the app, while C++ stays the host, validator, lifecycle
//  manager, persistence layer and bridge controller.
//
//  The renderer is the SYSTEM MSHTML / WebBrowser (Trident) OLE control. It is
//  present on every Windows 7 → 11 (x86 and x64) with NO extra runtime, NO
//  shipped DLL and NO internet — so the product stays a single static EXE and
//  works in offline / air-gapped clinics. The HTML/CSS/JS assets are embedded
//  in the EXE (RCDATA) and served from an in-process about:blank document, so
//  nothing ever touches the disk or the network.
//
//  Native <-> web bridge:
//    • C++ exposes an IDispatch as window.external. JS calls
//        window.external.call("verb", "jsonArgs")  -> returns a JSON string.
//      C++ owns validation / persistence / printing and answers synchronously.
//    • C++ pushes state into JS by invoking  window.azReceive(jsonString)
//      (a global the page installs). All state flows through this one channel
//      so the native and web layers can never drift.
//
//  Lifecycle contract (deterministic, race-free):
//    1. Native handler creates the host window over the tab area.
//    2. A centered loader overlay (progress bar) is shown immediately.
//    3. Native state / section metadata / form configuration are synchronized
//       (WebHost_BeginSync ... the page is NOT created until this completes).
//    4. The browser is created, the embedded document is navigated.
//    5. On DocumentComplete the bridge is wired, the boot payload is pushed,
//       the loader fades out and the web view is revealed.
//    6. Any failure keeps the app alive and is written to the persistent error
//       log (never a noisy UI trace).
// ============================================================================
#pragma once
#include <windows.h>
#include <string>

// The two hybrid surfaces the host can render.
enum WebHostKind {
    WH_RECEPTION = 0,   // پذیرش / پذیرش جدید
    WH_APPOINTMENT = 1  // نوبت‌دهی
};

// Create a hybrid host as a child of `parent`, filling its client area. The
// host paints its own centered loader first, then renders the HTML UI for the
// requested surface. Returns the host HWND (or NULL on hard failure — the
// caller then falls back to the classic native form so the app never breaks).
//
// `parent` is expected to be a tab-page window; the host sizes itself to the
// parent client rect and re-flows on WM_SIZE.
HWND WebHost_Create(HWND parent, int kind);

// True if MSHTML hosting is available on this machine (it always is on a real
// Windows install; the probe exists so callers can fall back deterministically
// in a degraded environment instead of showing a blank pane).
bool WebHost_Available();

// Persistent, throttled error logger for the hybrid layer. Writes to
// logs\webhost_errors.log (created on demand). NEVER shows UI; NEVER floods —
// identical messages within a short window are coalesced. Use for sync
// failures, navigation failures and bridge faults.
void WebHost_LogError(const wchar_t* where, const wchar_t* detail);
