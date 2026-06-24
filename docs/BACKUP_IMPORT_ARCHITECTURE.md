# Backup Analysis & Patient-Import Architecture (v1.12.0)

This document is the single engineering reference for §10–§15 of the 1.12.0
work order: how Azadi-Teb reads a **real SQL Server `.bak`**, how a patient
import is staged and reconciled, and how the data layer is shaped so it can move
to a network server / REST service later **without rewriting the UI**.

---

## 1. SQL Server `.bak` reality (§10)

SQL Server *native* backups are **Microsoft Tape Format (MTF)** streams — they
are NOT zip/SQLite/plain SQL. A typical clinic full backup is large
(observed ~13.7 GB). The application therefore **never loads the whole file**.

### Layout of an MTF `.bak`

```
TAPE  ── media header        (software/vendor, media label, unicode flag)
 └ SSET ── backup set         (database name, set description, backup type)
    └ VOLB ── volume          (server/machine name)
       └ DBDB/FILE ──         (the embedded .mdf / .ldf database file list)
          └ … data blocks …   (the multi-GB page stream — NOT parsed)
       └ ESET / EOTM ──       (end-of-set / end-of-tape markers)
```

### What the analyzer extracts (and where)

| Field | DBLK | Surfaced as |
|-------|------|-------------|
| Database name | SSET / DBDB | `BkAnalysis.dbName`, report section |
| Server / machine name | VOLB / SSET | report section |
| Backup software / vendor | TAPE | report section |
| Media / set label | TAPE / SSET | report section |
| Backup type (Full/Diff/Log) | SSET | report section |
| Start / finish time | SSET | report section |
| Embedded DB file names | DBDB / FILE | first ≤32 names |
| DB compatibility level | DBDB | when present |

Implementation: `src/backup_mtf.{h,cpp}` (vendored, dependency-free, fully
bounds-checked `SpanReader`) + `src/backup_analyzer.cpp` (reads only the leading
~4 MiB descriptor region, auto-detects type by magic bytes, wraps the parse in
**VEH + `thread_local setjmp/longjmp`** containment so a corrupt descriptor can
never crash the process). The public API is `analyzeBackupFile()` in
`src/backup_analyzer.h`; it **never throws** and returns a Persian error on
failure.

> **Why VEH + setjmp and not `__try/__except`?** MinGW GCC (win32 i686) does not
> support MS structured exception handling for arbitrary C++ code (it errors
> with `expected 'catch' before '__except'`). The codebase standardises on
> `AddVectoredExceptionHandler` + a `thread_local jmp_buf` that the VEH handler
> `longjmp`s back to. See `azAnalyzeVeh` / `analyzeSeh` in
> `backup_analyzer.cpp` and the identical pattern in `print_designer_ui.inc`.

### Table / column names

The leading descriptor region of an MTF `.bak` carries **database-level**
metadata (database name, file list, server, timestamps) — it does **not** carry
the relational catalog (table/column names live inside the page data far beyond
the descriptor region). Extracting `sys.tables` / `sys.columns` requires the
database to be **restored into a live SQL Server instance** first. That is
**Path A** below. The offline path (**Path B**) therefore consumes a
**staged export** (CSV) produced from the restored database.

---

## 2. Two import paths (§11–§12)

```
                ┌─────────────────────────────────────────────┐
                │            patient import (§11)             │
                └─────────────────────────────────────────────┘
   Path A (SQL-Server-assisted)            Path B (offline staged)  ← shipped
   ───────────────────────────            ────────────────────────
   1. RESTORE .bak into SQL Server         1. Operator exports patients to a
   2. SELECT national_id, first,              CSV/TSV from the restored DB (or
      last, father, gender, birth,            from any other source) — one row
      mobile, insurance FROM dbo.Patients     per patient, with a national code
   3. Export the result to CSV             2. App parses + imports the CSV with
   4. Feed the CSV into Path B                national-ID dedup (no SQL needed)
```

**Path B is implemented and shipped** because it has zero external dependency
(no ODBC, no live SQL Server, runs on any clinic PC). Path A is a documented
operational procedure that **terminates in Path B** — the same import core
ingests its CSV, so no extra code path is needed and there is a single,
testable reconciliation routine.

### Staged-file format (Path B)

`parsePatientImportFile()` in `src/data_ext.cpp`:

* Reads UTF-8 **or** UTF-16 (BOM-aware via `readFileUtf8`).
* **Auto-detects the delimiter**: `|`, `,`, `;` or TAB.
* **Auto-detects a header row** (English *or* Persian column names, matched
  case-insensitively): `national_id/کدملی`, `first/نام`, `last/خانوادگی`,
  `father/پدر`, `gender/جنسیت`, `birth/تولد`, `mobile/موبایل`, `insurance/بیمه`.
  If no trustworthy header is found, columns are read **positionally** in the
  canonical order above.
* Normalises gender to the store's canonical `مرد` / `زن`.
* Never throws; returns a Persian `parseError` on malformed input.

---

## 3. Dedup / national-ID matching (§13)

The **10-digit national code is the clinical primary key.** `importPatients()`
in `src/data_ext.cpp`:

1. Loads the current `data\patients.dat` once into an in-memory index.
2. For each incoming row:
   * skips rows with an **invalid/empty** national code (`validNationalId`) →
     counted in `skippedInvalid`;
   * skips rows with **no usable name** → `skippedEmpty`;
   * if the code **already exists**, the record is **updated in place**
     (newer wins) → `updated`;
   * otherwise it is **appended** → `inserted`.
3. Persists the whole store **once** (single write, not per-row).

Returns an `ImportResult { total, inserted, updated, skippedInvalid,
skippedEmpty, ok, error }` which the UI shows as a Persian reconciliation
summary (`bkAnImportPatients` in `src/backup.cpp`, reachable from the hidden
backup-analyzer page via the «ورود بیماران» button).

This is **exactly** the dedup contract `rememberPatient()` already used for a
single reception save (it drops any prior copy of the same code), so a patient
imported in bulk and a patient saved at reception share one store and one
matching rule.

---

## 4. Reception auto-fill by national ID (§14)

Already wired and unchanged in spirit:

* `nidEditProc` (in `src/reception.cpp`) intercepts **Enter** in the national-ID
  field and calls `doInquiry()`.
* `doInquiry()` → `lookupCitizen(nid)` which trusts, in order:
  1. a configured **online registry** web-service (`registry_url` in
     `data\settings.ini`), then
  2. the **local patient store** (`lookupLocalPatient` → `data\patients.dat`).
* On a hit it auto-fills name / surname / father / birth / gender / mobile /
  insurance and hops focus to the first empty field. **No fabrication** ever —
  an unverified code leaves the fields for manual entry with a red ring.

Because the import pipeline writes the **same** `data\patients.dat`, every
imported patient is instantly recall-able at reception by typing their national
code and pressing Enter.

---

## 5. Network / server readiness (§15)

The data layer is intentionally **function-boundary based**, so the file-backed
implementation can be swapped for a network/REST/SQL one **without touching any
UI code**:

```
 UI (reception / admin / appointment)
        │  calls only these stable signatures:
        ▼
 ┌───────────────────────────────────────────────────────────────┐
 │  Repository surface (declared in app.h)                        │
 │   lookupCitizen(nid) -> CitizenInfo                            │
 │   rememberPatient(...)                                          │
 │   loadAllPatients() -> vector<PatientRow>                       │
 │   deletePatient(nid) -> bool                                    │
 │   importPatients(rows) -> ImportResult                          │
 │   parsePatientImportFile(path,&err) -> vector<ImportPatientRow> │
 └───────────────────────────────────────────────────────────────┘
        │  current implementation (src/data_ext.cpp):
        ▼
   file-backed  (data\patients.dat, settings.ini, *.csv)
```

To move to a server, only `src/data_ext.cpp` changes:

* **Controller / service / repository separation** — the UI is already the
  *controller* (it only calls the repository signatures above); `data_ext.cpp`
  is the *repository*. Introducing a `PatientService` that talks WinHTTP/REST
  to a clinic server is a drop-in replacement behind the same signatures.
* `lookupRegistry()` already demonstrates the network pattern (WinHTTP GET with
  short timeouts, graceful offline fallback) — the same approach generalises to
  a full REST repository.
* Conflict / sync is already modelled by the **national-ID-keyed upsert**:
  last-write-wins per national code, which maps cleanly to a server-side
  `MERGE`/`UPSERT`.

No access-level or existing-data behaviour changes (§16): the permission matrix
(`canAccess`) and `data\*.dat` schemas are untouched; the import only *adds*
rows through the existing dedup-aware writer.
