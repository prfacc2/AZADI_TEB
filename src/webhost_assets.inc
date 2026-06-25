// ============================================================================
//  webhost_assets.inc — the embedded HTML/CSS/JS documents for the hybrid
//  reception & appointment surfaces. Included into webhost_bridge.inc.
//
//  Release 1.14.0 (§2/§4): the bare HTML skeleton is replaced by a complete,
//  polished, responsive, form-driven workspace — a real UI system, not a raw
//  browser form. It ships:
//    • a full CSS design system (CSS custom-property theme, spacing rhythm,
//      card surfaces, rounded controls, focus/hover/disabled states, a top bar,
//      a tab strip, a centred loader overlay, responsive breakpoints and a
//      compact/dense mode);
//    • three visible working zones — left summary panel, centre reception form,
//      right patient/search/insurance panel — arranged HIGH so ordinary
//      reception tasks need no scrolling on normal desktop resolutions;
//    • full keyboard-driven form behaviour (Enter → next field, Tab/Shift+Tab,
//      keyboard-safe dropdowns), tab switching, print actions, save/reset/
//      open/load/search handlers, validation hooks, deterministic state
//      hydration from native C++, and safe event dispatch back to native.
//
//  The documents are authored as ES5-safe scripts and force IE=edge so MSHTML
//  renders them in the most modern standards mode available on the host. They
//  are self-contained (no external CSS/JS/fonts — Vazirmatn is already
//  installed system-wide by installVazirFont(), with a sans-serif fallback).
//
//  Authoring rule: keep the C++ string literals readable. We assemble the doc
//  from std::wstring pieces so the markup is not one giant unescaped blob.
//  C++ stays the source of truth; JS never fabricates a lookup/bill/save.
// ============================================================================

// Modern blue-outline SVG icon set (no image files, no icon font). Stroke uses
// currentColor so each icon inherits the surrounding text/accent colour. Kept
// ES-free (pure inline SVG markup) so MSHTML renders them in standards mode.
static std::wstring wh_svg(const wchar_t* name){
    std::wstring n(name);
    const wchar_t* open =
        L"<svg viewBox='0 0 24 24' fill='none' stroke='currentColor' "
        L"stroke-width='1.9' stroke-linecap='round' stroke-linejoin='round'>";
    std::wstring body;
    if(n==L"search")
        body=L"<circle cx='11' cy='11' r='7'/><path d='M21 21l-4.3-4.3'/>";
    else if(n==L"user")
        body=L"<circle cx='12' cy='8' r='4'/><path d='M4 20c0-4 4-6 8-6s8 2 8 6'/>";
    else if(n==L"shield")
        body=L"<path d='M12 3l7 3v5c0 5-3.5 8-7 10-3.5-2-7-5-7-10V6z'/><path d='M9 12l2 2 4-4'/>";
    else if(n==L"stethoscope")
        body=L"<path d='M6 3v6a4 4 0 0 0 8 0V3'/><path d='M6 3H4M14 3h2'/><path d='M10 17v1a4 4 0 0 0 8 0v-2'/><circle cx='19' cy='13' r='2'/>";
    else if(n==L"clipboard")
        body=L"<rect x='6' y='4' width='12' height='17' rx='2'/><path d='M9 4V3h6v1'/><path d='M9 10h6M9 14h4'/>";
    else if(n==L"layers")
        body=L"<path d='M12 3l9 5-9 5-9-5z'/><path d='M3 13l9 5 9-5'/>";
    else if(n==L"receipt")
        body=L"<path d='M5 3v18l2-1 2 1 2-1 2 1 2-1 2 1V3l-2 1-2-1-2 1-2-1-2 1z'/><path d='M8 8h8M8 12h8M8 16h5'/>";
    else if(n==L"print")
        body=L"<path d='M7 9V3h10v6'/><rect x='4' y='9' width='16' height='8' rx='2'/><path d='M7 17h10v4H7z'/>";
    else if(n==L"plus")
        body=L"<path d='M12 5v14M5 12h14'/>";
    else if(n==L"eraser")
        body=L"<path d='M4 15l7-7 6 6-5 5H8z'/><path d='M9 21h11'/>";
    else if(n==L"x")
        body=L"<path d='M6 6l12 12M18 6L6 18'/>";
    else
        body=L"<circle cx='12' cy='12' r='8'/>";
    return std::wstring(open)+body+L"</svg>";
}

