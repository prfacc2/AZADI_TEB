# تاریخچه تغییرات (CHANGELOG)

> قانون: هر تغییری در کد، باید یک ورودی جدید در این فایل ثبت کند:
> نسخه، تاریخ، چه چیزی اضافه/تغییر/رفع شد، و در کدام فایل‌ها.

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
