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
#define APP_VERSION_W   L"1.18.0"

// ----------------------------------------------------------- logging policy -
//  RELEASE 1.2.0 (Section A): all general user-behavior logging is gated behind
//  AZ_DEBUG_LOGS, which is OFF (0) in release. The only log channels that remain
//  active are the dedicated Backup Log (backup_log.h) and the crash dump.
#ifndef AZ_DEBUG_LOGS
#define AZ_DEBUG_LOGS 0
#endif
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
// §G (1.11.0): a TRUE fixed-pitch font for section / personnel CODES so digits
// align in a clean column. Falls back Consolas → Courier New at build time.
extern HFONT g_fCode;

// scale helper
inline int S(int v){ return (int)(v * g_scale + 0.5); }

// Blend color `a` toward `b` by `pct` percent (0 = pure a, 100 = pure b).
// Used for subtle, theme-aware UI tints (e.g. a soft focus ring).
inline COLORREF blendColor(COLORREF a, COLORREF b, int pct){
    if(pct<0) pct=0;
    if(pct>100) pct=100;
    int ra=GetRValue(a), ga=GetGValue(a), ba=GetBValue(a);
    int rb=GetRValue(b), gb=GetGValue(b), bb=GetBValue(b);
    int r=ra+(rb-ra)*pct/100;
    int g=ga+(gb-ga)*pct/100;
    int bl=ba+(bb-ba)*pct/100;
    return RGB(r,g,bl);
}

// ------------------------------------------------------------------ theme --
struct Theme {
    COLORREF bg;          // window background
    COLORREF bg2;         // secondary page tint (subtle gradient bottom)
    COLORREF surface;     // card background
    COLORREF surface2;    // secondary surface (bars)
    COLORREF surfaceTop;  // card gradient top (lighter)
    COLORREF border;      // borders / separators
    COLORREF text;        // primary text
    COLORREF textDim;     // secondary text
    COLORREF accent;      // primary accent
    COLORREF accent2;     // accent gradient end (for buttons/header)
    COLORREF accentHover;
    COLORREF accentText;  // text on accent
    COLORREF danger;
    COLORREF dangerHover;
    COLORREF success;
    COLORREF warn;        // amber for highlights / chips
    COLORREF inputBg;
    COLORREF inputText;
    COLORREF hover;       // ghost-button hover
    COLORREF headerTop;   // top header bar gradient top
    COLORREF headerBot;   // top header bar gradient bottom
};
extern Theme   g_theme;
extern HBRUSH  g_brBg, g_brSurface, g_brSurface2, g_brInput;
void applyTheme(bool dark);             // rebuild colors + brushes
void broadcastThemeChange();            // invalidate everything
#define WM_APP_THEME (WM_APP+11)        // sent to every window on theme switch
// ---------------------------------------------------------- 1.4.0 messages --
//  Broadcast / internal messages introduced by release 1.4.0.
#define WM_APP_THEME_CHANGED (WM_APP+12) // a settings panel changed the theme
#define WM_APP_LAYOUT_REDO   (WM_APP+13) // AzLayoutGuard requests a relayout
#define WM_APP_DESIGN_PUSHED (WM_APP+14) // a print design was pushed to sections

// ------------------------------------------------------------- flat button -
enum IconId {
    ICO_NONE=0, ICO_X, ICO_CALC, ICO_PRINT, ICO_UPDATE, ICO_MOON, ICO_SUN,
    ICO_USER, ICO_SHIELD, ICO_PLUS, ICO_LOGOUT, ICO_DETACH, ICO_CROSS_MED,
    ICO_CHECK, ICO_TRASH, ICO_SAVE, ICO_BACK,
    ICO_ID, ICO_PHONE, ICO_CAL, ICO_PIN, ICO_RECEIPT, ICO_CLOCK, ICO_REFRESH,
    ICO_GEAR, ICO_BELL, ICO_TAB, ICO_CHEVRON,
    // v1.11.0 §F: bookmark glyph for the Saved-Messages feature.
    ICO_SAVED_MSG,
    // v1.11.0 §A: people / contact / palette glyphs for messenger-style rows.
    ICO_PALETTE, ICO_INFO, ICO_PEOPLE,
    // v1.19.0: wallet glyph for the «مبلغ نهایی» (final-amount) summary card.
    ICO_WALLET
};
// §F: spec name alias — the work order references this symbol explicitly.
#define IC_SAVED_MSG ICO_SAVED_MSG
enum BtnStyle { BS_GHOST=0, BS_PRIMARY=1, BS_DANGER=2, BS_OUTLINE=3, BS_CARD=4,
                //  v1.8.0: a distinct, non-red "attention" style (violet/teal)
                //  used for the management change-request categories so they
                //  stand out clearly WITHOUT using the danger red.
                BS_INFO=5 };
