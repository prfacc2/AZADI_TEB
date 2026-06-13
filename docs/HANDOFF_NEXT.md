# گزارش تحویل برای مدل بعدی — آزادی طب (Azadi-Teb) v1.6.0

> این فایل را قبل از هر کاری بخوان. وضعیت دقیق پروژه، کارهای انجام‌شده،
> کارهای باقی‌مانده، و نقطه‌ی دقیق ادامه را توضیح می‌دهد.

---

## ۱) معرفی پروژه
- **آزادی طب**: نرم‌افزار دسکتاپ پذیرش/مدیریت درمانگاه، فارسی و راست‌به‌چپ (RTL).
- **زبان/فناوری**: C++17 خالص با Win32 API (بدون Qt/MFC/.NET).
- **خروجی**: یک فایل EXE استاتیک (PE32 i686) برای ویندوز ۷ تا ۱۱، در `build/AzadiTeb.exe`.
- **کامپایل**: `i686-w64-mingw32-g++` از طریق `./build.sh` (فلگ‌ها: `-municode -mwindows -static ...`).
- **مخزن**: https://github.com/prfgame/AzadiTeb — برنچ کاری: `genspark_ai_developer` — PR باز: **#3** (`https://github.com/prfgame/AzadiTeb/pull/3`).
- **قواعد مهم کاربر**:
  1. بعد از **هر بخش**، فوراً کامیت + پوش روی گیت‌هاب (نه به‌صورت دسته‌ای).
  2. توضیح/سؤال اضافی به کاربر نده؛ کار را تا ۱۰۰٪ کامل کن.
  3. EXE جدید باید جایگزین `build/AzadiTeb.exe` شود.
  4. **CHANGELOG را فقط بعد از آپلود فایل‌ها** به‌روز کن.
  5. RTL دستی است (بدون `WS_EX_LAYOUTRTL`).

---

## ۲) معماری و نکات فنی کلیدی
- **RTL دستی** + `DT_RTLREADING`. مقیاس واکنش‌گرا با ماکرو `S(int)` و `g_scale`.
- **تم**: `g_theme` (پالت) + `applyTheme`/`broadcastThemeChange`/`WM_APP_THEME`.
- **GDI+ helpers**: `gpRoundRect`, `gpGradRoundRect`, `gpShadow`, `gpFillAlpha`, `gpLine`, `fillRoundRect`.
- **دکمه‌های flat owner-drawn**: `createFlatButton` (سبک‌ها: `BS_GHOST/PRIMARY/DANGER/OUTLINE/CARD`)، `setFlatButtonBg`، `drawIcon` با enum `IconId`.
- **کمبوی تم‌دار owner-draw**: `createThemedCombo` + `drawThemedComboItem` (در `theme.cpp`؛ هندل `WM_DRAWITEM`).
- **لایه‌ی داده فایل‌محور** در پوشه‌ی `data/` (UTF-8، جداکننده `|`).
- **دیالوگ‌های مودال**: `runModal` در `dialogs.cpp` **static است (export نشده)** — حلقه‌ی پیام تودرتو + `EnableWindow`. الگوهای مرجع: `showLoginDialog`, `showShiftDialog`. تابع جدید `showProfileDialog` هم همین `runModal` را استفاده می‌کند.
- **تقویم جلالی**: `iranNow` (UTC+3:30)، `gregToJalali`، `jalaliDateShort`، `toFaDigits`.
- **پذیرش**: تب‌های مرورگرگونه (enum `TabKind`)، هر تب یک پنجره‌ی فرزند. ترتیب تب‌ها در `createReceptionScreen`: **نوبت‌دهی → پذیرش → کارتابل** (TK_APPOINTMENT اول).
- **بیمه‌ها**: `INSURANCES[7]` و `SUPP_INSURANCES[10]` در `billing.cpp`.

---

## ۳) کارهای انجام‌شده (کامل و کامیت‌شده)
ترتیب کامیت‌ها روی `genspark_ai_developer` (جدید به قدیم):

1. **`ef0061a` — طراح چاپ (printer-designer)**: 
   - استک Undo (۵۰ مرحله) با `dsPushUndo()`/`dsUndo()`. اسنپ‌شات قبل از: شروع درگ آیتم، افزودن، حذف (دکمه + کلید Delete)، قالب قبلی/بعدی، تغییر کاغذ، بایند فیلد، و toggleهای bold/italic/underline/strike/align.
   - **Ctrl+Z** = undo.
   - **چرخ ماوس ساده** = اسکرول عمودی صفحه (Shift = افقی)؛ **Ctrl+چرخ** = زوم به مکان مکان‌نما (مثل قبل).
   - **PageUp/PageDown** = اسکرول صفحه بالا/پایین.
   - درگ-پن روی بوم خالی و پن با دکمه‌ی وسط ماوس از قبل بود.
   - فایل: `src/printer_designer.inc`.

