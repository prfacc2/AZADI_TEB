# تاریخچه تغییرات (CHANGELOG)

> قانون: هر تغییری در کد، باید یک ورودی جدید در این فایل ثبت کند:
> نسخه، تاریخ، چه چیزی اضافه/تغییر/رفع شد، و در کدام فایل‌ها.

---

## 1.46.0 — 1405-04-22

**BREAKING ARCHITECTURE CHANGE — the reception «پذیرش بیمار» page is now
rendered 100% by native Win32 GDI, exactly as it was before v1.13.0.**

Why:
- Every attempt (v1.42→v1.45) to stabilise the embedded MSHTML/WebView2
  admission page failed. The root cause is the loopback-HTTP JS↔C++ bridge:
  MSHTML's per-host connection cap, WinINet buffering of small localhost
  responses, and endpoint-security products throttling same-process loopback
  traffic combine to freeze the UI thread on the operator's real hardware.
- .NET / C# was evaluated and rejected: it would break the single-static-EXE
  guarantee, require a .NET runtime, and forces Windows-only build tooling.
- The native GDI reception form already exists in reception.cpp (~4200 lines),
  was designed pixel-match to the reference mock (v1.18–v1.31 comments), and
  had been retained as the "fallback" path. In v1.46 it becomes the ONLY path.

What changed:
- Deleted: assets/admission/{admission.js,bridge.js,contextmenu.js,admission.css,index.html,vazir.ttf},
  src/web_admission.{cpp,h}, src/web_admission_{api,dispatch,host,http,mshtml,webview2}.inc,
  RCDATA 400–405, and the "prefer web admission" branch in reception.cpp.
- Kept: the print-designer WebView (opened only on demand from Management —
  never freezes reception because it never runs on that tab).
- Kept: every C++ data-layer function (billing, services, insurance, employees,
  print pipeline, backup, appointment). All calculations still run in C++.
- Kept: 30 reception templates + print designer table element + tokens from v1.44.
- The reception tab now opens INSTANTLY (no browser boot, no loopback wait,
  no WebView2 environment creation) and cannot freeze because it uses only
  standard Win32 message dispatch.

Look and feel: unchanged. Same three-column RTL layout, same insurance panel,
same services table, same unpaid queue, same action buttons.

---

## 1.45.0 — 1405-04-22

> **رفعِ ریشه‌ایِ قفل‌شدنِ برنامه پس از افزودنِ خدمت (تلاشِ نسخهٔ ۱.۴۴ اوضاع را
> بدتر کرده بود).**

**علتِ اصلی (ریشه‌ای):**
- نسخهٔ قبلی یک Vectored Exception Handler به‌همراهِ `setjmp/longjmp` نصب کرده بود
  تا استثناهایِ ساختاری را «مهار» کند. اما `longjmp` از داخلِ یک VEH روی ویندوز
  **رفتارِ تعریف‌نشده (UB)** است: زنجیرهٔ dispatchِ SEH در ntdll و فیلدهایِ TEB
  را خراب می‌کند، قفل‌هایِ critical-section را نگه می‌دارد و باعث می‌شود اولین
  فراخوانیِ بعدیِ worker برایِ همیشه در ntdll بلاک شود — یعنی همان قفل‌شدنِ کاملِ
  برنامه که کاربر گزارش کرده بود (حتی دکمهٔ بستن هم کار نمی‌کرد).

**تغییرات:**
- حذفِ کاملِ داربستِ VEH + `setjmp/longjmp` از `src/web_admission_api.inc`؛
  `admissionApiSafe` اکنون تنها بر `admissionApiInner` (با `try/catch`ِ ++C) تکیه
  می‌کند. — `src/web_admission_api.inc`.
- حذفِ همان الگویِ UB (VEH + `longjmp`) از تحلیل‌گرِ پشتیبان. — `src/backup_analyzer.cpp`.
- تنها **یک** VEH در کلِ برنامه باقی مانده که فقط لاگ می‌گیرد و هرگز stack را
  unwind نمی‌کند (`vehLogFilter`). — `src/handlers.cpp`.
- افزودنِ `try/catch(std::exception&)/catch(...)` در هر job از استخرِ نخ به‌همراهِ
  یک خطِ breadcrumb در `logs\client.log`. — `src/web_thread_pool.cpp`.
- کوتاه‌کردنِ تایم‌اوتِ ترابری HTTP و WebView از ۱۵ ثانیه به **۸ ثانیه** تا UI
  هرگز بیش از ۸ ثانیه قفل نماند. — `assets/admission/bridge.js`.
- افزودنِ شبکهٔ ایمنیِ سراسریِ `window.onerror` در پذیرش. — `assets/admission/admission.js`.
- سخت‌سازیِ بیشترِ گاردهایِ حلقهٔ پارسر: افزودنِ گاردِ صریحِ forward-progress
  (`lastP`) و سقفِ اسکنِ داخلی (`qGuard`) در `adComputeBill`. — `src/web_admission_api.inc`.
- گاردِ شمارندهٔ حلقهٔ accept برایِ حالتِ طوفانِ INVALID_SOCKET. — `src/web_admission_http.inc`.
- افزودنِ گام‌هایِ `[verify]` به `build.sh` (غیرمسدودکننده). — `build.sh`.
- تمامِ قابلیت‌هایِ نسخهٔ ۱.۴۴ (۳۰ قالب، جدول‌ها، توکن‌ها، پیش‌نمایش، چاپِ
  باکیفیت) حفظ شده‌اند.

---

## 1.44.0 — 2026-07-13

> **رفعِ ریشه‌ایِ قفل‌شدنِ پذیرشِ وب هنگامِ افزودنِ خدمت + لاگِ ساختاریافتهٔ
> رویدادهایِ غیرعادی + بازطراحیِ حرفه‌ایِ «طراحِ چاپ» با المانِ جدولِ کامل و
> ۱۴ توکنِ تازه و پیش‌نمایشِ زنده + ۳۰ قالبِ پذیرش با تضمینِ کامپایلی + جریانِ
> چاپِ باکیفیت با آنتی‌آلیاسِ DPI-scaled + سخت‌سازیِ عمومیِ برنامه (VEH، دیده‌بانِ
> نخِ UI، WM_ENDSESSION، گاردهایِ حلقهٔ پارسر).**

**۱-۲) رفعِ قفل‌شدنِ پذیرش هنگامِ افزودنِ خدمت — JS/C++**
- `_billBusy` دیگر قفلِ دائمی نمی‌سازد: افزودنِ watchdog + شمارندهٔ توالی (seq) +
  `setTimeout(0)` برای آزادسازیِ حلقهٔ رویداد. — `assets/admission/admission.js`.
- `callHttp` با تایم‌اوتِ ۱۵ ثانیه‌ای در برابرِ بلاک‌شدنِ نامحدود. —
  `assets/admission/bridge.js`.
- `admissionApiSafe` اکنون با لفافهٔ VEH + `setjmp/longjmp` استثناهایِ ساختاری را
  (که در GCC/DWARF فاقدِ `__try/__except` است) مهار می‌کند و به‌جایِ کشتنِ نخ،
  JSONِ خطا برمی‌گرداند. — `src/web_admission_api.inc`.
- `bill.compute` پارسر: سقفِ تکرار + تضمینِ پیشرویِ رو‌به‌جلو (forward-progress)
  در برابرِ حلقهٔ بی‌نهایت. — `src/web_admission_api.inc`, `src/manage.inc`,
  `src/printer.cpp`.
- بازآراییِ کشِ خدمات با `snapshotServices()` برایِ رفعِ re-entrancy. —
  `src/services.cpp`, `src/app.h`.
- رفعِ سرریزِ cast در پذیرشِ بومی (۶۴-بیتی) + بررسیِ `WM_PAINT`. —
  `src/reception.cpp`.
- تایم‌اوتِ سوکتِ loopback (۳۰۰۰ms) + ثبتِ هشدارِ `WSAETIMEDOUT`. —
  `src/web_admission_http.inc`.

**۳) لاگِ ساختاریافتهٔ رویدادهایِ غیرعادی — C++/JS**
- افزودنِ `src/client_log.cpp`/`.h`: نوشتنِ JSON-per-line در `logs\client.log`
  فقط هنگامِ رویدادهایِ غیرعادی (warn/error/crash)؛ نویزِ «page loaded» حذف شد.
  نوشتنِ هم‌زمان-امن با `FILE_SHARE_READ|WRITE` و retry پس از `Sleep(3)` روی
  `ERROR_SHARING_VIOLATION`؛ اتصالِ crash-handler. — `src/client_log.cpp`,
  `src/client_log.h`, `src/handlers.cpp`, `assets/shell/common.js`.

**۴) طراحِ چاپ — HTML/JS/C++**
- ۱۴ نوعِ توکنِ جدید (`{{svc.price}}`, `{{ins.base.pct}}` و…). —
  `assets/designer/fields.js`, `src/printer.cpp`.
- المانِ حرفه‌ایِ TABLE با بازرس (rows/cols/header/stripe/border/headerBg/
  padding/colAlign/زیرنوعِ خدمات‌درمقابلِ‌ثابت) + سریال‌سازی با `tables`. —
  `assets/designer/designer.js`, `designer.css`, `index.html`, `src/printer.cpp`.
- پیش‌نمایشِ زنده (`btnPreview` + `PREVIEW_SAMPLE` + `resolvePreview` +
  `.preview-mode`). — `assets/designer/designer.js`, `designer.css`, `index.html`.

**۵) ۳۰ قالبِ پذیرش — C++/JS**
- دقیقاً ۳۰ ورودیِ `RECEPTION_TEMPLATES[]` با
  `static_assert(count==30)` (تضمینِ کامپایلی) + گالریِ شبکه‌ای با فیلترِ
  «بخش = پذیرش». — `src/print_designer_templates.inc`,
  `assets/designer/templates.js`.

**۶) جریانِ چاپِ باکیفیت — C++**
- پس از «ثبت پذیرش و صدور قبض»: محاسبه + ذخیره + چاپ با آنتی‌آلیاسِ GDI/GDI+
  و DPI-scaled؛ `pdSetHighQuality` (GM_ADVANCED + HALFTONE)، خطوطِ آنتی‌آلیاس با
  `gpLine`، و پایدارسازیِ چاپگرِ انتخابی با `pdPersistPickedPrinter`. —
  `src/printer.cpp`, `src/gdiplus.cpp`.

**۷) سخت‌سازیِ عمومی — C++**
- `WM_ENDSESSION`، `AddVectoredExceptionHandler`، دیده‌بانِ نخِ UI (۵ ثانیه با
  `SendMessageTimeout`/`SMTO_ABORTIFHUNG`)، و گاردهایِ حلقه. — `src/main.cpp`,
  `src/handlers.cpp`.

**۸) ساخت/انتشار**
- افزودنِ `src/client_log.cpp` به `SRCS` در `build.sh`؛ ساختِ موفقِ EXEِ
  ۳۲-بیتیِ استاتیکِ معتبر (PE32، >۵۰۰KB) با
  `i686-w64-mingw32-g++`. — `build.sh`.

---

## 1.43.0 — 2026-07-12

> **رفعِ عدم‌تکمیلِ خودکارِ اطلاعاتِ بیمار (جنسیت/موبایل) + سخت‌سازیِ ضدِ قفل‌شدنِ
> پذیرش + محاسبهٔ معتبرِ صورت‌حساب در C++ + هم‌گام‌سازیِ پوستهٔ تیره با صفحهٔ پذیرش +
> بخشِ جدیدِ «مدیریت بیمه‌ها» با درصدِ پوشش و نوعِ پایه/تکمیلی.**

**۱) رفعِ باگِ عدم‌پرشدنِ جنسیت و موبایل پس از واردکردنِ کد ملی — HTML/JS**
- علت: گزینه‌هایِ کشویِ «جنسیت» مقدارِ `value=` نداشتند و مقدارِ بازگشتی از رکوردِ
  بیمار ممکن بود دارایِ نشانه‌هایِ جهت (ZWNJ/LRM/RLM) یا نمایشِ غیرِفارسی باشد؛
  همچنین موبایل با ارقامِ انگلیسی/فارسیِ ناهمگون پر نمی‌شد.
- رفع: افزودنِ `value="مرد"/"زن"` به گزینه‌ها + تابعِ نرمال‌سازِ `setGender()` که
  مرد/زن/male/female/m/f/۱/۲… را تشخیص و انتخاب می‌کند، و نرمال‌سازیِ ارقامِ موبایل/
  تلفن. اکنون جنسیت، موبایل، تلفن، آدرس و بیمه به‌درستی auto-fill می‌شوند. —
  `assets/admission/index.html`, `assets/admission/admission.js`.

**۲) سخت‌سازیِ ضدِ قفل‌شدنِ پذیرش پس از افزودنِ خدمت — C++**
- افزودنِ لایهٔ محافظِ `admissionApiSafe()` (try/catch) رویِ هر دو ترابرِ IPC
  (WebView2 و HTTPِ loopback): هر استثنایِ مدیریت‌نشده به‌جایِ کشتنِ نخِ کارگر/
  ری‌اِنترِ پیام‌حلقه (که منجر به قفل می‌شد) اکنون به‌صورتِ JSONِ خطا برمی‌گردد. —
  `src/web_admission_api.inc`, `src/web_admission_http.inc`, `src/web_admission_host.inc`.
- بالابردنِ کفِ تعدادِ نخ‌هایِ استخرِ کارگر (۴ رویِ سیستمِ ضعیف، ۶ در حالتِ عادی)
  تا در سیستم‌هایِ ضعیف نیز گرسنگیِ نخ (starvation) رخ ندهد. — `src/web_thread_pool.cpp`.

**۳) محاسبهٔ معتبرِ صورت‌حساب در C++ (رفعِ مبلغِ اشتباه) — C++/JS**
- JS ابتدا نتیجهٔ محلیِ فوری را نمایش می‌دهد، سپس به‌صورتِ ناهم‌زمان (debounced)
  `bill.compute`ِ C++ را صدا می‌زند و همهٔ مبالغ را با نتیجهٔ معتبرِ C++ آشتی می‌دهد؛
  UI هرگز بلاک نمی‌شود. — `assets/admission/admission.js` (`scheduleServerBill`).

**۴) هم‌گام‌سازیِ پوستهٔ تیره با صفحهٔ پذیرش — C++/CSS/JS**
- پیش‌تر صفحهٔ HTMLِ پذیرش هیچ منطقِ پوسته‌ای نداشت. اکنون C++ در `init` مقدارِ
  `theme` را می‌فرستد و هنگامِ تغییرِ پوسته رویدادِ `theme` را push می‌کند؛ JS با
  `applyTheme()` کلاسِ تیره/روشن را روی `html/body` می‌گذارد و ~۲۰۰ خط CSSِ تیرهٔ
  خوانا/واکنش‌گرا اعمال می‌شود. تعویضِ مکررِ تیره/روشن دیگر گلیچ نمی‌سازد. —
  `src/theme.cpp`, `src/web_admission_api.inc`, `assets/admission/admission.js`,
  `assets/admission/admission.css`.

**۵) بخشِ جدید «مدیریت بیمه‌ها» — C++**
- افزودنِ فروشگاهِ ویرایش‌پذیرِ بیمه‌ها (`data/insurances.dat`) با ستون‌هایِ
  نام | درصدِ پوشش | نوع(پایه/تکمیلی) | وضعیت؛ در اولین اجرا از جدولِ داخلی seed
  می‌شود. — `src/insurance.cpp`, `src/app.h`.
- صفحهٔ بومیِ «مدیریت بیمه‌ها» در پنلِ مدیریت (فرمِ افزودن/ویرایش/حذف + جستجو +
  جدول)؛ افزودن/ویرایشِ درصدِ پوشش و نوعِ بیمه به‌صورتِ زنده به پذیرش push می‌شود. —
  `src/manage.inc` (صفحهٔ `PG_INSURANCE`).
- پذیرش و همهٔ محاسباتِ صورت‌حساب اکنون درصدِ پوششِ بیمهٔ پایه/تکمیلی را از همین
  فروشگاه می‌خوانند (`insBasePctAt`/`insSuppPctAt`)؛ اندیس‌هایِ کشویِ وب دقیقاً با
  C++ هم‌راستا هستند. — `src/web_admission_api.inc`.

**۶) قیمتِ خدمات به ریال + تاریخِ تولدِ تاریخی**
- ستون/برچسبِ مبلغ در «مدیریت خدمات» به «مبلغ (ریال)»/«مبلغ پایه (ریال)» است.
- فیلدِ تاریخِ تولد در پذیرش ورودیِ تاریخِ جلالیِ ماسک‌دار است (مناسبِ ایران).

**فایل‌ها:** `assets/admission/{index.html,admission.js,admission.css}`,
`src/{insurance.cpp,manage.inc,app.h,app.rc,theme.cpp,web_admission_api.inc,
web_admission_http.inc,web_admission_host.inc,web_thread_pool.cpp}`, `build.sh`.

---

## 1.42.0 — 2026-07-12

> **رفعِ باگِ قفل‌شدنِ برنامه پس از افزودنِ خدمت + بازطراحیِ انتخابگرِ خدمت +
> کارایی برای هزاران خدمت + بهبودِ صفحهٔ «مدیریت خدمات» + تأییدِ چاپِ متنی
> (نه عکس) و مطابق با «طراحی چاپگر».**

**۱) رفعِ ریشه‌ایِ قفل‌شدن (freeze) پس از ثبتِ پذیرش/چاپ — C++**
- علت: هندلرهای `admission.save` و `print.*` عملیاتِ چاپ (که ممکن است دیالوگِ
  مودالِ `PrintDlgW` باز کند و DCِ چاپگر بسازد) را **به‌صورت هم‌زمان (synchronous)**
  داخلِ callbackِ `WebMessageReceived`ِ WebView2 (رویِ نخِ رابط‌کاربری) یا رویِ نخِ
  کارگرِ HTTP اجرا می‌کردند. این کار WebView را **deadlock** می‌کرد → همان علامتِ
  «همه‌چیز قفل شد، هیچ دکمه‌ای کار نمی‌کند».
- رفع: کلِّ کارِ چاپ + بازکردنِ کشویِ پول (cash-drawer) با `RunOnUiThread`
  (PostMessageِ ناهم‌زمان به نخِ GUI) **به تعویق** انداخته شد و پاسخِ
  `{"ok":true,"printMode":"deferred"}` بی‌درنگ به JS برگردانده می‌شود؛ رابط‌کاربری
  دیگر هرگز قفل نمی‌شود. — `src/web_admission_api.inc` (verbهای `admission.save` و
  `print.last/print.insurance/print.rx/print.receipt`).

**۲) کارایی برای هزاران خدمت — C++**
- `loadServices()` پیش‌تر در هر بار صدا زدن، کلِّ فایلِ `data/services.dat` را
  دوباره می‌خواند و پارس می‌کرد (در هر ضربهٔ کلید در جستجویِ خدمت و در هر ردیفِ
  صورت‌حساب). با هزاران خدمت این باعثِ لگ/افتِ فریم می‌شد.
- افزودنِ **کشِ درون‌حافظه‌ای با ابطالِ مبتنی بر زمانِ آخرین‌نگارشِ فایل (FILETIME)
  + اندازهٔ فایل**؛ در صورتِ عدم‌تغییرِ فایل، بازگشت در O(1). `saveServices()` کش را
  باطل می‌کند. مقاوم در برابرِ چند نخ (mutex). — `src/services.cpp`.

**۳) بازطراحیِ انتخابگرِ خدمت — یک مینی‌پاپ‌آپِ (modal) شکیل — HTML/CSS/JS**
- دکمهٔ «افزودن خدمت» اکنون یک پنجرهٔ کوچکِ حرفه‌ای باز می‌کند: ورودِ **کد خدمت**،
  جستجویِ زندهٔ نام/کد، جدولِ نتایجِ چسبنده‌سر، افزودن با کلیک/Enter/کلیدهایِ جهت،
  و افزودنِ چندبارهٔ سریع. — `assets/admission/index.html`,
  `assets/admission/admission.css`, `assets/admission/admission.js`.

**۴) صفحهٔ «مدیریت خدمات» — رفعِ نمایشِ خالی (هیچی)**
- فهرستِ `LVS_OWNERDATA` گاهی پس از بارگذاری، ردیف‌هایِ کهنه/خالی نگه می‌داشت.
  اکنون `ListView_SetItemCountEx` + `ListView_RedrawItems` + `UpdateWindow` صریح
  اجرا می‌شود تا جدولِ خدمات همیشه داده‌ها را نشان دهد. — `src/manage.inc`.

**۵) چاپ — تأییدِ متنی‌بودن (نه عکس) و مطابقتِ کامل با «طراحی چاپگر»**
- بازبینی نشان داد مسیرِ چاپِ طراحی‌شده (`printPrintDesign`) از قبل **متنی** است:
  آیتم‌هایِ متن با `DrawTextW` و فونتِ واقعیِ Vazirmatn (با auto-fit برایِ عدمِ
  برش) رسم می‌شوند، جدول‌ها با `pdDrawTable` (متن)، و فقط لوگو/عکس با
  `gpDrawImageRectFit` تصویری می‌شوند. هیچ رَستری‌سازیِ تمام‌صفحه‌ای در خروجیِ
  چاپگر وجود ندارد (BitBltهایِ موجود صرفاً double-bufferِ روی‌صفحه‌اند). خروجی
  دقیقاً همان چیدمانِ طراحی‌شده در «مدیریت ← طراحی چاپگر» است.

**فایل‌های تغییرکرده:** `src/web_admission_api.inc`, `src/services.cpp`,
`src/manage.inc`, `src/app.h` (نسخه ۱٫۴۲٫۰), `assets/admission/index.html`,
`assets/admission/admission.css`, `assets/admission/admission.js`,
`docs/CHANGELOG.md`.

---

## 1.41.0 — 2026-07-11

> **رفعِ ریشه‌ایِ خرابیِ کیبورد (Enter / Tab / Shift+Tab / Ctrl+A) در صفحهٔ
> «پذیرش بیمار»ِ امبد.** این ایراد یک باگِ **C++** بود، نه جاوااسکریپت: پیام‌هایِ
> `WM_KEYDOWN` مربوط به Tab / Enter / Ctrl+A / کلیدهایِ جهت هرگز به کنترلِ مرورگرِ
> میزبان‌شده مسیریابی نمی‌شدند، پس رویدادِ `keydown` در سندِ HTML اصلاً شلیک نمی‌شد.
> سه قطعهٔ گمشده تأمین شد و مشکل به‌طورِ کامل حل شد.

**ریشهٔ مشکل (فارسی):** کنترلِ WebBrowser (Trident/MSHTML) و — به‌شکلی ملایم‌تر —
WebView2 وقتی داخلِ یک HWNDِ بیگانه امبد می‌شوند، برایِ دریافتِ کلیدهایِ شتاب‌دهنده
(Tab/Enter/…) باید از مسیرِ `IOleInPlaceActiveObject::TranslateAccelerator` +
`IDocHostUIHandler::TranslateAccelerator` (برایِ MSHTML) و رویدادِ
`add_AcceleratorKeyPressed` (برایِ WebView2) عبور کنند. این سه مورد پیاده نشده بود؛
هیچ مقدار جاوااسکریپت نمی‌تواند پیام‌هایی را که اصلاً به کنترل تحویل نشده‌اند بسازد.

**Root cause (English):** The embedded WebBrowser (Trident/MSHTML) — and, more
mildly, WebView2 — do not receive accelerator keys (Tab/Enter/Ctrl+A/arrows) when
hosted inside a foreign parent HWND unless the container routes messages through
`IOleInPlaceActiveObject::TranslateAccelerator` **plus** an
`IDocHostUIHandler::TranslateAccelerator` that returns `S_FALSE` (MSHTML), and
subscribes to `add_AcceleratorKeyPressed` leaving `Handled=FALSE` (WebView2).
These three pieces were missing, so the page's `keydown` listeners never fired.
No amount of JavaScript can synthesize Win32 messages that were never delivered.

### Fixed — مسیریابیِ کلیدهایِ کیبورد به سطحِ HTMLِ امبد
- **MSHTML:** به `MshtmlHost` رابطِ `IDocHostUIHandler` اضافه شد؛
  `TranslateAccelerator` مقدارِ `S_FALSE` برمی‌گرداند تا Tab/Enter/Ctrl+A به سندِ
  صفحه برسند. `GetHostInfo` (بدونِ حاشیهٔ سه‌بعدی + تم)، `ShowContextMenu` (سرکوبِ
  منویِ پیش‌فرضِ IE)، و بقیهٔ متدها به‌صورتِ استاب. `IDocHostUIHandler` با
  `ICustomDoc::SetUIHandler` روی سند ثبت می‌شود؛ `IOleInPlaceActiveObject` گرفته و
  در `MshtmlView::ipao` نگه داشته و در Destroy آزاد می‌شود. کمک‌تابعِ جدید
  `MshtmlAdmission_TranslateAccel(MSG*)` فقط برایِ نمایی که HWNDِ میزبانش جدِ
  `msg->hwnd` است، `ipao->TranslateAccelerator` را صدا می‌زند.
  (`src/web_admission_mshtml.inc`)
- **WebView2:** رابط‌هایِ `ICoreWebView2AcceleratorKeyPressedEventArgs` و
  `...EventHandler` به‌صورتِ inline اعلام شد؛ پس از `get_CoreWebView2` با
  `add_AcceleratorKeyPressed` مشترک می‌شویم و برایِ Tab/Enter/جهت/Home/End/Esc و
  Ctrl+A مقدارِ `put_Handled(FALSE)` را می‌گذاریم تا WebView خودش پردازش کند.
  (`src/web_admission_webview2.inc`, `src/web_admission_host.inc`)
- **حلقهٔ پیامِ اصلی:** پیش از `TranslateMessage`/`DispatchMessage`، تابعِ
  عمومیِ جدیدِ `WebAdmission_TranslateAccel(&msg)` صدا زده می‌شود تا کنترلِ مرورگرِ
  امبد فرصتِ بلعیدنِ کلیدهایِ شتاب‌دهنده را داشته باشد.
  (`src/main.cpp`, `src/web_admission.h`, `src/web_admission_dispatch.inc`)
- **JS (کمربند و شلوارک):** `focusEl` علاوه بر `n.select()` از
  `setSelectionRange(0, len)` هم استفاده می‌کند (چون روی برخی بیلدهایِ MSHTML
  `select()` بی‌صدا شکست می‌خورد). `NAV_ORDER`/`lookupNid`/`fillPatient`/فراخوان‌هایِ
  Bridge **دست‌نخورده** ماند. (`assets/admission/admission.js`)
- **تستِ دود (Smoke):** تحتِ `AZ_DEBUG_BUILD` با `AZ_DEBUG_SCREEN=admission_keys`
  (`--smoke-admission-keys`) نمایِ پذیرش باز می‌شود، `#nid` با یک کدِ ملیِ موجود
  فوکوس/پر می‌شود، سپس با `keybd_event(VK_RETURN,…)` کلیدِ Enter شبیه‌سازی و پس از
  ۱۵۰۰ms با `WebAdmission_DebugInitHits()` و گتِرِ جدیدِ
  `WebAdmission_DebugLastFilledNid()` صحتِ پُرشدنِ فیلدها راستی‌آزمایی می‌شود.
  (`src/main.cpp`, `src/web_admission_dispatch.inc`, `src/web_admission_api.inc`)

### Fixed (English summary)
- MSHTML: `MshtmlHost` now implements `IDocHostUIHandler`; `TranslateAccelerator`
  returns `S_FALSE`, registered via `ICustomDoc::SetUIHandler`; the site's
  `IOleInPlaceActiveObject` is cached and driven by a new
  `MshtmlAdmission_TranslateAccel(MSG*)` helper (ancestor-scoped).
- WebView2: subscribes to `add_AcceleratorKeyPressed` and leaves Tab/Enter/
  arrows/Home/End/Esc/Ctrl+A unhandled so the WebView processes them internally.
- `main.cpp` message pump calls `WebAdmission_TranslateAccel(&msg)` before
  `TranslateMessage`/`DispatchMessage`.
- `admission.js`: `focusEl` adds a `setSelectionRange` fallback. NAV_ORDER,
  lookupNid, fillPatient and every Bridge call are untouched.
- New `--smoke-admission-keys` debug test proves Enter-on-#nid routes into the
  page → `patient.lookup` → C++ store.

**Build:** `./build.sh` سبز (PE32 استاتیکِ ۳۲‌بیتی، بدونِ اخطارِ جدید).

---

## 1.40.0 — 2026-07-08

> **رفعِ شش باگِ صفحهٔ پذیرشِ امبد (B1–B6) + پایه‌گذاریِ «پوستهٔ چندصفحه‌ایِ وبِ
> تعبیه‌شده».** این نسخه دو دستاورد دارد: (۱) رفعِ ریشه‌ایِ شش ایرادِ تعاملی/داده‌ایِ
> صفحهٔ پذیرش، و (۲) یک زیرساختِ چندصفحه‌ای تا صفحاتِ HTML/JS بعدی (نه فقط پذیرش) از
> همان میزبانِ لوپ‌بک، با دارایی‌ها و پلِ ارتباطیِ مشترک، سرو شوند. هیچ منطقِ نیتیوِ
> پذیرشِ GDI (`reception.cpp`) دست نخورد، هیچ وابستگیِ زمانِ‌اجرا اضافه نشد و همهٔ
> رشته‌های رابط فارسی و RTL/وزیرمتن دست‌نخورده ماند. تکْ‌اکسِ استاتیکِ PE32 (۳۲‌بیتی)
> از `./build.sh` با `-Werror` سبز است.

### Fixed — باگ‌های صفحهٔ پذیرش (B1–B6)
- **B1 — ناوبریِ Enter/Tab:** جایگزینیِ ناوبریِ مبتنی‌بر CSS-query با یک ترتیبِ
  صریحِ سخت‌کدشده (`NAV_ORDER`) به‌همراهِ `focusNext`/`focusPrev`، مدیریتِ
  MSHTML-امنِ `<select>` (keydown + keyup محافظت‌شده با پرچمِ `__navHandled`)،
  **بدونِ wrap-around** و انتخابِ همهٔ متن با Ctrl+A.
  (`assets/admission/admission.js`, `assets/shell/common.js` → `AzNav`)
- **B2 — استعلامِ کدِ ملی، آدرس/تلفنِ‌ثابت/بیمهٔ مکمل را پر نمی‌کرد:** توسعهٔ
  `PatientRow`/`CitizenInfo` و امضایِ `rememberPatient`، اسکیمایِ ۱۱‌ستونهٔ
  `patients.dat` (سازگارِ عقب‌رو با ۷/۸/۹/۱۰/۱۱ ستون + مهاجرتِ هنگامِ نوشتن)،
  به‌روزرسانیِ `patientJson`/`patient.lookup`/`admission.save`/`fillPatient` و
  مودال + ListViewِ `manage.inc`.
  (`src/app.h`, `src/data_ext.cpp`, `src/web_admission_api.inc`, `src/manage.inc`،
  و همهٔ فراخوان‌گاه‌ها: `appointment.cpp`/`backup.cpp`/`reception.cpp`)
- **B3 — `hasIns` بیمهٔ مکمل را خودکار انتخاب نمی‌کرد / بی‌ثبات بود:** فقط وقتی
  `insurances[0]>0` باشد `hasIns` **روشن** می‌شود و هرگز به‌صورتِ خودکار خاموش
  نمی‌شود. (`assets/admission/admission.js`)
- **B4 — کندیِ تعدادِ خدمت + مسابقهٔ Enter در جستجو:** افزودنِ
  `oninput`/`onkeyup` به فیلدِ تعداد، بازگشتِ Enter از تعداد به جستجو، و
  حذفِ تکرارِ درخواستِ در حالِ اجرا (`_svcSearchInFlight`).
  (`assets/admission/admission.js`)