//  v1.8.0: a distinct accent for the "attention / requests" controls (NOT red).
extern COLORREF g_infoAccent, g_infoAccent2;
void  registerFlatButton();
HWND  createFlatButton(HWND parent, int id, const wchar_t* text,
                       int icon, int style,
                       int x,int y,int w,int h, const wchar_t* sub=NULL);
void  setFlatButtonIcon(HWND btn, int icon);   // in-place icon swap (v1.1.0)
//  v1.4.0: tell a flat button what colour sits BEHIND its rounded corners so
//  the antialiased corners blend into the real surface (fixes the "white
//  corners in dark mode" bug on header/bar buttons). Pass CLR_INVALID to let
//  the button ask its parent (default behaviour).
void  setFlatButtonBg(HWND btn, COLORREF bg);
//  v1.4.1: give a flat button a real raster icon (RCDATA id; 0 = vector icon).
void  setFlatButtonImage(HWND btn, int resId);
void  drawIcon(HDC dc, int icon, RECT rc, COLORREF col, int thick);
void  fillRoundRect(HDC dc, RECT rc, int rad, COLORREF fill, COLORREF border);
//  v1.6.0: a fully theme-aware owner-draw combobox (fixes dark-mode dropdown
//  white-on-white) — create with createThemedCombo, forward WM_DRAWITEM to
//  drawThemedComboItem in the parent.
HWND  createThemedCombo(HWND parent, int id);
bool  drawThemedComboItem(LPDRAWITEMSTRUCT dis);

// ---------------------------------------------------------------- GDI+ -----
//  v1.3.0: a thin GDI+ helper layer gives us the "richer colours, lighting,
//  smooth layers" the UI redesign needs — anti-aliased rounded cards, vertical
//  gradients, soft drop-shadows, translucent overlays and a real background
//  image — all without leaving the single static EXE (gdiplus ships with
//  every Windows since XP).  All helpers degrade gracefully if GDI+ is absent.
void gdipStartup();
void gdipShutdown();
//  Anti-aliased filled rounded rectangle (optionally with a 1px border).
void gpRoundRect(HDC dc, RECT rc, int rad, COLORREF fill, COLORREF border, int alpha=255);
//  Vertical 2-stop gradient rounded rectangle.
void gpGradRoundRect(HDC dc, RECT rc, int rad, COLORREF top, COLORREF bottom, COLORREF border);
//  v1.8.0: rounded-rect variants that FIRST paint the 4 corner triangles (the
//  area outside the rounded path but inside the bounding rect) with `bg` so the
//  corners always match the surrounding theme background — no dark/black/wrong
//  colour artefacts on rounded controls, cards, wells, lists or combos.
void gpRoundRectBg(HDC dc, RECT rc, int rad, COLORREF fill, COLORREF border, COLORREF bg, int alpha=255);
void gpGradRoundRectBg(HDC dc, RECT rc, int rad, COLORREF top, COLORREF bottom, COLORREF border, COLORREF bg);
//  v1.19.0: HORIZONTAL 2-stop gradient rounded rect (left→right), corners first
//  filled with `bg`. Used for the «مبلغ نهایی» card (sky-blue → royal-blue).
void gpGradRoundRectBgH(HDC dc, RECT rc, int rad, COLORREF left, COLORREF right, COLORREF border, COLORREF bg);
//  Paint only the 4 rounded-corner gaps of `rc` (radius `rad`) with `bg`. Use
//  this to "patch" the corners behind any rounded region whose interior is
//  already drawn (e.g. owner-drawn lists / combos / regions).
void gpFillCorners(HDC dc, RECT rc, int rad, COLORREF bg);
//  Soft drop shadow behind a rounded rect (blurred, layered look).
void gpShadow(HDC dc, RECT rc, int rad, int spread, int alpha);
//  Solid translucent rounded fill (for glass overlays / dim layers).
void gpFillAlpha(HDC dc, RECT rc, int rad, COLORREF fill, int alpha);
//  Draw the embedded background image (id 103 light / 104 dark) cover-fitted
//  into rc, then a translucent scrim of `scrim` colour at `scrimA` alpha so
//  foreground text stays perfectly legible.  Returns false if no image.
bool gpDrawBackground(HDC dc, RECT rc, bool dark, COLORREF scrim, int scrimA);
//  v1.4.1: draw a real (raster) RCDATA PNG icon, aspect-fit & centred in rc and
//  recoloured to `tint`. Used for the print-action buttons. Returns false if
//  GDI+ / resource unavailable so callers fall back to the vector drawIcon().
bool gpDrawTintedImageRes(HDC dc, int resId, RECT rc, COLORREF tint);
bool gpDrawImageFileCircle(HDC dc, const std::wstring& path, RECT rc);
//  RCDATA ids of the print-action raster icons (see app.rc):
#define IMG_IC_PRINTER 201
#define IMG_IC_RECEIPT 202
#define IMG_IC_SHIELD  203
#define IMG_IC_LAST    204
//  v1.8.0: header settings (gear) + calculator raster icons.
#define IMG_IC_SETTINGS 205
#define IMG_IC_CALC     206
//  Crisp anti-aliased line / circle helpers used by the new header clock etc.
void gpLine(HDC dc, int x1,int y1,int x2,int y2, COLORREF col, float w, int alpha=255);

