// ============================================================================
//  user_settings.cpp — two-tier settings windows (release 1.4.0, §1).
//  Reception users get a 7-item settings window; admins get a 16-item
//  management settings window. A shared left-nav + right-panel framework backs
//  both. Fully-implemented panels: profile, theme, printer, save-messages,
//  notifications, about, logout (reception); plus management profile, global
//  theme, print designer, restore design, global printer, profile-requests
//  inbox, backup-log viewer. Other management panels are "coming soon" stubs
//  but present in the nav so navigation never regresses.
// ============================================================================
#include "app.h"
#include "ui_kit.h"
#include "sections.h"
#include "print_designer.h"
#include "net_sync.h"
#include "backup_log.h"
#include <objbase.h>
#include <vector>
#include <string>

void OpenProfileRequestsInbox(HWND owner);   // §5 (profile_requests.cpp)
void OpenBackupLogViewer(HWND owner);        // §7.2 (backup_log_viewer.cpp)

// ---------------------------------------------------------------- framework --
namespace {

struct NavItem {
    std::wstring label;
    int          panelId;      // identifies which panel to build
};

// §A: the three audiences the settings window can serve. Kept as an explicit
// mode (not just isAdmin) so adding future tiers never silently regresses the
// guest contract (guest = theme + contact-us ONLY).
enum SettingsMode { SM_GUEST=0, SM_RECEPTION=1, SM_ADMIN=2 };

struct SettingsWin {
    User                  user;
    bool                  isAdmin;
    int                   mode;         // SettingsMode
    std::vector<NavItem>  nav;
    int                   active;       // index in nav
    int                   navW;
    HWND                  hwnd;
    std::vector<HWND>     panelCtrls;   // controls of the active panel
    HWND                  hMain;
};

#define IDC_NAV_BASE   7000
#define IDC_PANEL_BASE 7100

static void destroyPanelControls(SettingsWin* sw){
    for(HWND c : sw->panelCtrls) if(c && IsWindow(c)) DestroyWindow(c);
    sw->panelCtrls.clear();
}

static RECT panelRect(SettingsWin* sw){
    RECT rc; GetClientRect(sw->hwnd,&rc);
    RECT p={0,0,rc.right-sw->navW,rc.bottom};   // right panel (RTL: panel on left)
    // RTL: nav on the right, panel on the left
    p.left=0; p.right=rc.right-sw->navW;
    p.top=S(8); p.bottom=rc.bottom-S(8);
    return p;
}

// ------------------------------------------------------------- panel builders
static void buildPanel(SettingsWin* sw);

// Profile panel (reception: queues a request; admin: writes directly)
static void buildProfilePanel(SettingsWin* sw, bool admin){
    RECT p=panelRect(sw);
    int x=p.left+S(16), y=p.top+S(50), w=p.right-p.left-S(32);
    HWND title=CreateWindowExW(0,L"STATIC",
        admin? L"\u067e\u0631\u0648\u0641\u0627\u06cc\u0644 \u0645\u062f\u06cc\u0631\u06cc\u062a"
             : L"\u067e\u0631\u0648\u0641\u0627\u06cc\u0644 \u0645\u0646",
        WS_CHILD|WS_VISIBLE|SS_RIGHT,x,p.top+S(8),w,S(28),sw->hwnd,NULL,g_hInst,NULL);
    SendMessageW(title,WM_SETFONT,(WPARAM)g_fTitle,TRUE); sw->panelCtrls.push_back(title);

    HWND capName=CreateWindowExW(0,L"STATIC",
        L"\u0646\u0627\u0645 \u062c\u062f\u06cc\u062f",WS_CHILD|WS_VISIBLE|SS_RIGHT,
        x,y,w,S(18),sw->hwnd,NULL,g_hInst,NULL);
    SendMessageW(capName,WM_SETFONT,(WPARAM)g_fSmall,TRUE); sw->panelCtrls.push_back(capName);
    HWND eName=CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",sw->user.fullname.c_str(),
        WS_CHILD|WS_VISIBLE|ES_AUTOHSCROLL,x,y+S(20),w,S(28),sw->hwnd,
        (HMENU)(INT_PTR)(IDC_PANEL_BASE+1),g_hInst,NULL);
    SendMessageW(eName,WM_SETFONT,(WPARAM)g_fUI,TRUE); sw->panelCtrls.push_back(eName);

    HWND bPhoto=createFlatButton(sw->hwnd,IDC_PANEL_BASE+2,
        L"\u062a\u063a\u06cc\u06cc\u0631 \u0639\u06a9\u0633",ICO_USER,BS_OUTLINE,
        x,y+S(56),w/2-S(4),S(30)); sw->panelCtrls.push_back(bPhoto);
    HWND bDl=createFlatButton(sw->hwnd,IDC_PANEL_BASE+3,
        L"\u062f\u0627\u0646\u0644\u0648\u062f \u0639\u06a9\u0633 \u0641\u0639\u0644\u06cc",ICO_SAVE,BS_OUTLINE,
        x+w/2+S(4),y+S(56),w/2-S(4),S(30)); sw->panelCtrls.push_back(bDl);

    HWND bSubmit=createFlatButton(sw->hwnd,IDC_PANEL_BASE+4,
        admin? L"\u0630\u062e\u06cc\u0631\u0647 \u062a\u063a\u06cc\u06cc\u0631\u0627\u062a"
             : L"\u062b\u0628\u062a \u062f\u0631\u062e\u0648\u0627\u0633\u062a",
        ICO_CHECK,BS_PRIMARY,x,y+S(96),w,S(34)); sw->panelCtrls.push_back(bSubmit);

    HWND hint=CreateWindowExW(0,L"STATIC",
        admin? L"\u062a\u063a\u06cc\u06cc\u0631\u0627\u062a \u0645\u062f\u06cc\u0631 \u0628\u062f\u0648\u0646 \u062a\u0627\u06cc\u06cc\u062f \u0627\u0639\u0645\u0627\u0644 \u0645\u06cc\u200c\u0634\u0648\u062f."
             : L"\u062f\u0631\u062e\u0648\u0627\u0633\u062a \u0628\u0631\u0627\u06cc \u0645\u062f\u06cc\u0631\u06cc\u062a \u0627\u0631\u0633\u0627\u0644 \u0645\u06cc\u200c\u0634\u0648\u062f.",
        WS_CHILD|WS_VISIBLE|SS_RIGHT,x,y+S(136),w,S(20),sw->hwnd,NULL,g_hInst,NULL);
    SendMessageW(hint,WM_SETFONT,(WPARAM)g_fSmall,TRUE); sw->panelCtrls.push_back(hint);
}