// Shared CSS used by both surfaces. Themed at runtime from the boot payload
// (CSS custom properties on :root). This is a full design system, not a few
// utility rules — surfaces, rhythm, controls, tabs, top bar, loader, states.
static std::wstring wh_commonHead(){
    return
    L"<meta http-equiv='X-UA-Compatible' content='IE=edge'>"
    L"<meta charset='utf-8'>"
    L"<style>"
    // ============================ THEME TOKENS ============================
    // Overwritten at runtime by applyTheme(); seeded with a clean medical-blue
    // palette so the surface is fully styled before the first theme push.
    // The palette is deliberately BLUE-and-WHITE harmonised: the primary call
    // to action uses the accent blue (no clashing green), success is reserved
    // only for confirmation chips and the live payable total.
    // 2026 medical palette — White + Blue per spec:
    //   bg #F5F8FD · primary #2E7DFF · secondary #4DA3FF · accent #1E5EFF ·
    //   success #18B75A · danger #E53935 · border #DCE7F5 · navy text.
    L":root{--bg:#f5f8fd;--surface:#ffffff;--surfaceTop:#ffffff;--surface2:#f1f6fd;"
    L"--border:#dce7f5;--text:#13294b;--textDim:#6b7d99;--accent:#2e7dff;"
    L"--accent2:#1e5eff;--accent3:#4da3ff;--accentText:#ffffff;--danger:#e53935;--success:#18b75a;"
    L"--warn:#f59e0b;--inputBg:#ffffff;--inputText:#13294b;"
    L"--r:20px;--r2:13px;--r3:10px;--sp:14px;--sp2:10px;--sp3:7px;"
    L"--shadow:0 1px 2px rgba(30,94,255,.05),0 14px 34px rgba(30,94,255,.09);"
    L"--shadowSm:0 1px 3px rgba(30,94,255,.08);"
    L"--ring:0 0 0 3px rgba(46,125,255,.22);}"
    // ============================ RESET / BASE ===========================
    L"*{box-sizing:border-box;-ms-user-select:none;}"
    L"html,body{height:100%;margin:0;}"
    L"body{font-family:Vazirmatn,Vazir,Tahoma,'Segoe UI',sans-serif;direction:rtl;"
    L"background:"
      L"radial-gradient(1300px 600px at 92% -12%,rgba(46,125,255,.13),transparent 60%),"
      L"radial-gradient(1000px 560px at 3% 112%,rgba(77,163,255,.10),transparent 60%),"
      L"var(--bg);"
    L"color:var(--text);font-size:14.5px;overflow:hidden;line-height:1.55;"
    L"-ms-overflow-style:none;}"
    L"::-webkit-scrollbar{width:10px;height:10px;}"
    L"::-webkit-scrollbar-thumb{background:#cdd8e8;border-radius:7px;}"
    L"::-webkit-scrollbar-thumb:hover{background:#b8c5da;}"
    // ============================== SHELL ================================
    L".wrap{height:100%;display:flex;flex-direction:column;padding:12px 14px;}"
    L".topbar{display:flex;align-items:center;justify-content:space-between;"
    L"gap:12px;padding:0 2px 10px;flex:0 0 auto;}"
    L".brand{display:flex;align-items:center;gap:10px;min-width:0;}"
    L".brand .dot{width:34px;height:34px;border-radius:11px;flex:0 0 auto;"
    L"background:linear-gradient(135deg,var(--accent),var(--accent2));"
    L"display:flex;align-items:center;justify-content:center;color:#fff;"
    L"font-weight:800;font-size:17px;box-shadow:0 6px 16px rgba(31,111,229,.30);}"
    L".brand .crumb{font-weight:800;font-size:16px;white-space:nowrap;}"
    L".brand .crumb i{color:var(--textDim);font-weight:600;font-style:normal;"
    L"font-size:13px;margin:0 7px;}"
    L".meta{color:var(--textDim);font-size:12.5px;white-space:nowrap;overflow:hidden;"
    L"text-overflow:ellipsis;display:flex;align-items:center;gap:8px;}"
    L".meta .pill{background:var(--surface);border:1px solid var(--border);"
    L"border-radius:999px;padding:5px 13px;font-weight:700;color:var(--text);"
    L"box-shadow:var(--shadowSm);font-size:12.5px;}"
    // ========================== THREE-ZONE GRID ==========================
    L".grid{flex:1 1 auto;display:flex;gap:13px;min-height:0;align-items:stretch;}"
    L".zone{display:flex;flex-direction:column;gap:13px;min-height:0;}"
    L".zone-left{flex:0 0 270px;}"
    L".zone-center{flex:1 1 auto;min-width:0;}"
    L".zone-right{flex:0 0 308px;}"
    L".zone.scroll{overflow:auto;padding-bottom:2px;}"
    // ============================= CARDS =================================
    L".card{background:var(--surface);border:1px solid var(--border);"
    L"border-radius:var(--r);padding:15px 16px;box-shadow:var(--shadow);"
    L"position:relative;}"
    L".card.tight{padding:13px 14px;}"
    L".card.flex,.card.fill{flex:1 1 auto;min-height:0;display:flex;flex-direction:column;}"
    L".sect{font-weight:800;color:var(--text);margin:0 0 13px;font-size:14px;"
    L"display:flex;align-items:center;gap:9px;}"
    L".sect .ic{width:28px;height:28px;flex:0 0 auto;border-radius:9px;"
    L"background:linear-gradient(135deg,rgba(46,125,255,.16),rgba(46,125,255,.06));"
    L"color:var(--accent);display:flex;align-items:center;justify-content:center;"
    L"font-size:15px;}"
    L".sect .ic svg{width:16px;height:16px;}"
    L".sect .acc{width:4px;height:17px;border-radius:3px;flex:0 0 auto;"
    L"background:linear-gradient(180deg,var(--accent),var(--accent2));}"
    L".sect.mt{margin-top:15px;padding-top:14px;border-top:1px solid var(--border);}"
    // ============================ FORM LAYOUT ============================
    // Field widths are sized to the *realistic content length* of each field:
    //   نام/نام خانوادگی ~ medium, کد ملی = 10 digits, جنسیت = short select,
    //   نام پدر ~ medium, تاریخ تولد = fixed 10 chars, موبایل = 11, تلفن = ~11,
    //   آدرس = full line.
    L".row{display:flex;flex-wrap:wrap;gap:11px;margin-bottom:11px;}"
    L".row:last-child{margin-bottom:0;}"
    L".fld{display:flex;flex-direction:column;min-width:0;}"
    L".fld label{font-size:12px;color:var(--textDim);margin-bottom:5px;"
    L"font-weight:700;padding-right:3px;}"
    // sizing helpers (flex-basis tuned to content)
    L".f-name{flex:1 1 160px;}"          // نام
    L".f-family{flex:1 1 190px;}"        // نام خانوادگی
    L".f-nid{flex:0 0 168px;}"           // کد ملی (۱۰ رقم + دکمه)
    L".f-gender{flex:0 0 116px;}"        // جنسیت
    L".f-father{flex:1 1 170px;}"        // نام پدر
    L".f-birth{flex:0 0 150px;}"         // تاریخ تولد
    L".f-mobile{flex:0 0 158px;}"        // موبایل (۱۱ رقم)
    L".f-phone{flex:0 0 150px;}"         // تلفن ثابت
    L".f-addr{flex:1 1 100%;}"           // آدرس (تمام عرض)
    L".f-sel{flex:1 1 160px;}"           // selects (نوع بیمار/نوبت/بیمه)
    L".f-money{flex:1 1 170px;}"         // مبلغ/تخفیف
    L".f-half{flex:1 1 47%;}"            // half-width insurance fields
    L".f-third{flex:1 1 30%;}"           // third-width compact fields
    L".fld.wide{flex:1 1 100%;}"
    L".fld.xs{flex:0 0 96px;}"
    // ============================== CONTROLS =============================
    L"input,select,textarea{width:100%;height:40px;border:1px solid var(--border);"
    L"border-radius:var(--r2);background:var(--inputBg);color:var(--inputText);"
    L"padding:0 12px;font-family:inherit;font-size:14px;font-weight:600;outline:none;"
    L"transition:border-color .12s,box-shadow .12s,background .12s;}"
    L"select{cursor:pointer;padding-left:30px;font-weight:700;"
    L"background-image:url(\"data:image/svg+xml;charset=utf8,%3Csvg xmlns='http://www.w3.org/2000/svg' width='11' height='7'%3E%3Cpath d='M0 0l5.5 7 5.5-7z' fill='%23889'/%3E%3C/svg%3E\");"
    L"background-repeat:no-repeat;background-position:11px center;}"
    L"select::-ms-expand{display:none;}"
    L"input:hover,select:hover,textarea:hover{border-color:#b5c4dc;}"
    L"input:focus,select:focus,textarea:focus{border-color:var(--accent);"
    L"box-shadow:var(--ring);background:var(--surface);}"
    L"input[readonly]{background:var(--surface2);color:var(--textDim);cursor:default;}"
    L"input.ltr{direction:ltr;text-align:right;letter-spacing:.4px;}"
    L"input.ltr.center{text-align:center;}"
    L"input::-ms-clear{display:none;}input::placeholder{color:#a6b2c5;font-weight:500;}"
    // a field with an inline action button (e.g. کد ملی + استعلام)
    L".ig{display:flex;gap:7px;}"
    L".ig input{flex:1 1 auto;min-width:0;}"
    L".ig .btn{flex:0 0 auto;height:40px;padding:0 13px;}"
    // ============================== BUTTONS ==============================
    L".btn{height:40px;border:1px solid var(--border);border-radius:var(--r2);"
    L"background:var(--surface);color:var(--text);font-family:inherit;"
    L"font-size:14px;font-weight:700;cursor:pointer;padding:0 17px;"
    L"display:inline-flex;align-items:center;justify-content:center;gap:8px;"
    L"transition:all .12s;white-space:nowrap;box-shadow:var(--shadowSm);}"
    L".btn:hover{background:var(--surface2);border-color:#b5c4dc;}"
    L".btn:active{transform:translateY(1px);}"
    L".btn:focus{box-shadow:var(--ring);}"
    L".btn .hint{font-size:11px;font-weight:600;opacity:.72;}"
    // primary call-to-action: ACCENT BLUE (harmonised with the theme).
    L".btn.primary{background:linear-gradient(135deg,var(--accent),var(--accent2));"
    L"border-color:var(--accent2);color:var(--accentText);box-shadow:0 8px 20px rgba(31,111,229,.28);}"
    L".btn.primary:hover{filter:brightness(1.06);}"
    L".btn.success{background:linear-gradient(135deg,var(--success),#0c8f54);"
    L"border-color:#0c8f54;color:#fff;box-shadow:0 8px 20px rgba(15,166,99,.26);}"
    L".btn.success:hover{filter:brightness(1.05);}"
    L".btn.danger{background:var(--danger);border-color:var(--danger);color:#fff;}"
    L".btn.ghost{background:transparent;border-color:transparent;color:var(--textDim);"
    L"box-shadow:none;}"
    L".btn.ghost:hover{background:var(--surface2);color:var(--text);}"
    L".btn.block{width:100%;}"
    L".btn.sm{height:34px;font-size:12.5px;padding:0 12px;}"
    L".btn.lg{height:48px;font-size:15px;font-weight:800;}"
    L".btn[disabled]{opacity:.5;cursor:not-allowed;}"
    L".btnrow{display:flex;gap:8px;flex-wrap:wrap;}"
    L".btnrow.stack{flex-direction:column;}"
    // compact square icon button (search keys)
    L".btn.ic-btn{padding:0;width:40px;flex:0 0 40px;}"
    L".btn.ic-btn svg{width:17px;height:17px;}"
    // outline variant (secondary actions)
    L".btn.outline{background:transparent;border-color:var(--accent);color:var(--accent);box-shadow:none;}"
    L".btn.outline:hover{background:rgba(46,125,255,.08);}"
    L".btn svg{width:16px;height:16px;flex:0 0 auto;}"
    // bottom action bar (sticky footer of the center form)
    L".footbar{display:flex;gap:11px;flex-wrap:wrap;flex-direction:row-reverse;"
    L"margin-top:13px;padding-top:13px;border-top:1px solid var(--border);}"
    // actions row — the primary «save» button sits on the VISUAL LEFT (LTR end)
    // because in this RTL layout flex children flow right→left, so listing the
    // save button FIRST in markup places it on the right; to honour the request
    // («ثبت پذیرش سمت چپ») we reverse the row direction.
    L".actions{display:flex;gap:11px;flex-wrap:wrap;margin-top:auto;"
    L"padding-top:14px;flex-direction:row-reverse;}"
    // ===================== AVATAR / PATIENT CARD (right) =================
    L".pcard{text-align:center;background:"
    L"linear-gradient(180deg,rgba(31,111,229,.07),transparent 70%),var(--surface);}"
    L".avatar{width:70px;height:70px;border-radius:50%;margin:0 auto 8px;"
    L"background:linear-gradient(135deg,var(--accent),var(--accent2));color:#fff;"
    L"display:flex;align-items:center;justify-content:center;font-size:29px;"
    L"font-weight:800;box-shadow:0 8px 18px rgba(31,111,229,.32);"
    L"border:3px solid var(--surface);}"
    L".pname{font-weight:800;font-size:15px;}"
    L".psub{color:var(--textDim);font-size:12px;margin-top:3px;}"
    L".chip{display:inline-flex;align-items:center;gap:5px;padding:5px 12px;"
    L"border-radius:999px;background:var(--surface2);color:var(--textDim);"
    L"font-size:11.5px;font-weight:700;margin:9px 3px 0;border:1px solid var(--border);}"
    L".chip.ok{background:rgba(15,166,99,.12);color:var(--success);"
    L"border-color:rgba(15,166,99,.25);}"
    L".pcounts{display:flex;gap:8px;margin-top:11px;flex-wrap:wrap;}"
    L".pcounts .c{flex:1 1 30%;background:var(--surface2);border:1px solid var(--border);"
    L"border-radius:var(--r2);padding:8px 4px;text-align:center;}"
    L".pcounts .c .k{font-size:10.5px;color:var(--textDim);font-weight:700;}"
    L".pcounts .c .v{font-size:16px;font-weight:800;margin-top:2px;"
    L"font-variant-numeric:tabular-nums;}"
    // patient status mini-widgets (electronic version / status / insurance state)
    L".pstat{display:flex;flex-direction:column;gap:7px;margin-top:11px;}"
    L".pstat .w{display:flex;align-items:center;justify-content:space-between;"
    L"background:var(--surface2);border:1px solid var(--border);border-radius:var(--r3);"
    L"padding:7px 11px;font-size:12px;}"
    L".pstat .w .k{color:var(--textDim);font-weight:700;}"
    L".pstat .w .v{font-weight:800;}"
    L".pstat .w .v.on{color:var(--success);} .pstat .w .v.off{color:var(--textDim);}"
    // ============================= BILLING (left) ========================
    L".bill .line{display:flex;justify-content:space-between;align-items:center;"
    L"padding:8px 2px;border-bottom:1px dashed var(--border);font-size:13.5px;}"
    L".bill .line:last-of-type{border-bottom:none;}"
    L".bill .line span{color:var(--textDim);font-weight:600;}"
    L".bill .line b{font-variant-numeric:tabular-nums;font-weight:800;}"
    L".bill .line .u{font-size:10.5px;color:var(--textDim);font-weight:600;margin-right:3px;}"
    L".bill .grp{font-size:11.5px;font-weight:800;color:var(--accent);"
    L"margin:11px 0 3px;letter-spacing:.2px;}"
    L".bill .grp:first-child{margin-top:0;}"
    L".bill .pay{margin-top:13px;"
    L"background:linear-gradient(135deg,var(--success),#0c8f54);"
    L"color:#fff;border-radius:var(--r2);padding:13px 15px;display:flex;"
    L"justify-content:space-between;align-items:center;font-weight:800;font-size:18px;"
    L"box-shadow:0 8px 20px rgba(15,166,99,.30);}"
    L".bill .pay b{font-variant-numeric:tabular-nums;}"
    // =========================== APPOINTMENT TABLE =======================
    L".tablewrap{flex:1 1 auto;overflow:auto;border:1px solid var(--border);"
    L"border-radius:var(--r2);min-height:0;background:var(--surface);}"
    L"table{width:100%;border-collapse:collapse;font-size:13px;}"
    L"th,td{border-bottom:1px solid var(--border);padding:9px;text-align:center;"
    L"white-space:nowrap;}"
    L"th{background:var(--surface2);color:var(--textDim);position:sticky;top:0;"
    L"font-weight:800;z-index:1;font-size:12px;}"
    L"tbody tr:hover{background:var(--surface2);}"
    L"tr.cancel td{text-decoration:line-through;color:var(--textDim);}"
    L".empty{padding:38px 14px;text-align:center;color:var(--textDim);font-size:14px;}"
    // ============================== TOAST ================================
    L".toast{position:fixed;left:50%;bottom:24px;transform:translateX(-50%) translateY(8px);"
    L"background:var(--text);color:#fff;padding:12px 24px;border-radius:13px;"
    L"opacity:0;transition:opacity .22s,transform .22s;font-size:14px;z-index:80;"
    L"box-shadow:0 14px 34px rgba(0,0,0,.28);pointer-events:none;font-weight:700;}"
    L".toast.show{opacity:.99;transform:translateX(-50%) translateY(0);}"
    L".toast.bad{background:var(--danger);}"
    L".toast.ok{background:linear-gradient(135deg,var(--success),#0c8f54);}"
    // ===================== COMPACT MODE (small screens) ==================
    L"body.compact{font-size:13px;}"
    L"body.compact .card{padding:11px 12px;}"
    L"body.compact input,body.compact select{height:36px;}"
    L"body.compact .btn{height:37px;}"
    L"body.compact .row{margin-bottom:8px;}"
    L"body.compact .avatar{width:58px;height:58px;font-size:24px;}"
    // ========================= RESPONSIVE BREAKPOINTS ====================
    L"@media (max-width:1240px){.zone-left{flex-basis:238px;}.zone-right{flex-basis:276px;}}"
    L"@media (max-width:1024px){.grid{flex-direction:column;overflow:auto;}"
    L".zone-left,.zone-right{flex-basis:auto;}.zone-center{min-width:0;}"
    L".f-nid,.f-gender,.f-birth,.f-mobile,.f-phone{flex:1 1 140px;}}"
    L"@media (max-width:560px){.fld{flex:1 1 100% !important;}}"
    L"</style>";
}

