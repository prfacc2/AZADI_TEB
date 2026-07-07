// ============================================================================
//  web_admission_dispatch.inc — the PUBLIC WebAdmission_* API. Picks the best
//  available embedded engine at runtime and routes resize/destroy to whichever
//  one actually created the view. Included last into web_admission.cpp.
//
//  Engine preference:
//    1) Microsoft WebView2 (Chromium) — modern, best. Needs the runtime.
//    2) OLE WebBrowser (Trident/MSHTML) — UNIVERSAL fallback that ships with
//       every Windows (and Wine), so the HTML admission page ALWAYS renders
//       INSIDE the app — never an external browser, never the native GDI form
//       (that stays only as an absolute last-resort safety net).
// ============================================================================

// True whenever ANY embedded engine can host the page (always true in practice,
// since the MSHTML WebBrowser control is present on every Windows).
bool WebAdmission_Available(){
    return true;
}

HWND WebAdmission_CreateView(HWND parent){
    if(!parent) return nullptr;
    if(!WebAdmission_EnsureHost()) return nullptr;

    // 1) modern Chromium engine when its runtime is present
    if(WV2_Available()){
        HWND wv=WV2_CreateView(parent);
        if(wv) return wv;
    }
    // 2) universal Trident/MSHTML fallback (still inside the app)
    if(MshtmlAdmission_Available()){
        HWND mv=MshtmlAdmission_CreateView(parent);
        if(mv) return mv;
    }
    return nullptr;   // caller keeps the native GDI form
}

void WebAdmission_Resize(HWND view, int w, int h){
    if(!view) return;
    if(WV2_Owns(view)){ WV2_Resize(view,w,h); return; }
    MshtmlAdmission_Resize(view,w,h);
}

void WebAdmission_PushEvent(const char* eventName, const std::string& jsonData){
    // WebView2 push (direct). The MSHTML page consumes events via /api/poll,
    // which the HTTP host already queues in WV2_PushEvent's shared queue.
    WV2_PushEvent(eventName,jsonData);
}

void WebAdmission_DestroyView(HWND view){
    if(!view) return;
    if(WV2_DestroyView(view)) return;   // was a WebView2 view
    MshtmlAdmission_Destroy(view);      // else an MSHTML view
}
