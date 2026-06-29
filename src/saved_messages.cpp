// ============================================================================
//  saved_messages.cpp — RELEASE 1.11.0, §F
//  Standalone "Saved Messages" (پیام‌های ذخیره‌شده) viewer.
//
//  Opened from the messenger-style settings panel (user_settings.cpp →
//  IDC_PANEL_BASE+63) and reachable from the inbox toolbar. It renders the
//  permanent local archive backed by data\saved_msgs.dat (loadSavedMsgs() in
//  employees.cpp). The store is pipe-delimited and forward-compatible (§I):
//  unknown trailing columns are ignored on read and never rewritten here — this
//  window NEVER mutates the file except via the public pushSavedMsg() API.
//
//  Layout (manual RTL, no OS mirroring, double-buffered):
//    • Header bar: bookmark icon + «پیام‌های ذخیره‌شده» title (right), close ×.
//    • Vertical stack of full-width message cards (newest first). Each card
//      shows the sender, a severity chip, the date/time and a 2-line body.
//    • Empty state: a centred bookmark glyph + «پیامی ذخیره نشده است».
//  Scroll: mouse wheel. Click a card → detail box. No flicker (MemDC).
// ============================================================================
#include "app.h"
#include "ui_kit.h"
#include <windows.h>
#include <commdlg.h>
#include <shellapi.h>
#include <vector>
#include <string>

void SavedMessages_Show(HWND owner);   // public entry (declared in callers)