static void buildThemePanel(SettingsWin* sw, bool global){
    RECT p=panelRect(sw);
    int x=p.left+S(16), y=p.top+S(50), w=p.right-p.left-S(32);
    HWND title=CreateWindowExW(0,L"STATIC",
        global? L"\u062a\u063a\u06cc\u06cc\u0631 \u067e\u0648\u0633\u062a\u0647 (\u0633\u0631\u0627\u0633\u0631\u06cc)"
              : L"\u062a\u063a\u06cc\u06cc\u0631 \u067e\u0648\u0633\u062a\u0647",
        WS_CHILD|WS_VISIBLE|SS_RIGHT,x,p.top+S(8),w,S(28),sw->hwnd,NULL,g_hInst,NULL);
    SendMessageW(title,WM_SETFONT,(WPARAM)g_fTitle,TRUE); sw->panelCtrls.push_back(title);
    const wchar_t* names[4]={
        L"\u0631\u0648\u0634\u0646",L"\u062a\u06cc\u0631\u0647",
        L"\u0633\u0648\u0644\u0627\u0631\u06cc\u0632\u0647",L"\u06a9\u0646\u062a\u0631\u0627\u0633\u062a \u0628\u0627\u0644\u0627" };
    for(int i=0;i<4;i++){
        HWND b=createFlatButton(sw->hwnd,IDC_PANEL_BASE+10+i,names[i],ICO_MOON,
            BS_OUTLINE,x+(i%2)*(w/2),y+(i/2)*S(44),w/2-S(8),S(38));
        sw->panelCtrls.push_back(b);
    }
}

// Printer panel (reception per-user, or admin global default)
static void buildPrinterPanel(SettingsWin* sw, bool global){
    RECT p=panelRect(sw);
    int x=p.left+S(16), y=p.top+S(50), w=p.right-p.left-S(32);
    HWND title=CreateWindowExW(0,L"STATIC",
        global? L"\u062a\u0646\u0638\u06cc\u0645\u0627\u062a \u0686\u0627\u067e\u06af\u0631 \u0633\u0631\u0627\u0633\u0631\u06cc"
              : L"\u0627\u0646\u062a\u062e\u0627\u0628 \u0686\u0627\u067e\u06af\u0631",
        WS_CHILD|WS_VISIBLE|SS_RIGHT,x,p.top+S(8),w,S(28),sw->hwnd,NULL,g_hInst,NULL);
    SendMessageW(title,WM_SETFONT,(WPARAM)g_fTitle,TRUE); sw->panelCtrls.push_back(title);

    HWND capP=CreateWindowExW(0,L"STATIC",L"\u0686\u0627\u067e\u06af\u0631",
        WS_CHILD|WS_VISIBLE|SS_RIGHT,x,y,w,S(18),sw->hwnd,NULL,g_hInst,NULL);
    SendMessageW(capP,WM_SETFONT,(WPARAM)g_fSmall,TRUE); sw->panelCtrls.push_back(capP);
    HWND cbP=CreateWindowExW(0,L"COMBOBOX",L"",
        WS_CHILD|WS_VISIBLE|CBS_DROPDOWNLIST|WS_VSCROLL,x,y+S(20),w,S(220),sw->hwnd,
        (HMENU)(INT_PTR)(IDC_PANEL_BASE+20),g_hInst,NULL);
    SendMessageW(cbP,WM_SETFONT,(WPARAM)g_fUI,TRUE); sw->panelCtrls.push_back(cbP);
    // enumerate printers
    DWORD needed=0,returned=0;
    EnumPrintersW(PRINTER_ENUM_LOCAL|PRINTER_ENUM_CONNECTIONS,NULL,2,NULL,0,&needed,&returned);
    if(needed){
        std::vector<BYTE> buf(needed);
        if(EnumPrintersW(PRINTER_ENUM_LOCAL|PRINTER_ENUM_CONNECTIONS,NULL,2,
                         buf.data(),needed,&needed,&returned)){
            PRINTER_INFO_2W* pi=(PRINTER_INFO_2W*)buf.data();
            for(DWORD i=0;i<returned;i++)
                SendMessageW(cbP,CB_ADDSTRING,0,(LPARAM)pi[i].pPrinterName);
        }
    }
    SendMessageW(cbP,CB_SETCURSEL,0,0);

    HWND capS=CreateWindowExW(0,L"STATIC",L"\u0627\u0646\u062f\u0627\u0632\u0647 \u0635\u0641\u062d\u0647",
        WS_CHILD|WS_VISIBLE|SS_RIGHT,x,y+S(54),w,S(18),sw->hwnd,NULL,g_hInst,NULL);
    SendMessageW(capS,WM_SETFONT,(WPARAM)g_fSmall,TRUE); sw->panelCtrls.push_back(capS);
    HWND cbS=CreateWindowExW(0,L"COMBOBOX",L"",
        WS_CHILD|WS_VISIBLE|CBS_DROPDOWNLIST|WS_VSCROLL,x,y+S(74),w,S(220),sw->hwnd,
        (HMENU)(INT_PTR)(IDC_PANEL_BASE+21),g_hInst,NULL);
    SendMessageW(cbS,WM_SETFONT,(WPARAM)g_fUI,TRUE); sw->panelCtrls.push_back(cbS);
    const wchar_t* sizes[]={L"A4",L"A5",L"A6",L"B5",L"Letter",
        L"\u0633\u0641\u0627\u0631\u0634\u06cc\u2026"};
    for(auto s:sizes) SendMessageW(cbS,CB_ADDSTRING,0,(LPARAM)s);
    SendMessageW(cbS,CB_SETCURSEL,1,0);

    HWND capO=CreateWindowExW(0,L"STATIC",L"\u062c\u0647\u062a",
        WS_CHILD|WS_VISIBLE|SS_RIGHT,x,y+S(108),w,S(18),sw->hwnd,NULL,g_hInst,NULL);
    SendMessageW(capO,WM_SETFONT,(WPARAM)g_fSmall,TRUE); sw->panelCtrls.push_back(capO);
    HWND cbO=CreateWindowExW(0,L"COMBOBOX",L"",
        WS_CHILD|WS_VISIBLE|CBS_DROPDOWNLIST,x,y+S(128),w,S(120),sw->hwnd,
        (HMENU)(INT_PTR)(IDC_PANEL_BASE+22),g_hInst,NULL);
    SendMessageW(cbO,WM_SETFONT,(WPARAM)g_fUI,TRUE); sw->panelCtrls.push_back(cbO);
    SendMessageW(cbO,CB_ADDSTRING,0,(LPARAM)L"\u0639\u0645\u0648\u062f\u06cc");
    SendMessageW(cbO,CB_ADDSTRING,0,(LPARAM)L"\u0627\u0641\u0642\u06cc");
    SendMessageW(cbO,CB_SETCURSEL,0,0);

    HWND bPrev=createFlatButton(sw->hwnd,IDC_PANEL_BASE+23,
        L"\u0646\u0645\u0627\u06cc\u0634 \u067e\u06cc\u0634\u200c\u0646\u0645\u0627\u06cc\u0634",ICO_RECEIPT,BS_OUTLINE,
        x,y+S(162),w/2-S(4),S(32)); sw->panelCtrls.push_back(bPrev);
    HWND bTest=createFlatButton(sw->hwnd,IDC_PANEL_BASE+24,
        L"\u062a\u0633\u062a \u0686\u0627\u067e",ICO_PRINT,BS_PRIMARY,
        x+w/2+S(4),y+S(162),w/2-S(4),S(32)); sw->panelCtrls.push_back(bTest);
}

