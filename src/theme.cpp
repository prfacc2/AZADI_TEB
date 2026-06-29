// ============================================================================
//  theme.cpp — light/dark themes, owner-drawn flat buttons, vector icons
// ============================================================================
#include "app.h"

Theme   g_theme;
bool    g_dark = false;
HBRUSH  g_brBg=0, g_brSurface=0, g_brSurface2=0, g_brInput=0;
//  v1.8.0: distinct non-red "attention" accent (violet) for change-requests.
COLORREF g_infoAccent  = RGB(124, 92, 230);
COLORREF g_infoAccent2 = RGB(98, 70, 210);

void applyTheme(bool dark){
    g_dark = dark;
    if(dark){
        // ---- True-black dark palette. The page background is (near) pure black
        //      as requested so card edges & button corners no longer glow white;
        //      surfaces step up in lightness only slightly so the UI has depth
        //      without bright halos. Labels/text are deliberately bright. ----
        g_theme.bg          = RGB(0, 0, 0);       // pure black page
        g_theme.bg2         = RGB(8, 10, 14);
        g_theme.surface     = RGB(20, 24, 30);    // card
        g_theme.surfaceTop  = RGB(28, 33, 41);    // card gradient top
        g_theme.surface2    = RGB(12, 14, 18);    // bars
        g_theme.border      = RGB(54, 62, 76);    // clearly visible separators
        g_theme.text        = RGB(238, 243, 249);
        g_theme.textDim     = RGB(170, 182, 198); // brighter dim text (legible)
        g_theme.accent      = RGB(56, 170, 255);  // bright sky-blue
        g_theme.accent2     = RGB(30, 120, 220);  // gradient end
        g_theme.accentHover = RGB(96, 190, 255);
        g_theme.accentText  = RGB(255, 255, 255);
        g_theme.danger      = RGB(240, 100, 100);
        g_theme.dangerHover = RGB(250, 128, 128);
        g_theme.success     = RGB(74, 210, 148);
        g_theme.warn        = RGB(245, 184, 84);
        g_theme.inputBg     = RGB(26, 31, 39);    // distinctly lighter than card
        g_theme.inputText   = RGB(240, 245, 250);
        g_theme.hover       = RGB(34, 40, 50);
        g_theme.headerTop   = RGB(14, 17, 22);
        g_theme.headerBot   = RGB(6, 8, 11);
        g_infoAccent  = RGB(150, 120, 245);   // bright violet (stands out, not red)
        g_infoAccent2 = RGB(120, 92, 225);
    } else {
        // ---- Premium light palette (v1.10.0 §C): a genuinely LAYERED light
        //      theme, not a washed-out white sheet. Five clearly distinct
        //      elevation tones — page → wells/bars → cards → card-top highlight
        //      → header — give real depth. Borders are crisp, text is high
        //      contrast (near-WCAG-AAA on white), the accent is a richer, more
        //      saturated indigo→sky, and hover/focus/active/disabled states are
        //      visually separated so every control reads clearly.
        //
        //  Elevation ladder (lightness):
        //      bg2  214 ── page bottom (deepest)
        //      bg   222 ── page
        //      surface2 234 ── wells / bars / list backgrounds
        //      border 196 ── crisp hairline between layers
        //      surface 255 ── cards (clean white, pops off the tinted page)
        //      surfaceTop 250 ── soft top-light on cards
        // ---- v1.19.0: premium "Azadi-Teb 2026" light palette — matches the
        //  reference reception design exactly.
        //  bg #F5F8FD · surface #FFFFFF · surface2 #EAF4FF · border #DCE6F2
        //  text #233042 · muted #6B7A90 · accent #1976F3 · hover #2D8CFF
        //  success #16C47F · warning #F4B740 · danger #E24C4B.
        g_theme.bg          = RGB(0xF5, 0xF8, 0xFD); // #F5F8FD app background
        g_theme.bg2         = RGB(0xE8, 0xF0, 0xFA); // page gradient bottom (subtle)
        g_theme.surface     = RGB(0xFF, 0xFF, 0xFF); // #FFFFFF cards / sheets
        g_theme.surfaceTop  = RGB(0xFF, 0xFF, 0xFF); // card gradient top (flat white)
        g_theme.surface2    = RGB(0xEA, 0xF4, 0xFF); // #EAF4FF secondary wash
        g_theme.border      = RGB(0xDC, 0xE6, 0xF2); // #DCE6F2 hairline
        g_theme.text        = RGB(0x23, 0x30, 0x42); // #233042 primary ink
        g_theme.textDim     = RGB(0x6B, 0x7A, 0x90); // #6B7A90 muted
        g_theme.accent      = RGB(0x19, 0x76, 0xF3); // #1976F3 primary actions
        g_theme.accent2     = RGB(0x2D, 0x8C, 0xFF); // #2D8CFF gradient end / hover
        g_theme.accentHover = RGB(0x2D, 0x8C, 0xFF); // #2D8CFF lighter accent on hover
        g_theme.accentText  = RGB(0xFF, 0xFF, 0xFF);
        g_theme.danger      = RGB(0xE2, 0x4C, 0x4B); // #E24C4B
        g_theme.dangerHover = RGB(0xEC, 0x66, 0x65);
        g_theme.success     = RGB(0x16, 0xC4, 0x7F); // #16C47F
        g_theme.warn        = RGB(0xF4, 0xB7, 0x40); // #F4B740 warning
        g_theme.inputBg     = RGB(0xF7, 0xFA, 0xFE); // tinted well (just above bg)
        g_theme.inputText   = RGB(0x23, 0x30, 0x42);
        g_theme.hover       = RGB(0xEA, 0xF4, 0xFF); // #EAF4FF soft accent wash on hover
        g_theme.headerTop   = RGB(0xFF, 0xFF, 0xFF);
        g_theme.headerBot   = RGB(0xEA, 0xF4, 0xFF); // header reads as its own layer
        g_infoAccent  = RGB(0x7C, 0x56, 0xE4);    // violet (distinct, non-red)
        g_infoAccent2 = RGB(0x5E, 0x42, 0xD0);
    }
    if(g_brBg)       DeleteObject(g_brBg);
    if(g_brSurface)  DeleteObject(g_brSurface);
    if(g_brSurface2) DeleteObject(g_brSurface2);
    if(g_brInput)    DeleteObject(g_brInput);
    g_brBg       = CreateSolidBrush(g_theme.bg);
    g_brSurface  = CreateSolidBrush(g_theme.surface);
    g_brSurface2 = CreateSolidBrush(g_theme.surface2);
    g_brInput    = CreateSolidBrush(g_theme.inputBg);
    setSetting(L"theme", dark ? L"dark" : L"light");
}
static BOOL CALLBACK invProc(HWND h, LPARAM){
    SendMessageW(h, WM_APP_THEME, 0, 0);     // let controls re-color themselves
    InvalidateRect(h, NULL, TRUE);
    EnumChildWindows(h, invProc, 0);
    return TRUE;
}
static BOOL CALLBACK topProc(HWND h, LPARAM){
    // also refresh OUR other top-level windows (calculator, detached tabs)
    DWORD pid=0; GetWindowThreadProcessId(h,&pid);
    if(pid==GetCurrentProcessId()){
        SendMessageW(h, WM_APP_THEME, 0, 0);
        InvalidateRect(h,NULL,TRUE);
        EnumChildWindows(h, invProc, 0);
    }
    return TRUE;
}
void broadcastThemeChange(){
    EnumWindows(topProc, 0);
}

