// ============================================================================
//  printer_designer.inc — DEPRECATED (RELEASE 1.4.0, §3)
//  ----------------------------------------------------------------------------
//  The legacy in-place print designer that used to live here has been replaced
//  by the full vector print-designer in src/print_designer.{h,cpp} (with the
//  section picker, WYSIWYG canvas, draggable/resizable/rotatable items,
//  inspector, 20 built-in templates and .aztpl import/export).
//
//  This file is now only a thin shim: the single legacy entry point
//  openPrintDesigner(owner, sectionIdx) forwards to the new designer so any
//  remaining caller keeps working. New code should call PrintDesigner_Open()
//  (or route through the settings window) directly.
//
//  NOTE: This file is #included from printer.cpp; print_designer.h is included
//  there as well so PrintDesigner_Open is visible.
// ============================================================================

void openPrintDesigner(HWND owner, int /*sectionIdx*/){
    // The new designer opens with its own section picker, so the legacy
    // sectionIdx hint is intentionally ignored.
    PrintDesigner_Open(owner ? owner : g_hFrame);
}