// switch row helper
static HWND addSwitchRow(SettingsWin* sw,int& y,int x,int w,const wchar_t* label,
                         int id,bool on){
    HWND cap=CreateWindowExW(0,L"STATIC",label,WS_CHILD|WS_VISIBLE|SS_RIGHT,
        x+S(56),y,w-S(56),S(24),sw->hwnd,NULL,g_hInst,NULL);
    SendMessageW(cap,WM_SETFONT,(WPARAM)g_fUI,TRUE); sw->panelCtrls.push_back(cap);
    HWND s=uikit::AzSwitch_Create(sw->hwnd,id,on,x,y,S(48),S(24));
    sw->panelCtrls.push_back(s);
    y+=S(32);
    return s;
}

static void buildSaveMsgsPanel(SettingsWin* sw){
    RECT p=panelRect(sw);
    int x=p.left+S(16), y=p.top+S(50), w=p.right-p.left-S(32);
    HWND title=CreateWindowExW(0,L"STATIC",
        L"\u067e\u06cc\u0627\u0645\u200c\u0647\u0627\u06cc \u0630\u062e\u06cc\u0631\u0647",
        WS_CHILD|WS_VISIBLE|SS_RIGHT,x,p.top+S(8),w,S(28),sw->hwnd,NULL,g_hInst,NULL);
    SendMessageW(title,WM_SETFONT,(WPARAM)g_fTitle,TRUE); sw->panelCtrls.push_back(title);
    // §D: master toggle — enable the «پیام‌های ذخیره‌شده» (archive) feature so
    // the cartable archive view becomes available. This was previously only
    // settable via a raw setting; surfacing it here makes the feature reachable.
    addSwitchRow(sw,y,x,w,L"\u0641\u0639\u0627\u0644\u200c\u0633\u0627\u0632\u06cc \u067e\u06cc\u0627\u0645\u200c\u0647\u0627\u06cc \u0630\u062e\u06cc\u0631\u0647\u200c\u0634\u062f\u0647",IDC_PANEL_BASE+34,getSetting(L"saved_msgs_enabled",L"0")==L"1");
    addSwitchRow(sw,y,x,w,L"\u0646\u0645\u0627\u06cc\u0634 \u067e\u06cc\u0627\u0645 \u067e\u0633 \u0627\u0632 \u0630\u062e\u06cc\u0631\u0647 \u0645\u0648\u0641\u0642",IDC_PANEL_BASE+30,getSetting(L"msg.save_ok",L"1")==L"1");
    addSwitchRow(sw,y,x,w,L"\u0646\u0645\u0627\u06cc\u0634 \u067e\u06cc\u0627\u0645 \u067e\u0633 \u0627\u0632 \u0630\u062e\u06cc\u0631\u0647 \u0646\u0627\u0645\u0648\u0641\u0642 \u0628\u0627 \u062c\u0632\u0626\u06cc\u0627\u062a",IDC_PANEL_BASE+31,getSetting(L"msg.save_fail",L"1")==L"1");
    addSwitchRow(sw,y,x,w,L"\u0646\u0645\u0627\u06cc\u0634 \u067e\u06cc\u0627\u0645 \u067e\u0633 \u0627\u0632 \u0686\u0627\u067e \u0645\u0648\u0641\u0642",IDC_PANEL_BASE+32,getSetting(L"msg.print_ok",L"1")==L"1");
    addSwitchRow(sw,y,x,w,L"\u0646\u0645\u0627\u06cc\u0634 \u067e\u06cc\u0627\u0645 \u067e\u0633 \u0627\u0632 \u0686\u0627\u067e \u0646\u0627\u0645\u0648\u0641\u0642",IDC_PANEL_BASE+33,getSetting(L"msg.print_fail",L"1")==L"1");
}

static void buildNotifyPanel(SettingsWin* sw){
    RECT p=panelRect(sw);
    int x=p.left+S(16), y=p.top+S(50), w=p.right-p.left-S(32);
    HWND title=CreateWindowExW(0,L"STATIC",
        L"\u062a\u0646\u0638\u06cc\u0645\u0627\u062a \u0627\u0639\u0644\u0627\u0646",
        WS_CHILD|WS_VISIBLE|SS_RIGHT,x,p.top+S(8),w,S(28),sw->hwnd,NULL,g_hInst,NULL);
    SendMessageW(title,WM_SETFONT,(WPARAM)g_fTitle,TRUE); sw->panelCtrls.push_back(title);
    addSwitchRow(sw,y,x,w,L"\u0627\u0639\u0644\u0627\u0646 \u0646\u0648\u0628\u062a \u062c\u062f\u06cc\u062f",IDC_PANEL_BASE+40,getSetting(L"notif.newappt",L"1")==L"1");
    addSwitchRow(sw,y,x,w,L"\u0627\u0639\u0644\u0627\u0646 \u0634\u06a9\u0633\u062a \u0686\u0627\u067e",IDC_PANEL_BASE+41,getSetting(L"notif.printfail",L"1")==L"1");
    addSwitchRow(sw,y,x,w,L"\u0635\u062f\u0627 \u0628\u0631\u0627\u06cc \u0631\u0648\u06cc\u062f\u0627\u062f\u0647\u0627\u06cc \u0645\u0647\u0645",IDC_PANEL_BASE+42,getSetting(L"notif.sound",L"1")==L"1");
    HWND capR=CreateWindowExW(0,L"STATIC",
        L"\u06cc\u0627\u062f\u0622\u0648\u0631 \u067e\u06cc\u0634 \u0627\u0632 \u0646\u0648\u0628\u062a",
        WS_CHILD|WS_VISIBLE|SS_RIGHT,x,y+S(4),w,S(18),sw->hwnd,NULL,g_hInst,NULL);
    SendMessageW(capR,WM_SETFONT,(WPARAM)g_fSmall,TRUE); sw->panelCtrls.push_back(capR);
    HWND cb=CreateWindowExW(0,L"COMBOBOX",L"",WS_CHILD|WS_VISIBLE|CBS_DROPDOWNLIST,
        x,y+S(24),w,S(160),sw->hwnd,(HMENU)(INT_PTR)(IDC_PANEL_BASE+43),g_hInst,NULL);
    SendMessageW(cb,WM_SETFONT,(WPARAM)g_fUI,TRUE); sw->panelCtrls.push_back(cb);
    const wchar_t* opts[]={L"\u06f5 \u062f\u0642\u06cc\u0642\u0647",L"\u06f1\u06f0 \u062f\u0642\u06cc\u0642\u0647",
        L"\u06f1\u06f5 \u062f\u0642\u06cc\u0642\u0647",L"\u06f3\u06f0 \u062f\u0642\u06cc\u0642\u0647",L"\u062e\u0627\u0645\u0648\u0634"};
    for(auto o:opts) SendMessageW(cb,CB_ADDSTRING,0,(LPARAM)o);
    SendMessageW(cb,CB_SETCURSEL,1,0);
}

