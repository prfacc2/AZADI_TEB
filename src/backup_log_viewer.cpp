// ============================================================================
//  backup_log_viewer.cpp — RELEASE 1.4.0, §7.2
//  A read-only viewer for the ONLY on-disk log channel: backup.log.
//  Reads %LOCALAPPDATA%/AzadiTeb/backup_logs/backup.log (and rotated siblings
//  backup.log.1 .. backup.log.5, plain ones; gzipped ones are skipped because
//  the app does not link a gz reader here) and shows entries newest-first in a
//  ListView. Filter chips switch between همه / موفق / ناموفق. A details pane at
//  the bottom shows the raw payload of the selected row.
//
//  backup_log.cpp keeps its path vars private (static), so we reconstruct the
//  same path independently via SHGetFolderPathW(CSIDL_LOCAL_APPDATA).
// ============================================================================
#include "app.h"
#include "ui_kit.h"
#include "backup_log.h"
#include <windows.h>
#include <commctrl.h>
#include <shlobj.h>
#include <vector>
#include <string>
#include <algorithm>

// ----------------------------------------------------------------- helpers ---
namespace {

struct LogRow {
    std::wstring ts;       // timestamp column
    std::wstring phase;    // phase column
    std::wstring file;     // file column
    std::wstring detail;   // detail column
    std::wstring raw;      // full raw line (for the details pane)
    bool         ok;       // true if phase/detail looks successful
};

struct ViewerWin {
    HWND               hwnd;
    HWND               hMain;
    HWND               hList;
    HWND               hDetail;
    int                filter;        // 0 = all, 1 = ok, 2 = fail
    std::vector<LogRow> all;
    std::vector<int>   shown;         // indices into all
};

#define IDC_BLV_LIST    8201
#define IDC_BLV_DETAIL  8202
#define IDC_BLV_ALL     8210
#define IDC_BLV_OK      8211
#define IDC_BLV_FAIL    8212
#define IDC_BLV_REFRESH 8213
#define IDC_BLV_CLOSE   8214

// Reconstruct %LOCALAPPDATA%\AzadiTeb\backup_logs (mirrors backup_log.cpp).
static std::wstring logDir(){
    wchar_t local[MAX_PATH]={0};
    if(SHGetFolderPathW(NULL,CSIDL_LOCAL_APPDATA,NULL,0,local)!=S_OK) return L"";
    return std::wstring(local)+L"\\AzadiTeb\\backup_logs";
}

// Read a whole (small) text file as UTF-8 → wstring.
static std::wstring readFileUtf8(const std::wstring& path){
    HANDLE hf=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,
        NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    if(hf==INVALID_HANDLE_VALUE) return L"";
    std::string bytes;
    char buf[65536]; DWORD got=0;
    while(ReadFile(hf,buf,sizeof(buf),&got,NULL) && got>0)
        bytes.append(buf,got);
    CloseHandle(hf);
    if(bytes.empty()) return L"";
    int wn=MultiByteToWideChar(CP_UTF8,0,bytes.data(),(int)bytes.size(),NULL,0);
    std::wstring w(wn,L'\0');
    MultiByteToWideChar(CP_UTF8,0,bytes.data(),(int)bytes.size(),&w[0],wn);
    return w;
}

static std::wstring trim(const std::wstring& s){
    size_t a=s.find_first_not_of(L" \t\r\n");
    if(a==std::wstring::npos) return L"";
    size_t b=s.find_last_not_of(L" \t\r\n");
    return s.substr(a,b-a+1);
}

// A "field" is delimited by '\t' or '|' inside a log line, depending on how the
// emitter wrote it. We parse defensively: pick the first ISO-like token as the
// timestamp, the first ALL_CAPS_WITH_UNDERSCORES token as the phase, and treat
// the rest as detail. The full line is always retained as raw.
static LogRow parseLine(const std::wstring& line){
    LogRow r; r.raw=line; r.ok=true;
    // Split on tab first; fall back to pipe.
    std::vector<std::wstring> cols;
    {
        std::wstring cur; wchar_t sep = (line.find(L'\t')!=std::wstring::npos)?L'\t':L'|';
        for(wchar_t c : line){
            if(c==sep){ cols.push_back(cur); cur.clear(); }
            else cur.push_back(c);
        }
        cols.push_back(cur);
    }
    for(auto& c : cols) c=trim(c);
    // Heuristic column assignment.
    for(auto& c : cols){
        if(r.ts.empty() && c.size()>=10 &&
           (c[4]==L'-'||c[4]==L'/') && iswdigit(c[0])) { r.ts=c; continue; }
        bool caps = !c.empty();
        for(wchar_t ch : c) if(!(iswupper(ch)||ch==L'_'||iswdigit(ch))) { caps=false; break; }
        if(r.phase.empty() && caps && c.size()>=4){ r.phase=c; continue; }
        if(r.file.empty() && (c.find(L'\\')!=std::wstring::npos ||
                              c.find(L".azb")!=std::wstring::npos)) { r.file=c; continue; }
        if(!c.empty()){ if(!r.detail.empty()) r.detail+=L"  "; r.detail+=c; }
    }
    if(r.ts.empty()){ // first 19 chars fallback
        if(line.size()>=19 && iswdigit(line[0])) r.ts=line.substr(0,19);
    }
    if(r.phase.empty()) r.phase=L"-";
    // ok/fail classification.
    std::wstring up=r.phase; for(auto& ch:up) ch=towupper(ch);
    std::wstring du=r.detail; for(auto& ch:du) ch=towupper(ch);
    if(up.find(L"FAIL")!=std::wstring::npos || up.find(L"ERROR")!=std::wstring::npos ||
       du.find(L"FAIL")!=std::wstring::npos || du.find(L"ERROR")!=std::wstring::npos ||
       du.find(L"EXCEPTION")!=std::wstring::npos)
        r.ok=false;
    return r;
}

// Load all plain (non-gz) backup logs newest-first.
static void loadRows(ViewerWin* vw){
    vw->all.clear();
    std::wstring dir=logDir();
    if(dir.empty()) return;
    // backup.log is newest; rotated .1 .. .5 are older.
    std::vector<std::wstring> files;
    files.push_back(dir+L"\\backup.log");
    for(int i=1;i<=5;i++) files.push_back(dir+L"\\backup.log."+std::to_wstring(i));
    // We want newest entries first overall. backup.log holds the most recent
    // lines (append order = chronological); read each file, reverse its lines,
    // and process files in current→older order.
    for(const auto& f : files){
        std::wstring data=readFileUtf8(f);
        if(data.empty()) continue;
        std::vector<std::wstring> lines;
        std::wstring cur;
        for(wchar_t c : data){
            if(c==L'\n'){ lines.push_back(cur); cur.clear(); }
            else if(c!=L'\r') cur.push_back(c);
        }
        if(!cur.empty()) lines.push_back(cur);
        for(auto it=lines.rbegin(); it!=lines.rend(); ++it){
            std::wstring t=trim(*it);
            if(t.empty()) continue;
            vw->all.push_back(parseLine(t));
        }
    }
}

static void applyFilter(ViewerWin* vw){
    vw->shown.clear();
    for(int i=0;i<(int)vw->all.size();i++){
        const LogRow& r=vw->all[i];
        if(vw->filter==1 && !r.ok) continue;
        if(vw->filter==2 && r.ok)  continue;
        vw->shown.push_back(i);
    }
    ListView_DeleteAllItems(vw->hList);
    for(int row=0; row<(int)vw->shown.size(); row++){
        const LogRow& r=vw->all[vw->shown[row]];
        LVITEMW it={0}; it.mask=LVIF_TEXT|LVIF_PARAM; it.iItem=row;
        it.pszText=(LPWSTR)r.ts.c_str(); it.lParam=row;
        ListView_InsertItem(vw->hList,&it);
        ListView_SetItemText(vw->hList,row,1,(LPWSTR)(r.ok?L"\u0645\u0648\u0641\u0642":L"\u0646\u0627\u0645\u0648\u0641\u0642"));
        ListView_SetItemText(vw->hList,row,2,(LPWSTR)r.phase.c_str());
        ListView_SetItemText(vw->hList,row,3,(LPWSTR)r.detail.c_str());
    }
    if(vw->shown.empty())
        SetWindowTextW(vw->hDetail,
            L"\u0647\u06cc\u0686 \u0631\u06a9\u0648\u0631\u062f\u06cc \u0628\u0631\u0627\u06cc \u0646\u0645\u0627\u06cc\u0634 \u0648\u062c\u0648\u062f \u0646\u062f\u0627\u0631\u062f.");
}

static void layout(ViewerWin* vw){
    RECT rc; GetClientRect(vw->hwnd,&rc);
    int pad=S(12), chipY=S(10), chipH=S(30), chipW=S(96);
    // Chips along the top (RTL: from right).
    int x=rc.right-pad-chipW;
    auto place=[&](int id){
        HWND b=GetDlgItem(vw->hwnd,id);
        if(b) MoveWindow(b,x,chipY,chipW,chipH,TRUE);
        x-=chipW+S(6);
    };
    place(IDC_BLV_ALL); place(IDC_BLV_OK); place(IDC_BLV_FAIL);
    // Close + refresh on the left.
    HWND bc=GetDlgItem(vw->hwnd,IDC_BLV_CLOSE);
    HWND br=GetDlgItem(vw->hwnd,IDC_BLV_REFRESH);
    if(bc) MoveWindow(bc,pad,chipY,chipW,chipH,TRUE);
    if(br) MoveWindow(br,pad+chipW+S(6),chipY,chipW,chipH,TRUE);

    int top=chipY+chipH+S(10);
    int detailH=S(150);
    int listH=rc.bottom-top-detailH-S(20);
    if(listH<S(80)) listH=S(80);
    MoveWindow(vw->hList,pad,top,rc.right-2*pad,listH,TRUE);
    MoveWindow(vw->hDetail,pad,top+listH+S(8),rc.right-2*pad,detailH,TRUE);
    // Auto-size last column.
    int total=rc.right-2*pad-S(20);
    ListView_SetColumnWidth(vw->hList,0,(int)(total*0.22));
    ListView_SetColumnWidth(vw->hList,1,(int)(total*0.12));
    ListView_SetColumnWidth(vw->hList,2,(int)(total*0.22));
    ListView_SetColumnWidth(vw->hList,3,(int)(total*0.44));
}

static LRESULT CALLBACK ViewerProc(HWND h,UINT m,WPARAM w,LPARAM l){
    ViewerWin* vw=(ViewerWin*)GetWindowLongPtrW(h,GWLP_USERDATA);
    switch(m){
    case WM_CREATE: return 0;
    case WM_SIZE: if(vw) layout(vw); return 0;
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLOREDIT: {
        HDC dc=(HDC)w; SetBkColor(dc,g_theme.surface);
        SetTextColor(dc,g_theme.text); return (LRESULT)g_brSurface; }
    case WM_NOTIFY: {
        LPNMHDR nh=(LPNMHDR)l;
        if(nh->idFrom==IDC_BLV_LIST && nh->code==LVN_ITEMCHANGED){
            int sel=ListView_GetNextItem(vw->hList,-1,LVNI_SELECTED);
            if(sel>=0 && sel<(int)vw->shown.size())
                SetWindowTextW(vw->hDetail,vw->all[vw->shown[sel]].raw.c_str());
        }
        return 0; }
    case WM_COMMAND: {
        int id=LOWORD(w);
        switch(id){
        case IDC_BLV_ALL:  vw->filter=0; applyFilter(vw); return 0;
        case IDC_BLV_OK:   vw->filter=1; applyFilter(vw); return 0;
        case IDC_BLV_FAIL: vw->filter=2; applyFilter(vw); return 0;
        case IDC_BLV_REFRESH: loadRows(vw); applyFilter(vw); return 0;
        case IDC_BLV_CLOSE: DestroyWindow(h); return 0;
        }
        return 0; }
    case WM_CLOSE: DestroyWindow(h); return 0;
    case WM_DESTROY:
        if(vw){ if(vw->hMain){ EnableWindow(vw->hMain,TRUE); SetForegroundWindow(vw->hMain); }
            delete vw; SetWindowLongPtrW(h,GWLP_USERDATA,0); }
        return 0;
    }
    return DefWindowProcW(h,m,w,l);
}

} // anonymous namespace

