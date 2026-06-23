// ============================================================================
//  ui_kit.h  — composable, theme-aware UI helper layer on top of Win32 + GDI/GDI+
//  Goal: give future redesigns simple, reusable, leak-free building blocks.
//  Everything respects the existing theme.cpp palette (g_theme) and the global
//  Scale() helper (S()), uses Vazirmatn fonts, and renders right-to-left.
//
//  IMPORTANT: nothing here hardcodes colors — all colors come from g_theme.
// ============================================================================
#pragma once
#include "app.h"

namespace uikit {

// --------------------------------------------------------------- RAII GDI ----
//  Leak-free wrappers around GDI handles. Selecting an object restores the
//  previous one automatically on scope exit (no orphaned HFONT/HBRUSH/HPEN).
struct GdiObj {
    HGDIOBJ h{nullptr};
    GdiObj() = default;
    explicit GdiObj(HGDIOBJ o):h(o){}
    GdiObj(const GdiObj&) = delete;
    GdiObj& operator=(const GdiObj&) = delete;
    GdiObj(GdiObj&& o) noexcept : h(o.h){ o.h=nullptr; }
    GdiObj& operator=(GdiObj&& o) noexcept {
        if(this!=&o){ reset(); h=o.h; o.h=nullptr; } return *this;
    }
    ~GdiObj(){ reset(); }
    void reset(){ if(h){ DeleteObject(h); h=nullptr; } }
    HGDIOBJ get() const { return h; }
};

//  Scoped SelectObject: selects `obj` into `dc`, restores the old object on
//  destruction. Use for transient pens/brushes/fonts/bitmaps in a paint pass.
struct SelectScope {
    HDC dc; HGDIOBJ old;
    SelectScope(HDC d, HGDIOBJ obj):dc(d){ old=SelectObject(d,obj); }
    SelectScope(const SelectScope&) = delete;
    SelectScope& operator=(const SelectScope&) = delete;
    ~SelectScope(){ if(dc) SelectObject(dc,old); }
};

//  Scoped device context with a backing bitmap for flicker-free double-buffer.
//  Create from a real window DC; BitBlt(dst) inside paint then let it free.
struct MemDC {
    HDC src{nullptr}, dc{nullptr};
    HBITMAP bmp{nullptr}; HGDIOBJ oldbmp{nullptr};
    int w{0}, h{0};
    MemDC(HDC srcDC, int cx, int cy):src(srcDC),w(cx),h(cy){
        dc=CreateCompatibleDC(srcDC);
        bmp=CreateCompatibleBitmap(srcDC,cx>0?cx:1,cy>0?cy:1);
        oldbmp=SelectObject(dc,bmp);
    }
    MemDC(const MemDC&) = delete;
    MemDC& operator=(const MemDC&) = delete;
    ~MemDC(){
        if(dc){ if(oldbmp) SelectObject(dc,oldbmp); DeleteDC(dc); }
        if(bmp) DeleteObject(bmp);
    }
    void blitTo(HDC dst, int x=0, int y=0){ BitBlt(dst,x,y,w,h,dc,0,0,SRCCOPY); }
};

//  Scoped GetDC / ReleaseDC pair — never leak a window DC.
struct WindowDC {
    HWND hwnd; HDC dc;
    explicit WindowDC(HWND h):hwnd(h){ dc=GetDC(h); }
    WindowDC(const WindowDC&) = delete;
    WindowDC& operator=(const WindowDC&) = delete;
    ~WindowDC(){ if(dc) ReleaseDC(hwnd,dc); }
};

// --------------------------------------------------- standardized headers ----
//  A single source of truth for every BLUE section title in the app. Pixel
//  identical everywhere: marginTop=10, marginBottom=4, fontHeight=14 (scaled).
//  Returns the y just below the header so callers can place the next row.
//  `x`/`right` are the horizontal bounds (RTL: text is right-aligned).
int  DrawSectionHeader(HDC dc, const wchar_t* text, int x, int right, int y);
//  The total vertical space (top margin + glyph + bottom margin) a section
//  header consumes — for layout math without painting.
int  SectionHeaderHeight();

// --------------------------------------------------------------- panels -------
//  Anti-aliased rounded panel/card. Uses the GDI+ helpers when available and
//  falls back to fillRoundRect. `bg` is the colour BEHIND the rounded corners.
void RoundedPanel(HDC dc, RECT rc, int radius, COLORREF fill, COLORREF border,
                  COLORREF bgBehind);
void Card(HDC dc, RECT rc);          // standard surface card (theme colours)

//  A small pill / chip with text — used for filters & inline warnings.
//  Returns the chip width actually drawn (for laying chips left-to-right RTL).
int  Chip(HDC dc, int rightX, int y, const wchar_t* text,
          COLORREF fill, COLORREF textCol, bool selected);

// ----------------------------------------------------- input field skin -------
//  Paint the rounded "well" behind an edit control (border + focus ring).
void InputWell(HDC dc, RECT rc, bool focused);

// ---------------------------------------------- Persian text normalization ----
//  Normalize Persian/Arabic text for search/compare:
//   ي→ی, ك→ک, ة→ه, ﻻ→لا, remove ZWNJ (U+200C) & tatweel (U+0640),
//   Arabic-Indic & Persian digits → ASCII, lower-case, collapse spaces.
std::wstring NormalizeFa(const std::wstring& s);

// ============================================================================
//  RELEASE 1.4.0 — NEW CONTROLS (§10) — all REAL HWND children so the
//  AzZOrderShield / AzLayoutGuard contracts hold. Each owns its GDI objects
//  via RAII and double-buffers through MemDC.
// ============================================================================

// Register all new window classes once (idempotent). Call at startup.
void Az_RegisterControls();

// --- AzSwitch (toggle) ------------------------------------------------------
//  A flat iOS-style on/off switch. State stored in window; query via
//  AzSwitch_Get / set via AzSwitch_Set. Sends WM_COMMAND BN_CLICKED to parent
//  on toggle (use GetDlgCtrlID to identify).
HWND AzSwitch_Create(HWND parent, int id, bool on, int x,int y,int w,int h);
bool AzSwitch_Get(HWND sw);
void AzSwitch_Set(HWND sw, bool on);

// --- AzNumberSpinner --------------------------------------------------------
HWND AzNumberSpinner_Create(HWND parent, int id, double val, double mn,
                            double mx, double step, int x,int y,int w,int h);
double AzNumberSpinner_Get(HWND sp);
void   AzNumberSpinner_Set(HWND sp, double v);

// --- AzColorPicker ----------------------------------------------------------
//  A swatch button; click opens the system colour dialog. Current colour is
//  stored on the control. Sends WM_COMMAND on change.
HWND     AzColorPicker_Create(HWND parent, int id, COLORREF c, int x,int y,int w,int h);
COLORREF AzColorPicker_Get(HWND cp);
void     AzColorPicker_Set(HWND cp, COLORREF c);

// --- AzDropZone -------------------------------------------------------------
//  A dashed drop target accepting file drops (WM_DROPFILES). On drop it posts
//  WM_APP+40 to the parent with WPARAM = control id, LPARAM = pointer to a
//  heap std::wstring* (the dropped path, parent must delete). Also clickable to
//  open a file dialog when a filter is set.
#define AZ_DROPZONE_DROPPED (WM_APP+40)
HWND AzDropZone_Create(HWND parent, int id, const wchar_t* caption,
                       const wchar_t* filter, int x,int y,int w,int h);

// --- Designer-only painters -------------------------------------------------
//  These are lightweight painter helpers (NOT separate HWNDs by default) used
//  by the print designer canvas. Kept here so the kit owns the look.
void AzGridLayer_Paint(HDC dc, RECT area, double mmPerPx, COLORREF line);
void AzRulerH_Paint(HDC dc, RECT area, double originPx, double mmPerPx);
void AzRulerV_Paint(HDC dc, RECT area, double originPx, double mmPerPx);
//  Draw the 8 resize handles + 1 rotate handle around a selected rect.
void AzHandle_Paint(HDC dc, RECT sel);

// ============================================================================
//  AzLayoutGuard (Handler #1, §6.2) — anti-overlap / anti-jump.
//  After a layout pass, Verify() walks the direct children of hParent and, in
//  RELEASE builds, softly corrects any two that overlap by pushing the lower-z
//  one below the higher-z one and reposting WM_APP_LAYOUT_REDO. Pairs flagged
//  with WS_EX_LAYERED or the "az_overlap_ok" window-prop are excluded.
// ============================================================================
void AzLayoutGuard_AllowOverlap(HWND child);   // mark a child as overlap-OK
bool AzLayoutGuard_Verify(HWND hParent);        // returns false if it corrected

// ============================================================================
//  AzZOrderShield (Handler #2, §6.2) — no bleed-through across pages.
//  Push(hPage): hides all siblings of hPage, remembers their visibility, and
//  lets hPage cover the parent client area. Pop(hPage): restores them.
//  Re-entrant via an internal stack; double-push is a no-op.
// ============================================================================
void AzZOrderShield_Push(HWND hPage);
void AzZOrderShield_Pop(HWND hPage);

} // namespace uikit