static void buildAboutPanel(SettingsWin* sw){
    RECT p=panelRect(sw);
    int x=p.left+S(16), y=p.top+S(50), w=p.right-p.left-S(32);
    HWND title=CreateWindowExW(0,L"STATIC",
        L"\u062f\u0631\u0628\u0627\u0631\u0647 \u0628\u0631\u0646\u0627\u0645\u0647",
        WS_CHILD|WS_VISIBLE|SS_RIGHT,x,p.top+S(8),w,S(28),sw->hwnd,NULL,g_hInst,NULL);
    SendMessageW(title,WM_SETFONT,(WPARAM)g_fTitle,TRUE); sw->panelCtrls.push_back(title);
    std::wstring body=L"\u0622\u0632\u0627\u062f\u06cc \u0637\u0628 \u2014 \u0646\u0633\u062e\u0647 ";
    body+=APP_VERSION_W; body+=L"\r\n";
    body+=L"\u0633\u0627\u0645\u0627\u0646\u0647 \u067e\u0630\u06cc\u0631\u0634 \u0648 \u0645\u062f\u06cc\u0631\u06cc\u062a \u062f\u0631\u0645\u0627\u0646\u06af\u0627\u0647\r\n";
    body+=L"\u0645\u062c\u0648\u0632: \u0627\u062e\u062a\u0635\u0627\u0635\u06cc \u00a9 Azadi-Teb\r\n";
    OSVERSIONINFOEXW os; ZeroMemory(&os,sizeof(os)); os.dwOSVersionInfoSize=sizeof(os);
    HWND b=CreateWindowExW(0,L"STATIC",body.c_str(),WS_CHILD|WS_VISIBLE|SS_RIGHT,
        x,y,w,S(160),sw->hwnd,NULL,g_hInst,NULL);
    SendMessageW(b,WM_SETFONT,(WPARAM)g_fUI,TRUE); sw->panelCtrls.push_back(b);
}

static void buildLogoutPanel(SettingsWin* sw){
    RECT p=panelRect(sw);
    int x=p.left+S(16), y=p.top+S(60), w=p.right-p.left-S(32);
    HWND title=CreateWindowExW(0,L"STATIC",
        L"\u062e\u0631\u0648\u062c \u0627\u0632 \u062d\u0633\u0627\u0628",
        WS_CHILD|WS_VISIBLE|SS_RIGHT,x,p.top+S(8),w,S(28),sw->hwnd,NULL,g_hInst,NULL);
    SendMessageW(title,WM_SETFONT,(WPARAM)g_fTitle,TRUE); sw->panelCtrls.push_back(title);
    HWND b=createFlatButton(sw->hwnd,IDC_PANEL_BASE+50,
        L"\u062e\u0631\u0648\u062c \u0627\u0632 \u062d\u0633\u0627\u0628 \u06a9\u0627\u0631\u0628\u0631\u06cc",ICO_LOGOUT,BS_DANGER,
        x,y,w,S(38)); sw->panelCtrls.push_back(b);
}

// §A: "Contact us" page — present for guest, reception and admin. Pulls the
// clinic contact details from settings (with sensible defaults) so management
// can edit them later without a code change. Future-proof: unknown/extra keys
// are never dropped (we only read the ones we render).
static void buildContactPanel(SettingsWin* sw){
    RECT p=panelRect(sw);
    int x=p.left+S(16), y=p.top+S(50), w=p.right-p.left-S(32);
    HWND title=CreateWindowExW(0,L"STATIC",
        L"\u0627\u0631\u062a\u0628\u0627\u0637 \u0628\u0627 \u0645\u0627",   // ارتباط با ما
        WS_CHILD|WS_VISIBLE|SS_RIGHT,x,p.top+S(8),w,S(28),sw->hwnd,NULL,g_hInst,NULL);
    SendMessageW(title,WM_SETFONT,(WPARAM)g_fTitle,TRUE); sw->panelCtrls.push_back(title);

    std::wstring phone = getSetting(L"contact.phone", L"\u06f0\u06f2\u06f1-\u06f1\u06f2\u06f3\u06f4\u06f5\u06f6\u06f7\u06f8");
    std::wstring mobile= getSetting(L"contact.mobile",L"\u06f0\u06f9\u06f1\u06f2-\u06f3\u06f4\u06f5\u06f6\u06f7\u06f8\u06f9");
    std::wstring email = getSetting(L"contact.email", L"support@azaditeb.ir");
    std::wstring addr  = getSetting(L"contact.address",
        L"\u062a\u0647\u0631\u0627\u0646\u060c \u062f\u0631\u0645\u0627\u0646\u06af\u0627\u0647 \u0622\u0632\u0627\u062f\u06cc \u0637\u0628");
    std::wstring hours = getSetting(L"contact.hours",
        L"\u0634\u0646\u0628\u0647 \u062a\u0627 \u067e\u0646\u062c\u200c\u0634\u0646\u0628\u0647\u060c \u06f8 \u062a\u0627 \u06f2\u06f0");

    std::wstring body;
    body += L"\u0628\u0631\u0627\u06cc \u067e\u0634\u062a\u06cc\u0628\u0627\u0646\u06cc\u060c \u0627\u0646\u062a\u0642\u0627\u062f\u0627\u062a \u0648 \u067e\u06cc\u0634\u0646\u0647\u0627\u062f\u0627\u062a \u0627\u0632 \u0631\u0627\u0647\u200c\u0647\u0627\u06cc \u0632\u06cc\u0631 \u062f\u0631 \u062a\u0645\u0627\u0633 \u0628\u0627\u0634\u06cc\u062f:\r\n\r\n";
    body += L"\u0640 \u062a\u0644\u0641\u0646 \u062b\u0627\u0628\u062a: " + toFaDigits(phone) + L"\r\n";
    body += L"\u0640 \u062a\u0644\u0641\u0646 \u0647\u0645\u0631\u0627\u0647: " + toFaDigits(mobile) + L"\r\n";
    body += L"\u0640 \u0631\u0627\u06cc\u0627\u0646\u0627\u0645\u0647: " + email + L"\r\n";
    body += L"\u0640 \u0646\u0634\u0627\u0646\u06cc: " + addr + L"\r\n";
    body += L"\u0640 \u0633\u0627\u0639\u0627\u062a \u067e\u0627\u0633\u062e\u06af\u0648\u06cc\u06cc: " + hours;

    HWND b=CreateWindowExW(0,L"STATIC",body.c_str(),
        WS_CHILD|WS_VISIBLE|SS_RIGHT,x,y,w,S(220),sw->hwnd,NULL,g_hInst,NULL);
    SendMessageW(b,WM_SETFONT,(WPARAM)g_fUI,TRUE); sw->panelCtrls.push_back(b);

    HWND bCopy=createFlatButton(sw->hwnd,IDC_PANEL_BASE+70,
        L"\u06a9\u067e\u06cc \u0627\u0637\u0644\u0627\u0639\u0627\u062a \u062a\u0645\u0627\u0633",ICO_RECEIPT,BS_OUTLINE,
        x,y+S(232),w,S(34)); sw->panelCtrls.push_back(bCopy);
}