// =================================================================== draw ==
void fillRoundRect(HDC dc, RECT rc, int rad, COLORREF fill, COLORREF border){
    HBRUSH br = CreateSolidBrush(fill);
    HPEN   pn = (border==CLR_INVALID) ? (HPEN)GetStockObject(NULL_PEN)
                                      : CreatePen(PS_SOLID, 1, border);
    HGDIOBJ ob = SelectObject(dc, br), op = SelectObject(dc, pn);
    RoundRect(dc, rc.left, rc.top, rc.right, rc.bottom, rad, rad);
    SelectObject(dc, ob); SelectObject(dc, op);
    DeleteObject(br);
    if(border!=CLR_INVALID) DeleteObject(pn);
}

//  Simple vector icons drawn with pens — crisp at any DPI, zero assets.
void drawIcon(HDC dc, int icon, RECT rc, COLORREF col, int thick){
    int cx=(rc.left+rc.right)/2, cy=(rc.top+rc.bottom)/2;
    int r = ((rc.right-rc.left) < (rc.bottom-rc.top) ? (rc.right-rc.left)
                                                     : (rc.bottom-rc.top)) / 2;
    // v1.9.0: a GEOMETRIC pen with ROUND end-caps + ROUND joins renders every
    // line-art glyph with smooth, professional terminations (no hard "childish"
    // corners). Falls back to a plain cosmetic pen if creation fails.
    LOGBRUSH lb={ BS_SOLID, col, 0 };
    HPEN pen = ExtCreatePen(PS_GEOMETRIC|PS_SOLID|PS_ENDCAP_ROUND|PS_JOIN_ROUND,
                            thick<1?1:thick, &lb, 0, NULL);
    if(!pen) pen = CreatePen(PS_SOLID, thick, col);
    HGDIOBJ op = SelectObject(dc, pen);
    HGDIOBJ ob = SelectObject(dc, GetStockObject(NULL_BRUSH));
    int oldBk = SetBkMode(dc, TRANSPARENT);
    switch(icon){
    case ICO_X: {
        int d=(r*60)/100;
        MoveToEx(dc,cx-d,cy-d,0); LineTo(dc,cx+d+1,cy+d+1);
        MoveToEx(dc,cx+d,cy-d,0); LineTo(dc,cx-d-1,cy+d+1);
        break; }
    case ICO_CALC: {
        int w=(r*70)/100, h=(r*88)/100;
        Rectangle(dc,cx-w,cy-h,cx+w,cy+h);
        MoveToEx(dc,cx-w,cy-h/3,0); LineTo(dc,cx+w,cy-h/3);
        int s=(w*45)/100;
        MoveToEx(dc,cx-s,cy+h/4,0); LineTo(dc,cx-s,cy+h/4);
        SetPixel(dc,cx-s,cy+h/4,col); SetPixel(dc,cx,cy+h/4,col); SetPixel(dc,cx+s,cy+h/4,col);
        SetPixel(dc,cx-s,cy+h/2+2,col); SetPixel(dc,cx,cy+h/2+2,col); SetPixel(dc,cx+s,cy+h/2+2,col);
        Ellipse(dc,cx-s-1,cy+h/4-1,cx-s+2,cy+h/4+2); Ellipse(dc,cx-1,cy+h/4-1,cx+2,cy+h/4+2);
        Ellipse(dc,cx+s-1,cy+h/4-1,cx+s+2,cy+h/4+2);
        break; }
    case ICO_PRINT: {
        int w=(r*75)/100;
        Rectangle(dc,cx-w,cy-r/4,cx+w,cy+r/2);                 // body
        Rectangle(dc,cx-w/2,cy-r+2,cx+w/2,cy-r/4);             // paper top
        Rectangle(dc,cx-w/2,cy+r/6,cx+w/2,cy+r-1);             // paper out
        break; }
    case ICO_UPDATE: {
        Arc(dc,cx-r+2,cy-r+2,cx+r-2,cy+r-2, cx+r,cy-r, cx-r,cy+r);
        POINT a={cx+(r*55)/100, cy-(r*78)/100};
        MoveToEx(dc,a.x-r/3,a.y,0); LineTo(dc,a.x+1,a.y+1);
        MoveToEx(dc,a.x,a.y-r/3,0); LineTo(dc,a.x+1,a.y+1);
        break; }
    case ICO_MOON: {
        Arc(dc,cx-r+2,cy-r+2,cx+r-2,cy+r-2, cx,cy-r, cx,cy+r);
        Arc(dc,cx-r/2,cy-r+2,cx+r,cy+r-2, cx,cy+r, cx,cy-r);
        break; }
    case ICO_SUN: {
        Ellipse(dc,cx-r/2,cy-r/2,cx+r/2,cy+r/2);
        for(int i=0;i<8;i++){
            double a=i*3.14159/4;
            int x1=cx+(int)((r*65/100)*cos(a)+0.5), y1=cy+(int)((r*65/100)*sin(a)+0.5);
            int x2=cx+(int)((r*95/100)*cos(a)+0.5), y2=cy+(int)((r*95/100)*sin(a)+0.5);
            MoveToEx(dc,x1,y1,0); LineTo(dc,x2,y2);
        }
        break; }
    case ICO_USER: {
        Ellipse(dc,cx-r/2,cy-r+1,cx+r/2,cy);                   // head
        Arc(dc,cx-r+1,cy+1,cx+r-1,cy+2*r, cx+r,cy+r, cx-r,cy+r); // shoulders
        break; }
    case ICO_SHIELD: {
        POINT p[6]={{cx,cy-r+1},{cx+r-2,cy-r/2},{cx+r-2,cy+r/4},
                    {cx,cy+r-1},{cx-r+2,cy+r/4},{cx-r+2,cy-r/2}};
        Polygon(dc,p,6);
        MoveToEx(dc,cx-r/3,cy,0); LineTo(dc,cx-r/8,cy+r/4); LineTo(dc,cx+r/3,cy-r/4);
        break; }
    case ICO_PLUS: {
        int d=(r*65)/100;
        MoveToEx(dc,cx-d,cy,0); LineTo(dc,cx+d+1,cy);
        MoveToEx(dc,cx,cy-d,0); LineTo(dc,cx,cy+d+1);
        break; }
    case ICO_LOGOUT: {
        Rectangle(dc,cx-r+2,cy-r+3,cx+r/4,cy+r-2);
        MoveToEx(dc,cx-r/4,cy,0); LineTo(dc,cx+r-1,cy);
        MoveToEx(dc,cx+r/2,cy-r/3,0); LineTo(dc,cx+r-1,cy);
        LineTo(dc,cx+r/2,cy+r/3);
        break; }
    case ICO_DETACH: {
        Rectangle(dc,cx-r+2,cy-r/4,cx+r/4,cy+r-2);
        MoveToEx(dc,cx-r/4,cy-r+2,0); LineTo(dc,cx+r-2,cy-r+2);
        LineTo(dc,cx+r-2,cy+r/4);
        break; }
    case ICO_CROSS_MED: {
        int a=(r*35)/100, b=(r*85)/100;
        POINT p[12]={{cx-a,cy-b},{cx+a,cy-b},{cx+a,cy-a},{cx+b,cy-a},
                     {cx+b,cy+a},{cx+a,cy+a},{cx+a,cy+b},{cx-a,cy+b},
                     {cx-a,cy+a},{cx-b,cy+a},{cx-b,cy-a},{cx-a,cy-a}};
        Polygon(dc,p,12);
        break; }
    case ICO_CHECK: {
        MoveToEx(dc,cx-r+3,cy,0); LineTo(dc,cx-r/4,cy+r/2); LineTo(dc,cx+r-2,cy-r/2);
        break; }
    case ICO_TRASH: {
        Rectangle(dc,cx-r/2,cy-r/3,cx+r/2,cy+r-2);
        MoveToEx(dc,cx-r+3,cy-r/3,0); LineTo(dc,cx+r-3,cy-r/3);
        MoveToEx(dc,cx-r/4,cy-r/3,0); LineTo(dc,cx-r/4,cy-(r*60)/100);
        LineTo(dc,cx+r/4,cy-(r*60)/100); LineTo(dc,cx+r/4,cy-r/3);
        break; }
    case ICO_SAVE: {
        Rectangle(dc,cx-r+2,cy-r+2,cx+r-2,cy+r-2);
        Rectangle(dc,cx-r/2,cy-r+2,cx+r/2,cy-r/4);
        Rectangle(dc,cx-r/2,cy+r/6,cx+r/2,cy+r-2);
        break; }
    case ICO_BACK: {
        MoveToEx(dc,cx-r+3,cy,0); LineTo(dc,cx+r-2,cy);
        MoveToEx(dc,cx-r/4,cy-r/2,0); LineTo(dc,cx-r+3,cy); LineTo(dc,cx-r/4,cy+r/2);
        break; }
    case ICO_ID: {   // ID card
        Rectangle(dc,cx-r+1,cy-r/2,cx+r-1,cy+r/2);
        Ellipse(dc,cx-r/2,cy-r/4,cx-r/8,cy+r/8);             // photo head
        MoveToEx(dc,cx,cy-r/5,0);  LineTo(dc,cx+r-r/3,cy-r/5);
        MoveToEx(dc,cx,cy+r/8,0);  LineTo(dc,cx+r-r/3,cy+r/8);
        break; }
    case ICO_PHONE: {  // phone handset
        int a=(r*70)/100;
        MoveToEx(dc,cx-a,cy-a,0);
        LineTo(dc,cx-a/3,cy-a/3); LineTo(dc,cx,cy);
        LineTo(dc,cx+a/3,cy+a/3); LineTo(dc,cx+a,cy+a);
        MoveToEx(dc,cx-a,cy-a,0); LineTo(dc,cx-a/2,cy-a-2);
        MoveToEx(dc,cx+a,cy+a,0); LineTo(dc,cx+a+2,cy+a/2);
        break; }
    case ICO_CAL: {   // calendar
        Rectangle(dc,cx-r+1,cy-r+3,cx+r-1,cy+r-1);
        MoveToEx(dc,cx-r+1,cy-r/3,0); LineTo(dc,cx+r-1,cy-r/3);
        MoveToEx(dc,cx-r/2,cy-r+3,0); LineTo(dc,cx-r/2,cy-r-1);
        MoveToEx(dc,cx+r/2,cy-r+3,0); LineTo(dc,cx+r/2,cy-r-1);
        break; }
    case ICO_PIN: {   // location pin
        Ellipse(dc,cx-r/2,cy-r+1,cx+r/2,cy);
        MoveToEx(dc,cx-r/2,cy-r/3,0); LineTo(dc,cx,cy+r-1); LineTo(dc,cx+r/2,cy-r/3);
        SetPixel(dc,cx,cy-r/2,col);
        break; }
    case ICO_RECEIPT: {  // receipt / invoice
        int w=(r*65)/100;
        MoveToEx(dc,cx-w,cy-r+2,0); LineTo(dc,cx+w,cy-r+2);
        LineTo(dc,cx+w,cy+r-2); LineTo(dc,cx+w-w/2,cy+r-r/3);
        LineTo(dc,cx,cy+r-2); LineTo(dc,cx-w+w/2,cy+r-r/3);
        LineTo(dc,cx-w,cy+r-2); LineTo(dc,cx-w,cy-r+2);
        MoveToEx(dc,cx-w/2,cy-r/3,0); LineTo(dc,cx+w/2,cy-r/3);
        MoveToEx(dc,cx-w/2,cy+r/8,0); LineTo(dc,cx+w/2,cy+r/8);
        break; }
    case ICO_CLOCK: {
        Ellipse(dc,cx-r+1,cy-r+1,cx+r-1,cy+r-1);
        MoveToEx(dc,cx,cy,0); LineTo(dc,cx,cy-r/2);
        MoveToEx(dc,cx,cy,0); LineTo(dc,cx+r/3,cy);
        break; }
    case ICO_REFRESH: {
        Arc(dc,cx-r+2,cy-r+2,cx+r-2,cy+r-2, cx-r,cy, cx,cy-r);
        Arc(dc,cx-r+2,cy-r+2,cx+r-2,cy+r-2, cx+r,cy, cx,cy+r);
        POINT a={cx, cy-r+1};
        MoveToEx(dc,a.x-r/3,a.y,0); LineTo(dc,a.x,a.y); LineTo(dc,a.x,a.y+r/3);
        break; }
    case ICO_GEAR: {
        // A clear, recognisable cog drawn as an OUTLINE (no hole-punch needed):
        //   • a toothed outer ring (single closed polygon with 8 teeth)
        //   • a small centre circle hole.
        // Drawn with the current pen so it inherits icon colour & thickness and
        // blends onto any background.
        const int N=8;
        double rOut=r*0.96, rIn=r*0.66;
        POINT poly[N*4]; int n=0;
        double tw=0.22;   // tooth angular half width (fraction of the gap)
        double half=(3.14159265/N);
        for(int i=0;i<N;i++){
            double a=i*2*3.14159265/N;
            double aA=a-half*(1.0-tw), aB=a-half*tw;
            double aC=a+half*tw,       aD=a+half*(1.0-tw);
            poly[n].x=cx+(int)(rIn *cos(aA)+0.5); poly[n].y=cy+(int)(rIn *sin(aA)+0.5); n++;
            poly[n].x=cx+(int)(rOut*cos(aB)+0.5); poly[n].y=cy+(int)(rOut*sin(aB)+0.5); n++;
            poly[n].x=cx+(int)(rOut*cos(aC)+0.5); poly[n].y=cy+(int)(rOut*sin(aC)+0.5); n++;
            poly[n].x=cx+(int)(rIn *cos(aD)+0.5); poly[n].y=cy+(int)(rIn *sin(aD)+0.5); n++;
        }
        Polygon(dc,poly,n);                       // toothed ring (outline)
        int rh=(r*34)/100;
        Ellipse(dc,cx-rh,cy-rh,cx+rh,cy+rh);      // centre hole
        break; }
    case ICO_BELL: {
        Arc(dc,cx-r+2,cy-r+2,cx+r-2,cy+r,  cx-r+2,cy+r/3, cx+r-2,cy+r/3);
        MoveToEx(dc,cx-r+2,cy+r/3,0); LineTo(dc,cx-r+2,cy-r/3);
        MoveToEx(dc,cx+r-2,cy+r/3,0); LineTo(dc,cx+r-2,cy-r/3);
        Arc(dc,cx-r+2,cy-r,cx+r-2,cy+r/2, cx+r-2,cy-r/3, cx-r+2,cy-r/3);
        MoveToEx(dc,cx-r/2,cy+r/3,0); LineTo(dc,cx+r/2,cy+r/3);
        MoveToEx(dc,cx-r/5,cy+r/2,0); LineTo(dc,cx+r/5,cy+r/2);
        break; }
    case ICO_TAB: {
        MoveToEx(dc,cx-r+2,cy+r-2,0);
        LineTo(dc,cx-r+2,cy-r/3); LineTo(dc,cx-r/4,cy-r/3);
        LineTo(dc,cx,cy-r+2); LineTo(dc,cx+r/4,cy-r/3);
        LineTo(dc,cx+r-2,cy-r/3); LineTo(dc,cx+r-2,cy+r-2);
        break; }
    case ICO_CHEVRON: {
        MoveToEx(dc,cx-r/2,cy-r/2,0); LineTo(dc,cx+r/3,cy); LineTo(dc,cx-r/2,cy+r/2);
        break; }
    case ICO_SAVED_MSG: {   // §F: bookmark / ribbon glyph
        int w=(r*60)/100, top=cy-r+2, bot=cy+r-2;
        MoveToEx(dc,cx-w,top,0);
        LineTo(dc,cx+w,top); LineTo(dc,cx+w,bot);
        LineTo(dc,cx,bot-r/3);                 // notch up
        LineTo(dc,cx-w,bot); LineTo(dc,cx-w,top);
        break; }
    case ICO_PALETTE: {     // §A: theme / palette (a swatch ring + dot)
        Ellipse(dc,cx-r+1,cy-r+1,cx+r-1,cy+r-1);
        int d=(r*22)/100;
        Ellipse(dc,cx-r/2-d,cy-r/3-d,cx-r/2+d,cy-r/3+d);
        Ellipse(dc,cx+r/3-d,cy-r/2-d,cx+r/3+d,cy-r/2+d);
        Ellipse(dc,cx+r/2-d,cy+r/4-d,cx+r/2+d,cy+r/4+d);
        break; }
    case ICO_INFO: {        // §A: about / info (circle + i)
        Ellipse(dc,cx-r+1,cy-r+1,cx+r-1,cy+r-1);
        SetPixel(dc,cx,cy-r/2,col);
        MoveToEx(dc,cx,cy-r/5,0); LineTo(dc,cx,cy+r/2);
        break; }
    case ICO_PEOPLE: {      // §A/§G: two-person group glyph
        int rr=(r*30)/100;
        Ellipse(dc,cx-r/2-rr,cy-r/3-rr,cx-r/2+rr,cy-r/3+rr);   // head 1
        Arc(dc,cx-r,cy,cx,cy+r, cx,cy, cx-r,cy);               // body 1
        Ellipse(dc,cx+r/3-rr,cy-r/4-rr,cx+r/3+rr,cy-r/4+rr);   // head 2
        Arc(dc,cx,cy+r/8,cx+r,cy+r, cx+r,cy+r/8, cx,cy+r/8);   // body 2
        break; }
    case ICO_WALLET: {      // v1.19.0: wallet / billfold glyph (rounded body +
        // a flap with a button stud — reads clearly at 18-28px sizes).
        int w=(r*86)/100, hh=(r*62)/100;
        // rounded wallet body
        RoundRect(dc,cx-w,cy-hh,cx+w,cy+hh, r/2, r/2);
        // flap on the right edge (RTL-neutral: a small pocket on one side)
        int fx=cx+w-(r*52)/100;
        MoveToEx(dc,fx,cy-hh,0); LineTo(dc,fx,cy+hh);
        // button stud on the flap
        int sr=(r*16)/100; if(sr<1) sr=1;
        Ellipse(dc,fx+(r*22)/100-sr,cy-sr,fx+(r*22)/100+sr,cy+sr);
        break; }
    }
    SetBkMode(dc, oldBk);
    SelectObject(dc, op); SelectObject(dc, ob);
    DeleteObject(pen);
}