- **B5 — طرحِ چاپ اعمال نمی‌شد:** `admission.save` اکنون `printMode` را برمی‌گرداند
  و JS هنگامِ افتادن به چاپِ کلاسیکِ GDI یک toastِ هشدار (`.toast.warn`) نشان
  می‌دهد. (`src/web_admission_api.inc`, `assets/admission/admission.js`,
  `assets/admission/admission.css`)
- **B6 — پس از «پذیرشِ جدید»، `hasIns` ناسازگار بود:** `clearForm` مقدارِ
  `insMain.selectedIndex=0` و `hasIns=false` را تنظیم می‌کند (تأییدشده با
  `billing.cpp`: ایندکسِ صفر = «آزاد» با درصدِ ۰). (`assets/admission/admission.js`)

### Added — پوستهٔ چندصفحه‌ایِ وبِ تعبیه‌شده
- **`assets/shell/`** — دارایی‌های مشترک: `common.css` (RTL فارسی، وزیرمتن،
  پوسته‌های روشن/تیره، میزبانِ toast)، `common.js` (رانتایمِ مشترک) و `vazir.ttf`.
- **رانتایمِ مشترک (ES5)** با پنج فضای‌نام روی `window`:
  - `AzBoot` — شناسهٔ صفحه (`<meta name="az-page">`)، پوسته و دروازهٔ `ready`
  - `AzBridge` — تنها لایهٔ IPCِ C++↔JS (WebView2 postMessage یا XHR `/api`)،
    سرآیندِ `X-Az-Page`، namespacing فعل، حذفِ تکرارِ درخواستِ همزمان، و
    uplink های `client.log`/`client.metrics`
  - `AzUi` — toast مشترک + کمک‌های DOM
  - `AzNav` — ناوبریِ Enter/Tabِ قابلِ‌استفاده‌مجدد روی یک ترتیبِ صریح (بدونِ wrap)
  - `AzPerf` — شمارنده‌های سبکِ کارایی که به `client.metrics` فرستاده می‌شوند
- **`src/web_pages.{h,cpp}`** — رجیستریِ صفحات: نگاشتِ «مسیرِ URL → RCDATA» و
  «فعلِ `/api` → هندلر»، به‌همراهِ فعل‌های داخلیِ `ping`/`client.log`/`client.metrics`.
- **`src/web_thread_pool.{h,cpp}`** — استخرِ نخِ کارگرِ کران‌دار (۲ روی سخت‌افزارِ
  کم‌توان / ۴ پیش‌فرض / سقفِ ۸) که اتصال‌های پذیرفته‌شده را سرو می‌کند، و
  `RunOnUiThread` برای مارشالِ یک callable به نخِ رابط (`WM_APP_UI_TASK`).
- **`src/web_ping_api.cpp` + `assets/pages/ping/`** — صفحهٔ نمونهٔ «آزمونِ اتصال»
  که کلِّ خطِّ لوله (رجیستری + استخرِ نخ + پل + فعلِ `/api/ping`) را سرتاسر ثابت
  می‌کند.
- **`src/web_admission_http.inc`** — دیسپچِ عمومی: ابتدا سوییچِ قدیمیِ پذیرش
  (400..405)، سپس رجیستری (پوسته 500..502 و ping 600..602)؛ فعل‌ها ابتدا از
  رجیستری و در صورتِ نبود از `admissionApi`. سخت‌سازیِ شبکه: **فقط لوپ‌بک**
  (رد کردنِ هر همتای غیرِ 127.0.0.0/8)، `SO_REUSEADDR`، و timeoutهای
  ارسال/دریافت (۸ ثانیه).

### Changed — نسخه و منابع
- ارتقای نسخه به **1.40.0** در `src/app.h`، `src/app.rc`
  (`FILEVERSION`/`PRODUCTVERSION 1,40,0,0` + رشته‌ها) و `update/version.txt`.
- **`src/app.rc`**: افزودنِ RCDATA 500/501/502 (پوسته) و 600/601/602 (ping) —
  بلوکِ 400..405ِ پذیرش بازشماری **نشد**.
- **`build.sh`**: افزودنِ `web_pages.cpp`، `web_thread_pool.cpp` و
  `web_ping_api.cpp` به فهرستِ منابع.
- **`assets/admission/index.html`**: افزودنِ `<meta name="az-page">` و
  `common.css`/`common.js` (پیش از ورقهٔ پذیرش و `bridge.js`).

---

## 1.39.0 — 2026-07-08

> **همگام‌سازیِ زندهٔ خدمات از «مدیریت» + پولیشِ تعاملی/تصویریِ صفحهٔ «پذیرش
> بیمار».** تمرکز این نسخه رفعِ تنها شکافِ کارکردیِ باقی‌ماندهٔ ماژولِ پذیرشِ
> امبد (HTML/CSS/JS داخلِ برنامهٔ C++) است: تغییرِ خدمات در پنلِ مدیریت اکنون
> بلافاصله و بدونِ رفرش در صفحهٔ پذیرش منعکس می‌شود؛ به‌همراه چند اصلاحِ ظریفِ
> تعامل و ظاهر که با یک خطِّ لولهٔ راستی‌آزماییِ مرورگری (Playwright) تأیید شد.
> هیچ منطق/تم/هدرِ نیتیو/ماژولِ نامرتبطی حذف یا تغییرِ رفتاری نکرد.

### Added — همگام‌سازیِ زندهٔ کاتالوگِ خدمات (C++ → JS، بدونِ رفرش)
- **`src/web_admission.h` / `src/web_admission_dispatch.inc`**: تابعِ عمومیِ
  جدید `WebAdmission_NotifyCatalogChanged()`. هر بار که مدیریت خدمتی را
  **اضافه/ویرایش/حذف** می‌کند، این تابع یک اسنپ‌شاتِ تازه از کاتالوگِ فعالِ خدمات
  (`catalog.update`) و جدول‌های بیمه (`insurance.update`) را به **همهٔ** ویوهای
  بازِ پذیرش پوش می‌کند (هم WebView2 مستقیم، هم صفِ `/api/poll` برای MSHTML).
  اگر هیچ ویویی باز نباشد no-op است.
- **`src/admin.cpp`**: افزودنِ `#include "web_admission.h"`.
- **`src/manage.inc`**: پس از `mgServicesSave` (افزودن/ویرایش) و
  `mgServicesDelete` فراخوانیِ `WebAdmission_NotifyCatalogChanged()`.
- **`assets/admission/admission.js`**: هندلرِ رویدادِ `catalog.update`؛
  (۱) کاتالوگِ تازه را نگه می‌دارد، (۲) قیمتِ خدماتِ **ازپیش‌افزوده** به پذیرشِ
  جاری را با قیمتِ جدیدِ مدیریت به‌روزرسانی می‌کند و صورت‌حساب را دوباره محاسبه
  می‌کند، (۳) اگر فهرستِ پیشنهادِ خدمت باز باشد، جستجوی جاری را زنده تکرار می‌کند.

### Fixed — تعاملِ افزودنِ خدمت (ضدِّ دادهٔ کهنه)
- **`assets/admission/admission.js`**: Enter در جعبهٔ جستجوی خدمت و دکمهٔ «افزودن
  خدمت» دیگر به `state.catalog[0]`ِ کش‌شده (که ممکن بود به‌خاطرِ debounce کهنه
  باشد) تکیه نمی‌کنند؛ همیشه یک `service.search`ِ تازه از C++ می‌گیرند و اولین
  نتیجه را اضافه می‌کنند — پس افزودن همیشه کاتالوگِ زندهٔ مدیریت را بازتاب می‌دهد.
  دکمهٔ «افزودن خدمت» اگر عبارت دقیقاً به یک خدمت برسد آن را مستقیم اضافه می‌کند،
  وگرنه فهرستِ پیشنهاد را نشان می‌دهد.

### Changed — پولیشِ ظاهری (تمِ سفیدِ پزشکی حفظ شد)
- **`assets/admission/admission.css`**:
  - دکمهٔ «افزودن خدمت» (`#svcAddBtn`): حداقلِ‌عرضِ ۱۱۸px، فونتِ کمی کوچک‌تر و
    `flex:0 0 auto` تا **کلِّ متن همیشه دیده شود** (تأیید شد: بدونِ برش).
  - بلوک‌های خدمات/صف (`.svc-card`/`.queue-card`) حاشیهٔ کمی قوی‌تر و نوارِ آبیِ
    بالای هدر گرفتند تا از ستونِ مرکزی متمایز شوند.
  - قابِ جدولِ خدمات/صف (`.tbl-wrap`) حاشیهٔ واضح‌تر گرفت.
  - اسکرول‌بارها: افزودنِ propهای اختصاصیِ MSHTML/Trident
    (`scrollbar-*-color`) تا در آن موتور هم اسکرول‌بارِ ملایم دیده شود
    (کرومیوم از قبل `::-webkit-scrollbar` باریکِ ۶px داشت).

### Verification
- بیلدِ موفقِ `./build.sh` → `build/AzadiTeb.exe` (PE32 i386، static).
- راستی‌آزماییِ مرورگری (Playwright) با یک پلِ ماکِ C++: بدونِ خطای JS؛ پنهان‌شدنِ
  لودر، پرشدنِ بیمه‌ها، صفرـبودنِ صورت‌حساب هنگامِ باز شدن، اتوفیلِ کدِملی با Enter،
  افزودنِ خدمت با Enter، به‌روزرسانیِ زندهٔ قیمت پس از `catalog.update`، و
  **بدونِ سرریزِ عمودی** در ۱۳۶۶×۷۶۸ همگی تأیید شدند؛ و بررسیِ استایل:
  «مانده قابل پرداخت» بدونِ پس‌زمینهٔ آبی، فقط نشانِ P/S رنگی، تمِ سفید حفظ‌شده.

### نسخه
- `src/app.h` (`APP_VERSION_W` → `L"1.39.0"`)، `src/app.rc`
  (FILE/PRODUCTVERSION → 1.39.0)، `update/version.txt` → `1.39.0`.

---

## 1.32.0 — 2026-07-06

> **بازطراحیِ واقعیِ چیدمانِ صفحهٔ «پذیرش بیمار» (نه اصلاحِ ظاهریِ سطحی).** در
> نسخهٔ ۱.۳۱ تغییرات آن‌قدر ظریف/تدریجی بودند که کاربر «هیچ تغییری» ندید. در این
> نسخه تغییراتِ ساختاریِ **قابلِ‌مشاهده و قابلِ‌راستی‌آزمایی** انجام شد و برای
> نخستین‌بار یک خطِّ لولهٔ راستی‌آزماییِ تصویری (wine 32-bit + Xvfb + اسکرین‌شات)
> برقرار شد تا نتیجه واقعاً روی صفحه دیده و تأیید شود. هیچ منطق/دیتابیس/رویداد/
> ناوبری حذف نشد.

### Removed / Fixed (`src/reception.cpp`)
- **حذفِ بلوکِ تکراریِ پزشک از پنلِ راست**: گروهِ «جستجوی پزشک معالج» از کارتِ
  پروفایلِ راست کاملاً حذف شد (`computeInfoLayout` خطوطِ پایهٔ گروهِ سوم را پارک
  می‌کند، `tabPageLayout` کنترل‌های `eDocCode`/`bDocSearch`/`eDocName` را مخفی و
  به صفر می‌برد، و `paintInfoPanel` رسمِ گروهِ سوم را رها می‌کند). بخشِ «پزشک
  معالج» فقط در یک محلِّ مرکزی می‌ماند.
- **حذفِ پس‌زمینهٔ آبیِ ردیفِ «مانده قابل پرداخت»**: به یک ردیفِ خلاصهٔ معمولی
  تبدیل شد (در اسکرین‌شات تأیید شد).
- **رفعِ تداخلِ بلوکِ P/S**: بلوکِ P/S به‌صورتِ چیپ‌های `psCard` با حاشیه بازنویسی
  شد (سطحِ کناره‌دار + مربعِ نشان روی لبهٔ بیرونی + برچسب/مقدارِ روی‌هم‌چیده) که
  دیگر روی هم نمی‌افتند.

### Changed (`src/reception.cpp`)
- **کارتِ پروفایلِ راست جادار شد**: متریک‌های سرآیند بازطراحی شدند (آواتار `SF(28)`،
  چیپ `SF(28)`، جعبه‌ها `SF(44)`، P/S `SF(48)`؛ فاصلهٔ عنوان `+SF(42)`). عنوانِ
  «بیمار جدید» دیگر به ناحیهٔ نسخهٔ الکترونیک/حاشیه‌ها برخورد نمی‌کند.
- **چیپِ «نسخه الکترونیک»** به یک قرصِ تقریباً تمام‌عرض بازنویسی شد
  (`pad=L.iw/8`, شعاع `L.chipH/2`).
- **فاصله‌گذاریِ گروه‌ها**: `titleH=SF(30)`, `grpGap=SF(18)`.

### Added (`src/reception.cpp`)
- **`drawCollapseCaret()`**: هلپرِ مثلثِ GDI (`Polygon`) برای کلیدهای جمع‌شونده.
  گلیف‌های ▾/▸ در فونتِ Vazirmatn به‌صورتِ tofu نمایش داده می‌شدند؛ حالا هر دو
  کلیدِ جمع‌شوندهٔ لیستِ خدمات و صندوق با مثلثِ GDI رسم می‌شوند و همیشه دیده
  می‌شوند.

### Build / Verification
- نسخه به **1.32.0** ارتقا یافت (`src/app.h`, `src/app.rc`).
- خروجیِ نهاییِ production با `-Werror` تمیز build شد (`build/AzadiTeb.exe`، ۳٫۴ مگابایت).
- راستی‌آزماییِ تصویری با wine 32-bit + Xvfb در ۱۳۶۶×۷۶۸ و ۱۶۰۰×۹۰۰ تأیید کرد:
  کارتِ پروفایل تمیز، P/S بدون تداخل، پزشکِ تکراری حذف، کلیدهای جمع‌شونده دیده
  می‌شوند، بدونِ سرریزِ پایین، و باز/جمع‌شدن کار می‌کند.

---

## 1.31.0 — 2026-07-06

> **رفعِ ریشه‌ایِ تداخلِ برچسب‌ها با کنترل‌ها در صفحهٔ «پذیرش بیمار» + بازطراحیِ
> کارتِ پروفایلِ راست مطابقِ عکسِ مرجع.** علتِ اصلیِ باگ‌های تکراریِ نسخه‌های ۱.۲۷
> تا ۱.۳۰ این بود که فونتِ برچسب/عنوان با پیکسلِ **ثابت** (`S(13)`/`S(16)`) رسم
> می‌شد، ولی وقتی صفحه با ضریبِ تناسب (fit-factor) کوچک می‌شد، **باندِ برچسب** هم
> کوچک می‌شد و متنِ برچسب زیرِ تکست‌باکس/کمبو پنهان یا نصفه می‌شد. حالا فونتِ
> برچسب/عنوان با همان ضریب مقیاس می‌شود و باندِ برچسب همیشه ≥ ارتفاعِ فونت + فاصله
> تضمین می‌شود، پس هیچ برچسبی هرگز بریده یا پنهان نمی‌شود. هیچ منطق/دیتابیس/رویداد
> حذف نشد.

### Added
- **فونت‌های مقیاس‌پذیرِ تناسبی (`fitFont`)** در `src/main.cpp` + اعلان در
  `src/app.h`: یک کشِ کوچک از `CreateFontW(-S(px)*f)` که با ضریبِ تناسبِ چیدمان
  کوچک می‌شود تا برچسب همیشه داخلِ باندِ خودش جا شود.
- **جمع‌شونده کردن لیستِ خدمات و لیستِ صندوق/صف** (`src/reception.cpp`): دو کلیدِ
  کوچکِ چِوران (▾/▸) در سرِ هر لیست؛ در حالتِ جمع‌شده فقط چند ردیف نشان داده
  می‌شود تا کلِ صفحه در یک قاب بماند و اسکرول لازم نشود؛ با باز کردن، بدنهٔ لیست
  رشد می‌کند. حالتِ جدید در `TabPage::svcCollapsed` / `upCollapsed` نگه‌داری و در
  `WM_LBUTTONDOWN` با hit-test کلیک می‌شود.

### Fixed / Changed (`src/reception.cpp`)
- **`computeCenterV`**: ضریبِ تناسب در `v.fitF` ذخیره می‌شود و باندِ برچسب
  (`v.lbl`) تضمین می‌شود که ≥ فونتِ برچسب + ۶px فاصله باشد؛ در غیرِ این‌صورت
  ردیف‌ها به‌اندازهٔ دلتا به پایین می‌روند (دو جدولِ پایینی این فضا را جذب می‌کنند
  تا چیزی از پایین بیرون نزند).
- **رسّامِ فرم (`WM_PAINT`)**: هلپرهای `subcard`/`card3`/`fieldLabel` حالا با
  `fitFont(...,v.fitF)` رسم می‌شوند و باندِ سرآیندِ کارت‌ها هم با ضریب مقیاس
  می‌شود؛ عنوان/آیکون هرگز روی خط جداکننده یا فیلد نمی‌افتد.
- **کارتِ پروفایلِ راست** (`computeInfoLayout` + `paintInfoPanel`): ناهماهنگیِ
  آفستِ ثابتِ رسّام با چیدمانِ مقیاس‌شده رفع شد. عنوان «بیمار جدید» با فاصلهٔ
  بیشتر از بالای کارت (دیگر به لبه نمی‌چسبد)، چیپِ «نسخه الکترونیک»، دو جعبهٔ
  شمارنده و بلوکِ **تک**ِ P/S همه با `L.fitF` مقیاس می‌شوند و هرگز روی هم نمی‌افتند.
- **حذفِ برچسبِ تکراریِ «انجام دهنده»** از پایینِ پنلِ راست (مورد ۲۷ اسکریپت) —
  چون بلوکِ «انجام دهنده» در ستونِ میانی وجود دارد.
- **دکمهٔ «افزودن خدمت»** پهن‌تر شد (`S(132)`) تا آیکون + متن کامل دیده شود
  (مورد ۱۲).
- **ردیفِ «مانده قابل پرداخت»** دیگر پس‌زمینهٔ آبی ندارد (مورد ۱۱): یک ردیفِ
  معمولی روی سفید با جداکنندهٔ خاکستریِ ملایم و متنِ تیرهٔ خوانا؛ آبیِ پررنگ فقط
  برای کارتِ «جمع مبلغ نهایی» می‌ماند.
- **عنوان پزشکِ راست** به «جستجوی پزشک معالج» و برچسب‌ها به «شماره نظام پزشکی»
  اصلاح شد تا با عکسِ مرجع یکی شود.

### Files
- `src/app.h`, `src/main.cpp`, `src/reception.cpp`, `src/app.rc`

---

## 1.30.0 — 2026-07-06

> **رفعِ اساسیِ خرابی‌های ظاهریِ صفحه «پذیرش بیمار» و ریسپانسیوسازیِ واقعی طبق
> اسکریپتِ بازطراحی: برچسب‌ها دیگر زیر کنترل‌ها پنهان نمی‌شوند، متنِ دکمه‌ها کامل
> دیده می‌شود، هیچ آیتمی از پایینِ کادر خارج نمی‌شود، و صفحه در هر رزولوشن در یک
> قاب جا می‌شود (۱۳۶۶×۷۶۸، ۱۶۰۰×۹۰۰، ۱۹۲۰×۱۰۸۰). هیچ منطق/دیتابیس/رویداد/میان‌بر
> حذف نشد.**

### Fixed / Changed (`src/reception.cpp`)
- **پنلِ راستِ اطلاعات بیمار دیگر از پایینِ کادر بیرون نمی‌زند و صفحه اسکرول
  نمی‌شود:** تابع `computeInfoLayout` بازنویسی شد تا ابتدا ارتفاعِ طبیعیِ خودش را
  اندازه بگیرد و اگر از ارتفاعِ در دسترسِ پنل بیشتر بود، ارتفاعِ ردیف‌ها و
  فاصله‌ها را با **یک ضریبِ تناسبِ واحد (fit-factor)** کوچک کند تا آخرین کنترل
  («پزشک معالج» → نام پزشک) همیشه بالای لبهٔ پایینیِ پنل بنشیند. این ریشهٔ اصلیِ
  «کلیپ‌شدن و اسکرولِ اجباریِ صفحه» بود.
- **بودجهٔ عمودیِ ستونِ میانی هم ریسپانسیو و تطبیقی شد:** `computeCenterV` اکنون
  ارتفاعِ طبیعیِ کلِ فرم (کارت مشخصات + سه کارت + دو جدول + نوار دکمه‌ها) را
  محاسبه می‌کند و اگر از ارتفاعِ کلاینت بیشتر شد، همه‌چیز را با یک fit-factor
  فشرده می‌کند. در حالتِ عادی ارتفاع‌ها **سخاوتمندانه و خوانا** هستند
  (کنترل `rh=40`، نوار برچسب `lbl=22`، فاصلهٔ ردیف `12`) تا برچسب‌ها روی کنترل
  نیفتند و متنِ دکمه‌ها کامل دیده شود.
- **برچسب↔کنترل فاصلهٔ کافی گرفت** (نوار برچسب ۲۲px، فاصلهٔ ۶–۸px) تا هیچ
  برچسبی زیرِ فیلد پنهان نشود.
- دو جدولِ پایین همچنان تمامِ فضای باقی‌مانده را پر می‌کنند و هرگز از پایین بیرون
  نمی‌زنند؛ در صورتِ نیاز فقط بدنهٔ جدول اسکرول می‌شود، نه کلِ صفحه.
- تک‌بلوکِ P/S، تک‌فیلدِ درصدِ بیمهٔ تکمیلی، و نبودِ قیمت‌گذاریِ دستیِ خدمت در
  پذیرش (از نسخه‌های قبل) حفظ شد؛ تبِ «مدیریت خدمات / افزودن خدمت» فعال است.

### Version
- ارتقاء نسخه به **1.30.0** (`src/app.h`, `src/app.rc`, `update/version.txt`).

---

## 1.29.0 — 2026-07-06

> **ریسپانسیوسازی و جمع‌وجورسازیِ کاملِ فرم «پذیرش بیمار» تا کل صفحه در یک قاب
> جا شود — بدون اسکرول، بدون تداخل، و بدون خروجِ آیتم‌ها از پایینِ کادر. طبق
> عکس مرجع تأییدشده. هیچ منطق/دیتابیس/رویداد/میان‌بر/کلاسی حذف نشد.**

### Fixed / Changed (`src/reception.cpp`)
- **جا شدنِ کاملِ صفحه در یک قاب (بدون اسکرول عمودیِ صفحه):** بازطراحیِ بودجهٔ
  عمودی در `computeCenterV` تا مجموعِ ارتفاع (کارت مشخصات + سه کارت میانی + دو
  جدولِ پایین + نوار دکمه‌ها) دقیقاً در ارتفاعِ کلاینتِ تبِ پذیرش جا شود.
  ارتفاع‌ها فشرده شدند: کنترل `rh=34`، نوار برچسب `lbl=18`، فاصلهٔ ردیف `rgap=8`،
  هدرِ کارت‌ها `32`.
- **حذف ردیفِ سومِ کارت «پزشک معالج»** (فیلدِ تکراریِ «درصد بیمه مکمل» /
  `eSuppPct2`): کارت‌های میانی حالا فقط ۲ ردیف دارند (مطابق مرجع). تنها فیلدِ
  درصدِ بیمهٔ تکمیلیِ باقی‌مانده `eSuppPctIns` در کارت «بیمه و نوبت» است.
- **حذفِ نوارِ جداگانهٔ «تاریخ/شیفت نوبت»:** این دو کنترل به ردیفِ دومِ کارتِ
  «بیمه و نوبت» منتقل شدند (ستون‌های ۱ و ۲، کنارِ «درصد بیمه تکمیل»)، که یک
  نوارِ ~۹۰ پیکسلی را حذف می‌کند و فضای لازم برای جدول‌های پایین را آزاد می‌کند.
- **مخفی‌سازیِ کاملِ اسکرول‌بار وقتی محتوا جا می‌شود** (`recUpdateScrollbar` با
  `ShowScrollBar`) تا صفحه به‌صورتِ یک قابِ ثابتِ واحد دیده شود؛ اسکرول فقط روی
  نمایشگرهایِ بسیار کوچک ظاهر می‌شود.
- **بررسیِ ریسپانسیو** روی ۱۳۶۶×۷۶۸، ۱۶۰۰×۹۰۰ و ۱۹۲۰×۱۰۸۰: بدون تداخل، بدون
  کلیپ، بدون خروج از کادر، و بدون اسکرولِ صفحه در هر سه رزولوشن.
- بالا بردنِ نسخه به ۱.۲۹.۰ (`src/app.h`, `src/app.rc`).

---

## 1.28.0 — 2026-07-05

> **بازطراحی فرم «پذیرش بیمار» طبق اسکریپت طراحی + راهنمای فنی، و افزودن ماژول
> «مدیریت خدمات» به پنل مدیریت. اپراتور پذیرش دیگر هرگز قیمت خدمت را دستی وارد
> نمی‌کند؛ قیمت‌ها منبع واحد دارند (پایگاه‌دادهٔ خدمات). هیچ منطق/دیتابیس/کلاس/
> رویداد/میان‌بر صفحه‌کلید/بایندینگِ موجودی حذف نشد.**

### Added
- **ماژول «مدیریت خدمات» (`src/services.cpp` [جدید], `src/app.h`, `src/manage.inc`):**
  - ساختار `ServiceDef` و API دادهٔ خدمات: `loadServices` / `addService` /
    `updateService` / `removeService` / `findService`.
  - انبارهٔ فایلی `data/services.dat` (pipe-delimited، UTF-8) با ستون‌های
    `code|name|category|dept|price|insType|desc|status|created|modified`.
  - صفحهٔ جدید `PG_SERVICES` در داشبورد مدیریت با فرم کامل (کد خدمت، نام،
    دسته، بخش، مبلغ پایه، نوع بیمه، توضیحات، وضعیت)، دکمه‌های
    ذخیره/ویرایش/حذف/پاک‌سازی، جست‌وجو و گریدِ فهرست خدمات.
- **ویجت‌های P/S** در یک محلِ واحد (نوار کناری راست، زیر «نسخهٔ الکترونیک»):
  P زرد ۴۰×۴۰ و S سبز ۴۰×۴۰؛ تمام نمونه‌های تکراری حذف شد.
- **فیلد «درصد بیمه تکمیل»** (تک‌فیلد) در کارت بیمه (`eSuppPctIns`).

### Changed (`src/reception.cpp`)
- **حذف تمام فیلدهای قیمتِ دستی** از پذیرش (مبلغ خدمت، تخفیف، سهم بیمه‌ها) —
  کنترل‌های `ePrice`/`eDiscount` مخفی نگه داشته شدند (برای `recalc`) اما از UI
  حذف شدند؛ اپراتور قیمت وارد نمی‌کند.
- **نوار نوبت** ساده شد به «تاریخ نوبت + شیفت»؛ پیش‌نمایش نوبت و مربع‌های
  تکراری P/S حذف شدند.
- **پیکربندی خدمت در پذیرش از پایگاه‌داده تغذیه می‌شود:** تابع `svcResolve`
  ابتدا `findService`/`loadServices` را می‌خواند (خدماتِ ساخته‌شده در «مدیریت
  خدمات» به‌صورت خودکار در پذیرش در دسترس‌اند و قیمت از دیتابیس می‌آید) و در
  نبودِ رکورد به کاتالوگِ درون‌ساخت برمی‌گردد.

### Build (`build.sh`, `shot.sh`, `build_incremental.sh`)
- `src/services.cpp` به فهرست منابع اضافه شد.

---

## 1.27.1 — 2026-07-05

> **اصلاح دقیق چیدمان (pixel-correction) فرم «پذیرش بیمار» طبق اسکریپت
> «UI LAYOUT CORRECTION INSTRUCTIONS» و تصویر مرجع تأییدشده. هیچ قابلیت،
> فیلد یا بخشی حذف/اضافه/جابه‌جا نشد؛ فقط اندازه‌ها و فاصله‌ها مطابق مرجع
> بازسازی شدند.**

### Changed (`src/reception.cpp`)
- **مارجین‌ها و گریدِ سراسری** مطابق اسکریپت: مارجین بیرونی ۱۶px، فاصلهٔ بین
  کارت‌ها ۱۴px، پدینگ داخلی کارت ۱۶px، فاصلهٔ سطرها/ستون‌ها ۱۲px.
- **ارتفاع کنترل‌ها ۳۸px** (تکست‌باکس == کمبوباکس)، رادیوس ۶px، لیبل بالای
  کنترل با فاصلهٔ ۶px.
- **فشرده‌سازیِ نیمهٔ بالا اصلاح شد:** باندهای هدر کارت‌ها و باند لیبل کم شدند
  (`hdr1=40`, `hdr3=38`, `lbl=22`) تا سه‌کارتِ میانی و نوارِ نوبت جمع‌وجور شوند
  و جدول‌ها («خدمات» و «صندوق نرفته‌ها/صف پذیرش») در همان صفحه دیده شوند.
- **کارت «بیمه و نوبت»**: ردیف دوم به ۴ فیلدِ هم‌تراز تبدیل شد
  (مبلغ خدمت | تخفیف | سهم بیمه ٪ | سهم بیمه تکمیلی ٪) — بدون هم‌پوشانی.
- **رفع هم‌پوشانی‌ها:** لیبل «نام پزشک» کوتاه شد و موقعیت مبلغ خدمت/تخفیف در
  `tabPageLayout` اصلاح شد تا کنترل‌ها و لیبل‌ها و بوردرها روی هم نیفتند.
- **کارت پزشک و انجام‌دهنده** هم‌ارتفاع و هم‌ترازِ بالا/پایین شدند.
- ارتفاع دکمه‌های اکشن ۴۶px، تولبار جدول ۴۴px، هدر جدول ۴۰px، سطر ۳۶px.

---

## 1.27.0 — 2026-07-05

> **بازطراحی ظاهری فرم «پذیرش بیمار» بر اساس پرامپت و تصویر مرجع تأییدشده —
> رفع کامل مشکلاتِ خوانا نبودن لیبل‌ها، فاصله‌های نااستاندارد و سلسله‌مراتب
> بصری ضعیف. هیچ قابلیت، فیلد یا بخشی حذف یا جابه‌جا نشد؛ فقط چیدمان،
> فونت‌ها و رنگ‌ها برای رسیدن به کیفیت یک HIS تجاری اصلاح شد.**

### Added
- **دو فونت اختصاصی UI (`src/main.cpp`, `src/app.h`):**
  - `g_fLabel` = ۱۳px نیمه‌ضخیم (Medium) برای همهٔ لیبل‌های فیلد — خوانا و
    نه ریز.
  - `g_fSection` = ۱۶px ضخیم برای عناوین بخش‌ها (اطلاعات بیمار، بیمه و نوبت،
    پزشک معالج، انجام دهنده، خدمات، صورت حساب).
- **دو رنگ اختصاصی در تم (`src/theme.cpp`, `src/app.h`):**
  - `labelInk` = `#374151` (تم روشن) برای لیبل‌ها — دیگر خاکستریِ محو یا آبی
    نیست و در پس‌زمینه گم نمی‌شود.
  - `sectionInk` = `#1F2937` (تم روشن) برای عناوین بخش‌ها.

### Changed (فقط ظاهر — `src/reception.cpp`)
- **ارتفاع کنترل‌ها** از ۲۸px به ۳۴px، **باند لیبل** بالای هر کنترل از ۱۵px
  به ۲۰px (فونت ۱۳ + ~۴px فاصله)، و **فاصلهٔ ردیف‌ها** از ۶px به ۱۲px —
  لیبل‌ها دیگر به تکست‌باکس نمی‌چسبند و روی هم نمی‌افتند.
- **عناوین بخش‌ها** با فونت ۱۶ ضخیم + یک خط جداکنندهٔ نازک زیر عنوان
  (padding پایین) رسم می‌شوند تا واضح از فیلدها جدا باشند.
