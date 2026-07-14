# Azadi-Teb v2.0.0 — Avalonia UI (.NET 9) + C++ Core (معماری جدید)

> این سند تصمیم معماری نهایی نسخهٔ ۲ را ثبت می‌کند و جایگزین تمام تصمیم‌های
> قبلی (Win32/GDI و MSHTML) برای **لایهٔ نمایش** می‌شود. هستهٔ منطق کسب‌وکار
> در C++ حفظ و به یک سرویس REST مستقل تبدیل شده است.

## چرا بازنویسی لایهٔ نمایش؟

نسخه‌های قبلی بین دو رویکرد نوسان داشتند:

1. **Win32/GDI خالص** — کنترل‌های دستی با مختصات `x/y`، محاسبهٔ چیدمان دستی،
   و در فرم پذیرش دچار پیچیدگی و هنگ‌کردن هنگام افزودن خدمات می‌شد.
2. **MSHTML/WebBrowser** — رندر HTML داخل برنامه که وابسته به Trident و مستعد
   فریز روی سخت‌افزار اپراتور بود.

راه‌حل نهایی: **Avalonia UI با الگوی MVVM** برای یک UI کاملاً Data-Binding،
Responsive، RTL و زیبا — بدون هیچ کنترل دستی مختصاتی.

## نمای کلی

```
Presentation Layer      Avalonia UI (.NET 9) — MVVM
        │
        │  REST / JSON (System.Text.Json  ↔  nlohmann::json)
        ▼
C++ Core Engine         Business Logic (BillingService, Repository, DI)
        │
        ▼
SQL Server (Production) / SQLite / In-Memory (Test)
```

Backend هیچ اطلاعی از UI ندارد؛ ارتباط فقط از طریق REST است:

```
Avalonia  ──POST /api/patient/search──▶  C++ Backend  ──▶  SQL Server
```

## ساختار مخزن

```
AZADI_TEB/
├── app/                       # لایهٔ نمایش .NET 9
│   ├── AzadiTeb.sln
│   └── AzadiTeb.UI/
│       ├── Models/            # مدل‌های دامنه (Patient, ServiceItem, ...)
│       ├── Services/          # IReceptionBackend + REST/Local + Persian tools
│       ├── ViewModels/        # MVVM (ReceptionViewModel, MainWindowViewModel)
│       ├── Views/             # XAML (ReceptionView, MainWindow)
│       ├── Styles/            # توکن‌های رنگ + تم کنترل‌ها (Light/Dark, RTL)
│       ├── Converters/        # مبدل‌های Binding
│       └── Assets/            # فونت وزیرمتن، آیکون
│
├── backend/                   # هستهٔ C++20 (REST)
│   ├── CMakeLists.txt
│   ├── include/azaditeb/      # Models, IRepository, BillingService, Json
│   ├── src/                   # BillingService, MemoryRepository, main (REST)
│   ├── tests/                 # تست واحد محاسبات قبض
│   └── third_party/           # cpp-httplib + nlohmann/json (vendored)
│
├── build/                     # خروجی نهایی: AzadiTeb.exe (single-file, self-contained)
└── src/                       # کد نسخهٔ ۱ (Win32) — نگه‌داشته شده به‌عنوان مرجع
```

## الگوهای طراحی استفاده‌شده

| الگو | محل |
|---|---|
| MVVM | `ViewModels/*` + `ViewLocator` (نگاشت قراردادی VM→View) |
| Repository | `IPatientRepository`, `IReferenceRepository` (C++) |
| Service Layer | `BillingService` (C++) — منبع واحد حقیقت محاسبهٔ قبض |
| Dependency Injection | ترکیب در `main.cpp` (C++) و `ServiceLocator` (UI) |
| Strategy / Fallback | `RestReceptionBackend` با fallback خودکار به `LocalReceptionBackend` |
| Observer | `INotificationService` (توست غیرمسدودکننده) + `ObservableObject` |
| Command | `[RelayCommand]` از CommunityToolkit.Mvvm |

## اصول UI

- **هیچ کنترل دستی با مختصات `x/y` ساخته نمی‌شود.** تمام چیدمان با
  `Grid`, `StackPanel`, `WrapPanel`, `DockPanel` و Data-Binding است.
- **RTL کامل** با `FlowDirection=RightToLeft` و فونت وزیرمتن embedded.
- **Responsive**: `WrapPanel` فیلدها را در عرض کم به تک‌ستونی می‌ریزد؛
  `ScrollViewer` پیمایش نرم می‌دهد؛ پنل قبض چسبان است.
- **Dark/Light Theme** با `ThemeDictionaries` و سوییچ زندهٔ runtime.
- **Async کامل**: هر I/O (استعلام، محاسبه، ثبت) `async` و Cancellable است؛
  هیچ Thread رابط کاربری بلوکه نمی‌شود ⇒ نه هنگ، نه کرش.

## ضدفریز / ضدکرش

- تمام فراخوانی‌های backend `async` با `CancellationToken` هستند.
- محاسبهٔ قبض هنگام تایپ سریع، درخواست قبلی را Cancel می‌کند (debounce).
- کلاینت REST با `Timeout=4s` سریع Fail و به موتور محلی fallback می‌کند.
- `Program.Main` تمام استثناها را در `logs/crash.log` ثبت می‌کند.

## بیلد

```bash
# لایهٔ نمایش (خروجی: build/AzadiTeb.exe)
./build.sh

# به‌همراه هستهٔ C++ و اجرای تست‌ها
./build.sh core
```

## نقاط توسعهٔ آینده (طبق نقشهٔ راه)

- هوش مصنوعی، PACS/DICOM، HL7/FHIR، پیامک، واتساپ، تلگرام، پرداخت،
  نسخهٔ تحت وب و موبایل — همگی از طریق افزودن Endpoint در هستهٔ C++ و یک
  `ExternalServiceManager` بدون تغییر لایهٔ UI قابل افزودن‌اند.