static void buildStubPanel(SettingsWin* sw,const wchar_t* name){
    RECT p=panelRect(sw);
    int w=p.right-p.left, hgt=p.bottom-p.top;
    HWND title=CreateWindowExW(0,L"STATIC",name,WS_CHILD|WS_VISIBLE|SS_RIGHT,
        p.left+S(16),p.top+S(8),w-S(32),S(28),sw->hwnd,NULL,g_hInst,NULL);
    SendMessageW(title,WM_SETFONT,(WPARAM)g_fTitle,TRUE); sw->panelCtrls.push_back(title);
    HWND c=CreateWindowExW(0,L"STATIC",
        L"\u0627\u06cc\u0646 \u0628\u062e\u0634 \u0628\u0647\u200c\u0632\u0648\u062f\u06cc",
        WS_CHILD|WS_VISIBLE|SS_CENTER,p.left,p.top+hgt/2-S(14),w,S(28),
        sw->hwnd,NULL,g_hInst,NULL);
    SendMessageW(c,WM_SETFONT,(WPARAM)g_fBig,TRUE); sw->panelCtrls.push_back(c);
}

// panel id constants
enum {
    PANEL_REC_PROFILE=1, PANEL_REC_THEME, PANEL_REC_PRINTER, PANEL_REC_SAVEMSG,
    PANEL_REC_NOTIFY, PANEL_REC_ABOUT, PANEL_REC_LOGOUT, PANEL_REC_SAVEDVIEW,
    PANEL_MG_PROFILE=100, PANEL_MG_CLINIC, PANEL_MG_THEME, PANEL_MG_USERS,
    PANEL_MG_SECTIONS, PANEL_MG_INSURANCE, PANEL_MG_DESIGNER, PANEL_MG_RESTORE,
    PANEL_MG_GPRINTER, PANEL_MG_NETWORK, PANEL_MG_BACKUP, PANEL_MG_PROFREQ,
    PANEL_MG_BACKUPLOG, PANEL_MG_UPDATE, PANEL_MG_ABOUT, PANEL_MG_LOGOUT,
    // §A: shared "Contact us" page — available to EVERY audience incl. guest.
    PANEL_CONTACT=200
};

static void buildPanel(SettingsWin* sw){
    destroyPanelControls(sw);
    if(sw->active<0 || sw->active>=(int)sw->nav.size()) return;
    int pid=sw->nav[sw->active].panelId;
    switch(pid){
    case PANEL_REC_PROFILE: buildProfilePanel(sw,false); break;
    case PANEL_REC_THEME:   buildThemePanel(sw,false); break;
    case PANEL_REC_PRINTER: buildPrinterPanel(sw,false); break;
    case PANEL_REC_SAVEMSG: buildSaveMsgsPanel(sw); break;
    case PANEL_REC_NOTIFY:  buildNotifyPanel(sw); break;
    case PANEL_REC_ABOUT:   buildAboutPanel(sw); break;
    case PANEL_REC_LOGOUT:  buildLogoutPanel(sw); break;
    case PANEL_MG_PROFILE:  buildProfilePanel(sw,true); break;
    case PANEL_MG_THEME:    buildThemePanel(sw,true); break;
    case PANEL_MG_GPRINTER: buildPrinterPanel(sw,true); break;
    case PANEL_MG_ABOUT:    buildAboutPanel(sw); break;
    case PANEL_MG_LOGOUT:   buildLogoutPanel(sw); break;
    case PANEL_CONTACT:     buildContactPanel(sw); break;
    case PANEL_MG_DESIGNER: {
        buildStubPanel(sw,L"\u062f\u06cc\u0632\u0627\u06cc\u0646 \u0686\u0627\u067e\u06af\u0631");
        RECT p=panelRect(sw);
        HWND b=createFlatButton(sw->hwnd,IDC_PANEL_BASE+60,
            L"\u0628\u0627\u0632 \u06a9\u0631\u062f\u0646 \u062f\u06cc\u0632\u0627\u06cc\u0646\u0631",ICO_PRINT,BS_PRIMARY,
            p.left+S(16),p.top+S(50),p.right-p.left-S(32),S(36));
        sw->panelCtrls.push_back(b);
        break; }
    case PANEL_MG_RESTORE: {
        buildStubPanel(sw,L"\u0628\u0627\u0632\u06af\u0631\u062f\u0627\u0646\u06cc \u062f\u06cc\u0632\u0627\u06cc\u0646 \u0686\u0627\u067e");
        RECT p=panelRect(sw);
        HWND b=createFlatButton(sw->hwnd,IDC_PANEL_BASE+61,
            L"\u0628\u0627\u0632 \u06a9\u0631\u062f\u0646 \u067e\u0646\u062c\u0631\u0647 \u0628\u0627\u0632\u06af\u0631\u062f\u0627\u0646\u06cc",ICO_BACK,BS_PRIMARY,
            p.left+S(16),p.top+S(50),p.right-p.left-S(32),S(36));
        sw->panelCtrls.push_back(b);
        break; }
    case PANEL_MG_PROFREQ: {
        buildStubPanel(sw,L"\u062f\u0631\u062e\u0648\u0627\u0633\u062a\u200c\u0647\u0627\u06cc \u067e\u0631\u0648\u0641\u0627\u06cc\u0644");
        RECT p=panelRect(sw);
        HWND b=createFlatButton(sw->hwnd,IDC_PANEL_BASE+62,
            L"\u0628\u0627\u0632 \u06a9\u0631\u062f\u0646 \u0635\u0646\u062f\u0648\u0642 \u062f\u0631\u062e\u0648\u0627\u0633\u062a\u200c\u0647\u0627",ICO_BELL,BS_PRIMARY,
            p.left+S(16),p.top+S(50),p.right-p.left-S(32),S(36));
        sw->panelCtrls.push_back(b);
        break; }
    case PANEL_MG_BACKUPLOG: {
        buildStubPanel(sw,L"\u0644\u0627\u06af\u200c\u0647\u0627\u06cc \u0628\u06a9\u0627\u067e");
        RECT p=panelRect(sw);
        HWND b=createFlatButton(sw->hwnd,IDC_PANEL_BASE+63,
            L"\u0646\u0645\u0627\u06cc\u0634 \u0644\u0627\u06af\u200c\u0647\u0627\u06cc \u0628\u06a9\u0627\u067e",ICO_RECEIPT,BS_PRIMARY,
            p.left+S(16),p.top+S(50),p.right-p.left-S(32),S(36));
        sw->panelCtrls.push_back(b);
        break; }
    default: buildStubPanel(sw,sw->nav[sw->active].label.c_str()); break;
    }
    uikit::AzLayoutGuard_Verify(sw->hwnd);
}