2. **`56a2e7b` — کارتابل نسخه ۲ (cartable v2)**:
   - بازنویسی `drawCartable` در `reception.cpp`: پیام‌ها به‌صورت **کاشی‌های مستطیلی کنار هم** که در ردیف‌ها می‌پیچند (۱ تا ۴ ستون واکنش‌گرا)، نه خطوط تمام‌عرض. پس‌زمینه‌ی تیره‌ی یکدست (بدون نشت گرادیان).
   - هر کاشی: برچسب شدت، فرستنده، **تاریخ ارسال**، متن (۲ خط)، نقطه‌ی نادیده، نوار رنگی شدت، و نشان 📌 پین.
   - مرتب‌سازی: پین‌شده‌ها اول، سپس جدیدترین بر اساس تاریخ.
   - کلیک روی کاشی → منوی راست‌کلیک: **پین کردن/برداشتن پین، علامت دیده‌شده، پاک کردن**. ایندکس نمایش به ایندکس خام (newest-first) نگاشت می‌شود تا لایه‌ی داده (`pinMessage`/`seenOneMessage`/`deleteOneMessage`) درست عمل کند و **به مدیر اطلاع داده شود**.
   - افزودن `<algorithm>`/`<utility>` به `reception.cpp`.

3. **`bef3ef0` — دیالوگ ویرایش پروفایل + تأیید مدیریت**:
   - `showProfileDialog` در `dialogs.cpp`: نام فعلی (فقط‌خواندنی)، تکست‌باکس نام جدید، انتخاب عکس، دکمه‌های تأیید/انصراف + اخطار تأیید. روی تأیید یک `ProfReq` در صف تأیید مدیریت قرار می‌گیرد. پروتوتایپ در `app.h` (`bool showProfileDialog(HWND)`).
   - `settings.cpp` → `doProfile` حالا همین دیالوگ را باز می‌کند.
   - `manage.inc`: دکمه‌ی جدید «درخواست‌های تغییر پروفایل» (`MG_PROFREQS`) + مودال `prProc`/`openProfReqs` با دکمه‌های هر ردیف **تأیید** (سبز → کارتابل سبز + اعمال `name_override_`/`photo_`) و **رد** (قرمز → کارتابل قرمز با دلیل اختیاری از تکست‌باکس). نشان (badge) تعداد در انتظار کنار دکمه (`mgRefreshReqBadge` به‌روزرسانی شد).
   - `users.cpp`: هنگام لاگین، `name_override_<user>` روی `fullname` نشست اعمال می‌شود.

4. **`b2508d2` — پنل راست پذیرش** (قبلی): آواتار مهمان، نسخه الکترونیک، کلیدهای جستجو، بلوک بیمه، پزشک معالج، استعلام چندبیمه‌ای.

5. **`3e1aecc` — تب نوبت‌دهی + ماشین‌حساب در هدر** (قبلی): `appointment.cpp` کامل، نوبت‌دهی به‌عنوان تب اول، ماشین‌حساب سمت چپ هدر، کمبوهای تم‌دار، autodir، استعلام چندبیمه‌ای.

> همه‌ی موارد بالا **کامپایل می‌شوند** و آخرین `./build.sh` موفق بود (`build/AzadiTeb.exe`, PE32 i386, فقط warningهای بی‌ضرر).

---

## ۴) کارهای باقی‌مانده (نقطه‌ی ادامه)
به‌ترتیب اولویت:

### الف) پولیش هدر پنل تنظیمات (settings panel header)
- خواسته: در پنل تنظیمات، بالای صفحه آواتار + نام + نقش کاربر نمایش داده شود.
- فایل: `src/settings.cpp` — تابع `paintPanel` (حدود خط ۱۸۶ به بعد) و ساختار `SetState`/`buildRows`. نام/نقش از `g_session.user.fullname` و `g_session.user.role` موجود است؛ آواتار اگر `photo_<user>` ست شده باشد.

### ب) بمپ نسخه به 1.6.0 (اولویت بالا)
- `src/app.h`: خط `#define APP_VERSION_W L"1.5.0"` → **`L"1.6.0"`**.
- `src/app.rc`: نسخه‌ی منبع را هم 1.6.0 کن (FILEVERSION/PRODUCTVERSION و رشته‌ها).
- `update/version.txt`: خط اول الان `1.3.0` است → **`1.6.0`** (خط دوم URL آپدیت است، دست نزن مگر لازم).

### ج) به‌روزرسانی CHANGELOG (فقط بعد از آپلود فایل‌ها)
- `docs/CHANGELOG.md`: یک ورودی v1.6.0 با خلاصه‌ی همه‌ی بخش‌های بالا اضافه کن.

### د) بیلد نهایی + جایگزینی EXE
- `./build.sh` بزن، مطمئن شو `build/AzadiTeb.exe` تازه است، کامیت/پوش کن.

