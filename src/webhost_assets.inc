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
    L":root{--bg:#eef2f9;--surface:#ffffff;--surfaceTop:#ffffff;--surface2:#f1f5fb;"
    L"--border:#dde3ee;--text:#15202e;--textDim:#697587;--accent:#2563eb;"
    L"--accent2:#1d4ed8;--accentText:#ffffff;--danger:#e23b3b;--success:#15a34a;"
    L"--warn:#d97706;--inputBg:#ffffff;--inputText:#15202e;"
    L"--r:16px;--r2:11px;--r3:8px;--sp:14px;--sp2:10px;--sp3:7px;"
    L"--shadow:0 1px 2px rgba(16,24,40,.06),0 10px 26px rgba(16,24,40,.07);"
    L"--shadowSm:0 1px 2px rgba(16,24,40,.07);"
    L"--ring:0 0 0 3px rgba(37,99,235,.20);}"
    // ============================ RESET / BASE ===========================
    L"*{box-sizing:border-box;-ms-user-select:none;}"
    L"html,body{height:100%;margin:0;}"
    L"body{font-family:Vazirmatn,Tahoma,'Segoe UI',sans-serif;direction:rtl;"
    L"background:"
      L"radial-gradient(1100px 520px at 88% -8%,rgba(37,99,235,.10),transparent 60%),"
      L"radial-gradient(900px 480px at 6% 108%,rgba(21,163,74,.07),transparent 60%),"
      L"var(--bg);"
    L"color:var(--text);font-size:13px;overflow:hidden;line-height:1.5;"
    L"-ms-overflow-style:none;}"
    L"::-webkit-scrollbar{width:9px;height:9px;}"
    L"::-webkit-scrollbar-thumb{background:var(--border);border-radius:6px;}"
    L"::-webkit-scrollbar-thumb:hover{background:#c3ccda;}"
    // ============================== SHELL ================================
    L".wrap{height:100%;display:flex;flex-direction:column;padding:10px 12px;}"
    L".topbar{display:flex;align-items:center;justify-content:space-between;"
    L"gap:12px;padding:0 2px 8px;flex:0 0 auto;}"
    L".brand{display:flex;align-items:center;gap:9px;min-width:0;}"
    L".brand .dot{width:30px;height:30px;border-radius:10px;flex:0 0 auto;"
    L"background:linear-gradient(135deg,var(--accent),var(--accent2));"
    L"display:flex;align-items:center;justify-content:center;color:#fff;"
    L"font-weight:800;font-size:15px;box-shadow:var(--shadow);}"
    L".brand .crumb{font-weight:800;font-size:14px;white-space:nowrap;}"
    L".brand .crumb i{color:var(--textDim);font-weight:600;font-style:normal;"
    L"font-size:12px;margin:0 6px;}"
    L".meta{color:var(--textDim);font-size:11.5px;white-space:nowrap;overflow:hidden;"
    L"text-overflow:ellipsis;display:flex;align-items:center;gap:8px;}"
    L".meta .pill{background:var(--surface);border:1px solid var(--border);"
    L"border-radius:999px;padding:4px 11px;font-weight:600;color:var(--text);"
    L"box-shadow:var(--shadowSm);}"
    // ========================== THREE-ZONE GRID ==========================
    L".grid{flex:1 1 auto;display:flex;gap:11px;min-height:0;align-items:stretch;}"
    L".zone{display:flex;flex-direction:column;gap:11px;min-height:0;}"
    L".zone-left{flex:0 0 256px;}"
    L".zone-center{flex:1 1 auto;min-width:0;}"
    L".zone-right{flex:0 0 296px;}"
    L".zone.scroll{overflow:auto;padding-bottom:2px;}"
    // ============================= CARDS =================================
    L".card{background:var(--surface);border:1px solid var(--border);"
    L"border-radius:var(--r);padding:13px 14px;box-shadow:var(--shadow);"
    L"position:relative;}"
    L".card.tight{padding:11px 12px;}"
    L".card.flex,.card.fill{flex:1 1 auto;min-height:0;display:flex;flex-direction:column;}"
    L".sect{font-weight:800;color:var(--text);margin:0 0 11px;font-size:12.5px;"
    L"display:flex;align-items:center;gap:8px;}"
    L".sect .ic{width:24px;height:24px;flex:0 0 auto;border-radius:8px;"
    L"background:linear-gradient(135deg,rgba(37,99,235,.14),rgba(37,99,235,.05));"
    L"color:var(--accent);display:flex;align-items:center;justify-content:center;"
    L"font-size:13px;}"
    L".sect .acc{width:4px;height:15px;border-radius:3px;flex:0 0 auto;"
    L"background:linear-gradient(180deg,var(--accent),var(--accent2));}"
    L".sect.mt{margin-top:13px;padding-top:12px;border-top:1px solid var(--border);}"
    // ============================ FORM LAYOUT ============================
    L".row{display:flex;flex-wrap:wrap;gap:9px;margin-bottom:9px;}"
    L".row:last-child{margin-bottom:0;}"
    L".fld{flex:1 1 150px;min-width:96px;display:flex;flex-direction:column;}"
    L".fld.sm{flex:0 0 130px;}"
    L".fld.xs{flex:0 0 90px;}"
    L".fld.wide{flex:1 1 100%;}"
    L".fld label{font-size:11px;color:var(--textDim);margin-bottom:4px;"
    L"font-weight:700;padding-right:2px;}"
    // ============================== CONTROLS =============================
    L"input,select,textarea{width:100%;height:35px;border:1px solid var(--border);"
    L"border-radius:var(--r2);background:var(--inputBg);color:var(--inputText);"
    L"padding:0 11px;font-family:inherit;font-size:13px;outline:none;"
    L"transition:border-color .12s,box-shadow .12s,background .12s;}"
    L"select{cursor:pointer;padding-left:26px;"
    // custom caret for IE/Edge — pushed to the visual-left (LTR left) in RTL
    L"background-image:url(\"data:image/svg+xml;charset=utf8,%3Csvg xmlns='http://www.w3.org/2000/svg' width='10' height='6'%3E%3Cpath d='M0 0l5 6 5-6z' fill='%23889'/%3E%3C/svg%3E\");"
    L"background-repeat:no-repeat;background-position:10px center;}"
    L"select::-ms-expand{display:none;}"
    L"input:hover,select:hover,textarea:hover{border-color:#bcc6d6;}"
    L"input:focus,select:focus,textarea:focus{border-color:var(--accent);"
    L"box-shadow:var(--ring);background:var(--surface);}"
    L"input[readonly]{background:var(--surface2);color:var(--textDim);cursor:default;}"
    L"input.ltr{direction:ltr;text-align:right;letter-spacing:.3px;}"
    L"input::-ms-clear{display:none;}input::placeholder{color:#aab3c2;}"
    // a field with an inline action button (e.g. کد ملی + استعلام)
    L".ig{display:flex;gap:6px;}"
    L".ig input{flex:1 1 auto;}"
    L".ig .btn{flex:0 0 auto;height:35px;padding:0 12px;}"
    // ============================== BUTTONS ==============================
    L".btn{height:37px;border:1px solid var(--border);border-radius:var(--r2);"
    L"background:var(--surface);color:var(--text);font-family:inherit;"
    L"font-size:13px;font-weight:700;cursor:pointer;padding:0 16px;"
    L"display:inline-flex;align-items:center;justify-content:center;gap:7px;"
    L"transition:all .12s;white-space:nowrap;box-shadow:var(--shadowSm);}"
    L".btn:hover{background:var(--surface2);border-color:#bcc6d6;}"
    L".btn:active{transform:translateY(1px);}"
    L".btn:focus{box-shadow:var(--ring);}"
    L".btn.primary{background:linear-gradient(135deg,var(--accent),var(--accent2));"
    L"border-color:var(--accent2);color:var(--accentText);box-shadow:var(--shadow);}"
    L".btn.primary:hover{filter:brightness(1.05);}"
    L".btn.success{background:linear-gradient(135deg,var(--success),#0f9d58);"
    L"border-color:#0f9d58;color:#fff;box-shadow:var(--shadow);}"
    L".btn.success:hover{filter:brightness(1.05);}"
    L".btn.danger{background:var(--danger);border-color:var(--danger);color:#fff;}"
    L".btn.ghost{background:transparent;border-color:transparent;color:var(--textDim);"
    L"box-shadow:none;}"
    L".btn.ghost:hover{background:var(--surface2);color:var(--text);}"
    L".btn.block{width:100%;}"
    L".btn.sm{height:31px;font-size:12px;padding:0 11px;}"
    L".btn.lg{height:44px;font-size:14px;font-weight:800;}"
    L".btn[disabled]{opacity:.5;cursor:not-allowed;}"
    L".btnrow{display:flex;gap:7px;flex-wrap:wrap;}"
    L".btnrow.stack{flex-direction:column;}"
    L".actions{display:flex;gap:9px;flex-wrap:wrap;margin-top:auto;"
    L"padding-top:12px;}"
    // ===================== AVATAR / PATIENT CARD (right) =================
    L".pcard{text-align:center;background:"
    L"linear-gradient(180deg,rgba(37,99,235,.06),transparent 70%),var(--surface);}"
    L".avatar{width:66px;height:66px;border-radius:50%;margin:0 auto 7px;"
    L"background:linear-gradient(135deg,var(--accent),var(--accent2));color:#fff;"
    L"display:flex;align-items:center;justify-content:center;font-size:27px;"
    L"font-weight:800;box-shadow:0 6px 16px rgba(37,99,235,.30);"
    L"border:3px solid var(--surface);}"
    L".pname{font-weight:800;font-size:14px;}"
    L".psub{color:var(--textDim);font-size:11px;margin-top:2px;}"
    L".chip{display:inline-flex;align-items:center;gap:5px;padding:4px 11px;"
    L"border-radius:999px;background:var(--surface2);color:var(--textDim);"
    L"font-size:10.5px;font-weight:700;margin:7px 3px 0;border:1px solid var(--border);}"
    L".chip.ok{background:rgba(21,163,74,.12);color:var(--success);"
    L"border-color:rgba(21,163,74,.25);}"
    L".pcounts{display:flex;gap:7px;margin-top:9px;}"
    L".pcounts .c{flex:1 1 0;background:var(--surface2);border:1px solid var(--border);"
    L"border-radius:var(--r2);padding:6px 4px;}"
    L".pcounts .c .k{font-size:10px;color:var(--textDim);font-weight:700;}"
    L".pcounts .c .v{font-size:15px;font-weight:800;margin-top:1px;}"
    // ============================= BILLING (left) ========================
    L".bill .line{display:flex;justify-content:space-between;align-items:center;"
    L"padding:6px 2px;border-bottom:1px dashed var(--border);font-size:12px;}"
    L".bill .line:last-of-type{border-bottom:none;}"
    L".bill .line span{color:var(--textDim);font-weight:600;}"
    L".bill .line b{font-variant-numeric:tabular-nums;font-weight:700;}"
    L".bill .grp{font-size:10.5px;font-weight:800;color:var(--accent);"
    L"margin:9px 0 2px;letter-spacing:.2px;}"
    L".bill .grp:first-child{margin-top:0;}"
    L".bill .pay{margin-top:11px;"
    L"background:linear-gradient(135deg,var(--success),#0f9d58);"
    L"color:#fff;border-radius:var(--r2);padding:11px 13px;display:flex;"
    L"justify-content:space-between;align-items:center;font-weight:800;font-size:16px;"
    L"box-shadow:0 6px 16px rgba(21,163,74,.28);}"
    L".bill .pay b{font-variant-numeric:tabular-nums;}"
    // =========================== APPOINTMENT TABLE =======================
    L".tablewrap{flex:1 1 auto;overflow:auto;border:1px solid var(--border);"
    L"border-radius:var(--r2);min-height:0;background:var(--surface);}"
    L"table{width:100%;border-collapse:collapse;font-size:12px;}"
    L"th,td{border-bottom:1px solid var(--border);padding:8px;text-align:center;"
    L"white-space:nowrap;}"
    L"th{background:var(--surface2);color:var(--textDim);position:sticky;top:0;"
    L"font-weight:800;z-index:1;font-size:11px;}"
    L"tbody tr:hover{background:var(--surface2);}"
    L"tr.cancel td{text-decoration:line-through;color:var(--textDim);}"
    L".empty{padding:34px 14px;text-align:center;color:var(--textDim);font-size:13px;}"
    // ============================== TOAST ================================
    L".toast{position:fixed;left:50%;bottom:22px;transform:translateX(-50%) translateY(8px);"
    L"background:var(--text);color:#fff;padding:11px 22px;border-radius:12px;"
    L"opacity:0;transition:opacity .22s,transform .22s;font-size:13px;z-index:80;"
    L"box-shadow:0 12px 30px rgba(0,0,0,.28);pointer-events:none;font-weight:700;}"
    L".toast.show{opacity:.99;transform:translateX(-50%) translateY(0);}"
    L".toast.bad{background:var(--danger);}"
    L".toast.ok{background:linear-gradient(135deg,var(--success),#0f9d58);}"
    // ===================== COMPACT MODE (small screens) ==================
    L"body.compact{font-size:12px;}"
    L"body.compact .card{padding:10px 11px;}"
    L"body.compact input,body.compact select{height:32px;}"
    L"body.compact .btn{height:34px;}"
    L"body.compact .row{margin-bottom:7px;}"
    L"body.compact .avatar{width:56px;height:56px;font-size:23px;}"
    // ========================= RESPONSIVE BREAKPOINTS ====================
    L"@media (max-width:1180px){.zone-left{flex-basis:226px;}.zone-right{flex-basis:266px;}}"
    L"@media (max-width:980px){.grid{flex-direction:column;overflow:auto;}"
    L".zone-left,.zone-right{flex-basis:auto;}.zone-center{min-width:0;}}"
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
    L"var AZ_TOK={bg:'#eef2f9',surface:'#ffffff',surfaceTop:'#ffffff',surface2:'#f1f5fb',"
    L"  border:'#dde3ee',text:'#15202e',textDim:'#697587',accent:'#2563eb',accent2:'#1d4ed8',"
    L"  accentText:'#ffffff',danger:'#e23b3b',success:'#15a34a',warn:'#d97706',"
    L"  inputBg:'#ffffff',inputText:'#15202e',"
    L"  r:'16px',r2:'11px',r3:'8px',sp:'14px',sp2:'10px',sp3:'7px',"
    L"  shadow:'0 1px 2px rgba(16,24,40,.06),0 10px 26px rgba(16,24,40,.07)',"
    L"  shadowSm:'0 1px 2px rgba(16,24,40,.07)',"
    L"  ring:'0 0 0 3px rgba(37,99,235,.20)'};"
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
    L"function num(s){return parseInt(((''+s).replace(/[^0-9]/g,''))||'0',10);}"
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
    L"function azBoot(d){"
    L"  setHeader(d);"
    L"  fillSel($('ins'),d.insurances,true); fillSel($('supp'),d.supp,true);"
    L"  recalc(); var n=$('nid'); if(n)n.focus();"
    L"}"
    // helper: numeric value of a control, tolerant of missing nodes
    L"function v(id){var e=$(id);return e?e.value:'';}"
    L"function recalc(){"
    L"  var r=call('bill',{price:num(v('price')),discount:num(v('discount')),"
    L"    ptype:$('ptype').selectedIndex,ntype:$('ntype').selectedIndex,"
    L"    ins:parseInt(v('ins')||'0',10),supp:parseInt(v('supp')||'0',10)});"
    L"  if(!r||typeof r.price==='undefined')return;"
    L"  $('b_total').innerHTML=fa(fmt(r.price)); $('b_main').innerHTML=fa(fmt(r.mainShare));"
    L"  $('b_diff').innerHTML=fa(fmt(r.baseDiff)); $('b_org').innerHTML=fa(fmt(r.orgShare));"
    L"  $('b_disc').innerHTML=fa(fmt(num(v('discount')))); $('b_paid').innerHTML=fa(fmt(r.paid));"
    L"  if(num(v('price'))===0){ $('price').value=fmt(r.price); }"
    L"}"
    L"function setPatient(r,found){var a=$('p_avatar'),nm=$('p_name'),sb=$('p_sub');"
    L"  if(found){var fn=((r.first||'')+' '+(r.last||'')).replace(/^ +| +$/g,'');"
    L"    if(a)a.innerHTML=esc((r.first||r.last||'?').substr(0,1));"
    L"    if(nm)nm.innerHTML=esc(fn||'\u2014');"
    L"    if(sb)sb.innerHTML=esc((r.gender||'')+(r.birth?(' \u00b7 '+r.birth):''));}"
    L"  else{if(a)a.innerHTML='?';if(nm)nm.innerHTML='\u0628\u06cc\u0645\u0627\u0631 \u062c\u062f\u06cc\u062f';if(sb)sb.innerHTML='\u0628\u062f\u0648\u0646 \u0633\u0627\u0628\u0642\u0647';}}"
    L"function lookup(){var nid=v('nid');if(nid.length<10)return;"
    L"  var r=call('lookup',{nid:nid});"
    L"  if(r.found){ $('first').value=r.first||''; $('last').value=r.last||'';"
    L"    $('father').value=r.father||''; $('birth').value=r.birth||''; $('mobile').value=r.mobile||'';"
    L"    if(r.gender){$('gender').value=r.gender;} setPatient(r,true);"
    L"    toast('\u0628\u06cc\u0645\u0627\u0631 \u06cc\u0627\u0641\u062a \u0634\u062f','ok'); $('ptype').focus(); }"
    L"  else if(!r.idValid){ toast('\u06a9\u062f \u0645\u0644\u06cc \u0646\u0627\u0645\u0639\u062a\u0628\u0631','bad'); }"
    L"  else { setPatient({},false); toast('\u0628\u06cc\u0645\u0627\u0631 \u062c\u062f\u06cc\u062f','');} }"
    L"function clearForm(){var ids=['nid','first','last','father','birth','mobile','phone','address','price','discount','archive','file','book','insExp','docCode','docName'];"
    L"  for(var i=0;i<ids.length;i++){var el=$(ids[i]);if(el)el.value='';}"
    L"  if($('ptype'))$('ptype').selectedIndex=0; if($('ntype'))$('ntype').selectedIndex=0;"
    L"  if($('ins'))$('ins').selectedIndex=0; if($('supp'))$('supp').selectedIndex=0;"
    L"  setPatient({},false); recalc();"
    L"  var n=$('nid'); if(n)n.focus();}"
    L"function azSubmit(){"
    L"  var insSel=$('ins'),suppSel=$('supp');"
    L"  var r=call('saveReception',{first:v('first'),last:v('last'),nid:v('nid'),"
    L"    father:v('father'),birth:v('birth'),gender:v('gender'),mobile:v('mobile'),"
    L"    phone:v('phone'),address:v('address'),"
    L"    ptypeName:v('ptype'),ntypeName:v('ntype'),"
    L"    insName:insSel.options[insSel.selectedIndex]?insSel.options[insSel.selectedIndex].text:'',"
    L"    suppName:suppSel.options[suppSel.selectedIndex]?suppSel.options[suppSel.selectedIndex].text:'',"
    L"    ins:parseInt(insSel.value||'0',10),supp:parseInt(suppSel.value||'0',10),"
    L"    price:num(v('price')),discount:num(v('discount'))});"
    L"  if(r.ok){ toast('\u067e\u0630\u06cc\u0631\u0634 \u062b\u0628\u062a \u0634\u062f \u2014 \u0646\u0648\u0628\u062a '+fa(r.queue),'ok'); clearForm(); }"
    L"  else { toast(r.error||'\u062e\u0637\u0627 \u062f\u0631 \u062b\u0628\u062a','bad'); } }"
    L"window.azSubmit=azSubmit;"
    L"function doPrint(k){var r=call('print',{kind:k}); if(!r||r.ok!==true)toast('\u0686\u0627\u067e \u0627\u0646\u062c\u0627\u0645 \u0646\u0634\u062f','bad');}"
    L"function on(id,ev,fn){var e=$(id);if(e)e[ev]=fn;}"
    L"function wire(){"
    L"  on('nid','onkeyup',function(e){var k=(e||window.event).keyCode; if(v('nid').length===10&&k!==13)lookup();});"
    L"  on('price','onkeyup',recalc); on('discount','onkeyup',recalc);"
    L"  on('ptype','onchange',function(){$('price').value='';recalc();});"
    L"  on('ntype','onchange',function(){$('price').value='';recalc();});"
    L"  on('ins','onchange',recalc); on('supp','onchange',recalc);"
    L"  on('btnSave','onclick',azSubmit); on('btnNew','onclick',clearForm);"
    L"  on('btnReset','onclick',clearForm);"
    L"  on('p_ins','onclick',function(){doPrint(0);});"
    L"  on('p_rx','onclick',function(){doPrint(1);});"
    L"  on('p_last','onclick',function(){var r=call('printLast'); if(!r||r.ok!==true)toast('\u0686\u0627\u067e \u0627\u0646\u062c\u0627\u0645 \u0646\u0634\u062f','bad');});"
    L"  on('btnSearch','onclick',function(){var n=$('nid'); if(n){n.focus();lookup();}});"
    L"  on('q_archive','onclick',function(){toast('\u062c\u0633\u062a\u062c\u0648 \u0628\u0631 \u0627\u0633\u0627\u0633 \u0634\u0645\u0627\u0631\u0647 \u0628\u0627\u06cc\u06af\u0627\u0646\u06cc','');});"
    L"  on('q_file','onclick',function(){toast('\u062c\u0633\u062a\u062c\u0648 \u0628\u0631 \u0627\u0633\u0627\u0633 \u0634\u0645\u0627\u0631\u0647 \u067e\u0631\u0648\u0646\u062f\u0647','');});"
    // Ctrl+R clears the form (matches the legacy shortcut on the action button)
    L"  document.onkeydown=function(e){e=e||window.event;var k=e.keyCode||e.which;"
    L"    if((e.ctrlKey||e.metaKey)&&(k===82)){clearForm();if(e.preventDefault)e.preventDefault();return false;}"
    L"    return onKey(e);};"
    L"}"
    L"wire(); call('ping');"
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
    // patient identity card, search keys (with استعلام), insurance, doctor.
    h += L"<div class='zone zone-right scroll'>";
    h += L"<div class='card tight pcard'>"
         L"<div class='avatar' id='p_avatar'>?</div>"
         L"<div class='pname' id='p_name'>\u0628\u06cc\u0645\u0627\u0631 \u062c\u062f\u06cc\u062f</div>"
         L"<div class='psub' id='p_sub'>\u0628\u062f\u0648\u0646 \u0633\u0627\u0628\u0642\u0647</div>"
         L"<div><span class='chip ok'>\u0646\u0633\u062e\u0647 \u0627\u0644\u06a9\u062a\u0631\u0648\u0646\u06cc\u06a9</span></div>"
         L"<div class='pcounts'>"
         L"<div class='c'><div class='k'>P (\u0642\u0628\u0636)</div><div class='v' id='p_p'>0</div></div>"
         L"<div class='c'><div class='k'>S (\u0633\u0631\u067e\u0627\u06cc\u06cc)</div><div class='v' id='p_s'>0</div></div>"
         L"</div>"
         L"</div>";
    h += L"<div class='card tight'>"
         L"<div class='sect'><span class='acc'></span><span class='ic'>\u26b2</span>\u06a9\u0644\u06cc\u062f\u0647\u0627\u06cc \u062c\u0633\u062a\u062c\u0648</div>"
         L"<div class='row'><div class='fld wide'><label>\u0634\u0645\u0627\u0631\u0647 \u0628\u0627\u06cc\u06af\u0627\u0646\u06cc</label>"
         L"<div class='ig'><input id='archive' class='ltr' inputmode='numeric'>"
         L"<button class='btn sm' id='q_archive'>\u0627\u0633\u062a\u0639\u0644\u0627\u0645</button></div></div></div>"
         L"<div class='row'><div class='fld wide'><label>\u0634\u0645\u0627\u0631\u0647 \u067e\u0631\u0648\u0646\u062f\u0647</label>"
         L"<div class='ig'><input id='file' class='ltr' inputmode='numeric'>"
         L"<button class='btn sm' id='q_file'>\u0627\u0633\u062a\u0639\u0644\u0627\u0645</button></div></div></div>"
         L"</div>";
    h += L"<div class='card tight'>"
         L"<div class='sect'><span class='acc'></span><span class='ic'>\u26ce</span>\u0628\u06cc\u0645\u0647</div>"
         L"<div class='row'><div class='fld wide'><label>\u0628\u06cc\u0645\u0647 \u067e\u0627\u06cc\u0647</label><select id='ins' data-nav></select></div></div>"
         L"<div class='row'><div class='fld'><label>\u0634 \u062f\u0641\u062a\u0631\u0686\u0647</label><input id='book' class='ltr' inputmode='numeric'></div>"
         L"<div class='fld'><label>\u062a\u0627\u0631\u06cc\u062e \u0627\u0639\u062a\u0628\u0627\u0631</label><input id='insExp' class='ltr' placeholder='____/__/__'></div></div>"
         L"<div class='row'><div class='fld wide'><label>\u0628\u06cc\u0645\u0647 \u0645\u06a9\u0645\u0644</label><select id='supp' data-nav></select></div></div>"
         L"</div>";
    h += L"<div class='card tight'>"
         L"<div class='sect'><span class='acc'></span><span class='ic'>\u271a</span>\u067e\u0632\u0634\u06a9 \u0645\u0639\u0627\u0644\u062c</div>"
         L"<div class='row'>"
         L"<div class='fld xs'><label>\u06a9\u062f \u0646\u0638\u0627\u0645</label><input id='docCode' class='ltr' inputmode='numeric'></div>"
         L"<div class='fld'><label>\u0646\u0627\u0645 \u067e\u0632\u0634\u06a9</label><input id='docName'></div>"
         L"</div>"
         L"</div>";
    h += L"</div>";   // zone-right

    // ============================ CENTER ZONE ============================
    // patient identity + insurance/visit form + primary actions.
    h += L"<div class='zone zone-center'>";
    h += L"<div class='card fill'>";
    h += L"<div class='sect'><span class='acc'></span><span class='ic'>\u263b</span>\u0645\u0634\u062e\u0635\u0627\u062a \u0628\u06cc\u0645\u0627\u0631</div>";
    h += L"<div class='row'>"
         L"<div class='fld sm'><label>\u06a9\u062f \u0645\u0644\u06cc</label>"
         L"<div class='ig'><input id='nid' class='ltr' data-nav maxlength='10' inputmode='numeric'>"
         L"<button class='btn sm' id='btnSearch'>\u0627\u0633\u062a\u0639\u0644\u0627\u0645</button></div></div>"
         L"<div class='fld'><label>\u0646\u0627\u0645</label><input id='first' data-nav></div>"
         L"<div class='fld'><label>\u0646\u0627\u0645 \u062e\u0627\u0646\u0648\u0627\u062f\u06af\u06cc</label><input id='last' data-nav></div>"
         L"</div>";
    h += L"<div class='row'>"
         L"<div class='fld'><label>\u0646\u0627\u0645 \u067e\u062f\u0631</label><input id='father' data-nav></div>"
         L"<div class='fld sm'><label>\u062a\u0627\u0631\u06cc\u062e \u062a\u0648\u0644\u062f</label><input id='birth' class='ltr' data-nav placeholder='____/__/__'></div>"
         L"<div class='fld xs'><label>\u062c\u0646\u0633\u06cc\u062a</label><select id='gender' data-nav><option>\u0645\u0631\u062f</option><option>\u0632\u0646</option></select></div>"
         L"</div>";
    h += L"<div class='row'>"
         L"<div class='fld sm'><label>\u0645\u0648\u0628\u0627\u06cc\u0644</label><input id='mobile' class='ltr' data-nav inputmode='numeric'></div>"
         L"<div class='fld sm'><label>\u062a\u0644\u0641\u0646 \u062b\u0627\u0628\u062a</label><input id='phone' class='ltr' data-nav inputmode='numeric'></div>"
         L"<div class='fld'><label>\u0622\u062f\u0631\u0633</label><input id='address' data-nav></div>"
         L"</div>";
    h += L"<div class='sect mt'><span class='acc'></span><span class='ic'>\u26ce</span>\u0628\u06cc\u0645\u0647 \u0648 \u0646\u0648\u0628\u062a</div>";
    h += L"<div class='row'>"
         L"<div class='fld'><label>\u0646\u0648\u0639 \u0628\u06cc\u0645\u0627\u0631</label><select id='ptype' data-nav><option>\u0633\u0631\u067e\u0627\u06cc\u06cc</option><option>\u0628\u0633\u062a\u0631\u06cc</option><option>\u0639\u0627\u062f\u06cc</option></select></div>"
         L"<div class='fld'><label>\u0646\u0648\u0639 \u0646\u0648\u0628\u062a</label><select id='ntype' data-nav><option>\u0639\u0627\u062f\u06cc</option><option>\u0627\u0648\u0631\u0698\u0627\u0646\u0633</option><option>\u067e\u0631\u0633\u0646\u0644\u06cc</option></select></div>"
         L"</div>";
    h += L"<div class='row'>"
         L"<div class='fld'><label>\u0645\u0628\u0644\u063a \u062e\u062f\u0645\u062a (\u0631\u06cc\u0627\u0644)</label><input id='price' class='ltr' data-nav inputmode='numeric'></div>"
         L"<div class='fld'><label>\u062a\u062e\u0641\u06cc\u0641 (\u0631\u06cc\u0627\u0644)</label><input id='discount' class='ltr' data-nav inputmode='numeric'></div>"
         L"</div>";
    h += L"<div class='actions'>"
         L"<button class='btn success lg' style='flex:2 1 220px' id='btnSave' data-nav data-enter='submit'>\u062b\u0628\u062a \u067e\u0630\u06cc\u0631\u0634 \u0648 \u0635\u062f\u0648\u0631 \u0642\u0628\u0636</button>"
         L"<button class='btn lg' style='flex:1 1 130px' id='btnNew' data-nav>\u067e\u0630\u06cc\u0631\u0634 \u062c\u062f\u06cc\u062f</button>"
         L"<button class='btn lg' style='flex:1 1 130px' id='btnReset' data-nav>\u067e\u0627\u06a9 \u06a9\u0631\u062f\u0646 (Ctrl+R)</button>"
         L"</div>";
    h += L"</div>";   // card fill
    h += L"</div>";   // zone-center

    // ============================ LEFT ZONE ==============================
    // billing breakdown + payable + print.
    h += L"<div class='zone zone-left scroll'>";
    h += L"<div class='card tight bill'>"
         L"<div class='sect'><span class='acc'></span><span class='ic'>\u00a4</span>\u0635\u062f\u0648\u0631 \u0642\u0628\u0636</div>"
         L"<div class='grp'>\u0628\u06cc\u0645\u0647 \u0627\u0635\u0644\u06cc</div>"
         L"<div class='line'><span>\u0633\u0647\u0645 \u0628\u06cc\u0645\u0647</span><b id='b_main'>0</b></div>"
         L"<div class='line'><span>\u062c\u0645\u0639 \u06a9\u0644</span><b id='b_total'>0</b></div>"
         L"<div class='grp'>\u0628\u06cc\u0645\u0647 \u0645\u06a9\u0645\u0644</div>"
         L"<div class='line'><span>\u0645\u0627\u0628\u0647\u200c\u0627\u0644\u062a\u0641\u0627\u0648\u062a \u067e\u0627\u06cc\u0647</span><b id='b_diff'>0</b></div>"
         L"<div class='line'><span>\u0633\u0647\u0645 \u0633\u0627\u0632\u0645\u0627\u0646</span><b id='b_org'>0</b></div>"
         L"<div class='grp'>\u0645\u0628\u0644\u063a \u0646\u0647\u0627\u06cc\u06cc</div>"
         L"<div class='line'><span>\u062a\u062e\u0641\u06cc\u0641</span><b id='b_disc'>0</b></div>"
         L"<div class='pay'><span>\u067e\u0631\u062f\u0627\u062e\u062a\u06cc</span><b id='b_paid'>0</b></div>"
         L"</div>";
    h += L"<div class='card tight'>"
         L"<div class='sect'><span class='acc'></span><span class='ic'>\u2399</span>\u0686\u0627\u067e</div>"
         L"<div class='btnrow stack'>"
         L"<button class='btn block sm' id='p_ins'>\u0631\u0633\u06cc\u062f \u0628\u06cc\u0645\u0647</button>"
         L"<button class='btn block sm' id='p_rx'>\u0686\u0627\u067e \u0646\u0633\u062e\u0647</button>"
         L"<button class='btn block sm' id='p_last'>\u0686\u0627\u067e \u0622\u062e\u0631\u06cc\u0646 \u0642\u0628\u0636 (F8)</button>"
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
