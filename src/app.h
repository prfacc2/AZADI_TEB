// ============================================================================
//  Azadi-Teb  (آزادی طب)
//  Clinic Reception & Management System
//  Core shared header
//  Target: Windows 7/8/8.1/10/11+  (single x86 exe, runs on x86 & x64)
// ============================================================================
#pragma once
#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0601   // Windows 7 baseline
#define WINVER       0x0601
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <winspool.h>
#include <math.h>
#include <string>
#include <vector>

// ---------------------------------------------------------------- version --
#define APP_VERSION_W   L"1.0.1"
#define APP_NAME_W      L"\u0622\u0632\u0627\u062f\u06cc \u0637\u0628"   // آزادی طب
#define APP_CLASS_W     L"AzadiTebFrame"

// ----------------------------------------------------------------- globals -
extern HINSTANCE g_hInst;
extern HWND      g_hFrame;          // fullscreen main frame
extern double    g_scale;           // responsive UI scale
extern bool      g_lowSpec;         // speed-handler: weak hardware mode
extern bool      g_dark;            // dark theme active

// fonts (Vazirmatn, embedded)
extern HFONT g_fUI, g_fUIB, g_fSmall, g_fTitle, g_fBig, g_fHuge, g_fMono;

// scale helper
inline int S(int v){ return (int)(v * g_scale + 0.5); }

// ------------------------------------------------------------------ theme --
struct Theme {
    COLORREF bg;          // window background
    COLORREF surface;     // card background
    COLORREF surface2;    // secondary surface (bars)
    COLORREF border;      // borders / separators
    COLORREF text;        // primary text
    COLORREF textDim;     // secondary text
    COLORREF accent;      // primary accent
    COLORREF accentHover;
    COLORREF accentText;  // text on accent
    COLORREF danger;
    COLORREF dangerHover;
    COLORREF success;
    COLORREF inputBg;
    COLORREF inputText;
    COLORREF hover;       // ghost-button hover
};
extern Theme   g_theme;
extern HBRUSH  g_brBg, g_brSurface, g_brSurface2, g_brInput;
void applyTheme(bool dark);             // rebuild colors + brushes
void broadcastThemeChange();            // invalidate everything

// ------------------------------------------------------------- flat button -
enum IconId {
    ICO_NONE=0, ICO_X, ICO_CALC, ICO_PRINT, ICO_UPDATE, ICO_MOON, ICO_SUN,
    ICO_USER, ICO_SHIELD, ICO_PLUS, ICO_LOGOUT, ICO_DETACH, ICO_CROSS_MED,
    ICO_CHECK, ICO_TRASH, ICO_SAVE, ICO_BACK
};
enum BtnStyle { BS_GHOST=0, BS_PRIMARY=1, BS_DANGER=2, BS_OUTLINE=3, BS_CARD=4 };
void  registerFlatButton();
HWND  createFlatButton(HWND parent, int id, const wchar_t* text,
                       int icon, int style,
                       int x,int y,int w,int h, const wchar_t* sub=NULL);
void  drawIcon(HDC dc, int icon, RECT rc, COLORREF col, int thick);
void  fillRoundRect(HDC dc, RECT rc, int rad, COLORREF fill, COLORREF border);

// ------------------------------------------------------------------- time --
SYSTEMTIME   iranNow();                                  // UTC+3:30 (fixed; no DST since 2022)
void         gregToJalali(int gy,int gm,int gd,int&jy,int&jm,int&jd);
std::wstring jalaliDateStr(const SYSTEMTIME& st);        // e.g. سه‌شنبه ۲۰ خرداد ۱۴۰۵
std::wstring jalaliDateShort(const SYSTEMTIME& st);      // 1405/03/20
std::wstring iranTimeStr(const SYSTEMTIME& st, bool seconds);
std::wstring toFaDigits(const std::wstring& s);
int          iranMinutesOfDay();
int          detectShift();                              // 0=صبح 1=عصر 2=شب
std::wstring shiftName(int s);