- **هدر کارت‌ها** بلندتر شد (کارت اطلاعات بیمار و سه کارت میانی) و آیکن‌ها
  ۱۸~۲۰px هم‌تراز با عنوان.
- **جدول‌ها:** ارتفاع هدر ۲۸px با فونت لیبل، ارتفاع ردیف‌ها از ۲۶px به ۳۲px
  (خدمات + صندوق نرفته‌ها/صف پذیرش).
- **پنل صورت‌حساب (چپ):** عنوان «صورت حساب» و «چاپ» با فونت بخش، عنوان‌های
  گروه ضخیم، و لیبل ردیف‌های مبلغ با `labelInk`.
- **پنل کناری راست:** لیبل‌های فیلد با `g_fLabel`/`labelInk`، افزایش
  فاصلهٔ لیبل↔کنترل (`lblGap` ۴px، `rowGap` ۱۰px، `grpGap` ۱۴px) و ارتفاع
  کنترل ۳۲px.

---

## 1.26.0 — 2026-07-02

> **تکمیل بازطراحی «پذیرش بیمار» بر اساس تصویر مرجع — پنل پایین-چپ
> «صندوق نرفته‌ها / صف پذیرش» با تب مسطح، نوار ابزار (تاریخ‌تا + پنجرهٔ
> ساعات اخیر + رفرش)، جدول ۶ ستونه با ستون «دقیقه پیش»، لینک آبی
> «+ افزودن به …»، حذف ردیف با تأیید، و صورت‌حساب گروه‌بندی‌شده
> (بیمه اصلی / بیمه مکمل / مبلغ نهایی + ردیف هایلایت «مانده قابل پرداخت»).**

### Added
- **پنل «صندوق نرفته‌ها / صف پذیرش» (پایین-چپ):** دو تب مسطح دسکتاپی؛ کلیک
  روی هر تب فهرست همان فایل داده را بارگذاری می‌کند. (`s_upTabR`,
  `upLoad`, `upMarkDirty` در `src/reception.cpp`)
- **نوار ابزار پنل:** تکست‌باکس «تاریخ تا» (ماسک تاریخ جلالی)، کمبوی
  «مراجعات اخیر (ساعت)» با گزینه‌های ۶/۱۲/۲۴، و دکمهٔ رفرش ⟳ که فهرست را
  از دیسک دوباره می‌خواند. (`ID_F_UP_DATE`, `ID_F_UP_HOURS`, `ID_F_UP_REFRESH`)
- **جدول ۶ ستونه:** بارکد/کد پرونده، نام بیمار، تاریخ، زمان، «دقیقه پیش»
  (محاسبهٔ زنده از epoch ذخیره‌شده)، و ستون عملیات با ۴ آیکن
  (تکرار/رسید/ویرایش/حذف). حذف ردیف با پیام تأیید و حذف همان خط از فایل
  داده انجام می‌شود. (`UpRow`, `UpHit`, `upDeleteRaw`)
- **لینک آبی «+ افزودن به صندوق نرفته‌ها / صف پذیرش»** در پاصفحهٔ جدول —
  hit-test شده در `WM_LBUTTONDOWN` و به همان اکشن دکمه‌های
  `ID_F_ADD_UNPAID` / `ID_F_ADD_QUEUE` وصل است.
- **فرمت جدید فایل‌های داده:** `q|tag|name|paid|date|time|epoch` — سه فیلد
  تازه، ستون‌های تاریخ/زمان/دقیقه‌پیش را تغذیه می‌کنند؛ پارسر نسبت به
  خطوط قدیمی ۴فیلدی تحمل‌پذیر است. (`upParse`, `upNowHHMM`)
- **فیلترها:** جستجو بر اساس نام/کد + پنجرهٔ ساعات اخیر + «تاریخ تا»؛
  ردیف‌های قدیمی بدون epoch/تاریخ همیشه نمایش داده می‌شوند تا چیزی گم نشود.

### Changed
- **صورت‌حساب (ستون چپ):** گروه‌بندی دقیق مطابق تصویر مرجع — «بیمه اصلی»
  (جمع کل/سهم بیمار/سهم سازمان)، «بیمه مکمل» (جمع کل/مابه‌التفاوت پایه/
  سهم سازمان)، «مبلغ نهایی» (جمع کل/تخفیف/پرداختی) + ردیف هایلایت آبی
  «مانده قابل پرداخت» و کارت گرادیان آبی «جمع مبلغ نهایی».
- هندلر `WM_APP_THEME` تب پذیرش حالا دکمه‌های جدید (افزودن خدمت،
  تأیید خدمت، صندوق نرفته‌ها، صف پذیرش، رفرش) را هم به‌روزرسانی می‌کند.
- `shot.sh` با فهرست سورس/کتابخانه‌های `build.sh` همگام شد
  (`web_designer.cpp`, `-lws2_32`, `-lpsapi`, `-loleaut32`).

### Fixed
- افزودن `#include <ctime>` به `reception.cpp` (خطای کامپایل `time`).

### Files
- `src/reception.cpp` — تکمیل پنل صندوق نرفته‌ها/صف پذیرش + صورت‌حساب.
- `src/app.h` — ارتقای نسخه به 1.26.0.
- `docs/CHANGELOG.md`, `shot.sh`

---

## 1.25.0 — 2026-07-01

> **بازطراحی کامل بخش «پذیرش بیمار» مطابق اسکریپت طراحی و تصویر مرجع —
> چیدمان سه‌ستونی پرتراکم، کارت‌های پزشک معالج/انجام‌دهنده/بیمه/نوبت،
> جدول خدمات با پنل «افزودن خدمت» درون‌خطی، پیش‌نمایش P/S، و دو مسیر
> پرداخت‌مؤخر (صندوق نرفته‌ها + صف پذیرش).**

### Added
- **کارت «پزشک معالج» و «انجام‌دهنده» (وسط فرم):** هر کدام یک تکست‌باکس کد +
  یک لیست‌باکس (کمبو). با تایپ کد و زدن Enter، نام از فهرست انتخاب می‌شود؛ یا
  می‌توان مستقیم از لیست انتخاب کرد. کد و لیست دوطرفه همگام می‌شوند.
  (`src/reception.cpp`: `applyDocByCode`, `mirrorDocCodeFromList`, `docCodeProc`)
- **پنل «افزودن خدمت» درون‌خطی (بدون باز شدن پنجرهٔ جدید):** با کلیک روی
  «افزودن خدمت» یک نوار درون‌خطی باز می‌شود شامل: تکست‌باکس کوچک کد (مثلاً ۱۱۱ +
  Enter → «تزریقات»)، تکست‌باکس بلندتر نام خدمت، انتخاب‌گر تعداد، تیک «نرخ آزاد»
  که یک تکست‌باکس مبلغ را آشکار می‌کند، و دکمهٔ «افزودن». هر خدمت به انتهای
  فهرست خدمات اضافه می‌شود. (`SvcRow`, `svcCommitPanel`, `svcCodeProc`, `svcNameProc`)
- **کاتالوگ خدمات داخلی:** نگاشت کد→نام/قیمت (۱۱۱ تزریقات، ۱۱۲ سرم‌تراپی، …)
  تا تایپ کد فوراً نام و نرخ پیش‌فرض را پر کند. (`SVC_CATALOGUE`, `svcCatFind`)
- **جدول خدمات:** ۹ ستون (ردیف/کد/نام خدمت/تعداد/مبلغ/تخفیف/سهم بیمه/سهم بیمار/
  عملیات)، راه‌راه (zebra)، آیکن سطل قرمز برای حذف هر سطر، نوار جمع پایین، و
  پیام حالت خالی. جمع کل صورتحساب از مجموع سطرها محاسبه می‌شود. (`recalc`)
- **پیش‌نمایش P/S:** مربع P سبز و مربع S زرد؛ کلیک روی هرکدام فوکوس را به
  تکست‌باکس مبلغ متناظر می‌برد.
- **دکمهٔ «افزودن به صندوق نرفته‌ها»:** برای بیمارانی که اکنون پرداخت نمی‌کنند —
  رکورد با `paid=0` ذخیره و در `data/unpaid_box.dat` برچسب می‌خورد.
- **دکمهٔ «افزودن به صف پذیرش»:** برای مواقعی مثل قطعی اینترنت — رکورد در
  `data/recept_queue.dat` صف می‌شود. تیک «عدم پرداخت در حال حاضر» هم افزوده شد.

### Changed
- **قانون جدید Enter در پذیرش:** اگر بیمار از پیش ثبت نشده باشد → نام Enter،
  نام‌خانوادگی Enter، کد ملی. اگر ثبت شده باشد → کد ملی Enter، داده‌ها پر می‌شود
  و Enter بعدی به اولین فیلد **خالی** می‌پرد (فیلدهای پر رد می‌شوند).
  (`nidEditProc`)
- **فیلدهای الزامی:** فقط ۵ فیلد اول نمی‌توانند خالی بمانند — نام، نام خانوادگی،
  کد ملی، **نام پدر**، تاریخ تولد (تلفن همراه دیگر اجباری نیست).
- **راست‌چین بودن تکست‌باکس‌ها:** سبک پایهٔ EDIT با `ES_RIGHT` تنظیم شد تا همهٔ
  تکست‌باکس‌ها به‌صورت پیش‌فرض راست‌چین باشند.

### Files
- `src/reception.cpp` — بازطراحی کامل فرم پذیرش (کارت‌ها، جدول خدمات، پنل
  افزودن خدمت، دکمه‌های صندوق نرفته‌ها/صف پذیرش، هندلرها و رویدادهای ماوس).
- `src/app.h`, `src/app.rc` — ارتقای نسخه به 1.25.0.

---

## 1.24.0 — 2026-06-30

> **رفع کامل اشکالات بخش «طراحی چاپ» و بازطراحی ۲۰ طرح آمادهٔ پذیرش —
> راست‌به‌چپ درست، لوگو همیشه داخل کادر، طرح‌های حرفه‌ای و متمایز، و
> واکنش‌گرایی هنگام تغییر اندازهٔ کاغذ.**

### Fixed
- **اشکال RTL/LTR رفع شد:** پیش‌تر انتخاب «راست‌به‌چپ» متن را به سمت چپ
  می‌برد و تعویض RTL/LTR هیچ تفاوتی ایجاد نمی‌کرد. حالا سلکتور «جهت متن»
  چینش افقی را هم به‌صورت خودکار جفت می‌کند: RTL → راست‌چین، LTR → چپ‌چین،
  Center → وسط‌چین. (`assets/designer/designer.js`)
- **اشکال جابه‌جایی/بیرون‌زدگی لوگو رفع شد:** لوگو دقیقاً در همان کادر، با
  همان جا و اندازه باقی می‌ماند، هرگز بیرون نمی‌زند، روی متن نمی‌افتد و به
  وسط صفحه نمی‌پرد (hard-clip در `gpDrawImageRectFit` + `overflow:hidden`
  در پیش‌نمایش + مقیاس‌گذاری یکنواخت لوگو/QR در ری‌فلو تا نسبت تصویر حفظ شود).
- **خروجی چاپ ناخوانا/نامرتب رفع شد:** همهٔ طرح‌های جدید به‌صورت پیش‌فرض
  RTL (`dir:0`/`align:0`) ساخته شده‌اند و موتور چاپ GDI با
  `DT_RIGHT|DT_RTLREADING` به‌درستی راست‌چین می‌کند → خروجی پذیرش تمیز و حرفه‌ای.
- **نصف‌شدن/بریده‌شدن کلمات و لیبل‌ها رفع شد (مشکل گزارش‌شدهٔ چاپ):** پیش‌تر
  اگر متن از کادر بزرگ‌تر بود، با `DT_WORDBREAK` به خط دوم می‌رفت و چون ارتفاع
  کادر کم بود، نیمی از حروف بریده می‌شد (مثلاً «ر» در «مرد» دیده نمی‌شد). حالا
  موتور چاپ و پیش‌نمایش هر دو **اندازهٔ فونت را خودکار کوچک می‌کنند تا متن کامل
  داخل کادر جا شود** (auto-fit) و با `DT_NOCLIP` آخرین خط هرگز بریده نمی‌شود.
  (`src/printer.cpp`, `assets/designer/designer.js`)
- **متن‌ها به سمت راست نمی‌آمدند رفع شد:** رندر متن در پیش‌نمایش از flexbox به
  **چیدمان بلوکی** تغییر کرد تا متن RTL همیشه دقیقاً به لبهٔ راست بچسبد و هرگز
  سمت چپ کادر شناور نشود؛ جهت (RTL/LTR/Center) و چینش کاملاً با موتور چاپ یکی
  است. (`assets/designer/designer.js`, `assets/designer/designer.css`)
- **چاپ‌نشدن لوگو/عکس رفع و تأیید شد:** مسیر تصویر (`data:base64`) از طراح تا
  موتور چاپ کامل حفظ می‌شود؛ تصویر با `gpDrawImageRectFit` داخل کادر hard-clip
  و رسم می‌گردد (تست شد: تصویر دقیقاً درون کادر و بدون بیرون‌زدگی).

### Added
- **۲۰ طرح آمادهٔ پذیرش کاملاً متمایز و حرفه‌ای:** به‌جای طرح‌های تقریباً
  یکسانِ قبلی، حالا ۲۰ طرح دست‌ساز با رنگ تأکیدی، چیدمان، اندازهٔ کاغذ
  (A4/A5/A6/فیش ۸۰/فیش ۵۸) و مجموعهٔ فیلد متفاوت ارائه می‌شود.
  (`assets/designer/templates.js`)
- **فیلدهای جدید در فهرست سمت‌راست:** دستهٔ «بالینی و علائم حیاتی» و
  فیلدهای جدید پزشک/نوبت و مالی: `{refdoctor}`، `{room}`، `{nextvisit}`،
  `{weight}`، `{height}`، `{bp}`، `{temp}`، `{pulse}`، `{allergy}`،
  `{diagnosis}`، `{servicecode}`، `{visitfee}`، `{paytype}`، `{cashier}`،
  `{insshareonly}`. (`assets/designer/fields.js` + resolverها در
  `src/printer.cpp`)

### Changed
- **واکنش‌گرایی طرح‌ها هنگام تغییر کاغذ:** `reflowItems` اکنون مختصات،
  اندازه، فونت (`pt`)، padding، corner و ضخامت خط را متناسب با کاغذ جدید
  مقیاس می‌دهد؛ آیتم‌ها بزرگ/کوچک می‌شوند و چیدمان حفظ می‌گردد. لوگو/QR با
  ضریب یکنواخت مقیاس می‌گیرند تا کِش نیایند. (`assets/designer/designer.js`)
- نسخه به **1.24.0** ارتقا یافت (`src/app.h`, `src/app.rc`).

---

## 1.23.0 — 2026-06-29

> **بازسازی موتور چاپ و طراح چاپ — خروجی چاپ دقیقاً برابر پیش‌نمایش طراح
> (WYSIWYG واقعی). رفع کامل مشکل لوگو/تصویر بیرون‌زدگی، RTL، چیدمان و فاصله‌ها.**

### Fixed
- **بیرون‌زدگی لوگو/تصویر رفع شد (مشکل اصلی):** پیش‌تر موتور چاپ تصویر را فقط
  به‌صورت contain با کلیپ رسم می‌کرد، ولی پیش‌نمایش طراح اصلاً تصویر واقعی را نشان
  نمی‌داد (فقط حرف placeholder «ل/ع/ت»). حالا هر دو از **یک موتور مشترک**
  (`gpDrawImageRectFit`) استفاده می‌کنند؛ تصویر همیشه درون مستطیل تعریف‌شده
  hard-clip می‌شود و هرگز بیرون نمی‌زند، کشیده نمی‌شود و روی متن نمی‌افتد.
- **پیش‌نمایش طراح ≠ خروجی چاپ رفع شد:** پیش‌نمایش حالا تصویر واقعی لوگو/عکس را
  با همان object-fit و padding و حذف کادر اضافی رسم می‌کند → پیش‌نمایش پیکسل‌به‌پیکسل
  با چاپ یکی است.
- **چیدمان عمودی متن در چاپ و پیش‌نمایش هماهنگ شد:** هر دو از فیلد `valign`
  (بالا/وسط/پایین) پیروی می‌کنند (با اندازه‌گیری بلاک متن چندخطی در موتور چاپ).
- **فاصلهٔ داخلی (padding) متن و تصویر** اکنون در چاپ هم اعمال می‌شود (قبلاً نادیده
  گرفته می‌شد) و با پیش‌نمایش یکی است.

### Added
- **پشتیبانی object-fit برای لوگو/تصویر/عکس:** سه حالت **داخل (contain)**،
  **پوشش (cover)** و **کشیده (fill)** — مطابق تنظیمات طراح. (`PrintItem.objectFit`)
- **چینش عمودی متن (`PrintItem.valign`):** بالا / وسط / پایین.
- **کنترل‌های جدید در inspector طراح چاپ:** سلکتورهای سه‌بخشی برای چیدمان افقی،
  چیدمان عمودی، جهت متن (فارسی/لاتین/خودکار) و تناسب تصویر + فیلد حاشیهٔ داخلی،
  همگی با کنترل‌های استاندارد Win32 (بدون owner-draw، crash-safe).
- **رندر تصویر باکیفیت‌تر:** افزودن `CompositingQualityHighQuality` و
  `WrapModeTileFlipXY` برای حذف لبه‌های محو/درز روی حالت cover.

### Changed
- `PrintItem` دو فیلد جدید `objectFit` و `valign` گرفت؛ هر دو با مقدار پیش‌فرض
  سازگار با نسخه‌های قبل (contain / top) و **round-trip کامل** در JSON بومی
  (`fit`/`valign`) و JSON وب‌دیزاینر (`objectFit` رشته‌ای + `valign`). طرح‌های
  قدیمی بدون این کلیدها بدون تغییر ظاهر بارگذاری می‌شوند.
- فونت متن چاپ با `OUT_TT_PRECIS` + `CLIP_DEFAULT_PRECIS` ساخته می‌شود (شکل‌دهی و
  baseline دقیق‌تر فارسی).

### فایل‌های تغییر یافته
- `src/print_designer.h` — فیلدهای `objectFit` و `valign`.
- `src/print_designer.cpp` — مقداردهی پیش‌فرض + serialize/parse بومی.
- `src/web_designer.cpp` — serialize/parse JSON وب‌دیزاینر برای فیلدهای جدید.
- `src/gdiplus.cpp` — موتور مشترک `gpDrawImageRectFit` (contain/cover/fill +
  padding + hard-clip)؛ `gpDrawImageRectAny` اکنون wrapper آن است.
- `src/app.h` — اعلان `gpDrawImageRectFit` + bump نسخه به 1.23.0.
- `src/printer.cpp` — موتور چاپ: object-fit/padding تصویر + چینش عمودی و padding متن.
- `src/print_designer_ui.inc` — پیش‌نمایش طراح: رندر تصویر واقعی + valign/dir/padding،
  و کنترل‌های جدید inspector.
- `src/app.rc` — همگام‌سازی نسخه به 1.23.0.

---

## 1.21.2 — 2026-06-28

> **رفع خطای «This app doesn't support print preview» هنگام چاپ، و چاپ روی
> اندازهٔ کاغذ دقیقِ طرح (مثلاً A5 واقعی).**

### Fixed
- **«This app doesn't support print preview» رفع شد:** این پیام را خودِ ویندوز
  وقتی نشان می‌داد که چاپگر پیش‌فرض/ذخیره‌شده یک «چاپگر مجازی برنامه‌ای»
  (مثل Microsoft Print to PDF یا برنامهٔ عکس) بود و کار چاپ را رد می‌کرد. اکنون
  اگر `StartDoc` شکست بخورد، به‌صورت خودکار پنجرهٔ انتخاب چاپگر باز می‌شود تا
  کاربر یک چاپگر واقعی انتخاب کند؛ و اگر باز هم نشد پیام راهنمای فارسی نمایش
  داده می‌شود. (`src/printer.cpp` در `printPrintDesign`، `src/billing.cpp` در
  `printReceipt`/«چاپ آخرین قبض»)
- **چاپ روی اندازهٔ کاغذِ طرح:** قبلاً DC چاپ با DEVMODE پیش‌فرض چاپگر ساخته
  می‌شد (معمولاً A4)، پس طرح A5 روی A4 چاپ می‌شد. حالا `pdCreatePrinterDC`
  اندازهٔ کاغذ و جهت را از روی طرح در DEVMODE تنظیم می‌کند:
  A3/A4/A5/A6/B5/Letter/Legal با کد استاندارد `DMPAPER_*`، و
  R80/R58/L90/L100/سفارشی با ابعاد صریح (`DMPAPER_USER` + عرض/طول بر حسب
  دهم‌میلی‌متر). (`src/printer.cpp`)
- نسخه به **1.21.2** ارتقا یافت و EXE بازساخته شد.

---

## 1.21.1 — 2026-06-28

> **رفع باگ بارگذاری طرح در «تنظیمات چاپ» + رفع تداخل/درشتی متن در پیش‌نمایش.**

### Fixed
- **«فایل طرح نامعتبر است (AZTEMPLATE/1 یافت نشد)» رفع شد:** طراح وب فایل را در
  قالب JSON خودش (بدون سرآمد AZTEMPLATE) دانلود می‌کرد، ولی «بارگذاری» تنظیمات
  چاپ فقط قالب بومی را می‌پذیرفت. اکنون `pcUpload` **هر دو قالب** را می‌پذیرد
  (ابتدا بومی، سپس قالب طراح وب). تابع‌های `Design_ToWebJson`/`Design_FromWebJson`
  در هدر expose شدند. حذف BOM و بررسی «طرح بدون آیتم» هم اضافه شد.
  (`src/manage.inc`، `src/web_designer.cpp`، `src/print_designer.h`)
- **تداخل/درهم‌ریختگی و درشتی متن در پیش‌نمایش تنظیمات چاپ:** ضریب اندازهٔ فونت
  اشتباه بود (`3.78/2.83` ≈ ۳٫۸ برابر بزرگ‌تر). به فرمول درست WYSIWYG
  (`pt × pxPerMm × 25.4/72`) اصلاح شد؛ هم برای متن و هم برای جدول.
  (`src/manage.inc`)
- نسخه به **1.21.1** ارتقا یافت و EXE بازساخته شد.

---

## 1.21.0 — 2026-06-28

> **چیدمان درست طراح (منو/تنظیمات راست، پیش‌نمایش چپ)، دانلود واقعی فایل،
> ذخیرهٔ مطمئن با گزارش خطا در کنسول، ابزار «جدول» کامل، اندازه‌های کاغذ بیشتر
> (لیزری/فیش‌پرینتر)، و چاپ دقیقاً مطابق طراحی (WYSIWYG) با رنگ صحیح و پر شدن
> پس‌زمینهٔ کادرها.**

### Fixed — رفع باگ‌ها
- **چیدمان معکوس طراح اصلاح شد:** پنل ابزار/تنظیمات حالا در **سمت راست** و
  بوم پیش‌نمایش در **سمت چپ** قرار دارد (ترتیب DOM + `order` در فلکس RTL).
  (`assets/designer/index.html`، `assets/designer/designer.css`)
- **رنگ‌ها در چاپ و پیش‌نمایش درست شد:** رنگ‌ها به‌صورت `0x00RRGGBB` ذخیره
  می‌شدند ولی مستقیماً به‌عنوان `COLORREF` (که `0x00BBGGRR` است) استفاده
  می‌شدند → جابه‌جایی قرمز/آبی. اکنون با `pdCR/pcCR` تبدیل می‌شوند.
  (`src/printer.cpp`، `src/manage.inc`)
- **تنظیمات چاپ (افت FPS / تصویر خراب / درهم‌ریختگی):** شاخهٔ رندر تصویر
  (`PIT_IMAGE/LOGO/PHOTO`) و پر کردن پس‌زمینهٔ کادرها در پیش‌نمایش پیاده شد؛
  پیش‌نمایش با دابل‌بافر رسم می‌شود و دقیقاً طرح طراحی‌شده را نشان می‌دهد.
  (`src/manage.inc`)
- **WYSIWYG چاپ:** پر شدن پس‌زمینهٔ `PIT_RECT/PIT_FRAME` هنگام شفاف‌نبودن،
  و یکسان‌سازی رنگ/فونت بین بوم، پیش‌نمایش و چاپ. (`src/printer.cpp`)

### Added — قابلیت‌های جدید
- **ابزار جدول (`PIT_TABLE`):** افزودن جدول کامل با تعداد ستون/سطر، سطر سرستون،
  عرض ستون‌ها و ویرایش محتوای سلول‌ها (با پشتیبانی از `{field}`). مدل جدول به
  صورت JSON داخل فیلد `text` ذخیره می‌شود تا بدون تغییر ساختار داده round-trip
  شود. رندر یکسان در بوم/پیش‌نمایش/چاپ.
  (`src/print_designer.h`، `src/printer.cpp`، `src/manage.inc`,
   `src/web_designer.cpp`، `assets/designer/*`)
- **دانلود واقعی فایل:** چون طراح در مرورگر پیش‌فرض اجرا می‌شود، دکمهٔ دانلود
  حالا با `Blob`/`createObjectURL` یک فایل واقعی `.aztpl` می‌سازد. در تنظیمات
  چاپ نیز دانلود با `GetSaveFileNameW` فایل واقعی ذخیره می‌کند. (`assets/designer/designer.js`)
- **اندازه‌های کاغذ بیشتر:** Legal، فیش‌پرینتر `R80/R58` و کاغذ کوچک
  `L90/L100` به جدول اندازه‌ها اضافه شد. (`src/print_designer.cpp`، `assets/designer/designer.js`)

### Changed
- نسخه به **1.21.0** ارتقا یافت. (`src/app.h`، `src/app.rc`)
- EXE بازساخته‌شده در `build/AzadiTeb.exe` جایگزین شد.

---

## 1.20.0 — 2026-06-27

> **بازنویسی کامل طراح چاپ (UI حرفه‌ای)، رفع قطعی باگ «خطا در ذخیره»، حذف
> هشدارهای کنسول (sync XHR)، ۶۰ طرح آمادهٔ حرفه‌ای، جابه‌جایی صفحه با ماوس،
> و چاپ صحیح اطلاعات بیمار + لوگو/عکس روی طرح ذخیره‌شده.**

### Fixed — رفع باگ ذخیره و هشدارهای کنسول
- **علت ریشه‌ای «خطا در ذخیره» و هشدارهای کنسول رفع شد:** درخواست‌های `/api/*`
  قبلاً به‌صورت **همزمان (synchronous XHR)** و با مقدار `timeout` ارسال می‌شدند.
  تنظیم `timeout` روی XHR همزمان در مرورگر استثنا می‌انداخت → ذخیره شکست می‌خورد و
  این پیام‌ها در کنسول ظاهر می‌شدند:
  «Synchronous XMLHttpRequest … is deprecated» و «timeout attribute is not
  supported in the synchronous mode». اکنون همهٔ درخواست‌ها **ناهمزمان (async)**
  هستند؛ ذخیره بدون بلاک‌شدن UI و بدون هیچ هشداری انجام می‌شود.
  (`assets/designer/designer.js`)
- **تست خودکار (Playwright):** بارگذاری طراح، اعمال طرح، افزودن فیلد، و گردش کامل
  `/api/save` تأیید شد — نتیجه `{"ok":true,"id":…}`، پیام «ذخیره شد و بر بخش اعمال
  گردید ✓»، **صفر هشدار sync-XHR، صفر خطای صفحه**. (`scripts/test_designer.py`)
- **چاپ اطلاعات بیمار روی طرح:** طرح‌های آماده اکنون از فیلدهای داده‌محور با کلید
  `{full}`،`{nid}`،`{paid}`،`{queue}` و … ساخته می‌شوند؛ چون ذخیره درست کار می‌کند،
  `SectionDesign_Resolve` همان طرح را برمی‌گرداند و `pdFieldValue` مقدار واقعی بیمار
  را جایگزین می‌کند (دیگر «قالب خالی» چاپ نمی‌شود). (`src/printer.cpp`)

### Added — UI کاملاً بازطراحی‌شدهٔ حرفه‌ای
- **پنجرهٔ «✨ طرح‌های آماده»** در بالای صفحه: گالری با سه دستهٔ تفکیک‌شده
  (پذیرش / نوبت‌دهی / آزمایشگاه و رادیولوژی)، هرکدام **۲۰ طرح حرفه‌ای** (مجموعاً
  ۶۰)، با تصویر بندانگشتیِ مقیاس‌شدهٔ واقعی هر طرح. (`assets/designer/templates.js`,
  `index.html`, `designer.css`)
- **پالت آبی فیلدها:** آیتم‌های سمت راست فیلدهای داده با ظاهر آبی هستند؛ روی بوم نیز
  فقط **برچسب فیلد داخل کروشه** نمایش داده می‌شود (مثل `[نام و نام خانوادگی]`) —
  **هیچ مقدار نمونه/مثالی** دیگر نوشته نمی‌شود.
- **جابه‌جایی صفحه با ماوس (pan):** کشیدن فضای خالی بوم صفحه را جابه‌جا می‌کند؛
  چرخ ماوس = اسکرول، `Ctrl`+چرخ = زوم، کلیدهای جهت = جابه‌جایی دقیق آیتم.
- **بارگذاری تصویر برای لوگو و عکس بیمار:** آیتم‌های «لوگو/عکس بیمار/تصویر» دکمهٔ
  «بارگذاری تصویر…» دارند؛ تصویر به‌صورت data-URI ذخیره و در زمان چاپ با
  `gpDrawImageRectAny` (پشتیبانی از مسیر فایل و base64) چاپ می‌شود.
  (`src/gdiplus.cpp`, `src/printer.cpp`, `src/web_designer.cpp`)
- **ریسپانسیو A4↔A5:** آیتم‌ها مبتنی بر میلی‌متر هستند و با تغییر اندازهٔ کاغذ،
  چیدمان به‌درستی بازچینش و متناسب (fit) می‌شود.
- بازرس آیتم (Inspector) و پنل لایه‌ها کاملاً بازنویسی شدند؛ ذخیره با `Ctrl+S`،
  بازگشت/جلو، زوم، دانلود/بارگذاری `.aztpl`.

### Changed
- نسخه به **1.20.0** ارتقا یافت. (`src/app.rc`)
- `imgPath` اکنون در پل JSON طراح وب (هر دو جهت) سریالایز/پارس می‌شود تا تصاویر
  بارگذاری‌شده با ذخیره حفظ شوند. (`src/web_designer.cpp`)

---

## 1.19.1 — 2026-06-27

> **رفع کامل ارور «not responding»/صفحهٔ سفید طراح چاپ، چیدمان LTR (پیش‌نمایش
> سمت چپ، ابزارها سمت راست)، و افزودن گزینهٔ «تنظیمات چاپ» به منوی تنظیمات
> حساب مدیریت.**

### Fixed — رفع هنگ/کرش/صفحهٔ سفید طراح چاپ
- میزبان embedded قبلی (کنترل WebBrowser/Trident) که روی بسیاری از سیستم‌ها
  صفحهٔ سفید و حالت «not responding» می‌داد، **حذف شد**. علت ریشه‌ای: درخواست
  همزمان (synchronous XHR) صفحهٔ Trident روی همان نخ UI که حلقهٔ پیام مودال
  اجرا می‌شد → بن‌بست. اکنون طراح چاپ در **مرورگر پیش‌فرض سیستم** باز می‌شود
  (`ShellExecute` روی آدرس loopback) که ضدکرش/ضدهنگ، سبک و روی همهٔ سیستم‌ها
  در دسترس است. (`src/web_designer.cpp`, `src/web_designer_host.inc`)