// The bridge JS shared by both pages: theme application, the azReceive push
// channel, a synchronous external.call helper, keyboard navigation (Enter ->
// next field, Tab/Shift+Tab native), keyboard-safe dropdowns, tab switching and
// a toast. C++ stays the source of truth; this layer is presentation only.
static std::wstring wh_commonScript(){
    return
    L"<script type='text/javascript'>"
    // ---- JSON polyfill guard (ES3-safe) ----------------------------------
    // FEATURE_BROWSER_EMULATION promotes us to IE11 standards mode where JSON,
    // querySelectorAll, addEventListener etc. all exist. As a belt-and-braces
    // for any degraded Trident that still lacks the native JSON object, install
    // a tiny compatible stringify/parse so the bridge can never die on the very
    // first call (which is what produced the 'Object doesn't support…' error).
    L"if(typeof JSON==='undefined'||!JSON.stringify){window.JSON=window.JSON||{};"
    L" var _esc=function(s){return '\"'+String(s).replace(/[\\\\\"\\u0000-\\u001f]/g,"
    L"  function(c){var m={'\\\\':'\\\\\\\\','\"':'\\\\\"','\\n':'\\\\n','\\r':'\\\\r','\\t':'\\\\t'};"
    L"  return m[c]||('\\\\u'+('0000'+c.charCodeAt(0).toString(16)).slice(-4));})+'\"';};"
    L" JSON.stringify=JSON.stringify||function(v){"
    L"  if(v===null||typeof v==='undefined')return 'null';"
    L"  if(typeof v==='number')return isFinite(v)?String(v):'null';"
    L"  if(typeof v==='boolean')return String(v);"
    L"  if(typeof v==='string')return _esc(v);"
    L"  var i,a;"
    L"  if(Object.prototype.toString.call(v)==='[object Array]'){a=[];"
    L"   for(i=0;i<v.length;i++)a.push(JSON.stringify(v[i]));return '['+a.join(',')+']';}"
    L"  a=[];for(i in v){if(v.hasOwnProperty(i)&&typeof v[i]!=='function')"
    L"   a.push(_esc(i)+':'+JSON.stringify(v[i]));}return '{'+a.join(',')+'}';};"
    L" JSON.parse=JSON.parse||function(s){return eval('('+s+')');}; }"
    L"var AZ={kind:0,boot:null,ready:false};"
    // synchronous bridge call; never throws across the COM boundary.
    L"function call(verb,args){try{"
    L"  if(!window.external||!window.external.call)return {};"
    L"  var s=window.external.call(verb, args?JSON.stringify(args):'{}');"
    L"  return s?JSON.parse(s):{};"
    L"}catch(e){ if(window.external&&window.external.log)window.external.log('call '+verb+':'+e); return {}; }}"
    L"function logErr(m){try{if(window.external&&window.external.log)window.external.log(''+m);}catch(e){}}"
    // detect native CSS custom-property (var()) support. IE11 / older Trident
    // do NOT support var(), so we must resolve tokens ourselves (ponyfill).
    L"var AZ_VARS_OK=(function(){try{return !!(window.CSS&&CSS.supports&&CSS.supports('--a','0'));}catch(e){return false;}})();"
    // remember the raw CSS text of our style sheets so the ponyfill can re-resolve
    // var(--token) -> concrete value whenever the theme changes.
    L"var AZ_CSS_RAW=null;"
    L"function azCollectCss(){if(AZ_CSS_RAW!==null)return;var out=[];"
    L"  var ss=document.getElementsByTagName('style');"
    L"  for(var i=0;i<ss.length;i++){ if(ss[i].getAttribute('data-az')==='1') continue;"
    L"    out.push(ss[i].innerHTML||ss[i].textContent||'');} AZ_CSS_RAW=out.join('\\n');}"
    // current token map (token -> #hex). Seeded from the :root defaults so the
    // ponyfill has values even before the first theme push.
    L"var AZ_TOK={bg:'#eef3fb',surface:'#ffffff',surfaceTop:'#ffffff',surface2:'#f3f7fd',"
    L"  border:'#dbe4f1',text:'#16263c',textDim:'#64748b',accent:'#1f6fe5',accent2:'#1657c4',"
    L"  accentText:'#ffffff',danger:'#e0413f',success:'#0fa663',warn:'#d97706',"
    L"  inputBg:'#ffffff',inputText:'#16263c',"
    L"  r:'18px',r2:'12px',r3:'9px',sp:'14px',sp2:'10px',sp3:'7px',"
    L"  shadow:'0 1px 2px rgba(20,40,80,.05),0 12px 30px rgba(20,40,80,.08)',"
    L"  shadowSm:'0 1px 3px rgba(20,40,80,.07)',"
    L"  ring:'0 0 0 3px rgba(31,111,229,.20)'};"
    // resolve every var(--x[, fallback]) in the collected CSS to a literal value,
    // then write the result into a single managed <style data-az='1'> element.
    L"function azPonyResolve(){ azCollectCss(); if(AZ_CSS_RAW===null)return;"
    L"  var css=AZ_CSS_RAW.replace(/var\\(\\s*--([a-zA-Z0-9_-]+)\\s*(?:,\\s*([^)]*))?\\)/g,"
    L"    function(m,name,fb){ var v=AZ_TOK[name]; if(typeof v!=='undefined')return v;"
    L"      return (typeof fb!=='undefined'&&fb!==null)?fb:'inherit'; });"
    L"  var el=document.getElementById('az-theme');"
    L"  if(!el){el=document.createElement('style');el.id='az-theme';el.setAttribute('data-az','1');"
    L"    var hd=document.getElementsByTagName('head')[0]||document.documentElement;hd.appendChild(el);}"
    L"  if(el.styleSheet){el.styleSheet.cssText=css;}else{el.innerHTML=css;} }"
    // theme: apply hex tokens. On modern engines we set real custom properties;
    // on IE11/old Trident we update AZ_TOK and re-run the ponyfill.
    L"function applyTheme(t){if(!t)return;var k;"
    L"  for(k in t){ if(typeof t[k]==='string'&&t[k].charAt(0)==='#'){ AZ_TOK[k]=t[k];"
    L"     if(AZ_VARS_OK){try{document.documentElement.style.setProperty('--'+k,t[k]);}catch(_){}} } }"
    L"  if(!AZ_VARS_OK){ azPonyResolve(); }"
    L"  if(typeof t.compact!=='undefined'){ document.body.className=t.compact?'compact':''; } }"
    // on first load (before any push) make sure the ponyfill paints the defaults
    // so the page is never unstyled on IE11.
    L"if(!AZ_VARS_OK){ if(window.attachEvent){window.attachEvent('onload',azPonyResolve);} "
    L"  else if(document.addEventListener){document.addEventListener('DOMContentLoaded',azPonyResolve,false);} "
    L"  setTimeout(azPonyResolve,0); }"
    L"function toast(msg,kind){var t=document.getElementById('toast');if(!t)return;"
    L"  t.innerHTML=msg; t.className='toast show '+(kind||'');"
    L"  if(t._tm)clearTimeout(t._tm);"
    L"  t._tm=setTimeout(function(){t.className='toast '+(kind||'');},2300);}"
    // navigation: ordered list of focusable [data-nav] controls currently shown.
    L"function navList(){var n=document.querySelectorAll('[data-nav]');var a=[];"
    L"  for(var i=0;i<n.length;i++){ if(!n[i].disabled&&n[i].offsetParent!==null) a.push(n[i]); } return a;}"
    L"function focusNext(cur,back){var list=navList();var idx=-1;"
    L"  for(var i=0;i<list.length;i++){if(list[i]===cur){idx=i;break;}}"
    L"  if(idx<0)return;var nx=list[idx+(back?-1:1)];"
    L"  if(nx){nx.focus();try{if(nx.select)nx.select();}catch(_){}}"
    L"  else if(!back&&window.azSubmit){azSubmit();}}"
    // Enter moves to next field (or submits on data-enter='submit'); a SELECT
    // with the keyboard open lets the browser handle Enter (keyboard-safe).
    L"function onKey(ev){var e=ev||window.event;var k=e.keyCode||e.which;"
    L"  var tg=e.target||e.srcElement;"
    L"  if(k===13){"
    L"    if(tg&&tg.tagName==='TEXTAREA')return true;"
    L"    if(tg&&tg.getAttribute&&tg.getAttribute('data-enter')==='submit'){ if(window.azSubmit)azSubmit();"
    L"      if(e.preventDefault)e.preventDefault();return false; }"
    L"    focusNext(tg,false);"
    L"    if(e.preventDefault)e.preventDefault(); return false; }"
    L"  return true; }"
    L"document.onkeydown=onKey;"
    // tab switching (presentation only; the native tab strip remains authority).
    L"function azTab(name){var ts=document.querySelectorAll('.tab');"
    L"  for(var i=0;i<ts.length;i++){var on=ts[i].getAttribute('data-tab')===name;"
    L"    ts[i].className='tab'+(on?' active':'');}"
    L"  var ps=document.querySelectorAll('[data-pane]');"
    L"  for(var j=0;j<ps.length;j++){ps[j].style.display=(ps[j].getAttribute('data-pane')===name)?'':'none';}"
    L"  if(window.azOnTab)azOnTab(name);}"
    L"window.azTab=azTab;"
    // the C++ -> JS push channel. Tolerant of partial/early payloads.
    L"function azReceive(s){ var d; try{d=JSON.parse(s);}catch(e){logErr('recv parse');return;} "
    L"  if(!d)return;"
    L"  if(d.event==='boot'){ AZ.boot=d; AZ.kind=d.kind; applyTheme(d.theme);"
    L"     if(typeof d.dark!=='undefined'&&!d.theme){} "
    L"     try{ if(window.azBoot)azBoot(d); AZ.ready=true; }catch(e){logErr('boot:'+e);} } "
    L"  else if(d.event==='theme'){ applyTheme(d); } "
    L"  else if(d.event==='refresh'){ if(window.azRefresh)azRefresh(d); } "
    L"  else if(window.azEvent){ try{azEvent(d);}catch(e){logErr('event:'+e);} } }"
    L"window.azReceive=azReceive;"
    // small shared helpers
    L"function $(id){return document.getElementById(id);}"
    L"function fmt(n){n=Math.round(n||0);var s=''+Math.abs(n);var o='';"
    L"  for(var i=0;i<s.length;i++){if(i>0&&(s.length-i)%3===0)o+=',';o+=s.charAt(i);}return(n<0?'-':'')+o;}"
    L"function fa(s){var m={'0':'\u06f0','1':'\u06f1','2':'\u06f2','3':'\u06f3','4':'\u06f4','5':'\u06f5','6':'\u06f6','7':'\u06f7','8':'\u06f8','9':'\u06f9'};"
    L"  return (''+s).replace(/[0-9]/g,function(d){return m[d];});}"
    // num(): tolerant numeric parse — converts Persian/Arabic digits to Latin,
    // then strips grouping/non-digits. Needed because money inputs now show
    // Persian-grouped values.
    L"function num(s){s=(''+s).replace(/[\u06f0-\u06f9]/g,function(d){return '\u06f0\u06f1\u06f2\u06f3\u06f4\u06f5\u06f6\u06f7\u06f8\u06f9'.indexOf(d);})"
    L"  .replace(/[\u0660-\u0669]/g,function(d){return '\u0660\u0661\u0662\u0663\u0664\u0665\u0666\u0667\u0668\u0669'.indexOf(d);});"
    L"  return parseInt((s.replace(/[^0-9]/g,''))||'0',10);}"
    L"function esc(s){return (''+(s==null?'':s)).replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;');}"
    L"function fillSel(sel,arr,useIdx){if(!sel)return;sel.innerHTML='';"
    L"  for(var i=0;i<arr.length;i++){var o=document.createElement('option');"
    L"    o.value=(useIdx&&typeof arr[i].idx!=='undefined')?arr[i].idx:i;"
    L"    o.text=arr[i].name||arr[i];sel.appendChild(o);}}"
    L"</script>";
}

