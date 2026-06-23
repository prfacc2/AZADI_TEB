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

// ============================================================================
//  RELEASE 1.4.0 — new controls + handlers (§10, §6.2)
// ============================================================================
static const wchar_t* CLS_SWITCH = L"AzSwitch";
static const wchar_t* CLS_SPIN   = L"AzNumberSpinner";
static const wchar_t* CLS_COLOR  = L"AzColorPicker";
static const wchar_t* CLS_DROP   = L"AzDropZone";

// ----------------------------------------------------------- AzSwitch -------
struct SwState { bool on; bool hot; };
static LRESULT CALLBACK SwitchProc(HWND h, UINT m, WPARAM w, LPARAM l){
    SwState* st = (SwState*)GetWindowLongPtrW(h, GWLP_USERDATA);
    switch(m){
    case WM_NCCREATE: {
        SwState* s = new SwState(); s->on=false; s->hot=false;
        SetWindowLongPtrW(h, GWLP_USERDATA, (LONG_PTR)s);
        return TRUE;
    }
    case WM_LBUTTONUP:
        if(st){ st->on=!st->on; InvalidateRect(h,NULL,FALSE);
            HWND p=GetParent(h);
            if(p) SendMessageW(p, WM_COMMAND,
                MAKEWPARAM(GetDlgCtrlID(h), BN_CLICKED), (LPARAM)h); }
        return 0;
    case WM_MOUSEMOVE:
        if(st && !st->hot){ st->hot=true; InvalidateRect(h,NULL,FALSE);
            TRACKMOUSEEVENT t{sizeof(t),TME_LEAVE,h,0}; TrackMouseEvent(&t); }
        return 0;
    case WM_MOUSELEAVE:
        if(st){ st->hot=false; InvalidateRect(h,NULL,FALSE); } return 0;
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps; HDC dc0=BeginPaint(h,&ps);
        RECT rc; GetClientRect(h,&rc);
        MemDC mem(dc0, rc.right, rc.bottom);
        HBRUSH bg=CreateSolidBrush(g_theme.surface);
        FillRect(mem.dc,&rc,bg); DeleteObject(bg);
        bool on = st && st->on;
        COLORREF track = on ? g_theme.accent
                            : blendColor(g_theme.border, g_theme.surface, 30);
        RECT tr=rc; int pad=S(2); InflateRect(&tr,-pad,-pad);
        gpRoundRectBg(mem.dc, tr, (tr.bottom-tr.top)/2, track,
                      g_theme.border, g_theme.surface);
        int d = (tr.bottom-tr.top) - S(4);
        int ky = tr.top + S(2);
        int kx = on ? (tr.right - d - S(2)) : (tr.left + S(2));
        RECT kr={kx,ky,kx+d,ky+d};
        gpRoundRectBg(mem.dc, kr, d/2, RGB(255,255,255), g_theme.border, track);
        mem.blitTo(dc0);
        EndPaint(h,&ps);
        return 0;
    }
    case WM_DESTROY: if(st){ delete st; SetWindowLongPtrW(h,GWLP_USERDATA,0);} return 0;
    }
    return DefWindowProcW(h,m,w,l);
}
HWND AzSwitch_Create(HWND parent,int id,bool on,int x,int y,int w,int h){
    HWND s=CreateWindowExW(0,CLS_SWITCH,L"",WS_CHILD|WS_VISIBLE,
        x,y,w,h,parent,(HMENU)(INT_PTR)id,g_hInst,NULL);
    if(s) AzSwitch_Set(s,on);
    return s;
}
bool AzSwitch_Get(HWND sw){ SwState* s=(SwState*)GetWindowLongPtrW(sw,GWLP_USERDATA); return s&&s->on; }
void AzSwitch_Set(HWND sw,bool on){ SwState* s=(SwState*)GetWindowLongPtrW(sw,GWLP_USERDATA);
    if(s){ s->on=on; InvalidateRect(sw,NULL,FALSE);} }

