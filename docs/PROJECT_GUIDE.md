# راهنمای کامل پروژه آزادی طب — برای توسعه‌دهندگان و مدل‌های هوش مصنوعی

این سند «نقشه ذهنی» کامل برنامه است. **قبل از هر تغییری این فایل را کامل بخوانید.**

> **نسخهٔ فعلی: ۱.۱۰.۰** — بازطراحی و پایدارسازی production:
> - **تنظیمات سبک پیام‌رسان** با مسیر اختصاصی مهمان (فقط «تغییر پوستر» + «ارتباط با ما»). دیسپچر `OpenSettings` با `SettingsMode {SM_GUEST, SM_RECEPTION, SM_ADMIN}` در `user_settings.cpp`.
> - **پذیرش بدون انیمیشن**: انیمیشن جمع‌شدن هدر کامل حذف شد؛ چیدمان فشرده فوری. ساعت/ماشین‌حساب سمت **چپ**. (`handlers.cpp`، `main.cpp`، `reception.cpp`)
> - **تم روشن پریمیوم لایه‌ای** با حالت‌های focus/disabled/hover/active روشن (`theme.cpp`).
> - **تحلیل واقعی `.bak` (SQL Server / MTF)** + مهار واقعی SEH با `setjmp/longjmp` در `backup_analyzer.cpp` (فقط ابتدای فایل خوانده می‌شود، نه کل فایل چندگیگابایتی).
> - **پنل کناری پیام‌رسانی کارمندان** گروه‌بندی‌شده بر اساس بخش با کد بخش/کارمند، سقف ۲۰×۲۰ (`manage.inc`). تغییر نام «کارکنان» → «کارمندان».
> - **رفع باز نشدن طراح چاپ** (حذف `PostQuitMessage` از حلقه‌های modal تو‌در‌تو + رفع use-after-free).
> - **ایمنی داده و سازگاری رو‌به‌جلو (§H)**: `setSetting` کامنت/کلیدهای ناشناخته را حفظ می‌کند؛ `User`/`EmpProfile`/`DeptCat` ستون‌ها/کلیدهای ناشناختهٔ نسخه‌های آینده را round-trip می‌کنند و هرگز حذف نمی‌کنند.

## 1. معماری کلی

- **زبان:** C++17 — Win32 API خالص (بدون Qt/MFC/.NET) → یک EXE واحد، سبک و سریع.
- **هدف باینری:** PE32 (i686) با لینک استاتیک کامل → همان یک فایل روی x86 **و** x64 ویندوز 7 تا 11+ اجرا می‌شود.
- **بیلد:** `build.sh` با `i686-w64-mingw32-g++` (کراس‌کامپایل از لینوکس). فلگ‌های مهم:
  `-municode -mwindows -static -static-libgcc -static-libstdc++`
- **داده:** فایل‌محور در پوشه `data/` کنار EXE (آماده مهاجرت به دیتابیس سرور):
  - `users.dat` — کاربران: `username|fullname|dept|role|hash` (هر خط یک کاربر، UTF-8)
  - `settings.ini` — تنظیمات key=value (تم، تیک خودکار شیفت، update_url)
  - `receptions_YYYY-MM-DD.csv` — پذیرش‌های هر روز (تاریخ جلالی، با BOM برای اکسل)
  - `last_receipt.dat` — آخرین قبض برای F8 و چاپ مجدد
- **لاگ:** `logs/app.log` + `logs/crash_*.log`

## 2. فایل‌های سورس و مسئولیت هرکدام

