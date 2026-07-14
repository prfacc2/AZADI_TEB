// ============================================================================
//  av_reception.h — Avalonia (.NET) Patient-Admission surface host (v1.48.0).
//
//  The «پذیرش بیمار» screen's APPEARANCE is now provided by an embedded
//  Avalonia UI process (AzadiTeb.Reception.exe) instead of the retired embedded
//  HTML page. The C++ core is UNCHANGED: the Avalonia UI talks to the very same
//  loopback (127.0.0.1) /api bridge the HTML page used (see web_admission_*),
//  so patients, services, insurance, billing, queue and printing stay 100%
//  driven by C++ — C++ remains the single source of truth.
//
//  Embedding model:
//   * A plain child host window is created inside the reception tab.
//   * The Avalonia exe is launched with:  --port <loopbackPort> --parent <hwnd>
//   * The Avalonia process reparents its top window into that host (SetParent)
//     and the C++ side keeps it sized to fill the tab (WM_SIZE → MoveWindow).
//   * If .NET/Avalonia cannot run (exe missing, launch failed), the caller
//     falls back to the proven engines below (WebView2/MSHTML HTML, then the
//     native GDI form) so the feature is NEVER lost.
// ============================================================================
#pragma once
#include <windows.h>

// True when the Avalonia reception exe is available (present on disk after a
// one-time extraction from the embedded RCDATA payload, or shipped alongside).
bool AvReception_Available();

// Create the embedded Avalonia view as a child of `parent`, launch the process
// and wait (bounded) until its window reparents in. Returns the host child HWND
// on success, or NULL so the caller can fall back to another engine.
HWND AvReception_CreateView(HWND parent);

// Resize the host (and the embedded Avalonia child) to the given client size.
void AvReception_Resize(HWND view, int w, int h);

// True if `view` is a live Avalonia host we own.
bool AvReception_Owns(HWND view);

// Destroy the host window and terminate the Avalonia child process.
bool AvReception_DestroyView(HWND view);