// ------------------------------------------------------------------- time --
SYSTEMTIME   iranNow();                                  // UTC+3:30 (fixed; no DST since 2022)
void         gregToJalali(int gy,int gm,int gd,int&jy,int&jm,int&jd);
std::wstring jalaliDateStr(const SYSTEMTIME& st);        // e.g. سه‌شنبه ۲۰ خرداد ۱۴۰۵
std::wstring jalaliDateShort(const SYSTEMTIME& st);      // 1405/03/20
std::wstring iranTimeStr(const SYSTEMTIME& st, bool seconds);
//  v1.4.0: single canonical Persian Jalali formatter. Returns the date for a
//  UTC time_t as «۱۴۰۵/۰۴/۰۲» using Persian-Indic digits with RTL-safe marks.
//  Every date label in the app must route through this helper. Pass 0 for "now".
std::wstring FormatJalaliPersian(time_t utc);
//  Jalali Y/M/D string for *today* in Tehran, as "YYYY/MM/DD" (ASCII digits) —
//  used as the per-day key for appointment counters.
std::wstring JalaliTodayKey();
std::wstring toFaDigits(const std::wstring& s);
int          iranMinutesOfDay();
int          detectShift();                              // 0=صبح 1=عصر 2=شب
std::wstring shiftName(int s);

// ------------------------------------------------------------------ utils --
std::wstring exeDir();
std::wstring dataDir();      // <exe>\data   (auto-created)
std::wstring logsDir();      // <exe>\logs   (auto-created)
void         writeSchemaVersion();   // §I: stamp data\.schema_version (informational only)
void         logLine(const std::wstring& s);
// §J: record a flow breadcrumb (last 32 are dumped into the crash report).
void         Breadcrumb(const wchar_t* what);
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

// ------------------------------------------------------------- setup splash --
// First-run / every-run prerequisite preparation with a visible progress bar.
// Verifies + installs the things the hybrid (MSHTML) reception surface needs to
// work on *this* client machine: data/log folders, the Vazirmatn font, the
// FEATURE_BROWSER_EMULATION registry key (so Trident runs in IE11 standards
// mode instead of IE7-quirks), and a live MSHTML availability probe. Returns
// true when the environment is ready for the hybrid UI; false means the native
// fallback form will be used. Shows the bar only when there is real work to do
// (first run, or after a version bump), otherwise returns instantly.
bool RunSetupSplash(HINSTANCE hInst);

