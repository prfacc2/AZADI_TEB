// ============================================================================
//  backup_log.h  — RELEASE 1.2.0  (Section A.3)
//  The ONLY general log channel that remains in the app: "Backup Log".
//
//  Every backup analyze / import / create operation reports through
//  BackupLog_Event(). Each line carries (at minimum):
//    timestamp (Tehran tz, ISO-8601 with ms), pid, tid, phase, file path,
//    file size, sha-256(file[:1MiB]) identity, Win32 GetLastError + message,
//    SQLite errno + message (if relevant), C++ exception text (if relevant),
//    SEH exception code (if relevant), full stack trace, free disk space,
//    and the free-form "detail" payload.
//
//  Storage: %LOCALAPPDATA%/AzadiTeb/backup_logs/backup.log
//  Rotated at 2 MB, keeps the last 5 files (older ones gzipped via miniz).
// ============================================================================
#pragma once
#include "app.h"

//  Initialize/teardown the backup log channel. Safe to call multiple times.
void BackupLog_Init();
void BackupLog_Shutdown();

//  Record a structured event. `phase` is one of:
//    ANALYZE_START / ANALYZE_PROGRESS / ANALYZE_OK / ANALYZE_FAIL
//    IMPORT_START  / IMPORT_OK        / IMPORT_FAIL
//    BACKUP_CREATE_START / BACKUP_CREATE_OK / BACKUP_CREATE_FAIL
//  `file`   — full path of the backup involved (may be empty).
//  `detail` — free-form details (counts, durations, error text, etc.).
//  Never throws; never touches the UI.
void BackupLog_Event(const wchar_t* phase,
                     const wchar_t* file,
                     const wchar_t* detail);

//  Per-call progress ring buffer: push the last-16 progress breadcrumbs so a
//  failure entry can reconstruct what was happening just before a crash.
void BackupLog_Breadcrumb(const wchar_t* step);

//  Return the most-recent full backup-log payload (used to populate the inline
//  error card's "technical details" box). Thread-safe snapshot.
std::wstring BackupLog_LastPayload();