// ------------------------------------------------------------------ utils --
std::wstring exeDir();
std::wstring dataDir();      // <exe>\data   (auto-created)
std::wstring logsDir();      // <exe>\logs   (auto-created)
void         logLine(const std::wstring& s);
std::wstring formatMoney(long long v);                   // 1,234,567
long long    parseMoney(const std::wstring& s);
std::wstring trim(const std::wstring& s);
bool         writeFileUtf8(const std::wstring& path, const std::wstring& text, bool append);
std::wstring readFileUtf8(const std::wstring& path);

// --------------------------------------------------------------- settings --
std::wstring getSetting(const std::wstring& key, const std::wstring& def);
void         setSetting(const std::wstring& key, const std::wstring& val);

// --------------------------------------------------------------- handlers --
void installCrashHandler();   // crash-handler: dump + log + friendly message
void detectSpec();            // speed-handler: sets g_lowSpec
void installVazirFont();      // embed-load + install on user system if missing

// ------------------------------------------------------------------ users --
struct User {
    std::wstring username, fullname, dept, hash;
    int role;                 // 0 = پذیرش, 1 = مدیریت
};
std::vector<User> loadUsers();
bool addUser(const User& u, std::wstring& err);
bool removeUser(const std::wstring& username);
bool verifyLogin(const std::wstring& u, const std::wstring& p,
                 int wantRole, User& out, std::wstring& err);
std::wstring hashPassword(const std::wstring& p);

// -------------------------------------------------------------- insurance --
struct InsuranceDef { const wchar_t* name; int pct; };
extern const InsuranceDef INSURANCES[];     extern const int N_INSURANCES;
extern const InsuranceDef SUPP_INSURANCES[];extern const int N_SUPP;

// -------------------------------------------------------------- reception --
struct ReceptionRecord {
    std::wstring firstName, lastName, nationalId, fatherName, birthDate,
                 gender, mobile, landline, address, patientType,
                 insurance, suppInsurance, apptDate, apptTime,
                 shift, dept, userName;
    long long total, mainShare, patientShare, baseDiff, orgShare,
              finalTotal, discount, paid;
    int queueNo, insIdx, suppIdx;
    ReceptionRecord():total(0),mainShare(0),patientShare(0),baseDiff(0),
        orgShare(0),finalTotal(0),discount(0),paid(0),queueNo(0),
        insIdx(0),suppIdx(0){}
};
int  saveReception(ReceptionRecord& r);          // assigns queue no, persists CSV
int  countTodayReceptions();
void saveLastReceipt(const ReceptionRecord& r);
bool loadLastReceipt(ReceptionRecord& r);

// --------------------------------------------------------------- printing --
// kind: 0 = رسید بیمه  1 = نسخه  2 = قبض
bool printReceipt(const ReceptionRecord& r, int kind, HWND owner);
bool printLastReceipt(HWND owner);

// ---------------------------------------------------------------- session --
struct Session {
    User user;
    int  shift;          // chosen at login, never auto-revoked
    SYSTEMTIME loginAt;
};
extern Session g_session;

// ---------------------------------------------------------------- screens --
enum ScreenId { SC_HOME=0, SC_RECEPTION=1, SC_ADMIN=2, SC_MANAGE=3 };
void switchScreen(ScreenId id);
RECT frameContentRect();                 // area between top & bottom bars

HWND createHomeScreen(HWND frame);       // main.cpp
HWND createReceptionScreen(HWND frame);  // reception.cpp
HWND createAdminScreen(HWND frame);      // admin.cpp
HWND createManageScreen(HWND frame);     // admin.cpp

// ---------------------------------------------------------------- dialogs --
// role: 0 پذیرش / 1 مدیریت / 2 admin (hidden, prf)
bool showLoginDialog(HWND parent, int role, User& out);
bool showShiftDialog(HWND parent, int& shift);

// ------------------------------------------------------------- calculator --
void openCalculator(HWND owner);

// ----------------------------------------------------------------- update --
void checkRemoteUpdate(HWND owner);      // remote-update over HTTP(S)

// edit subclass: Enter => next field
void enableEnterNavigation(HWND ctl);
