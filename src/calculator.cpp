// ============================================================================
//  calculator.cpp — custom always-on-top calculator
//  keyboard + numpad + mouse, fast double math, special UI
// ============================================================================
#include "app.h"
#include <math.h>
#include <stdio.h>

#define CALC_CLASS L"AzCalc"

struct CalcState {
    std::wstring display;     // current entry
    std::wstring history;     // expression line
    double acc;               // accumulator
    wchar_t pendOp;           // 0 + - * /
    bool fresh;               // next digit starts new entry
    bool err;
    HWND btns[24];
    int  hot;                 // hovered button index (-1)
    int  press;               // pressed index
};

// layout: 6 rows x 4 cols
static const wchar_t* KEYS[24] = {
    L"C",  L"\u232B", L"%",  L"\u00F7",
    L"7",  L"8",  L"9",  L"\u00D7",
    L"4",  L"5",  L"6",  L"\u2212",
    L"1",  L"2",  L"3",  L"+",
    L"\u00B1", L"0",  L".",  L"=",
    L"\u221A", L"x\u00B2", L"1/x", L"M"   // extra row at top? keep 5 rows -> use 20
};
// We'll actually use 5 rows of 4 = 20 keys: indices 0..19

static double toNum(const std::wstring& s){
    if(s.empty()) return 0;
    return _wtof(s.c_str());
}
static std::wstring numStr(double v){
    if(v != v) return L"خطا";
    wchar_t buf[64];
    if(fabs(v) < 1e15 && v == floor(v) && fabs(v) >= 1.0 - 1e-9)
        swprintf(buf,64,L"%.0f",v);
    else if(v == 0) wcscpy(buf, L"0");
    else {
        swprintf(buf,64,L"%.10G",v);
    }
    return buf;
}
static std::wstring groupNum(const std::wstring& s){
    // thousands grouping for integer part
    size_t dot = s.find(L'.');
    std::wstring ip = (dot==std::wstring::npos)?s:s.substr(0,dot);
    std::wstring fp = (dot==std::wstring::npos)?L"":s.substr(dot);
    bool neg = !ip.empty() && ip[0]==L'-';
    if(neg) ip = ip.substr(1);
    if(ip.size()>3 && ip.find_first_not_of(L"0123456789")==std::wstring::npos){
        std::wstring o; int c=0;
        for(int i=(int)ip.size()-1;i>=0;i--){
            o.insert(o.begin(), ip[i]);
            if(++c%3==0 && i>0) o.insert(o.begin(), L',');
        }
        ip = o;
    }
    return (neg?L"-":L"")+ip+fp;
}

static void calcEval(CalcState* s){
    if(!s->pendOp){ s->acc = toNum(s->display); return; }
    double b = toNum(s->display);
    switch(s->pendOp){
        case L'+': s->acc += b; break;
        case L'-': s->acc -= b; break;
        case L'*': s->acc *= b; break;
        case L'/':
            if(b==0){ s->err=true; s->display=L"تقسیم بر صفر"; return; }
            s->acc /= b; break;
    }
    s->display = numStr(s->acc);
}

static void calcKey(HWND h, CalcState* s, const std::wstring& k){
    if(s->err && k!=L"C"){ return; }
    if(k.size()==1 && k[0]>=L'0' && k[0]<=L'9'){
        if(s->fresh){ s->display.clear(); s->fresh=false; }
        if(s->display==L"0") s->display.clear();
        if(s->display.size()<18) s->display += k;
    }
    else if(k==L"."){
        if(s->fresh){ s->display=L"0"; s->fresh=false; }
        if(s->display.find(L'.')==std::wstring::npos) s->display += L".";
    }
    else if(k==L"C"){
        s->display=L"0"; s->history.clear(); s->acc=0; s->pendOp=0;
        s->fresh=true; s->err=false;
    }
    else if(k==L"\u232B"){ // backspace
        if(!s->fresh && s->display.size()>0) s->display.erase(s->display.size()-1);
        if(s->display.empty()) { s->display=L"0"; s->fresh=true; }
    }
    else if(k==L"\u00B1"){
        if(s->display!=L"0"){
            if(s->display[0]==L'-') s->display.erase(0,1);
            else s->display.insert(s->display.begin(), L'-');
        }
    }
    else if(k==L"%"){
        double v = toNum(s->display);
        if(s->pendOp) v = s->acc * v / 100.0;
        else v = v/100.0;
        s->display = numStr(v);
    }
    else if(k==L"\u221A"){
        double v = toNum(s->display);
        if(v<0){ s->err=true; s->display=L"خطا"; }
        else s->display = numStr(sqrt(v));
        s->fresh = true;
    }
    else if(k==L"x\u00B2"){
        double v = toNum(s->display);
        s->display = numStr(v*v); s->fresh=true;
    }
    else if(k==L"1/x"){
        double v = toNum(s->display);
        if(v==0){ s->err=true; s->display=L"تقسیم بر صفر"; }
        else s->display = numStr(1.0/v);
        s->fresh=true;
    }
    else if(k==L"+"||k==L"\u2212"||k==L"-"||k==L"\u00D7"||k==L"*"||k==L"\u00F7"||k==L"/"){
        wchar_t op = k==L"+"?L'+' : (k==L"\u2212"||k==L"-")?L'-'
                   : (k==L"\u00D7"||k==L"*")?L'*' : L'/';
        if(s->pendOp && !s->fresh) calcEval(s);
        else if(!s->pendOp) s->acc = toNum(s->display);
        if(s->err){ InvalidateRect(h,NULL,TRUE); return; }
        s->pendOp = op;
        s->history = numStr(s->acc) + L" " + std::wstring(1,
            op==L'*'?L'\u00D7':op==L'/'?L'\u00F7':op==L'-'?L'\u2212':op);
        s->fresh = true;
    }
    else if(k==L"="){
        if(s->pendOp){
            std::wstring b = s->display;
            s->history += L" " + b + L" =";
            calcEval(s);
            s->pendOp = 0;
            s->fresh = true;
        }
    }
    InvalidateRect(h, NULL, FALSE);
}