// ------------------------------------------------------------- window proc ---
static void paintWin(SettingsWin* sw,HDC dc0){
    RECT rc; GetClientRect(sw->hwnd,&rc);
    uikit::MemDC mem(dc0,rc.right,rc.bottom);
    HBRUSH bg=CreateSolidBrush(g_theme.bg); FillRect(mem.dc,&rc,bg); DeleteObject(bg);
    // RTL: nav on the RIGHT
    RECT nav={rc.right-sw->navW,0,rc.right,rc.bottom};
    HBRUSH nb=CreateSolidBrush(g_theme.surface); FillRect(mem.dc,&nav,nb); DeleteObject(nb);
    HPEN pen=CreatePen(PS_SOLID,1,g_theme.border); HGDIOBJ op=SelectObject(mem.dc,pen);
    MoveToEx(mem.dc,nav.left,0,NULL); LineTo(mem.dc,nav.left,rc.bottom);
    SelectObject(mem.dc,op); DeleteObject(pen);
    SetBkMode(mem.dc,TRANSPARENT);
    HGDIOBJ of=SelectObject(mem.dc,g_fUI);
    int y=S(12);
    for(size_t i=0;i<sw->nav.size();++i){
        RECT item={nav.left+S(6),y,nav.right-S(6),y+S(36)};
        if((int)i==sw->active){
            gpRoundRectBg(mem.dc,item,S(8),
                blendColor(g_theme.surface,g_theme.accent,16),g_theme.accent,g_theme.surface);
            SetTextColor(mem.dc,g_theme.accent);
        } else SetTextColor(mem.dc,g_theme.text);
        RECT t={item.left+S(10),item.top,item.right-S(10),item.bottom};
        DrawTextW(mem.dc,sw->nav[i].label.c_str(),-1,&t,
            DT_RIGHT|DT_VCENTER|DT_SINGLELINE|DT_RTLREADING|DT_END_ELLIPSIS);
        y+=S(40);
    }
    SelectObject(mem.dc,of);
    mem.blitTo(dc0);
}

static int navHit(SettingsWin* sw,int mx,int my){
    RECT rc; GetClientRect(sw->hwnd,&rc);
    int navL=rc.right-sw->navW;
    if(mx<navL) return -1;
    int y=S(12);
    for(size_t i=0;i<sw->nav.size();++i){
        if(my>=y && my<y+S(36)) return (int)i;
        y+=S(40);
    }
    return -1;
}

static void applyThemeByName(const std::wstring& name, bool global, SettingsWin* sw){
    bool dark = (name==L"\u062a\u06cc\u0631\u0647");      // تیره
    setSetting(L"theme", dark?L"dark":L"light");
    wchar_t key[64]; swprintf(key,64,L"user:%d",sw->user.id);
    setSetting(std::wstring(L"theme.")+key, name);
    applyTheme(dark);
    broadcastThemeChange();
    if(global && g_hFrame) PostMessageW(g_hFrame,WM_APP_THEME_CHANGED,1,0);
    InvalidateRect(sw->hwnd,NULL,TRUE);
}

