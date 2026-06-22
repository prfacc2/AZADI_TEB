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

} // namespace uikit
