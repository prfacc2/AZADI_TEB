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
    // ---- theme tokens (overwritten at runtime by applyTheme) ----
    L":root{--bg:#eef1f7;--surface:#ffffff;--surfaceTop:#ffffff;--surface2:#eef1f6;"
    L"--border:#d7dce5;--text:#1d2330;--textDim:#6b7689;--accent:#2563eb;"
    L"--accent2:#1d4ed8;--accentText:#ffffff;--danger:#dc2626;--success:#16a34a;"
    L"--warn:#d97706;--inputBg:#ffffff;--inputText:#1d2330;"
    L"--r:14px;--r2:10px;--r3:8px;--sp:14px;--sp2:10px;--sp3:7px;"
    L"--shadow:0 1px 3px rgba(16,24,40,.08),0 6px 18px rgba(16,24,40,.06);"
    L"--ring:0 0 0 3px rgba(37,99,235,.18);}"
    // ---- reset / base ----
    L"*{box-sizing:border-box;-ms-user-select:none;}"
    L"html,body{height:100%;margin:0;}"
    L"body{font-family:Vazirmatn,Tahoma,'Segoe UI',sans-serif;direction:rtl;"
    L"background:var(--bg);color:var(--text);font-size:13.5px;overflow:hidden;"
    L"line-height:1.5;}"
    L"::-webkit-scrollbar{width:10px;height:10px;}"
    L"::-webkit-scrollbar-thumb{background:var(--border);border-radius:6px;}"
    // ---- app shell ----
    L".wrap{height:100%;display:flex;flex-direction:column;padding:12px 14px 10px;}"
    // ---- top bar ----
    L".topbar{display:flex;align-items:center;justify-content:space-between;"
    L"gap:12px;padding:0 4px 10px;flex:0 0 auto;}"
    L".brand{display:flex;align-items:center;gap:10px;min-width:0;}"
    L".brand .dot{width:30px;height:30px;border-radius:9px;flex:0 0 auto;"
    L"background:linear-gradient(135deg,var(--accent),var(--accent2));"
    L"display:flex;align-items:center;justify-content:center;color:#fff;"
    L"font-weight:700;font-size:15px;box-shadow:var(--shadow);}"
    L".title{font-weight:800;font-size:18px;white-space:nowrap;}"
    L".meta{color:var(--textDim);font-size:12px;white-space:nowrap;overflow:hidden;"
    L"text-overflow:ellipsis;}"
    L".topActions{display:flex;align-items:center;gap:8px;flex:0 0 auto;}"
    // ---- tab strip ----
    L".tabs{display:flex;gap:6px;padding:0 4px 10px;flex:0 0 auto;}"
    L".tab{height:34px;display:flex;align-items:center;gap:6px;padding:0 16px;"
    L"border:1px solid var(--border);border-radius:999px;background:var(--surface);"
    L"color:var(--textDim);cursor:pointer;font-size:13px;font-family:inherit;"
    L"transition:all .15s;white-space:nowrap;}"
    L".tab:hover{background:var(--surface2);color:var(--text);}"
    L".tab.active{background:var(--accent);border-color:var(--accent);"
    L"color:var(--accentText);font-weight:700;box-shadow:var(--shadow);}"
    // ---- three-zone grid ----
    L".grid{flex:1 1 auto;display:flex;gap:14px;min-height:0;align-items:stretch;}"
    L".zone{display:flex;flex-direction:column;gap:12px;min-height:0;}"
    L".zone-left{flex:0 0 250px;}"
    L".zone-center{flex:1 1 auto;min-width:0;}"
    L".zone-right{flex:0 0 320px;}"
    L".zone.scroll{overflow:auto;padding-bottom:4px;}"
    // ---- cards / surfaces ----
    L".card{background:var(--surface);border:1px solid var(--border);"
    L"border-radius:var(--r);padding:14px;box-shadow:var(--shadow);}"
    L".card.tight{padding:12px;}"
    L".card.flex{flex:1 1 auto;min-height:0;display:flex;flex-direction:column;}"
    L".card.fill{flex:1 1 auto;display:flex;flex-direction:column;min-height:0;}"
    L".sect{font-weight:800;color:var(--text);margin:0 0 10px;font-size:13px;"
    L"display:flex;align-items:center;gap:7px;padding-bottom:8px;"
    L"border-bottom:1px solid var(--border);}"
    L".sect .ic{width:20px;height:20px;flex:0 0 auto;border-radius:6px;"
    L"background:var(--surface2);color:var(--accent);display:flex;"
    L"align-items:center;justify-content:center;font-size:12px;}"
    L".sect.mt{margin-top:14px;}"
    // ---- form layout ----
    L".row{display:flex;flex-wrap:wrap;gap:10px;margin-bottom:9px;}"
    L".row:last-child{margin-bottom:0;}"
    L".fld{flex:1 1 160px;min-width:120px;display:flex;flex-direction:column;}"
    L".fld.sm{flex:0 0 120px;}"
    L".fld.xs{flex:0 0 92px;}"
    L".fld.wide{flex:1 1 100%;}"
    L".fld label{font-size:11.5px;color:var(--textDim);margin-bottom:4px;"
    L"font-weight:600;}"
    // ---- controls ----
    L"input,select,textarea{width:100%;height:36px;border:1px solid var(--border);"
    L"border-radius:var(--r2);background:var(--inputBg);color:var(--inputText);"
    L"padding:0 10px;font-family:inherit;font-size:13.5px;outline:none;"
    L"transition:border-color .12s,box-shadow .12s;}"
    L"select{cursor:pointer;}"
    L"input:hover,select:hover{border-color:#b9c2d2;}"
    L"input:focus,select:focus,textarea:focus{border-color:var(--accent);"
    L"box-shadow:var(--ring);}"
    L"input[readonly]{background:var(--surface2);color:var(--textDim);cursor:default;}"
    L"input::-ms-clear{display:none;}"
    // ---- buttons ----
    L".btn{height:38px;border:1px solid var(--border);border-radius:var(--r2);"
    L"background:var(--surface);color:var(--text);font-family:inherit;"
    L"font-size:13.5px;font-weight:600;cursor:pointer;padding:0 16px;"
    L"display:inline-flex;align-items:center;justify-content:center;gap:6px;"
    L"transition:all .12s;white-space:nowrap;}"
    L".btn:hover{background:var(--surface2);border-color:#b9c2d2;}"
    L".btn:active{transform:translateY(1px);}"
    L".btn:focus{box-shadow:var(--ring);}"
    L".btn.primary{background:var(--accent);border-color:var(--accent);"
    L"color:var(--accentText);box-shadow:var(--shadow);}"
    L".btn.primary:hover{background:var(--accent2);border-color:var(--accent2);}"
    L".btn.danger{background:var(--danger);border-color:var(--danger);color:#fff;}"
    L".btn.ghost{background:transparent;border-color:transparent;color:var(--textDim);}"
    L".btn.ghost:hover{background:var(--surface2);color:var(--text);}"
    L".btn.block{width:100%;}"
    L".btn.sm{height:32px;font-size:12.5px;padding:0 12px;}"
    L".btn[disabled]{opacity:.5;cursor:not-allowed;}"
    L".btnrow{display:flex;gap:8px;flex-wrap:wrap;}"
    L".btnrow.stack{flex-direction:column;}"
    L".actions{display:flex;gap:9px;flex-wrap:wrap;margin-top:12px;"
    L"padding-top:12px;border-top:1px solid var(--border);}"
    // ---- summary tiles (left zone) ----
    L".tile{border:1px solid var(--border);border-radius:var(--r2);padding:11px 12px;"
    L"background:linear-gradient(180deg,var(--surfaceTop),var(--surface));"
    L"margin-bottom:9px;}"
    L".tile:last-child{margin-bottom:0;}"
    L".tile .k{font-size:11.5px;color:var(--textDim);font-weight:600;}"
    L".tile .v{font-size:20px;font-weight:800;margin-top:3px;color:var(--text);}"
    L".tile.accent{background:linear-gradient(135deg,var(--accent),var(--accent2));"
    L"border-color:var(--accent);}"
    L".tile.accent .k,.tile.accent .v{color:#fff;}"
    // ---- billing block ----
    L".bill .line{display:flex;justify-content:space-between;align-items:center;"
    L"padding:6px 0;border-bottom:1px dashed var(--border);font-size:12.5px;}"
    L".bill .line span{color:var(--textDim);}"
    L".bill .line b{font-variant-numeric:tabular-nums;}"
    L".bill .pay{margin-top:11px;background:linear-gradient(135deg,var(--success),#0f9d58);"
    L"color:#fff;border-radius:var(--r2);padding:11px 13px;display:flex;"
    L"justify-content:space-between;align-items:center;font-weight:800;font-size:15px;"
    L"box-shadow:var(--shadow);}"
    // ---- avatar / patient panel (right zone) ----
    L".pcard{text-align:center;}"
    L".avatar{width:74px;height:74px;border-radius:50%;margin:2px auto 8px;"
    L"background:linear-gradient(135deg,var(--accent),var(--accent2));color:#fff;"
    L"display:flex;align-items:center;justify-content:center;font-size:30px;"
    L"font-weight:800;box-shadow:var(--shadow);}"
    L".pname{font-weight:800;font-size:14px;}"
    L".psub{color:var(--textDim);font-size:11.5px;margin-top:2px;}"
    L".chip{display:inline-flex;align-items:center;gap:5px;padding:3px 10px;"
    L"border-radius:999px;background:var(--surface2);color:var(--textDim);"
    L"font-size:11px;font-weight:600;margin:6px 3px 0;}"
    // ---- table (appointment grid) ----
    L".tablewrap{flex:1 1 auto;overflow:auto;border:1px solid var(--border);"
    L"border-radius:var(--r2);min-height:0;}"
    L"table{width:100%;border-collapse:collapse;font-size:12px;}"
    L"th,td{border-bottom:1px solid var(--border);padding:7px 8px;text-align:center;"
    L"white-space:nowrap;}"
    L"th{background:var(--surface2);color:var(--textDim);position:sticky;top:0;"
    L"font-weight:700;z-index:1;}"
    L"tbody tr:hover{background:var(--surface2);}"
    L"tr.cancel td{text-decoration:line-through;color:var(--textDim);}"
    L".empty{padding:30px 14px;text-align:center;color:var(--textDim);font-size:13px;}"
    // ---- toast ----
    L".toast{position:fixed;left:50%;bottom:24px;transform:translateX(-50%) translateY(8px);"
    L"background:var(--text);color:var(--bg);padding:11px 20px;border-radius:12px;"
    L"opacity:0;transition:opacity .22s,transform .22s;font-size:13px;z-index:80;"
    L"box-shadow:0 8px 24px rgba(0,0,0,.25);pointer-events:none;font-weight:600;}"
    L".toast.show{opacity:.98;transform:translateX(-50%) translateY(0);}"
    L".toast.bad{background:var(--danger);color:#fff;}"
    L".toast.ok{background:var(--success);color:#fff;}"
    // ---- compact / dense mode (toggled by body.compact) ----
    L"body.compact{font-size:12.5px;}"
    L"body.compact .card{padding:11px;}"
    L"body.compact input,body.compact select{height:32px;}"
    L"body.compact .btn{height:34px;}"
    L"body.compact .row{margin-bottom:7px;}"
    // ---- responsive breakpoints ----
    L"@media (max-width:1180px){.zone-left{flex-basis:210px;}.zone-right{flex-basis:280px;}}"
    L"@media (max-width:1000px){.zone-left{display:none;}}"
    L"@media (max-width:820px){.grid{flex-direction:column;overflow:auto;}"
    L".zone-right{flex-basis:auto;}.zone-center{min-width:0;}}"
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
    L"var AZ_TOK={bg:'#eef1f7',surface:'#ffffff',surfaceTop:'#ffffff',surface2:'#eef1f6',"
    L"  border:'#d7dce5',text:'#1d2330',textDim:'#6b7689',accent:'#2563eb',accent2:'#1d4ed8',"
    L"  accentText:'#ffffff',danger:'#dc2626',success:'#16a34a',warn:'#d97706',"
    L"  inputBg:'#ffffff',inputText:'#1d2330',"
    L"  r:'14px',r2:'10px',r3:'8px',sp:'14px',sp2:'10px',sp3:'7px',"
    L"  shadow:'0 1px 3px rgba(16,24,40,.08),0 6px 18px rgba(16,24,40,.06)',"
    L"  ring:'0 0 0 3px rgba(37,99,235,.18)'};"
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
    L"function setHeader(d){var m=$('meta');if(m)m.innerHTML="
    L"  esc(d.userName)+' \u2014 '+esc(d.userDept)+' \u2014 '+esc(d.shift)+' \u2014 '+esc(d.today);"
    L"  var su=$('s_user');if(su)su.innerHTML=esc(d.userName);"
    L"  var sd=$('s_dept');if(sd)sd.innerHTML=esc(d.userDept||'\u2014');"
    L"  var ss=$('s_shift');if(ss)ss.innerHTML=esc(d.shift);"
    L"  var st=$('s_today');if(st)st.innerHTML=esc(d.today);}"
    L"function azBoot(d){"
    L"  setHeader(d);"
    L"  fillSel($('ins'),d.insurances,true); fillSel($('supp'),d.supp,true);"
    L"  var nx=call('apptNext'); if(nx&&nx.next){var q=$('s_queue'); if(q)q.innerHTML=fa(nx.next);}"
    L"  recalc(); var n=$('nid'); if(n)n.focus();"
    L"}"
    L"function recalc(){"
    L"  var r=call('bill',{price:num($('price').value),discount:num($('discount').value),"
    L"    ptype:$('ptype').selectedIndex,ntype:$('ntype').selectedIndex,"
    L"    ins:parseInt($('ins').value||'0',10),supp:parseInt($('supp').value||'0',10)});"
    L"  $('b_total').innerHTML=fa(fmt(r.price)); $('b_main').innerHTML=fa(fmt(r.mainShare));"
    L"  $('b_diff').innerHTML=fa(fmt(r.baseDiff)); $('b_org').innerHTML=fa(fmt(r.orgShare));"
    L"  $('b_paid').innerHTML=fa(fmt(r.paid));"
    L"  if(num($('price').value)===0){ $('price').value=fmt(r.price); }"
    L"}"
    L"function setPatient(r,found){var a=$('p_avatar'),nm=$('p_name'),sb=$('p_sub');"
    L"  if(found){var fn=(r.first||'')+' '+(r.last||'');"
    L"    if(a)a.innerHTML=esc((r.first||r.last||'?').substr(0,1));"
    L"    if(nm)nm.innerHTML=esc(fn.replace(/^ +| +$/g,'')||'\u2014');"
    L"    if(sb)sb.innerHTML=esc((r.gender||'')+(r.birth?(' \u2014 '+r.birth):''));}"
    L"  else{if(a)a.innerHTML='?';if(nm)nm.innerHTML='\u0628\u06cc\u0645\u0627\u0631 \u062c\u062f\u06cc\u062f';if(sb)sb.innerHTML='\u0628\u062f\u0648\u0646 \u0633\u0627\u0628\u0642\u0647';}}"
    L"function lookup(){var nid=$('nid').value;if(nid.length<10)return;"
    L"  var r=call('lookup',{nid:nid});"
    L"  if(r.found){ $('first').value=r.first||''; $('last').value=r.last||'';"
    L"    $('father').value=r.father||''; $('birth').value=r.birth||''; $('mobile').value=r.mobile||'';"
    L"    if(r.gender){$('gender').value=r.gender;} setPatient(r,true);"
    L"    toast('\u0628\u06cc\u0645\u0627\u0631 \u06cc\u0627\u0641\u062a \u0634\u062f','ok'); $('ptype').focus(); }"
    L"  else if(!r.idValid){ toast('\u06a9\u062f \u0645\u0644\u06cc \u0646\u0627\u0645\u0639\u062a\u0628\u0631','bad'); }"
    L"  else { setPatient({},false); } }"
    L"function clearForm(){var ids=['nid','first','last','father','birth','mobile','phone','address','price','discount','archive','file','book','docCode','docName'];"
    L"  for(var i=0;i<ids.length;i++){var el=$(ids[i]);if(el)el.value='';}"
    L"  if($('ptype'))$('ptype').selectedIndex=0; setPatient({},false); recalc();"
    L"  var nx=call('apptNext'); if(nx&&nx.next){var q=$('s_queue'); if(q)q.innerHTML=fa(nx.next);}"
    L"  var n=$('nid'); if(n)n.focus();}"
    L"function azSubmit(){"
    L"  var insSel=$('ins'),suppSel=$('supp');"
    L"  var r=call('saveReception',{first:$('first').value,last:$('last').value,nid:$('nid').value,"
    L"    father:$('father').value,birth:$('birth').value,gender:$('gender').value,mobile:$('mobile').value,"
    L"    phone:$('phone').value,address:$('address').value,"
    L"    ptypeName:$('ptype').value,ntypeName:$('ntype').value,"
    L"    insName:insSel.options[insSel.selectedIndex]?insSel.options[insSel.selectedIndex].text:'',"
    L"    suppName:suppSel.options[suppSel.selectedIndex]?suppSel.options[suppSel.selectedIndex].text:'',"
    L"    ins:parseInt(insSel.value||'0',10),supp:parseInt(suppSel.value||'0',10),"
    L"    price:num($('price').value),discount:num($('discount').value)});"
    L"  if(r.ok){ toast('\u067e\u0630\u06cc\u0631\u0634 \u062b\u0628\u062a \u0634\u062f \u2014 \u0646\u0648\u0628\u062a '+fa(r.queue),'ok'); clearForm(); }"
    L"  else { toast(r.error||'\u062e\u0637\u0627 \u062f\u0631 \u062b\u0628\u062a','bad'); } }"
    L"window.azSubmit=azSubmit;"
    L"function doPrint(k){var r=call('print',{kind:k}); if(!r||r.ok!==true)toast('\u0686\u0627\u067e \u0627\u0646\u062c\u0627\u0645 \u0646\u0634\u062f','bad');}"
    L"function wire(){"
    L"  $('nid').onkeyup=function(e){var k=(e||window.event).keyCode; if($('nid').value.length===10&&k!==13)lookup();};"
    L"  $('price').onkeyup=recalc; $('discount').onkeyup=recalc;"
    L"  $('ptype').onchange=function(){$('price').value='';recalc();};"
    L"  $('ntype').onchange=function(){$('price').value='';recalc();};"
    L"  $('ins').onchange=recalc; $('supp').onchange=recalc;"
    L"  $('btnSave').onclick=azSubmit; $('btnNew').onclick=clearForm;"
    L"  $('btnReset').onclick=clearForm;"
    L"  $('p_ins').onclick=function(){doPrint(0);};"
    L"  $('p_rx').onclick=function(){doPrint(1);};"
    L"  $('p_last').onclick=function(){var r=call('printLast'); if(!r||r.ok!==true)toast('\u0686\u0627\u067e \u0627\u0646\u062c\u0627\u0645 \u0646\u0634\u062f','bad');};"
    L"  var sb=$('btnSearch'); if(sb)sb.onclick=function(){var n=$('nid'); if(n){n.focus();lookup();}};"
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
    // top bar: brand + meta (right) ; nothing on left (native owns window chrome)
    h += L"<div class='topbar'>"
         L"<div class='brand'><div class='dot'>\u0622</div>"
         L"<div class='title'>\u067e\u0630\u06cc\u0631\u0634 \u0628\u06cc\u0645\u0627\u0631</div></div>"
         L"<div class='meta' id='meta'></div></div>";
    // three-zone working grid
    h += L"<div class='grid'>";

    // ---- LEFT ZONE: quick summary + actions ----
    h += L"<div class='zone zone-left'>";
    h += L"<div class='card tight'>"
         L"<div class='sect'><span class='ic'>\u2261</span>\u062e\u0644\u0627\u0635\u0647</div>"
         L"<div class='tile accent'><div class='k'>\u0646\u0648\u0628\u062a \u0628\u0639\u062f\u06cc</div><div class='v' id='s_queue'>\u2014</div></div>"
         L"<div class='tile'><div class='k'>\u0634\u06cc\u0641\u062a</div><div class='v' id='s_shift' style='font-size:15px'>\u2014</div></div>"
         L"<div class='tile'><div class='k'>\u0628\u062e\u0634</div><div class='v' id='s_dept' style='font-size:15px'>\u2014</div></div>"
         L"<div class='tile'><div class='k'>\u062a\u0627\u0631\u06cc\u062e</div><div class='v' id='s_today' style='font-size:14px'>\u2014</div></div>"
         L"</div>";
    h += L"<div class='card tight flex'>"
         L"<div class='sect'><span class='ic'>\u2691</span>\u0639\u0645\u0644\u06cc\u0627\u062a</div>"
         L"<div class='btnrow stack'>"
         L"<button class='btn primary block' id='btnSaveQuick' data-nav data-enter='submit'>\u062b\u0628\u062a \u067e\u0630\u06cc\u0631\u0634</button>"
         L"<button class='btn block' id='btnReset'>\u067e\u0627\u06a9 \u06a9\u0631\u062f\u0646 \u0641\u0631\u0645</button>"
         L"</div></div>";
    h += L"</div>";   // zone-left

    // ---- CENTER ZONE: main reception form ----
    h += L"<div class='zone zone-center'>";
    h += L"<div class='card fill'>";
    h += L"<div class='sect'><span class='ic'>\u263b</span>\u0645\u0634\u062e\u0635\u0627\u062a \u0628\u06cc\u0645\u0627\u0631</div>";
    h += L"<div class='row'>"
         L"<div class='fld sm'><label>\u06a9\u062f \u0645\u0644\u06cc</label><input id='nid' data-nav maxlength='10' inputmode='numeric'></div>"
         L"<div class='fld'><label>\u0646\u0627\u0645</label><input id='first' data-nav></div>"
         L"<div class='fld'><label>\u0646\u0627\u0645 \u062e\u0627\u0646\u0648\u0627\u062f\u06af\u06cc</label><input id='last' data-nav></div>"
         L"</div>";
    h += L"<div class='row'>"
         L"<div class='fld'><label>\u0646\u0627\u0645 \u067e\u062f\u0631</label><input id='father' data-nav></div>"
         L"<div class='fld sm'><label>\u062a\u0627\u0631\u06cc\u062e \u062a\u0648\u0644\u062f</label><input id='birth' data-nav placeholder='____/__/__'></div>"
         L"<div class='fld xs'><label>\u062c\u0646\u0633\u06cc\u062a</label><select id='gender' data-nav><option>\u0645\u0631\u062f</option><option>\u0632\u0646</option></select></div>"
         L"</div>";
    h += L"<div class='row'>"
         L"<div class='fld sm'><label>\u0645\u0648\u0628\u0627\u06cc\u0644</label><input id='mobile' data-nav inputmode='numeric'></div>"
         L"<div class='fld sm'><label>\u062a\u0644\u0641\u0646 \u062b\u0627\u0628\u062a</label><input id='phone' data-nav inputmode='numeric'></div>"
         L"<div class='fld'><label>\u0622\u062f\u0631\u0633</label><input id='address' data-nav></div>"
         L"</div>";
    h += L"<div class='sect mt'><span class='ic'>\u26ce</span>\u0628\u06cc\u0645\u0647 \u0648 \u0646\u0648\u0628\u062a</div>";
    h += L"<div class='row'>"
         L"<div class='fld'><label>\u0646\u0648\u0639 \u0628\u06cc\u0645\u0627\u0631</label><select id='ptype' data-nav><option>\u0639\u0627\u062f\u06cc</option><option>\u0633\u0631\u067e\u0627\u06cc\u06cc</option><option>\u0628\u0633\u062a\u0631\u06cc</option></select></div>"
         L"<div class='fld'><label>\u0646\u0648\u0639 \u0646\u0648\u0628\u062a</label><select id='ntype' data-nav><option>\u0639\u0627\u062f\u06cc</option><option>\u0627\u0648\u0631\u0698\u0627\u0646\u0633</option><option>\u067e\u0631\u0633\u0646\u0644\u06cc</option></select></div>"
         L"</div>";
    h += L"<div class='row'>"
         L"<div class='fld'><label>\u0628\u06cc\u0645\u0647 \u067e\u0627\u06cc\u0647</label><select id='ins' data-nav></select></div>"
         L"<div class='fld'><label>\u0628\u06cc\u0645\u0647 \u0645\u06a9\u0645\u0644</label><select id='supp' data-nav></select></div>"
         L"<div class='fld xs'><label>\u0645\u0628\u0644\u063a (\u0631\u06cc\u0627\u0644)</label><input id='price' data-nav inputmode='numeric'></div>"
         L"<div class='fld xs'><label>\u062a\u062e\u0641\u06cc\u0641</label><input id='discount' data-nav inputmode='numeric'></div>"
         L"</div>";
    h += L"<div class='actions'>"
         L"<button class='btn primary' id='btnSave' data-nav data-enter='submit'>\u062b\u0628\u062a \u067e\u0630\u06cc\u0631\u0634 \u0648 \u0635\u062f\u0648\u0631 \u0642\u0628\u0636</button>"
         L"<button class='btn' id='btnNew' data-nav>\u067e\u0630\u06cc\u0631\u0634 \u062c\u062f\u06cc\u062f</button>"
         L"</div>";
    h += L"</div>";   // card fill
    h += L"</div>";   // zone-center

    // ---- RIGHT ZONE: patient / search / insurance / billing ----
    h += L"<div class='zone zone-right scroll'>";
    h += L"<div class='card tight pcard'>"
         L"<div class='avatar' id='p_avatar'>?</div>"
         L"<div class='pname' id='p_name'>\u0628\u06cc\u0645\u0627\u0631 \u062c\u062f\u06cc\u062f</div>"
         L"<div class='psub' id='p_sub'>\u0628\u062f\u0648\u0646 \u0633\u0627\u0628\u0642\u0647</div>"
         L"<div><span class='chip'>\u0646\u0633\u062e\u0647 \u0627\u0644\u06a9\u062a\u0631\u0648\u0646\u06cc\u06a9</span>"
         L"<span class='chip' id='s_user'>\u2014</span></div>"
         L"</div>";
    h += L"<div class='card tight'>"
         L"<div class='sect'><span class='ic'>\u26b2</span>\u06a9\u0644\u06cc\u062f\u0647\u0627\u06cc \u062c\u0633\u062a\u062c\u0648</div>"
         L"<div class='row'>"
         L"<div class='fld'><label>\u0634\u0645\u0627\u0631\u0647 \u0628\u0627\u06cc\u06af\u0627\u0646\u06cc</label><input id='archive' inputmode='numeric'></div>"
         L"<div class='fld'><label>\u0634\u0645\u0627\u0631\u0647 \u067e\u0631\u0648\u0646\u062f\u0647</label><input id='file' inputmode='numeric'></div>"
         L"</div>"
         L"<button class='btn block sm' id='btnSearch'>\u062c\u0633\u062a\u062c\u0648\u06cc \u0628\u06cc\u0645\u0627\u0631</button>"
         L"</div>";
    h += L"<div class='card tight'>"
         L"<div class='sect'><span class='ic'>\u271a</span>\u067e\u0632\u0634\u06a9 \u0645\u0639\u0627\u0644\u062c</div>"
         L"<div class='row'>"
         L"<div class='fld xs'><label>\u06a9\u062f \u0646\u0638\u0627\u0645</label><input id='docCode' inputmode='numeric'></div>"
         L"<div class='fld'><label>\u0646\u0627\u0645 \u067e\u0632\u0634\u06a9</label><input id='docName'></div>"
         L"</div>"
         L"</div>";
    h += L"<div class='card tight bill'>"
         L"<div class='sect'><span class='ic'>\u00a4</span>\u0635\u062f\u0648\u0631 \u0642\u0628\u0636</div>"
         L"<div class='line'><span>\u0645\u0628\u0644\u063a \u06a9\u0644</span><b id='b_total'>0</b></div>"
         L"<div class='line'><span>\u0633\u0647\u0645 \u0628\u06cc\u0645\u0647 \u067e\u0627\u06cc\u0647</span><b id='b_main'>0</b></div>"
         L"<div class='line'><span>\u0645\u0627\u0628\u0647\u200c\u0627\u0644\u062a\u0641\u0627\u0648\u062a</span><b id='b_diff'>0</b></div>"
         L"<div class='line'><span>\u0633\u0647\u0645 \u0628\u06cc\u0645\u0647 \u0645\u06a9\u0645\u0644</span><b id='b_org'>0</b></div>"
         L"<div class='pay'><span>\u067e\u0631\u062f\u0627\u062e\u062a\u06cc \u0628\u06cc\u0645\u0627\u0631</span><b id='b_paid'>0</b></div>"
         L"</div>";
    h += L"<div class='card tight'>"
         L"<div class='sect'><span class='ic'>\u2399</span>\u0686\u0627\u067e</div>"
         L"<div class='btnrow stack'>"
         L"<button class='btn block sm' id='p_ins'>\u0631\u0633\u06cc\u062f \u0628\u06cc\u0645\u0647</button>"
         L"<button class='btn block sm' id='p_rx'>\u0686\u0627\u067e \u0646\u0633\u062e\u0647</button>"
         L"<button class='btn block sm' id='p_last'>\u0686\u0627\u067e \u0622\u062e\u0631\u06cc\u0646 \u0642\u0628\u0636 (F8)</button>"
         L"</div></div>";
    h += L"</div>";   // zone-right

    h += L"</div>";   // grid
    h += L"<div class='toast' id='toast'></div>";
    h += L"</div>";   // wrap
    // wire the quick-save button in the left zone to azSubmit after scripts load.
    h += wh_commonScript();
    h += wh_receptionScript();
    h += L"<script type='text/javascript'>var bsq=$('btnSaveQuick');"
         L"if(bsq)bsq.onclick=function(){if(window.azSubmit)azSubmit();};</script>";
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
         L"<div class='title'>\u0646\u0648\u0628\u062a\u200c\u062f\u0647\u06cc</div></div>"
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