namespace {

struct SavedWin {
    HWND hwnd{nullptr};
    HWND owner{nullptr};
    std::vector<SavedMsg> msgs;
    int scrollY{0};
    int contentH{0};
    int hotCard{-1};      // hovered card index
    int closeHot{0};      // close button hover
    int composeHot{0};    // «پیام جدید» (new message) button hover
    std::vector<RECT> cardRects;  // hit map (client coords, pre-scroll baked in)
};

static SavedWin* g_sw = nullptr;

static COLORREF sevCol(int type){
    return type==KMSG_CRITICAL ? g_theme.danger
         : type==KMSG_URGENT   ? g_theme.warn
         :                       g_theme.success;
}
static const wchar_t* sevLbl(int type){
    return type==KMSG_CRITICAL ? L"\u0628\u062d\u0631\u0627\u0646\u06cc"   // بحرانی
         : type==KMSG_URGENT   ? L"\u0641\u0648\u0631\u06cc"                // فوری
         :                       L"\u0639\u0627\u062f\u06cc";               // عادی
}

// -- geometry ---------------------------------------------------------------
static const int HEADER_H = 56;
static int cardH(){ return S(92); }
static int gap(){ return S(10); }
static int pad(){ return S(16); }

static void rebuildLayout(SavedWin* w){
    w->cardRects.clear();
    RECT rc; GetClientRect(w->hwnd,&rc);
    int top = S(HEADER_H)+pad();
    int x = pad(), right = rc.right-pad();
    int y = top - w->scrollY;
    for(size_t i=0;i<w->msgs.size();++i){
        RECT c{ x, y, right, y+cardH() };
        w->cardRects.push_back(c);
        y += cardH()+gap();
    }
    w->contentH = (int)w->msgs.size()*(cardH()+gap()) + pad();
}

static void clampScroll(SavedWin* w){
    RECT rc; GetClientRect(w->hwnd,&rc);
    int visible = rc.bottom - S(HEADER_H);
    int maxY = w->contentH - visible;
    if(maxY<0) maxY=0;
    if(w->scrollY<0) w->scrollY=0;
    if(w->scrollY>maxY) w->scrollY=maxY;
}

// -- painting ---------------------------------------------------------------
static void paintHeader(HDC dc, SavedWin* w, const RECT& rc){
    RECT hb{0,0,rc.right,S(HEADER_H)};
    { HBRUSH b=CreateSolidBrush(g_theme.surface); FillRect(dc,&hb,b); DeleteObject(b); }
    { HPEN p=CreatePen(PS_SOLID,1,g_theme.border); HGDIOBJ o=SelectObject(dc,p);
      MoveToEx(dc,0,S(HEADER_H)-1,NULL); LineTo(dc,rc.right,S(HEADER_H)-1);
      SelectObject(dc,o); DeleteObject(p); }
    SetBkMode(dc,TRANSPARENT);
    // bookmark icon + title (RTL: right side)
    int ic=S(22);
    RECT ir{ rc.right-pad()-ic, (S(HEADER_H)-ic)/2, rc.right-pad(), (S(HEADER_H)-ic)/2+ic };
    drawIcon(dc,ICO_SAVED_MSG,ir,g_theme.accent,2);
    HFONT of=(HFONT)SelectObject(dc,g_fTitle);
    SetTextColor(dc,g_theme.text);
    RECT tr{ rc.right/2, 0, ir.left-S(8), S(HEADER_H) };
    DrawTextW(dc,L"\u067e\u06cc\u0627\u0645\u200c\u0647\u0627\u06cc \u0630\u062e\u06cc\u0631\u0647\u200c\u0634\u062f\u0647",-1,&tr,
        DT_RIGHT|DT_VCENTER|DT_SINGLELINE|DT_RTLREADING|DT_END_ELLIPSIS);
    SelectObject(dc,of);
    // close × (top-left)
    int cz=S(30);
    RECT cb{ pad(), (S(HEADER_H)-cz)/2, pad()+cz, (S(HEADER_H)-cz)/2+cz };
    if(w->closeHot){ gpRoundRectBg(dc,cb,S(8),g_theme.surface2,g_theme.border,g_theme.surface); }
    RECT xi{ cb.left+S(7), cb.top+S(7), cb.right-S(7), cb.bottom-S(7) };
    drawIcon(dc,ICO_X,xi,g_theme.textDim,2);
    // «پیام جدید» compose pill (top-left, right of the close button)
    {
        const wchar_t* lbl=L"\u067e\u06cc\u0627\u0645 \u062c\u062f\u06cc\u062f"; // پیام جدید
        SIZE sz; HFONT pf=(HFONT)SelectObject(dc,g_fUI);
        GetTextExtentPoint32W(dc,lbl,(int)wcslen(lbl),&sz);
        SelectObject(dc,pf);
        int ph=S(32), icon=S(16);
        RECT pb{ cb.right+S(10), (S(HEADER_H)-ph)/2,
                 cb.right+S(10)+icon+sz.cx+S(26), (S(HEADER_H)-ph)/2+ph };
        COLORREF base=w->composeHot ? blendColor(g_theme.accent,g_theme.surface,30)
                                    : g_theme.accent;
        gpRoundRectBg(dc,pb,ph/2,base,base,g_theme.surface);
        RECT ii{ pb.left+S(10), pb.top+(ph-icon)/2, pb.left+S(10)+icon, pb.top+(ph-icon)/2+icon };
        drawIcon(dc,ICO_PLUS,ii,RGB(255,255,255),2);
        HFONT of=(HFONT)SelectObject(dc,g_fUI);
        SetTextColor(dc,RGB(255,255,255));
        RECT tr{ ii.right+S(4), pb.top, pb.right-S(8), pb.bottom };
        DrawTextW(dc,lbl,-1,&tr,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_RTLREADING);
        SelectObject(dc,of);
    }
}

// hit-rect of the compose pill (must mirror paintHeader geometry)
static RECT composeRect(SavedWin* w){
    HDC dc=GetDC(w->hwnd);
    const wchar_t* lbl=L"\u067e\u06cc\u0627\u0645 \u062c\u062f\u06cc\u062f";
    SIZE sz; HFONT pf=(HFONT)SelectObject(dc,g_fUI);
    GetTextExtentPoint32W(dc,lbl,(int)wcslen(lbl),&sz);
    SelectObject(dc,pf); ReleaseDC(w->hwnd,dc);
    int cz=S(30), ph=S(32), icon=S(16);
    int cbRight=pad()+cz;
    RECT pb{ cbRight+S(10), (S(HEADER_H)-ph)/2,
             cbRight+S(10)+icon+sz.cx+S(26), (S(HEADER_H)-ph)/2+ph };
    return pb;
}

// ---------------------------------------------------------------------------
//  Composer popup (پیام جدید): text body + severity + optional file attach.
//  Modal child window; on «ذخیره» it persists via pushSavedMsg() and asks the
//  parent to reload. Standard Win32 controls only (no custom uikit controls).
// ---------------------------------------------------------------------------
struct ComposeWin {
    HWND hwnd{nullptr};
    HWND owner{nullptr};      // the SavedWin window to refresh on success
    HWND edText{nullptr};
    HWND cbSev{nullptr};
    HWND btAttach{nullptr};
    HWND btSave{nullptr};
    HWND btCancel{nullptr};
    std::wstring attachPath;  // source path chosen by the user (copied on save)
    bool saved{false};
};

enum {
    CMP_TEXT   = 4101,
    CMP_SEV    = 4102,
    CMP_ATTACH = 4103,
    CMP_SAVE   = 4104,
    CMP_CANCEL = 4105
};

static void paintCard(HDC dc, SavedWin* w, int idx, const RECT& c){
    const SavedMsg& m=w->msgs[idx];
    bool hot=(idx==w->hotCard);
    COLORREF fill = hot ? g_theme.surface2 : g_theme.surface;
    gpRoundRectBg(dc,c,S(12),fill,g_theme.border,g_theme.bg);
    SetBkMode(dc,TRANSPARENT);
    int ix=c.left+pad(), ir=c.right-pad();
    // sender (bold, right)
    HFONT of=(HFONT)SelectObject(dc,g_fUIB);
    SetTextColor(dc,g_theme.text);
    RECT sr{ ix, c.top+S(8), ir, c.top+S(8)+S(20) };
    std::wstring who = m.from.empty()? L"\u0633\u06cc\u0633\u062a\u0645" : m.from;  // سیستم
    DrawTextW(dc,who.c_str(),-1,&sr,DT_RIGHT|DT_VCENTER|DT_SINGLELINE|DT_RTLREADING|DT_END_ELLIPSIS);
    SelectObject(dc,of);
    // severity chip (left side)
    {
        const wchar_t* lbl=sevLbl(m.type);
        SIZE sz; HFONT sf=(HFONT)SelectObject(dc,g_fSmall);
        GetTextExtentPoint32W(dc,lbl,(int)wcslen(lbl),&sz);
        int chW=sz.cx+S(16), chH=S(18);
        RECT chip{ ix, c.top+S(9), ix+chW, c.top+S(9)+chH };
        COLORREF cc=sevCol(m.type);
        gpRoundRectBg(dc,chip,chH/2,blendColor(cc,g_theme.surface,40),cc,fill);
        SetTextColor(dc,cc);
        DrawTextW(dc,lbl,-1,&chip,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
        SelectObject(dc,sf);
    }
    // date/time (muted, right, second line)
    {
        HFONT sf=(HFONT)SelectObject(dc,g_fSmall);
        SetTextColor(dc,g_theme.textDim);
        RECT dr{ ix, c.top+S(30), ir, c.top+S(30)+S(16) };
        DrawTextW(dc,m.time.c_str(),-1,&dr,DT_RIGHT|DT_VCENTER|DT_SINGLELINE|DT_RTLREADING);
        SelectObject(dc,sf);
    }
    // body (2 lines)
    {
        HFONT bf=(HFONT)SelectObject(dc,g_fUI);
        SetTextColor(dc,g_theme.text);
        RECT br{ ix, c.top+S(48), ir, c.bottom-S(8) };
        DrawTextW(dc,m.text.c_str(),-1,&br,
            DT_RIGHT|DT_WORDBREAK|DT_RTLREADING|DT_END_ELLIPSIS|DT_EDITCONTROL);
        SelectObject(dc,bf);
    }
    // attachment hint
    if(!m.attachPath.empty()){
        HFONT sf=(HFONT)SelectObject(dc,g_fSmall);
        SetTextColor(dc,g_theme.accent2);
        RECT ar{ ix, c.bottom-S(20), ir, c.bottom-S(4) };
        DrawTextW(dc,L"\u067e\u06cc\u0648\u0633\u062a",-1,&ar,DT_LEFT|DT_VCENTER|DT_SINGLELINE);
        SelectObject(dc,sf);
    }
}

static void paintEmpty(HDC dc, const RECT& rc){
    SetBkMode(dc,TRANSPARENT);
    int ic=S(64);
    RECT ir{ (rc.right-ic)/2, rc.bottom/2-S(60), (rc.right+ic)/2, rc.bottom/2-S(60)+ic };
    drawIcon(dc,ICO_SAVED_MSG,ir,blendColor(g_theme.textDim,g_theme.bg,120),2);
    HFONT of=(HFONT)SelectObject(dc,g_fUI);
    SetTextColor(dc,g_theme.textDim);
    RECT tr{ 0, rc.bottom/2+S(10), rc.right, rc.bottom/2+S(40) };
    DrawTextW(dc,L"\u067e\u06cc\u0627\u0645\u06cc \u0630\u062e\u06cc\u0631\u0647 \u0646\u0634\u062f\u0647 \u0627\u0633\u062a.",-1,&tr,
        DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_RTLREADING);
    SelectObject(dc,of);
}

static void paintWin(SavedWin* w){
    RECT rc; GetClientRect(w->hwnd,&rc);
    uikit::WindowDC wdc(w->hwnd);
    uikit::MemDC mem(wdc.dc,rc.right,rc.bottom);
    { HBRUSH b=CreateSolidBrush(g_theme.bg); FillRect(mem.dc,&rc,b); DeleteObject(b); }
    if(w->msgs.empty()){
        paintEmpty(mem.dc,rc);
    } else {
        rebuildLayout(w);
        HRGN clip=CreateRectRgn(0,S(HEADER_H),rc.right,rc.bottom);
        SelectClipRgn(mem.dc,clip);
        for(size_t i=0;i<w->cardRects.size();++i){
            const RECT& c=w->cardRects[i];
            if(c.bottom<S(HEADER_H) || c.top>rc.bottom) continue;  // cull off-screen
            paintCard(mem.dc,w,(int)i,c);
        }
        SelectClipRgn(mem.dc,NULL);
        DeleteObject(clip);
    }
    paintHeader(mem.dc,w,rc);
    mem.blitTo(wdc.dc);
}

static int hitCard(SavedWin* w, POINT pt){
    for(size_t i=0;i<w->cardRects.size();++i)
        if(PtInRect(&w->cardRects[i],pt)) return (int)i;
    return -1;
}

static void showDetail(SavedWin* w, int idx){
    if(idx<0 || idx>=(int)w->msgs.size()) return;
    const SavedMsg& m=w->msgs[idx];
    std::wstring body;
    body += L"\u0641\u0631\u0633\u062a\u0646\u062f\u0647: " + (m.from.empty()?L"\u0633\u06cc\u0633\u062a\u0645":m.from) + L"\r\n";
    body += L"\u0632\u0645\u0627\u0646: " + m.time + L"\r\n";
    body += L"\u0627\u0648\u0644\u0648\u06cc\u062a: " + std::wstring(sevLbl(m.type)) + L"\r\n";
    if(!m.attachPath.empty())
        body += L"\u067e\u06cc\u0648\u0633\u062a: " + m.attachPath + L"\r\n";
    body += L"\r\n" + m.text;
    if(!m.attachPath.empty()){
        body += L"\r\n\r\n\u0628\u0631\u0627\u06cc \u0628\u0627\u0632 \u06a9\u0631\u062f\u0646 \u067e\u06cc\u0648\u0633\u062a \u00abYes\u00bb \u0631\u0627 \u0628\u0632\u0646\u06cc\u062f."; // برای باز کردن پیوست Yes را بزنید
        int r=MessageBoxW(w->hwnd, body.c_str(),
            L"\u067e\u06cc\u0627\u0645 \u0630\u062e\u06cc\u0631\u0647\u200c\u0634\u062f\u0647",
            MB_YESNO|MB_ICONINFORMATION);
        if(r==IDYES)
            ShellExecuteW(w->hwnd,L"open",m.attachPath.c_str(),NULL,NULL,SW_SHOWNORMAL);
        return;
    }
    MessageBoxW(w->hwnd, body.c_str(),
        L"\u067e\u06cc\u0627\u0645 \u0630\u062e\u06cc\u0631\u0647\u200c\u0634\u062f\u0647",
        MB_OK|MB_ICONINFORMATION);
}

// ---------------------------------------------------------------------------
//  Composer implementation
// ---------------------------------------------------------------------------
static const wchar_t* COMPOSE_CLASS = L"AzSavedMsgCompose";

static void composeDoAttach(ComposeWin* c){
    wchar_t buf[MAX_PATH]; buf[0]=0;
    OPENFILENAMEW ofn{}; ofn.lStructSize=sizeof(ofn);
    ofn.hwndOwner=c->hwnd;
    ofn.lpstrFile=buf; ofn.nMaxFile=MAX_PATH;
    ofn.lpstrFilter=L"\u0647\u0645\u0647 \u0641\u0627\u06cc\u0644\u200c\u0647\u0627\0*.*\0"
                    L"\u062a\u0635\u0627\u0648\u06cc\u0631\0*.png;*.jpg;*.jpeg;*.bmp;*.gif\0"
                    L"PDF\0*.pdf\0\0";
    ofn.nFilterIndex=1;
    ofn.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_EXPLORER;
    if(GetOpenFileNameW(&ofn)){
        c->attachPath=buf;
        size_t slash=c->attachPath.find_last_of(L"\\/");
        std::wstring base=(slash==std::wstring::npos)?c->attachPath:c->attachPath.substr(slash+1);
        std::wstring lbl=L"\U0001F4CE "+base;   // paperclip + name
        SetWindowTextW(c->btAttach,lbl.c_str());
    }
}

static void composeDoSave(ComposeWin* c){
    int len=GetWindowTextLengthW(c->edText);
    std::wstring text(len+1,L'\0');
    GetWindowTextW(c->edText,&text[0],len+1);
    text.resize(len);
    // trim
    size_t a=text.find_first_not_of(L" \t\r\n");
    size_t b=text.find_last_not_of(L" \t\r\n");
    std::wstring body = (a==std::wstring::npos)? L"" : text.substr(a,b-a+1);
    if(body.empty() && c->attachPath.empty()){
        MessageBoxW(c->hwnd,
            L"\u0645\u062a\u0646 \u067e\u06cc\u0627\u0645 \u0631\u0627 \u0648\u0627\u0631\u062f \u06a9\u0646\u06cc\u062f \u06cc\u0627 \u06cc\u06a9 \u0641\u0627\u06cc\u0644 \u067e\u06cc\u0648\u0633\u062a \u06a9\u0646\u06cc\u062f.",
            L"\u067e\u06cc\u0627\u0645 \u062c\u062f\u06cc\u062f", MB_OK|MB_ICONWARNING);
        return;
    }
    int sev=(int)SendMessageW(c->cbSev,CB_GETCURSEL,0,0);
    if(sev<0) sev=KMSG_NORMAL;
    std::wstring stored;
    if(!c->attachPath.empty()) stored=copyAttachmentLocal(c->attachPath);
    std::wstring from = g_session.user.fullname.empty()
        ? L"\u0645\u062f\u06cc\u0631\u06cc\u062a" : g_session.user.fullname;
    pushSavedMsg(from, from, body, sev, stored);
    c->saved=true;
    if(c->owner && IsWindow(c->owner))
        SendMessageW(c->owner, WM_APP+0x51, 0, 0);   // ask SavedWin to reload
    DestroyWindow(c->hwnd);
}

static LRESULT CALLBACK ComposeProc(HWND h,UINT msg,WPARAM wp,LPARAM lp){
    ComposeWin* c=(ComposeWin*)GetWindowLongPtrW(h,GWLP_USERDATA);
    switch(msg){
    case WM_CREATE:{
        CREATESTRUCTW* cs=(CREATESTRUCTW*)lp;
        c=(ComposeWin*)cs->lpCreateParams;
        SetWindowLongPtrW(h,GWLP_USERDATA,(LONG_PTR)c);
        c->hwnd=h;
        RECT rc; GetClientRect(h,&rc);
        int m=pad(), x=m, right=rc.right-m, w=right-x;
        int y=m;
        HFONT f=g_fUI?g_fUI:(HFONT)GetStockObject(DEFAULT_GUI_FONT);
        // label-less; controls speak for themselves in RTL layout
        // severity combo
        c->cbSev=CreateWindowExW(0,L"COMBOBOX",NULL,
            WS_CHILD|WS_VISIBLE|CBS_DROPDOWNLIST|WS_TABSTOP,
            x,y,w,S(200),h,(HMENU)(INT_PTR)CMP_SEV,g_hInst,NULL);
        SendMessageW(c->cbSev,WM_SETFONT,(WPARAM)f,TRUE);
        SendMessageW(c->cbSev,CB_ADDSTRING,0,(LPARAM)L"\u0639\u0627\u062f\u06cc");      // عادی
        SendMessageW(c->cbSev,CB_ADDSTRING,0,(LPARAM)L"\u0641\u0648\u0631\u06cc");      // فوری
        SendMessageW(c->cbSev,CB_ADDSTRING,0,(LPARAM)L"\u0628\u062d\u0631\u0627\u0646\u06cc"); // بحرانی
        SendMessageW(c->cbSev,CB_SETCURSEL,0,0);
        y+=S(34)+S(10);
        // multiline text body
        int edH=rc.bottom - y - S(48) - S(10) - S(44) - S(10);
        if(edH<S(120)) edH=S(120);
        c->edText=CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",NULL,
            WS_CHILD|WS_VISIBLE|WS_TABSTOP|WS_VSCROLL|ES_MULTILINE|ES_AUTOVSCROLL|ES_WANTRETURN,
            x,y,w,edH,h,(HMENU)(INT_PTR)CMP_TEXT,g_hInst,NULL);
        SendMessageW(c->edText,WM_SETFONT,(WPARAM)f,TRUE);
        y+=edH+S(10);
        // attach button (full width)
        c->btAttach=CreateWindowExW(0,L"BUTTON",
            L"\U0001F4CE \u0627\u0641\u0632\u0648\u062f\u0646 \u0641\u0627\u06cc\u0644 \u067e\u06cc\u0648\u0633\u062a", // افزودن فایل پیوست
            WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_PUSHBUTTON,
            x,y,w,S(38),h,(HMENU)(INT_PTR)CMP_ATTACH,g_hInst,NULL);
        SendMessageW(c->btAttach,WM_SETFONT,(WPARAM)f,TRUE);
        y+=S(38)+S(10);
        // save / cancel row
        int bw=(w-S(10))/2;
        c->btSave=CreateWindowExW(0,L"BUTTON",
            L"\u0630\u062e\u06cc\u0631\u0647",   // ذخیره
            WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_DEFPUSHBUTTON,
            x+bw+S(10),y,bw,S(40),h,(HMENU)(INT_PTR)CMP_SAVE,g_hInst,NULL);
        SendMessageW(c->btSave,WM_SETFONT,(WPARAM)f,TRUE);
        c->btCancel=CreateWindowExW(0,L"BUTTON",
            L"\u0627\u0646\u0635\u0631\u0627\u0641", // انصراف
            WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_PUSHBUTTON,
            x,y,bw,S(40),h,(HMENU)(INT_PTR)CMP_CANCEL,g_hInst,NULL);
        SendMessageW(c->btCancel,WM_SETFONT,(WPARAM)f,TRUE);
        SetFocus(c->edText);
        return 0; }
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX:{
        HDC dc=(HDC)wp;
        SetTextColor(dc,g_theme.text);
        SetBkColor(dc,g_theme.surface);
        static HBRUSH br=NULL; if(br) DeleteObject(br);
        br=CreateSolidBrush(g_theme.surface);
        return (LRESULT)br; }
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN:{
        HDC dc=(HDC)wp;
        SetTextColor(dc,g_theme.text);
        SetBkColor(dc,g_theme.bg);
        static HBRUSH bb=NULL; if(bb) DeleteObject(bb);
        bb=CreateSolidBrush(g_theme.bg);
        return (LRESULT)bb; }
    case WM_ERASEBKGND:{
        HDC dc=(HDC)wp; RECT rc; GetClientRect(h,&rc);
        HBRUSH b=CreateSolidBrush(g_theme.bg); FillRect(dc,&rc,b); DeleteObject(b);
        return 1; }
    case WM_COMMAND:{
        int id=LOWORD(wp);
        if(id==CMP_ATTACH && HIWORD(wp)==BN_CLICKED){ composeDoAttach(c); return 0; }
        if(id==CMP_SAVE   && HIWORD(wp)==BN_CLICKED){ composeDoSave(c);   return 0; }
        if(id==CMP_CANCEL && HIWORD(wp)==BN_CLICKED){ DestroyWindow(h);   return 0; }
        return 0; }
    case WM_KEYDOWN:
        if(wp==VK_ESCAPE){ DestroyWindow(h); return 0; }
        break;
    case WM_CLOSE: DestroyWindow(h); return 0;
    case WM_DESTROY:{
        HWND own=c?c->owner:NULL;
        if(c){ delete c; }
        SetWindowLongPtrW(h,GWLP_USERDATA,0);
        if(own && IsWindow(own)) EnableWindow(own,TRUE);
        return 0; }
    }
    return DefWindowProcW(h,msg,wp,lp);
}

static void openComposer(SavedWin* sw){
    static bool reg=false;
    if(!reg){
        WNDCLASSW wc{};
        wc.lpfnWndProc=ComposeProc;
        wc.hInstance=g_hInst;
        wc.hCursor=LoadCursorW(NULL,IDC_ARROW);
        wc.lpszClassName=COMPOSE_CLASS;
        wc.hbrBackground=NULL;
        RegisterClassW(&wc);
        reg=true;
    }
    ComposeWin* c=new ComposeWin();
    c->owner=sw->hwnd;
    int wpx=S(440), hpx=S(440);
    RECT pr{0,0,0,0}; GetWindowRect(sw->hwnd,&pr);
    int px=pr.left+((pr.right-pr.left)-wpx)/2;
    int py=pr.top +((pr.bottom-pr.top)-hpx)/2;
    HWND h=CreateWindowExW(WS_EX_DLGMODALFRAME,COMPOSE_CLASS,
        L"\u067e\u06cc\u0627\u0645 \u062c\u062f\u06cc\u062f",   // پیام جدید
        WS_POPUP|WS_CAPTION|WS_SYSMENU,
        px,py,wpx,hpx,sw->hwnd,NULL,g_hInst,c);
    if(!h){ delete c; return; }
    EnableWindow(sw->hwnd,FALSE);   // pseudo-modal
    ShowWindow(h,SW_SHOW);
    UpdateWindow(h);
}

static LRESULT CALLBACK SavedProc(HWND h,UINT msg,WPARAM wp,LPARAM lp){
    SavedWin* w=(SavedWin*)GetWindowLongPtrW(h,GWLP_USERDATA);
    switch(msg){
    case WM_CREATE:{
        CREATESTRUCTW* cs=(CREATESTRUCTW*)lp;
        w=(SavedWin*)cs->lpCreateParams;
        SetWindowLongPtrW(h,GWLP_USERDATA,(LONG_PTR)w);
        w->hwnd=h;
        w->msgs=loadSavedMsgs();
        return 0; }
    case WM_ERASEBKGND: return 1;   // MemDC owns the surface
    case WM_PAINT:{ PAINTSTRUCT ps; BeginPaint(h,&ps); paintWin(w); EndPaint(h,&ps); return 0; }
    case WM_MOUSEMOVE:{
        if(!w) break;
        POINT pt{ (short)LOWORD(lp), (short)HIWORD(lp) };
        int cz=S(30); RECT cb{ pad(), (S(HEADER_H)-cz)/2, pad()+cz, (S(HEADER_H)-cz)/2+cz };
        RECT pb=composeRect(w);
        int ch = PtInRect(&cb,pt)?1:0;
        int cm = PtInRect(&pb,pt)?1:0;
        int hc = (pt.y>S(HEADER_H)) ? hitCard(w,pt) : -1;
        if(ch!=w->closeHot || cm!=w->composeHot || hc!=w->hotCard){
            w->closeHot=ch; w->composeHot=cm; w->hotCard=hc;
            InvalidateRect(h,NULL,FALSE); }
        TRACKMOUSEEVENT tme{ sizeof(tme), TME_LEAVE, h, 0 }; TrackMouseEvent(&tme);
        return 0; }
    case WM_MOUSELEAVE:{
        if(w && (w->hotCard!=-1||w->closeHot||w->composeHot)){
            w->hotCard=-1; w->closeHot=0; w->composeHot=0;
            InvalidateRect(h,NULL,FALSE); }
        return 0; }
    case WM_LBUTTONDOWN:{
        if(!w) break;
        POINT pt{ (short)LOWORD(lp), (short)HIWORD(lp) };
        int cz=S(30); RECT cb{ pad(), (S(HEADER_H)-cz)/2, pad()+cz, (S(HEADER_H)-cz)/2+cz };
        if(PtInRect(&cb,pt)){ DestroyWindow(h); return 0; }
        RECT pb=composeRect(w);
        if(PtInRect(&pb,pt)){ openComposer(w); return 0; }
        if(pt.y>S(HEADER_H)){ int idx=hitCard(w,pt); if(idx>=0) showDetail(w,idx); }
        return 0; }
    case WM_APP+0x51:{   // composer saved → reload list
        if(w){ w->msgs=loadSavedMsgs(); w->scrollY=0; clampScroll(w);
               InvalidateRect(h,NULL,FALSE); }
        return 0; }
    case WM_MOUSEWHEEL:{
        if(!w) break;
        int delta=GET_WHEEL_DELTA_WPARAM(wp);
        w->scrollY -= (delta/WHEEL_DELTA)*S(48);
        clampScroll(w);
        InvalidateRect(h,NULL,FALSE);
        return 0; }
    case WM_KEYDOWN:
        if(wp==VK_ESCAPE){ DestroyWindow(h); return 0; }
        break;
    case WM_SIZE: if(w){ clampScroll(w); InvalidateRect(h,NULL,FALSE); } return 0;
    case WM_APP_THEME: InvalidateRect(h,NULL,TRUE); return 0;
    case WM_CLOSE: DestroyWindow(h); return 0;
    case WM_DESTROY:
        if(w){ if(g_sw==w) g_sw=nullptr; delete w; }
        SetWindowLongPtrW(h,GWLP_USERDATA,0);
        return 0;
    }
    return DefWindowProcW(h,msg,wp,lp);
}

} // namespace