static void calcLayout(HWND h, RECT& disp, RECT cells[20]){
    RECT rc; GetClientRect(h,&rc);
    int pad = S(12);
    int dispH = S(110);
    disp = { pad, pad, rc.right-pad, pad+dispH };
    int gy = disp.bottom + S(10);
    int gw = rc.right - 2*pad, gh = rc.bottom - gy - pad;
    int cw = gw/4, ch = gh/5;
    for(int r2=0;r2<5;r2++) for(int c=0;c<4;c++){
        int i = r2*4+c;
        // RTL: column 0 at right
        int cx = rc.right - pad - (c+1)*cw;
        cells[i] = { cx+S(3), gy+r2*ch+S(3), cx+cw-S(3), gy+(r2+1)*ch-S(3) };
    }
}
static int cellHit(HWND h, POINT pt){
    RECT d, cells[20]; calcLayout(h,d,cells);
    for(int i=0;i<20;i++) if(PtInRect(&cells[i],pt)) return i;
    return -1;
}

static LRESULT CALLBACK calcProc(HWND h, UINT m, WPARAM w, LPARAM l){
    CalcState* s = (CalcState*)GetWindowLongPtrW(h,GWLP_USERDATA);
    switch(m){
    case WM_CREATE: {
        s = new CalcState();
        s->display=L"0"; s->acc=0; s->pendOp=0; s->fresh=true; s->err=false;
        s->hot=-1; s->press=-1;
        SetWindowLongPtrW(h,GWLP_USERDATA,(LONG_PTR)s);
        return 0; }
    case WM_DESTROY: delete s; break;
    case WM_ERASEBKGND: return 1;
    case WM_MOUSEMOVE: {
        if(!s) break;
        POINT pt={GET_X_LPARAM(l),GET_Y_LPARAM(l)};
        int hit = cellHit(h,pt);
        if(hit != s->hot){ s->hot=hit; InvalidateRect(h,NULL,FALSE); }
        TRACKMOUSEEVENT t={sizeof(t),TME_LEAVE,h,0}; TrackMouseEvent(&t);
        break; }
    case WM_MOUSELEAVE:
        if(s && s->hot!=-1){ s->hot=-1; InvalidateRect(h,NULL,FALSE);} break;
    case WM_LBUTTONDOWN: {
        if(!s) break;
        POINT pt={GET_X_LPARAM(l),GET_Y_LPARAM(l)};
        s->press = cellHit(h,pt);
        if(s->press>=0) InvalidateRect(h,NULL,FALSE);
        break; }
    case WM_LBUTTONUP: {
        if(!s) break;
        POINT pt={GET_X_LPARAM(l),GET_Y_LPARAM(l)};
        int hit = cellHit(h,pt);
        if(hit>=0 && hit<20 && hit==s->press) calcKey(h, s, KEYS[hit]);
        s->press=-1; InvalidateRect(h,NULL,FALSE);
        break; }
    case WM_KEYDOWN: {
        if(!s) break;
        switch(w){
        case VK_ESCAPE: DestroyWindow(h); return 0;
        case VK_RETURN: calcKey(h,s,L"="); return 0;
        case VK_BACK:   calcKey(h,s,L"\u232B"); return 0;
        case VK_DELETE: calcKey(h,s,L"C"); return 0;
        case VK_ADD:      calcKey(h,s,L"+"); return 0;
        case VK_SUBTRACT: calcKey(h,s,L"-"); return 0;
        case VK_MULTIPLY: calcKey(h,s,L"*"); return 0;
        case VK_DIVIDE:   calcKey(h,s,L"/"); return 0;
        case VK_DECIMAL:  calcKey(h,s,L"."); return 0;
        }
        if(w>=L'0'&&w<=L'9' && !(GetKeyState(VK_SHIFT)&0x8000))
            { calcKey(h,s,std::wstring(1,(wchar_t)w)); return 0; }
        if(w>=VK_NUMPAD0 && w<=VK_NUMPAD9)
            { calcKey(h,s,std::wstring(1,(wchar_t)(L'0'+w-VK_NUMPAD0))); return 0; }
        break; }
    case WM_CHAR: {
        if(!s) break;
        wchar_t c=(wchar_t)w;
        if(c==L'+'||c==L'-'||c==L'*'||c==L'/') calcKey(h,s,std::wstring(1,c));
        else if(c==L'.'||c==L',') calcKey(h,s,L".");
        else if(c==L'%') calcKey(h,s,L"%");
        else if(c==L'=') calcKey(h,s,L"=");
        return 0; }
    case WM_PAINT: {
        if(!s){ PAINTSTRUCT ps; BeginPaint(h,&ps); EndPaint(h,&ps); return 0; }
        PAINTSTRUCT ps; HDC dc0=BeginPaint(h,&ps);
        RECT rc; GetClientRect(h,&rc);
        HDC dc=CreateCompatibleDC(dc0);
        HBITMAP bmp=CreateCompatibleBitmap(dc0,rc.right,rc.bottom);
        HGDIOBJ obm=SelectObject(dc,bmp);
        // Use the deeper bg2 tone so the white key/display surfaces stand out
        // clearly in the light theme (they previously blended into the bg).
        { HBRUSH bb=CreateSolidBrush(g_theme.bg2); FillRect(dc,&rc,bb); DeleteObject(bb); }

        RECT disp, cells[20]; calcLayout(h,disp,cells);
        // display panel
        fillRoundRect(dc,disp,S(14),g_theme.surface,g_theme.border);
        SetBkMode(dc,TRANSPARENT);
        // history line
        SetTextColor(dc,g_theme.textDim);
        SelectObject(dc,g_fSmall);
        RECT hr={disp.left+S(14),disp.top+S(10),disp.right-S(14),disp.top+S(34)};
        DrawTextW(dc,toFaDigits(s->history).c_str(),-1,&hr,
            DT_LEFT|DT_SINGLELINE|DT_NOPREFIX);
        // main number
        SetTextColor(dc, s->err? g_theme.danger : g_theme.text);
        SelectObject(dc,g_fHuge);
        RECT mr={disp.left+S(14),disp.top+S(32),disp.right-S(14),disp.bottom-S(10)};
        std::wstring shown = s->err ? s->display : toFaDigits(groupNum(s->display));
        DrawTextW(dc,shown.c_str(),-1,&mr,
            DT_LEFT|DT_SINGLELINE|DT_VCENTER|DT_NOPREFIX|DT_END_ELLIPSIS);

        // keys
        for(int i=0;i<20;i++){
            const wchar_t* k = KEYS[i];
            bool isOp = (i%4==3) || i<4;
            bool isEq = wcscmp(k,L"=")==0;
            COLORREF fill = isEq ? g_theme.accent
                          : isOp ? g_theme.surface2 : g_theme.surface;
            COLORREF txt  = isEq ? g_theme.accentText
                          : isOp ? g_theme.accent : g_theme.text;
            if(i==s->hot)  fill = isEq ? g_theme.accentHover : g_theme.hover;
            if(i==s->press){
                RECT r2=cells[i]; r2.top+=2; r2.bottom+=2;
                fillRoundRect(dc,r2,S(12),fill,g_theme.border);
                SetTextColor(dc,txt);
                SelectObject(dc,g_fTitle);
                DrawTextW(dc,k,-1,&r2,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);
                continue;
            }
            fillRoundRect(dc,cells[i],S(12),fill,
                isEq?CLR_INVALID:g_theme.border);
            SetTextColor(dc,txt);
            SelectObject(dc,g_fTitle);
            RECT cr = cells[i];
            DrawTextW(dc,k,-1,&cr,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);
        }
        BitBlt(dc0,0,0,rc.right,rc.bottom,dc,0,0,SRCCOPY);
        SelectObject(dc,obm); DeleteObject(bmp); DeleteDC(dc);
        EndPaint(h,&ps);
        return 0; }
    case WM_CLOSE: DestroyWindow(h); return 0;
    }
    return DefWindowProcW(h,m,w,l);
}

void openCalculator(HWND owner){
    static bool reg=false;
    if(!reg){
        WNDCLASSW wc={0};
        wc.lpfnWndProc=calcProc; wc.hInstance=g_hInst;
        wc.hCursor=LoadCursor(NULL,IDC_ARROW);
        wc.lpszClassName=CALC_CLASS;
        RegisterClassW(&wc);
        reg=true;
    }
    // if already open — bring to front
    HWND ex = FindWindowW(CALC_CLASS, NULL);
    if(ex){ SetForegroundWindow(ex); return; }

    int w=S(340), hgt=S(520);
    RECT scr; SystemParametersInfoW(SPI_GETWORKAREA,0,&scr,0);
    int x = scr.right - w - S(60), y = (scr.bottom-hgt)/2;
    HWND c = CreateWindowExW(WS_EX_TOPMOST|WS_EX_TOOLWINDOW, CALC_CLASS,
        L"ماشین حساب — آزادی طب",
        WS_POPUP|WS_CAPTION|WS_SYSMENU|WS_VISIBLE,
        x,y,w,hgt, owner, NULL, g_hInst, NULL);
    SetForegroundWindow(c);
    SetFocus(c);
}