// ------------------------------------------------------------------ users --
// Role constants (kept as plain ints for ABI stability with stored data).
#define ROLE_RECEPTION 0
#define ROLE_ADMIN     1
struct User {
    std::wstring username, fullname, dept, hash;
    int role;                 // 0 = پذیرش, 1 = مدیریت
    int id;                   // stable per-user id (index into users store)
    // §H forward-compat: any extra pipe-delimited columns written by a FUTURE
    // version (fields 6,7,…) are captured verbatim here and written back
    // unchanged, so an older build never silently drops newer data.
    std::wstring extra;
    User():role(0),id(0){}
};
std::vector<User> loadUsers();
bool addUser(const User& u, std::wstring& err);
bool removeUser(const std::wstring& username);
bool setUserFullName(const std::wstring& username, const std::wstring& fullname); // §5
bool verifyLogin(const std::wstring& u, const std::wstring& p,
                 int wantRole, User& out, std::wstring& err);
std::wstring hashPassword(const std::wstring& p);

// -------------------------------------------------------------- insurance --
struct InsuranceDef { const wchar_t* name; int pct; };
extern const InsuranceDef INSURANCES[];     extern const int N_INSURANCES;
extern const InsuranceDef SUPP_INSURANCES[];extern const int N_SUPP;

// -------------------------------------------------------------- tariffs ----
//  Default service tariffs (Rial) so the program computes the bill itself.
//  Indexed by patient-visit type: 0=عادی 1=سرپایی 2=بستری.
extern const long long VISIT_TARIFF[3];
//  Surcharge multipliers for appointment type: 0=عادی 1=اورژانس 2=پرسنلی
long long applyApptTariff(long long base, int apptType);
//  Returns the default service price for a given patient + appointment type.
long long defaultServicePrice(int patientType, int apptType);

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

// v1.7.0: header→reception action routing. The frame header (main.cpp) owns
// the «پذیرش جدید» / «نوبت‌دهی» / «تب جدید» buttons and routes them to the
// active reception screen via these helpers. RA_* names the requested action.
enum RecAction { RA_NEWPAT=0, RA_APPOINTMENT=1, RA_NEWTAB=2 };
HWND receptionWindow();                  // the live reception HWND (or NULL)
void receptionAction(RecAction a);       // route a header action to reception

// ---------------------------------------------------------------- dialogs --
// role: 0 پذیرش / 1 مدیریت / 2 admin (hidden, prf)
bool showLoginDialog(HWND parent, int role, User& out);
bool showShiftDialog(HWND parent, int& shift);
// profile edit (name + photo) — submits a ProfReq for management approval.
// returns true if the user pressed «تأیید» and a request was queued.
bool showProfileDialog(HWND parent);

// ------------------------------------------------------------- calculator --
void openCalculator(HWND owner);

// ------------------------------------------------------------- settings ----
//  v1.3.0: a slide-over "settings" panel styled like a social-network profile
//  page (avatar, identity card, then grouped option rows). Opened from the
//  gear button in the top header. Hosts: theme switch, check-for-update,
//  density, auto-print toggle, server URL, about.
void openSettingsPanel(HWND frameOwner);
bool settingsPanelVisible();
void closeSettingsPanel();

// ------------------------------------------------------ v1.4.0 settings tiers
//  Role dispatcher: reception users get OpenReceptionSettings, admins get
//  OpenManagementSettings (§1.1). The gear icon routes here.
void OpenSettings(HWND hMain, const User& u);
void OpenReceptionSettings(HWND hMain, const User& u);
void OpenManagementSettings(HWND hMain, const User& u);

// ------------------------------------------------- v1.4.0 header collapse (§6)
//  A small state machine animates the reception header's action bar between
//  fully-expanded (factor 1.0) and collapsed (factor 0.0). The frame queries
//  HeaderCollapse_Factor() while laying out the header; HeaderCollapse_Set()
//  starts an animation toward the target; HeaderCollapse_Tick() advances it and
//  returns true while still animating (so the frame keeps the timer alive).
#define HEADER_COLLAPSE_TIMER 0xC0A1
void  HeaderCollapse_Set(HWND frame, bool collapsed);  // begin animating
bool  HeaderCollapse_Tick(HWND frame);                 // advance; true if more
float HeaderCollapse_Factor();                         // 0.0..1.0 (1 = expanded)
bool  HeaderCollapse_Collapsed();                      // current target state

// ----------------------------------------------------------------- update --
void checkRemoteUpdate(HWND owner);      // remote-update over HTTP(S)