### ه) (اختیاری/در صورت زمان) بهبودهای صفحه‌ی نوبت‌دهی
- دکمه‌های خدمت F5/F4/F3، «انتقال نوبت»، و «چاپ» در `appointment.cpp` فعلاً placeholder هستند. کاربر این‌ها را به‌صراحت ضروری نکرده، ولی می‌توان تکمیل کرد.
- در `reception.cpp` هندلر `ID_F_DOCSEARCH` (جستجوی پزشک معالج) هم placeholder است.

---

## ۵) فایل‌های مهم و خطوط مرجع
- `src/dialogs.cpp` — `runModal` (static، خط ~۱۸۱)؛ `showProfileDialog` در انتهای فایل (کلاس `AzProfile`، idها 701-705).
- `src/settings.cpp` — `doProfile` (حالا فقط `showProfileDialog` را صدا می‌زند)؛ `paintPanel` برای پولیش هدر.
- `src/manage.inc` — مودال پروفایل: `prProc`/`openProfReqs` (کلاس `AzMgProf`)، دکمه `MG_PROFREQS`، `mgRefreshReqBadge` (badge هر دو نوع درخواست)؛ الگوی مرجع مودال: `mrProc`/`openSetReqs`.
- `src/data_ext.cpp` — لایه‌ی داده آماده: `loadProfReqs/pushProfReq/setProfReqStatus/unseenProfReqCount` (پروفایل)؛ `pinMessage/seenOneMessage/deleteOneMessage` + `rawIndexForUserMsg` (کارتابل v2، انتظار ایندکس **plain newest-first**)؛ `validNationalId/lookupCitizen/loadDoctors/...` و CRUD نوبت‌ها.
- `src/reception.cpp` — `drawCartable`/`cartReload`/`cartHit` (کارتابل v2)، `s_cartMsgs`/`s_cartNF`/`s_cartTiles`؛ هندلر کلیک در `WM_LBUTTONDOWN` تب `TK_PORTAL`؛ پنل راست `paintInfoPanel`/`drawGuestAvatar`؛ `rcMetrics2`.
- `src/printer_designer.inc` — `DesignerState.undo`, `dsPushUndo/dsUndo`, `WM_KEYDOWN` (Ctrl+Z/PageUp/PageDown), `WM_MOUSEWHEEL` (split اسکرول/زوم).
- `src/app.h` — همه‌ی پروتوتایپ‌های v1.6.0 موجود است؛ فقط `APP_VERSION_W` باید بمپ شود.
- `src/users.cpp` — اعمال `name_override_` روی نشست هنگام لاگین.
- `build.sh` — `SRCS` شامل `src/data_ext.cpp` و `src/appointment.cpp`.

---

## ۶) وضعیت بیلد/گیت در لحظه‌ی تحویل
- **آخرین بیلد**: `./build.sh` → موفق، `build/AzadiTeb.exe` (~1.5M, PE32 i386). فقط warningهای `-Wmisleading-indentation`/`unused-function`/`unused-variable`.
- **برنچ**: `genspark_ai_developer` — درخت کاری **تمیز** (همه کامیت/پوش شده).
- **آخرین کامیت پوش‌شده**: `ef0061a`.
- **PR #3** باز است: https://github.com/prfgame/AzadiTeb/pull/3
- **محیط گیت‌هاب** پیکربندی شده (کاربر prfgame).
- **نسخه هنوز 1.5.0 است** (باید به 1.6.0 بمپ شود — بخش ۴-ب).

---

## ۷) پرامپت پیشنهادی برای مدل بعدی (کپی کن و ادامه بده)
> «در پروژه‌ی `/home/user/webapp` (آزادی طب، C++ Win32، برنچ `genspark_ai_developer`)
> از روی `docs/HANDOFF_NEXT.md` ادامه بده. بخش‌های نوبت‌دهی، پنل پذیرش، دیالوگ
> پروفایل + تأیید مدیریت، کارتابل v2، و طراح چاپ (Ctrl+Z/اسکرول/پن) قبلاً انجام
> و پوش شده‌اند. حالا این کارهای باقی‌مانده را به‌ترتیب کامل کن و بعد از هر بخش
> فوراً کامیت+پوش کن: (۱) پولیش هدر پنل تنظیمات (آواتار+نام+نقش) در
> `src/settings.cpp::paintPanel`. (۲) بمپ نسخه به 1.6.0 در `src/app.h`
> (APP_VERSION_W)، `src/app.rc`، و `update/version.txt`. (۳) بیلد نهایی با
> `./build.sh` و اطمینان از تازه‌بودن `build/AzadiTeb.exe`. (۴) فقط بعد از آپلود
> فایل‌ها، یک ورودی v1.6.0 در `docs/CHANGELOG.md` اضافه کن. (۵) همه را کامیت+پوش
> کن و PR #3 را به‌روز نگه دار. RTL دستی است؛ بدون توضیح اضافه، کار را تا ۱۰۰٪
> کامل کن.»

---

_تهیه‌شده در نقطه‌ی توقف به درخواست کاربر — تاریخ سیستم ۲۰۲۶-۰۶-۱۳._
