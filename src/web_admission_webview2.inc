// ============================================================================
//  web_admission_webview2.inc — minimal, self-contained WebView2 COM surface.
//  Included into web_admission.cpp. The official WebView2 SDK headers are not
//  shipped with MinGW, so we declare the small subset of interfaces we need and
//  load CreateCoreWebView2EnvironmentWithOptions dynamically from
//  WebView2Loader.dll (falls back to the in-proc export in the Evergreen
//  runtime). All interfaces match the public ABI exactly.
// ============================================================================
#pragma once
#include <windows.h>
#include <objbase.h>

// ---- forward decls ---------------------------------------------------------
struct ICoreWebView2;
struct ICoreWebView2Controller;
struct ICoreWebView2Environment;
struct ICoreWebView2WebMessageReceivedEventArgs;
struct ICoreWebView2WebMessageReceivedEventHandler;
struct ICoreWebView2NavigationCompletedEventArgs;
struct ICoreWebView2NavigationCompletedEventHandler;
struct ICoreWebView2AcceleratorKeyPressedEventArgs;
struct ICoreWebView2AcceleratorKeyPressedEventHandler;

typedef struct EventRegistrationToken { __int64 value; } EventRegistrationToken;

// KeyEventKind values (public ABI). Only KeyDown/SystemKeyDown carry the key.
enum COREWEBVIEW2_KEY_EVENT_KIND {
  COREWEBVIEW2_KEY_EVENT_KIND_KEY_DOWN = 0,
  COREWEBVIEW2_KEY_EVENT_KIND_KEY_UP = 1,
  COREWEBVIEW2_KEY_EVENT_KIND_SYSTEM_KEY_DOWN = 2,
  COREWEBVIEW2_KEY_EVENT_KIND_SYSTEM_KEY_UP = 3
};

// ---- IIDs (from WebView2.h, stable public ABI) -----------------------------
static const IID IID_ICoreWebView2Environment_ =
  {0xb96d755e,0x0319,0x4e92,{0xa2,0x96,0x23,0x43,0x6f,0x46,0xa1,0xfc}};
static const IID IID_ICoreWebView2Controller_ =
  {0x4d00c0d1,0x9434,0x4eb6,{0x80,0x78,0x86,0x27,0xd1,0x03,0xf4,0x2a}};
static const IID IID_ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler_ =
  {0x4e8a3389,0xc9d8,0x4bd2,{0xb6,0xb5,0x12,0x4f,0xee,0x6c,0xc1,0x4d}};
static const IID IID_ICoreWebView2CreateCoreWebView2ControllerCompletedHandler_ =
  {0x6c4819f3,0xc9b7,0x4260,{0x81,0x27,0xc9,0xf5,0xbd,0xe7,0xf6,0x8c}};
static const IID IID_ICoreWebView2WebMessageReceivedEventHandler_ =
  {0x57213f19,0x00e6,0x49fa,{0x8e,0x07,0x89,0x8e,0xa0,0x1e,0xcb,0xd2}};
static const IID IID_ICoreWebView2NavigationCompletedEventHandler_ =
  {0xd33a35bf,0x1c49,0x4f98,{0x93,0xab,0x00,0x6e,0x05,0x33,0xfe,0x1c}};
static const IID IID_ICoreWebView2AcceleratorKeyPressedEventHandler_ =
  {0x9f760f8a,0xfb79,0x42be,{0x99,0x90,0x7b,0x56,0x90,0x0f,0xa9,0xc7}};

