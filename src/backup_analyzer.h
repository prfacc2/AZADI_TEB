// ============================================================================
//  backup_analyzer.h — public API for the real backup-file analyzer.
//  Implemented in backup_analyzer.cpp. See that file for details.
// ============================================================================
#pragma once
#include "app.h"

struct BkSection { std::wstring title; std::wstring body; };
struct BkAnalysis {
    std::vector<BkSection> sections;
    bool ok=false;
    std::wstring error;
    // §C.7: surfaced identity for the logs/app.log breadcrumb (no PII).
    std::wstring dbName;     // database name (MTF/SQLite) when known
    std::wstring fileType;   // short type tag: mtf/sqlite/zip/sql/json/text
};

//  Progress callback: pct 0..100 + a Persian status line.
typedef void (*BkProgFn)(int pct, const std::wstring& status, void* user);

//  Analyze a backup file (auto-detects type by magic bytes). Never throws —
//  on failure returns an analysis with ok=false and a Persian error message.
BkAnalysis analyzeBackupFile(const std::wstring& path, BkProgFn prog, void* user);
