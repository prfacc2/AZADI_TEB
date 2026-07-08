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

// ---- LIVE SERVICE / INSURANCE SYNC from Management -------------------------
//  When the Management «مدیریت خدمات» page adds / edits / deletes a service (or
//  the insurance tables change), we push the fresh catalog + insurance snapshot
//  to every open embedded Admission view. The JS side re-renders any open
//  service-suggestion list and refreshes the insurance selectors WITHOUT a
//  page reload — the admission surface reflects Management edits immediately.
void WebAdmission_NotifyCatalogChanged(){
    // Only build/push when at least one embedded view is alive (cheap guard).
    {
        std::lock_guard<std::mutex> lk(g_adViewsMx);
        if(g_adViews.empty()) return;
    }
    // Full active service catalog (Management is the single source of truth).
    std::string cat="{\"services\":[";
    {
        auto svcs=loadServices();
        int n=0;
        for(const auto& s:svcs){
            if(s.status==0) continue;      // only sellable services
            if(n++) cat+=",";
            cat+=serviceJson(s);
        }
    }
    cat+="]}";
    WV2_PushEvent("catalog.update",cat);

    // Insurance selectors (base + supplementary) may also change in Management.
    std::string ins="{\"main\":"+insurancesJson()+",\"supp\":"+suppInsurancesJson()+"}";
    WV2_PushEvent("insurance.update",ins);
}

void WebAdmission_DestroyView(HWND view){
    if(!view) return;
    if(WV2_DestroyView(view)) return;   // was a WebView2 view
    MshtmlAdmission_Destroy(view);      // else an MSHTML view
}

#ifdef AZ_DEBUG_BUILD
// Headless diagnostics only: number of times the embedded page reached
// /api/init. Proves the bundled ES5 JS parsed & ran (a syntax error would stop
// the script before it ever called the bridge).
long WebAdmission_DebugInitHits(){ return (long)g_adInitHits; }
#endif
