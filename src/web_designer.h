// ============================================================================
//  web_designer.h — retired browser designer compatibility surface.
//
//  PrintDesigner_Open routes directly to the embedded native GDI designer.
//  WebDesigner_Open is retained only as a safe false-returning compatibility
//  stub; web-format import/export remains available through print_designer.h.
// ============================================================================
#pragma once
#include <windows.h>
#include <vector>

// The loopback/browser runtime is disabled. Always returns false so an older
// caller that still probes this entry point falls back to the native designer.
bool WebDesigner_Open(HWND owner, const std::vector<int>& sectionIds);