- sync با C++ کامل: صفحه از طریق `/api/save` ذخیره می‌کند، که طرح بخش را
  می‌نویسد و `WM_APP_DESIGN_PUSHED` را post می‌کند تا پذیرش بلافاصله طرح جدید
  را بردارد. یک پنجرهٔ کوچک بومی «جلسهٔ طراحی» امکان «باز کردن دوباره در مرورگر»
  و «پایان طراحی» را می‌دهد.

### Changed — چیدمان LTR و خوانایی
- صفحهٔ طراح از `dir="rtl"` به **`dir="ltr"`** تغییر کرد: اکنون **پیش‌نمایش/بوم
  سمت چپ** و **پالت ابزارها/تنظیمات آیتم سمت راست** قرار می‌گیرند (تأییدشده با
  تست مرورگر: canvasLeft=0، panelLeft=960). متن‌های فارسی همچنان RTL و خوانا
  هستند (کلاس‌های متنی `direction:rtl`). (`assets/designer/index.html`,
  `assets/designer/designer.css`)
- ریسپانسیو و بدون ارور JS؛ پل ارتباطی با timeout ۴ ثانیه تا هرگز UI هنگ نکند.
  (`assets/designer/designer.js`)
- انتخاب/جابه‌جایی آیتم با ماوس در پیش‌نمایش و باز شدن خودکار **تنظیمات آیتم در
  سمت راست** هنگام انتخاب — تأییدشده با تست (کلیک→انتخاب→اینسپکتور، درگ ۱۰→۲۲٫۵).

### Added — «تنظیمات چاپ» در منوی تنظیمات حساب مدیریت
- صفحهٔ «طراحی چاپ» در تنظیمات اکنون **دو دکمه** دارد: «باز کردن طراح چاپ» و
  دکمهٔ جدید **«تنظیمات چاپ»** که پنل انتخاب بخش/پیش‌نمایش/بزرگ‌نمایی/دانلود/
  بارگذاری/اعمال را به‌صورت مودال باز می‌کند. تابع عمومی `PrintCfg_Open()` افزوده
  شد. (`src/user_settings.cpp`, `src/manage.inc`, `src/print_designer.h`)

### Files
- Changed: `src/web_designer.cpp` (حذف وابستگی OLE/Trident، افزودن shellapi)،
  `src/web_designer_host.inc` (میزبان مرورگر سیستمی + پنجرهٔ جلسه)،
  `assets/designer/{index.html,designer.css,designer.js}` (LTR، پل مقاوم)،
  `src/user_settings.cpp` (+دکمهٔ تنظیمات چاپ، `buildDesignerPage`)،
  `src/manage.inc` (+`PrintCfg_Open` عمومی مودال)، `src/print_designer.h`
  (نمونه‌اولیهٔ `PrintCfg_Open`)، `src/app.h`/`src/app.rc` (نسخهٔ ۱.۱۹.۱).

---

## 1.19.0 — 2026-06-27

> **دیزاینر چاپ سبک و پایدار به‌صورت صفحهٔ وب جاسازی‌شده (HTML/CSS/JS/Bootstrap)
> داخل خود برنامه، گزینهٔ مدیریتی جدید «تنظیمات چاپ» (انتخاب بخش → پیش‌نمایش/بزرگ‌نمایی/
> دانلود/بارگذاری/اعمال طرح)، دکمهٔ «افزودن بیمار» با تمام فیلدهای پذیرش در پنل مدیریت،
> و مسیر چاپ تازه که طرح بخش را روی چاپگر متصل چاپ می‌کند.**

### Added — دیزاینر چاپ وب‌محور و سبک (جدید)
- دیزاینر چاپ اکنون به‌صورت یک **صفحهٔ HTML/CSS/JS (Bootstrap)** داخل خود برنامه
  باز می‌شود (نه مرورگر بیرونی). میزبانی با کنترل **WebBrowser (Trident)** که روی همهٔ
  ویندوزهای ۷ تا ۱۱ موجود است و سربار رم/CPU بسیار کم دارد؛ مناسب سیستم‌های ضعیف
  (۲ گیگ رم / دو هسته‌ای). در صورت نبود میزبان، **دیزاینر بومی** به‌عنوان پشتیبان اجرا می‌شود.
- پل ارتباطی JS↔C++ از طریق سرور HTTP لوکال (فقط `127.0.0.1`، پورت تصادفی، یک نخ
  accept مسدودکننده) — موتورمستقل و سبک. فایل‌ها: `src/web_designer.cpp`,
  `src/web_designer_http.inc`, `src/web_designer_host.inc`, `src/web_designer.h`,
  `assets/designer/*` (index.html, designer.css, bootstrap.min.css, fields.js,
  templates.js, designer.js) که به‌صورت `RCDATA 300–305` در EXE جاسازی شده‌اند.
- ۲۰ قالب آماده در بالای دیزاینر؛ قابل ویرایش؛ در صورت تغییر، به‌جای جایگزینی،
  **به‌صورت قالب جدید با نام دلخواه** ذخیره می‌شود.
- پالت با جست‌وجو در بالا و دسته‌بندی‌ها: ساعت/تاریخ، اطلاعات بیمار (همهٔ فیلدهای پذیرش)،
  بیمه/بیمه تکمیلی/شماره دفترچه، پزشک و نوبت، مالی/تخفیف/مبلغ نهایی، لوگو/برچسب/کادر.
- Inspector پیشرفته per-item: متن، رنگ متن/پس‌زمینه، اندازه، فونت، نوع/رنگ/ضخامت
  حاشیه، x/y، شفافیت، نمایش، قفل. تعامل بوم: درگ/تغییراندازه/چرخش، پن، Ctrl+اسکرول
  زوم، Ctrl+Z واگرد، Ctrl+S ذخیره، جابه‌جایی با کلیدهای جهت‌دار.

### Added — گزینهٔ مدیریتی «تنظیمات چاپ» (`src/manage.inc`, صفحهٔ `PG_PRINTCFG`)
- مودال جدید `AzPrintCfg`: فهرست بخش‌های تعریف‌شده در سمت راست، **پیش‌نمایش WYSIWYG**
  طرح فعلی هر بخش در سمت چپ، دکمه‌های **بزرگ‌نمایی**، **دانلود طرح فعلی** (به فایل
  `.aztpl`)، **بارگذاری طرح** (از فایل)، **اعمال** (طرح وارداتی روی بخش انتخاب‌شده).
- پس از «اعمال»، طرح به‌صورت یک ردیف **جدید** ثبت و به بخش متصل می‌شود
  (`Designs_Insert` + `SectionDesign_Set`) — هیچ طرح آماده‌ای بازنویسی نمی‌شود.

### Added — «افزودن بیمار» در پنل مدیریت (`src/manage.inc`, مودال `AzNewPatient`)
- دکمهٔ **«افزودن بیمار»** در صفحهٔ بیماران؛ مودال با همهٔ فیلدهای پذیرش (نام،
  نام‌خانوادگی، کد ملی، نام پدر، جنسیت، تاریخ تولد، تلفن همراه، تلفن ثابت، آدرس،
  بیمه، بیمه تکمیلی). ذخیره از طریق `rememberPatient()` در همان فروشگاهی که
  پذیرش با کد ملی می‌خواند؛ بیمار افزوده‌شده بلافاصله در پذیرش بازخوانی می‌شود.

### Added — مسیر چاپ تازه با طرح بخش (`src/printer.cpp`, `src/reception.cpp`, `src/app.h`)
- تابع `printPrintDesign(rec, sectionId, owner)`: طرح `PrintDesign` متصل به بخش را
  حل کرده و با GDI روی **چاپگر متصل** چاپ می‌کند. بار اول (هر نشست) دیالوگ استاندارد
  چاپ برای انتخاب چاپگر و کاغذ (A4/A5) باز می‌شود؛ سپس از چاپگر پیش‌فرض استفاده می‌شود.
- پذیرش اکنون بخش اپراتور را با `recResolveSectionId()` (نگاشت نام دپارتمان →
  `Section.id`) حل می‌کند و به‌ترتیب: ۱) `printPrintDesign` ۲) `printDesignedReceipt`
  ۳) `printReceipt` کلاسیک را امتحان می‌کند.

### Performance — پایداری روی سخت‌افزار ضعیف
- استفاده از Trident (موجود در خود ویندوز) به‌جای WebView2/Chromium سنگین؛ سرور HTTP
  تک‌نخ مسدودکننده (بدون busy-loop)، فقط هنگام باز بودن دیزاینر فعال؛ EXE استاتیک
  بدون وابستگی زمان‌اجرا؛ رندر چاپ با GDI ساده. تنظیم `designer_engine=native` برای
  اجبار به دیزاینر بومی روی سیستم‌های بسیار ضعیف در دسترس است.

### Files
- New: `src/web_designer.{h,cpp}`, `src/web_designer_http.inc`,
  `src/web_designer_host.inc`, `assets/designer/*`.
- Changed: `src/printer.cpp` (+`printPrintDesign`/`pdFieldValue`), `src/app.h`
  (+نمونه‌اولیه، نسخه ۱.۱۹.۰)، `src/app.rc` (RCDATA 300–305، نسخه)،
  `src/reception.cpp` (مسیر چاپ + `recResolveSectionId`)، `src/manage.inc`
  (+`AzNewPatient`/`AzPrintCfg`/`PG_PRINTCFG`)، `src/admin.cpp` (اینکلودها)،
  `src/print_designer.cpp` / `src/print_designer_ui.inc` (میزبان وب با پشتیبان بومی)،
  `build.sh` (+`web_designer.cpp` و `-lws2_32`).

---

## 1.18.3 — 2026-06-27

> **رفع کرش دیزاینر چاپ (بازنویسی کامل با کنترل‌های استاندارد)، جابه‌جایی دکمه‌های
> چاپ پذیرش به زیر کارت «مبلغ نهایی»، رفع باگ تغییر تم، و افزودن سازندهٔ پیام
> (ساخت پیام + پیوست فایل) به «پیام‌های ذخیره‌شده».**

### Fixed — کرش دیزاینر چاپ `ACCESS_VIOLATION 0xC0000005` (`src/print_designer_ui.inc`)
- **ریشهٔ مشکل:** پنل Inspector دیزاینر از کنترل‌های سفارشی owner-draw
  (`AzNumberSpinner` / `AzColorPicker` / `AzSwitch` در `ui_kit.cpp`) استفاده می‌کرد
  که منبع کرش قطعی در `AzadiTeb.exe+0x12789` بود.
- **راه‌حل (طبق درخواست کاربر «با کتابخانه/زبان دیگر بازنویسی شود»):** کل Inspector
  با **کنترل‌های استاندارد Win32** بازنویسی شد:
  - ویژگی‌های عددی → `EDIT` (IDها 6001–6006) با تابع `readNumEdit()` که ارقام
    فارسی/عربی را به ASCII نرمال می‌کند و در `EN_KILLFOCUS` ثبت می‌شود.
  - رنگ‌ها → دکمه‌های `BS_OWNERDRAW` (6007 متن، 6009 حاشیه) که `ChooseColorW`
    باز می‌کنند؛ رندر در `WM_DRAWITEM`.
  - قفل → `BS_AUTOCHECKBOX` (6010)؛ متن/پیشوند → `EDIT` (6008/6013)؛
    حذف/تکثیر → `BS_PUSHBUTTON` (6011/6012)؛ آیتم‌های پالت → `BUTTON` ساده.
  - `WM_CTLCOLOREDIT` برای هماهنگی تم؛ حذف فراخوانی `AzLayoutGuard_Verify()`.

### Changed — جای دکمه‌های چاپ پذیرش (`src/reception.cpp`)
- سه دکمهٔ **رسید بیمه / چاپ نسخه / چاپ آخرین قبض (F8)** اکنون دقیقاً **زیر کارت
  آبی «مبلغ نهایی»** در ستون صورتحساب چپ قرار می‌گیرند (طبق تصویر مرجع).
- تابع مشترک `recPrintGroupTop()` افزوده شد و هم در `tabPageLayout`، هم در نقاشی
  عنوان گروه «چاپ» و هم در `recPageVH` (ارتفاع کارت صورتحساب) استفاده می‌شود تا
  چیدمان و نقاشی هرگز از هم جدا نشوند.

### Verified — رفتارهای پذیرش (`src/reception.cpp`)
- **Enter** فوکوس را به فیلد بعدی می‌برد (نام → نام‌خانوادگی → کد ملی → نام پدر →
  تاریخ تولد → …) از طریق `enableEnterNavigation`/`hopField`.
- اگر **کد ملی** پر باشد و Enter زده شود، اطلاعات بیمار از پایگاه‌داده
  (`patients.dat`) به‌صورت خودکار در تکست‌باکس‌ها پر می‌شود
  (`nidEditProc`→`doInquiry`→`lookupCitizen`).
- **Ctrl+R** همهٔ تکست‌باکس‌ها را پاک می‌کند (معادل دکمهٔ «پاک کردن» → `resetForm`).

### Fixed — باگ تغییر تم سفید↔مشکی (`src/theme.cpp`)
- پس‌زمینهٔ دکمه‌ها دیگر رنگ مطلق را cache نمی‌کند؛ یک **توکن معنایی**
  (`BtnBgToken`: `BBG_PARENT`/`BBG_BG`/`BBG_SURFACE`/…) ذخیره می‌شود و در
  `WM_PAINT` از طریق `btnBgColor()` به رنگ **زندهٔ** `g_theme` تبدیل می‌شود، لذا
  هنگام تعویض تم پس‌زمینهٔ مشکی کهنه باقی نمی‌ماند.

### Added — سازندهٔ پیام در «پیام‌های ذخیره‌شده» (`src/saved_messages.cpp`)
- دکمهٔ **«پیام جدید»** در نوار بالای پنجره افزوده شد.
- پنجرهٔ سازنده (modal) شامل: انتخاب اولویت (عادی/فوری/بحرانی)، کادر متن چندخطی،
  دکمهٔ **«افزودن فایل پیوست»** (`GetOpenFileNameW`)، و دکمه‌های ذخیره/انصراف.
- پیوست از طریق `copyAttachmentLocal()` در `data\attachments\` کپی و سپس با
  `pushSavedMsg(...)` به‌صورت محلی ذخیره می‌شود؛ لیست بلافاصله بازخوانی می‌شود.
- پیام‌ها همچنان به‌صورت **مستطیل‌های مجزا** (کارت‌های گردگوشه) نمایش داده می‌شوند؛
  با کلیک روی کارتِ دارای پیوست، گزینهٔ باز کردن فایل (`ShellExecuteW`) ارائه می‌شود.

### Build
- ارتقای نسخه به `1.18.3` در `src/app.h` (`APP_VERSION_W`)، `src/app.rc`
  (`FILEVERSION`/`PRODUCTVERSION` = `1,18,3,0`) و `update/version.txt`.
- بازتولید EXE تک‌فایلی استاتیک 32 بیتی در `build/AzadiTeb.exe`.

---

## 1.18.2 — 2026-06-26

> **جابه‌جایی ردیف دکمه‌های عملیات پذیرش به محل صحیح طبق تصویر هدف و اسپک:
> «زیر کارت بیمه و بالای بخش خدمات».**

### Changed — ترتیب عمودی صفحهٔ پذیرش (`src/reception.cpp`)
- طبق اسپک («Action buttons located below Insurance and above Service section»)
  و تصویر مرجع، ردیف دکمه‌های **ثبت پذیرش و صدور قبض / پذیرش جدید / پاک کردن /
  انصراف** اکنون در `computeCenterV()` دقیقاً **زیر کارت «بیمه و نوبت»** و **بالای
  جدول «افزودن خدمت»** قرار می‌گیرد (پیش از این، ردیف دکمه‌ها بعد از جدول خدمات بود).
- ترتیب نهایی ستون مرکزی: کارت مشخصات بیمار → کارت بیمه و نوبت → **ردیف دکمه‌های
  عملیات** → جدول خدمات. سایر متریک‌های چیدمان (فاصله‌ها، ارتفاع‌ها) دست‌نخورده ماند.

### Build
- ارتقای نسخه به `1.18.2` در `src/app.h` (`APP_VERSION_W`)، `src/app.rc`
  (`FILEVERSION`/`PRODUCTVERSION` = `1,18,2,0`) و `update/version.txt`.
- بازتولید EXE تک‌فایلی استاتیک 32 بیتی در `build/AzadiTeb.exe`.

---

## 1.18.1 — 2026-06-26

> **رفع کرش «طراحی پرینت» (دیزاینر چاپ)، اسپلش راه‌اندازی روی هر اجرا، و
> هماهنگی نهایی ردیف دکمه‌های پذیرش با تصویر هدف.**

### Fixed — کرش دیزاینر چاپ (ACCESS_VIOLATION 0xC0000005) (`src/print_designer_ui.inc`)
- **ریشهٔ مشکل:** صفحهٔ Inspector دیزاینر چاپ کنترل‌های سفارشی
  (`AzNumberSpinner` / `AzColorPicker` / `AzSwitch`) می‌سازد که کلاس پنجرهٔ آن‌ها
  توسط `uikit::Az_RegisterControls()` ثبت می‌شود. این ثبت فقط هنگام باز شدن صفحهٔ
  «تنظیمات کاربری» انجام می‌شد. وقتی دیزاینر از **پنل مدیریت** («طراحی پرینت / دیزاین
  چاپ») بدون باز شدن آن صفحه باز می‌شد، کلاس‌ها ثبت‌نشده بودند، `CreateWindowExW`
  برای هر کنترل `NULL` برمی‌گرداند و اولین دسترسی به آن هندل صفر کرش می‌داد —
  دقیقاً همان کرش گزارش‌شده در «print designer: open editor».
- **راه‌حل:** فراخوانی `uikit::Az_RegisterControls()` در ابتدای
  `PrintDesigner_OpenCore` (و نیز داخل اسپلش راه‌اندازی)؛ idempotent و سبک، بدون
  اشغال هیچ منبع سخت‌افزاری. اکنون دیزاینر روی هر سیستمی (حتی ضعیف، حتی بدون
  پرینتر متصل) باز می‌شود — مسیر باز شدن هیچ API چاپگر/اسپولر را صدا نمی‌زند.

### Changed — اسپلش راه‌اندازی روی هر اجرا (`src/setup_splash.cpp`)
- اسپلش لودینگ اکنون در **هر بار اجرا** نمایش داده می‌شود و سیستم کاربر را آماده
  می‌کند: بررسی پوشه‌ها، **شناسایی سخت‌افزار** (CPU/RAM، تشخیص سیستم ضعیف و ذخیرهٔ
  `sys_low_spec`)، آزمایش موتور گرافیکی GDI+، و **ثبت اجزای رابط کاربری**.
- کار سنگین یک‌باره (نصب فونت Vazirmatn) فقط در اولین اجرای هر نسخه انجام می‌شود؛
  روی سیستم‌های ضعیف، زمان‌بندی اسپلش کوتاه‌تر می‌شود.

### Changed — ردیف دکمه‌های پذیرش مطابق تصویر هدف (`src/reception.cpp`)
- ترتیب راست‌به‌چپ اصلاح شد: «ثبت پذیرش و صدور قبض» (آبی) | «پذیرش جدید» |
  «پاک کردن» (قرمز) | «انصراف». دکمهٔ «پاک کردن» با استایل خطر (قرمز، آیکن سطل
  زباله) و «پذیرش جدید» با آیکن جهت‌دار.

### Files
- `src/print_designer_ui.inc`، `src/setup_splash.cpp`، `src/reception.cpp`،
  `src/app.h`، `src/app.rc`، `update/version.txt`

---

## 1.18.0 — 2026-06-25

> **بازطراحی کامل ظاهر صفحهٔ «پذیرش بیمار» مطابق دیزاین هدف.**
> هدف: تبدیل چیدمان قبلی (دو ستونی/عمودی فشرده) به یک چیدمان مدرن، تمیز و
> سازمان‌یافته مطابق تصویر هدف؛ همگی ۱۰۰٪ بومی C++/GDI+ بدون هیچ HTML.

### Changed — چیدمان جدید سه‌ناحیه‌ای (`src/reception.cpp`)
- **متریک افقی جدید (`RecH`/`rcH()`)**: ناحیهٔ مرکزی فرم بین یک ستون
  «صورتحساب/چاپ» (راست‌چین، عرض ثابت) و یک «پنل اطلاعات بیمار» (چپ) قرار
  می‌گیرد. در عرض‌های کم به‌صورت تک‌ستونه (stacked) درمی‌آید.
- **متریک عمودی جدید (`CenterV`/`computeCenterV()`)**: ارتفاع هر بخش
  (مشخصات بیمار، بیمه و نوبت، جدول خدمات، ردیف دکمه‌ها) دقیق محاسبه می‌شود.
- **کارت «مشخصات بیمار»**: گرید سه‌ستونی (نام/نام خانوادگی/کد ملی، نام پدر/
  تاریخ تولد/جنسیت، موبایل/تلفن/آدرس) با برچسب‌های راست‌چین و ستارهٔ قرمز
  برای فیلدهای الزامی.
- **کارت «بیمه و نوبت»**: گرید چهارستونی (نوع پذیرش/نوع نوبت/نوع بیمه/نوع
  بیمه پایه) و ردیف دوستونی (مبلغ خدمت/تخفیف).
- **جدول خدمات**: نوار هدر هفت‌ستونی + حالت خالی با آیکن جعبهٔ باز و پیام
  «هیچ خدمتی اضافه نشده است.»؛ نوار ابزار با «افزودن خدمت» و جستجوی خدمت.
- **ردیف دکمه‌ها (راست‌چین)**: «ثبت پذیرش و صدور قبض» (Primary) | «پاک کردن»
  | «پذیرش جدید» | «انصراف».
- **کارت صورتحساب (راست)**: ردیف‌های مبلغ و یک ردیف «چک پرداختی» با پس‌زمینهٔ
  سرمه‌ای تیره و متن سفید، به‌علاوهٔ گروه دکمه‌های «چاپ».
- **پنل اطلاعات بیمار (چپ)**: هدر «بیمار جدید / بدون سابقه»، چیپ سبز
  «نسخه الکترونیک»، و دو جعبهٔ شمارنده «نسخه (سرپایی)» و «نسخه (الکترونیکی)».

### Added — کنترل‌های جدید (`src/reception.cpp`)
- شناسه‌ها: `ID_F_NEW` (667)، `ID_F_SVC_ADD` (668)، `ID_F_SVC_SEARCH` (669).
- دکمه‌های «پذیرش جدید» و «افزودن خدمت» و فیلد «جستجوی خدمت» با هندلرهای
  متناظر در `WM_COMMAND`.

### Files
- `src/reception.cpp` (بازنویسی متریک‌ها، چیدمان، نقاشی، کنترل‌ها)
- `src/app.h`, `src/app.rc`, `update/version.txt` (نسخه → 1.18.0)
- `docs/CHANGELOG.md` (این ورودی)

---

## 1.17.0 — 2026-06-25

> **بازنویسی کامل صفحهٔ «پذیرش بیمار» به‌صورت ۱۰۰٪ C++ بومی و حذف کامل HTML/MSHTML.**
> هدف: حذف وابستگی به موتور IE/Trident، رفع کرش طراح چاپ، اصلاح لیست بخش‌ها و
> بیمه‌ها، اسکرول روان، و ساخت EXE سبک‌تر و پایدارتر.

### Removed — حذف کامل HTML و موتور MSHTML/Trident
- فایل‌های میزبان وب و دارایی‌های HTML/CSS/JS حذف فیزیکی شدند:
  `src/webhost.cpp`, `src/webhost.h`, `src/webhost_assets.inc`,
  `src/webhost_bridge.inc`, `src/webhost_host.inc`, `src/webhost_run.inc`.
- `build.sh`: `src/webhost.cpp` از فهرست `SRCS` حذف شد؛ دیگر هیچ کد HTML
  کامپایل نمی‌شود. خروجی EXE از ~۳٫۲MB به ~۲٫۹MB کاهش یافت و وابستگی
  IE/Trident به‌طور کامل قطع شد (تک‌موتوره و crash-safe).

### Changed — رابط پذیرش/نوبت ۱۰۰٪ بومی (`src/reception.cpp`)
- تب «پذیرش» و تب «نوبت» دیگر `WebHost_Create` را صدا نمی‌زنند؛ همیشه فرم
  بومی C++ (GDI+/Win32، دابل‌بافر) رندر می‌شود. `include` مربوط به
  `webhost.h` حذف شد.
- ثبت و چاپ قبض (`ID_F_SUBMIT`) به‌صورت کامل بومی و کاربردی: اعتبارسنجی
  فیلدهای لازم، `saveReception`, `rememberPatient`, طراحی/چاپ قبض و باز کردن
  کشوی پول.

### Changed — راه‌اندازی سبک‌تر (`src/setup_splash.cpp`, `src/main.cpp`)
- تمام ارجاع‌های `WebHost_*` و کلید رجیستری `FEATURE_BROWSER_EMULATION`
  حذف شدند. مرحلهٔ آماده‌سازی اکنون فقط فونت فارسی + پوشه‌های `data/`,
  `logs/` را آماده می‌کند و پیام «آماده‌سازی رابط کاربری بومی (C++)…» نشان
  می‌دهد.

### Fixed — رفع کرش طراح چاپ `STATUS_LONGJUMP` (`src/print_designer_ui.inc`)
- مکانیزم «containment» مبتنی بر `setjmp/longjmp` داخل VEH حذف شد. روی
  زنجیرهٔ MinGW (DWARF/SEH) فراخوانی `longjmp()` از داخل VEH باعث برخاستن
  استثنای دوم در `RtlUnwind` و کرش `0x80000026` می‌شد. اکنون حلقهٔ مودال
  ساده‌ی Win32 بدون پرش غیرمحلی استفاده می‌شود؛ نقص‌های واقعی از پیش با
  null-check و فونت/طرح ایمن مهار شده‌اند.

### Fixed — فهرست بخش‌ها فقط بخش‌های واقعی (`src/sections.cpp`)
- دانهٔ اولیه از ۹ بخش نمایشی به **۱ بخش واقعی** («پذیرش») کاهش یافت تا در
  «تنظیمات → طراحی چاپگر» فقط بخش‌های تعریف‌شده دیده شوند. مهاجرت خودکار
  برای داده‌های قدیمیِ دست‌نخورده (کلید `sections_demo_migrated`).

### Fixed — اسکرول روان (`src/reception.cpp`)
- در `WM_VSCROLL` و `WM_MOUSEWHEEL` پس از `tabPageLayout` + `InvalidateRect`
  فراخوانی `UpdateWindow(h)` افزوده شد تا رسم در همان فریم تخلیه شود و
  هم‌پوشانی/پارگی هنگام اسکرول از بین برود (فرم دارای `WS_CLIPCHILDREN` و
  رسم دابل‌بافر است).

### Changed — نسخه‌بندی
- `src/app.h`: `APP_VERSION_W = L"1.17.0"`.
- `src/app.rc`: `FILEVERSION/PRODUCTVERSION = 1,17,0,0` و رشته‌های نسخه.
- `update/version.txt`: `1.17.0`.

### Build
- خروجی: `build/AzadiTeb.exe` (PE32/i386، استاتیک، ~۲٫۹MB) جایگزین نسخهٔ
  قبلی شد؛ `AzadiTeb.exe.sha256` به‌روزرسانی شد.

---

## 1.16.1 — 2026-06-25

> **تطبیق دقیق ظاهر صفحهٔ پذیرش با طرح مرجع + نوار راه‌اندازی پیش‌نیازها برای هر کلاینت.**
> هدف: روی هر دستگاه کاربر، ظاهر دقیقاً مثل طرح باشد و ارتباط C++↔HTML
> همیشه کار کند تا قبض بدون خطا ثبت و چاپ شود.

### Added — نوار راه‌اندازی (`src/setup_splash.cpp` + `RunSetupSplash`)
- یک پنجرهٔ راه‌اندازی برند‌دار با **نوار پیشرفت** هنگام نخستین اجرا (و پس از هر
  ارتقای نسخه) که پیش‌نیازهای دستگاه کاربر را آماده می‌کند:
  1. ساخت/بررسی پوشه‌های `data\` و `logs\` (علت اصلی «خطا در ثبت» = پوشهٔ
     غیرقابل‌نوشتن).
  2. نصب فونت فارسی Vazirmatn به‌صورت per-user.
  3. ثبت EXE زیر **FEATURE_BROWSER_EMULATION = 11001** تا موتور Trident در حالت
     استاندارد IE11 اجرا شود (دلیل اصلی کارکردِ درست CSS/JS مدرن و پل
     C++↔HTML روی کلاینت — پیش‌تر حالت IE7-quirks باعث می‌شد «تغییرات اعمال
     نشده» و «ارتباط مشکل دارد» دیده شود).
  4. بررسی زندهٔ در دسترس بودن MSHTML و انتخاب قطعی مسیر (هیبرید یا fallback).
- اجراهای بعدی فوری و بدون پنجره‌اند (کلید `setup_done_version`).

### Changed — تطبیق پیکسلی ظاهر پذیرش (`src/webhost_assets.inc`)
- شبکهٔ ستونی دقیق برای ردیف‌های فرم: `.row.cols3`/`.cols4`/`.cols2` تا عرض و
  چینش تکست‌باکس‌ها **دقیقاً مثل عکس مرجع** باشد (سه‌ستونه برای مشخصات بیمار،
  چهارستونه برای «بیمه و نوبت»، دوستونه برای مبلغ/تخفیف).
- بازچینش کارت «بیمه» در ستون راست مطابق مرجع: ردیف۱ «بیمه پایه/شعبه طرف
  قرارداد»، ردیف۲ «بیمه مکمل/تاریخ اعتبار» (فیلدهای «نوع بیمه پایه» و «درصد
  پوشش» به‌صورت mirror پنهان نگه داشته شدند تا JS موجود نشکند).

### Files
- `src/setup_splash.cpp` (جدید)، `src/main.cpp` (فراخوانی `RunSetupSplash`)،
  `src/webhost_assets.inc`، `build.sh` (افزودن منبع جدید)، `src/app.h`،
  `src/app.rc`، `update/version.txt` → نسخه **1.16.1**؛ بازبیلد
  `build/AzadiTeb.exe` + `build/AzadiTeb.exe.sha256`.

---

## 1.16.0 — 2026-06-25

> **بازطراحی کامل صفحهٔ پذیرش بیمار + جدول خدمات + لاگ‌گیری خطا + اصلاح بیمه‌ها.**
> این نسخه علاوه بر ظاهر، **بک‌اند** را هم اصلاح می‌کند تا قبض واقعاً بدون خطا ثبت شود.

### Added (UI — `src/webhost_assets.inc`)
- **جدول خدمات (Service Table)** با امکانات کامل:
  - دکمهٔ «افزودن خدمت» + جعبهٔ جستجوی خدمت با popup انتخاب از کاتالوگ.
  - ستون‌ها: ردیف / کد خدمت / نام خدمت / مبلغ / تخفیف / سهم بیمه / عملیات (حذف).
  - جمع خودکار مبالغ خدمات (`svcSum`) که مستقیماً فیلد «مبلغ» و صورتحساب را تغذیه می‌کند.
  - حالت خالی (empty state) با آیکون «box».
- آیکون‌های SVG جدید: `box`، `trash`.
- چهار select بیمه (بیمه پایه / نوع بیمه پایه / بیمه مکمل / نوع بیمه) همگی
  از کاتالوگ C++ پر می‌شوند.

### Changed (UI — `src/webhost_assets.inc`)
- بازنویسی `wh_receptionScript()`: افزودن `SVC_CATALOG`/`SVC_ROWS`،
  توابع `fillServices`/`renderSvc`/`addSvc`/`delSvc`/`svcInsPct`/popup خدمت،
  آینه‌سازی بیمهٔ پایه (baseType↔ins، insType↔supp)، محاسبهٔ مجدد بر اساس جمع خدمات،
  و `collect()` که نام خدمات را هم ارسال می‌کند.
- **پیل خطا قابل بستن شد:** کلیک روی دکمهٔ «خطا در ثبت» فقط پیل را پنهان می‌کند
  (به‌جای ارسال مجدد فرم).
- دکمهٔ «انصراف» اضافه شد (پاک‌سازی فرم).

### Fixed (Backend — `src/webhost_bridge.inc`)
- **لاگ‌گیری دقیق خطا:** برای هر مسیر خطا یک `WebHost_LogError` ثبت می‌شود
  (اعتبارسنجی خالی بودن نام، استثناء هنگام `saveReception`، شکست نوشتن CSV،
  شکست چاپ) → فایل `logs\webhost_errors.log` دلیل واقعی خطا را نشان می‌دهد.
- افزودن `WhService` + کاتالوگ `WH_SERVICES[]` (۱۵ خدمت) + `wh_servicesJson()`؛
  افزودن `services` به payload راه‌اندازی و یک verb مستقل `services`.
- بازنویسی بلوک ذخیره (`saveReception`/`saveAndPrint`) با try/catch و پیام
  خطای فارسی قابل‌فهم به‌جای `{}` خام.

### Insurance (verified)
- لیست بیمهٔ پایه (۷): آزاد / تأمین اجتماعی / سلامت ایرانیان / سلامت روستایی /
  سلامت کارکنان دولت / نیروهای مسلح / کمیتهٔ امداد.
- لیست بیمهٔ تکمیلی (۱۰): ندارد + بیمهٔ ایران/آسیا/دانا/البرز/پاسارگاد/ملت/کوثر/دی/SOS.

### Files
- `src/webhost_assets.inc`، `src/webhost_bridge.inc`، `src/app.h`، `src/app.rc`،
  `update/version.txt` → نسخه **1.16.0**؛ بازبیلد `build/AzadiTeb.exe` +
  `build/AzadiTeb.exe.sha256`.

---

## 1.15.1 — 2026-06-25

> **بازطراحی ظاهری صفحهٔ پذیرش مطابق طرح مرجع (ChatGPT mock) — پیکسل‌به‌پیکسل.**
> کل منطق و رفتار حفظ شد؛ فقط لایهٔ نمایش بازچینش شد تا دقیقاً مطابق طرح مرجع شود.

### Changed (UI — `src/webhost_assets.inc`)
- پالت روشن‌تر و تخت‌تر: `--bg #f5f7fa`، کارت سفید، آبی اصلی `#2b6df4`، سبز
  `#22c55e`، قرمز `#ef4444`، بوردر `#e6ebf2`؛ حذف گرادیان‌های سنگین پس‌زمینه.