// ---------------------------------------------------------------- public -----
void OpenBackupLogViewer(HWND owner){
    static bool reg=false; const wchar_t* CLS=L"AzBackupLogViewer";
    if(!reg){ WNDCLASSW wc={0}; wc.lpfnWndProc=ViewerProc; wc.hInstance=g_hInst;
        wc.hCursor=LoadCursor(NULL,IDC_ARROW); wc.lpszClassName=CLS;
        wc.hbrBackground=g_brBg; wc.style=CS_HREDRAW|CS_VREDRAW;
        RegisterClassW(&wc); reg=true; }

    ViewerWin* vw=new ViewerWin();
    vw->hMain=owner; vw->filter=0;

    int W=S(900),H=S(620);
    RECT pr={0,0,0,0};
    if(owner) GetWindowRect(owner,&pr);
    int x=pr.left+((pr.right-pr.left)-W)/2;
    int y=pr.top +((pr.bottom-pr.top)-H)/2;
    if(W<=0||x<0){ x=120; y=80; }

    HWND h=CreateWindowExW(WS_EX_DLGMODALFRAME,CLS,
        L"\u0644\u0627\u06af\u200c\u0647\u0627\u06cc \u067e\u0634\u062a\u06cc\u0628\u0627\u0646\u200c\u06af\u06cc\u0631\u06cc (backup.log)",
        WS_POPUP|WS_CAPTION|WS_SYSMENU|WS_SIZEBOX|WS_CLIPCHILDREN,
        x,y,W,H,owner,NULL,g_hInst,NULL);
    vw->hwnd=h;
    SetWindowLongPtrW(h,GWLP_USERDATA,(LONG_PTR)vw);

    // Filter chips + actions.
    createFlatButton(h,IDC_BLV_ALL,
        L"\u0647\u0645\u0647",ICO_NONE,BS_PRIMARY,0,0,10,10);
    createFlatButton(h,IDC_BLV_OK,
        L"\u0645\u0648\u0641\u0642",ICO_NONE,BS_OUTLINE,0,0,10,10);
    createFlatButton(h,IDC_BLV_FAIL,
        L"\u0646\u0627\u0645\u0648\u0641\u0642",ICO_NONE,BS_OUTLINE,0,0,10,10);
    createFlatButton(h,IDC_BLV_REFRESH,
        L"\u0628\u0627\u0632\u062e\u0648\u0627\u0646\u06cc",ICO_REFRESH,BS_OUTLINE,0,0,10,10);
    createFlatButton(h,IDC_BLV_CLOSE,
        L"\u0628\u0633\u062a\u0646",ICO_NONE,BS_OUTLINE,0,0,10,10);

    // ListView.
    vw->hList=CreateWindowExW(WS_EX_CLIENTEDGE,WC_LISTVIEWW,L"",
        WS_CHILD|WS_VISIBLE|LVS_REPORT|LVS_SINGLESEL|LVS_SHOWSELALWAYS,
        0,0,10,10,h,(HMENU)(INT_PTR)IDC_BLV_LIST,g_hInst,NULL);
    ListView_SetExtendedListViewStyle(vw->hList,
        LVS_EX_FULLROWSELECT|LVS_EX_GRIDLINES);
    {
        LVCOLUMNW c={0}; c.mask=LVCF_TEXT|LVCF_WIDTH|LVCF_SUBITEM;
        struct { const wchar_t* t; int w; } cols[]={
            {L"\u0632\u0645\u0627\u0646",160},
            {L"\u0648\u0636\u0639\u06cc\u062a",90},
            {L"\u0645\u0631\u062d\u0644\u0647",160},
            {L"\u062c\u0632\u0626\u06cc\u0627\u062a",380},
        };
        for(int i=0;i<4;i++){ c.iSubItem=i; c.pszText=(LPWSTR)cols[i].t; c.cx=cols[i].w;
            ListView_InsertColumn(vw->hList,i,&c); }
    }

    // Details pane.
    vw->hDetail=CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",L"",
        WS_CHILD|WS_VISIBLE|ES_MULTILINE|ES_READONLY|ES_AUTOVSCROLL|WS_VSCROLL,
        0,0,10,10,h,(HMENU)(INT_PTR)IDC_BLV_DETAIL,g_hInst,NULL);

    loadRows(vw);
    applyFilter(vw);
    layout(vw);

    ShowWindow(h,SW_SHOW);
    if(owner) EnableWindow(owner,FALSE);
    SetForegroundWindow(h);
}