// ------------------------------------------------------------- print system --
//  v1.4.0: printer configuration + a full print-layout DESIGNER.
//  Sections (each prints differently): پذیرش / تزریقات / آزمایشگاه …
extern const wchar_t* PRINT_SECTIONS[];  extern const int N_PRINT_SECTIONS;
//  Open the printer-settings dialog (default printer, test, paper size,
//  fit/fill, advanced) — persisted in settings.
void openPrinterSettings(HWND owner);
//  Open the visual print designer for a given section index.
void openPrintDesigner(HWND owner, int sectionIdx);
//  Render the saved design for a section onto a printer DC for a real receipt.
//  Returns false if no design exists (caller falls back to the classic layout).
bool printDesignedReceipt(const ReceptionRecord& r, int sectionIdx, HWND owner);
//  Pulse the cash drawer connected to the configured printer (ESC/POS kick),
//  but only when the «باز کردن کشوی پول» option is enabled in printer settings.
void kickCashDrawer();

// --------------------------------------------------------------- employees --
//  Department categories + employee directory (management panel).
struct DeptCat { std::wstring id, name, manager, icon;
    // §H forward-compat: extra pipe columns from a newer version, kept verbatim
    // (already pipe-prefixed + escaped) so a save round-trip never drops them.
    std::wstring extra; };
std::vector<DeptCat> loadDepts();
bool addDept(const DeptCat& c, std::wstring& err);
bool removeDept(const std::wstring& id);
void seedDefaultDepts();   // ensure the «پذیرش» default category exists
//  Extended employee profile (beyond the login User record).
//  v1.8.0: added empId (auto/manual personnel code), uniqueId (system-unique
//  identifier, auto/manual), position/title, mobile, email, hireDate and
//  workHours (weekly hours) so the new-employee form is complete.
struct EmpProfile {
    std::wstring username, nationalId, fatherName, address, landline,
                 shiftFrom, shiftTo, photoPath, idCardPath, deptId;
    std::wstring empId, uniqueId, position, mobile, email, hireDate, workHours;
    // §H forward-compat: unknown key=value lines written by a FUTURE version are
    // captured here (already including their trailing CRLF) and re-emitted on
    // save so older builds never silently drop newer profile fields.
    std::wstring extraKv;
};
EmpProfile loadEmpProfile(const std::wstring& username);
void       saveEmpProfile(const EmpProfile& p);
bool       isUserOnline(const std::wstring& username);   // session presence (heartbeat <90s)
void       setUserOnline(const std::wstring& username, bool on);
void       heartbeatUser(const std::wstring& username);  // §G: refresh presence on a timer

// ------------------------------------------------------------------ kartabl --
//  Cartable / inbox messages pushed from management to a user (or broadcast).
//  v1.4.1: messages carry a severity TYPE so the cartable can colour-code them:
//    0 = عادی   (green / safe)
//    1 = فوری   (yellow / warning)
//    2 = بحرانی (red / error)
enum { KMSG_NORMAL=0, KMSG_URGENT=1, KMSG_CRITICAL=2 };
struct KMsg { std::wstring from, to, text, time; bool seen; int type; bool pinned;
    KMsg():seen(false),type(0),pinned(false){} };
std::vector<KMsg> loadMessages(const std::wstring& forUser);
//  legacy 3-arg push (type defaults to عادی) and the new typed push.
void  pushMessage(const std::wstring& from, const std::wstring& to,
                  const std::wstring& text);
void  pushMessageT(const std::wstring& from, const std::wstring& to,
                   const std::wstring& text, int type);
int   unseenMessageCount(const std::wstring& forUser);
void  markMessagesSeen(const std::wstring& forUser);

// ------------------------------------------------- settings-change requests --
//  v1.4.1: when a reception workstation changes printer / design settings, a
//  change-request record is written so management sees who / which system /
//  what changed (with date+time) under a red notification badge.
//  v1.9.0: the request now carries an APPROVAL workflow. Settings are NOT
//  applied immediately — they are queued here and only applied after management
//  approves. Fields:
//    user|system|change|profile|time|seen|status|payload|title
//  status: 0=pending 1=approved 2=rejected.  payload = the pending setting(s)
//  (key=value;key=value) that are applied verbatim on approval. title = short
//  human title (e.g. «تغییر نوع چاپگر»). preview = optional preview text/path.
struct SetReq {
    std::wstring user, system, change, profile, time, payload, title, preview;
    bool seen;
    int  status;
    SetReq():seen(false),status(0){}
};
std::vector<SetReq> loadSetReqs();
//  legacy 4-arg push (kept for source compatibility — queues with no payload).
void  pushSetReq(const std::wstring& user, const std::wstring& system,
                 const std::wstring& change, const std::wstring& profile);