// ---------------------------------------------------------- reception JS -----
static std::wstring wh_receptionScript(){
    return
    L"<script type='text/javascript'>"
    L"function setHeader(d){"
    L"  var sd=$('s_dept');if(sd)sd.innerHTML=esc(d.userDept||'\u2014');"
    L"  var ss=$('s_shift');if(ss)ss.innerHTML=esc(d.shift);"
    L"  var su=$('s_user');if(su)su.innerHTML=esc(d.userName);"
    L"  var st=$('s_today');if(st)st.innerHTML=esc(d.today);}"
    // populate the base + supplemental insurance dropdowns from a payload, and
    // if the pushed payload was empty PULL the lists directly from C++ so the
    // selects are NEVER left empty (root-cause fix for the blank insurance list).
    L"function fillInsurance(d){"
    L"  var insArr=(d&&d.insurances&&d.insurances.length)?d.insurances:null;"
    L"  var supArr=(d&&d.supp&&d.supp.length)?d.supp:null;"
    L"  if(!insArr){ var ri=call('insurances'); if(ri&&ri.length)insArr=ri; }"
    L"  if(!supArr){ var rs=call('supp'); if(rs&&rs.length)supArr=rs; }"
    L"  if(!insArr)insArr=[{idx:0,name:'\\u0622\\u0632\\u0627\\u062f (\\u0628\\u062f\\u0648\\u0646 \\u0628\\u06cc\\u0645\\u0647)'}];"
    L"  if(!supArr)supArr=[{idx:0,name:'\\u0646\\u062f\\u0627\\u0631\\u062f'}];"
    L"  fillSel($('ins'),insArr,true); fillSel($('supp'),supArr,true);"
    L"}"
    L"function azBoot(d){"
    L"  setHeader(d);"
    L"  fillInsurance(d);"
    L"  recalc(); var n=$('first'); if(n)n.focus();"
    L"}"
    // helper: numeric value of a control, tolerant of missing nodes
    L"function v(id){var e=$(id);return e?e.value:'';}"
    // live رﯾﺎﻝ thousands grouping inside a money input (keeps caret at end,
    // strips non-digits, then re-inserts comma separators).
    L"function moneyMask(el){if(!el)return;var raw=(''+el.value).replace(/[^0-9\\u06f0-\\u06f9]/g,'');"
    L"  raw=raw.replace(/[\\u06f0-\\u06f9]/g,function(d){return '\\u06f0\\u06f1\\u06f2\\u06f3\\u06f4\\u06f5\\u06f6\\u06f7\\u06f8\\u06f9'.indexOf(d);});"
    L"  if(raw==='') {el.value='';return;} el.value=fa(fmt(parseInt(raw,10)));}"
    L"function recalc(){"
    L"  var r=call('bill',{price:num(v('price')),discount:num(v('discount')),"
    L"    ptype:$('ptype').selectedIndex,ntype:$('ntype').selectedIndex,"
    L"    ins:parseInt(v('ins')||'0',10),supp:parseInt(v('supp')||'0',10)});"
    L"  if(!r||typeof r.price==='undefined')return;"
    L"  var U=\"<span class='u'>\\u0631\\u06cc\\u0627\\u0644</span>\";"
    L"  function st(id,val){var e=$(id);if(e)e.innerHTML=fa(fmt(val))+U;}"
    L"  st('b_total',r.price); st('b_main',r.mainShare);"
    L"  st('b_diff',r.baseDiff); st('b_org',r.orgShare);"
    L"  st('b_pat',r.patientShare); st('b_disc',num(v('discount')));"
    L"  if($('b_paid'))$('b_paid').innerHTML=fa(fmt(r.paid));"
    // reflect insurance coverage % into the right-panel widget
    L"  var pct=(r.price>0)?Math.round(r.mainShare*100/r.price):0;"
    L"  if($('insCov'))$('insCov').value=fa(pct)+'\u066a';"
    // auto-fill the service price (formatted) when the user left it blank/zero.
    L"  if(num(v('price'))===0){ $('price').value=fa(fmt(r.price)); }"
    L"}"
    L"function statW(id,txt,on){var e=$(id);if(!e)return;e.innerHTML=esc(txt);"
    L"  e.className='v '+(on?'on':'off');}"
    L"function setPatient(r,found){var a=$('p_avatar'),nm=$('p_name'),sb=$('p_sub');"
    L"  if(found){var fn=((r.first||'')+' '+(r.last||'')).replace(/^ +| +$/g,'');"
    L"    if(a)a.innerHTML=esc((r.first||r.last||'?').substr(0,1));"
    L"    if(nm)nm.innerHTML=esc(fn||'\u2014');"
    L"    if(sb)sb.innerHTML=esc((r.gender||'')+(r.birth?(' \u00b7 '+r.birth):''));"
    L"    statW('w_status','\u062f\u0627\u0631\u0627\u06cc \u0633\u0627\u0628\u0642\u0647',true);"
    L"    statW('w_ins','\u062f\u0627\u0631\u0627\u06cc \u0628\u06cc\u0645\u0647',true);}"
    L"  else{if(a)a.innerHTML='?';if(nm)nm.innerHTML='\u0628\u06cc\u0645\u0627\u0631 \u062c\u062f\u06cc\u062f';if(sb)sb.innerHTML='\u0628\u062f\u0648\u0646 \u0633\u0627\u0628\u0642\u0647';"
    L"    statW('w_status','\u062c\u062f\u06cc\u062f',false);"
    L"    statW('w_ins','\u0646\u0627\u0645\u0634\u062e\u0635',false);"
    L"    if($('w_file'))$('w_file').innerHTML='\u2014';}}"
    L"function lookupNid(nid){if(!nid||nid.length<10)return;"
    L"  var r=call('lookup',{nid:nid});"
    L"  if(r.found){ $('nid').value=nid; $('first').value=r.first||''; $('last').value=r.last||'';"
    L"    $('father').value=r.father||''; $('birth').value=r.birth||''; $('mobile').value=r.mobile||'';"
    L"    if(r.gender){$('gender').value=r.gender;} setPatient(r,true);"
    L"    toast('\u0628\u06cc\u0645\u0627\u0631 \u06cc\u0627\u0641\u062a \u0634\u062f','ok'); $('ptype').focus(); }"
    L"  else if(!r.idValid){ toast('\u06a9\u062f \u0645\u0644\u06cc \u0646\u0627\u0645\u0639\u062a\u0628\u0631','bad'); }"
    L"  else { setPatient({},false); toast('\u0628\u06cc\u0645\u0627\u0631 \u062c\u062f\u06cc\u062f','');} }"
    L"function lookup(){lookupNid(v('nid'));}"
    L"function clearForm(){var ids=['nid','nidSearch','first','last','father','birth','mobile','phone','address','price','discount','archive','file','book','insExp','insBranch','docCode','docName','docRef'];"
    L"  for(var i=0;i<ids.length;i++){var el=$(ids[i]);if(el)el.value='';}"
    L"  if($('ptype'))$('ptype').selectedIndex=0; if($('ntype'))$('ntype').selectedIndex=0;"
    L"  if($('ins'))$('ins').selectedIndex=0; if($('supp'))$('supp').selectedIndex=0;"
    L"  setPatient({},false); recalc();"
    L"  var n=$('nid'); if(n)n.focus();}"
    // collect the full reception payload from the form (shared by save & print).
    L"function collect(){var insSel=$('ins'),suppSel=$('supp');var pt=$('ptype'),nt=$('ntype');"
    L"  return {first:v('first'),last:v('last'),nid:v('nid'),"
    L"    father:v('father'),birth:v('birth'),gender:v('gender'),mobile:v('mobile'),"
    L"    phone:v('phone'),address:v('address'),"
    L"    ptypeName:(pt&&pt.options[pt.selectedIndex])?pt.options[pt.selectedIndex].text:'',"
    L"    ntypeName:(nt&&nt.options[nt.selectedIndex])?nt.options[nt.selectedIndex].text:'',"
    L"    ptype:pt?pt.selectedIndex:0,ntype:nt?nt.selectedIndex:0,"
    L"    insName:(insSel&&insSel.options[insSel.selectedIndex])?insSel.options[insSel.selectedIndex].text:'',"
    L"    suppName:(suppSel&&suppSel.options[suppSel.selectedIndex])?suppSel.options[suppSel.selectedIndex].text:'',"
    L"    ins:parseInt((insSel?insSel.value:'0')||'0',10),supp:parseInt((suppSel?suppSel.value:'0')||'0',10),"
    L"    price:num(v('price')),discount:num(v('discount'))};}"
    // PRIMARY action: atomic save + print of the receipt. C++ saves first and
    // ONLY prints when the save truly succeeded — this is the fix for the old
    // «چاپ → خطای ثبت» bug (no two-step race, no print against a stale record).
    L"function azSubmit(){"
    L"  var r=call('saveAndPrint',collect());"
    L"  if(r.ok){ var m='\u067e\u0630\u06cc\u0631\u0634 \u062b\u0628\u062a \u0634\u062f \u2014 \u0646\u0648\u0628\u062a '+fa(r.queue);"
    L"    if(r.printed)m+=' \u00b7 \u0631\u0633\u06cc\u062f \u0686\u0627\u067e \u0634\u062f';"
    L"    toast(m,'ok'); clearForm(); }"
    L"  else { toast(r.error||'\u062e\u0637\u0627 \u062f\u0631 \u062b\u0628\u062a \u067e\u0630\u06cc\u0631\u0634','bad'); } }"
    L"window.azSubmit=azSubmit;"
    // save WITHOUT printing (kept for completeness / future use).
    L"function azSave(){var r=call('saveReception',collect());"
    L"  if(r.ok){toast('\u067e\u0630\u06cc\u0631\u0634 \u062b\u0628\u062a \u0634\u062f \u2014 \u0646\u0648\u0628\u062a '+fa(r.queue),'ok');clearForm();}"
    L"  else{toast(r.error||'\u062e\u0637\u0627 \u062f\u0631 \u062b\u0628\u062a \u067e\u0630\u06cc\u0631\u0634','bad');}}"
    L"function doPrint(k){var r=call('print',{kind:k}); if(!r||r.ok!==true)toast(r&&r.error?r.error:'\u0686\u0627\u067e \u0627\u0646\u062c\u0627\u0645 \u0646\u0634\u062f','bad');}"
    L"function on(id,ev,fn){var e=$(id);if(e)e[ev]=fn;}"
    // ---- smart Jalali birth-date mask ------------------------------------
    // The user types only digits (e.g. 14001021). We normalise Persian/Arabic
    // digits to Latin, keep at most 8 digits and rebuild as YYYY/MM/DD with the
    // slashes inserted automatically. Because the value is rebuilt from the bare
    // digits, pressing Backspace over a slash naturally removes the slash AND the
    // preceding digit; Ctrl+A then Delete clears the whole field. The maskKey
    // handler lets Ctrl/Meta combos (select-all, copy, paste) pass through.
    L"function dateDigits(s){s=(''+s);"
    L"  s=s.replace(/[\\u06f0-\\u06f9]/g,function(d){return '\\u06f0\\u06f1\\u06f2\\u06f3\\u06f4\\u06f5\\u06f6\\u06f7\\u06f8\\u06f9'.indexOf(d);});"
    L"  s=s.replace(/[\\u0660-\\u0669]/g,function(d){return '\\u0660\\u0661\\u0662\\u0663\\u0664\\u0665\\u0666\\u0667\\u0668\\u0669'.indexOf(d);});"
    L"  return s.replace(/[^0-9]/g,'').substr(0,8);}"
    L"function dateFormat(el){if(!el)return;var d=dateDigits(el.value);var o=d.substr(0,4);"
    L"  if(d.length>4)o+='/'+d.substr(4,2); if(d.length>6)o+='/'+d.substr(6,2); el.value=o;}"
    L"function wire(){"
    L"  on('nid','onkeyup',function(e){var k=(e||window.event).keyCode; if(v('nid').length===10&&k!==13)lookup();});"
    L"  on('birth','oninput',function(){dateFormat($('birth'));});"
    L"  on('birth','onkeyup',function(e){var ev=e||window.event; if(ev.ctrlKey||ev.metaKey)return true; dateFormat($('birth'));});"
    L"  on('price','onkeyup',function(){moneyMask($('price'));recalc();}); "
    L"  on('discount','onkeyup',function(){moneyMask($('discount'));recalc();});"
    L"  on('ptype','onchange',function(){$('price').value='';recalc();});"
    L"  on('ntype','onchange',function(){$('price').value='';recalc();});"
    L"  on('ins','onchange',recalc); on('supp','onchange',recalc);"
    L"  on('btnSave','onclick',azSubmit); on('btnNew','onclick',clearForm);"
    L"  on('btnClear','onclick',clearForm);"
    L"  on('btnCancel','onclick',function(){clearForm();toast('\u0627\u0646\u0635\u0631\u0627\u0641 \u2014 \u0641\u0631\u0645 \u067e\u0627\u06a9 \u0634\u062f','');});"
    L"  on('p_ins','onclick',function(){doPrint(0);});"
    L"  on('p_rx','onclick',function(){doPrint(1);});"
    L"  on('p_last','onclick',function(){var r=call('printLast'); if(!r||r.ok!==true)toast(r&&r.error?r.error:'\u0686\u0627\u067e \u0627\u0646\u062c\u0627\u0645 \u0646\u0634\u062f','bad');});"
    L"  on('p_reprint','onclick',function(){var r=call('print',{kind:2}); if(!r||r.ok!==true)toast(r&&r.error?r.error:'\u0686\u0627\u067e \u0627\u0646\u062c\u0627\u0645 \u0646\u0634\u062f','bad'); else toast('\u0686\u0627\u067e \u0645\u062c\u062f\u062f \u0627\u0646\u062c\u0627\u0645 \u0634\u062f','ok');});"
    L"  on('btnSearch','onclick',function(){var n=$('nid'); if(n){n.focus();lookup();}});"
    L"  on('q_nid','onclick',function(){lookupNid(v('nidSearch'));});"
    L"  on('nidSearch','onkeyup',function(e){var k=(e||window.event).keyCode; if(k===13){lookupNid(v('nidSearch'));} else if(v('nidSearch').length===10){lookupNid(v('nidSearch'));}});"
    L"  on('q_archive','onclick',function(){toast('\u062c\u0633\u062a\u062c\u0648 \u0628\u0631 \u0627\u0633\u0627\u0633 \u0634\u0645\u0627\u0631\u0647 \u0628\u06cc\u0645\u0647','');});"
    L"  on('q_file','onclick',function(){toast('\u062c\u0633\u062a\u062c\u0648 \u0628\u0631 \u0627\u0633\u0627\u0633 \u0634\u0645\u0627\u0631\u0647 \u067e\u0631\u0648\u0646\u062f\u0647','');});"
    // Ctrl+R clears the form (= «پذیرش جدید»). Ctrl+A inside an input must NOT be
    // hijacked, so we only intercept Ctrl+R / Ctrl+Enter and otherwise fall back
    // to the shared navigation handler.
    L"  document.onkeydown=function(e){e=e||window.event;var k=e.keyCode||e.which;"
    L"    if((e.ctrlKey||e.metaKey)&&(k===82)){clearForm();if(e.preventDefault)e.preventDefault();return false;}"
    L"    if((e.ctrlKey||e.metaKey)&&k===13){if(window.azSubmit)azSubmit();if(e.preventDefault)e.preventDefault();return false;}"
    L"    return onKey(e);};"
    L"}"
    L"wire(); call('ping');"
    // §self-hydration: if no boot push arrived shortly after load (timing race
    // with the native reveal/watchdog), pull the payload ourselves so the form
    // is always populated (header pills + insurance lists + tariff).
    L"function selfHydrate(){ if(AZ.ready)return; var d=call('boot');"
    L"  if(d&&d.event==='boot'){ try{azReceive(JSON.stringify(d));}catch(e){ try{azBoot(d);AZ.ready=true;}catch(_){} } }"
    L"  else { fillInsurance(null); recalc(); } }"
    L"setTimeout(selfHydrate,350); setTimeout(function(){ if(!$('ins')||!$('ins').options.length)fillInsurance(null); },900);"
    L"</script>";
}