| فایل | مسئولیت |
|---|---|
| `src/app.h` | هدر مشترک: ساختارها (Theme, User, ReceptionRecord, Session)، prototype همه ماژول‌ها |
| `src/main.cpp` | `wWinMain`، پنجره اصلی تمام‌صفحه (WS_POPUP)، **نوار بالا** (نام+لوگو سمت **راست** کنار خروج؛ دکمه‌های تم/آپدیت سمت **چپ**)، نوار پایین (ساعت+تاریخ ایران زنده / شیفت)، صفحه Home با دو کارت، روتینگ Ctrl+P+N و F8، **`enableEnterNavigation`/`hopField`** (Enter و Tab به فیلد بعدی، Shift+Tab به قبلی)، **`enableDateMask`/`dateEditProc`** (ماسک تاریخ جلالی هوشمند YYYY/MM/DD)، محاسبه `g_scale` ریسپانسیو. نکته: بلوک `#ifdef AZ_DEBUG_BUILD` فقط برای تست headless (پرش مستقیم به صفحه با متغیر محیطی `AZ_DEBUG_SCREEN`) — در بیلد محصول (`build.sh`) تعریف نمی‌شود |
| `src/util.cpp` | ساعت ایران (UTC+3:30 ثابت — DST از ۲۰۲۲ حذف شده)، تبدیل میلادی→جلالی، اعداد فارسی، فرمت پول، فایل UTF-8، settings، trim، لاگ |
| `src/handlers.cpp` | **Crash handler** (`SetUnhandledExceptionFilter` → crash log + پیام فارسی)، **Speed handler** (`detectSpec` → `g_lowSpec` وقتی ≤2 هسته یا ≤2.2GB رم)، **نصب فونت وزیر** (لود از RCDATA با `AddFontMemResourceEx` + کپی به `%LOCALAPPDATA%\Microsoft\Windows\Fonts` + ثبت HKCU اگر نصب نبود) |
| `src/theme.cpp` | پالت تم روشن/تیره (`applyTheme`)، براش‌های سراسری، **دکمه Flat سفارشی** (کلاس `AzFlatBtn` با ۵ استایل: GHOST/PRIMARY/DANGER/OUTLINE/CARD)، **آیکون‌های وکتوری** (`drawIcon` — بدون فایل تصویری)، `fillRoundRect` |
| `src/users.cpp` | ذخیره/خواندن کاربران، هش رمز (FNV-1a دوپاس + salt)، `verifyLogin` با کنترل نقش (پذیرش≠مدیریت)، ادمین مخفی `prf/prf123` هاردکد با role=2 |
| `src/billing.cpp` | جدول **بیمه‌های ایران** (`INSURANCES` پایه + `SUPP_INSURANCES` مکمل با درصد سهم)، **تعرفهٔ خودکار ویزیت** (`VISIT_TARIFF[3]`=عادی/سرپایی/بستری، `applyApptTariff` برای VIP×۱۵۰٪ و تخفیف‌دار×۵۰٪، `defaultServicePrice`)، ذخیره پذیرش در CSV روزانه + شماره نوبت، `last_receipt`، **چاپ واقعی GDI** (`printReceipt` با `PrintDlgW`/پرینتر پیش‌فرض — ۳ نوع: رسید بیمه=0، نسخه=1، قبض=2) |
| `src/calculator.cpp` | ماشین حساب مستقل Always-on-Top (کلاس `AzCalc`)، گرید ۵×۴ owner-draw، پشتیبانی کیبورد/Numpad/ماوس، عملیات: + − × ÷ % √ x² 1/x ± ⌫ |
| `src/dialogs.cpp` | **دیالوگ لاگین** سبک ویندوز ۱۱ (کارت گرد وسط + لایه dim + انیمیشن shake خطا) با حلقه پیام modal خودی (`runModal`)، **دیالوگ شیفت** (تشخیص خودکار، تیک خودکار با حافظه در settings، انتخاب دستی) |
| `src/admin.cpp` | صفحه پنل مخفی ادمین: فرم ساخت کاربر (نام شخص/نام کاربری/رمز/بخش تایپی/نوع دسترسی) + ListView جدول کاربران + حذف؛ صفحه مدیریت (placeholder قابل گسترش) |
| `src/reception.cpp` | فضای کاری پذیرش: نوار ابزار (**پذیرش جدید** = `ID_RC_NEWPAT` که `resetForm` در همان تب را صدا می‌زند؛ **تب جدید**؛ ماشین حساب)، **نوار تب مرورگری** (رسم دستی: باز/بستن/**جدا کردن به پنجره مستقل** `AzDetached`)، **فرم کارت‌محور بخش‌بندی‌شده** (`rcMetrics`/`rcVMetrics`/`tabPageLayout`/`WM_PAINT` — کارت مشخصات + بخش‌های آیکون‌دار + قاب دور هر فیلد)، ناوبری Enter و Tab به علاوهٔ ماسک تاریخ روی `eBirth`، **کارت صدور قبض** با محاسبهٔ **خودکار** (`recalc` که `defaultServicePrice` را با گارد `autoPrice` پر می‌کند) و چیپ سبز «پرداختی»، دکمه‌های چاپ |
| `src/webhost.{h,cpp}` + `webhost_host.inc` / `webhost_run.inc` / `webhost_bridge.inc` / `webhost_assets.inc` | **میزبان هایبرید HTML/CSS/JS (§3، نسخه 1.13.0)**: میزبانی کنترل سیستمیِ `WebBrowser`/`MSHTML` (Trident) بدون ATL — `IOleClientSite`/`IOleInPlaceSite`/`IOleInPlaceFrame`/`IDocHostUIHandler` + sink رویدادهای `DWebBrowserEvents2` + پل `IDispatch` به‌صورت `window.external`. پل **هم‌زمان** است: `window.external.call(verb, jsonArgs)` → پاسخ JSON؛ و C++→JS با `window.azReceive`. هنگام باز شدن، **لودر وسط‌چین با نوار پیشرفت** نمایش داده می‌شود تا وضعیت نیتیو/متادیتای بخش‌ها/پیکربندی فرم همگام شود، سپس UI رندر می‌شود. تمام اعتبارسنجی/محاسبه تعرفه/ذخیره‌سازی **سمت C++** (`WebHostBridge_Call`: `lookup`/`bill`/`saveReception`/`print`/`printLast`/`apptList`/`apptNext`/`saveAppointment`/`cancelAppointment`/`setTheme`/...) انجام می‌شود؛ JS هرگز نتیجه را جعل نمی‌کند. اگر کنترل در دسترس نباشد یا خطا بدهد، به فرم نیتیو کلاسیک **fallback قطعی** می‌شود و علت در `logs\webhost_errors.log` ثبت می‌گردد |
| `src/update.cpp` | **آپدیت از راه دور**: WinINet → دانلود `version.txt` (خط۱=نسخه، خط۲=URL exe) → مقایسه نسخه → دانلود `AzadiTeb_new.exe` → `update.bat` جایگزینی پس از خروج |
| `src/app.rc` | فونت‌های تعبیه‌شده (ID 101/102)، منیفست، VERSIONINFO |
| `src/app.manifest` | ComCtl32 v6، سازگاری Win7→11، DPI-aware، asInvoker |

## 3. جریان‌های اصلی برنامه (Flows)

### ورود پذیرش
Home → کارت «پذیرش درمانگاه» → لاگین (role=0) → دیالوگ شیفت (خودکار/دستی) → `SC_RECEPTION` با یک تب باز.

### پنل مخفی ادمین
صفحه Home → نگه داشتن `Ctrl+P+N` → لاگین role=2 (`prf`/`prf123`) → `SC_ADMIN`.

### قاعده مهم شیفت
شیفت در لحظه ورود انتخاب/تشخیص داده می‌شود و در `g_session.shift` می‌ماند. **هرگز با گذر زمان باطل نمی‌شود** — فقط دکمه خروج (✕ بالا) سشن را می‌بندد. یک کاربر می‌تواند در هر ۳ شیفت کار کند. مرزهای شیفت: صبح ۶:۰۰–۱۴:۳۰، عصر ۱۴:۳۰–۲۲:۳۰، شب ۲۲:۳۰–۶:۰۰. وضعیت تیک «حالت خودکار» در `settings.ini` کلید `shift_auto` ذخیره و دفعه بعد بازیابی می‌شود.

### محاسبه خودکار قبض (billing)
ابتدا اگر مبلغ خدمت خالی یا صفر باشد، **خودکار پر می‌شود**: `مبلغ خدمت = applyApptTariff(VISIT_TARIFF[نوع بیمار]، نوع نوبت)` — عادی=۲.۵M / سرپایی=۳.۵M / بستری=۸M ریال و VIP×۱.۵ / تخفیف‌دار×۰.۵. سپس:
```
سهم بیمه اصلی    = مبلغ × درصد بیمه پایه
مابه‌التفاوت پایه  = مبلغ − سهم بیمه اصلی
سهم سازمان مکمل  = مابه‌التفاوت × درصد بیمه مکمل
سهم بیمار        = مابه‌التفاوت − سهم سازمان
پرداختی          = سهم بیمار − تخفیف   (حداقل صفر)
```
گارد `autoPrice` از حلقهٔ بی‌نهایت `recalc` → `SetWindowTextW` → `EN_CHANGE` → `recalc` جلوگیری می‌کند. تغییر «نوع بیمار» یا «نوع نوبت» مبلغ را پاک و دوباره خودکار محاسبه می‌کند.

### نکته فنی RTL
RTL به‌صورت **دستی** پیاده شده و **`WS_EX_LAYOUTRTL` استفاده نمی‌شود** (برای پرهیز از باگ‌های آینه‌شدن GDI). چیدمان فیلدها در `tabPageLayout` دستی راست‌چین می‌شود و متن‌ها با پرچم `DT_RTLREADING` رسم می‌شوند. نوار تب هم دستی رسم می‌شود.

### نکته فنی دیالوگ‌های modal
`showLoginDialog` / `showShiftDialog` پنجره‌ی child تمام‌صفحه می‌سازند و با `runModal` (حلقه پیام تو در تو + `EnableWindow`) بلاک می‌کنند. `WM_DESTROY` آن‌ها `PostQuitMessage` می‌زند که داخل `runModal` مصرف می‌شود — **به حلقه اصلی نمی‌رسد**.

## 4. قوانین توسعه (الزامی)

1. هیچ قابلیت موجود را حذف نکنید / رفتار آن را عوض نکنید مگر صریحاً خواسته شود.
2. هر تغییر → یک ورودی جدید در `docs/CHANGELOG.md`.
3. شماره نسخه را در `src/app.h` (`APP_VERSION_W`) و `src/app.rc` همگام بالا ببرید.
4. متن‌های UI فارسی + از `toFaDigits` برای اعداد نمایشی استفاده کنید.
5. رنگ جدید فقط از `g_theme` — هر دو تم (روشن/تیره) را بررسی کنید؛ تداخل رنگ متن و پس‌زمینه ممنوع.
6. کنترل جدید: فونت `g_fUI`، اندازه‌ها با `S()` (مقیاس ریسپانسیو)، ادیت‌باکس‌ها `enableEnterNavigation` (و `enableDateMask` برای فیلدهای تاریخ). برای آیکون از `drawIcon` برداری استفاده کنید — **ایموجی رنگی پشتیبانی نمی‌شود** (فونت وزیر گلیف رنگی ندارد).
7. بعد از تغییر: `./build.sh` بدون خطا + کپی EXE جدید در `build/`.
8. برای انتشار نسخه جدید از راه دور: `update/version.txt` را هم به‌روز کنید.

## 5. مسیر گسترش به سرور

لایه داده (users/billing/settings) عمداً پشت توابع ساده (`loadUsers`, `saveReception`, …) ایزوله شده. برای سرور:
- همین امضاها را نگه دارید و پیاده‌سازی را به REST/SQL تغییر دهید (مثلاً `datasource.cpp` جدید).
- WinINet از قبل لینک شده (`update.cpp` نمونه GET دارد).
- ساختار `ReceptionRecord` معادل یک ردیف جدول پذیرش سرور طراحی شده.