//  v1.9.0: full approval-aware push. Returns nothing; the change is applied
//  ONLY when management later approves it (setSetReqStatus).
void  pushSetReqEx(const std::wstring& user, const std::wstring& system,
                   const std::wstring& title, const std::wstring& change,
                   const std::wstring& payload, const std::wstring& preview);
//  Approve (1) / reject (2) the i-th request (newest-first). On approval the
//  payload key=value pairs are written to settings; on rejection an inbox
//  message «درخواست شما توسط مدیریت رد شد.» is delivered to the requester.
void  setSetReqStatus(int indexNewestFirst, int status, const std::wstring& reason);
void  markOneSetReqSeen(int indexNewestFirst);   // mark a single request read
void  deleteSetReq(int indexNewestFirst);        // remove a request entirely
int   unseenSetReqCount();
int   pendingSetReqCount();
void  markSetReqsSeen();
//  v1.9.0: the local network/system identity used to stamp requests (computer
//  name; falls back to a stored id). Shown to management as the request source.
std::wstring systemSourceName();

// ------------------------------------------------- saved (archived) messages --
//  v1.8.0: when «پیام‌های ذخیره‌شده» (Saved Messages) is enabled in settings, a
//  message can be archived to permanent local storage with its text and any
//  downloadable attachments preserved. Stored in data\saved_msgs.dat:
//      from|to|time|type|attachPath|text
struct SavedMsg { std::wstring from, to, time, attachPath, text; int type; bool seen;
    SavedMsg():type(0),seen(false){} };
std::vector<SavedMsg> loadSavedMsgs();
void  pushSavedMsg(const std::wstring& from, const std::wstring& to,
                   const std::wstring& text, int type,
                   const std::wstring& attachPath);
int   savedMsgCount();
bool  savedMsgsEnabled();                 // settings flag "saved_msgs_enabled"
//  v1.8.0: department-targeted message helper (records the dept/route in `to`).
//  Attachments (image/video/gif/png/jpg/word/…) are copied into a local
//  attachments folder and the stored path lets the recipient download them.
std::wstring copyAttachmentLocal(const std::wstring& srcPath);  // returns stored path or L""
//  v1.9.0: a SECOND, always-on saved-messages store that is strictly LOCAL to
//  this machine/user and is NEVER transmitted across the network. Both the
//  employee and the management side keep their own personal note board here:
//  the user can type text notes and attach images. Stored per-user in
//  data\local_notes_<user>.dat:  time|attachPath|text
struct LocalNote { std::wstring time, attachPath, text; };
std::vector<LocalNote> loadLocalNotes(const std::wstring& forUser);
void  pushLocalNote(const std::wstring& forUser, const std::wstring& text,
                    const std::wstring& attachPath);
void  deleteLocalNote(const std::wstring& forUser, int indexNewestFirst);
int   localNoteCount(const std::wstring& forUser);

// ------------------------------------------------------------- notifications --
//  v1.9.0: a lightweight Windows toast/balloon notification. Used so that ONLY
//  the recipients of a management message see «شما یک پیام جدید دارید.» — the
//  manager who sent it does NOT get the notification back.
void  showWindowsNotification(const std::wstring& title, const std::wstring& body);
//  Deliver a message AND fire the recipient notification on every recipient
//  workstation (but not the sender). Returns recipients count.
void  notifyNewMessageRecipients();   // checks the pending flag for THIS user