// --------------------------------------------------------- appointment JS ----
static std::wstring wh_appointmentScript(){
    return
    L"<script type='text/javascript'>"
    L"var DOCS=[];"
    L"function setHeader(d){var m=$('meta');if(m)m.innerHTML="
    L"  esc(d.userName)+' \u2014 '+esc(d.userDept)+' \u2014 '+esc(d.shift)+' \u2014 '+esc(d.today);}"
    L"function fillDoctors(){var sel=$('doctor');if(!sel)return;sel.innerHTML='';"
    L"  for(var i=0;i<DOCS.length;i++){var o=document.createElement('option');o.value=i;"
    L"    o.text=DOCS[i].name+' \u2014 '+DOCS[i].specialty;sel.appendChild(o);}fillServices();}"
    L"function fillServices(){var sel=$('service');if(!sel)return;sel.innerHTML='';"
    L"  var d=DOCS[$('doctor').selectedIndex];if(!d)return;"
    L"  for(var i=0;i<d.services.length;i++){var o=document.createElement('option');o.text=d.services[i];sel.appendChild(o);}}"
    L"function azBoot(d){"
    L"  setHeader(d); $('date').value=d.today; DOCS=d.doctors||[]; fillDoctors(); refresh();"
    L"  var n=$('nid'); if(n)n.focus();"
    L"}"
    L"function azRefresh(){refresh();}"
    L"function refresh(){var rows=call('apptList'); if(!rows||!rows.length){rows=rows||[];}"
    L"  var tb=$('gbody'); if(!tb)return; tb.innerHTML='';"
    L"  if(!rows.length){var tr=document.createElement('tr');"
    L"    tr.innerHTML='<td colspan=\"9\" class=\"empty\">\u0647\u0646\u0648\u0632 \u0646\u0648\u0628\u062a\u06cc \u062b\u0628\u062a \u0646\u0634\u062f\u0647 \u0627\u0633\u062a</td>';tb.appendChild(tr);"
    L"    var c=$('a_count'); if(c)c.innerHTML=fa(0); return;}"
    L"  for(var i=0;i<rows.length;i++){var r=rows[i];var tr=document.createElement('tr'); if(r.cancelled)tr.className='cancel';"
    L"    var btn=r.cancelled?'\u2014':'<button class=\"btn danger sm\" onclick=\"cancelRow('+r.idx+')\">\u2716</button>';"
    L"    tr.innerHTML='<td>'+btn+'</td><td>'+fa(r.queue)+'</td><td>'+fa(r.time)+'</td><td>'+fa(r.date)+'</td><td>'+esc(r.first)+'</td><td>'+esc(r.last)+'</td><td>'+esc(r.doctor)+'</td><td>'+esc(r.service)+'</td><td>'+esc(r.kind)+'</td>';"
    L"    tb.appendChild(tr);}"
    L"  var c=$('a_count'); if(c)c.innerHTML=fa(rows.length);}"
    L"function cancelRow(idx){var r=call('cancelAppointment',{idx:idx}); if(r.ok){toast('\u0646\u0648\u0628\u062a \u0627\u0628\u0637\u0627\u0644 \u0634\u062f','ok');refresh();}}"
    L"window.cancelRow=cancelRow;"
    L"function lookup(){var nid=$('nid').value;if(nid.length<10)return;var r=call('lookup',{nid:nid});"
    L"  if(r.found){$('first').value=r.first||'';$('last').value=r.last||'';$('mobile').value=r.mobile||'';toast('\u0628\u06cc\u0645\u0627\u0631 \u06cc\u0627\u0641\u062a \u0634\u062f','ok');}}"
    L"function clearForm(){var ids=['nid','first','last','mobile','time'];for(var i=0;i<ids.length;i++){var el=$(ids[i]);if(el)el.value='';}var n=$('nid');if(n)n.focus();}"
    L"function azSubmit(){var d=DOCS[$('doctor').selectedIndex]||{};"
    L"  var r=call('saveAppointment',{nid:$('nid').value,first:$('first').value,last:$('last').value,mobile:$('mobile').value,"
    L"    doctor:d.name||'',service:$('service').value,date:$('date').value,time:$('time').value,kind:$('kind').value});"
    L"  if(r.ok){toast('\u0646\u0648\u0628\u062a \u062b\u0628\u062a \u0634\u062f \u2014 \u0634\u0645\u0627\u0631\u0647 '+fa(r.queue),'ok');clearForm();refresh();}"
    L"  else{toast(r.error||'\u062e\u0637\u0627 \u062f\u0631 \u062b\u0628\u062a','bad');} }"
    L"window.azSubmit=azSubmit;"
    L"function wire(){"
    L"  $('doctor').onchange=fillServices;"
    L"  $('nid').onkeyup=function(e){var k=(e||window.event).keyCode;if($('nid').value.length===10&&k!==13)lookup();};"
    L"  $('btnReserve').onclick=azSubmit; $('btnNew').onclick=clearForm;"
    L"  var rf=$('btnRefresh'); if(rf)rf.onclick=refresh;"
    L"}"
    L"wire(); call('ping');"
    L"</script>";
}