// ============================================================ flat button ==
// v1.18.3 THEME-TOGGLE BUG FIX: previously BtnData::bg cached an ABSOLUTE
// COLORREF (e.g. the value of g_theme.surface at button-creation time). When
// the theme toggled (dark→light), applyTheme() changed g_theme.surface but the
// button still held the OLD raw colour — so the rounded-corner background stayed
// the previous theme's colour (the "black behind buttons" bug). The fix: store a
// SEMANTIC TOKEN identifying WHICH theme colour the button sits on, and resolve
// it to the LIVE g_theme value at paint time, so a theme switch is always
// reflected immediately with no per-button refresh bookkeeping.
enum BtnBgToken {
    BBG_PARENT = 0,   // ask parent (CLR_INVALID behaviour)
    BBG_BG,           // g_theme.bg
    BBG_BG2,          // g_theme.bg2
    BBG_SURFACE,      // g_theme.surface
    BBG_SURFACE2,     // g_theme.surface2
    BBG_HEADERTOP,    // g_theme.headerTop
    BBG_EXPLICIT      // a literal colour stored in `bg` (theme-independent)
};
struct BtnData {
    std::wstring text, sub;
    int icon, style;
    bool hover, down;
    COLORREF bg;     // literal colour (only used when bgToken==BBG_EXPLICIT)
    int      bgToken;// which live theme colour to paint behind the corners
    int imgIcon;     // RCDATA id of a raster icon (0 = use vector `icon`)
};
// resolve the live background colour for a button from its semantic token.
static COLORREF btnBgColor(const BtnData* d){
    if(!d) return CLR_INVALID;
    switch(d->bgToken){
        case BBG_BG:        return g_theme.bg;
        case BBG_BG2:       return g_theme.bg2;
        case BBG_SURFACE:   return g_theme.surface;
        case BBG_SURFACE2:  return g_theme.surface2;
        case BBG_HEADERTOP: return g_theme.headerTop;
        case BBG_EXPLICIT:  return d->bg;
        case BBG_PARENT: default: return CLR_INVALID;
    }
}
static LRESULT CALLBACK btnProc(HWND h, UINT m, WPARAM w, LPARAM l){
    BtnData* d = (BtnData*)GetWindowLongPtrW(h, GWLP_USERDATA);
    switch(m){
    case WM_NCCREATE: {
        CREATESTRUCTW* cs = (CREATESTRUCTW*)l;
        d = new BtnData();
        d->icon  = LOWORD((UINT_PTR)cs->lpCreateParams);
        d->style = HIWORD((UINT_PTR)cs->lpCreateParams);
        d->hover = d->down = false;
        d->bg    = CLR_INVALID;
        d->bgToken = BBG_PARENT;
        d->imgIcon = 0;
        if(cs->lpszName) d->text = cs->lpszName;
        SetWindowLongPtrW(h, GWLP_USERDATA, (LONG_PTR)d);
        return TRUE; }
    case WM_NCDESTROY: delete d; break;
    case WM_SETTEXT:
        if(d){ d->text = (const wchar_t*)l; InvalidateRect(h,NULL,TRUE); }
        return TRUE;
    case WM_MOUSEMOVE:
        if(d && !d->hover){
            d->hover = true; InvalidateRect(h,NULL,TRUE);
            TRACKMOUSEEVENT t={sizeof(t),TME_LEAVE,h,0}; TrackMouseEvent(&t);
        }
        break;
    case WM_MOUSELEAVE:
        if(d){ d->hover=false; d->down=false; InvalidateRect(h,NULL,TRUE); }
        break;
    case WM_SETFOCUS:
    case WM_KILLFOCUS:
    case WM_ENABLE:
        InvalidateRect(h,NULL,TRUE);   // §C: repaint focus ring / disabled state
        break;
    case WM_LBUTTONDOWN:
        if(d){ d->down=true; InvalidateRect(h,NULL,TRUE); SetCapture(h); }
        break;
    case WM_LBUTTONUP: {
        if(d && d->down){
            d->down=false; InvalidateRect(h,NULL,TRUE); ReleaseCapture();
            RECT rc; GetClientRect(h,&rc);
            POINT pt={GET_X_LPARAM(l),GET_Y_LPARAM(l)};
            if(PtInRect(&rc,pt)){
                // v1.1.0: POST (not send) the click. Some handlers destroy
                // this very button (close-tab, switch-screen, theme-toggle);
                // with SendMessage we'd return into freed window state.
                PostMessageW(GetParent(h), WM_COMMAND,
                    MAKEWPARAM(GetDlgCtrlID(h), BN_CLICKED), (LPARAM)h);
                return 0;
            }
        }
        break; }
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps; HDC dc0 = BeginPaint(h,&ps);
        RECT rc; GetClientRect(h,&rc);
        // double buffer
        HDC dc = CreateCompatibleDC(dc0);
        HBITMAP bmp = CreateCompatibleBitmap(dc0, rc.right, rc.bottom);
        HGDIOBJ obm = SelectObject(dc, bmp);

        // Background behind the rounded corners. v1.4.0: when the host told us
        // the exact colour it sits on (header gradient, surface2 bar, card,…)
        // we paint THAT solid colour so the antialiased corners blend perfectly
        // — this is the definitive fix for the "white corners in dark mode" bug.
        COLORREF liveBg = btnBgColor(d);   // resolves the LIVE theme colour
        if(d && liveBg!=CLR_INVALID){
            HBRUSH eb=CreateSolidBrush(liveBg);
            FillRect(dc,&rc,eb); DeleteObject(eb);
        } else {
            HWND par = GetParent(h);
            HBRUSH pb = (HBRUSH)SendMessageW(par, WM_CTLCOLORSTATIC,(WPARAM)dc,(LPARAM)h);
            if(!pb) pb = g_brBg;
            FillRect(dc,&rc,pb);
        }

        COLORREF fill, txt, bord = CLR_INVALID;
        int st = d ? d->style : BS_GHOST;
        bool enabled = IsWindowEnabled(h)!=FALSE;
        bool hv = enabled && d && d->hover, dn = enabled && d && d->down;
        bool focused = enabled && (GetFocus()==h);   // §C: explicit focus ring
        switch(st){
        case BS_PRIMARY:
            fill = dn ? g_theme.accent : hv ? g_theme.accentHover : g_theme.accent;
            txt  = g_theme.accentText; break;
        case BS_DANGER:
            fill = dn||hv ? g_theme.dangerHover : g_theme.danger;
            txt  = RGB(255,255,255); break;
        case BS_INFO:
            fill = dn||hv ? g_infoAccent2 : g_infoAccent;
            txt  = RGB(255,255,255); break;
        case BS_OUTLINE:
            fill = hv ? g_theme.hover : g_theme.surface;
            txt  = g_theme.text; bord = g_theme.border; break;
        case BS_CARD:
            fill = hv ? g_theme.hover : g_theme.surface;
            txt  = g_theme.text;
            bord = hv ? g_theme.accent : g_theme.border; break;
        default: // ghost
            fill = hv ? g_theme.hover : g_theme.surface2;
            txt  = hv ? g_theme.text : g_theme.textDim;
            if(d && d->icon==ICO_X && hv){ fill=g_theme.danger; txt=RGB(255,255,255); }
            break;
        }
        RECT rr = rc;
        if(dn){ rr.top+=1; rr.bottom+=1; }
        int rad = S(st==BS_CARD?16:10);
        // v1.3.0: anti-aliased GDI+ fills with a soft gradient on solid styles
        if(st==BS_PRIMARY){
            COLORREF a = dn ? g_theme.accent : (hv?g_theme.accentHover:g_theme.accent);
            COLORREF b = dn ? g_theme.accent2: (hv?g_theme.accent:g_theme.accent2);
            gpGradRoundRect(dc, rr, rad, a, b, CLR_INVALID);
        } else if(st==BS_DANGER){
            COLORREF a = (dn||hv)?g_theme.dangerHover:g_theme.danger;
            gpGradRoundRect(dc, rr, rad, a, g_theme.danger, CLR_INVALID);
        } else if(st==BS_INFO){
            COLORREF a = (dn||hv)?g_infoAccent2:g_infoAccent;
            gpGradRoundRect(dc, rr, rad, a, g_infoAccent2, CLR_INVALID);
        } else if(st==BS_CARD){
            gpGradRoundRect(dc, rr, rad, g_theme.surfaceTop,
                hv?g_theme.hover:g_theme.surface, bord);
        } else {
            gpRoundRect(dc, rr, rad, fill, bord);
        }
        // §C: explicit focus ring — a crisp accent hairline inset 2px so keyboard
        //     focus is always visible without shifting layout.
        if(focused){
            RECT fr=rr; InflateRect(&fr,-S(2),-S(2));
            gpRoundRect(dc, fr, rad>S(3)?rad-S(2):rad, CLR_INVALID, g_theme.accent);
        }

        SetBkMode(dc, TRANSPARENT);
        // §C: disabled controls render dim + low-contrast text so the state is
        //     unmistakable (and they ignore hover/active above).
        if(!enabled) txt = g_theme.textDim;
        SetTextColor(dc, txt);
        if(st==BS_CARD){
            // big card: icon centered upper, title, subtitle
            if(d && d->icon){
                int isz=S(34);
                RECT ir={rc.right/2-isz/2, rr.top+S(22), rc.right/2+isz/2, rr.top+S(22)+isz};
                drawIcon(dc, d->icon, ir, hv?g_theme.accent:g_theme.accent, S(2)+1);
            }
            SelectObject(dc, g_fTitle);
            RECT tr = rr; tr.top += S(64); tr.bottom = tr.top+S(34);
            DrawTextW(dc, d?d->text.c_str():L"", -1, &tr,
                DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
            if(d && !d->sub.empty()){
                SelectObject(dc, g_fSmall);
                SetTextColor(dc, g_theme.textDim);
                RECT sr = rr; sr.top += S(100); sr.bottom = sr.top+S(24);
                DrawTextW(dc, d->sub.c_str(), -1, &sr,
                    DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
            }
        } else {
            bool hasText = d && !d->text.empty();
            bool hasImg  = d && d->imgIcon;
            int isz = S(17);
            int iszImg = S(22);   // raster icons read better a touch larger
            if(hasImg && hasText){
                // raster icon flush-right, text to its left (RTL feel)
                RECT ir={rc.right-S(12)-iszImg, rc.bottom/2-iszImg/2,
                         rc.right-S(12), rc.bottom/2+iszImg/2};
                if(!gpDrawTintedImageRes(dc, d->imgIcon, ir, txt))
                    drawIcon(dc, d->icon, ir, txt, S(2));
                SelectObject(dc, g_fUI);
                RECT tr={S(10),0,rc.right-S(18)-iszImg,rc.bottom};
                DrawTextW(dc, d->text.c_str(), -1, &tr,
                    DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
            } else if(hasImg){
                RECT ir={rc.right/2-iszImg/2, rc.bottom/2-iszImg/2,
                         rc.right/2+iszImg/2, rc.bottom/2+iszImg/2};
                if(!gpDrawTintedImageRes(dc, d->imgIcon, ir, txt))
                    drawIcon(dc, d->icon, ir, txt, S(2));
            } else if(d && d->icon && hasText){
                // icon right, text left of it (RTL feel)
                RECT ir={rc.right-S(12)-isz, rc.bottom/2-isz/2,
                         rc.right-S(12), rc.bottom/2+isz/2};
                drawIcon(dc, d->icon, ir, txt, S(2));
                SelectObject(dc, g_fUI);
                RECT tr={S(10),0,rc.right-S(16)-isz,rc.bottom};
                DrawTextW(dc, d->text.c_str(), -1, &tr,
                    DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
            } else if(d && d->icon){
                RECT ir={rc.right/2-isz/2, rc.bottom/2-isz/2,
                         rc.right/2+isz/2, rc.bottom/2+isz/2};
                drawIcon(dc, d->icon, ir, txt, S(2));
            } else if(hasText){
                SelectObject(dc, g_fUI);
                DrawTextW(dc, d->text.c_str(), -1, &rc,
                    DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
            }
        }
        // §C: disabled veil — a translucent wash of the page colour over the
        //     whole control so disabled buttons are visually muted but still
        //     legible. Applied last, over both fill and content.
        if(!enabled){
            gpRoundRectBg(dc, rr, rad,
                blendColor(g_theme.bg, g_theme.surface2, 40), CLR_INVALID, g_theme.surface, 150);
        }
        BitBlt(dc0,0,0,rc.right,rc.bottom,dc,0,0,SRCCOPY);
        SelectObject(dc,obm); DeleteObject(bmp); DeleteDC(dc);
        EndPaint(h,&ps);
        return 0; }
    }
    return DefWindowProcW(h,m,w,l);
}
void registerFlatButton(){
    WNDCLASSW wc={0};
    wc.lpfnWndProc = btnProc;
    wc.hInstance   = g_hInst;
    wc.hCursor     = LoadCursor(NULL, IDC_HAND);
    wc.lpszClassName = L"AzFlatBtn";
    RegisterClassW(&wc);
}
HWND createFlatButton(HWND parent,int id,const wchar_t* text,int icon,int style,
                      int x,int y,int w,int h,const wchar_t* sub){
    HWND b = CreateWindowExW(0, L"AzFlatBtn", text, WS_CHILD|WS_VISIBLE,
        x,y,w,h, parent,(HMENU)(UINT_PTR)id, g_hInst,
        (LPVOID)(UINT_PTR)MAKELONG(icon,style));
    if(sub && b){
        BtnData* d=(BtnData*)GetWindowLongPtrW(b,GWLP_USERDATA);
        if(d) d->sub = sub;
    }
    return b;
}
//  v1.1.0: change a flat button's icon in place (e.g. moon↔sun on theme
//  toggle) instead of destroying & recreating it mid-click.
void setFlatButtonIcon(HWND btn, int icon){
    if(!btn || !IsWindow(btn)) return;
    BtnData* d=(BtnData*)GetWindowLongPtrW(btn,GWLP_USERDATA);
    if(d){ d->icon = icon; InvalidateRect(btn,NULL,TRUE); }
}
void setFlatButtonBg(HWND btn, COLORREF bg){
    if(!btn || !IsWindow(btn)) return;
    BtnData* d=(BtnData*)GetWindowLongPtrW(btn,GWLP_USERDATA);
    if(!d) return;
    // v1.18.3: map the caller's colour to a SEMANTIC theme token where it
    // matches a live g_theme colour, so a later theme toggle re-resolves it
    // automatically (fixes the "black behind buttons after dark→light" bug).
    // Any colour that is not a recognised theme slot is kept as an explicit
    // literal (theme-independent, e.g. a one-off brand colour).
    if(bg==CLR_INVALID)              d->bgToken = BBG_PARENT;
    else if(bg==g_theme.bg)          d->bgToken = BBG_BG;
    else if(bg==g_theme.bg2)         d->bgToken = BBG_BG2;
    else if(bg==g_theme.surface)     d->bgToken = BBG_SURFACE;
    else if(bg==g_theme.surface2)    d->bgToken = BBG_SURFACE2;
    else if(bg==g_theme.headerTop)   d->bgToken = BBG_HEADERTOP;
    else { d->bgToken = BBG_EXPLICIT; d->bg = bg; }
    InvalidateRect(btn,NULL,TRUE);
}
//  v1.4.1: give a flat button a real raster icon (RCDATA id). Pass 0 to clear
//  and fall back to the vector icon.
void setFlatButtonImage(HWND btn, int resId){
    if(!btn || !IsWindow(btn)) return;
    BtnData* d=(BtnData*)GetWindowLongPtrW(btn,GWLP_USERDATA);
    if(d){ d->imgIcon = resId; InvalidateRect(btn,NULL,TRUE); }
}

// ============================================================================
//  Themed owner-draw combobox (v1.6.0)
//  CBS_DROPDOWNLIST combos painted their dropdown LIST with the system colours
//  (white bg / black text) which is unreadable in dark mode. Creating the combo
//  with CBS_OWNERDRAWFIXED|CBS_HASSTRINGS and forwarding WM_DRAWITEM here paints
//  every row with the theme palette and RTL-aligns Persian text.
// ============================================================================
// v1.8.0: combo boxes must have NO visible border (redesign brief). We subclass
// the combo and, after the default non-client paint, overpaint the entire
// non-client FRAME with the theme input-well colour so the system's chunky 3-D
// edge / white box disappears completely and the control reads as a clean,
// borderless, integrated input that matches the wells around it.
static WNDPROC s_comboOldProc = NULL;
static LRESULT CALLBACK themedComboProc(HWND h, UINT m, WPARAM w, LPARAM l){
    LRESULT r = CallWindowProcW(s_comboOldProc, h, m, w, l);
    if(m==WM_PAINT || m==WM_NCPAINT){
        HDC dc=GetWindowDC(h);
        if(dc){
            RECT wr; GetWindowRect(h,&wr);
            RECT br={0,0,wr.right-wr.left,wr.bottom-wr.top};
            // paint ONLY the 2-px frame ring with the input colour (no border):
            // top, bottom, left, right strips — the interior is owner-drawn.
            HBRUSH eb=CreateSolidBrush(g_theme.inputBg);
            RECT t={br.left,br.top,br.right,br.top+2};
            RECT b={br.left,br.bottom-2,br.right,br.bottom};
            RECT lft={br.left,br.top,br.left+2,br.bottom};
            RECT rt={br.right-2,br.top,br.right,br.bottom};
            FillRect(dc,&t,eb); FillRect(dc,&b,eb);
            FillRect(dc,&lft,eb); FillRect(dc,&rt,eb);
            DeleteObject(eb);
            ReleaseDC(h,dc);
        }
    }
    return r;
}
HWND createThemedCombo(HWND parent, int id){
    HWND c = CreateWindowExW(0, L"COMBOBOX", L"",
        WS_CHILD|WS_VISIBLE|WS_TABSTOP|WS_VSCROLL|
        CBS_DROPDOWNLIST|CBS_OWNERDRAWFIXED|CBS_HASSTRINGS,
        0,0,10,10, parent,(HMENU)(UINT_PTR)id, g_hInst,0);
    SendMessageW(c,WM_SETFONT,(WPARAM)g_fUI,TRUE);
    WNDPROC old=(WNDPROC)SetWindowLongPtrW(c,GWLP_WNDPROC,(LONG_PTR)themedComboProc);
    if(!s_comboOldProc) s_comboOldProc=old;
    return c;
}
static bool comboHasPersian(const wchar_t* s){
    for(const wchar_t* p=s; *p; ++p){
        wchar_t c=*p;
        if(c>=0x06F0&&c<=0x06F9) continue;
        if(c>=0x0660&&c<=0x0669) continue;
        if((c>=0x0600&&c<=0x06FF)||(c>=0xFB50&&c<=0xFDFF)||(c>=0xFE70&&c<=0xFEFF))
            return true;
    }
    return false;
}
//  Call from the parent's WM_DRAWITEM. Returns true if it handled a combo item.
bool drawThemedComboItem(LPDRAWITEMSTRUCT dis){
    if(!dis) return false;
    if(dis->CtlType!=ODT_COMBOBOX) return false;
    HDC dc=dis->hDC;
    RECT rc=dis->rcItem;
    bool selected = (dis->itemState & ODS_SELECTED)!=0;
    COLORREF bg = selected ? g_theme.accent : g_theme.inputBg;
    COLORREF fg = selected ? g_theme.accentText : g_theme.inputText;
    HBRUSH br=CreateSolidBrush(bg); FillRect(dc,&rc,br); DeleteObject(br);
    // itemID == -1 is the COLLAPSED SELECTION FIELD (the always-visible part of
    // the combo). We must paint its text too, otherwise the chosen value would
    // be invisible. Pull it from the current selection.
    wchar_t buf[256]={0};
    int item=(int)dis->itemID;
    if(item<0) item=(int)SendMessageW(dis->hwndItem,CB_GETCURSEL,0,0);
    if(item>=0){
        SendMessageW(dis->hwndItem,CB_GETLBTEXT,item,(LPARAM)buf);
        SetBkMode(dc,TRANSPARENT);
        SetTextColor(dc,fg);
        HGDIOBJ of=SelectObject(dc,g_fUI);
        // leave room on the LEFT for the dropdown arrow when this is the
        // collapsed field (RTL: arrow sits on the left, text on the right).
        RECT tr=rc; tr.right-=6; tr.left+=((int)dis->itemID<0?S(22):6);
        UINT flags=DT_SINGLELINE|DT_VCENTER|DT_NOPREFIX;
        if(comboHasPersian(buf)) flags|=DT_RIGHT|DT_RTLREADING; else flags|=DT_LEFT;
        DrawTextW(dc,buf,-1,&tr,flags);
        SelectObject(dc,of);
    }
    // v1.7.0: draw a flat, theme-coloured dropdown arrow on the collapsed
    // field so dark mode no longer shows the system's thick white arrow box.
    if((int)dis->itemID<0){
        int cx=rc.left+S(12), cy=(rc.top+rc.bottom)/2;
        POINT tri[3]={ {cx-S(5),cy-S(2)}, {cx+S(5),cy-S(2)}, {cx,cy+S(4)} };
        HBRUSH ab=CreateSolidBrush(g_theme.textDim);
        HPEN   ap=CreatePen(PS_SOLID,1,g_theme.textDim);
        HGDIOBJ ob=SelectObject(dc,ab), op=SelectObject(dc,ap);
        Polygon(dc,tri,3);
        SelectObject(dc,ob); SelectObject(dc,op);
        DeleteObject(ab); DeleteObject(ap);
    }
    if((dis->itemState & ODS_FOCUS) && (int)dis->itemID>=0) DrawFocusRect(dc,&rc);
    return true;
}