- هر ورودی اکنون **آیکون داخلی** (سمت راست) دارد (کلاس `.ic-in`) + placeholderهای
  فارسی («... را وارد کنید») و **ستارهٔ قرمز** روی فیلدهای اجباری (نام/نام
  خانوادگی/کد ملی/تاریخ تولد).
- کارت بیمار: آواتار آبی‌کم‌رنگ با آیکون کاربر، چیپ سبز «نسخه الکترونیک»، و دو
  باکس «نسخه (سرپایی)» و «نسخه (الکترونیکی)».
- بخش «کلیدهای جستجو» با دکمه‌های متنی «استعلام»؛ بخش «بیمه» با ردیف‌های
  بیمه پایه/نوع بیمه پایه/بیمه مکمل/شعبه طرف قرارداد/تاریخ اعتبار/درصد پوشش؛
  بخش «پزشک معالج» با کد نظام و نام پزشک.
- مرکز: «مشخصات بیمار» + «بیمه و نوبت» (نوع بیمه پایه/نوع بیمه/نوع نوبت/نوع
  پذیرش) + ردیف مبلغ خدمت/تخفیف.
- چپ: کارت «صورتحساب» با ردیف‌های بیمه اصلی/سهم بیمه/جمع کل/بیمه مکمل/مابه‌التفاوت
  بیمه پایه/سهم سازمان/مبلغ نهایی/تخفیف و ردیف **آبی «پرداختی»**؛ واحد «تومان».
- نوار پایین: «ثبت پذیرش و صدور قبض» (آبی پررنگ) · «پذیرش جدید» (آبی روشن) ·
  «پاک کردن» (سفید با آیکون refresh) + پیل قرمز «خطا در ثبت» که فقط هنگام خطا
  ظاهر می‌شود.
- آیکون‌های SVG جدید: `check/refresh/calendar/phone/mobile/map/id/gender`.

### Files
- `src/webhost_assets.inc` (بازچینش کامل ظاهر + JS سازگار)، `src/app.h`،
  `src/app.rc`، `update/version.txt` → نسخه **1.15.1**؛ بازبیلد
  `build/AzadiTeb.exe` + `build/AzadiTeb.exe.sha256`.

---

## 1.15.0 — 2026-06-25

> **بازطراحی کامل صفحهٔ پذیرش بیمار (UI/UX مدرن ۲۰۲۶) + رفع قطعی باگ «چاپ →
> خطای ثبت» + همگام‌سازی کامل HTML↔C++.** صفحهٔ پذیرش به‌طور کامل بازطراحی شد
> (تم سفید/آبی پزشکی، کارت‌محور، آیکون‌های وکتوری SVG آبی‌خط، چیدمان تک‌صفحه‌ای
> بدون اسکرول) و باگ اصلی که هنگام «ثبت پذیرش و چاپ» خطای ثبت نشان می‌داد به‌طور
> ریشه‌ای برطرف شد.

### Fixed (باگ «چاپ → خطای ثبت» — §6)
- **علت ریشه‌ای:** دکمهٔ اصلی «ثبت پذیرش و صدور قبض» در واقع فقط `saveReception`
  را صدا می‌زد و چاپ نمی‌کرد؛ چاپ یک فراخوانی COM **جداگانه** بود که رسید را با
  `loadLastReceipt` از روی فایل می‌خواند. اگر این فراخوانی قبل از تثبیت رکورد یا
  روی `last_receipt.dat` کهنه/ناقص اجرا می‌شد، خطای ثبت کاذب نمایش داده می‌شد.
  افزون بر این، `ReceptionRecord::finalTotal` پیش از سریال‌سازی رسید **هرگز
  مقداردهی نمی‌شد** و «مبلغ نهایی» چاپی صفر/آشغال می‌شد.
- **اصلاح** (`src/webhost_bridge.inc`):
  - فعل جدید و **اتمیک** `saveAndPrint`: ابتدا ثبت می‌کند و **فقط در صورت موفقیت
    قطعی ثبت** (`q>0`) رسیدِ همان رکورد درون‌حافظه‌ای را چاپ می‌کند — بدون
    بازخوانی فایل و بدون رقابت دو مرحله‌ای. خطای چاپ هرگز نتیجهٔ ثبت را باطل
    نمی‌کند و در پاسخ JSON با کلید `printed` گزارش می‌شود.
  - `r.finalTotal = r.paid` پیش از `saveLastReceipt` ست می‌شود.
  - `print`/`printLast` حالا وضعیت واقعی چاپ (`ok=printed`) و در نبود قبض پیام
    «هنوز قبضی ثبت نشده است» را برمی‌گردانند (به‌جای `ok=true` همیشگی).
- **اصلاح** (`src/webhost_assets.inc`): دکمهٔ اصلی به «ثبت پذیرش و چاپ رسید»
  تغییر نام یافت و `saveAndPrint` را صدا می‌زند؛ پیام موفقیت شمارهٔ نوبت و وضعیت
  چاپ رسید را نشان می‌دهد. تابع مشترک `collect()` بدنهٔ فرم را برای ثبت و چاپ
  یک‌جا جمع می‌کند.

### Changed (بازطراحی UI/UX پذیرش — §3)
- **پالت رنگ ۲۰۲۶** (`src/webhost_assets.inc`): پس‌زمینه `#F5F8FD`، کارت‌های
  سفید، آبی اصلی `#2E7DFF`، آبی ثانویه `#4DA3FF`، آبی تأکید `#1E5EFF`، سبز موفق
  `#18B75A`، قرمز `#E53935`، مرز `#DCE7F5`، متن سرمه‌ای؛ سایه‌های نرم، گوشه‌های
  گردتر، حلقهٔ فوکوس آبی.
- **آیکون‌های وکتوری SVG** (تابع جدید `wh_svg`): مجموعهٔ آیکون آبی‌خط
  (search/user/shield/stethoscope/clipboard/layers/receipt/print/plus/eraser/x)
  جایگزین آیکون‌های یونیکد قدیمی شد — بدون فایل تصویری، با `currentColor`.
- **چیدمان سه‌زونه‌ٔ بازطراحی‌شده**:
  - **زون راست:** کارت بیمار (آواتار + ویجت‌های وضعیت: نسخهٔ الکترونیک، وضعیت
    بیمار، وضعیت بیمه، شمارهٔ پرونده + شمارندهٔ تعداد ویزیت/قبض/سرپایی) · کارت
    جستجو با دکمه‌های آیکونی (شمارهٔ پرونده/شمارهٔ بیمه/کد ملی) · کارت بیمه
    (بیمهٔ اصلی، بیمهٔ مکمل، شمارهٔ دفترچه، شعبه، تاریخ اعتبار، درصد پوشش) · کارت
    پزشک (کد نظام، نام پزشک، پزشک ارجاع‌دهنده).
  - **زون مرکز:** مشخصات بیمار با ترتیب دقیق ردیف‌ها (نام/نام‌خانوادگی/کد ملی ـ
    نام‌پدر/تاریخ‌تولد/جنسیت ـ موبایل/تلفن/آدرسِ عریض)، بخش اطلاعات درمانی و نوبت،
    بخش اطلاعات خدمت، و نوار اکشن پایین.
  - **زون چپ:** خلاصهٔ پرداخت (جمع کل، سهم بیمه، مابه‌التفاوت، سهم سازمان، سهم
    بیمار، تخفیف، مبلغ نهایی سبز) + کارت چاپ (رسید بیمه/نسخه/آخرین قبض/چاپ مجدد).
- **اندازهٔ فیلدها** مطابق طول واقعی محتوا تنظیم شد (کد ملی ثابت، تاریخ تولد ثابت،
  جنسیت فشرده، آدرس تمام‌عرض، نام‌پدر عرض متوسط).
- **نوار اکشن پایین** کامل شد: «ثبت پذیرش و چاپ رسید» (آبی) · «پذیرش جدید»
  (outline) · «پاک کردن» · «انصراف» (قرمز) — با آیکون و انیمیشن hover/press.
- **همگام‌سازی** (`recalc`): سهم بیمار (`b_pat`) و درصد پوشش بیمه (`insCov`) زندهٔ
  محاسبه و در پنل راست نمایش داده می‌شوند؛ ویجت‌های وضعیت با نتیجهٔ استعلام بیمار
  به‌روز می‌شوند. جستجوی کد ملی از کارت راست (`nidSearch`/`q_nid`) به فرم تزریق
  می‌شود.

### Files
- `src/webhost_assets.inc` — افزودن `wh_svg`، پالت/CSS جدید، بازنویسی کامل
  `wh_receptionHtml`، به‌روزرسانی `wh_receptionScript` (collect/azSubmit/recalc/
  setPatient/lookup/wire/clearForm + دکمه‌های جدید).
- `src/webhost_bridge.inc` — فعل اتمیک `saveAndPrint`، مقداردهی `finalTotal`،
  بازگشت وضعیت واقعی چاپ در `print`/`printLast`.
- `src/app.h`, `src/app.rc`, `update/version.txt` — بمپ نسخه به 1.15.0.

---

## 1.14.4 — 2026-06-24

> **Print-designer crash containment for the modal pump + reception save/insurance
> hardening.** Closes the remaining crash window in «دیزاین پرینتر» (the
> `0xC0000005` ACCESS_VIOLATION at `AzadiTeb.exe+0x12649`, last breadcrumb
> *"print designer: open editor"*, garbage `EBP`) and finishes the patient
> reception update (save error, empty insurance lists, ptype tariff parity).

### Fixed (print designer crash — modal-pump containment, §4)
- **Root cause:** release 1.14.3 *disarmed* the open-flow VEH (`s_pdArmed=false`)
  around the designer's modal message pump on the theory that `longjmp` out of a
  nested `DispatchMessage` was unsafe. That left the pump body — `designer_paint`
  / `WM_COMMAND` inspector edits / `designer_buildInspector` — with **no fault
  containment at all**, so any GDI+ paint fault became process-fatal. The crash
  log (garbage `EBP`) is the classic signature of a fault dispatched with no
  armed handler.
- **Correction:** `longjmp` does **not** run C++ destructors or unwind frames —
  it only restores SP/registers — so the "unwinding past GDI+ destructors" worry
  was unfounded. This toolchain is DWARF-EH MinGW with **no `__try`/`__except`**,
  so a vectored handler + `setjmp`/`longjmp` is the only available containment.
- **Fix** (`src/print_designer_ui.inc`) — added a dedicated, thread-local pump
  landing pad (`s_pdPumpJmp` / `s_pdPumpArmed`). `OpenDesignerWindow` installs a
  per-pump `setjmp` recovery point and arms `s_pdPumpArmed` for the whole modal
  loop; `pdOpenVeh` now recovers to the **innermost** armed pad (pump first, then
  open-flow). On a contained pump fault we tear down the inspector + editor
  window, still run `delete st`, re-enable the owner, and show a friendly notice —
  the designer remains re-openable and the app never dies.
- **`makeSafeFont` hardened further** — clamps the pixel size (`1 … 4000`),
  guards `GenericSansSerif()` against a null/error family on a GDI+ that failed
  to start, adds an `Arial` last-resort family, and returns `nullptr` (all call
  sites already null-check) instead of ever building a `Font` from a bad family.

### Fixed (patient reception)
- **Save error «خطا در ثبت»** (`src/webhost_host.inc`, `src/webhost_bridge.inc`) —
  `WebHostBridge_Call` is wrapped in `try/catch(...)` inside `BridgeDispatch::Invoke`
  so a COM/STL exception can never escape as an empty `{}` (which the JS surfaced
  as the generic save error); a proper JSON error is always returned.
- **Empty insurance lists** (`src/webhost_bridge.inc`, `src/webhost_assets.inc`) —
  added `boot` / `insurances` / `supp` bridge verbs; `fillInsurance()` pulls the
  lists directly from C++ when the boot push is empty, and `selfHydrate()` polls
  at 350 ms / 900 ms so the «بیمه پایه» / «بیمه مکمل» selects are never blank.
- **Tariff parity on save** (`src/webhost_bridge.inc`, `src/webhost_assets.inc`) —
  `saveReception` now falls back to `defaultServicePrice(ptype, ntype)` using the
  real selected indices (was hardcoded `0,0`), matching the live bill preview;
  `azSubmit` sends the `ptype`/`ntype` indices and the correct option *text*.

### Reception UI redesign (carried from the in-progress 1.14.3 work)
- Modern blue/white design system, bigger Vazir fonts, content-sized field
  classes, correct field order (نام→نام خانوادگی→کد ملی→جنسیت→نام پدر→تاریخ
  تولد→موبایل+تلفن ثابت→آدرس), «ثبت پذیرش و صدور قبض» moved to the visual left
  (`flex-direction:row-reverse`), «پاک کردن» removed and «پذیرش جدید» now resets
  with a `(Ctrl+R)` hint, smart Jalali birth-date mask (auto slashes, Ctrl+A,
  correct backspace), live Rial thousands grouping with live billing recompute,
  and responsive breakpoints. (`src/webhost_assets.inc`)

### Files
- `src/print_designer_ui.inc` — pump VEH landing pad + `makeSafeFont` hardening.
- `src/webhost_assets.inc` — reception HTML/CSS/JS redesign, masks, insurance
  fallback, ptype/ntype submit.
- `src/webhost_bridge.inc` — `boot`/`insurances`/`supp` verbs, save tariff parity.
- `src/webhost_host.inc` — `try/catch` around the bridge call in `Invoke`.
- `src/app.h`, `src/app.rc` — version bump to **1.14.4**.

---

## 1.14.3 — 2026-06-24

> **Print-designer crash root-cause fix + reception UI redesign.** Eliminates
> the recurring print-designer crash (reported `0x80000026`) at its true
> source — a GDI+ font-construction fault during canvas paint on machines that
> do not have the requested font installed — and completely redesigns the
> reception screen into a modern, professional three-zone layout.

### Fixed (print designer crash — root cause, §4)
- **Crash-proof GDI+ font construction** (`src/print_designer_ui.inc`) — new
  `makeSafeFont(reqName, pxSize, gdiStyle)` helper builds a font through a
  deterministic fallback chain (requested family → bundled `Vazirmatn` →
  `GenericSansSerif`), validating `GetLastStatus()`/`IsAvailable()` at every
  step. The two previously-unguarded `FontFamily`/`Font` construction sites in
  the canvas painter (LOGO/IMAGE/PHOTO placeholder glyph and the
  APPTNO/LABEL/FIELD text path) now route through `makeSafeFont`, so a missing
  font name can no longer throw inside GDI+ and trip the contained VEH fault.
- **Safe VEH around the modal designer pump** (`src/print_designer_ui.inc`) —
  `0x80000026` was the VEH's own `longjmp` status (`STATUS_LONGJMP`).
  `longjmp`-ing out of a nested Win32 modal message pump unwinds past GDI+
  destructors and `delete st`, corrupting state. `OpenDesignerWindow` now
  disarms the handler (`s_pdArmed = false`) for the duration of its modal
  `MSG` pump and re-arms it afterward, so a paint fault during
  `DispatchMessage` is handled normally instead of being `longjmp`'d out of
  deep Win32 dispatch. `s_pdArmed` was moved to file scope to make this
  arm/disarm coordination explicit.

### Changed (reception UI redesign)
- **Modern design system** (`src/webhost_assets.inc`, `wh_commonHead()`) —
  rebuilt the CSS with a medical-blue palette, elevated cards, accent bars,
  icon chips, gradient primary/success buttons, a custom select caret, a
  toast, and responsive breakpoints (1180px / 980px) plus a `compact` mode.
- **Three-zone reception layout** (`src/webhost_assets.inc`,
  `wh_receptionHtml()`) — removed the in-page "پذیرش بیمار" title bar and the
  "خلاصه"/"عملیات" summary panel; lifted the form fields up so the screen
  needs no scrolling. Right zone: patient avatar + counts, search keys with
  استعلام buttons, insurance + supplementary insurance, treating doctor.
  Center zone: مشخصات بیمار + بیمه و نوبت + save/new/reset actions. Left zone:
  صدور قبض billing breakdown + print buttons.
- **Reworked reception script** (`src/webhost_assets.inc`,
  `wh_receptionScript()`) — new field IDs, `v()`/`on()` helpers, header
  binding, `recalc()`/`lookup()`/`setPatient()`/`clearForm()`/`azSubmit()`/
  `doPrint()` wiring, and a Ctrl+R clear shortcut. The appointment top bar was
  updated to the new `.crumb` breadcrumb and the IE11 CSS-var ponyfill token
  defaults were refreshed (added `shadowSm`).

### Files
- `src/print_designer_ui.inc`, `src/webhost_assets.inc`,
  `src/app.h`, `src/app.rc`, `update/version.txt`,
  `build/AzadiTeb.exe`, `build/AzadiTeb.exe.sha256`, `docs/CHANGELOG.md`.

---

## 1.14.2 — 2026-06-24

> **Production stabilization pass.** Hardens the print-designer open path
> against dangling-pointer / stale-state faults on repeated open / close /
> reopen cycles, and finalizes the release (version bump + rebuilt EXE).

### Fixed (print designer crash-hardening, §4)
- **Instance-safe inspector handle table** (`src/print_designer_ui.inc`) —
  `OpenDesignerWindow` now calls `clearInspector()` before allocating a new
  `DesignerState`, so a process-global inspector handle table left over from a
  previous (possibly abnormally-closed) designer instance can never be reused
  with stale `HWND`s.
- **Post-destroy state detach** (`src/print_designer_ui.inc`) — `WM_DESTROY`
  in `PickerProc`, `DesignerProc` and `RestoreProc` now clears
  `GWLP_USERDATA`, so any late `WM_COMMAND` / drop / notify message that
  arrives after the window is gone reads a null state pointer instead of
  dereferencing freed memory.
- **Section-picker id validation** (`src/print_designer_ui.inc`) —
  `picker_syncSel` only records a selection when `ListView_GetItem` succeeds
  **and** the item `lParam` (section id) is `> 0`, rejecting the
  zero/garbage ids that previously fed the editor and triggered the contained
  VEH fault.
- **Restore-dialog null-safety** (`src/print_designer_ui.inc`) —
  `RestoreProc` guards `WM_COMMAND` and the drop-zone load on a non-null
  state pointer (and deletes the dropped-path payload unconditionally),
  preventing a crash when the dialog is interacted with mid-teardown.

### Changed
- **Design sanitation** (`src/print_designer_ui.inc`) — on open, an empty
  paper size defaults to `A5`, an out-of-range orientation is forced to `0`,
  and degenerate item geometry (`w<=0` / `h<=0`) is clamped to `1`, so a
  corrupt or partially-written design file can no longer drive the editor
  into an invalid layout state.
- **Version bump to 1.14.2** (`src/app.h`, `src/app.rc`,
  `update/version.txt`) — `APP_VERSION_W`, `FILEVERSION` / `PRODUCTVERSION`
  and the update channel manifest all advanced to `1.14.2`.

### Validated
- Clean cross-build with `./build.sh` under
  `-Wall -Wextra -Werror` (i686-w64-mingw32, static PE32). The rebuilt
  `build/AzadiTeb.exe` carries the `1.14.2` version resource and its
  `build/AzadiTeb.exe.sha256` sidecar was regenerated to match.

---

## 1.14.1 — 2026-06-24

> **Critical fix for the hybrid Reception/Appointment surface.** The screens
> showed a *Script Error — "Object doesn't support this property or method"* and
> rendered unstyled because a hosted WebBrowser control defaults to **IE7 quirks
> mode** (which lacks modern JS like `JSON`/`querySelectorAll` and ignores
> `<meta X-UA-Compatible>`), and because **IE11 has no support for CSS custom
> properties (`var()`)** — so every themed colour resolved to nothing. Both are
> now fixed and the surface renders fully styled and interactive.

### Fixed
- **Trident standards mode** (`src/webhost.cpp`, `src/webhost.h`,
  `src/main.cpp`, `src/webhost_run.inc`) — register this EXE under
  `FEATURE_BROWSER_EMULATION = 11001` (IE11 edge/standards) in **HKCU** (per-user,
  no admin) at startup and again defensively before the control is created
  (idempotent, failure-tolerant). The hosted WebBrowser now runs modern CSS/JS
  instead of IE7 quirks — eliminating the *“Object doesn’t support this property
  or method”* script error.
- **CSS custom-property ponyfill** (`src/webhost_assets.inc`) — because IE11
  itself does not implement `var()`, an ES5 ponyfill detects the lack of native
  `CSS.supports('--a','0')`, collects the page stylesheet, resolves every
  `var(--token[, fallback])` to a concrete value from the theme token map, and
  writes the result into a managed `<style id='az-theme'>`. `applyTheme()` now
  updates the token map and re-resolves on every theme push (and uses native
  custom properties on modern engines). Result: the three-zone layout, cards,
  buttons, tiles and billing block render fully styled on IE11 *and* modern
  engines.
- **JSON polyfill guard** (`src/webhost_assets.inc`) — a compact ES3-safe
  `JSON.stringify`/`JSON.parse` is installed only if the engine lacks a native
  `JSON`, so the synchronous bridge can never die on its first call in a degraded
  Trident.

### Validated
- Generated Reception & Appointment documents rendered headless in Chromium
  (modern path) and in a forced **no-`var()`** configuration (IE11 ponyfill
  path): **zero JavaScript errors**, three zones present, theme tokens resolved,
  bridge calls (`apptNext`/`bill`/`lookup`) round-trip cleanly.

---

## 1.14.0 — 2026-06-24

> **Incremental production refinement** of the 1.13.0 hybrid surface — no
> rewrite, no regressions. The Reception/Appointment HTML surface is completed
> into a polished, responsive, **three-zone** workspace; the native→HTML sync is
> hardened with a deterministic loader watchdog and idempotent repeated opens;
> the Settings panel is rebalanced (close button moved to the top-**right**,
> profile circle slightly smaller and lower); the Print Designer open/close
> lifecycle is crash-hardened; and the section identity model gains stable
> category codes plus serialisable network metadata. Single static PE32 i386
> EXE, zero warnings (`-Wall -Wextra -Werror`), Win7→Win11 x86/x64.

### Added
- **§7 Stable section identity** (`src/sections.h`, `src/sections.cpp`) — every
  section now resolves to a stable 3-letter **category code** (REC reception ·
  APR appointment · LAB laboratory · INJ injection · PHR pharmacy · BIL billing ·
  RAD radiology · PHY physiotherapy · GEN generic) via
  `Sections_CategoryCode()`, plus a durable `Sections_CodePrefix()` derived from
  the section code. Routing keys off these stable identifiers, never display
  names. Seeded appointment / pharmacy / billing sections (APR01/PHR01/BIL01).
- **§8 Serialisable network metadata** — the `Section` record gains an optional
  `net_meta` field, persisted as a backward-compatible 8th column (older files
  with 7 columns still load). Keeps the section model network-ready while local
  mode stays primary.
- **§4 Loader watchdog** (`src/webhost_host.inc`, `src/webhost_run.inc`) — a
  deterministic deadline (≈6 s hard cap, ≈1.5 s after `READYSTATE_COMPLETE`)
  force-reveals the document if `DocumentComplete` is slow or lost, so the
  centred loader can never stick. Forced reveals are logged.

### Changed
- **§2 Reception/Appointment HTML/CSS/JS completed** (`src/webhost_assets.inc`)
  — the bare shell becomes a complete design system: `:root` theme tokens, a top
  bar with brand, a tab strip, and a responsive **three-zone grid** (left
  summary panel · centre form · right patient/search/insurance panel) with
  cards, tiles, billing block, patient card, field/button states (focus, hover,
  disabled), a compact/dense mode and breakpoints at 1180/1000/820 px. Primary
  fields sit high — no scrolling for normal desktop tasks. JS adds tab switching,
  **Enter→next field**, **Tab/Shift+Tab**, keyboard-safe dropdowns, print/save/
  reset/open/search handlers, validation hooks and state hydration from the C++
  authority via `window.azReceive`. The boot payload now carries each section's
  stable `cat`/`prefix` keys.
- **§3/§4 Native→HTML sync hardened** — reveal logic is centralised in one
  idempotent `WebHost_RevealDocument()`; repeated `DocumentComplete` (tab
  re-entry / internal navigation) re-pushes a refresh rather than re-running
  boot; the boot push tolerates an empty payload (logged, still reveals).
- **§5 Settings panel refined** (`src/settings.cpp`) — the close (×) button
  moves from the top-left to the top-**right** corner (single `closeBtnRect()`
  helper keeps paint/hover/hit-test in lock-step); the profile circle is
  slightly **smaller** and sits slightly **lower**; the header is re-tuned so
  the name/role lines stay clear of the first row. The panel covers the full
  client area (scrim + opaque card, no bleed-through).

### Fixed
- **§6 Print Designer crash hardening** (`src/print_designer_ui.inc`) — the
  open/close/reopen path is now idempotent (`s_pdOpening` guard), guards a NULL
  editor-window creation, and the VEH containment landing pad catches a wider
  set of faults (access violation, in-page error, misalignment, bounds, stack
  overflow, illegal instruction, divide-by-zero), purges any orphaned designer
  popups, re-enables the owner and shows a graceful warning instead of taking
  the app down. Breadcrumb sequence and structured logs to
  `logs\print_designer.log` are preserved.

### Logging
- **§9** All failure categories persist with timestamps and **no live UI
  chatter**: hybrid host errors/watchdog/reveal/bridge → `logs\webhost_errors.log`
  (duplicate-throttled); print designer init/binding/section-picker/editor
  faults → `logs\print_designer.log`.

---

## 1.13.0 — 2026-06-24

> **Major production upgrade.** Reception & Appointment become a **hybrid
> HTML/CSS/JS surface hosted inside the app** via the system
> MSHTML/WebBrowser (Trident) OLE control — no shipped DLLs, no internet, works
> Win7→Win11 x86/x64 in the single static 32-bit EXE. C++ remains the host,
> validator, lifecycle owner, persistence layer and bridge; JS owns only layout
> and interaction. Settings header is rebalanced and gains a pinned close
> button. Single static PE32 EXE, zero warnings (`-Wall -Wextra -Werror`), no
> regressions, access levels + existing data preserved.

### Added
- **§3 Hybrid HTML reception/appointment host** (`src/webhost.h`,
  `src/webhost.cpp`, `src/webhost_host.inc`, `src/webhost_run.inc`,
  `src/webhost_bridge.inc`, `src/webhost_assets.inc`) — a hand-rolled (no ATL)
  OLE host for the system WebBrowser control: `IOleClientSite`,
  `IOleInPlaceSite/Frame`, `IDocHostUIHandler`, a `DWebBrowserEvents2` sink and
  a bridge `IDispatch` exposed as `window.external`. Opening Reception / New
  reception / Appointment shows a **centred, minimal loader with a determinate
  progress bar** while native state, section metadata and form config
  synchronise, then renders the HTML UI. The bridge is synchronous
  (`window.external.call(verb, jsonArgs)` → JSON) and C++ → JS pushes go through
  `window.azReceive`. Verbs: `lookup`, `bill`, `saveReception`, `print`,
  `printLast`, `apptList`, `apptNext`, `saveAppointment`, `cancelAppointment`,
  `setTheme`, `theme`, `sections`, `ping`. All validation, tariff math and
  persistence happen **server-side in C++** — JS never fabricates a result.
  Reception UI includes every field/button/date-time input, print + management
  actions, **Enter→next field** and **Tab/Shift+Tab** navigation, with primary
  fields placed high (no scrolling for normal use) and a responsive,
  theme-driven layout.
- **Deterministic native fallback** (`src/reception.cpp`) — if the WebBrowser
  control is unavailable or fails to start, the tab silently falls back to the
  classic native reception/appointment form and logs the cause; the app never
  loses the feature or crashes. No reentrancy / double-open / race on repeated
  open/close.
- **§2 Settings — pinned top-right close button** (`src/user_settings.cpp`) —
  every settings page (home + sub-pages) now carries a pinned ✕ close button in
  the top-right corner with hover/press feedback and a hand cursor.

### Changed
- **§2 Settings profile header** (`src/user_settings.cpp`) — the profile circle
  is now **slightly smaller** (`g_scaleAvatar` S(96)→S(84)) and sits **slightly
  lower** (`avatarTopDrop` = S(22)); the name/role identity block stays anchored
  to the circle so it remains visually connected, and `homeRowsTop()` was
  recomputed to keep clean, consistent spacing below the block.
- **Section stable codes (§4)** continue to back the reception/appointment
  bridge boot payload and the print-designer bindings (REC/INJ/LAB/RAD/PHY …).
- **Persistent logging (§7)** — sync failures, web-host OLE faults and
  bridge errors persist (throttled, de-duplicated) to
  `logs\webhost_errors.log`; UI spam / debug traces are not persisted.

### Build
- `build.sh` — added `src/webhost.cpp` to the source set and `-loleaut32` to
  the link flags (SafeArray / `SysAllocString` / `VariantClear`). Local IIDs are
  defined for `IWebBrowser2`, `IOleObject`, `IConnectionPointContainer`,
  `DWebBrowserEvents2` and `ICustomDoc` so the link succeeds across MinGW
  toolchain versions.

---

## 1.12.0 — 2026-06-24

> Full update / stability / UI-modernization sprint. Hardens the print-designer
> open path against the `0xC0000005` access-violation crash, makes the section
> picker the single source of truth (only real, active sections), turns Settings
> into a separate full-page scrollable surface in both modes, modernizes the
> reception header and the management dashboard, ships a dedup-aware patient
> **import pipeline** that feeds the same store the reception national-ID
> auto-fill reads, and documents the SQL Server `.bak` analysis and the
> network/service-readiness architecture. Single static 32-bit PE32 EXE, zero
> warnings (`-Wall -Wextra -Werror`), no regressions, access levels and existing
> data preserved.