// ---------------------------------------------------------- reception doc ----
static std::wstring wh_receptionHtml(){
    std::wstring h;
    h += L"<!DOCTYPE html><html><head>";
    h += wh_commonHead();
    h += L"</head><body><div class='wrap'>";
    // top bar: breadcrumb (right) + live meta pills (left). No in-page page title
    // bar and no «خلاصه/عملیات» summary panel — the native tab already labels it.
    h += L"<div class='topbar'>"
         L"<div class='brand'><div class='dot'>\u0622</div>"
         L"<div class='crumb'>\u067e\u0630\u06cc\u0631\u0634 \u0628\u06cc\u0645\u0627\u0631"
         L"<i>\u2039</i><span id='s_dept' style='color:var(--textDim);font-weight:600'>\u2014</span></div></div>"
         L"<div class='meta' id='meta'>"
         L"<span class='pill' id='s_today'>\u2014</span>"
         L"<span class='pill' id='s_shift'>\u2014</span>"
         L"<span class='pill' id='s_user'>\u2014</span>"
         L"</div></div>";
    h += L"<div class='grid'>";

    // ============================ RIGHT ZONE =============================
    // patient identity card (photo, status widgets, visit counts, file no),
    // search keys (icon buttons), insurance, doctor.
    h += L"<div class='zone zone-right scroll'>";
    h += L"<div class='card tight pcard'>"
         L"<div class='avatar' id='p_avatar'>?</div>"
         L"<div class='pname' id='p_name'>\u0628\u06cc\u0645\u0627\u0631 \u062c\u062f\u06cc\u062f</div>"
         L"<div class='psub' id='p_sub'>\u0628\u062f\u0648\u0646 \u0633\u0627\u0628\u0642\u0647</div>"
         // status mini-widgets
         L"<div class='pstat'>"
         L"<div class='w'><span class='k'>\u0646\u0633\u062e\u0647 \u0627\u0644\u06a9\u062a\u0631\u0648\u0646\u06cc\u06a9</span><span class='v on' id='w_ev'>\u0641\u0639\u0627\u0644</span></div>"
         L"<div class='w'><span class='k'>\u0648\u0636\u0639\u06cc\u062a \u0628\u06cc\u0645\u0627\u0631</span><span class='v off' id='w_status'>\u062c\u062f\u06cc\u062f</span></div>"
         L"<div class='w'><span class='k'>\u0648\u0636\u0639\u06cc\u062a \u0628\u06cc\u0645\u0647</span><span class='v off' id='w_ins'>\u0646\u0627\u0645\u0634\u062e\u0635</span></div>"
         L"<div class='w'><span class='k'>\u0634\u0645\u0627\u0631\u0647 \u067e\u0631\u0648\u0646\u062f\u0647</span><span class='v' id='w_file'>\u2014</span></div>"
         L"</div>"
         L"<div class='pcounts'>"
         L"<div class='c'><div class='k'>\u062a\u0639\u062f\u0627\u062f \u0648\u06cc\u0632\u06cc\u062a</div><div class='v' id='p_visits'>0</div></div>"
         L"<div class='c'><div class='k'>\u0642\u0628\u0636</div><div class='v' id='p_p'>0</div></div>"
         L"<div class='c'><div class='k'>\u0633\u0631\u067e\u0627\u06cc\u06cc</div><div class='v' id='p_s'>0</div></div>"
         L"</div>"
         L"</div>";
    h += L"<div class='card tight'>"
         L"<div class='sect'><span class='acc'></span><span class='ic'>"+wh_svg(L"search")+L"</span>\u062c\u0633\u062a\u062c\u0648 \u0628\u06cc\u0645\u0627\u0631</div>"
         L"<div class='row'><div class='fld wide'><label>\u0634\u0645\u0627\u0631\u0647 \u067e\u0631\u0648\u0646\u062f\u0647</label>"
         L"<div class='ig'><input id='file' class='ltr' inputmode='numeric'>"
         L"<button class='btn ic-btn' id='q_file' title='\u0627\u0633\u062a\u0639\u0644\u0627\u0645'>"+wh_svg(L"search")+L"</button></div></div></div>"
         L"<div class='row'><div class='fld wide'><label>\u0634\u0645\u0627\u0631\u0647 \u0628\u06cc\u0645\u0647</label>"
         L"<div class='ig'><input id='archive' class='ltr' inputmode='numeric'>"
         L"<button class='btn ic-btn' id='q_archive' title='\u0627\u0633\u062a\u0639\u0644\u0627\u0645'>"+wh_svg(L"search")+L"</button></div></div></div>"
         L"<div class='row'><div class='fld wide'><label>\u062c\u0633\u062a\u062c\u0648 \u0628\u0627 \u06a9\u062f \u0645\u0644\u06cc</label>"
         L"<div class='ig'><input id='nidSearch' class='ltr' maxlength='10' inputmode='numeric'>"
         L"<button class='btn ic-btn' id='q_nid' title='\u0627\u0633\u062a\u0639\u0644\u0627\u0645'>"+wh_svg(L"search")+L"</button></div></div></div>"
         L"</div>";
    h += L"<div class='card tight'>"
         L"<div class='sect'><span class='acc'></span><span class='ic'>"+wh_svg(L"shield")+L"</span>\u0628\u06cc\u0645\u0647</div>"
         L"<div class='row'><div class='fld wide'><label>\u0628\u06cc\u0645\u0647 \u0627\u0635\u0644\u06cc</label><select id='ins' data-nav></select></div></div>"
         L"<div class='row'><div class='fld wide'><label>\u0628\u06cc\u0645\u0647 \u0645\u06a9\u0645\u0644</label><select id='supp' data-nav></select></div></div>"
         L"<div class='row'><div class='fld f-half'><label>\u0634 \u062f\u0641\u062a\u0631\u0686\u0647</label><input id='book' class='ltr' inputmode='numeric'></div>"
         L"<div class='fld f-half'><label>\u0634\u0639\u0628\u0647 \u0628\u06cc\u0645\u0647</label><input id='insBranch'></div></div>"
         L"<div class='row'><div class='fld f-half'><label>\u062a\u0627\u0631\u06cc\u062e \u0627\u0639\u062a\u0628\u0627\u0631</label><input id='insExp' class='ltr center' placeholder='____/__/__'></div>"
         L"<div class='fld f-half'><label>\u062f\u0631\u0635\u062f \u067e\u0648\u0634\u0634</label><input id='insCov' class='ltr center' inputmode='numeric' readonly></div></div>"
         L"</div>";
    h += L"<div class='card tight'>"
         L"<div class='sect'><span class='acc'></span><span class='ic'>"+wh_svg(L"stethoscope")+L"</span>\u067e\u0632\u0634\u06a9 \u0645\u0639\u0627\u0644\u062c</div>"
         L"<div class='row'>"
         L"<div class='fld xs'><label>\u06a9\u062f \u0646\u0638\u0627\u0645</label><input id='docCode' class='ltr' inputmode='numeric'></div>"
         L"<div class='fld'><label>\u0646\u0627\u0645 \u067e\u0632\u0634\u06a9</label><input id='docName'></div>"
         L"</div>"
         L"<div class='row'><div class='fld wide'><label>\u067e\u0632\u0634\u06a9 \u0627\u0631\u062c\u0627\u0639\u200c\u062f\u0647\u0646\u062f\u0647</label><input id='docRef'></div></div>"
         L"</div>";
    h += L"</div>";   // zone-right

    // ============================ CENTER ZONE ============================
    // patient identity + insurance/visit form + service info + primary actions.
    h += L"<div class='zone zone-center scroll'>";
    h += L"<div class='card fill'>";
    h += L"<div class='sect'><span class='acc'></span><span class='ic'>"+wh_svg(L"user")+L"</span>\u0645\u0634\u062e\u0635\u0627\u062a \u0628\u06cc\u0645\u0627\u0631</div>";
    // Row 1: نام، نام خانوادگی، کد ملی (با استعلام)
    h += L"<div class='row'>"
         L"<div class='fld f-name'><label>\u0646\u0627\u0645</label><input id='first' data-nav></div>"
         L"<div class='fld f-family'><label>\u0646\u0627\u0645 \u062e\u0627\u0646\u0648\u0627\u062f\u06af\u06cc</label><input id='last' data-nav></div>"
         L"<div class='fld f-nid'><label>\u06a9\u062f \u0645\u0644\u06cc</label>"
         L"<div class='ig'><input id='nid' class='ltr' data-nav maxlength='10' inputmode='numeric'>"
         L"<button class='btn ic-btn' id='btnSearch' title='\u0627\u0633\u062a\u0639\u0644\u0627\u0645'>"+wh_svg(L"search")+L"</button></div></div>"
         L"</div>";
    // Row 2: نام پدر، تاریخ تولد، جنسیت
    h += L"<div class='row'>"
         L"<div class='fld f-father'><label>\u0646\u0627\u0645 \u067e\u062f\u0631</label><input id='father' data-nav></div>"
         L"<div class='fld f-birth'><label>\u062a\u0627\u0631\u06cc\u062e \u062a\u0648\u0644\u062f</label><input id='birth' class='ltr center' data-nav maxlength='10' inputmode='numeric' placeholder='____/__/__'></div>"
         L"<div class='fld f-gender'><label>\u062c\u0646\u0633\u06cc\u062a</label><select id='gender' data-nav><option>\u0645\u0631\u062f</option><option>\u0632\u0646</option></select></div>"
         L"</div>";
    // Row 3: موبایل، تلفن ثابت، آدرس (آدرس عریض)
    h += L"<div class='row'>"
         L"<div class='fld f-mobile'><label>\u0645\u0648\u0628\u0627\u06cc\u0644</label><input id='mobile' class='ltr' data-nav maxlength='11' inputmode='numeric'></div>"
         L"<div class='fld f-phone'><label>\u062a\u0644\u0641\u0646 \u062b\u0627\u0628\u062a</label><input id='phone' class='ltr' data-nav inputmode='numeric'></div>"
         L"<div class='fld f-addr'><label>\u0622\u062f\u0631\u0633</label><input id='address' data-nav></div>"
         L"</div>";
    // ----- medical / visit info -----
    h += L"<div class='sect mt'><span class='acc'></span><span class='ic'>"+wh_svg(L"clipboard")+L"</span>\u0627\u0637\u0644\u0627\u0639\u0627\u062a \u062f\u0631\u0645\u0627\u0646\u06cc \u0648 \u0646\u0648\u0628\u062a</div>";
    h += L"<div class='row'>"
         L"<div class='fld f-sel'><label>\u0646\u0648\u0639 \u0628\u06cc\u0645\u0627\u0631</label><select id='ptype' data-nav><option>\u0639\u0627\u062f\u06cc</option><option>\u0633\u0631\u067e\u0627\u06cc\u06cc</option><option>\u0628\u0633\u062a\u0631\u06cc</option></select></div>"
         L"<div class='fld f-sel'><label>\u0646\u0648\u0639 \u0646\u0648\u0628\u062a</label><select id='ntype' data-nav><option>\u0639\u0627\u062f\u06cc</option><option>\u0627\u0648\u0631\u0698\u0627\u0646\u0633</option><option>\u067e\u0631\u0633\u0646\u0644\u06cc</option></select></div>"
         L"</div>";
    // ----- service info (amount + discount) -----
    h += L"<div class='sect mt'><span class='acc'></span><span class='ic'>"+wh_svg(L"layers")+L"</span>\u0627\u0637\u0644\u0627\u0639\u0627\u062a \u062e\u062f\u0645\u062a</div>";
    h += L"<div class='row'>"
         L"<div class='fld f-money'><label>\u0645\u0628\u0644\u063a \u062e\u062f\u0645\u062a (\u0631\u06cc\u0627\u0644)</label><input id='price' class='ltr' data-nav inputmode='numeric'></div>"
         L"<div class='fld f-money'><label>\u062a\u062e\u0641\u06cc\u0641 (\u0631\u06cc\u0627\u0644)</label><input id='discount' class='ltr' data-nav inputmode='numeric'></div>"
         L"</div>";
    // ----- bottom action bar: register&print / new / clear / cancel -----
    h += L"<div class='footbar'>"
         L"<button class='btn primary lg' style='flex:2 1 230px' id='btnSave' data-nav data-enter='submit'>"+wh_svg(L"print")+L"\u062b\u0628\u062a \u067e\u0630\u06cc\u0631\u0634 \u0648 \u0686\u0627\u067e \u0631\u0633\u06cc\u062f</button>"
         L"<button class='btn outline lg' style='flex:1 1 140px' id='btnNew' data-nav>"+wh_svg(L"plus")+L"\u067e\u0630\u06cc\u0631\u0634 \u062c\u062f\u06cc\u062f</button>"
         L"<button class='btn lg' style='flex:1 1 120px' id='btnClear' data-nav>"+wh_svg(L"eraser")+L"\u067e\u0627\u06a9 \u06a9\u0631\u062f\u0646</button>"
         L"<button class='btn danger lg' style='flex:0 1 110px' id='btnCancel' data-nav>"+wh_svg(L"x")+L"\u0627\u0646\u0635\u0631\u0627\u0641</button>"
         L"</div>";
    h += L"</div>";   // card fill
    h += L"</div>";   // zone-center

    // ============================ LEFT ZONE ==============================
    // payment summary + payable + print.
    h += L"<div class='zone zone-left scroll'>";
    h += L"<div class='card tight bill'>"
         L"<div class='sect'><span class='acc'></span><span class='ic'>"+wh_svg(L"receipt")+L"</span>\u062e\u0644\u0627\u0635\u0647 \u067e\u0631\u062f\u0627\u062e\u062a</div>"
         L"<div class='grp'>\u0628\u06cc\u0645\u0647 \u0627\u0635\u0644\u06cc</div>"
         L"<div class='line'><span>\u062c\u0645\u0639 \u06a9\u0644</span><b id='b_total'>0<span class='u'>\u0631\u06cc\u0627\u0644</span></b></div>"
         L"<div class='line'><span>\u0633\u0647\u0645 \u0628\u06cc\u0645\u0647</span><b id='b_main'>0<span class='u'>\u0631\u06cc\u0627\u0644</span></b></div>"
         L"<div class='grp'>\u0628\u06cc\u0645\u0647 \u0645\u06a9\u0645\u0644</div>"
         L"<div class='line'><span>\u0645\u0627\u0628\u0647\u200c\u0627\u0644\u062a\u0641\u0627\u0648\u062a \u067e\u0627\u06cc\u0647</span><b id='b_diff'>0<span class='u'>\u0631\u06cc\u0627\u0644</span></b></div>"
         L"<div class='line'><span>\u0633\u0647\u0645 \u0633\u0627\u0632\u0645\u0627\u0646</span><b id='b_org'>0<span class='u'>\u0631\u06cc\u0627\u0644</span></b></div>"
         L"<div class='grp'>\u0633\u0647\u0645 \u0628\u06cc\u0645\u0627\u0631</div>"
         L"<div class='line'><span>\u0633\u0647\u0645 \u0628\u06cc\u0645\u0627\u0631</span><b id='b_pat'>0<span class='u'>\u0631\u06cc\u0627\u0644</span></b></div>"
         L"<div class='line'><span>\u062a\u062e\u0641\u06cc\u0641</span><b id='b_disc'>0<span class='u'>\u0631\u06cc\u0627\u0644</span></b></div>"
         L"<div class='pay'><span>\u0645\u0628\u0644\u063a \u0646\u0647\u0627\u06cc\u06cc</span><b><span id='b_paid'>0</span> <span style='font-size:11px;opacity:.85'>\u0631\u06cc\u0627\u0644</span></b></div>"
         L"</div>";
    h += L"<div class='card tight'>"
         L"<div class='sect'><span class='acc'></span><span class='ic'>"+wh_svg(L"print")+L"</span>\u0686\u0627\u067e</div>"
         L"<div class='btnrow stack'>"
         L"<button class='btn block sm' id='p_ins'>"+wh_svg(L"shield")+L"\u0631\u0633\u06cc\u062f \u0628\u06cc\u0645\u0647</button>"
         L"<button class='btn block sm' id='p_rx'>"+wh_svg(L"clipboard")+L"\u0686\u0627\u067e \u0646\u0633\u062e\u0647</button>"
         L"<button class='btn block sm' id='p_last'>"+wh_svg(L"receipt")+L"\u0686\u0627\u067e \u0622\u062e\u0631\u06cc\u0646 \u0642\u0628\u0636 (F8)</button>"
         L"<button class='btn block sm' id='p_reprint'>"+wh_svg(L"print")+L"\u0686\u0627\u067e \u0645\u062c\u062f\u062f</button>"
         L"</div></div>";
    h += L"</div>";   // zone-left

    h += L"</div>";   // grid
    h += L"<div class='toast' id='toast'></div>";
    h += L"</div>";   // wrap
    h += wh_commonScript();
    h += wh_receptionScript();
    h += L"</body></html>";
    return h;
}

