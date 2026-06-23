// ============================================================================
//  sections.h — clinic Sections / Departments registry (release 1.4.0, §2)
//  A single source of truth every other module reads from (print designer,
//  appointment counters, settings). File-backed (data\sections.dat) so the
//  program stays a single static EXE, ready to migrate to a real DB later.
// ============================================================================
#pragma once
#include <string>
#include <vector>

struct Section {
    int          id;
    std::wstring code;       // short stable code, e.g. "REC01"
    std::wstring name_fa;
    std::wstring kind;       // reception|injection|lab|radiology|physio|other
    int          is_active;
    std::wstring created_at, updated_at;
    Section():id(0),is_active(1){}
};

// Ensure the store exists and is seeded on first run (idempotent).
void Sections_Init();

// All sections (active + inactive), id-ascending.
int  Sections_All(std::vector<Section>& out);

// Find by code (case-insensitive) OR name_fa (Persian-normalized): prefix or
// substring match. Empty query returns all.
int  Sections_Find(const std::wstring& query, std::vector<Section>& out);

// Insert (id<=0) or update (id>0). Returns the row id (>0) or 0 on failure.
int  Sections_Upsert(const Section& s);

// Delete by id. Returns 1 on success.
int  Sections_Delete(int id);

// Localized label for a kind code.
const wchar_t* Sections_KindLabel(const std::wstring& kind);