static LRESULT CALLBACK SettingsProc(HWND h,UINT m,WPARAM w,LPARAM l){
    SettingsWin* sw=(SettingsWin*)GetWindowLongPtrW(h,GWLP_USERDATA);
    switch(m){
    case WM_ERASEBKGND: return 1;
    case WM_PAINT:{ PAINTSTRUCT ps; HDC dc=BeginPaint(h,&ps); if(sw) paintWin(sw,dc);
        EndPaint(h,&ps); return 0; }
    case WM_LBUTTONUP:{
        if(!sw) break;
        int idx=navHit(sw,GET_X_LPARAM(l),GET_Y_LPARAM(l));
        if(idx>=0 && idx!=sw->active){ sw->active=idx; buildPanel(sw);
            InvalidateRect(h,NULL,TRUE); }
        return 0;
    }
    case WM_CTLCOLORSTATIC:{ HDC dc=(HDC)w; SetBkColor(dc,g_theme.bg);
        SetTextColor(dc,g_theme.text); return (LRESULT)g_brBg; }
    case WM_COMMAND:{
        if(!sw) break;
        int id=LOWORD(w);
        // theme swatches
        if(id>=IDC_PANEL_BASE+10 && id<=IDC_PANEL_BASE+13){
            const wchar_t* names[4]={L"\u0631\u0648\u0634\u0646",L"\u062a\u06cc\u0631\u0647",
                L"\u0633\u0648\u0644\u0627\u0631\u06cc\u0632\u0647",L"\u06a9\u0646\u062a\u0631\u0627\u0633\u062a \u0628\u0627\u0644\u0627"};
            applyThemeByName(names[id-(IDC_PANEL_BASE+10)],sw->isAdmin,sw);
            return 0;
        }
        switch(id){
        case IDC_PANEL_BASE+4: {   // profile submit
            wchar_t nm[128]={0}; GetDlgItemTextW(h,IDC_PANEL_BASE+1,nm,127);
            if(sw->isAdmin){
                MessageBoxW(h,L"\u062a\u063a\u06cc\u06cc\u0631\u0627\u062a \u0630\u062e\u06cc\u0631\u0647 \u0634\u062f.",
                    L"\u067e\u0631\u0648\u0641\u0627\u06cc\u0644",MB_OK|MB_ICONINFORMATION);
            } else {
                int r=MessageBoxW(h,
                    L"\u0622\u06cc\u0627 \u0627\u0632 \u062a\u063a\u06cc\u06cc\u0631\u0627\u062a \u0645\u0637\u0645\u0626\u0646 \u0647\u0633\u062a\u06cc\u062f\u061f \u062f\u0631\u062e\u0648\u0627\u0633\u062a \u0634\u0645\u0627 \u0628\u0631\u0627\u06cc \u0645\u062f\u06cc\u0631\u06cc\u062a \u0627\u0631\u0633\u0627\u0644 \u0645\u06cc\u200c\u0634\u0648\u062f.",
                    L"\u062a\u0627\u06cc\u06cc\u062f",MB_OKCANCEL|MB_ICONQUESTION);
                if(r==IDOK){
                    // queue profile request via net_sync / outbox
                    std::string json="{\"type\":\"profile_request\",\"user\":\"";
                    char ub[256]; WideCharToMultiByte(CP_UTF8,0,sw->user.username.c_str(),-1,ub,256,NULL,NULL);
                    char nb[256]; WideCharToMultiByte(CP_UTF8,0,nm,-1,nb,256,NULL,NULL);
                    json+=ub; json+="\",\"new_name\":\""; json+=nb; json+="\"}";
                    NetSync_PostJson(L"/api/profile_requests",json);
                    MessageBoxW(h,L"\u062f\u0631\u062e\u0648\u0627\u0633\u062a \u0627\u0631\u0633\u0627\u0644 \u0634\u062f.",
                        L"\u067e\u0631\u0648\u0641\u0627\u06cc\u0644",MB_OK|MB_ICONINFORMATION);
                }
            }
            return 0;
        }
        case IDC_PANEL_BASE+50:    // logout
            DestroyWindow(h);
            if(g_hFrame){ setUserOnline(g_session.user.username,false);
                g_session=Session(); switchScreen(SC_HOME); }
            return 0;
        // §F fix: capture hMain BEFORE DestroyWindow(h) — WM_DESTROY deletes
        // `sw` synchronously, so reading sw->hMain afterwards was a use-after-
        // free (it usually still "worked" but is undefined behaviour and could
        // pass a stale/garbage HWND into the designer, leaving a disabled owner
        // and an invisible modal). We now keep the owner valid and re-enabled
        // by the settings WM_DESTROY handler, then launch the child window.
        case IDC_PANEL_BASE+60: { HWND mw=sw->hMain; DestroyWindow(h); PrintDesigner_Open(mw); return 0; }
        case IDC_PANEL_BASE+61: { HWND mw=sw->hMain; DestroyWindow(h); RestoreDesign_Open(mw); return 0; }
        case IDC_PANEL_BASE+62: { HWND mw=sw->hMain; DestroyWindow(h); OpenProfileRequestsInbox(mw); return 0; }
        case IDC_PANEL_BASE+63: { HWND mw=sw->hMain; DestroyWindow(h); OpenBackupLogViewer(mw); return 0; }
        case IDC_PANEL_BASE+30: setSetting(L"msg.save_ok",uikit::AzSwitch_Get((HWND)l)?L"1":L"0"); return 0;
        case IDC_PANEL_BASE+31: setSetting(L"msg.save_fail",uikit::AzSwitch_Get((HWND)l)?L"1":L"0"); return 0;
        case IDC_PANEL_BASE+32: setSetting(L"msg.print_ok",uikit::AzSwitch_Get((HWND)l)?L"1":L"0"); return 0;
        case IDC_PANEL_BASE+33: setSetting(L"msg.print_fail",uikit::AzSwitch_Get((HWND)l)?L"1":L"0"); return 0;
        case IDC_PANEL_BASE+34: setSetting(L"saved_msgs_enabled",uikit::AzSwitch_Get((HWND)l)?L"1":L"0"); return 0;
        case IDC_PANEL_BASE+40: setSetting(L"notif.newappt",uikit::AzSwitch_Get((HWND)l)?L"1":L"0"); return 0;
        case IDC_PANEL_BASE+41: setSetting(L"notif.printfail",uikit::AzSwitch_Get((HWND)l)?L"1":L"0"); return 0;
        case IDC_PANEL_BASE+42: setSetting(L"notif.sound",uikit::AzSwitch_Get((HWND)l)?L"1":L"0"); return 0;
        case IDC_PANEL_BASE+70: {   // §A: copy contact details to clipboard
            std::wstring c =
                getSetting(L"contact.phone", L"\u06f0\u06f2\u06f1-\u06f1\u06f2\u06f3\u06f4\u06f5\u06f6\u06f7\u06f8")+L"  |  "+
                getSetting(L"contact.email", L"support@azaditeb.ir");
            if(OpenClipboard(h)){
                EmptyClipboard();
                size_t bytes=(c.size()+1)*sizeof(wchar_t);
                HGLOBAL hg=GlobalAlloc(GMEM_MOVEABLE,bytes);
                if(hg){ void* dst=GlobalLock(hg); memcpy(dst,c.c_str(),bytes);
                    GlobalUnlock(hg); SetClipboardData(CF_UNICODETEXT,hg); }
                CloseClipboard();
            }
            MessageBoxW(h,L"\u0627\u0637\u0644\u0627\u0639\u0627\u062a \u062a\u0645\u0627\u0633 \u06a9\u067e\u06cc \u0634\u062f.",
                L"\u0627\u0631\u062a\u0628\u0627\u0637 \u0628\u0627 \u0645\u0627",MB_OK|MB_ICONINFORMATION);
            return 0; }
        }
        return 0;
    }
    case WM_APP_THEME: InvalidateRect(h,NULL,TRUE); return 0;
    case WM_CLOSE: DestroyWindow(h); return 0;
    case WM_DESTROY:
        if(sw){ destroyPanelControls(sw);
            if(sw->hMain){ EnableWindow(sw->hMain,TRUE); SetForegroundWindow(sw->hMain); }
            delete sw; SetWindowLongPtrW(h,GWLP_USERDATA,0); }
        return 0;
    }
    return DefWindowProcW(h,m,w,l);
}