// ----------------------------------------------------------------- backup -----
//  v1.9.0: management backup / restore of patient data. Designed to read very
//  large (~15 GB) Matin-Teb (.bak) backups WITHOUT freezing the UI by scanning
//  in a background thread and streaming. A scan returns a category breakdown
//  with estimated sizes; restore can apply the full backup or a selected
//  subset (e.g. only patient information).
struct BackupCategory {
    std::wstring id;        // stable id ("patients","images",...)
    std::wstring name;      // Persian display name
    long long    bytes;     // estimated size in bytes
    long long    records;   // estimated record count (0 if N/A)
    bool         selected;  // user tick for selective restore
    BackupCategory():bytes(0),records(0),selected(false){}
};
//  Opaque scan result. Filled by the background scanner.
struct BackupInfo {
    std::wstring path;
    long long    totalBytes;
    std::vector<BackupCategory> cats;
    bool         ready;     // scan complete
    BackupInfo():totalBytes(0),ready(false){}
};
//  Open the full management backup manager page (modal over the frame).
void  openBackupManager(HWND owner);

// ----------------------------------------------------------- appointment ----
//  v1.6.0: the نوبت‌دهی (appointment) module lives as a tab inside the reception
//  workspace. It owns its own page window with a search panel, an appointment-
//  details panel, a patient-details panel (enabled only when a citizen is
//  found) and a read-only DataGridView of the day's appointments.
HWND createAppointmentPage(HWND parent);   // appointment.cpp

// edit subclass: Enter / Tab => next field
void enableEnterNavigation(HWND ctl);

// edit subclass: smart Jalali date mask — user types digits only, slashes are
// inserted automatically as YYYY/MM/DD (also keeps Enter/Tab navigation).
void enableDateMask(HWND ctl);

// edit subclass: automatic RTL/LTR alignment based on the typed content —
// Persian/Arabic text aligns RIGHT (RTL), Latin/digits-only aligns LEFT.
void enableAutoDir(HWND ctl);

// ----------------------------------------------------- national registry ----
//  v1.6.0: a deterministic offline simulation of the Iranian Civil Registry
//  (ثبت احوال) + insurance enquiry.  validNationalId() does the real Iranian
//  10-digit checksum; lookupCitizen() derives a stable, realistic identity
//  (name, father, gender, birth date, mobile) from the code so the whole
//  reception / appointment workflow runs end-to-end with NO external API and
//  is ready to be swapped for a real web-service call later.
//  v1.7.0: identity is NEVER fabricated. `found` means a TRUSTED source returned
//  a verified record (an online ثبت احوال web-service, or a locally-stored
//  patient previously registered by an operator). When no trusted source can
//  verify the code, `found` stays false and the UI must let the operator type
//  the identity MANUALLY — it must not invent a name, gender, birth-date or
//  insurance. `source` says where the data came from so the UI can show it.
enum CitizenSource {
    CS_NONE = 0,   // nothing known — manual entry required
    CS_LOCAL,      // recalled from a patient previously registered here
    CS_REGISTRY    // returned by a configured online registry web-service
};
struct CitizenInfo {
    bool        found;          // true ONLY when a trusted source verified it
    int         source;         // CitizenSource
    bool        idValid;        // the 10-digit checksum is valid
    bool        lookupTried;    // an online lookup was attempted
    bool        lookupFailed;   // the online lookup failed/was unavailable
    std::wstring firstName, lastName, fatherName, gender, birthDate, mobile;
    std::vector<int> insurances;   // INSURANCES[] indices VERIFIED for this person
    CitizenInfo():found(false),source(CS_NONE),idValid(false),
        lookupTried(false),lookupFailed(false){}
};
bool         validNationalId(const std::wstring& id);
CitizenInfo  lookupCitizen(const std::wstring& nationalId);
//  Persist a verified/manually-confirmed patient locally so the SAME national
//  code recalls the SAME real identity next time (no fabrication, no randomness).
void         rememberPatient(const std::wstring& nationalId,
                 const std::wstring& firstName, const std::wstring& lastName,
                 const std::wstring& fatherName, const std::wstring& gender,
                 const std::wstring& birthDate, const std::wstring& mobile,
                 const std::vector<int>& insurances);

//  v1.10.0: a flat, read-only view of one row in the local patient store, used
//  by the admin «بیماران» (patients) tab. Mirrors the data\patients.dat schema:
//      nid | first | last | father | gender | birth | mobile | insCsv
struct PatientRow {
    std::wstring nid, first, last, father, gender, birth, mobile;
    std::vector<int> insurances;
};
//  Load every patient stored locally (newest first — same order the store is
//  written: appended records last, so we reverse to show newest on top).
std::vector<PatientRow> loadAllPatients();
//  Delete a patient record by national code from the local store. Returns true
//  if a row was removed.
bool                    deletePatient(const std::wstring& nationalId);

