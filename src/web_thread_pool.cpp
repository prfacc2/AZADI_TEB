// ============================================================================
//  web_thread_pool.cpp — implementation of web_thread_pool.h (v1.40.0).
//
//  A bounded worker pool (2 low-power / 4 default / cap 8) that serves accepted
//  loopback sockets, plus RunOnUiThread() which marshals a callable back onto
//  the GUI thread via PostMessage(g_hFrame, WM_APP_UI_TASK, ...).
//
//  Synchronisation uses a CRITICAL_SECTION + a manual auto-reset EVENT (a
//  classic bounded producer/consumer) rather than <condition_variable> so the
//  MinGW static runtime stays lean and the behaviour is 100% Win32-native.
// ============================================================================
#include "app.h"
#include "web_thread_pool.h"
#include "client_log.h"

#include <deque>
#include <vector>
#include <string>
#include <exception>
#include <winsock2.h>
#include <windows.h>

// v1.45.0 §5: local UTF-8 → wide helper for the worker C++ exception breadcrumb
// (u82w in web_admission.cpp is file-static and not visible here). JSON-escapes
// the payload so the client.log line stays valid JSON-per-line.
static std::wstring wtpJsonEsc(const std::string& s) {
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), NULL, 0);
    std::wstring w;
    if (n > 0) { w.resize(n); MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &w[0], n); }
    std::wstring out;
    for (size_t i = 0; i < w.size(); ++i) {
        wchar_t c = w[i];
        if (c == L'"' || c == L'\\') { out += L'\\'; out += c; }
        else if (c == L'\n') out += L"\\n";
        else if (c == L'\r') out += L"\\r";
        else if (c == L'\t') out += L"\\t";
        else if (c < 0x20) { wchar_t b[8]; swprintf(b, 8, L"\\u%04X", (unsigned)c); out += b; }
        else out += c;
    }
    return out;
}

extern HWND g_hFrame;
extern bool g_lowSpec;

namespace {

static WebPoolJob        g_serveFn;
static std::deque<SOCKET> g_jobs;
static CRITICAL_SECTION  g_cs;
static bool              g_csInit = false;
static HANDLE            g_evt   = NULL;     // signalled when a job is queued
static std::vector<HANDLE> g_workers;
static volatile LONG     g_stop  = 0;
static volatile LONG     g_ready = 0;

static void poolLock()   { EnterCriticalSection(&g_cs); }
static void poolUnlock() { LeaveCriticalSection(&g_cs); }

static int chooseWorkerCount() {
    // v1.43.0 FREEZE HARDENING: the MSHTML fallback opens several parallel
    // connections (page + css + js + font) AND polls /api/poll every ~900ms.
    // With only 2 workers those could momentarily saturate the pool and make
    // the page feel stuck. Raise the low-power floor to 4 (default 6, cap 8) so
    // an /api call, a poll, and asset loads can always be served concurrently.
    int n = g_lowSpec ? 4 : 6;
    if (n > 8) n = 8;      // hard cap (spec)
    if (n < 1) n = 1;
    return n;
}

static DWORD WINAPI workerMain(LPVOID) {
    for (;;) {
        // wait for work (or a stop signal).
        WaitForSingleObject(g_evt, INFINITE);
        for (;;) {
            if (InterlockedCompareExchange(&g_stop, 0, 0)) return 0;
            SOCKET c = INVALID_SOCKET;
            poolLock();
            if (!g_jobs.empty()) { c = g_jobs.front(); g_jobs.pop_front(); }
            bool more = !g_jobs.empty();
            poolUnlock();
            if (c == INVALID_SOCKET) break;       // queue drained
            // keep other workers awake if more jobs remain.
            if (more) SetEvent(g_evt);
            if (g_serveFn) {
                // v1.45.0 §5: keep the C++ catch (SEH is handled by the
                // process-wide crash filter — we NEVER __try/longjmp here) and
                // leave a single client.log breadcrumb on any C++ throw so a
                // recurring worker failure is diagnosable without UB.
                SOCKET localC = c;
                try {
                    g_serveFn(c);
                } catch (const std::exception& ex) {
                    ClientLog_Error(L"admission", L"worker C++ exception",
                        L"{\"what\":\"" + wtpJsonEsc(std::string(ex.what())) + L"\"}");
                    closesocket(localC);
                } catch (...) {
                    ClientLog_Error(L"admission", L"worker unknown C++ exception", L"{}");
                    closesocket(localC);
                }
            } else {
                closesocket(c);
            }
        }
        if (InterlockedCompareExchange(&g_stop, 0, 0)) return 0;
    }
}

} // namespace

int WebPool_Init(WebPoolJob serveFn) {
    if (InterlockedCompareExchange(&g_ready, 0, 0)) return (int)g_workers.size();
    if (!serveFn) return 0;
    if (!g_csInit) { InitializeCriticalSection(&g_cs); g_csInit = true; }
    g_evt = CreateEventW(NULL, /*manualReset*/FALSE, /*initial*/FALSE, NULL);
    if (!g_evt) return 0;
    g_serveFn = serveFn;
    g_stop = 0;
    int n = chooseWorkerCount();
    for (int i = 0; i < n; ++i) {
        HANDLE h = CreateThread(NULL, 0, workerMain, NULL, 0, NULL);
        if (h) g_workers.push_back(h);
    }
    if (g_workers.empty()) { CloseHandle(g_evt); g_evt = NULL; return 0; }
    InterlockedExchange(&g_ready, 1);
    return (int)g_workers.size();
}

bool WebPool_Submit(SOCKET c) {
    if (!InterlockedCompareExchange(&g_ready, 0, 0)) return false;
    poolLock();
    g_jobs.push_back(c);
    poolUnlock();
    SetEvent(g_evt);
    return true;
}

bool WebPool_Ready() {
    return InterlockedCompareExchange(&g_ready, 0, 0) != 0;
}

void WebPool_Shutdown() {
    if (!InterlockedCompareExchange(&g_ready, 0, 0)) return;
    InterlockedExchange(&g_stop, 1);
    // wake every worker so they observe the stop flag.
    for (size_t i = 0; i < g_workers.size(); ++i) SetEvent(g_evt);
    if (!g_workers.empty())
        WaitForMultipleObjects((DWORD)g_workers.size(), g_workers.data(), TRUE, 2000);
    for (size_t i = 0; i < g_workers.size(); ++i) CloseHandle(g_workers[i]);
    g_workers.clear();
    // close any sockets left in the queue.
    poolLock();
    while (!g_jobs.empty()) { closesocket(g_jobs.front()); g_jobs.pop_front(); }
    poolUnlock();
    if (g_evt) { CloseHandle(g_evt); g_evt = NULL; }
    InterlockedExchange(&g_ready, 0);
}

// ---------------------------------------------------------------------------
//  UI-thread marshalling.
// ---------------------------------------------------------------------------
void RunOnUiThread(std::function<void()> fn) {
    if (!fn) return;
    HWND h = g_hFrame;
    if (!h) { /* no frame yet — run inline as a best-effort fallback */ fn(); return; }
    // heap a copy; frameProc frees it in WebUiTask_Run.
    std::function<void()>* task = new std::function<void()>(std::move(fn));
    if (!PostMessageW(h, WM_APP_UI_TASK, 0, (LPARAM)task)) {
        // post failed (queue full / window gone) — run inline & free.
        try { (*task)(); } catch (...) {}
        delete task;
    }
}

void WebUiTask_Run(LPARAM lParam) {
    std::function<void()>* task = (std::function<void()>*)lParam;
    if (!task) return;
    try { (*task)(); } catch (...) {}
    delete task;
}