void SavedMessages_Show(HWND owner){
    // single-instance: focus the existing window if already open
    if(g_sw && IsWindow(g_sw->hwnd)){
        SetForegroundWindow(g_sw->hwnd);
        return;
    }
    static bool reg=false;
    if(!reg){
        WNDCLASSW wc{};
        wc.lpfnWndProc=SavedProc;
        wc.hInstance=g_hInst;
        wc.hCursor=LoadCursorW(NULL,IDC_ARROW);
        wc.lpszClassName=L"AzSavedMsgWin";
        wc.hbrBackground=NULL;
        RegisterClassW(&wc);
        reg=true;
    }
    SavedWin* w=new SavedWin();
    w->owner=owner;
    g_sw=w;

    int wpx=S(560), hpx=S(680);
    RECT pr{0,0,0,0};
    if(owner) GetWindowRect(owner,&pr);
    int px = owner ? pr.left+( (pr.right-pr.left)-wpx )/2 : (int)CW_USEDEFAULT;
    int py = owner ? pr.top +( (pr.bottom-pr.top)-hpx )/2 : (int)CW_USEDEFAULT;

    HWND h=CreateWindowExW(0,L"AzSavedMsgWin",
        L"\u067e\u06cc\u0627\u0645\u200c\u0647\u0627\u06cc \u0630\u062e\u06cc\u0631\u0647\u200c\u0634\u062f\u0647",
        WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_CLIPCHILDREN,
        px,py,wpx,hpx,owner,NULL,g_hInst,w);
    if(!h){ delete w; g_sw=nullptr; return; }
    ShowWindow(h,SW_SHOW);
    UpdateWindow(h);
}