// ----------------------------------------------- patient import pipeline ----
//  v1.12.0 (§11-13): a dedup-aware bulk import path that feeds the SAME local
//  patient store the reception national-ID auto-fill reads from. Records can
//  originate from an offline-staged CSV (Path B) extracted from a restored SQL
//  Server database, or from the in-app analyzer staging. Matching is by the
//  10-digit national code (the clinical primary key): an incoming row with a
//  code already on file UPDATES that record (newer wins) instead of creating a
//  duplicate; rows with an invalid/empty code are skipped and counted.
struct ImportPatientRow {
    std::wstring nid, first, last, father, gender, birth, mobile;
    std::vector<int> insurances;
};
struct ImportResult {
    int total=0;        // rows seen in the source
    int inserted=0;     // brand-new national codes added
    int updated=0;      // existing codes refreshed (dedup match)
    int skippedInvalid=0;// rows with an invalid/empty national code
    int skippedEmpty=0; // rows with no usable name
    std::wstring error; // non-empty on hard failure (file unreadable etc.)
    bool ok=false;
};
//  Bulk-import a vector of rows into the local store with national-ID dedup.
ImportResult importPatients(const std::vector<ImportPatientRow>& rows);
//  Parse a staged import file (UTF-8/UTF-16). Auto-detects the delimiter
//  (| , ; or TAB) and a header row. Expected columns (header names matched
//  case-insensitively, English or Persian):
//      national_id/کدملی, first/نام, last/خانوادگی, father/پدر,
//      gender/جنسیت, birth/تولد, mobile/موبایل, insurance/بیمه
//  Columns may also be positional in the canonical order above. Never throws.
std::vector<ImportPatientRow> parsePatientImportFile(const std::wstring& path,
                                                     std::wstring& parseError);
//  Convenience: parse + import a staged file in one call (Path B offline).
ImportResult importPatientsFromFile(const std::wstring& path);

// ----------------------------------------------------------- doctors --------
//  Doctors & their services for the appointment screen (file-backed, seeded
//  with realistic Iranian specialties so the workflow is usable out-of-box).
struct DoctorDef { std::wstring name, specialty; std::vector<std::wstring> services; };
std::vector<DoctorDef> loadDoctors();          // seeds defaults if empty
std::vector<DoctorDef> todaysDoctors();        // doctors on shift today

// ----------------------------------------------------------- appointments ---
struct Appointment {
    std::wstring nationalId, firstName, lastName, mobile;
    std::wstring doctor, service, apptDate, apptTime, day, shift;
    std::wstring kind;        // حضوری / غیرحضوری
    std::wstring user;        // who registered it
    int  queueNo;
    bool cancelled;
    Appointment():queueNo(0),cancelled(false){}
};
std::vector<Appointment> loadAppointments(bool includeCancelled);
int  saveAppointment(Appointment& a);          // assigns queue no, persists
bool cancelAppointment(int index);             // marks cancelled by list index
bool updateAppointment(int index, const Appointment& a);
std::vector<Appointment> searchAppointments(const std::wstring& nid,
        const std::wstring& mobile, const std::wstring& fn,
        const std::wstring& ln, bool includeCancelled);

// ----------------------------------------------------- profile-change reqs --
//  v1.6.0: a full profile-change request workflow (reception → management).
//  data\profreq.dat: user|oldName|newName|oldPhoto|newPhoto|time|status|reason
//   status: 0=pending 1=approved 2=rejected
struct ProfReq {
    std::wstring user, oldName, newName, oldPhoto, newPhoto, time, reason;
    int status;
    ProfReq():status(0){}
};
std::vector<ProfReq> loadProfReqs();
void pushProfReq(const ProfReq& r);
void setProfReqStatus(int indexNewestFirst, int status, const std::wstring& reason);
int  unseenProfReqCount();      // pending count (for the manager badge)

// ----------------------------------------------------------- cartable v2 ----
//  Message actions: pin / seen / delete, all reported back to the manager.
void  pinMessage(const std::wstring& forUser, int indexNewestFirst, bool pin);
void  seenOneMessage(const std::wstring& forUser, int indexNewestFirst);
void  deleteOneMessage(const std::wstring& forUser, int indexNewestFirst);
