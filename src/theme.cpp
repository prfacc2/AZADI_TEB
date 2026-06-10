// ============================================================================
//  theme.cpp — light/dark themes, owner-drawn flat buttons, vector icons
// ============================================================================
#include "app.h"

Theme   g_theme;
bool    g_dark = false;
HBRUSH  g_brBg=0, g_brSurface=0, g_brSurface2=0, g_brInput=0;

void applyTheme(bool dark){
    g_dark = dark;
    if(dark){
        // ---- Modern "midnight teal" dark palette (deep, low eye-strain) ----
        g_theme.bg          = RGB(13, 17, 23);    // near-black slate
        g_theme.surface     = RGB(22, 27, 34);    // card
        g_theme.surface2    = RGB(17, 22, 29);    // bars
        g_theme.border      = RGB(48, 54, 66);    // clearly visible separators
        g_theme.text        = RGB(230, 237, 243);
        g_theme.textDim     = RGB(139, 152, 168);
        g_theme.accent      = RGB(56, 170, 255);  // bright sky-blue (high contrast on dark)
        g_theme.accentHover = RGB(96, 190, 255);
        g_theme.accentText  = RGB(255, 255, 255);
        g_theme.danger      = RGB(240, 100, 100);
        g_theme.dangerHover = RGB(250, 128, 128);
        g_theme.success     = RGB(74, 210, 148);
        g_theme.inputBg     = RGB(28, 35, 45);    // distinctly lighter than card
        g_theme.inputText   = RGB(236, 242, 248);
        g_theme.hover       = RGB(33, 41, 53);
    } else {
        // ---- Clean modern light palette (soft blue-gray, indigo accent) ----
        g_theme.bg          = RGB(240, 243, 248); // page
        g_theme.surface     = RGB(255, 255, 255); // card
        g_theme.surface2    = RGB(249, 251, 253); // bars
        g_theme.border      = RGB(223, 229, 238);
        g_theme.text        = RGB(24, 32, 46);
        g_theme.textDim     = RGB(110, 122, 142);
        g_theme.accent      = RGB(37, 99, 235);   // indigo-blue
        g_theme.accentHover = RGB(59, 120, 246);
        g_theme.accentText  = RGB(255, 255, 255);
        g_theme.danger      = RGB(220, 53, 69);
        g_theme.dangerHover = RGB(235, 80, 95);
        g_theme.success     = RGB(22, 163, 110);
        g_theme.inputBg     = RGB(248, 250, 253);
        g_theme.inputText   = RGB(20, 28, 42);
        g_theme.hover       = RGB(237, 242, 250);
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
    HPEN pen  = CreatePen(PS_SOLID, thick, col);
    HGDIOBJ op = SelectObject(dc, pen);
    HGDIOBJ ob = SelectObject(dc, GetStockObject(NULL_BRUSH));
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
    }
    SelectObject(dc, op); SelectObject(dc, ob);
    DeleteObject(pen);
}

// ============================================================ flat button ==
struct BtnData {
    std::wstring text, sub;
    int icon, style;
    bool hover, down;
};
static LRESULT CALLBACK btnProc(HWND h, UINT m, WPARAM w, LPARAM l){
    BtnData* d = (BtnData*)GetWindowLongPtrW(h, GWLP_USERDATA);
    switch(m){
    case WM_NCCREATE: {
        CREATESTRUCTW* cs = (CREATESTRUCTW*)l;
        d = new BtnData();
        d->icon  = LOWORD((UINT_PTR)cs->lpCreateParams);
        d->style = HIWORD((UINT_PTR)cs->lpCreateParams);
        d->hover = d->down = false;
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

        // parent background behind rounded corners
        HWND par = GetParent(h);
        HBRUSH pb = (HBRUSH)SendMessageW(par, WM_CTLCOLORSTATIC,(WPARAM)dc,(LPARAM)h);
        if(!pb) pb = g_brBg;
        FillRect(dc,&rc,pb);

        COLORREF fill, txt, bord = CLR_INVALID;
        int st = d ? d->style : BS_GHOST;
        bool hv = d&&d->hover, dn = d&&d->down;
        switch(st){
        case BS_PRIMARY:
            fill = dn ? g_theme.accent : hv ? g_theme.accentHover : g_theme.accent;
            txt  = g_theme.accentText; break;
        case BS_DANGER:
            fill = dn||hv ? g_theme.dangerHover : g_theme.danger;
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
        fillRoundRect(dc, rr, S(st==BS_CARD?14:9), fill, bord);

        SetBkMode(dc, TRANSPARENT);
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
            int isz = S(17);
            if(d && d->icon && hasText){
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
