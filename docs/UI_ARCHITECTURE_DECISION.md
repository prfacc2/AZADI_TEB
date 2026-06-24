# UI Architecture Decision: Native GDI/GDI+ vs Hybrid HTML/CSS/JS (v1.12.0)

Work-order §3/§4 asks for a frontend decision for the reception + appointment
screens, explicitly allowing **"the best engineering choice; a native refactor
is acceptable."** This document records the decision and its rationale.

## Decision

**Keep and refine the native GDI/GDI+ frontend. Do NOT embed an HTML/CSS/JS
(WebView2 / CEF) frontend.**

## Why a hybrid HTML frontend was rejected

The product's hard, non-negotiable constraints make a web frontend the *wrong*
engineering choice here:

1. **Single static 32-bit PE32 EXE, no DLLs.** The whole app ships as one
   `AzadiTeb.exe` built with `-static -static-libgcc -static-libstdc++`. A
   WebView2 frontend requires the **WebView2 Runtime / Edge** to be installed
   and reachable, plus shipping the loader DLL — that breaks the
   "one self-contained EXE" guarantee.
2. **Windows 7 → 11, x86 and x64.** WebView2 is not guaranteed on older Windows
   7/8.1 clinic machines; CEF would balloon the binary into hundreds of MB and
   still need separate runtime files. The native GDI path runs identically and
   instantly on every target.
3. **RTL Persian + embedded Vazirmatn font.** The native renderer already does
   precise manual RTL layout (`DT_RTLREADING`, no `WS_EX_LAYOUTRTL`) and embeds
   the font in the resource — no system font dependency, no browser font
   fallback surprises.
4. **Offline, air-gapped clinics.** Many clinic PCs have no internet. A native
   renderer has zero runtime download/update surface.
5. **Crash containment & determinism.** The native path is covered by the
   process-wide crash handler (breadcrumbs + `module+offset` + minidump). A web
   layer would add an opaque JS runtime that the C++ crash handler cannot
   introspect.

A hybrid frontend would trade away portability, size, and determinism for
styling convenience the native renderer already provides (GDI+ rounded cards,
gradients, vector icons, double-buffered `MemDC`, a theme palette, the `S()`
responsive scale).

## What "native refactor" means in practice (and what shipped in 1.12.0)

The native frontend is treated as a proper view layer with a single source of
truth for layout, exactly as a web layout engine would enforce:

* **Single-source-of-truth geometry.** `rcVMetrics` / `rcMetrics2` compute the
  reception form geometry once; **both** the painter (`WM_PAINT`) and the
  control positioner (`tabPageLayout` → `MoveWindow`) consume the same
  `y0/step/rh`, so painted labels and real HWND controls can never drift apart
  (this is the fix for the "blue separator labels covered by text boxes" bug —
  the caption-vs-well clearance is now an enforced, strictly-positive invariant
  `step >= rh + S(52)`).
* **Reception header** centres the clock/date in the band, with a reduced
  header height (`mainBarH` S(64)→S(56)) and a slimmer, better-spaced tab strip
  (`tabBarH` S(40)→S(38), explicit `tabGap()`).
* **Management dashboard** is a structured CRM surface: a left nav rail, summary
  cards, quick-action tiles, and (1.12.0) an additive "at-a-glance" activity
  panel (patient roster, today's appointments, workload bar) — all read-only,
  sourced from the existing file-backed stores.
* **Settings** open as a separate full work-area page with push/pop sub-page
  navigation and full vertical scrolling (wheel + scrollbar + keyboard) in both
  reception and management modes.

## Appointment screen

The appointment screen (`src/appointment.cpp`) already shares the same data
repository and the **national-ID Enter lookup** (`nidProc` → `lookupCitizen`),
so an imported patient is auto-filled there too. It uses the same GDI+ grid
painter and theme. No web layer is warranted for the same reasons above.

## Conclusion

The native GDI/GDI+ renderer is the correct frontend for a single-EXE, fully
static, RTL, offline-capable Win7–11 clinical app. 1.12.0 invests in making it a
disciplined view layer (single-source-of-truth geometry, enforced spacing
invariants, CRM dashboard, full-page scrollable settings) rather than bolting on
a heavyweight web runtime that would violate the product's core constraints.