### Fixed
- **§8 Print-designer crash (`ACCESS_VIOLATION 0xC0000005`)**
  (`src/print_designer_ui.inc`, `src/print_designer.cpp/.h`) — root-caused to a
  NULL `st` dereference in `PickerProc` (notifications can fire before
  `SetWindowLongPtr`) and `LVN_ITEMCHANGED` re-entrancy during the picker's
  `ListView_SetCheckState`. Added NULL guards in `picker_reload` /
  `picker_syncSel` / `PickerProc`, a `reloading` re-entrancy flag, a
  blank-A5-design synthesis fallback when no paper is resolved
  (`paperW/paperH<=0`), GDI+ font-family validation (empty → Vazirmatn → generic
  sans), and **VEH + `thread_local setjmp/longjmp` crash containment** around the
  whole open path (`PrintDesigner_Open` → `PrintDesigner_OpenCore`). MinGW GCC
  cannot use `__try/__except` for arbitrary code, so the proven VEH pattern from
  `backup_analyzer.cpp` is reused.
- **§2.E Reception blue section labels covered by text boxes**
  (`src/reception.cpp`) — the caption-vs-input-well clearance is now an enforced,
  strictly-positive invariant (`step >= rh + S(52)`, ≥`S(6)` clear band). Painter
  (`WM_PAINT`) and control positioner (`tabPageLayout`) both consume the same
  `rcVMetrics` geometry, so painted labels and real HWND controls can never drift
  apart.

### Added
- **§11–§13 Patient import pipeline** (`src/data_ext.cpp`, `src/app.h`,
  `src/backup.cpp`) — `parsePatientImportFile()` (UTF-8/16, auto delimiter
  `| , ; TAB`, auto header in English **or** Persian, positional fallback) +
  `importPatients()` with **national-ID dedup** (existing code → update in place,
  new code → insert; invalid/empty codes and nameless rows skipped and counted)
  returning an `ImportResult` reconciliation summary. Reachable from the hidden
  backup-analyzer page via the new «ورود بیماران» button (Path B offline-staged).
- **§5 Management CRM "at-a-glance" activity panel** (`src/manage.inc`) — a
  read-only dashboard surface showing the total patient roster, today's
  appointments, and a workload bar (today vs. busiest recorded day), sourced from
  the existing stores; no data/functionality changed.
- **§10/§15 Architecture docs** — `docs/BACKUP_IMPORT_ARCHITECTURE.md` (MTF
  `.bak` layout, what the analyzer extracts and why table/column names need a
  live restore, Path A/B import, dedup contract, network/service-repository
  readiness) and `docs/UI_ARCHITECTURE_DECISION.md` (native GDI vs hybrid
  HTML/CSS/JS decision and rationale for §3/§4).

### Changed
- **§1/§6 Settings → full-page + scrollable** (`src/user_settings.cpp`) — opens
  as a full work-area `WS_POPUP|WS_VSCROLL` page (no background bleed) with a
  centred content column and full vertical scrolling (mouse wheel + scrollbar +
  `PgUp/PgDn/Home/End`), in **both** reception and management modes. Sub-page
  header is pinned; scroll resets on push/pop.
- **§2.A–C Reception header** (`src/main.cpp`, `src/reception.cpp`) — clock/date
  always centred in the header band on every screen; header height reduced
  (`mainBarH` S(64)→S(56)); slimmer, better-spaced tab strip (`tabBarH`
  S(40)→S(38), explicit `tabGap()=S(8)`, refined top padding and edge margin).
- **§7 Print-designer single source of truth** (`src/print_designer_ui.inc`,
  `src/print_designer.cpp`) — the section picker now shows ONLY real, active,
  defined sections (filters inactive on an empty query); `SectionDesign_Cleanup()`
  reconciles section↔design bindings with the live `Sections` registry and
  archives orphaned `.az_design` files; post-picker section validation.
- **§9 Crash breadcrumbs** — fine-grained `Breadcrumb()` markers along the
  print-designer open path (`PrintDesigner_OpenCore`, picker reload, restore).
- **§17 Version → 1.12.0** (`src/app.h`, `src/app.rc`, `update/version.txt`).

### Notes
- **§14 Reception national-ID Enter auto-fill** and **§16 access levels / data
  preservation** were already correct and are retained unchanged: imported
  patients flow into the same `data\patients.dat` that `lookupCitizen` →
  `lookupLocalPatient` reads, so Enter on a national code instantly recalls an
  imported identity. The `canAccess` permission matrix and all `data\*.dat`
  schemas are untouched.

---

## 1.11.0 — 2026-06-24

> Production hardening & UX-redesign sprint. A fully messenger-style settings
> panel with a circular avatar, tappable card rows and push/pop sub-page
> navigation governed by a single `canAccess(row, mode)` source of truth; a
> standalone Saved-Messages viewer reachable from settings; heartbeat-based
> online presence (<90s) with monospace section/personnel codes; a clock+date
> that centres in the full header and returns to the top-left when the reception
> header collapses; an informational `data\.schema_version` stamp; and a
> rebuilt crash handler that resolves the faulting `module+offset`, dumps the
> last **32** flow breadcrumbs, shows a Persian message and exits cleanly with
> **no auto-restart**. Single static 32-bit PE32 EXE, zero warnings
> (`-Wall -Wextra -Werror`), no regressions.

### Added
- **§A Messenger-style settings (rebuilt)** (`src/user_settings.cpp`) — a
  top-centre circular avatar (photo → initial → silhouette fallback), an
  identity block, and a vertical stack of full-width **tappable card rows**
  with hover / pressed states. Navigation is **push/pop within the same
  window** (`pageStack`) with a back button top-left. Row visibility is driven
  by a SINGLE SOURCE OF TRUTH, `canAccess(SettingsRow, SettingsMode)`, consulted
  by every row handler; a debug-build `selfCheckMatrix()` asserts the
  role/row matrix at startup. Guest contract (`SM_GUEST`): EXACTLY two rows —
  «تغییر پوسته» + «ارتباط با ما».
- **§F Standalone Saved-Messages viewer** (`src/saved_messages.cpp`, new) —
  `SavedMessages_Show(HWND)` renders the permanent local archive
  (`data\saved_msgs.dat`) as scrollable, double-buffered messenger cards with a
  bookmark-icon header, severity chips, click-to-detail and an empty state.
  Opened from the settings card row; the in-cartable «ذخیره در پیام‌ها» save
  button and archive view (reception.cpp) remain the write path.
- **§G Heartbeat presence + monospace codes** (`src/employees.cpp`,
  `src/manage.inc`, `src/main.cpp`) — `online.dat` now stores
  `username|epochSeconds` and `isUserOnline()` only reports online inside a
  90-second window; `heartbeatUser()` is pumped every ~30s from the frame clock
  timer. A new fixed-pitch `g_fCode` font (Consolas → Courier New) renders
  section and personnel codes in an aligned column. Send-message side panel
  keeps online-first grouping by section, the 20×20 cap with a «… و N مورد
  دیگر» overflow line, and clip-based virtualization.
- **§H Centred header clock** (`src/main.cpp`) — when the full header is
  visible the live clock + Jalali date are horizontally centred between the
  left tool buttons and the right identity block; on the reception screen
  (collapsed header, §B) they return to the top-left.
- **§I `data\.schema_version` stamp** (`src/util.cpp`) — `writeSchemaVersion()`
  writes `1.11.0` once at startup. Strictly informational: read by nothing in
  the load path, so it can never gate or migrate data. Prior value is kept as a
  comment line for an audit trail.
- **§J Breadcrumb trail (32)** (`src/handlers.cpp`) — a heap-free 32-slot ring
  buffer (`Breadcrumb(const wchar_t*)`); breadcrumbs are recorded at screen
  switches, settings open, backup manager/analyze and print-designer open.

### Changed
- **§J Crash handler** (`src/handlers.cpp`) — the faulting address is now
  resolved to `module.dll+0xOFFSET` via `EnumProcessModules` /
  `GetModuleInformation`, the last 32 breadcrumbs (newest first, with relative
  ms) are appended to the crash report, and the dialog is now a Persian
  information box that **exits cleanly with no auto-restart** (the previous
  one-click relaunch was removed to avoid crash-loops). Links `-lpsapi`.

### Build
- `APP_VERSION_W` → `L"1.11.0"` (`src/app.h`); `FILEVERSION` /
  `PRODUCTVERSION` → `1,11,0,0` and matching `StringFileInfo`
  (`src/app.rc`); `update/version.txt` line 1 → `1.11.0`. `src/saved_messages.cpp`
  added to `build.sh`.

---

## 1.10.0 — 2026-06-23

> Production-grade redesign & stabilization release. Messenger-style settings
> with a dedicated guest path, an animation-free compact reception layout, a
> premium layered light theme, real SQL-Server `.bak` (MTF) analysis with true
> SEH containment, an integrated employee-messaging side panel, and end-to-end
> forward-compatible data safety. Single static 32-bit PE32 EXE, no new
> warnings, no regressions.

### Added
- **§A Messenger-style settings + dedicated guest path** (`src/user_settings.cpp`)
  — a `SettingsMode { SM_GUEST, SM_RECEPTION, SM_ADMIN }` enum drives a
  future-proof, role-aware settings window. Guests (no login) now see ONLY two
  items: «تغییر پوستر» (change theme) and a new «ارتباط با ما» (Contact us)
  page (`buildContactPanel`, reads `contact.*` settings with Persian
  defaults + a clipboard-copy button). The Contact-us entry is also added to
  the reception and management navs. Window size/title are mode-aware.
- **§D Saved-messages / cartable master toggle** — a visible master switch in
  save-messages settings (`saved_msgs_enabled`), and the reception archive
  toggle icon is now ALWAYS drawn, reachable, and offers to enable the feature
  on click instead of silently doing nothing (`src/reception.cpp`).
- **§E Employee-messaging side panel** (`src/manage.inc`) — the messaging
  window gains an integrated right panel grouped by section, showing section
  codes and per-employee personnel codes, online-first ordering, and lazy
  «… و N مورد دیگر» overflow rows. Capped at 20 sections × 20 employees.
- **§G SQL Server `.bak` (MTF/TAPE) analysis** (`src/backup_analyzer.cpp`) —
  real magic-byte detection of the Microsoft Tape Format; reads ONLY the
  leading descriptor blocks (never loads a multi-GB backup), walks the DBLK
  chain to recover media/backup-set names, vendor, machine and the embedded
  database file list, computes a descriptor fingerprint, and reports the
  honest SQL-Server-restore import path.

### Changed
- **§B Reception layout** (`src/handlers.cpp`, `src/main.cpp`,
  `src/reception.cpp`) — the frame-by-frame header-collapse animation is
  REMOVED entirely; the reception tab now switches to its compact layout
  immediately on entry (`HeaderCollapse` reduced to a discrete state, no
  timer). The clock + calculator moved to the LEFT; scroll no longer drives
  the header, eliminating layer-blend / one-frame-stuck artifacts.
- **§C Premium light theme** (`src/theme.cpp`) — a genuinely layered light
  palette (five distinct elevation tones), crisper borders, higher-contrast
  ink, a richer indigo→sky accent, plus explicit focus ring, disabled veil and
  enable-aware hover/active states in the flat-button control.
- **§E Terminology** — every user-facing «کارکنان» renamed to «کارمندان».
- **§J Versioning** — unified to **1.10.0** across `src/app.h`, `src/app.rc`
  (FILEVERSION/PRODUCTVERSION 1.10.0.0) and `update/version.txt`.

### Fixed
- **§F Print designer would not open** (`src/print_designer_ui.inc`,
  `src/user_settings.cpp`) — removed the `PostQuitMessage(0)` from three nested
  modal `WM_DESTROY` handlers (it injected `WM_QUIT` that killed the next
  modal pump, so the designer window never appeared) and fixed a
  use-after-free where `sw->hMain` was read after `DestroyWindow` freed `sw`.
  All three modal loops hardened with `IsWindow` + `WM_QUIT` re-post.
- **§A Settings routing** (`src/settings.cpp`) — «طراح چاپ» now opens the print
  designer (`PrintDesigner_Open`) instead of the printer-settings page.
- **§G SEH containment** — `azAnalyzeVeh` now REALLY contains hardware faults
  via a thread-local `setjmp`/`longjmp` landing pad (was log-only), surfacing
  an honest Persian error instead of letting an access violation crash the UI.
- **§G stale `GetLastError`** (`src/main.cpp`) — the single-instance check now
  captures `GetLastError()` immediately after `CreateMutexW`.

### Safety (§H — data / permission / future-update)
- `setSetting` (`src/util.cpp`) now preserves comments, blank lines and ALL
  unknown keys; `getSetting` skips comment lines.
- `User` (users.dat), `EmpProfile` (emp_*.dat) and `DeptCat` (depts.dat) gained
  forward-compat `extra`/`extraKv` capture so unknown columns/keys written by a
  FUTURE version are round-tripped verbatim and never silently dropped.

### Build
- Single static 32-bit `build/AzadiTeb.exe` (PE32, runs on Windows 7–11),
  rebuilt with `-Wall -Wextra -Werror` clean; refreshed `AzadiTeb.exe.sha256`.

---

## 1.4.0 — 2026-06-23

> Major feature release. Two-tier settings, a full vector print-designer,
> a LAN-sync layer, a sections/departments registry, header-only UI-kit
> controls, an UndoStack, plus reception-form polish and two new admin
> inboxes (profile-change requests, backup-log viewer).

### Added
- **Two-tier settings windows** (`src/user_settings.cpp`, §1) — the gear
  icon now routes through `OpenSettings(hMain, u)`, which dispatches to
  `OpenReceptionSettings` (760×520, 7 nav items) for reception users or
  `OpenManagementSettings` (1000×640, 16 nav items) for the clinic-management
  admin. Shared right-panel framework with fully-built panels (profile,
  theme, printer, save-messages, notifications, about, logout) and management
  panels (designer/restore launch, profile-requests inbox, backup-log viewer,
  global theme/printer). RTL nav rail on the right.