// ============================================================================
//  Interface vtable declarations (only the methods we call).
// ============================================================================
struct ICoreWebView2 : public IUnknown {
  // IUnknown (0-2) then interface methods, in declared vtable order.
  virtual HRESULT STDMETHODCALLTYPE get_Settings(void**) = 0;
  virtual HRESULT STDMETHODCALLTYPE get_Source(LPWSTR*) = 0;
  virtual HRESULT STDMETHODCALLTYPE Navigate(LPCWSTR uri) = 0;
  virtual HRESULT STDMETHODCALLTYPE NavigateToString(LPCWSTR html) = 0;
  virtual HRESULT STDMETHODCALLTYPE add_NavigationStarting(void*, EventRegistrationToken*) = 0;
  virtual HRESULT STDMETHODCALLTYPE remove_NavigationStarting(EventRegistrationToken) = 0;
  virtual HRESULT STDMETHODCALLTYPE add_ContentLoading(void*, EventRegistrationToken*) = 0;
  virtual HRESULT STDMETHODCALLTYPE remove_ContentLoading(EventRegistrationToken) = 0;
  virtual HRESULT STDMETHODCALLTYPE add_SourceChanged(void*, EventRegistrationToken*) = 0;
  virtual HRESULT STDMETHODCALLTYPE remove_SourceChanged(EventRegistrationToken) = 0;
  virtual HRESULT STDMETHODCALLTYPE add_HistoryChanged(void*, EventRegistrationToken*) = 0;
  virtual HRESULT STDMETHODCALLTYPE remove_HistoryChanged(EventRegistrationToken) = 0;
  virtual HRESULT STDMETHODCALLTYPE add_NavigationCompleted(
      ICoreWebView2NavigationCompletedEventHandler*, EventRegistrationToken*) = 0;
  virtual HRESULT STDMETHODCALLTYPE remove_NavigationCompleted(EventRegistrationToken) = 0;
  virtual HRESULT STDMETHODCALLTYPE add_FrameNavigationStarting(void*, EventRegistrationToken*) = 0;
  virtual HRESULT STDMETHODCALLTYPE remove_FrameNavigationStarting(EventRegistrationToken) = 0;
  virtual HRESULT STDMETHODCALLTYPE add_FrameNavigationCompleted(void*, EventRegistrationToken*) = 0;
  virtual HRESULT STDMETHODCALLTYPE remove_FrameNavigationCompleted(EventRegistrationToken) = 0;
  virtual HRESULT STDMETHODCALLTYPE add_ScriptDialogOpening(void*, EventRegistrationToken*) = 0;
  virtual HRESULT STDMETHODCALLTYPE remove_ScriptDialogOpening(EventRegistrationToken) = 0;
  virtual HRESULT STDMETHODCALLTYPE add_PermissionRequested(void*, EventRegistrationToken*) = 0;
  virtual HRESULT STDMETHODCALLTYPE remove_PermissionRequested(EventRegistrationToken) = 0;
  virtual HRESULT STDMETHODCALLTYPE add_ProcessFailed(void*, EventRegistrationToken*) = 0;
  virtual HRESULT STDMETHODCALLTYPE remove_ProcessFailed(EventRegistrationToken) = 0;
  virtual HRESULT STDMETHODCALLTYPE AddScriptToExecuteOnDocumentCreated(LPCWSTR, void*) = 0;
  virtual HRESULT STDMETHODCALLTYPE RemoveScriptToExecuteOnDocumentCreated(LPCWSTR) = 0;
  virtual HRESULT STDMETHODCALLTYPE ExecuteScript(LPCWSTR javaScript, void* handler) = 0;
  virtual HRESULT STDMETHODCALLTYPE CapturePreview(int, IStream*, void*) = 0;
  virtual HRESULT STDMETHODCALLTYPE Reload() = 0;
  virtual HRESULT STDMETHODCALLTYPE PostWebMessageAsJson(LPCWSTR webMessageAsJson) = 0;
  virtual HRESULT STDMETHODCALLTYPE PostWebMessageAsString(LPCWSTR webMessageAsString) = 0;
  virtual HRESULT STDMETHODCALLTYPE add_WebMessageReceived(
      ICoreWebView2WebMessageReceivedEventHandler* handler, EventRegistrationToken* token) = 0;
  virtual HRESULT STDMETHODCALLTYPE remove_WebMessageReceived(EventRegistrationToken) = 0;
  // (remaining methods omitted — not called)
};

struct ICoreWebView2Controller : public IUnknown {
  virtual HRESULT STDMETHODCALLTYPE get_IsVisible(BOOL*) = 0;
  virtual HRESULT STDMETHODCALLTYPE put_IsVisible(BOOL) = 0;
  virtual HRESULT STDMETHODCALLTYPE get_Bounds(RECT*) = 0;
  virtual HRESULT STDMETHODCALLTYPE put_Bounds(RECT) = 0;
  virtual HRESULT STDMETHODCALLTYPE get_ZoomFactor(double*) = 0;
  virtual HRESULT STDMETHODCALLTYPE put_ZoomFactor(double) = 0;
  virtual HRESULT STDMETHODCALLTYPE add_ZoomFactorChanged(void*, EventRegistrationToken*) = 0;
  virtual HRESULT STDMETHODCALLTYPE remove_ZoomFactorChanged(EventRegistrationToken) = 0;
  virtual HRESULT STDMETHODCALLTYPE SetBoundsAndZoomFactor(RECT, double) = 0;
  virtual HRESULT STDMETHODCALLTYPE MoveFocus(int) = 0;
  virtual HRESULT STDMETHODCALLTYPE add_MoveFocusRequested(void*, EventRegistrationToken*) = 0;
  virtual HRESULT STDMETHODCALLTYPE remove_MoveFocusRequested(EventRegistrationToken) = 0;
  virtual HRESULT STDMETHODCALLTYPE add_GotFocus(void*, EventRegistrationToken*) = 0;
  virtual HRESULT STDMETHODCALLTYPE remove_GotFocus(EventRegistrationToken) = 0;
  virtual HRESULT STDMETHODCALLTYPE add_LostFocus(void*, EventRegistrationToken*) = 0;
  virtual HRESULT STDMETHODCALLTYPE remove_LostFocus(EventRegistrationToken) = 0;
  virtual HRESULT STDMETHODCALLTYPE add_AcceleratorKeyPressed(void*, EventRegistrationToken*) = 0;
  virtual HRESULT STDMETHODCALLTYPE remove_AcceleratorKeyPressed(EventRegistrationToken) = 0;
  virtual HRESULT STDMETHODCALLTYPE get_ParentWindow(HWND*) = 0;
  virtual HRESULT STDMETHODCALLTYPE put_ParentWindow(HWND) = 0;
  virtual HRESULT STDMETHODCALLTYPE NotifyParentWindowPositionChanged() = 0;
  virtual HRESULT STDMETHODCALLTYPE Close() = 0;
  virtual HRESULT STDMETHODCALLTYPE get_CoreWebView2(ICoreWebView2**) = 0;
};