// ------------------------------------------------------ AzNumberSpinner -----
struct SpState { double val,mn,mx,step; HWND edit; bool guard; };
static LRESULT CALLBACK SpinProc(HWND h, UINT m, WPARAM w, LPARAM l){
    SpState* st=(SpState*)GetWindowLongPtrW(h,GWLP_USERDATA);
    switch(m){
    case WM_NCCREATE:{ SpState* s=new SpState(); s->val=0;s->mn=0;s->mx=1000;
        s->step=1;s->edit=NULL;s->guard=false;
        SetWindowLongPtrW(h,GWLP_USERDATA,(LONG_PTR)s); return TRUE; }
    case WM_SIZE:{
        if(st && st->edit){
            RECT rc; GetClientRect(h,&rc);
            int bw=S(18);
            MoveWindow(st->edit, bw, 0, rc.right-bw, rc.bottom, TRUE);
        }
        return 0;
    }
    case WM_COMMAND:
        if(st && (HWND)l==st->edit && HIWORD(w)==EN_CHANGE && !st->guard){
            wchar_t b[64]={0}; GetWindowTextW(st->edit,b,63);
            st->val=_wtof(b);
            HWND p=GetParent(h);
            if(p) SendMessageW(p,WM_COMMAND,MAKEWPARAM(GetDlgCtrlID(h),EN_CHANGE),(LPARAM)h);
        }
        return 0;
    case WM_LBUTTONDOWN:{
        if(!st) break;
        RECT rc; GetClientRect(h,&rc);
        int bw=S(18); int my=GET_Y_LPARAM(l); int mx=GET_X_LPARAM(l);
        if(mx<bw){
            if(my < rc.bottom/2) st->val += st->step; else st->val -= st->step;
            if(st->val<st->mn) st->val=st->mn;
            if(st->val>st->mx) st->val=st->mx;
            AzNumberSpinner_Set(h, st->val);
            HWND p=GetParent(h);
            if(p) SendMessageW(p,WM_COMMAND,MAKEWPARAM(GetDlgCtrlID(h),EN_CHANGE),(LPARAM)h);
            InvalidateRect(h,NULL,FALSE);
        }
        return 0;
    }
    case WM_ERASEBKGND: return 1;
    case WM_PAINT:{
        PAINTSTRUCT ps; HDC dc=BeginPaint(h,&ps);
        RECT rc; GetClientRect(h,&rc);
        HBRUSH bg=CreateSolidBrush(g_theme.inputBg); FillRect(dc,&rc,bg); DeleteObject(bg);
        int bw=S(18);
        RECT btn={0,0,bw,rc.bottom};
        HBRUSH bb=CreateSolidBrush(blendColor(g_theme.surface,g_theme.accent,10));
        FillRect(dc,&btn,bb); DeleteObject(bb);
        HPEN pen=CreatePen(PS_SOLID,1,g_theme.border);
        HGDIOBJ op=SelectObject(dc,pen);
        MoveToEx(dc,bw,0,NULL); LineTo(dc,bw,rc.bottom);
        MoveToEx(dc,0,rc.bottom/2,NULL); LineTo(dc,bw,rc.bottom/2);
        SelectObject(dc,op); DeleteObject(pen);
        SetBkMode(dc,TRANSPARENT); SetTextColor(dc,g_theme.text);
        RECT up={0,0,bw,rc.bottom/2}, dn={0,rc.bottom/2,bw,rc.bottom};
        DrawTextW(dc,L"+",-1,&up,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
        DrawTextW(dc,L"\u2212",-1,&dn,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
        EndPaint(h,&ps);
        return 0;
    }
    case WM_DESTROY: if(st){ delete st; SetWindowLongPtrW(h,GWLP_USERDATA,0);} return 0;
    }
    return DefWindowProcW(h,m,w,l);
}
HWND AzNumberSpinner_Create(HWND parent,int id,double val,double mn,double mx,
                            double step,int x,int y,int w,int h){
    HWND sp=CreateWindowExW(0,CLS_SPIN,L"",WS_CHILD|WS_VISIBLE|WS_CLIPCHILDREN,
        x,y,w,h,parent,(HMENU)(INT_PTR)id,g_hInst,NULL);
    if(!sp) return NULL;
    SpState* st=(SpState*)GetWindowLongPtrW(sp,GWLP_USERDATA);
    if(st){
        st->mn=mn; st->mx=mx; st->step=step; st->val=val;
        int bw=S(18); RECT rc; GetClientRect(sp,&rc);
        st->edit=CreateWindowExW(0,L"EDIT",L"",
            WS_CHILD|WS_VISIBLE|ES_RIGHT|ES_AUTOHSCROLL,
            bw,0,rc.right-bw,rc.bottom,sp,(HMENU)1,g_hInst,NULL);
        SendMessageW(st->edit,WM_SETFONT,(WPARAM)g_fUI,TRUE);
        AzNumberSpinner_Set(sp,val);
    }
    return sp;
}
double AzNumberSpinner_Get(HWND sp){ SpState* st=(SpState*)GetWindowLongPtrW(sp,GWLP_USERDATA); return st?st->val:0; }
void AzNumberSpinner_Set(HWND sp,double v){
    SpState* st=(SpState*)GetWindowLongPtrW(sp,GWLP_USERDATA); if(!st) return;
    st->val=v; if(st->edit){ st->guard=true;
        wchar_t b[64]; if(v==(long long)v) swprintf(b,64,L"%lld",(long long)v);
        else swprintf(b,64,L"%.2f",v);
        SetWindowTextW(st->edit,b); st->guard=false; }
}

// -------------------------------------------------------- AzColorPicker -----
struct CpState { COLORREF c; };
static LRESULT CALLBACK ColorProc(HWND h, UINT m, WPARAM w, LPARAM l){
    CpState* st=(CpState*)GetWindowLongPtrW(h,GWLP_USERDATA);
    switch(m){
    case WM_NCCREATE:{ CpState* s=new CpState(); s->c=RGB(0,0,0);
        SetWindowLongPtrW(h,GWLP_USERDATA,(LONG_PTR)s); return TRUE; }
    case WM_LBUTTONUP:{
        if(!st) break;
        static COLORREF custom[16]; CHOOSECOLORW cc; ZeroMemory(&cc,sizeof(cc));
        cc.lStructSize=sizeof(cc); cc.hwndOwner=GetParent(h);
        cc.rgbResult=st->c; cc.lpCustColors=custom;
        cc.Flags=CC_FULLOPEN|CC_RGBINIT|CC_ANYCOLOR;
        if(ChooseColorW(&cc)){ st->c=cc.rgbResult; InvalidateRect(h,NULL,FALSE);
            HWND p=GetParent(h);
            if(p) SendMessageW(p,WM_COMMAND,MAKEWPARAM(GetDlgCtrlID(h),BN_CLICKED),(LPARAM)h); }
        return 0;
    }
    case WM_ERASEBKGND: return 1;
    case WM_PAINT:{
        PAINTSTRUCT ps; HDC dc=BeginPaint(h,&ps);
        RECT rc; GetClientRect(h,&rc);
        HBRUSH bg=CreateSolidBrush(g_theme.surface); FillRect(dc,&rc,bg); DeleteObject(bg);
        gpRoundRectBg(dc,rc,S(6), st?st->c:RGB(0,0,0), g_theme.border, g_theme.surface);
        EndPaint(h,&ps);
        return 0;
    }
    case WM_DESTROY: if(st){ delete st; SetWindowLongPtrW(h,GWLP_USERDATA,0);} return 0;
    }
    return DefWindowProcW(h,m,w,l);
}
HWND AzColorPicker_Create(HWND parent,int id,COLORREF c,int x,int y,int w,int h){
    HWND cp=CreateWindowExW(0,CLS_COLOR,L"",WS_CHILD|WS_VISIBLE,
        x,y,w,h,parent,(HMENU)(INT_PTR)id,g_hInst,NULL);
    if(cp) AzColorPicker_Set(cp,c);
    return cp;
}
COLORREF AzColorPicker_Get(HWND cp){ CpState* s=(CpState*)GetWindowLongPtrW(cp,GWLP_USERDATA); return s?s->c:RGB(0,0,0); }
void AzColorPicker_Set(HWND cp,COLORREF c){ CpState* s=(CpState*)GetWindowLongPtrW(cp,GWLP_USERDATA); if(s){ s->c=c; InvalidateRect(cp,NULL,FALSE);} }

// ------------------------------------------------------------ AzDropZone ----
struct DzState { std::wstring caption, filter; bool hot; };
static LRESULT CALLBACK DropProc(HWND h, UINT m, WPARAM w, LPARAM l){
    DzState* st=(DzState*)GetWindowLongPtrW(h,GWLP_USERDATA);
    switch(m){
    case WM_NCCREATE:{ DzState* s=new DzState(); s->hot=false;
        SetWindowLongPtrW(h,GWLP_USERDATA,(LONG_PTR)s);
        DragAcceptFiles(h,TRUE); return TRUE; }
    case WM_DROPFILES:{
        HDROP hd=(HDROP)w; wchar_t path[MAX_PATH]={0};
        if(DragQueryFileW(hd,0,path,MAX_PATH)){
            std::wstring* p=new std::wstring(path);
            HWND par=GetParent(h);
            if(par) PostMessageW(par,AZ_DROPZONE_DROPPED,
                (WPARAM)GetDlgCtrlID(h),(LPARAM)p);
            else delete p;
        }
        DragFinish(hd);
        return 0;
    }
    case WM_LBUTTONUP:{
        if(!st||st->filter.empty()) return 0;
        wchar_t file[MAX_PATH]={0};
        OPENFILENAMEW ofn; ZeroMemory(&ofn,sizeof(ofn));
        ofn.lStructSize=sizeof(ofn); ofn.hwndOwner=GetParent(h);
        ofn.lpstrFilter=st->filter.c_str(); ofn.lpstrFile=file;
        ofn.nMaxFile=MAX_PATH; ofn.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST;
        if(GetOpenFileNameW(&ofn)){
            std::wstring* p=new std::wstring(file);
            HWND par=GetParent(h);
            if(par) PostMessageW(par,AZ_DROPZONE_DROPPED,
                (WPARAM)GetDlgCtrlID(h),(LPARAM)p); else delete p;
        }
        return 0;
    }
    case WM_ERASEBKGND: return 1;
    case WM_PAINT:{
        PAINTSTRUCT ps; HDC dc0=BeginPaint(h,&ps);
        RECT rc; GetClientRect(h,&rc);
        MemDC mem(dc0,rc.right,rc.bottom);
        HBRUSH bg=CreateSolidBrush(g_theme.surface); FillRect(mem.dc,&rc,bg); DeleteObject(bg);
        RECT in=rc; InflateRect(&in,-S(4),-S(4));
        HPEN pen=CreatePen(PS_DASH,1, st&&st->hot?g_theme.accent:g_theme.border);
        HBRUSH hol=(HBRUSH)GetStockObject(NULL_BRUSH);
        HGDIOBJ op=SelectObject(mem.dc,pen), ob=SelectObject(mem.dc,hol);
        RoundRect(mem.dc,in.left,in.top,in.right,in.bottom,S(10),S(10));
        SelectObject(mem.dc,op); SelectObject(mem.dc,ob); DeleteObject(pen);
        SetBkMode(mem.dc,TRANSPARENT); SetTextColor(mem.dc,g_theme.textDim);
        HGDIOBJ of=SelectObject(mem.dc,g_fUI);
        DrawTextW(mem.dc, st?st->caption.c_str():L"",-1,&rc,
            DT_CENTER|DT_VCENTER|DT_WORDBREAK|DT_RTLREADING);
        SelectObject(mem.dc,of);
        mem.blitTo(dc0);
        EndPaint(h,&ps);
        return 0;
    }
    case WM_DESTROY: if(st){ delete st; SetWindowLongPtrW(h,GWLP_USERDATA,0);} return 0;
    }
    return DefWindowProcW(h,m,w,l);
}
HWND AzDropZone_Create(HWND parent,int id,const wchar_t* caption,
                       const wchar_t* filter,int x,int y,int w,int h){
    HWND d=CreateWindowExW(WS_EX_ACCEPTFILES,CLS_DROP,L"",WS_CHILD|WS_VISIBLE,
        x,y,w,h,parent,(HMENU)(INT_PTR)id,g_hInst,NULL);
    if(d){ DzState* st=(DzState*)GetWindowLongPtrW(d,GWLP_USERDATA);
        if(st){ st->caption=caption?caption:L""; st->filter=filter?filter:L""; } }
    return d;
}

// -------------------------------------------------- designer painters -------
void AzGridLayer_Paint(HDC dc, RECT area, double pxPerMm, COLORREF line){
    if(pxPerMm<=0) return;
    double stepPx = 5.0 * pxPerMm;        // 5 mm grid
    if(stepPx < 3) return;
    HPEN pen=CreatePen(PS_SOLID,1,line);
    HGDIOBJ op=SelectObject(dc,pen);
    for(double x=area.left; x<=area.right; x+=stepPx){
        MoveToEx(dc,(int)(x+0.5),area.top,NULL); LineTo(dc,(int)(x+0.5),area.bottom);
    }
    for(double y=area.top; y<=area.bottom; y+=stepPx){
        MoveToEx(dc,area.left,(int)(y+0.5),NULL); LineTo(dc,area.right,(int)(y+0.5));
    }
    SelectObject(dc,op); DeleteObject(pen);
}
void AzRulerH_Paint(HDC dc, RECT area, double originPx, double pxPerMm){
    HBRUSH bg=CreateSolidBrush(g_theme.surface2); FillRect(dc,&area,bg); DeleteObject(bg);
    HPEN pen=CreatePen(PS_SOLID,1,g_theme.border); HGDIOBJ op=SelectObject(dc,pen);
    SetBkMode(dc,TRANSPARENT); SetTextColor(dc,g_theme.textDim);
    HGDIOBJ of=SelectObject(dc,g_fSmall);
    for(int mm=0; ; mm+=10){
        double x = originPx + mm*pxPerMm;
        if(x>area.right) break;
        if(x<area.left) continue;
        MoveToEx(dc,(int)x,area.bottom-S(6),NULL); LineTo(dc,(int)x,area.bottom);
        wchar_t b[16]; swprintf(b,16,L"%d",mm);
        TextOutW(dc,(int)x+2,area.top+1,b,(int)wcslen(b));
    }
    SelectObject(dc,of); SelectObject(dc,op); DeleteObject(pen);
}
void AzRulerV_Paint(HDC dc, RECT area, double originPx, double pxPerMm){
    HBRUSH bg=CreateSolidBrush(g_theme.surface2); FillRect(dc,&area,bg); DeleteObject(bg);
    HPEN pen=CreatePen(PS_SOLID,1,g_theme.border); HGDIOBJ op=SelectObject(dc,pen);
    SetBkMode(dc,TRANSPARENT); SetTextColor(dc,g_theme.textDim);
    HGDIOBJ of=SelectObject(dc,g_fSmall);
    for(int mm=0; ; mm+=10){
        double y = originPx + mm*pxPerMm;
        if(y>area.bottom) break;
        if(y<area.top) continue;
        MoveToEx(dc,area.right-S(6),(int)y,NULL); LineTo(dc,area.right,(int)y);
        wchar_t b[16]; swprintf(b,16,L"%d",mm);
        TextOutW(dc,area.left+1,(int)y+1,b,(int)wcslen(b));
    }
    SelectObject(dc,of); SelectObject(dc,op); DeleteObject(pen);
}
void AzHandle_Paint(HDC dc, RECT sel){
    int hs=S(4);
    HPEN pen=CreatePen(PS_SOLID,1, RGB(0,120,255));
    HBRUSH br=CreateSolidBrush(RGB(255,255,255));
    HGDIOBJ op=SelectObject(dc,pen), ob=SelectObject(dc,br);
    int xs[3]={sel.left,(sel.left+sel.right)/2,sel.right};
    int ys[3]={sel.top,(sel.top+sel.bottom)/2,sel.bottom};
    for(int i=0;i<3;i++) for(int j=0;j<3;j++){
        if(i==1&&j==1) continue;
        Rectangle(dc, xs[i]-hs, ys[j]-hs, xs[i]+hs, ys[j]+hs);
    }
    // rotate handle on top center
    int cx=(sel.left+sel.right)/2, ry=sel.top-S(18);
    MoveToEx(dc,cx,sel.top,NULL); LineTo(dc,cx,ry);
    Ellipse(dc,cx-hs,ry-hs,cx+hs,ry+hs);
    SelectObject(dc,op); SelectObject(dc,ob); DeleteObject(pen); DeleteObject(br);
}

// ============================================================================
//  AzLayoutGuard
// ============================================================================
static const wchar_t* PROP_OVERLAP_OK = L"az_overlap_ok";
void AzLayoutGuard_AllowOverlap(HWND child){
    SetPropW(child, PROP_OVERLAP_OK, (HANDLE)1);
}
static bool guard_excluded(HWND c){
    if(GetPropW(c, PROP_OVERLAP_OK)) return true;
    LONG ex=GetWindowLongW(c, GWL_EXSTYLE);
    if(ex & WS_EX_LAYERED) return true;
    return false;
}
bool AzLayoutGuard_Verify(HWND hParent){
    if(!IsWindow(hParent)) return true;
    std::vector<HWND> kids;
    for(HWND c=GetWindow(hParent,GW_CHILD); c; c=GetWindow(c,GW_HWNDNEXT)){
        if(!IsWindowVisible(c)) continue;
        if(guard_excluded(c)) continue;
        kids.push_back(c);
    }
    bool clean=true;
    for(size_t i=0;i<kids.size();++i){
        for(size_t j=i+1;j<kids.size();++j){
            RECT a,b,inter; GetWindowRect(kids[i],&a); GetWindowRect(kids[j],&b);
            if(IntersectRect(&inter,&a,&b)){
                clean=false;
#ifndef NDEBUG
                OutputDebugStringW(L"[AzLayoutGuard] overlap detected\n");
#endif
                // soft correction: push the second (lower in z = later sibling)
                // below the first by a small gap.
                MapWindowPoints(NULL, hParent, (POINT*)&b, 2);
                MapWindowPoints(NULL, hParent, (POINT*)&a, 2);
                int newTop = a.bottom + S(6);
                SetWindowPos(kids[j], NULL, b.left, newTop, 0,0,
                             SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE);
            }
        }
    }
    if(!clean) PostMessageW(hParent, WM_APP_LAYOUT_REDO, 0, 0);
    return clean;
}

// ============================================================================
//  AzZOrderShield
// ============================================================================
struct ShieldFrame { HWND page; std::vector<std::pair<HWND,bool>> saved; };
static std::vector<ShieldFrame>& shieldStack(){
    static std::vector<ShieldFrame> s; return s;
}
void AzZOrderShield_Push(HWND hPage){
    if(!IsWindow(hPage)) return;
    // prevent double-push of the same page
    for(auto& f : shieldStack()) if(f.page==hPage) return;
    HWND parent=GetParent(hPage);
    if(!parent) return;
    ShieldFrame fr; fr.page=hPage;
    for(HWND c=GetWindow(parent,GW_CHILD); c; c=GetWindow(c,GW_HWNDNEXT)){
        if(c==hPage) continue;
        bool vis=IsWindowVisible(c)!=0;
        fr.saved.push_back({c,vis});
        if(vis) ShowWindow(c,SW_HIDE);
    }
    shieldStack().push_back(fr);
    RECT rc; GetClientRect(parent,&rc);
    SetWindowPos(hPage,HWND_TOP,0,0,rc.right,rc.bottom,SWP_SHOWWINDOW);
}
void AzZOrderShield_Pop(HWND hPage){
    auto& st=shieldStack();
    for(int i=(int)st.size()-1;i>=0;--i){
        if(st[i].page==hPage){
            for(auto& pr : st[i].saved)
                if(IsWindow(pr.first) && pr.second) ShowWindow(pr.first,SW_SHOW);
            HWND parent=GetParent(hPage);
            st.erase(st.begin()+i);
            if(parent){ InvalidateRect(parent,NULL,TRUE); UpdateWindow(parent); }
            return;
        }
    }
}

// ----------------------------------------------------- registration ---------
void Az_RegisterControls(){
    static bool done=false; if(done) return; done=true;
    WNDCLASSEXW wc; ZeroMemory(&wc,sizeof(wc)); wc.cbSize=sizeof(wc);
    wc.hInstance=g_hInst; wc.hCursor=LoadCursor(NULL,IDC_ARROW);
    wc.style=CS_HREDRAW|CS_VREDRAW;
    wc.lpfnWndProc=SwitchProc; wc.lpszClassName=CLS_SWITCH; RegisterClassExW(&wc);
    wc.lpfnWndProc=SpinProc;   wc.lpszClassName=CLS_SPIN;   RegisterClassExW(&wc);
    wc.lpfnWndProc=ColorProc;  wc.lpszClassName=CLS_COLOR;  RegisterClassExW(&wc);
    wc.hCursor=LoadCursor(NULL,IDC_HAND);
    wc.lpfnWndProc=DropProc;   wc.lpszClassName=CLS_DROP;   RegisterClassExW(&wc);
}

} // namespace uikit
