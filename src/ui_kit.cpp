// ============================================================================
//  ui_kit.cpp — implementation of the composable UI helper layer.
//  See ui_kit.h. All colours come from g_theme; all sizes go through S().
// ============================================================================
#include "ui_kit.h"

namespace uikit {

// --------------------------------------------------- standardized headers ----
int SectionHeaderHeight(){
    // marginTop(10) + glyph(14) + marginBottom(4)  (all scaled)
    return S(10) + S(14) + S(4);
}
int DrawSectionHeader(HDC dc, const wchar_t* text, int x, int right, int y){
    const int marginTop = S(10);
    const int fontH      = S(14);
    const int marginBot  = S(4);
    int top = y + marginTop;
    // Use the bold UI font for consistency, but force the standardized height
    // via a temporary font so EVERY section header is pixel-identical.
    HFONT f = CreateFontW(-fontH,0,0,0,FW_BOLD,0,0,0,DEFAULT_CHARSET,
                          OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,
                          CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Vazirmatn");
    {
        SelectScope sf(dc, f ? (HGDIOBJ)f : (HGDIOBJ)g_fUIB);
        int oldBk = SetBkMode(dc, TRANSPARENT);
        COLORREF oldCol = SetTextColor(dc, g_theme.accent);   // blue title
        RECT r = { x, top, right, top + fontH + S(2) };
        DrawTextW(dc, text, -1, &r,
                  DT_RIGHT|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);
        SetTextColor(dc, oldCol);
        SetBkMode(dc, oldBk);
    }
    if(f) DeleteObject(f);
    return top + fontH + marginBot;
}

// --------------------------------------------------------------- panels -------
void RoundedPanel(HDC dc, RECT rc, int radius, COLORREF fill, COLORREF border,
                  COLORREF bgBehind){
    gpRoundRectBg(dc, rc, radius, fill, border, bgBehind);
}
void Card(HDC dc, RECT rc){
    gpRoundRectBg(dc, rc, S(14), g_theme.surface, g_theme.border, g_theme.bg);
}

int Chip(HDC dc, int rightX, int y, const wchar_t* text,
         COLORREF fill, COLORREF textCol, bool selected){
    SelectScope sf(dc, (HGDIOBJ)g_fSmall);
    SIZE sz{0,0};
    GetTextExtentPoint32W(dc, text, (int)wcslen(text), &sz);
    int padX = S(12), h = S(24);
    int w = sz.cx + padX*2;
    RECT chip = { rightX - w, y, rightX, y + h };
    COLORREF bd = selected ? g_theme.accent : g_theme.border;
    COLORREF bg = selected ? blendColor(g_theme.surface, g_theme.accent, 16) : fill;
    gpRoundRectBg(dc, chip, h/2, bg, bd, g_theme.surface);
    int oldBk = SetBkMode(dc, TRANSPARENT);
    COLORREF oldCol = SetTextColor(dc, selected ? g_theme.accent : textCol);
    RECT tr = { chip.left, chip.top, chip.right, chip.bottom };
    DrawTextW(dc, text, -1, &tr,
              DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);
    SetTextColor(dc, oldCol);
    SetBkMode(dc, oldBk);
    return w;
}

void InputWell(HDC dc, RECT rc, bool focused){
    gpRoundRectBg(dc, rc, S(6), g_theme.inputBg,
                  focused ? g_theme.accent : g_theme.border, g_theme.surface);
}

// ---------------------------------------------- Persian text normalization ----
std::wstring NormalizeFa(const std::wstring& s){
    std::wstring out;
    out.reserve(s.size());
    for(wchar_t c : s){
        switch(c){
            case 0x064A: c = 0x06CC; break;   // ي -> ی  (Arabic yeh -> Persian)
            case 0x0643: c = 0x06A9; break;   // ك -> ک  (Arabic kaf -> Persian)
            case 0x0629: c = 0x0647; break;   // ة -> ه
            case 0x0640:                       // ـ tatweel -> drop
            case 0x200C:                       // ZWNJ -> drop
            case 0x200F: case 0x200E:          // RTL/LTR marks -> drop
                continue;
            default: break;
        }
        // Arabic-Indic digits 0x0660-0x0669 -> ASCII
        if(c >= 0x0660 && c <= 0x0669) c = L'0' + (c - 0x0660);
        // Persian digits 0x06F0-0x06F9 -> ASCII
        else if(c >= 0x06F0 && c <= 0x06F9) c = L'0' + (c - 0x06F0);
        // lower-case ASCII letters
        else if(c >= L'A' && c <= L'Z') c = (wchar_t)(c - L'A' + L'a');
        out.push_back(c);
    }
    // collapse runs of whitespace to a single space and trim
    std::wstring collapsed;
    collapsed.reserve(out.size());
    bool prevSpace = true;            // leading -> trim
    for(wchar_t c : out){
        bool sp = (c==L' '||c==L'\t'||c==L'\r'||c==L'\n');
        if(sp){ if(!prevSpace) collapsed.push_back(L' '); prevSpace = true; }
        else { collapsed.push_back(c); prevSpace = false; }
    }
    while(!collapsed.empty() && collapsed.back()==L' ') collapsed.pop_back();
    return collapsed;
}

} // namespace uikit