static void openSettingsWindow(HWND hMain,const User& u,int mode){
    static bool reg=false; const wchar_t* CLS=L"AzSettingsWin";
    if(!reg){ WNDCLASSW wc={0}; wc.lpfnWndProc=SettingsProc; wc.hInstance=g_hInst;
        wc.hCursor=LoadCursor(NULL,IDC_ARROW); wc.lpszClassName=CLS;
        wc.hbrBackground=g_brBg; wc.style=CS_HREDRAW|CS_VREDRAW;
        RegisterClassW(&wc); reg=true; }
    uikit::Az_RegisterControls();
    Sections_Init(); Designs_Init();

    bool admin = (mode==SM_ADMIN);
    SettingsWin* sw=new SettingsWin();
    sw->user=u; sw->isAdmin=admin; sw->mode=mode; sw->active=0; sw->hMain=hMain;
    sw->navW=admin?S(220):S(190);
    if(mode==SM_GUEST){
        // §A GUEST CONTRACT: ONLY «تغییر پوسته» + «ارتباط با ما». Nothing else.
        struct N{const wchar_t* t;int p;};
        static const N items[]={
            {L"\u062a\u063a\u06cc\u06cc\u0631 \u067e\u0648\u0633\u062a\u0647",PANEL_REC_THEME},
            {L"\u0627\u0631\u062a\u0628\u0627\u0637 \u0628\u0627 \u0645\u0627",PANEL_CONTACT},
        };
        for(auto& n:items) sw->nav.push_back({n.t,n.p});
    } else if(admin){
        struct N{const wchar_t* t;int p;};
        static const N items[]={
            {L"\u067e\u0631\u0648\u0641\u0627\u06cc\u0644 \u0645\u062f\u06cc\u0631\u06cc\u062a",PANEL_MG_PROFILE},
            {L"\u0627\u0637\u0644\u0627\u0639\u0627\u062a \u062f\u0631\u0645\u0627\u0646\u06af\u0627\u0647",PANEL_MG_CLINIC},
            {L"\u062a\u063a\u06cc\u06cc\u0631 \u067e\u0648\u0633\u062a\u0647 (\u0633\u0631\u0627\u0633\u0631\u06cc)",PANEL_MG_THEME},
            {L"\u0645\u062f\u06cc\u0631\u06cc\u062a \u06a9\u0627\u0631\u0628\u0631\u0627\u0646 \u0648 \u0646\u0642\u0634\u200c\u0647\u0627",PANEL_MG_USERS},
            {L"\u0628\u062e\u0634\u200c\u0647\u0627 \u0648 \u062f\u067e\u0627\u0631\u062a\u0645\u0627\u0646\u200c\u0647\u0627",PANEL_MG_SECTIONS},
            {L"\u0628\u06cc\u0645\u0647\u200c\u0647\u0627 \u0648 \u062a\u0639\u0631\u0641\u0647\u200c\u0647\u0627",PANEL_MG_INSURANCE},
            {L"\u062f\u06cc\u0632\u0627\u06cc\u0646 \u0686\u0627\u067e\u06af\u0631",PANEL_MG_DESIGNER},
            {L"\u0628\u0627\u0632\u06af\u0631\u062f\u0627\u0646\u06cc \u062f\u06cc\u0632\u0627\u06cc\u0646 \u0686\u0627\u067e",PANEL_MG_RESTORE},
            {L"\u062a\u0646\u0638\u06cc\u0645\u0627\u062a \u0686\u0627\u067e\u06af\u0631 \u0633\u0631\u0627\u0633\u0631\u06cc",PANEL_MG_GPRINTER},
            {L"\u062a\u0646\u0638\u06cc\u0645\u0627\u062a \u0634\u0628\u06a9\u0647 \u0648 \u0633\u0631\u0648\u0631",PANEL_MG_NETWORK},
            {L"\u067e\u0634\u062a\u06cc\u0628\u0627\u0646\u200c\u06af\u06cc\u0631\u06cc \u0648 \u0628\u0627\u0632\u06cc\u0627\u0628\u06cc",PANEL_MG_BACKUP},
            {L"\u062f\u0631\u062e\u0648\u0627\u0633\u062a\u200c\u0647\u0627\u06cc \u067e\u0631\u0648\u0641\u0627\u06cc\u0644",PANEL_MG_PROFREQ},
            {L"\u0644\u0627\u06af\u200c\u0647\u0627\u06cc \u0628\u06a9\u0627\u067e",PANEL_MG_BACKUPLOG},
            {L"\u0628\u0647\u200c\u0631\u0648\u0632\u0631\u0633\u0627\u0646\u06cc \u0628\u0631\u0646\u0627\u0645\u0647",PANEL_MG_UPDATE},
            {L"\u0627\u0631\u062a\u0628\u0627\u0637 \u0628\u0627 \u0645\u0627",PANEL_CONTACT},
            {L"\u062f\u0631\u0628\u0627\u0631\u0647 \u0628\u0631\u0646\u0627\u0645\u0647",PANEL_MG_ABOUT},
            {L"\u062e\u0631\u0648\u062c \u0627\u0632 \u062d\u0633\u0627\u0628",PANEL_MG_LOGOUT},
        };
        for(auto& n:items) sw->nav.push_back({n.t,n.p});
    } else {
        struct N{const wchar_t* t;int p;};
        static const N items[]={
            {L"\u067e\u0631\u0648\u0641\u0627\u06cc\u0644 \u0645\u0646",PANEL_REC_PROFILE},
            {L"\u062a\u063a\u06cc\u06cc\u0631 \u067e\u0648\u0633\u062a\u0647",PANEL_REC_THEME},
            {L"\u0627\u0646\u062a\u062e\u0627\u0628 \u0686\u0627\u067e\u06af\u0631",PANEL_REC_PRINTER},
            {L"\u067e\u06cc\u0627\u0645\u200c\u0647\u0627\u06cc \u0630\u062e\u06cc\u0631\u0647",PANEL_REC_SAVEMSG},
            {L"\u062a\u0646\u0638\u06cc\u0645\u0627\u062a \u0627\u0639\u0644\u0627\u0646",PANEL_REC_NOTIFY},
            {L"\u0627\u0631\u062a\u0628\u0627\u0637 \u0628\u0627 \u0645\u0627",PANEL_CONTACT},
            {L"\u062f\u0631\u0628\u0627\u0631\u0647 \u0628\u0631\u0646\u0627\u0645\u0647",PANEL_REC_ABOUT},
            {L"\u062e\u0631\u0648\u062c \u0627\u0632 \u062d\u0633\u0627\u0628",PANEL_REC_LOGOUT},
        };
        for(auto& n:items) sw->nav.push_back({n.t,n.p});
    }

    int W = admin? S(1000) : (mode==SM_GUEST? S(680) : S(760));
    int H = admin? S(640)  : (mode==SM_GUEST? S(480) : S(520));
    const wchar_t* titleTxt =
        admin?           L"\u062a\u0646\u0638\u06cc\u0645\u0627\u062a \u0645\u062f\u06cc\u0631\u06cc\u062a" :
        mode==SM_GUEST?  L"\u062a\u0646\u0638\u06cc\u0645\u0627\u062a" :
                         L"\u062a\u0646\u0638\u06cc\u0645\u0627\u062a \u067e\u0630\u06cc\u0631\u0634";
    RECT pr; GetWindowRect(hMain,&pr);
    int x=pr.left+((pr.right-pr.left)-W)/2, y=pr.top+((pr.bottom-pr.top)-H)/2;
    HWND h=CreateWindowExW(WS_EX_DLGMODALFRAME,CLS,titleTxt,
        WS_POPUP|WS_CAPTION|WS_SYSMENU|WS_CLIPCHILDREN,x,y,W,H,hMain,NULL,g_hInst,NULL);
    sw->hwnd=h;
    SetWindowLongPtrW(h,GWLP_USERDATA,(LONG_PTR)sw);
    buildPanel(sw);
    ShowWindow(h,SW_SHOW);
    EnableWindow(hMain,FALSE);
    SetForegroundWindow(h);
}

} // anonymous namespace

// ---------------------------------------------------------------- public -----
void OpenReceptionSettings(HWND hMain, const User& u){ openSettingsWindow(hMain,u,SM_RECEPTION); }
void OpenManagementSettings(HWND hMain, const User& u){ openSettingsWindow(hMain,u,SM_ADMIN); }
void OpenSettings(HWND hMain, const User& u){
    // §A: a GUEST (no login → empty username) gets the minimal settings window
    // containing ONLY «تغییر پوسته» + «ارتباط با ما». A logged-in reception user
    // gets the full reception settings; an admin gets management settings.
    if(u.username.empty())          openSettingsWindow(hMain,u,SM_GUEST);
    else if(u.role==ROLE_ADMIN)     OpenManagementSettings(hMain,u);
    else                            OpenReceptionSettings(hMain,u);
}
