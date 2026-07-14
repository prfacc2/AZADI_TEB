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

#include <deque>
#include <vector>
#include <winsock2.h>
#include <windows.h>

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
    int n = g_lowSpec ? 2 : 4;
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
                try { g_serveFn(c); } catch (...) { closesocket(c); }
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

// ---------------------------------------------------------------------------
//  RunOnUiThreadSync — run `fn` on the GUI thread and wait for it to finish.
//  See web_thread_pool.h for the rationale (freeze fix). We reuse the existing
//  WM_APP_UI_TASK plumbing but wrap `fn` in a small trampoline that signals an
//  auto-reset event once the real work has run, then block the caller on that
//  event. SendMessage is deliberately AVOIDED (it would re-enter the frame proc
//  and can itself deadlock if the UI thread is mid-SendMessage to us); the
//  post + event handshake is robust regardless of who is waiting on whom.
// ---------------------------------------------------------------------------
void RunOnUiThreadSync(std::function<void()> fn) {
    if (!fn) return;
    HWND h = g_hFrame;
    // No frame yet, OR we ARE the UI thread → run inline (never self-deadlock).
    if (!h || GetCurrentThreadId() == GetWindowThreadProcessId(h, NULL)) {
        try { fn(); } catch (...) {}
        return;
    }
    HANDLE done = CreateEventW(NULL, /*manualReset*/TRUE, /*initial*/FALSE, NULL);
    if (!done) { // fall back to a best-effort async post if the event failed
        RunOnUiThread(std::move(fn));
        return;
    }
    // trampoline: run the real fn, then signal completion.
    std::function<void()>* task = new std::function<void()>(
        [fn, done]() {
            try { fn(); } catch (...) {}
            SetEvent(done);
        });
    if (!PostMessageW(h, WM_APP_UI_TASK, 0, (LPARAM)task)) {
        // post failed → run inline on THIS thread as a last resort and clean up.
        try { fn(); } catch (...) {}
        delete task;
        CloseHandle(done);
        return;
    }
    // Block until the UI thread has finished the task. Bounded wait so a wedged
    // UI thread can never pin a worker forever; 30s is far beyond any legitimate
    // print dialog interaction and matches the JS-side bridge timeout budget.
    WaitForSingleObject(done, 30000);
    CloseHandle(done);
}