struct ICoreWebView2Environment : public IUnknown {
  virtual HRESULT STDMETHODCALLTYPE CreateCoreWebView2Controller(
      HWND parentWindow, void* handler) = 0;
  virtual HRESULT STDMETHODCALLTYPE CreateWebResourceResponse(
      IStream*, int, LPCWSTR, LPCWSTR, void**) = 0;
  virtual HRESULT STDMETHODCALLTYPE get_BrowserVersionString(LPWSTR*) = 0;
  virtual HRESULT STDMETHODCALLTYPE add_NewBrowserVersionAvailable(void*, EventRegistrationToken*) = 0;
  virtual HRESULT STDMETHODCALLTYPE remove_NewBrowserVersionAvailable(EventRegistrationToken) = 0;
};

struct ICoreWebView2WebMessageReceivedEventArgs : public IUnknown {
  virtual HRESULT STDMETHODCALLTYPE get_Source(LPWSTR*) = 0;
  virtual HRESULT STDMETHODCALLTYPE get_WebMessageAsJson(LPWSTR*) = 0;
  virtual HRESULT STDMETHODCALLTYPE TryGetWebMessageAsString(LPWSTR*) = 0;
};

struct ICoreWebView2WebMessageReceivedEventHandler : public IUnknown {
  virtual HRESULT STDMETHODCALLTYPE Invoke(
      ICoreWebView2* sender, ICoreWebView2WebMessageReceivedEventArgs* args) = 0;
};

struct ICoreWebView2NavigationCompletedEventArgs : public IUnknown {
  virtual HRESULT STDMETHODCALLTYPE get_IsSuccess(BOOL*) = 0;
  virtual HRESULT STDMETHODCALLTYPE get_WebErrorStatus(int*) = 0;
  virtual HRESULT STDMETHODCALLTYPE get_NavigationId(UINT64*) = 0;
};

struct ICoreWebView2NavigationCompletedEventHandler : public IUnknown {
  virtual HRESULT STDMETHODCALLTYPE Invoke(
      ICoreWebView2* sender, ICoreWebView2NavigationCompletedEventArgs* args) = 0;
};

// AcceleratorKeyPressed: fired for keys like Tab/Enter/Ctrl+A/arrows that the
// WebView would otherwise let bubble to the host. We keep Handled=FALSE for the
// navigation/edit keys so the WebView processes them internally (which fires the
// page's keydown listeners).
struct ICoreWebView2AcceleratorKeyPressedEventArgs : public IUnknown {
  virtual HRESULT STDMETHODCALLTYPE get_KeyEventKind(int*) = 0;
  virtual HRESULT STDMETHODCALLTYPE get_VirtualKey(UINT*) = 0;
  virtual HRESULT STDMETHODCALLTYPE get_KeyEventLParam(int*) = 0;
  virtual HRESULT STDMETHODCALLTYPE get_PhysicalKeyStatus(void*) = 0;
  virtual HRESULT STDMETHODCALLTYPE get_Handled(BOOL*) = 0;
  virtual HRESULT STDMETHODCALLTYPE put_Handled(BOOL) = 0;
};

struct ICoreWebView2AcceleratorKeyPressedEventHandler : public IUnknown {
  virtual HRESULT STDMETHODCALLTYPE Invoke(
      ICoreWebView2Controller* sender,
      ICoreWebView2AcceleratorKeyPressedEventArgs* args) = 0;
};

// Completed handlers for env + controller creation
struct ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler : public IUnknown {
  virtual HRESULT STDMETHODCALLTYPE Invoke(HRESULT result, ICoreWebView2Environment* env) = 0;
};
struct ICoreWebView2CreateCoreWebView2ControllerCompletedHandler : public IUnknown {
  virtual HRESULT STDMETHODCALLTYPE Invoke(HRESULT result, ICoreWebView2Controller* ctrl) = 0;
};

// Dynamic entry point exported by WebView2Loader.dll
typedef HRESULT (STDMETHODCALLTYPE *PFN_CreateWebView2Env)(
    PCWSTR browserExecutableFolder, PCWSTR userDataFolder, void* environmentOptions,
    ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler* handler);
typedef HRESULT (STDMETHODCALLTYPE *PFN_GetAvailableBrowserVersion)(
    PCWSTR browserExecutableFolder, LPWSTR* versionInfo);
