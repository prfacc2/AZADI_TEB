// ============================================================================
//  web_thread_pool.h — fixed-size worker pool for the embedded-web host + a
//  UI-thread marshalling helper (v1.40.0).
//
//  The loopback HTTP host used to spawn ONE CreateThread per accepted socket.
//  That works but is unbounded: a burst of parallel asset/API requests creates
//  a burst of threads. The pool bounds concurrency instead:
//     * low-power hardware (g_lowSpec) -> 2 workers
//     * default                        -> 4 workers
//     * hard cap                       -> 8 workers
//  Jobs (accepted SOCKETs) are pushed onto a queue; idle workers pop and serve
//  them. If the pool is not initialised the caller falls back to the classic
//  per-connection CreateThread so behaviour is never worse than before.
//
//  RunOnUiThread marshals an arbitrary callable back onto the GUI thread by
//  PostMessage(g_hFrame, WM_APP_UI_TASK, ...). Worker threads MUST use it for
//  anything that touches HWNDs / GDI / the WebView2 control, which are all
//  single-threaded-apartment bound. main.cpp's frameProc runs + frees the task.
//
//  C++17, no runtime deps, single static EXE.
// ============================================================================
#pragma once
#include <functional>
#include <winsock2.h>
#include <windows.h>

// Job callback: serve one accepted connection socket (the pool never closes it
// for you beyond what the callback does; the admission serve fn closes it).
typedef std::function<void(SOCKET)> WebPoolJob;

// Initialise the pool. `serveFn` is invoked on a worker thread for each queued
// socket. Worker count is chosen from g_lowSpec (2 / 4, capped at 8). Safe to
// call once at boot; a second call is a no-op. Returns the worker count (0 on
// failure, in which case callers must use the classic per-connection thread).
int  WebPool_Init(WebPoolJob serveFn);

// Queue an accepted socket for a worker. Returns false when the pool is not
// initialised or the queue push failed (caller falls back to CreateThread).
bool WebPool_Submit(SOCKET c);

// True once WebPool_Init succeeded.
bool WebPool_Ready();

// Stop the pool (drains + joins workers). Called at shutdown; optional.
void WebPool_Shutdown();

// ---------------------------------------------------------------------------
//  UI-thread marshalling. Runs `fn` on the GUI thread (owner of g_hFrame).
//  * async  : posts and returns immediately (fire-and-forget).
//  The frame window proc dispatches WM_APP_UI_TASK -> WebUiTask_Run(lParam).
// ---------------------------------------------------------------------------
void RunOnUiThread(std::function<void()> fn);

// Invoked by frameProc when it receives WM_APP_UI_TASK. `lParam` is the opaque
// task pointer that RunOnUiThread posted. Runs the callable then frees it.
void WebUiTask_Run(LPARAM lParam);
