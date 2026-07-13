// ============================================================================
//  client_log.h — structured, abnormal-events-ONLY client log (v1.44.0)
//
//  Writes one JSON object per line to <exe>\logs\client.log. It is called ONLY
//  when something goes wrong (hang, crash, timeout, exception, unresponsive UI
//  thread). It is NEVER called on happy paths (page loaded / keydown / etc.).
//
//  Line format (no pretty printing):
//    {"page":"admission","level":"warn","msg":"bill.compute timeout",
//     "extra":{"seq":42},"t":1783863168822}
//    · t     = Unix epoch MILLISECONDS.
//    · level = "warn" | "error" | "crash".
//
//  All writes append in a concurrent-writer-tolerant way (FILE_SHARE_READ |
//  FILE_SHARE_WRITE, retry once on ERROR_SHARING_VIOLATION after Sleep(3)).
// ============================================================================
#pragma once
#include <string>

// Create logs\ folder if missing. Safe to call more than once.
void ClientLog_Init();

// Append a single abnormal-event line. extraJson MUST be a valid JSON object
// literal (default "{}"). All strings are UTF-8 on disk.
void ClientLog_Warn (const std::wstring& page, const std::wstring& msg,
                     const std::wstring& extraJson = L"{}");
void ClientLog_Error(const std::wstring& page, const std::wstring& msg,
                     const std::wstring& extraJson = L"{}");
void ClientLog_Crash(const std::wstring& page, const std::wstring& msg,
                     const std::wstring& extraJson = L"{}");