- **Vector print-designer** (`src/print_designer.{h,cpp}`,
  `print_designer_templates.inc`, `print_designer_ui.inc`, §3) — section
  picker, maximized WYSIWYG canvas, draggable/resizable/rotatable items, an
  inspector, 20 built-in templates, appointment counters, `.aztpl`
  import/export with an `AZTEMPLATE/1` magic header, keyboard shortcuts and a
  page-border hollow hit-test. File-backed design store under `data\designs\`.
- **Restore-design window** (§4) — imports a `.aztpl` back into the store.
- **Sections / departments registry** (`src/sections.{h,cpp}`, §2) —
  `Sections_All/Find/Upsert/Delete` over a pipe-delimited `data\sections.dat`,
  seeded with reception/injection/lab/radiology/physician entries.
- **Profile-change requests inbox** (`src/profile_requests.cpp`, §5) — admin
  ListView merging `NetSync_GetJson` drains + persisted
  `data\profile_requests_inbox\*.json`, with approve (applies the new name via
  `setUserFullName`) / reject (archives) actions.
- **Backup-log viewer** (`src/backup_log_viewer.cpp`, §7.2) — read-only
  newest-first viewer of `%LOCALAPPDATA%\AzadiTeb\backup_logs\backup.log`
  (and rotated siblings) with همه / موفق / ناموفق filter chips and a raw
  details pane.
- **LAN-sync layer** (`src/net_sync.{h,cpp}`, §9) — WinHTTP-first
  `NetSync_PostJson/GetJson/HeadOk/HostReachable`, with a silent file-based
  inbox/outbox fallback under the configured SMB share (or a local outbox).
- **UI-kit controls** (`src/ui_kit.{h,cpp}`, §10) — AzSwitch, AzNumberSpinner,
  AzColorPicker, AzDropZone, AzRulerH/V, AzGridLayer, AzHandle, plus the
  AzLayoutGuard (anti-overlap) and AzZOrderShield (no page bleed-through)
  handlers.
- **Header-only UndoStack** (`src/undo.h`, §11) — ring-buffer undo/redo used
  by the print-designer.
- **`FormatJalaliPersian` / `JalaliTodayKey`** (`src/util.cpp`, `src/app.h`)
  — Tehran-tz Jalali formatting (RLM-wrapped Persian-Indic digits) and an
  ASCII day-key for counters.
- **Reception reset button + Ctrl+R** (`src/reception.cpp`, §6) — explicit
  «پاک کردن فرم» control to start a fresh reception.
- **Header-collapse animation** (`src/handlers.cpp`, `src/main.cpp`, §6) — a
  small state machine slides the reception action bar up/down as the form is
  scrolled.

### Changed
- **Reception no longer clears fields after printing** (`src/reception.cpp`,
  §6) — populated data stays on screen for re-prints/tweaks; clearing is now
  explicit (reset button / Ctrl+R).
- **Reception dates routed through `FormatJalaliPersian`** (`src/reception.cpp`)
  — اعتبار / تاریخ نسخه auto-fields use the unified Persian formatter.
- **Legacy print designer deprecated** (`src/printer_designer.inc`, §3) — the
  old in-place designer is now a thin shim whose `openPrintDesigner()` forwards
  to the new `PrintDesigner_Open()`; `printer.cpp` includes `print_designer.h`.
- **Build** (`build.sh`, §13) — adds the new sources, links
  `-lwinhttp -lurlmon -lcrypt32 -lwintrust -lwtsapi32` (alongside the existing
  winspool/gdiplus/dbghelp/etc.), enables `-Werror` and `-s`.
- **`shot.sh`** — source/lib list synced with `build.sh`.

### Fixed
- All new modules compile clean under `-Wall -Wextra -Werror`
  (misleading-indentation lambdas split onto separate lines).

### Notes
- Logging policy unchanged (§8): the only on-disk log remains
  `backup_logs/backup.log` (+ crash dumps); `.gitignore` extended with
  `logs/` and the v1.4.0 runtime stores.
- Still a single static PE32 exe built by `build.sh`; a `.sha256` sidecar is
  written next to `build/AzadiTeb.exe`.

---

## 1.3.0 — 2026-06-23

### Added
- **Clinic-management panel: «بیماران» (Patients) page** (`src/manage.inc`)
  — a virtualized `LVS_OWNERDATA` ListView placed as the second nav item
  (index 1, right after the dashboard). Reuses the existing patient data
  layer (`loadAllPatients`) loaded ONCE on a background worker thread so
  the UI never blocks; only visible rows are materialized via
  `LVN_GETDISPINFO`. Includes a debounced (250 ms) global search box that
  filters the whole set with Persian/Arabic normalization
  (`uikit::NormalizeFa`), plus a live "showing N of M" status line.
- **Reception-user profile entry in Settings** (`src/settings.cpp`) — the
  already-implemented profile change request flow (edit display name +
  avatar, queued for management approval via `showProfileDialog`) is now
  surfaced as the «پروفایل من» row for every signed-in user.
- **Backup analyzer: per-table + patients-domain analysis**
  (`src/backup_analyzer.cpp`) — the SQLite path now extracts the table
  list and per-table column counts straight from the recovered
  `CREATE TABLE` statements (no `sqlite3` link needed) and detects a
  `patients` table by name or by its signature columns, reporting a real
  domain summary taken from the on-disk schema.

### Changed
- **Reception form: hardened section-header / input no-overlap invariant**
  (`src/reception.cpp`) — row pitch raised to `step = rh + S(46)` and the
  blue section caption is now drawn `S(42)` above the input baseline with
  its band cleared to the card surface first, guaranteeing a strictly
  positive gap so a blue section label («اطلاعات تماس», «نوبت و بیمه»،
  «مبلغ و تخفیف») can never sit behind/under an input at any DPI rounding.
- Patients moved off the developer surface: the hidden-admin panel no
  longer exposes a patients tab; operational patient data lives in the
  clinic-management panel where it belongs.

### Removed
- **Patients tab from the hidden-admin panel** (`src/admin.cpp`) —
  `AD_TAB_COUNT` reduced to 1 (only «کاربران»); the patients tab is no
  longer reachable from the owner/developer backdoor surface.

### Fixed
- Blue reception section labels could visually touch/overlap the input
  directly beneath them at certain DPI roundings — eliminated by the
  larger clearance band and the per-caption background clear.
- Backup analyzer error card already shows the full rich diagnostic
  payload (breadcrumbs, stack, file identity, free disk) verbatim and the
  progress bar never reaches 100% before the worker reports it — verified
  and retained.

### Notes
- Logging policy re-verified: the only on-disk logs are the dedicated
  Backup Log (`%LOCALAPPDATA%/AzadiTeb/backup_logs/backup.log`) and crash
  dumps; the general `logLine()` channel is a compile-time no-op in
  release. `.gitignore` extended with `crashdumps/`.
- Build remains a single static PE32 exe via `build.sh`
  (`i686-w64-mingw32-g++`), compiling cleanly.

---

## 1.2.0 — 2026-06-23

### Added
- Admin: "بیماران" patients tab with virtualized list, debounced
  global search, multi-chip filters, detail drawer, background DB
  worker.
- Reception user settings: profile change request flow (name + avatar)
  with admin approval inbox.
- Reception user settings: theme switcher, notifications, printer
  picker with paper size + print preview + test print.
- Admin inbox for profile change requests.
- Backup Log channel (only logging that remains): dedicated
  `%LOCALAPPDATA%/AzadiTeb/backup_logs/backup.log`, 2 MB rotation,
  last-5 gzipped, with timestamp/pid/tid/phase/file/size/identity-hash/
  Win32+SQLite+SEH/C++ error text/stack trace/free-disk/breadcrumbs.
- Crash handler now also writes a full `MiniDumpWriteDump` to
  `%LOCALAPPDATA%/AzadiTeb/crashdumps/` (crash-only, last 5 kept).

### Changed
- Reception form layout: new 3-column grid; all blue section labels
  always visible; no vertical scroll at 1024×600; deterministic
  positioning across tab switches (DeferWindowPos atomic layout).
- Backup Analyzer page rewritten: real progress, real errors, no
  ghost controls (solid full-client background), modal-child window
  behaviour, Esc & Ctrl+B clean toggle (auto-repeat guarded).
- UI kit hardened: AzCard, AzSectionHeader, AzCombo, AzProgress,
  AzCheckbox, AzInput, AzDateInput etc. — all real HWND children
  with RAII GDI handles.

### Removed
- All user-behavior logging (`logLine` gated behind `AZ_DEBUG_LOGS`,
  OFF in release; no `app.log`/`build/logs/` writes in normal use).
- `build/logs/` directory.

### Fixed
- Right-column "بیمه مکمل" combobox border corruption.
- Right-column checkbox/caption misalignment ("فعال", "اتوماتیک").
- Items jumping position when switching admin tabs.
- Backup analyzer always failing with "خطای پیش‌بینی‌نشده" — now
  guarded with a vectored exception handler + try/catch and full
  forensic logging via the Backup Log channel.
- Ghost controls bleeding into analyzer page.
- GDI handle leaks under prolonged use.
- Persian-path file open failures (UTF-8 + URI mode).

---

## v1.10.0 — 2026-06-22 — تب «بیماران»، آنالایزر مخفی بکاپ (Ctrl+B)، فشرده‌سازی پذیرش، کیت رابط کاربری

این نسخه چند قابلیت بزرگ و یک لایهٔ رابط‌کاربری قابل‌استفادهٔ مجدد اضافه می‌کند و
چند باگ نهفته را رفع می‌نماید. تمام تغییرات با `-Wall -Wextra` بدون هیچ هشداری
کامپایل می‌شوند.

### ✨ کیت رابط کاربری جدید (`ui_kit.h`، `ui_kit.cpp`)
1. **محافظ‌های RAII برای GDI**: `GdiObj`، `SelectScope`، `MemDC`، `WindowDC` تا
   هیچ `HFONT/HBRUSH/HPEN/HDC` نشت نکند (انتخاب خودکار برمی‌گردد، آزادسازی تضمینی).
2. **`DrawSectionHeader()`**: یک منبع واحد برای همهٔ تیترهای آبی بخش‌ها با فاصلهٔ
   استانداردِ پیکسلی‌یکسان (marginTop=10، fontHeight=14، marginBottom=4).
3. **پنل‌ها و چیپ‌های گرد** (`RoundedPanel`، `Card`، `Chip`، `InputWell`) با لبهٔ
   ضدّپله و رنگ‌های گرفته‌شده از `theme.cpp` (هیچ رنگی هاردکد نشده).
4. **`NormalizeFa()`**: نرمال‌سازی فارسی/عربی برای جستجو (ي→ی، ك→ک، ة→ه، حذف
   ZWNJ و کشیده، تبدیل ارقام فارسی/عربی به اسکی، یکدست‌سازی فاصله‌ها).

### 🧑‍⚕️ تب جدید «بیماران» در پنل ادمین (`admin.cpp`، `data_ext.cpp`، `app.h`)
5. **نوار تب**: پنل ادمین حالا دو تب دارد: «کاربران» و «بیماران».
6. **لیست مجازی‌سازی‌شده** (`LVS_OWNERDATA` + `LVN_GETDISPINFO`): فقط ردیف‌های قابل‌مشاهده
   متریالایز می‌شوند؛ با ۱۰۰٬۰۰۰+ ردیف هم روان می‌ماند.
7. **بارگذاری پس‌زمینه**: کل مخزن بیماران یک‌بار روی یک `std::thread` خوانده می‌شود و با
   `PostMessage` به نخ رابط برمی‌گردد (هرگز نخ UI را بلاک نمی‌کند).
8. **جستجوی سراسری با debounce ۲۵۰ms**: جستجو روی نام/کد ملی/موبایلِ همهٔ بیماران با
   نرمال‌سازی فارسی؛ نتایج هم مجازی‌سازی شده‌اند.
9. **حذف بیمار** با تأیید، و به‌روزرسانیِ فقط ردیف اثرگذار.
10. لایهٔ داده: افزودن `loadAllPatients()` و `deletePatient()` (ساختار `PatientRow`).

### 🔎 آنالایزر مخفی بکاپ — فقط با Ctrl+B (`backup.cpp`، `backup_analyzer.cpp/.h`)
11. **صفحهٔ مخفی**: داخل پنجرهٔ پشتیبان‌گیری با **Ctrl+B** ظاهر و با Ctrl+B یا Esc
    پنهان می‌شود (هیچ منو/دکمه‌ای آن را آشکار نمی‌کند).
12. **آنالیز واقعی چندفرمتی** روی نخ پس‌زمینه با تشخیص نوع از روی **بایت‌های جادویی**:
    - **SQLite 3**: خواندن مستقیم هدر روی‌دیسک (اندازهٔ صفحه، تعداد صفحات → حجم،
      کدگذاری متن، schema cookie، user_version، application_id)، استخراج
      `CREATE TABLE/INDEX/TRIGGER/VIEW` و **اثرانگشت SHA-256 ساختار** (پیاده‌سازی
      مستقل SHA-256، بدون لینک‌کردن sqlite).
    - **ZIP**: شمارش ورودی‌ها، حجم خام/فشرده و نسبت فشرده‌سازی.
    - **دامپ SQL**: شمارش `CREATE TABLE` / `INSERT INTO` و تعداد دستورها.
    - **JSON**: شمارش اشیاء/آرایه‌ها/کلیدها.
    - **متن/`.aztbk`**: تشخیص و خلاصهٔ رکوردهای بیمار.
13. **نوار پیشرفت معین (بایت‌محور)** + خط وضعیت زنده.
14. **دکمه‌های کپی**: «کپی» برای هر بخش + «کپی کامل گزارش» (CF_UNICODETEXT).
15. **مقاوم**: هر خطا به‌صورت کارت خطای درون‌خطی نمایش داده می‌شود (بدون کرش)؛ همهٔ
    I/O با `_wfopen`/Unicode برای مسیرهای فارسی.

### 🧾 رفع چیدمان پذیرش — بدون اسکرول (`reception.cpp`)
16. **حذف ردیف هدر خالی** «میز پذیرش بیمار» و جمع‌کردن نوار اطلاعات (۵۴px → ۶px) و
    حذف خط جداکنندهٔ بالای آن؛ تب‌ها و فرم به‌سمت بالا منتقل شدند.
17. **فشرده‌سازی فرم**: ارتفاع ورودی‌ها ۳۰→۲۶، فاصلهٔ ردیف‌ها کم شد (`step` ۸۶→۶۶) و
    آفست تیترها هماهنگ شد تا فرم در ۱۰۲۴×۶۰۰ بدون اسکرول جا شود.
18. به‌روزرسانی `rcFormContentH` و ارتفاع دکمهٔ «ثبت پذیرش و صدور قبض».

### 🐞 رفع باگ‌ها و سخت‌سازی (`main.cpp`، `app.manifest`، `build.sh`)
19. **DPI نسخه ۲**: اعلام Per-Monitor V2 در مانیفست + فراخوانی
    `SetProcessDpiAwarenessContext` در startup با fallback به PerMonitor v1 و
    سپس `SetProcessDPIAware` روی ویندوزهای قدیمی.
20. **پاک‌سازی هشدارها**: کل پروژه با `-Wall -Wextra` بدون هشدار کامپایل می‌شود
    (رفع `dangling-else` در `manage.inc`، `unused-but-set` در `reception.cpp`،
    و کست‌های امن `GetProcAddress`).
21. **build.sh**: افزودن منابع جدید، فلگ‌های `-DUNICODE -D_UNICODE`، لینک
    `-lmsimg32 -ldwmapi -luxtheme -lversion -lwinmm`، و تولید خودکار
    `AzadiTeb.exe.sha256`.

---

## v1.9.7 — 2026-06-20 — تکمیل تنظیمات چاپ بخش پذیرش: گزینه‌های جامع چاپگر

تکمیل دیالوگ «تنظیمات چاپگر و چاپ» با افزودن گزینه‌هایی که قبلاً وجود نداشتند.

### ✨ افزوده / تغییر / رفع (`printer.cpp`، `reception.cpp`، `app.h`)
1. **اندازه‌های بیشتر کاغذ**: علاوه بر A4/A5، افزوده شدن **رول حرارتی ۸۰ میلی‌متر**
   و **رول حرارتی ۵۸ میلی‌متر** برای چاپگرهای فیش‌زن پذیرش.
2. **تعداد نسخهٔ چاپ** (− N +): امکان تعیین ۱ تا ۵ نسخه برای هر چاپ؛ در
   `printDesignedReceipt` به‌صورت حلقهٔ صفحه‌ای واقعی اعمال می‌شود.
3. **فعال/غیرفعال کردن چاپ هر بخش**: کلید روشن/خاموش مستقل برای هر بخش (۱۱ بخش)؛
   هنگام خاموش بودن، چاپ آن بخش انجام نمی‌شود.
4. **چاپ خودکار قبض پس از ثبت**: کلید روشن/خاموش (`auto_print`).
5. **باز کردن کشوی پول پس از چاپ**: پالس استاندارد ESC/POS (`ESC p`) مستقیماً از
   طریق اسپولر RAW؛ تنها وقتی گزینه فعال باشد (`kickCashDrawer`).
6. **چاپ سربرگ/لوگوی درمانگاه**: کلید روشن/خاموش (`print_logo`).
7. **بازچینش دیالوگ**: کارت بلندتر (S(820)) و چیدمان فشرده و بدون همپوشانی؛ همهٔ
   کلیدها و کلیدهای روشن/خاموش با hit-test دقیق هم‌تراز شدند.
8. اتصال پالس کشوی پول به مسیر چاپ قبض پذیرش پس از موفقیت.
9. ارتقای نسخه به **۱.۹.۷** و ساخت EXE تولیدی تازه.

---

## v1.9.6 — 2026-06-20 — تکمیل نهایی: بارگذاری تصویر کارکنان، نگاشت طراحی چاپ هر بخش، چاپ نوبت با قالب اختصاصی

تکمیل پاس ترمیم سراسری و آماده‌سازی نسخهٔ تولیدی نهایی.

### ✨ افزوده / تغییر / رفع
1. **ثبت کارکنان — بارگذاری عکس پرسنلی و تصویر کارت ملی** (`manage.inc`):
   - افزودن دو دکمهٔ بارگذاری به فرم کارمند جدید (`NE_B_PHOTO`, `NE_B_IDCARD`) با
     انتخابگر فایل تصویر (`GetOpenFileNameW`) و ذخیرهٔ محلی پیوست
     (`copyAttachmentLocal`). مسیرها در `EmpProfile.photoPath` / `idCardPath`
     ماندگار می‌شوند و متن دکمه پس از انتخاب با ✓ به‌روزرسانی می‌گردد.
   - افزایش ارتفاع کارت فرم با مهار سرریز (`neCard`/`neLayout`).
2. **چاپ — نگاشت طراحی هر بخش (۱۱ بخش)** (`printer.cpp`):
   - گسترش `PRINT_SECTIONS` از ۵ به ۱۱ بخش (پذیرش درمانگاه، نوبت‌دهی،
     قبض/صورتحساب، بیمه، بیمه مکمل، مبلغ نهایی، نسخه پزشک، تزریقات، آزمایشگاه،
     داروخانه، رادیولوژی).
   - افزودن انتخابگر بخش (‹ قبلی / نام بخش / بعدی ›) به دیالوگ تنظیمات چاپگر تا هر
     بخش طراحی و قالب اختصاصی خود را داشته باشد (`PSB_SEC_PREV`/`PSB_SEC_NEXT`).
3. **صفحهٔ نوبت‌دهی — چاپ قبض نوبت با قالب اختصاصی بخش** (`appointment.cpp`):
   - `printApptSlip` ابتدا طراحی ذخیره‌شدهٔ بخش «نوبت‌دهی» (اندیس ۱) را با
     `printDesignedReceipt` اعمال می‌کند و در نبود طراحی، به قبض داخلی تمیز
     بازمی‌گردد. این رفتار نگاشت طراحی هر بخش را برای نوبت‌دهی نیز کامل می‌کند.
4. **بازبینی و تأیید صحت** (بدون تغییر کد، تأیید عملکرد):
   - واردات بازیابی پشتیبان به لایهٔ داده (بومی `.aztbk` و خارجی SQL Server
     `.bak`/SQL/CSV) با بازخورد شمارش رکورد و عدم جعل داده تأیید شد.
   - گردش‌کار تأیید/رد ادمین (`setSetReqStatus`: ۰ در انتظار / ۱ تأیید+اعمال /
     ۲ رد+پیام) با محافظ idempotent تأیید شد.
   - دیالوگ انتخاب شیفت: گوشه‌های گرد، سایه، هایلایت وضعیت (موفقیت/اکسنت)، چیدمان
     بدون همپوشانی تأیید شد.
5. **نسخه**: ارتقا به `1.9.6` و ساخت EXE تولیدی تازه در `build/`.

---

## v1.9.2 — 2026-06-19 — تکمیل پاس ترمیم: نوبت‌دهی، عملکرد پشتیبان، واردات SQL Server، آواتار پذیرش

ادامه و تکمیل پاس ترمیم سراسری: رفع همپوشانی سرفصل/برچسب‌ها در صفحهٔ نوبت‌دهی،
بهبود اساسی نرخ فریم (FPS) صفحهٔ پشتیبان‌گیری ادمین، واردات واقعی رکوردهای بیمار از
فایل‌های پشتیبان خارجی (از جمله SQL Server `.bak`/متن SQL/CSV)، افزودن دکمهٔ «ذخیره
در پیام‌ها» به نمایشگر پیام، و ترمیم آواتار فرم پذیرش (دایرهٔ بزرگ‌تر، آیکن کوچک‌تر و
کاملاً مرکزشده، بدون بیرون‌زدگی). نسخهٔ EXE تولیدی تازه ساخته شد.

### ✨ افزوده / تغییر / رفع
1. **صفحهٔ نوبت‌دهی — رفع همپوشانی سرفصل/برچسب و کوتاهی متن دکمه‌ها** (`appointment.cpp`):
   - تنظیم متریک‌های ریتم عمودی: `apRowH` از `S(46)` به `S(58)`، `apLblGap` از
     `S(18)` به `S(22)`، `apGrpPad` از `S(8)` به `S(10)`. حالا هیچ کمبوباکس/ورودی
     روی سرفصل گروه یا برچسب فیلد نمی‌افتد و فاصلهٔ برچسب‌تا‌فیلد یکدست است.
   - رفع ارتفاع گروه «نوبت» (محاسبهٔ ۳ ردیف به‌جای ۲): `apptBottom = gy +
     apRowH()*3 + S(34) + apGrpPad()`.
   - پهن‌تر کردن دکمه‌های نوار ابزار تا متن کامل دیده شود (ارسال پیام / انتقال نوبت /
     چاپ / ذخیره چیدمان / حذف چیدمان).
2. **عملکرد صفحهٔ پشتیبان‌گیری ادمین — رفع افت FPS** (`backup.cpp`):
   - الگوی «پس‌زمینهٔ کش‌شده» (همانند `settings.cpp`): لایه‌های سنگین ثابت
     (اسکریم + سایه + گرادیان کارت + عنوان/زیرعنوان) در یک بیت‌مپ خارج از صفحه
     (`s_bgDC`/`s_bgBmp`) کش می‌شوند و فقط هنگام تغییر اندازه/پوسته/حالت بازسازی
     می‌گردند (`bkBuildBg`).
   - `bkPaintFg` فقط پیش‌زمینهٔ تعاملی ارزان را می‌کشد؛ `bkPaint` نوار کثیف را از کش
     بلیت می‌کند و پیش‌زمینه را کلیپ‌شده رسم می‌کند.
   - `WM_MOUSEMOVE` فقط مستطیل قبلی/جدید زیر اشاره‌گر را باطل می‌کند (نه کل پنجره)؛
     `WM_TIMER` فقط نوار پیشرفت را هنگام مشغول‌بودن باطل می‌کند. حذف کامل
     `gpFillAlpha` سراسری در هر حرکت ماوس.
3. **واردات واقعی رکوردهای بیمار از پشتیبان خارجی** (`backup.cpp`):
   - افزودن `sniffForeign()` (تشخیص قالب بر اساس هدر/پسوند: AZTBKP01 / TAPE (MTF) /
     MSSQL / INSERT INTO / CREATE TABLE / CSV) و `importForeignPatients()`.
   - واردات با پنجرهٔ لغزان ۴ مگابایتی (سقف ۵۱۲MB، همپوشانی ۲۵۶ بایت): یافتن
     رشته‌های ۱۰ رقمی، اعتبارسنجی چک‌سام `validNationalId`، خواندن نام مجاور به‌صورت
     UTF-16LE (SQL Server nvarchar) یا UTF-8، و فراخوانی `rememberPatient` (شبکه‌ای،
     بدون جعل داده). آزمون واحد: واردات ۲ رکورد از دادهٔ ساختگی MSSQL موفق.
   - `restoreWorker` در شاخهٔ خارجی اکنون به‌جای شبیه‌سازی، واقعاً رکوردها را وارد
     لایهٔ دادهٔ شبکه‌ای می‌کند و شمارش را گزارش می‌دهد.
4. **نمایشگر پیام — دکمهٔ «ذخیره در پیام‌ها»** (`reception.cpp`):
   - افزودن `CART_BTN_SAVE` و دکمهٔ سبز قابل‌مشاهده در `drawCartDetail` (تنها وقتی
     قابلیت پیام‌های ذخیره‌شده فعال و در حالت بایگانی نباشیم) که پیام انتخاب‌شده را با
     `pushSavedMsg` ذخیره و تأیید نمایش می‌دهد.
5. **فرم پذیرش — ترمیم آواتار** (`reception.cpp`):
   - `drawGuestAvatar` اکنون دیسک را به‌عنوان ناحیهٔ کلیپ دایره‌ای
     (`CreateEllipticRgn` + `SaveDC`/`RestoreDC`) به‌کار می‌برد تا سر/شانه هرگز از لبهٔ
     گرد بیرون نزند؛ شبح کوچک‌تر و کاملاً مرکزشده (شعاع سر ۳۰٪، مرکز سر در یک‌سوم
     بالایی، قوس شانهٔ کم‌عمق).
   - بزرگ‌تر شدن دایرهٔ آواتار `S(40)` → `S(44)` با فاصلهٔ نفس‌گیری بیشتر در پایین.
   - نتیجه: یک نشانگر «بدون عکس» تمیز و حرفه‌ای کاملاً درون حلقه.
6. **تأیید عملکردهای موجود** (بدون تغییر کد، بازبینی‌شده):
   - تأیید/رد درخواست‌های تغییر تنظیمات در `setSetReqStatus`/`applyPayload`
     (`employees.cpp`) به‌درستی پایدار و شبکه‌ای است (اعمال `key=val;key=val` و اطلاع
     به درخواست‌دهنده).
   - استعلام کد ملی با Enter و تکمیل خودکار در هر دو صفحهٔ پذیرش و نوبت‌دهی،
     شبکه‌ای از طریق `lookupCitizen` → فروشگاه محلی در `dataDir`.
   - چاپ: مسیر کامل `printDesignedReceipt` با مقیاس‌بندی DPI، حالت‌های fit/fill،
     متن RTL، خط/قاب/لوگو و گزینهٔ چاپگر پیش‌فرض یا دیالوگ سیستمی سالم است.
7. **ساخت نسخهٔ تولیدی تازه**: `build/AzadiTeb.exe` با همهٔ تغییرات این جلسه و بدون
   `AZ_DEBUG_BUILD` بازسازی شد. نسخه به **۱.۹.۲** در `app.h` و `app.rc` ارتقا یافت.

### 📁 فایل‌های تغییریافته
- `src/appointment.cpp` — متریک‌های ریتم عمودی + پهنای دکمه‌ها + ارتفاع گروه نوبت.
- `src/backup.cpp` — پس‌زمینهٔ کش‌شده + dirty-rect (FPS) + واردات SQL Server/خارجی.
- `src/reception.cpp` — دکمهٔ ذخیرهٔ پیام + ترمیم آواتار (کلیپ دایره‌ای، اندازه).
- `src/app.h`, `src/app.rc` — ارتقای نسخه به ۱.۹.۲.
- `docs/CHANGELOG.md` — همین ورودی.

---

## v1.9.1 — 2026-06-19 — پاس کامل ترمیم بصری/ساختاری/عملکردی سراسر پروژه

یک پاس کامل بازبینی و ترمیم روی همهٔ صفحات با حفظ تمام امکانات و معماری دسکتاپ
Win32. شامل رفع نشتی گوشه‌های گرد، اسکرول عمودی فرم پذیرش، استعلام کد ملی با
Enter و تکمیل خودکار، بازیابی پشتیبان به لایهٔ داده با تأیید کاربری، و یکدست‌سازی
فاصله‌گذاری/سرفصل‌ها/کنترل‌ها در همهٔ صفحات.

### ✨ افزوده / تغییر / رفع
1. **فرم پذیرش — اسکرول عمودی کامل** (`reception.cpp`):
   - افزودن سبک `WS_VSCROLL` فقط به برگهٔ پذیرش (`addTabKind`) و هندلرهای
     `WM_VSCROLL` + `WM_MOUSEWHEEL` (پرش خطی/صفحه‌ای/کشویی، چرخ ماوس `S(60)`).
   - تابع `recPageVH()` ارتفاع مجازی صفحه = بیشینهٔ ارتفاع فرم/پنل اطلاعات/صورتحساب؛
     `recClampScroll`/`recUpdateScrollbar` همگام‌سازی نوار اسکرول.
   - `paintInfoPanel()` کاملاً با آفست `SY()` بازنویسی شد (پنل اطلاعات سمت راست،
     کارت صورتحساب سمت چپ، و فرم میانی همگی با هم اسکرول می‌شوند). هیچ کنترلی
     بیرون از پنل نمی‌افتد و محتوای انتهایی (مبلغ/تخفیف/دکمهٔ ثبت/پزشک معالج)
     قابل دسترسی است.
   - کارت‌های فرم و صورتحساب با `gpRoundRectBg(...)` رسم می‌شوند تا گوشه‌های گرد
     بدون نشتی پس‌زمینهٔ سفید/خاکستری کلیپ شوند.
2. **استعلام شبکه‌ای بیمار با کد ملی + Enter + تکمیل خودکار** (`reception.cpp`, `data_ext.cpp`):
   - زیرکلاس اختصاصی `nidEditProc` روی `eNid`: با فشردن **Enter** همیشه
     `doInquiry()` اجرا می‌شود (مستقل از وضعیت «دارای بیمه») و مشخصات بیمار از
     منبع معتبر (سرویس ثبت احوال در صورت پیکربندی یا سوابق همین درمانگاه) به‌صورت
     خودکار پر می‌شود: نام/نام خانوادگی/نام پدر/تاریخ تولد/جنسیت/تماس/بیمه/بیمهٔ
     مکمل. سپس فوکوس به اولین فیلد خالی هویت می‌پرد.
   - رویداد `EN_KILLFOCUS` کد ملی: هنگام Tab، اگر کد ۱۰ رقمی کامل باشد تکمیل
     خودکار به‌صورت بی‌صدا انجام می‌شود (بدون نمایش خطای ناخواسته).
   - بدون جعل داده: کد نامعتبر/یافت‌نشده/ناقص با ظرافت مدیریت می‌شود (قاب قرمز و
     ورود دستی)؛ هویت‌های تأییدشده/ثبت‌شده در `rememberPatient` ذخیره می‌شوند تا
     دفعهٔ بعد همان بیمار بازیابی شود.
3. **بازیابی پشتیبان به لایهٔ داده با تأیید کاربری** (`backup.cpp`):
   - `restoreWorker` تعداد فایل‌های بازنویسی‌شده و تعداد رکوردهای بیمار بازیابی‌شده
     (`patients.dat`) را می‌شمارد؛ پرچم `doneSignal=2` در نخ کارگر تنظیم و در
     `WM_TIMER` نخ رابط، پیام تأیید «بازیابی موفق — N فایل / M رکورد بیمار در
     سامانه بارگذاری شد» نمایش داده می‌شود و فریم اصلی بازترسیم می‌گردد.
   - ساختار صفحهٔ بازیابی: نوار انتخاب فایل، چهار دستهٔ قابل‌تیک با آیکن/نام/حجم
     تخمینی، تیک اصلی «همهٔ اطلاعات بیماران»، نوار پیشرفت و دکمه‌های متوازن.
4. **دیالوگ تنظیمات** (`settings.cpp`):
   - رفع نشتی رنگ آبی در گوشه‌های بالای کارت با `gpFillCorners(...)`.
   - ارتفاع سربرگ `headerH()` از `S(176)` به `S(204)` و قرار دادن نام و نقش
     کاربر در خطوط مجزای خود (دیگر متن نقش بریده نمی‌شود).
5. **پنل ادمین (مدیریت کاربران)** (`admin.cpp`):
   - افزودن «چاهک ورودی» (input well) پشت ۵ فیلد فرم برای جداسازی بصری؛ لیبل‌ها
     بالای فیلدها با فضای عمودی مستقل.
6. **پنل مدیریت — صفحات داخلی** (`manage.inc`):
   - صفحهٔ «پیام‌های ذخیره‌شده»: سرفصل «یادداشت جدید» با آیکن، ویرایشگر چندخطی
     بلندتر (`S(74)`)، دکمه‌های «ذخیره/تصویر» چیده‌شده در سمت چپ، راهنما زیر بلوک.
   - صفحهٔ «درخواست‌ها»: دکمه‌ها به زیر بنر اعلان منتقل شدند تا روی عنوان/بنر
     نیفتند.
   - دکمهٔ ورود به مدیر پشتیبان: پهنا `S(320)→S(380)`، آیکن چسبیده به لبهٔ راست و
     متن در مرکز فضای سمت چپ (بدون بریدگی).
7. **نسخه**: `APP_VERSION_W` به `1.9.1` ارتقا یافت.

### 📁 فایل‌های تغییر یافته
`src/reception.cpp`، `src/data_ext.cpp`، `src/backup.cpp`، `src/settings.cpp`،
`src/admin.cpp`، `src/manage.inc`، `src/main.cpp`، `src/app.h`.

### 🧪 ساخت
`./build.sh` → `build/AzadiTeb.exe` (PE32 i386، استاتیک، ~1.7MB) بدون خطا.

---

## v1.9.0 — 2026-06-19 — بازطراحی پنل مدیریت، گردش‌کار تأیید درخواست‌ها، پیام‌های ذخیره‌شده، پنل کارکنان و مدیر پشتیبان

به‌روزرسانی بزرگ شامل بازطراحی کامل پنل مدیریت، گردش‌کار تأیید/رد درخواست‌های
تغییر تنظیمات، مرکز درخواست‌ها، درخواست‌های تغییر پروفایل، سیستم پیام‌های
ذخیره‌شده (یادداشت‌های محلی)، پنل پیام کارکنان، اعلان ویندوزی برای کارمندان،
و مدیر پشتیبان‌گیری/بازیابی.

### ✨ افزوده / تغییر / رفع
1. **آیکن‌های مدرن پنل مدیریت** (`theme.cpp`):
   - تابع `drawIcon()` اکنون از قلم هندسی با سرگرد (`ExtCreatePen` با
     `PS_GEOMETRIC|PS_SOLID|PS_ENDCAP_ROUND|PS_JOIN_ROUND`) استفاده می‌کند تا
     آیکن‌ها صاف‌تر و حرفه‌ای‌تر رسم شوند.
2. **گردش‌کار تأیید درخواست تغییر تنظیمات** (`settings.cpp`, `printer.cpp`, `employees.cpp`):
   - کارمند درخواست تغییر می‌دهد → دیالوگ «مطمئن هستید؟» + «به مدیریت ارسال شد»؛
     مدیر مستقیماً اعمال می‌کند. توابع `settingsRequestGate`/`printerRequestGate`
     و `pushSetReqEx`/`setSetReqStatus`/`markOneSetReqSeen`/`deleteSetReq`.
   - رفع همپوشانی لیبل آدرس سرور در تنظیمات مدیریت.
3. **مرکز درخواست‌ها** (`manage.inc`):
   - حاشیهٔ سبز نازک هنگام انتخاب، خوانده‌شدن = خاکستری، حذف با تأیید،
     «همه را خوانده‌شده علامت بزن»، شمارندهٔ زنده، و راست‌کلیک «ارسال به
     پیام‌های ذخیره‌شده».
4. **درخواست‌های تغییر پروفایل** (`manage.inc`):
   - نمایش منبع سیستمی + جزئیات کارمند + بزرگ‌نمایی/دانلود تصویر.
5. **پیام‌های ذخیره‌شده / یادداشت‌های محلی** (`manage.inc`, `employees.cpp`):
   - یادداشت محلی برای کارمندان و مدیریت (متن + تصویر)، هرگز شبکه‌ای نمی‌شود.
     توابع `pushLocalNote`/`loadLocalNotes`/`deleteLocalNote`/`localNoteCount`،
     ضمیمهٔ تصویر با `copyAttachmentLocal`، حذف ردیف با تأیید و باز کردن تصویر.
6. **پنل پیام کارکنان** (`manage.inc`):
   - لیست سمت راست، به‌صورت پیش‌فرض بسته، دکمهٔ «مشاهدهٔ کارکنان»، جستجو،
     گروه‌بندی بر اساس دپارتمان، نقطهٔ سبز برخط / خاکستری برون‌خط.
7. **اعلان ویندوزی فقط برای کارمندان** (`main.cpp`, `employees.cpp`):
   - `notifyNewMessageRecipients()` (مدیریت را نادیده می‌گیرد) +
     `showWindowsNotification()`، متصل در WM_TIMER.
8. **مدیر پشتیبان‌گیری/بازیابی** (`backup.cpp`):
   - بازیابی کامل/انتخابی، مدیریت فایل‌های بزرگ `.bak`، استریم در پس‌زمینه،
     نوار پیشرفت. قالب `AZTBKP01`، نقطهٔ ورود `openBackupManager(HWND)`.
9. **رابط پذیرش/نوبت‌دهی** (`reception.cpp`, `appointment.cpp`):
   - دکمه‌های آبی، کنترل‌های کوتاه‌تر و فاصلهٔ لیبل بیشتر
     (`rcVMetrics`: ارتفاع ۳۴→۳۰، گام ۵۲→۵۶)، آواتار خاکستری بزرگ‌تر،
     رفع برش‌خوردگی «ذخیرهٔ چیدمان».
10. **اعتبارسنجی فیلد خالی** (`reception.cpp`, `appointment.cpp`):
    - فقط حاشیهٔ قرمز نازک بدون هالهٔ مشکی (`invalidMask`).
11. **حذف دکمهٔ استعلام بیمه** از صورتحساب.
12. **حالت پشتیبان زنده دادهٔ بیماران** (`scripts/backup.sh`):
    - افزودن فلگ `--data`/`--live` برای ساخت آرشیو زندهٔ دادهٔ بیماران جهت دانلود.
13. **ارتقای نسخه به 1.9.0** (`app.rc`, `app.h`, `update/version.txt`).



رفع تداخل نمایشی باقی‌مانده (لیبل/تکست‌باکس/دکمه روی هم) و اصلاح رفتار
حاشیهٔ تکست‌باکس هنگام فوکوس طبق درخواست کاربر.

### ✨ تغییر / رفع
1. **حاشیهٔ فوکوس قرمز نازک و محو** (`reception.cpp`, `appointment.cpp`, `app.h`):
   - هنگام فوکوس (مثلاً بعد از زدن Enter روی کد ملی) به‌جای حاشیهٔ مشکی
     پیش‌فرض ویندوز، یک **حاشیهٔ قرمز خیلی نازک و محو** رسم می‌شود؛ با خروج/کلیک
     روی فیلد دیگر، به حاشیهٔ عادی برمی‌گردد.
   - تابع کمکی `blendColor()` در `app.h` افزوده شد (ترکیب رنگ خطر با پس‌زمینهٔ ورودی).
   - بازترسیم فوری با `EN_SETFOCUS/EN_KILLFOCUS/CBN_SETFOCUS/CBN_KILLFOCUS`.
2. **فرم نوبت‌دهی** (`appointment.cpp`):
   - کادر ورودی (well) با همان حاشیهٔ فوکوس قرمز برای همهٔ فیلدها افزوده شد
     (قبلاً تکست‌باکس‌ها بدون کادر و در هم بودند).
   - ارتفاع ردیف `30→44`، ارتفاع ورودی `→28`، لیبل‌ها بالاتر و شروع گروه جستجو
     پایین‌تر تا تداخل لیبل/ورودی/عنوان گروه/ردیف بعدی برطرف شود.
3. **بیلد**: `build/AzadiTeb.exe` تازه ساخته و جایگزین خروجی قبلی شد؛ نسخه `1.8.1`.

---

## v1.8.0 — 1405/03/28 (2026-06-18) — بازطراحی UI/UX، پنل مدیریت داشبوردی و پیام‌های ذخیره‌شده

این نسخه سیزده محور درخواستی کاربر را پوشش می‌دهد (UI/UX و گسترش امکانات).

### ✨ افزوده / تغییر / رفع
1. **آیکن‌های مدرن تنظیمات و ماشین‌حساب** (`app.rc`, `theme.cpp`, `main.cpp`):
   آیکن‌های تمیز و سازگار با تم روشن/تیره به‌عنوان آیکن دکمه‌ها.
2. **رفع سراسری باگ گوشه‌های گرد** (`gdiplus.cpp`, `app.h`, `reception.cpp`,
   `appointment.cpp`, `manage.inc`): توابع جدید `gpRoundRectBg` /
   `gpGradRoundRectBg` / `gpFillCorners` ناحیهٔ گوشهٔ المان‌های گردگوشه را با رنگ
   پس‌زمینهٔ تم وصله می‌کنند؛ دیگر هیچ گوشهٔ سیاه/نادرستی در دکمه‌ها، تکست‌باکس‌ها،
   فریم‌ها، پنل‌ها، لیست‌باکس‌ها و کمبوباکس‌ها دیده نمی‌شود.
3. **کمبوباکس/لیست‌باکس بدون حاشیه** (`theme.cpp`, `admin.cpp`, `appointment.cpp`):
   حذف خطوط شبکه‌ای، `WS_EX_CLIENTEDGE` و حاشیه‌های اضافی برای ظاهری یکپارچه.
4. **بازچینش هدر**: دکمه‌های آبی در لایهٔ دوم، راست‌چین (نوبت‌دهی، پذیرش جدید،
   تب جدید) که تب باز می‌کنند.
5. **تب پیش‌فرض پذیرش** (`reception.cpp`): با ورود به پذیرش هیچ تب قبلی باز
   نمی‌شود؛ نخستین تب همان **کارتابل** (`TK_PORTAL`) است.
6. **رفع به‌هم‌ریختگی چیدمان پذیرش** (`reception.cpp`): متریک عمودی جدید با
   تضمین عدم هم‌پوشانی عنوان بخش/برچسب/ورودی روی همهٔ رزولوشن‌ها.
7. **بازسازی کامل پنل مدیریت به داشبورد** (`manage.inc`): ریل ناوبری راست +
   شش صفحهٔ مجزا (داشبورد، بخش‌ها، کارکنان، پیام به کارکنان، درخواست‌ها،
   پیام‌های ذخیره‌شده) با کارت‌های خلاصه و دسترسی سریع.
8. **صفحهٔ «ایجاد بخش»** (`manage.inc`): نمایش بخش‌های موجود + فرم افزودن با
   نام بخش، شناسهٔ بخش (خودکار به‌صورت پیش‌فرض + امکان دستی) و دکمهٔ افزودن.
9. **فهرست کارکنان با فیلترهای ترکیب‌پذیر** (`manage.inc`): مرتب‌سازی الفبایی/
   جدیدترین/زمان ساخت + فیلتر بخش که با مرتب‌سازی ترکیب می‌شود + دکمهٔ افزودن کارمند.
10. **فرم کارمند جدید** (`manage.inc`, `employees.cpp`, `app.h`): فیلدهای کامل
    شامل کد پرسنلی و شناسهٔ یکتا (هر دو خودکار به‌صورت پیش‌فرض)، شیفت، ساعات کاری و
    سایر جزئیات؛ ذخیرهٔ کامل `EmpProfile`.
11. **درخواست‌های تغییر تنظیمات/پروفایل به‌صورت دسته‌ای** (`manage.inc`): بدون رنگ
    قرمز؛ از رنگ شاخص متمایز (`g_infoAccent`) استفاده شد؛ اعلان‌ها بالای بخش و با
    خط جداکننده تفکیک شدند.
12. **پیام به کارکنان** (`manage.inc`, `employees.cpp`): جعبهٔ جستجوی زنده
    (کد/شناسه/نام)، فیلتر بخش‌ها با ترکیب چندگانه، آپلود رسانه (تصویر/ویدیو/سند) با
    کپی محلی و حفظ محتوا، و هدف‌گذاری ارسال (تک‌کاربر/بخش/همگانی).
13. **پیام‌های ذخیره‌شده** (`reception.cpp`, `settings.cpp`, `employees.cpp`,
    `app.h`, `manage.inc`): آیکن بایگانی در گوشهٔ بالا-چپ کارتابل، نمای پیام‌های
    آرشیوشده، گزینهٔ «ارسال به پیام‌های ذخیره‌شده» (پیش‌فرض غیرفعال/خاکستری) و
    کلید تنظیمات «پیام‌های ذخیره‌شده» (پیش‌فرض غیرفعال). داده‌ها به‌صورت محلی و
    دائمی با متن و پیوست قابل‌دانلود ذخیره می‌شوند.
14. **بیلد تازه**: خروجی قبلی پاک و با `build/AzadiTeb.exe` تازه جایگزین شد.

### 🔢 نسخه
- `APP_VERSION_W` در `app.h` و `app.rc` به **1.8.0** ارتقا یافت.

---

## v1.7.0 (build) — 2026-06-15 — بیلد تازه و همگام‌سازی با گیت‌هاب

- **بیلد تمیز و تازهٔ `build/AzadiTeb.exe`** از روی سورس کامل v1.7.0 با
  کراس‌کامپایلر MinGW-w64 i686 (پاک‌سازی کامل `build/` و `obj/` قبل از بیلد).
  خروجی یک EXE ایستای ۳۲ بیتی PE32 i386 GUI است که با درخت سورس کاملاً منطبق
  است (بدون خطا؛ فقط چند هشدار بی‌خطر indentation).
- **همگام‌سازی شاخه‌ها**: شاخهٔ `main` به نسخهٔ v1.7.0 رسانده شد و با
  `genspark_ai_developer` همگام شد (در گذشته `main` روی v1.6.0 مانده بود و
  این نسخه به آن مرج/پوش نشده بود).

---

## v1.7.0 — 1405/03/25 (2026-06-15) — بازطراحی هدر، کارتابل، تم، عملکرد و هویت واقعی

این نسخه هشت محور اصلاحی درخواست کاربر را پوشش می‌دهد.

### ✨ افزوده / تغییر / رفع
1. **هویت و بیمهٔ واقعی** (`data_ext.cpp`, `reception.cpp`, `appointment.cpp`):
   حذف کامل ساخت دادهٔ جعلی. کد ملی فقط با چک‌سام اعتبارسنجی و سپس **تنها** از منبع
   مورد اعتماد (وب‌سرویس ثبت‌احوال پیکربندی‌شده یا فهرست بیماران قبلاً تأییدشده)
   استعلام می‌شود؛ در غیر این صورت وضعیت «تأییدنشده» نمایش داده و ورود دستی فعال
   می‌شود. هیچ نام/تاریخ تولد/آدرس/بیمه‌ای حدس زده نمی‌شود.
2. **بازچینش هدر** (`main.cpp`, `reception.cpp`, `app.h`): دکمه‌های **«پذیرش جدید»**،
   **«نوبت‌دهی»** و **«تب جدید»** از نوار تب به **هدر** منتقل و از طریق
   `receptionAction()` مسیردهی شدند؛ نوار اطلاعات پذیرش از این دکمه‌ها پاک شد.
3. **مرتب‌سازی تب‌ها با درگ-و-دراپ** (`reception.cpp`): جابه‌جایی تب‌ها با کشیدن،
   نشانگر محل رها کردن، نشانگر ماوس IDC_SIZEWE و **ذخیرهٔ ترتیب** (loadTabOrder/
   saveTabOrder) برای ماندگاری.
4. **بازطراحی کارتابل** (`reception.cpp`): نمای **جزئیات پیام** (فرستنده/گیرنده،
   اولویت/وضعیت، تاریخ، ساعت، متن)، نشانگر ماوس دست روی کاشی‌ها، دکمه‌های
   **خواندن/علامت خوانده‌شده/حذف/بازگشت**، بازگشت با **Esc**، **راست‌کلیک فقط سنجاق**،
   آیکن سنجاق در **گوشهٔ بالا-راست** کاشی و اولویت پیام‌های سنجاق‌شده در بالا.
5. **اصلاح تم تیره/روشن** (`theme.cpp`): کمبوی Owner-draw اکنون متن انتخاب‌شده را در
   حالت جمع‌شده می‌کشد، فلش بازشوی تخت و هم‌رنگ تم دارد و حاشیهٔ تخت گردگوشه (با
   subclass) جای حاشیهٔ سه‌بعدی سفید سیستم را می‌گیرد.
6. **رفع هم‌پوشانی چیدمان** (`reception.cpp`): حذف برچسب تکراری «بیمهٔ اصلی» که زیر
   چک‌باکس «دارای بیمه» قرار می‌گرفت؛ چک‌باکس اکنون تمام عرض ستون را می‌گیرد و
   گلیف آن با لبهٔ راست کمبوی زیرین هم‌تراز است.
7. **رفع پرش/کندی پنجرهٔ تنظیمات** (`settings.cpp`): کش پس‌زمینه (Memory DC +
   بیت‌مپ) و **بازترسیم ناحیه‌ای** به‌جای Invalidate تمام‌صفحه در هر حرکت ماوس؛
   حرکت ماوس روان شد.
8. **خروجی ساخت**: پاک‌سازی پوشهٔ build و تولید مجدد `build/AzadiTeb.exe` هماهنگ با
   سورس به‌روزشده.

---

## v1.6.0 — 1405/03/23 (2026-06-13) — نوبت‌دهی، پروفایل با تأیید مدیر، کارتابل نسخهٔ ۲

این نسخه بر اساس درخواست‌های جدید کاربر تکمیل شد.

### ✨ افزوده / تغییر
1. **تب نوبت‌دهی** (`appointment.cpp`): به‌عنوان **اولین تب** با گروه‌های جستجو،
   جزئیات نوبت و جزئیات بیمار + جدول (DataGridView) فقط‌خواندنی راست‌به‌چپ.
2. **ماشین‌حساب** به **سمت چپ هدر** منتقل شد.
3. **چک‌باکس «دارای بیمه»** بالای کمبوی بیمه؛ تشخیص بیمهٔ دوم/سوم و محدودسازی کمبو.
4. **رفع باگ حذف تاریخ تولد** و **اصلاح متن کمبو در تم تیره** (Owner-draw).
5. **هم‌ترازی خودکار RTL/LTR** متن ورودی‌ها.
6. **پنل اطلاعات راست پذیرش** (`reception.cpp`): آواتار جنسیت، نسخه الکترونیک،
   قبض/بارکد، P:0 S:0، کلیدهای جستجو، بلوک بیمه، پزشک معالج، انجام‌دهنده.
7. **دیالوگ ویرایش پروفایل** (`dialogs.cpp`, `manage.inc`): تغییر نام/عکس با
   **تأیید مدیر** (کارتابل سبز/قرمز + دلیل اختیاری) و اعمال `name_override` هنگام ورود.
8. **کارتابل نسخهٔ ۲** (`reception.cpp`): کاشی‌های مستطیلی، پس‌زمینهٔ تیره، تاریخ،
   منوی راست‌کلیک سنجاق/خوانده‌شده/حذف و اعلان به مدیر.
9. **طراح چاپ** (`printer_designer.inc`): واگرد با Ctrl+Z، اسکرول صفحه با غلتک،
   جابه‌جایی (Pan) با درگ، PageUp/PageDown.
10. لایهٔ دادهٔ توسعه‌یافته (`data_ext.cpp`): شبیه‌سازی ثبت‌احوال، پزشکان، نوبت‌ها،
    درخواست‌های پروفایل و توابع کارتابل نسخهٔ ۲.
11. **تکمیل دکمه‌های نوبت‌دهی** (`appointment.cpp`): چاپ واقعی قبض نوبت با GDI
    (`printApptSlip`)، **ویرایش** نوبت (بارگذاری در فرم و به‌روزرسانی)، **انتقال
    نوبت** (تغییر تاریخ + اعلان به بیمار در کارتابل)، **F5** بازخوانی خدمات پزشک،
    **F4** افزودن خدمت دلخواه (دیالوگ تم‌دار)، **F3** پاک‌سازی انتخاب خدمت؛ ستون
    چاپ و دکمه‌های چاپ/انتقال نوار ابزار فعال شدند.
12. **جستجوی پزشک معالج** (`reception.cpp`): فهرست پزشکان بر اساس نام/تخصص در منوی
    بازشو و پرکردن نام + کد نظام پزشکی پایدار.
13. **آواتار عکس پروفایل** (`gdiplus.cpp`, `settings.cpp`): تابع
    `gpDrawImageFileCircle` برای نمایش عکس کاربر به‌صورت دایره‌ای در پنل تنظیمات
    (در صورت تنظیم `photo_<user>`).

---

## v1.5.0 — 1405/03/23 (2026-06-13) — رفع اشکالات گسترده + امکانات مدیریت و چاپ

این نسخه یک پاس کامل رفع‌باگ و افزودن امکانات بر اساس درخواست‌های کاربر است.

### ✨ افزوده / تغییر
1. **تم تیره واقعی** (`theme.cpp`, `admin.cpp`): پس‌زمینهٔ مشکی واقعی، آیکن چرخ‌دنده
   واضح، رنگ‌های تیرهٔ لیست/کمبو با متن سفید، حذف درخشش گوشهٔ دکمه‌ها.
2. **فرم پذیرش** (`reception.cpp`): رفع هم‌پوشانی آیتم‌ها، ارتفاع‌های متناسب و واکنش‌گرا،
   چک‌باکس «دارای بیمه» کنار کد ملی (به‌صورت پیش‌فرض تیک‌خورده)، استعلام با Enter و
   نمایش خطا در صورت نامعتبر بودن، لیست دستی بیمه در حالت بدون‌بیمه، دکمه‌های چاپ در
   **سمت چپ** با آیکن‌های تصویری واقعی، همگام‌سازی چاپ رسید/آخرین قبض (F8)/قبض جاری.
3. **ماشین‌حساب** (`calculator.cpp`): کنتراست بهتر کلیدها در تم روشن.
4. **کارتابل پیام‌دار** (`employees.cpp`, `app.h`): پیام‌های نوع‌دار
   (عادی=سبز / فوری=زرد / بحرانی=قرمز) با کارت‌های رنگی.
5. **پنل مدیریت** (`manage.inc`): شروع از دستهٔ «بخش‌ها»، دستهٔ پیش‌فرض **پذیرش**،
   قابلیت **«پیام به کارکنان»** (انتخاب کارمند/همه + شدت + متن، همگام با کارتابل)،
   بخش **«درخواست‌های تغییر تنظیمات کارکنان»** با نشان قرمز (چه‌کسی/سیستم/چه‌تغییری/پروفایل + تاریخ‌وساعت).
6. **طراح چاپ** (`printer.cpp`, `printer_designer.inc`): زوم روی نقطهٔ ماوس با غلتک،
   جابه‌جایی (Pan) بوم با درگ، رنگ زمینه و چینش متن برای هر عنصر، **۱۰ طرح چاپ مدرن
   ایرانی**، دکمهٔ «نمای اصلی»، طراحی فقط از منوی تنظیمات، طرح پیش‌فرض هر بخش.
7. **همگام‌سازی شبکه‌ای** (`util.cpp`): فایل `dataroot.ini` کنار EXE می‌تواند مسیر دادهٔ
   اشتراکی شبکه را تعیین کند تا طرح‌ها/پیام‌ها/درخواست‌ها بین همهٔ ترمینال‌ها زنده همگام شوند.
8. ثبت خودکار «درخواست تغییر تنظیمات» هنگام تغییر چاپگر/طرح توسط پذیرش.

---

## v1.3.0 — 1405/03/20 (2026-06-10) — بازطراحی کامل رابط کاربری (GDI+) + منوی تنظیمات

این نسخه یک بازطراحی گستردهٔ ظاهری و کاربری روی کل برنامه است، دقیقاً بر اساس
درخواست‌های کاربر. از **GDI+** برای رنگ‌بندی، سایه، لایه‌بندی و گرادیان استفاده شد
(بدون خروج از Win32 خالص؛ خروجی همچنان یک EXE تک‌فایلی استاتیک است).

### ✨ امکانات و بازطراحی جدید
1. **موتور گرافیکی GDI+** (`gdiplus.cpp`, `build.sh`):
   - گوشه‌های گرد آنتی‌آلیاس، گرادیان، سایه، روکش آلفا و دیکد JPEG برای پس‌زمینه‌ها.
   - لینک با `-lgdiplus -lole32 -luuid`.
   - **نکته مهم:** آلفای GDI+ روی memory DC زیر Wine درست بلند نمی‌شود؛ بنابراین
     برای پرکردن‌های قابل‌اعتماد از `gpGradRoundRect` (گرادیان آگاه از تم) استفاده شد.

2. **تصویر پس‌زمینه روی صفحهٔ خوش‌آمد** (`app.rc`, منابع ۱۰۳ روشن / ۱۰۴ تیره).

3. **منوی تنظیمات شبیه صفحهٔ پروفایل شبکهٔ اجتماعی** (`settings.cpp` — فایل جدید):
   پنل کشویی (slide-over) با آواتار حرف اول، هویت کاربر، و ۷ ردیف تنظیمات:
   - **سوییچ تم** (روشن/تیره) با اعمال آنی و همگام‌سازی آیکن دکمهٔ تم در هدر.
   - **بررسی به‌روزرسانی** (دکمهٔ آپدیت).
   - **تراکم نمایش** (عادی/فشرده) که در `g_scale` اعمال می‌شود.
   - **چاپ خودکار قبض** (auto_print) — وقتی روشن باشد، پس از ثبت پذیرش قبض بدون
     پرسش چاپ می‌شود (`reception.cpp` → `ID_F_SUBMIT`).
   - **آدرس سرور** (server_url) قابل ویرایش.
   - **درباره** و **خروج از حساب**.
   - پنل به‌صورت `WS_POPUP | WS_EX_TOPMOST` ساخته شد تا مشکل z-order با صفحات هم‌رده
     برطرف شود؛ پیش از عملیات GDI+ یک پس‌زمینهٔ مات (scrim/base) کشیده می‌شود تا
     مشکل رندر سیاه/سفید زیر Wine رفع شود.

4. **هدر سه‌لایه** (`main.cpp`):
   - **لایهٔ ۱:** نام برنامه + نام کاربر واردشده + نوع دسترسی + ساعت (وسط‌چین و
     توپر با فونت مونو و رنگ تأکید) + تاریخ (وسط‌چین). دیگر «نام‌کاربری» نمایش
     داده نمی‌شود.
   - **لایهٔ ۲:** دکمه‌های ماشین‌حساب + تب جدید + پذیرش جدید، چیده‌شده در سمت **راست**.
   - **لایهٔ ۳:** نوار تب‌ها.

5. **تب پیام پرتابل هنگام ورود + تب جدید خالی** (`reception.cpp`):
   - `enum TabKind { TK_RECEPTION, TK_PORTAL, TK_EMPTY }` + تابع `drawTabPlaceholder`.
   - پس از ورود، تب «پیام پرتابل» فعال است (جای پیام‌های مدیر در آینده).
   - دکمهٔ «تب جدید» یک تب خالی باز می‌کند.

### 🐞 رفع اشکال‌ها
6. **رفع به‌هم‌ریختگی تاریخ جلالی** (`util.cpp`): دور رشته‌های عددی روز/سال با
   کاراکتر RLM (U+200F) پیچیده شد تا ترتیب BiDi به‌هم نریزد (نمایش قبلی به‌صورت
   اشکال نامفهوم بود).

7. **ورود انعطاف‌پذیر تاریخ تولد** (`main.cpp`): پذیرش «۱۳۴۰ ۵ ۲۰» بدون صفر اضافه،
   با جداکننده‌های فاصله/اسلش/خط‌تیره؛ کلمپ ماه ≤ ۱۲ و روز ≤ ۳۱
   (`splitJalaliTokens`).

8. **رفع همپوشانی آیکن هویت بیمار با متن نام** (`reception.cpp`): آیکن چسبیده به
   راست با فاصله، و جابه‌جایی عنوان بخش‌ها (`y0 = S(118)`) تا از جداکنندهٔ هدر عبور کند.

9. **تم روشن سفید اما نه تخت**؛ رفع نشت رنگ و همپوشانی اشکال؛ چیدمان واکنش‌گرا
   (responsive) در اندازه‌های مختلف تأیید شد (۱۲۸۰×۷۲۰).

### 📄 فایل‌های تغییر یافته
- `src/gdiplus.cpp` (جدید) — توابع کمکی GDI+
- `src/settings.cpp` (جدید) — پنل تنظیمات
- `src/main.cpp` — هدر سه‌لایه، ماسک تاریخ منعطف، گرادیان hero، تراکم نمایش، هندلر `WM_APP_THEME`
- `src/reception.cpp` — TabKind، تب پرتابل/خالی، رفع همپوشانی، چاپ خودکار
- `src/util.cpp` — رفع تاریخ جلالی با RLM
- `src/app.h` — نسخه ۱.۳.۰ + پروتوتایپ‌ها
- `src/app.rc` — تصاویر پس‌زمینه ۱۰۳/۱۰۴
- `build.sh` — افزودن gdiplus.cpp + settings.cpp و کتابخانه‌ها
- `update/version.txt` — ارتقا به ۱.۳.۰

---

## v1.2.0 — 1405/03/20 (2026-06-10) — بازطراحی کامل صفحهٔ پذیرش + صدور خودکار قبض

این نسخه دقیقاً بر اساس درخواست‌های کاربر (منشی درمانگاه) بازطراحی شد.

### ✨ امکانات جدید
1. **صدور خودکار قبض (محاسبهٔ خودکار مبلغ)** (`billing.cpp`, `reception.cpp`):
   دیگر نیازی به وارد کردن دستی مبلغ نیست. برنامه بر اساس **نوع بیمار**
   (عادی / سرپایی / بستری) و **نوع نوبت** (عادی / VIP / تخفیف‌دار) تعرفهٔ
   پیش‌فرض را خودش پر می‌کند.
   - جدول تعرفه: `VISIT_TARIFF[3] = {۲٬۵۰۰٬۰۰۰ ، ۳٬۵۰۰٬۰۰۰ ، ۸٬۰۰۰٬۰۰۰}` ریال
   - `applyApptTariff()`: VIP = ۱۵۰٪ ، تخفیف‌دار = ۵۰٪ ، عادی = ۱۰۰٪
   - `defaultServicePrice(patientType, apptType)`: تابع نهایی محاسبه
   - در `recalc()` اگر مبلغ خدمت ≤ ۰ باشد به‌صورت خودکار پر می‌شود
     (با گارد `autoPrice` برای جلوگیری از حلقهٔ بی‌نهایت EN_CHANGE).
   - تغییر «نوع بیمار» یا «نوع نوبت» مبلغ را پاک کرده و دوباره خودکار محاسبه می‌کند.

2. **فیلد تاریخ هوشمند (ماسک جلالی YYYY/MM/DD)** (`main.cpp`):
   کاربر فقط روی فیلد تاریخ تولد کلیک می‌کند و **عدد** می‌زند؛ برنامه خودش
   اسلش‌ها را در جای درست قرار می‌دهد و عدد را به سال/ماه/روز تقسیم می‌کند.
   - `digitsOnly()` ارقام فارسی/عربی را به انگلیسی نرمال می‌کند
   - `formatJalaliMask()` اسلش را در موقعیت ۴ و ۶ می‌گذارد (حداکثر ۸ رقم)
   - `dateEditProc` / `enableDateMask()`: subclass روی کنترل EDIT
   - Backspace ارقام را درست (با رد شدن از اسلش‌ها) حذف می‌کند.
   - **تأیید شد**: تایپ `14010320` → نمایش `1401/03/20`.

3. **ناوبری با Enter و Tab (هر دو به فیلد بعدی)** (`main.cpp`):
   منشی‌ها بیشتر با Enter کار می‌کنند، پس **هر دو کلید** به فیلد بعدی می‌روند
   (Shift+Tab → قبلی). صدای بوق آزاردهنده هم حذف شد.
   - `hopField()` با `GetNextDlgTabItem` روی خود صفحهٔ پذیرش
   - `enterEditProc`: مدیریت VK_RETURN + VK_TAB + کشتن بوق در WM_CHAR
   - ترتیب فیلدها به‌صورت منطقی و پشت‌سرهم تنظیم شد.

4. **پذیرش جدید در همان تب** (`reception.cpp`):
   دکمهٔ «پذیرش جدید» دیگر تب جدید باز **نمی‌کند**؛ فرم تب فعلی را پاک کرده و
   آمادهٔ پذیرش بیمار بعدی می‌شود (`resetForm()` → پاک‌سازی ۸ فیلد، ریست
   کمبوها، فوکوس روی فیلد اول). برای باز کردن تب واقعاً جدید، دکمهٔ جداگانهٔ
   «تب جدید» اضافه شد. **تأیید شد**: کلیک «پذیرش جدید» فرم را ریست می‌کند و
   تعداد تب‌ها ثابت می‌ماند.

### 🎨 بازطراحی کامل رابط کاربری (شکایت اصلی کاربر)
- **چیدمان نوار بالا اصلاح شد** (`main.cpp` — `frameLayout` + `WM_PAINT`):
  نام و لوگوی برنامه به سمت **راست** منتقل شد (کنار دکمهٔ بستن). دکمه‌های
  «تغییر تم» و «به‌روزرسانی» به سمت **چپ** منتقل شدند تا دیگر کنار دکمهٔ
  بستن (✕) نباشند و اشتباهی زده نشوند.
- **صفحهٔ پذیرش کارت‌محور و تمیز شد** (`reception.cpp` — بازنویسی کامل
  `rcMetrics` / `rcVMetrics` / `tabPageLayout` / `WM_PAINT`):
  - کارت «مشخصات و پذیرش بیمار» (سمت راست) با عنوان و آیکون کاربر
  - **بخش‌بندی** با عنوان و آیکون برداری: هویت بیمار، اطلاعات تماس، نوع پذیرش، …
  - **قاب (input well) دور هر فیلد** با حاشیهٔ رنگی روی فیلد فوکوس‌شده
  - کارت «صدور قبض» (سمت چپ، عرض ۳۴۰px) با ردیف‌های کلید/مقدار و
    **چیپ سبز «پرداختی»** برای مبلغ نهایی
- **آیکون‌های برداری جای ایموجی** (`theme.cpp`): چون فونت وزیرمتن گلیف ایموجی
  رنگی ندارد، تمام ایموجی‌ها (👤📞📋💰🧾📅) با آیکون برداری GDI جایگزین شدند:
  `ICO_ID, ICO_PHONE, ICO_CAL, ICO_PIN, ICO_RECEIPT, ICO_CLOCK, ICO_REFRESH`.

### 🌗 بازطراحی پالت رنگ (بدون تداخل رنگی)
- **تم تیره** مدرن و عمیق: `bg=RGB(13,17,23)`، `surface=RGB(22,27,34)`،
  `border=RGB(48,54,66)`، `accent=RGB(56,170,255)`. هیچ رنگی نزدیک به رنگ
  پس‌زمینه استفاده نشده (رفع تداخل رنگی موردِ شکایت کاربر).
- **تم روشن** (پیش‌فرض): `bg=RGB(240,243,248)`، `surface=سفید`،
  `accent=RGB(37,99,235)` (نیلی). هر دو تم با اسکرین‌شات روی Wine تأیید شدند.

### فایل‌های تغییریافته
`src/{theme.cpp, app.h, main.cpp, billing.cpp, reception.cpp}` +
`update/version.txt` (← 1.2.0) + `docs/{CHANGELOG.md, PROJECT_GUIDE.md, PROMPT.md}`
+ `README.md` + بکاپ سورس جدید در `backup/`

### تأیید بصری (Wine + Xvfb)
صفحهٔ خانه، صفحهٔ پذیرش (روشن و تیره)، ماسک تاریخ هوشمند، و «پذیرش جدید در
همان تب» همگی با اسکرین‌شات headless تست و تأیید شدند.

---

## v1.1.0 — 1405/03/20 (2026-06-10) — رفع کرش‌های بحرانی + Crash Handler حرفه‌ای

### 🔴 رفع شد (باگ‌های کشنده — علت اصلی «سریع کرش می‌خوره»)
1. **نشت WM_QUIT از دیالوگ‌ها → بسته‌شدن ناگهانی کل برنامه** (`dialogs.cpp`):
   دیالوگ‌های لاگین و انتخاب شیفت در `WM_DESTROY` تابع `PostQuitMessage(0)`
   را صدا می‌زدند. اما حلقه `runModal` قبل از مصرف آن WM_QUIT، با چک
   `IsWindow()` خارج می‌شد → پیام WM_QUIT در صف می‌ماند و وارد حلقه پیام
   **اصلی** برنامه می‌شد → برنامه بلافاصله بعد از هر لاگین/انصراف خودش را
   می‌بست (شبیه کرش). حالا `PostQuitMessage` حذف شد؛ `runModal` با
   `IsWindow()` خارج می‌شود و اگر WM_QUIT واقعی برسد آن را re-post می‌کند.
2. **Use-after-free دکمه تم** (`main.cpp` + `theme.cpp`): کلیک روی دکمه
   تغییر تم، همان دکمه را وسط handler خودش `DestroyWindow` می‌کرد →
   بازگشت به state آزادشده. حالا تابع جدید `setFlatButtonIcon` آیکون را
   درجا عوض می‌کند و کلیک دکمه‌ها با `PostMessage` (نه SendMessage) ارسال
   می‌شود تا handler هرگز روی پنجره مرده برنگردد.
3. **کرش بستن/جداکردن تب** (`reception.cpp`): مسیر re-attach تب جداشده به
   پنجره‌ای اشاره می‌کرد که ممکن بود از بین رفته باشد؛ لینک
   `GWLP_USERDATA` قبل از reparent قطع نمی‌شد → double-destroy. بازنویسی
   کامل `WM_CLOSE` پنجره جداشده + بررسی `IsWindow` + پاک‌سازی امن.

### 🛡️ Crash Handler کاملاً بازسازی شد (`handlers.cpp`)
- صفر تخصیص حافظه heap داخل مسیر کرش (بافر استاتیک + WinAPI خام) —
  handler قبلی خودش از `std::wstring` و `logLine` استفاده می‌کرد که وسط
  کرش heap خراب، خودش هم کرش می‌کرد!
- پوشش کامل: SEH + `std::terminate` + سیگنال‌های SIGSEGV/SIGABRT/SIGILL/SIGFPE
- گارد ضد-بازگشت (کرش داخل خود handler → خاتمه امن، نه حلقه بی‌نهایت)
- گزارش کامل: نام exception، آدرس، رجیسترها، تعداد هسته CPU، رم کل/آزاد
- دکمه «اجرای مجدد خودکار» بعد از کرش

### ✅ پایدارسازی سراسری
- گارد NULL برای pointer داده‌ها در همه message handler ها
  (`reception.cpp`, `admin.cpp`, `calculator.cpp`, `dialogs.cpp`)
- گارد محدوده ایندکس بیمه‌ها (`recalc`, `collect`)
- گارد ابعاد صفر در WM_PAINT (مینیمایز → CreateCompatibleBitmap(0,0) خطا می‌داد)
- گارد re-entry برای دیالوگ‌های مودال (دابل-کلیک سریع → دو دیالوگ تو در تو)
- فرم پذیرش ریسپانسیو عمودی شد: روی مانیتورهای کوتاه، فاصله سطرها خودکار
  فشرده می‌شود تا هیچ فیلدی بیرون صفحه نیفتد (`rcVMetrics`)
- تم تیره ListView ادمین حالا با سوییچ تم درجا آپدیت می‌شود (`WM_APP_THEME`)
- broadcastThemeChange حالا پنجره‌های top-level خودمان (ماشین‌حساب، تب جدا)
  را هم رفرش می‌کند

### فایل‌های تغییریافته
`src/{dialogs.cpp, handlers.cpp, main.cpp, theme.cpp, reception.cpp,
admin.cpp, calculator.cpp, app.h, app.rc}` + `update/version.txt` (← 1.1.0)

---

## v1.0.1 — 1405/03/20 (2026-06-10) — بازسازی کامل UI (رفع هم‌پوشانی و باگ لاگین)

### رفع شد
- **باگ بحرانی لاگین** (`dialogs.cpp` — بازنویسی کامل): دیالوگ‌های لاگین و
  انتخاب شیفت قبلاً پنجره فرزند (WS_CHILD) بودند و با غیرفعال‌شدن پنجره والد،
  خودشان هم غیرفعال/نامرئی می‌شدند → با کلیک روی «پذیرش درمانگاه» هیچ‌چیز
  نمایش داده نمی‌شد. حالا دیالوگ‌ها پنجره مستقل Owned Popup هستند که دقیقاً
  روی پنجره اصلی قرار می‌گیرند؛ Tab/Enter/Escape کامل کار می‌کند؛ کادرهای
  ورودی گرد و مدرن دور EDITها رسم می‌شود؛ وضعیت تیک «شیفت خودکار» ذخیره می‌شود.
- **آینه‌شدن/خراب‌شدن گرافیک RTL** (`reception.cpp`, `admin.cpp`, `main.cpp`):
  استایل `WS_EX_LAYOUTRTL` همراه با Double-Buffering دستی (BitBlt) باعث
  برعکس‌شدن و قاطی‌شدن همه متن‌ها و عناصر می‌شد. این استایل از همه پنجره‌های
  دارای رسم سفارشی حذف شد و چینش راست‌به‌چپ به‌صورت دستی با مختصات صریح
  پیاده شد (کارت صدور قبض چسبیده به راست، ستون اولِ فرم سمت راست، تب‌ها از
  راست به چپ، دکمه‌های ماشین‌حساب/پذیرش جدید سمت چپ نوار اطلاعات).
  فقط ListView جدول کاربران ادمین (که رسم سفارشی ندارد) RTL سیستمی ماند.
- **هم‌پوشانی صفحه اصلی** (`main.cpp`): کارت‌های «پذیرش درمانگاه» و
  «پنل مدیریت» روی متن «آزادی طب» می‌افتادند چون مختصات WM_PAINT و WM_SIZE
  جداگانه محاسبه می‌شد. حالا هر دو از یک پشته عمودی واحد استفاده می‌کنند:
  لوگو(۸۸) ← عنوان(۴۴) ← زیرعنوان(۲۸) ← فاصله(۳۶) ← کارت‌ها(۱۷۰) — همگی
  وسط‌چین عمودی و بدون هیچ هم‌پوشانی روی هر اندازه مانیتور.

### فایل‌های تغییریافته
`src/{dialogs.cpp (بازنویسی), main.cpp, reception.cpp, admin.cpp, app.h, app.rc}`
+ `update/version.txt` (نسخه ← 1.0.1)

---

## v1.0.0 — 1405/03/20 (2026-06-10) — انتشار اولیه

### ساخته شد (همه‌چیز از صفر)
- **هسته برنامه** (`main.cpp`): پنجره تمام‌صفحه بدون نوار عنوان/منو (WS_POPUP)،
  نوار بالا (دکمه خروج بالا-راست + تغییر تم + آپدیت)، نوار پایین با ساعت زنده
  ایران (دقت ثانیه) و تاریخ کامل جلالی پایین-راست، مقیاس ریسپانسیو بر اساس DPI
  و ارتفاع مانیتور، تک‌نمونه (single instance)، روتینگ کلیدهای سراسری.
- **Crash Handler** (`handlers.cpp`): گزارش خطای کامل با رجیسترها در
  `logs/crash_*.log` + پیام فارسی دوستانه.
- **Speed Handler** (`handlers.cpp`): تشخیص سخت‌افزار ضعیف (≤۲ هسته یا ≤۲.۲GB رم)
  → تایمر کندتر، کیفیت فونت ساده‌تر؛ تضمین کارکرد روان روی رم ۲ و CPU دو هسته.
- **فونت وزیر** (`handlers.cpp` + `app.rc`): Vazirmatn Regular/Bold داخل EXE
  تعبیه شد؛ در هر اجرا با AddFontMemResourceEx لود می‌شود و اگر روی سیستم نصب
  نباشد، خودکار برای کاربر جاری نصب می‌گردد (بدون نیاز به ادمین).
- **تم روشن + تیره** (`theme.cpp`): پالت‌های بدون تداخل رنگ، دکمه Flat سفارشی
  با ۵ استایل، آیکون‌های وکتوری GDI (بدون فایل تصویری)، ذخیره تم انتخابی.
- **زمان ایران** (`util.cpp`): UTC+3:30 ثابت، تبدیل دقیق میلادی→جلالی،
  اعداد فارسی، نام روز/ماه فارسی.
- **صفحه اصلی**: دو کارت «پذیرش درمانگاه» و «پنل مدیریت درمانگاه» با آیکون و لیبل.
- **پنل مخفی ادمین** (`admin.cpp`): فعال‌سازی با نگه داشتن Ctrl+P+N در صفحه
  اصلی؛ ورود با prf/prf123؛ ساخت کاربر (نام شخص، نام کاربری، رمز، بخش تایپی
  مثل «دندانپزشکی»، نوع دسترسی پذیرش/مدیریت)؛ جدول کاربران + حذف؛
  خطای یوزر تکراری؛ رمز اشتباه = عدم ورود.
- **لاگین سبک ویندوز ۱۱** (`dialogs.cpp`): کارت گرد وسط صفحه با لایه dim،
  یوزرنیم بالا و پسورد پایین، انیمیشن خطا — حتی روی ویندوز ۷ همین ظاهر را دارد.
- **انتخاب شیفت** (`dialogs.cpp`): صبح ۶–۱۴:۳۰ / عصر ۱۴:۳۰–۲۲:۳۰ / شب ۲۲:۳۰–۶؛
  پیش‌فرض حالت خودکار با غیرفعال‌شدن دکمه‌ها؛ تیک خودکار **به خاطر سپرده می‌شود**؛
  سشن کاربر با عبور ساعت از مرز شیفت قطع نمی‌شود (فقط خروج دستی).
- **فضای پذیرش با تب مرورگری** (`reception.cpp`): تب با نام «پذیرش + بخش»
  (مثل پذیرش دندانپزشکی)، باز/بستن/جدا کردن تب به پنجره مستقل و برگشت آن؛
  نوار اطلاعات: کاربر جاری، دکمه ماشین حساب، نوع دسترسی، تاریخ و ساعت ایران.
- **فرم پذیرش بیمار**: نام، نام خانوادگی، کد ملی، نام پدر، تاریخ تولد، جنسیت،
  تلفن، ثابت، آدرس، نوع بیمار، بیمه، بیمه مکمل، نوع نوبت (عادی/اورژانس/پرسنلی)؛
  تاریخ نوبت خودکارِ لحظه ثبت؛ شیفت نوبت؛ حرکت بین فیلدها با Enter.
- **صدور قبض با بیمه‌های واقعی ایران** (`billing.cpp`): تأمین اجتماعی، سلامت
  (ایرانیان/روستایی/کارکنان دولت)، نیروهای مسلح، کمیته امداد + ۹ بیمه مکمل؛
  محاسبه زنده: بیمه اصلی، جمع کل، سهم بیمار، مابه‌التفاوت پایه، سهم سازمان،
  تخفیف، پرداختی؛ ذخیره در CSV روزانه با شماره نوبت.
- **چاپ واقعی** (`billing.cpp`): رسید بیمه / چاپ نسخه / چاپ آخرین قبض روی
  پرینتر متصل (GDI + PrintDlg)؛ کلید F8 = چاپ آخرین قبض از همه‌جا.
- **ماشین حساب خاص** (`calculator.cpp`): پنجره Always-on-Top (هیچ‌وقت پشت
  برنامه نمی‌رود)، UI گرد مدرن هماهنگ با تم، کیبورد + Numpad + ماوس،
  عملیات کامل استاندارد + درصد/جذر/توان/معکوس، جداکننده هزارگان فارسی.
- **آپدیت از راه دور** (`update.cpp`): بررسی version.txt از سرور
  (پیش‌فرض raw گیت‌هاب همین مخزن — قابل تغییر با update_url)، دانلود EXE جدید
  و جایگزینی خودکار با اسکریپت پس از خروج.
- **بیلد** (`build.sh`): کراس‌کامپایل i686 استاتیک → یک EXE واحد (~۸۰۰KB)
  برای x86+x64، ویندوز ۷/۸/۸.۱/۱۰/۱۱+.

### فایل‌ها
`src/{app.h, main.cpp, util.cpp, handlers.cpp, theme.cpp, users.cpp,
billing.cpp, calculator.cpp, dialogs.cpp, admin.cpp, reception.cpp,
update.cpp, app.rc, app.manifest}` + `build.sh` + `fonts/` + `docs/` + `update/`