// Appointment HTML doc — left form, right live grid.
static std::wstring wh_appointmentHtml(){
    std::wstring h;
    h += L"<!DOCTYPE html><html><head>";
    h += wh_commonHead();
    h += L"</head><body><div class='wrap'>";
    h += L"<div class='topbar'>"
         L"<div class='brand'><div class='dot'>\u0622</div>"
         L"<div class='crumb'>\u0646\u0648\u0628\u062a\u200c\u062f\u0647\u06cc</div></div>"
         L"<div class='meta' id='meta'></div></div>";
    h += L"<div class='grid'>";

    // ---- LEFT/CENTER: appointment + patient form ----
    h += L"<div class='zone zone-right scroll' style='flex:0 0 380px'>";
    h += L"<div class='card'>"
         L"<div class='sect'><span class='ic'>\u26ce</span>\u0645\u0634\u062e\u0635\u0627\u062a \u0646\u0648\u0628\u062a</div>"
         L"<div class='row'><div class='fld wide'><label>\u067e\u0632\u0634\u06a9</label><select id='doctor' data-nav></select></div></div>"
         L"<div class='row'><div class='fld wide'><label>\u062e\u062f\u0645\u062a</label><select id='service' data-nav></select></div></div>"
         L"<div class='row'>"
         L"<div class='fld sm'><label>\u062a\u0627\u0631\u06cc\u062e \u0646\u0648\u0628\u062a</label><input id='date' data-nav placeholder='____/__/__'></div>"
         L"<div class='fld xs'><label>\u0633\u0627\u0639\u062a</label><input id='time' data-nav placeholder='__:__'></div>"
         L"<div class='fld xs'><label>\u0646\u0648\u0639</label><select id='kind' data-nav><option>\u062d\u0636\u0648\u0631\u06cc</option><option>\u063a\u06cc\u0631\u062d\u0636\u0648\u0631\u06cc</option></select></div>"
         L"</div>"
         L"</div>";
    h += L"<div class='card'>"
         L"<div class='sect'><span class='ic'>\u263b</span>\u0645\u0634\u062e\u0635\u0627\u062a \u0628\u06cc\u0645\u0627\u0631</div>"
         L"<div class='row'>"
         L"<div class='fld sm'><label>\u06a9\u062f \u0645\u0644\u06cc</label><input id='nid' data-nav maxlength='10' inputmode='numeric'></div>"
         L"<div class='fld'><label>\u0646\u0627\u0645</label><input id='first' data-nav></div>"
         L"</div>"
         L"<div class='row'>"
         L"<div class='fld'><label>\u0646\u0627\u0645 \u062e\u0627\u0646\u0648\u0627\u062f\u06af\u06cc</label><input id='last' data-nav></div>"
         L"<div class='fld sm'><label>\u0645\u0648\u0628\u0627\u06cc\u0644</label><input id='mobile' data-nav inputmode='numeric'></div>"
         L"</div>"
         L"<div class='actions'>"
         L"<button class='btn primary' id='btnReserve' data-nav data-enter='submit'>\u062b\u0628\u062a \u0646\u0648\u0628\u062a</button>"
         L"<button class='btn' id='btnNew' data-nav>\u0646\u0648\u0628\u062a \u062c\u062f\u06cc\u062f</button>"
         L"</div>"
         L"</div>";
    h += L"</div>";   // left zone

    // ---- CENTER: today's appointments grid ----
    h += L"<div class='zone zone-center'>";
    h += L"<div class='card fill'>";
    h += L"<div class='sect'><span class='ic'>\u2637</span>\u0646\u0648\u0628\u062a\u200c\u0647\u0627\u06cc \u0627\u0645\u0631\u0648\u0632 "
         L"<span class='chip' id='a_count' style='margin-right:auto'>0</span>"
         L"<button class='btn ghost sm' id='btnRefresh' style='margin-right:6px'>\u0628\u0627\u0632\u0622\u0648\u0631\u06cc</button></div>";
    h += L"<div class='tablewrap'><table id='grid'><thead><tr>"
         L"<th>\u0627\u0628\u0637\u0627\u0644</th><th>\u0634 \u0646\u0648\u0628\u062a</th><th>\u0633\u0627\u0639\u062a</th>"
         L"<th>\u062a\u0627\u0631\u06cc\u062e</th><th>\u0646\u0627\u0645</th><th>\u0646\u0627\u0645 \u062e\u0627\u0646\u0648\u0627\u062f\u06af\u06cc</th>"
         L"<th>\u067e\u0632\u0634\u06a9</th><th>\u062e\u062f\u0645\u062a</th><th>\u0646\u0648\u0639</th>"
         L"</tr></thead><tbody id='gbody'></tbody></table></div>";
    h += L"</div>";   // card fill
    h += L"</div>";   // zone-center

    h += L"</div>";   // grid
    h += L"<div class='toast' id='toast'></div>";
    h += L"</div>";   // wrap
    h += wh_commonScript();
    h += wh_appointmentScript();
    h += L"</body></html>";
    return h;
}

std::wstring WebHostBridge_Html(int kind){
    return (kind==WH_APPOINTMENT)? wh_appointmentHtml() : wh_receptionHtml();
}
