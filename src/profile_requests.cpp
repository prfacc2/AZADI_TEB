// ============================================================================
//  profile_requests.cpp — RELEASE 1.4.0, §5
//  Admin-side inbox for profile-change requests submitted by reception users.
//  Requests arrive either over the LAN (NetSync_GetJson drains them from the
//  HTTP endpoint or the SMB inbox) or as files already parked in
//  data\profile_requests_inbox\*.json. Each pending request is shown in a
//  ListView; the admin can approve (apply the new name to the user record) or
//  reject (archive the request). Decisions are persisted so they never re-appear.
//
//  JSON shape produced by reception (user_settings.cpp):
//    {"type":"profile_request","user":"<username>","new_name":"<fullname>"}
// ============================================================================
#include "app.h"
#include "ui_kit.h"
#include "net_sync.h"
#include <windows.h>
#include <commctrl.h>
#include <vector>
#include <string>
#include <algorithm>

// setUserFullName(username, fullname) lives in users.cpp (declared in app.h).

namespace {

struct ReqRow {
    std::wstring id;        // file stem (for archive/delete)
    std::wstring user;
    std::wstring newName;
    std::wstring path;      // disk path if file-backed (empty for net-only)
    std::wstring raw;       // raw json
};

struct InboxWin {
    HWND               hwnd;
    HWND               hMain;
    HWND               hList;
    std::vector<ReqRow> rows;
};

#define IDC_PR_LIST     8301
#define IDC_PR_APPROVE  8302
#define IDC_PR_REJECT   8303
#define IDC_PR_REFRESH  8304
#define IDC_PR_CLOSE    8305

// -- tiny JSON value extractor (string fields only, defensive) ---------------
static std::wstring jsonStr(const std::string& js,const char* key){
    std::string pat=std::string("\"")+key+"\"";
    size_t k=js.find(pat);
    if(k==std::string::npos) return L"";
    size_t c=js.find(':',k+pat.size());
    if(c==std::string::npos) return L"";
    size_t q1=js.find('"',c);
    if(q1==std::string::npos) return L"";
    std::string out; size_t i=q1+1;
    for(; i<js.size(); ++i){
        char ch=js[i];
        if(ch=='\\' && i+1<js.size()){ out.push_back(js[++i]); continue; }
        if(ch=='"') break;
        out.push_back(ch);
    }
    int wn=MultiByteToWideChar(CP_UTF8,0,out.data(),(int)out.size(),NULL,0);
    std::wstring w(wn,L'\0');
    MultiByteToWideChar(CP_UTF8,0,out.data(),(int)out.size(),&w[0],wn);
    return w;
}

static std::wstring inboxDir(){
    std::wstring d=dataDir()+L"\\profile_requests_inbox";
    CreateDirectoryW(d.c_str(),NULL);
    return d;
}
static std::wstring archiveDir(){
    std::wstring d=dataDir()+L"\\profile_requests_archive";
    CreateDirectoryW(d.c_str(),NULL);
    return d;
}

static bool writeUtf8(const std::wstring& path,const std::string& bytes){
    HANDLE h=CreateFileW(path.c_str(),GENERIC_WRITE,0,NULL,
        CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
    if(h==INVALID_HANDLE_VALUE) return false;
    DWORD wr=0; BOOL ok=WriteFile(h,bytes.data(),(DWORD)bytes.size(),&wr,NULL);
    CloseHandle(h); return ok && wr==bytes.size();
}
static std::string readUtf8(const std::wstring& path){
    HANDLE h=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,NULL,
        OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    if(h==INVALID_HANDLE_VALUE) return "";
    DWORD sz=GetFileSize(h,NULL); std::string out;
    if(sz!=INVALID_FILE_SIZE && sz>0){ out.resize(sz); DWORD rd=0;
        ReadFile(h,&out[0],sz,&rd,NULL); out.resize(rd); }
    CloseHandle(h); return out;
}

static std::wstring newStem(){
    SYSTEMTIME st; GetLocalTime(&st);
    wchar_t b[64];
    swprintf(b,64,L"req_%04d%02d%02d_%02d%02d%02d_%03d",
        st.wYear,st.wMonth,st.wDay,st.wHour,st.wMinute,st.wSecond,st.wMilliseconds);
    return b;
}

// Drain anything queued by net_sync into the persistent inbox so that we have a
// single source of truth and the request survives a restart.
static void drainNetwork(){
    for(int guard=0; guard<64; ++guard){
        std::string js;
        if(!NetSync_GetJson(L"/api/profile_requests", js)) break;
        if(js.empty()) break;
        if(js.find("profile_request")==std::string::npos) continue; // ignore unrelated
        std::wstring stem=newStem()+L"_"+std::to_wstring(guard);
        writeUtf8(inboxDir()+L"\\"+stem+L".json", js);
    }
}

static void loadRows(InboxWin* iw){
    drainNetwork();
    iw->rows.clear();
    std::wstring dir=inboxDir();
    WIN32_FIND_DATAW fd;
    std::wstring pat=dir+L"\\*.json";
    HANDLE hf=FindFirstFileW(pat.c_str(),&fd);
    if(hf!=INVALID_HANDLE_VALUE){
        do{
            if(fd.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY) continue;
            std::wstring full=dir+L"\\"+fd.cFileName;
            std::string js=readUtf8(full);
            if(js.empty()) continue;
            ReqRow r;
            r.path=full;
            r.id=fd.cFileName;
            r.raw.assign(js.begin(),js.end());
            r.user=jsonStr(js,"user");
            r.newName=jsonStr(js,"new_name");
            if(r.user.empty() && r.newName.empty()) continue;
            iw->rows.push_back(r);
        } while(FindNextFileW(hf,&fd));
        FindClose(hf);
    }
    // newest first
    std::sort(iw->rows.begin(),iw->rows.end(),
        [](const ReqRow&a,const ReqRow&b){ return a.id>b.id; });

    ListView_DeleteAllItems(iw->hList);
    for(int i=0;i<(int)iw->rows.size();i++){
        const ReqRow& r=iw->rows[i];
        LVITEMW it={0}; it.mask=LVIF_TEXT|LVIF_PARAM; it.iItem=i;
        it.pszText=(LPWSTR)r.user.c_str(); it.lParam=i;
        ListView_InsertItem(iw->hList,&it);
        ListView_SetItemText(iw->hList,i,1,(LPWSTR)r.newName.c_str());
        ListView_SetItemText(iw->hList,i,2,(LPWSTR)r.id.c_str());
    }
}

static int selectedRow(InboxWin* iw){
    return ListView_GetNextItem(iw->hList,-1,LVNI_SELECTED);
}

static void archiveRequest(const ReqRow& r,const wchar_t* decision){
    // Move file to archive with a decision marker.
    std::wstring dst=archiveDir()+L"\\"+r.id;
    // append decision into json before move (best effort, keep it simple)
    std::string js(r.raw.begin(),r.raw.end());
    // crude: insert decision field after the opening brace
    size_t br=js.find('{');
    if(br!=std::string::npos){
        char dec[64]; WideCharToMultiByte(CP_UTF8,0,decision,-1,dec,64,NULL,NULL);
        js.insert(br+1, std::string("\"decision\":\"")+dec+"\",");
    }
    writeUtf8(dst,js);
    if(!r.path.empty()) DeleteFileW(r.path.c_str());
}

static void approveSel(InboxWin* iw){
    int s=selectedRow(iw);
    if(s<0 || s>=(int)iw->rows.size()) return;
    ReqRow r=iw->rows[s];
    bool applied=false;
    if(!r.user.empty() && !r.newName.empty())
        applied=setUserFullName(r.user,r.newName);
    archiveRequest(r, applied? L"approved":L"approved_no_user");
    MessageBoxW(iw->hwnd,
        applied? L"\u062f\u0631\u062e\u0648\u0627\u0633\u062a \u062a\u0623\u06cc\u06cc\u062f \u0648 \u0627\u0639\u0645\u0627\u0644 \u0634\u062f."
               : L"\u062f\u0631\u062e\u0648\u0627\u0633\u062a \u062a\u0623\u06cc\u06cc\u062f \u0634\u062f (\u06a9\u0627\u0631\u0628\u0631 \u06cc\u0627\u0641\u062a \u0646\u0634\u062f).",
        L"\u062f\u0631\u062e\u0648\u0627\u0633\u062a \u067e\u0631\u0648\u0641\u0627\u06cc\u0644",MB_OK|MB_ICONINFORMATION);
    loadRows(iw);
}

static void rejectSel(InboxWin* iw){
    int s=selectedRow(iw);
    if(s<0 || s>=(int)iw->rows.size()) return;
    ReqRow r=iw->rows[s];
    int yn=MessageBoxW(iw->hwnd,
        L"\u0622\u06cc\u0627 \u0627\u06cc\u0646 \u062f\u0631\u062e\u0648\u0627\u0633\u062a \u0631\u062f \u0634\u0648\u062f\u061f",
        L"\u0631\u062f \u062f\u0631\u062e\u0648\u0627\u0633\u062a",MB_YESNO|MB_ICONQUESTION);
    if(yn!=IDYES) return;
    archiveRequest(r,L"rejected");
    loadRows(iw);
}

static void layout(InboxWin* iw){
    RECT rc; GetClientRect(iw->hwnd,&rc);
    int pad=S(12), btnY=S(10), btnH=S(30), btnW=S(110);
    int x=rc.right-pad-btnW;
    auto place=[&](int id,int w){
        HWND b=GetDlgItem(iw->hwnd,id);
        if(b) MoveWindow(b,x,btnY,w,btnH,TRUE);
        x-=w+S(6);
    };
    place(IDC_PR_APPROVE,btnW);
    place(IDC_PR_REJECT,btnW);
    place(IDC_PR_REFRESH,S(100));
    HWND bc=GetDlgItem(iw->hwnd,IDC_PR_CLOSE);
    if(bc) MoveWindow(bc,pad,btnY,S(90),btnH,TRUE);

    int top=btnY+btnH+S(10);
    MoveWindow(iw->hList,pad,top,rc.right-2*pad,rc.bottom-top-pad,TRUE);
    int total=rc.right-2*pad-S(20);
    ListView_SetColumnWidth(iw->hList,0,(int)(total*0.32));
    ListView_SetColumnWidth(iw->hList,1,(int)(total*0.40));
    ListView_SetColumnWidth(iw->hList,2,(int)(total*0.28));
}

static LRESULT CALLBACK InboxProc(HWND h,UINT m,WPARAM w,LPARAM l){
    InboxWin* iw=(InboxWin*)GetWindowLongPtrW(h,GWLP_USERDATA);
    switch(m){
    case WM_SIZE: if(iw) layout(iw); return 0;
    case WM_COMMAND: {
        switch(LOWORD(w)){
        case IDC_PR_APPROVE: approveSel(iw); return 0;
        case IDC_PR_REJECT:  rejectSel(iw);  return 0;
        case IDC_PR_REFRESH: loadRows(iw);   return 0;
        case IDC_PR_CLOSE:   DestroyWindow(h); return 0;
        }
        return 0; }
    case WM_NOTIFY: {
        LPNMHDR nh=(LPNMHDR)l;
        if(nh->idFrom==IDC_PR_LIST && nh->code==NM_DBLCLK){ approveSel(iw); return 0; }
        return 0; }
    case WM_CLOSE: DestroyWindow(h); return 0;
    case WM_DESTROY:
        if(iw){ if(iw->hMain){ EnableWindow(iw->hMain,TRUE); SetForegroundWindow(iw->hMain); }
            delete iw; SetWindowLongPtrW(h,GWLP_USERDATA,0); }
        return 0;
    }
    return DefWindowProcW(h,m,w,l);
}

} // anonymous namespace

// ---------------------------------------------------------------- public -----
void OpenProfileRequestsInbox(HWND owner){
    static bool reg=false; const wchar_t* CLS=L"AzProfileReqInbox";
    if(!reg){ WNDCLASSW wc={0}; wc.lpfnWndProc=InboxProc; wc.hInstance=g_hInst;
        wc.hCursor=LoadCursor(NULL,IDC_ARROW); wc.lpszClassName=CLS;
        wc.hbrBackground=g_brBg; wc.style=CS_HREDRAW|CS_VREDRAW;
        RegisterClassW(&wc); reg=true; }

    InboxWin* iw=new InboxWin();
    iw->hMain=owner;

    int W=S(720),H=S(520);
    RECT pr={0,0,0,0};
    if(owner) GetWindowRect(owner,&pr);
    int x=pr.left+((pr.right-pr.left)-W)/2;
    int y=pr.top +((pr.bottom-pr.top)-H)/2;
    if(W<=0||x<0){ x=140; y=90; }

    HWND h=CreateWindowExW(WS_EX_DLGMODALFRAME,CLS,
        L"\u062f\u0631\u062e\u0648\u0627\u0633\u062a\u200c\u0647\u0627\u06cc \u062a\u063a\u06cc\u06cc\u0631 \u067e\u0631\u0648\u0641\u0627\u06cc\u0644",
        WS_POPUP|WS_CAPTION|WS_SYSMENU|WS_SIZEBOX|WS_CLIPCHILDREN,
        x,y,W,H,owner,NULL,g_hInst,NULL);
    iw->hwnd=h;
    SetWindowLongPtrW(h,GWLP_USERDATA,(LONG_PTR)iw);

    createFlatButton(h,IDC_PR_APPROVE,
        L"\u062a\u0623\u06cc\u06cc\u062f",ICO_CHECK,BS_PRIMARY,0,0,10,10);
    createFlatButton(h,IDC_PR_REJECT,
        L"\u0631\u062f",ICO_NONE,BS_DANGER,0,0,10,10);
    createFlatButton(h,IDC_PR_REFRESH,
        L"\u0628\u0627\u0632\u062e\u0648\u0627\u0646\u06cc",ICO_REFRESH,BS_OUTLINE,0,0,10,10);
    createFlatButton(h,IDC_PR_CLOSE,
        L"\u0628\u0633\u062a\u0646",ICO_NONE,BS_OUTLINE,0,0,10,10);

    iw->hList=CreateWindowExW(WS_EX_CLIENTEDGE,WC_LISTVIEWW,L"",
        WS_CHILD|WS_VISIBLE|LVS_REPORT|LVS_SINGLESEL|LVS_SHOWSELALWAYS,
        0,0,10,10,h,(HMENU)(INT_PTR)IDC_PR_LIST,g_hInst,NULL);
    ListView_SetExtendedListViewStyle(iw->hList,
        LVS_EX_FULLROWSELECT|LVS_EX_GRIDLINES);
    {
        LVCOLUMNW c={0}; c.mask=LVCF_TEXT|LVCF_WIDTH|LVCF_SUBITEM;
        struct { const wchar_t* t; int w; } cols[]={
            {L"\u06a9\u0627\u0631\u0628\u0631",200},
            {L"\u0646\u0627\u0645 \u062c\u062f\u06cc\u062f",260},
            {L"\u0634\u0646\u0627\u0633\u0647 \u062f\u0631\u062e\u0648\u0627\u0633\u062a",200},
        };
        for(int i=0;i<3;i++){ c.iSubItem=i; c.pszText=(LPWSTR)cols[i].t; c.cx=cols[i].w;
            ListView_InsertColumn(iw->hList,i,&c); }
    }

    loadRows(iw);
    layout(iw);

    ShowWindow(h,SW_SHOW);
    if(owner) EnableWindow(owner,FALSE);
    SetForegroundWindow(h);
}
