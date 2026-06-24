// ============================================================================
//  webhost_assets.inc — the embedded HTML/CSS/JS documents for the hybrid
//  reception & appointment surfaces. Included into webhost_bridge.inc.
//
//  The documents are authored as ES5-safe scripts and force IE=edge so MSHTML
//  renders them in the most modern standards mode available on the host. They
//  are self-contained (no external CSS/JS/fonts — Vazirmatn is already
//  installed system-wide by installVazirFont(), with a sans-serif fallback).
//
//  Authoring rule: keep the C++ string literals readable. We assemble the doc
//  from std::wstring pieces so the markup is not one giant unescaped blob.
// ============================================================================

// Shared CSS + bridge JS used by both surfaces. Themed at runtime from the
// boot payload (CSS custom properties on :root).
static std::wstring wh_commonHead(){
    return
    L"<meta http-equiv='X-UA-Compatible' content='IE=edge'>"
    L"<meta charset='utf-8'>"
    L"<style>"
    L":root{--bg:#f3f5f9;--surface:#fff;--surfaceTop:#fff;--surface2:#eef1f6;"
    L"--border:#d7dce5;--text:#1d2330;--textDim:#6b7689;--accent:#2563eb;"
    L"--accent2:#1d4ed8;--accentText:#fff;--danger:#dc2626;--success:#16a34a;"
    L"--warn:#d97706;--inputBg:#fff;--inputText:#1d2330;}"
    L"*{box-sizing:border-box;-ms-user-select:none;}"
    L"html,body{height:100%;margin:0;}"
    L"body{font-family:Vazirmatn,Tahoma,'Segoe UI',sans-serif;direction:rtl;"
    L"background:var(--bg);color:var(--text);font-size:14px;overflow:hidden;}"
    L".wrap{height:100%;display:flex;flex-direction:column;padding:14px;}"
    L".topbar{display:flex;align-items:center;justify-content:space-between;"
    L"margin-bottom:12px;}"
    L".title{font-weight:700;font-size:18px;}"
    L".meta{color:var(--textDim);font-size:12px;}"
    L".grid{flex:1;display:flex;gap:14px;min-height:0;}"
    L".card{background:var(--surface);border:1px solid var(--border);"
    L"border-radius:14px;padding:16px;box-shadow:0 1px 3px rgba(0,0,0,.06);}"
    L".formcard{flex:1;overflow:auto;}"
    L".sidecard{width:330px;display:flex;flex-direction:column;gap:10px;overflow:auto;}"
    L".sect{font-weight:700;color:var(--accent);margin:6px 0 8px;font-size:13px;"
    L"border-bottom:1px solid var(--border);padding-bottom:4px;}"
    L".row{display:flex;flex-wrap:wrap;gap:10px;margin-bottom:8px;}"
    L".fld{flex:1 1 200px;min-width:150px;}"
    L".fld.sm{flex:0 0 130px;}"
    L".fld label{display:block;font-size:12px;color:var(--textDim);margin-bottom:3px;}"
    L"input,select{width:100%;height:34px;border:1px solid var(--border);"
    L"border-radius:9px;background:var(--inputBg);color:var(--inputText);"
    L"padding:0 9px;font-family:inherit;font-size:14px;outline:none;}"
    L"input:focus,select:focus{border-color:var(--accent);"
    L"box-shadow:0 0 0 3px rgba(37,99,235,.15);}"
    L"input[readonly]{background:var(--surface2);color:var(--textDim);}"
    L".btn{height:36px;border:1px solid var(--border);border-radius:10px;"
    L"background:var(--surface);color:var(--text);font-family:inherit;"
    L"font-size:14px;cursor:pointer;padding:0 14px;}"
    L".btn:hover{background:var(--surface2);}"
    L".btn.primary{background:var(--accent);border-color:var(--accent);"
    L"color:var(--accentText);}"
    L".btn.primary:hover{background:var(--accent2);}"
    L".btn.danger{background:var(--danger);border-color:var(--danger);color:#fff;}"
    L".btn.block{width:100%;margin-bottom:8px;}"
    L".bill{font-size:13px;}"
    L".bill .line{display:flex;justify-content:space-between;padding:4px 0;"
    L"border-bottom:1px dashed var(--border);}"
    L".bill .pay{margin-top:10px;background:var(--success);color:#fff;"
    L"border-radius:10px;padding:10px;display:flex;justify-content:space-between;"
    L"font-weight:700;font-size:15px;}"
    L".actions{display:flex;gap:8px;flex-wrap:wrap;margin-top:8px;}"
    L".toast{position:fixed;left:50%;bottom:22px;transform:translateX(-50%);"
    L"background:var(--text);color:var(--bg);padding:10px 18px;border-radius:12px;"
    L"opacity:0;transition:opacity .25s;font-size:13px;z-index:50;}"
    L".toast.show{opacity:.96;}"
    L"table{width:100%;border-collapse:collapse;font-size:12px;}"
    L"th,td{border:1px solid var(--border);padding:5px 6px;text-align:center;"
    L"white-space:nowrap;}"
    L"th{background:var(--surface2);color:var(--textDim);position:sticky;top:0;}"
    L"tr.cancel td{text-decoration:line-through;color:var(--textDim);}"
    L".tablewrap{flex:1;overflow:auto;border:1px solid var(--border);"
    L"border-radius:10px;}"
    L"</style>";
}

// The bridge JS shared by both pages: theme application, the azReceive push
// channel, a synchronous external.call helper, keyboard navigation (Enter ->
// next field, Tab/Shift+Tab native) and a toast.
static std::wstring wh_commonScript(){
    return
    L"<script type='text/javascript'>"
    L"var AZ={kind:0,boot:null};"
    L"function call(verb,args){try{"
    L"  var s=window.external.call(verb, args?JSON.stringify(args):'{}');"
    L"  return s?JSON.parse(s):{};"
    L"}catch(e){ if(window.external&&window.external.log)window.external.log('call '+verb+':'+e); return {}; }}"
    L"function applyTheme(t){if(!t)return;var r=document.documentElement.style;"
    L"  for(var k in t){ if(typeof t[k]==='string'&&t[k].charAt(0)==='#'){ r.setProperty('--'+k,t[k]); } } }"
    L"function toast(msg,bad){var t=document.getElementById('toast');if(!t)return;"
    L"  t.innerHTML=msg; t.className='toast show'+(bad?' bad':'');"
    L"  setTimeout(function(){t.className='toast';},2200);}"
    // navigation: Enter moves to the next [data-nav] control; Shift+Tab/Tab are
    // left to the browser (standard). This preserves the native keyboard flow.
    L"function navList(){var n=document.querySelectorAll('[data-nav]');var a=[];"
    L"  for(var i=0;i<n.length;i++){ if(!n[i].disabled&&n[i].offsetParent!==null) a.push(n[i]); } return a;}"
    L"function onKey(ev){var e=ev||window.event;var k=e.keyCode||e.which;"
    L"  if(k===13){var list=navList();var idx=-1;for(var i=0;i<list.length;i++){if(list[i]===e.target||list[i]===document.activeElement){idx=i;break;}}"
    L"    if(idx>=0){ if(e.target&&e.target.getAttribute&&e.target.getAttribute('data-enter')==='submit'){ if(window.azSubmit)azSubmit(); }"
    L"       else { var nx=list[idx+1]; if(nx){nx.focus(); if(nx.select)try{nx.select();}catch(_){}} else if(window.azSubmit){azSubmit();} } }"
    L"    if(e.preventDefault)e.preventDefault(); return false; } }"
    L"document.onkeydown=onKey;"
    // the C++ push channel.
    L"function azReceive(s){ try{var d=JSON.parse(s);}catch(e){return;} "
    L"  if(d.event==='boot'){ AZ.boot=d; AZ.kind=d.kind; applyTheme(d.theme); if(window.azBoot)azBoot(d); } "
    L"  else if(d.event==='theme'){ applyTheme(d); } "
    L"  else if(window.azEvent){ azEvent(d); } }"
    L"window.azReceive=azReceive;"
    L"</script>";
}

// ---------------------------------------------------------- reception JS -----
static std::wstring wh_receptionScript(){
    return
    L"<script type='text/javascript'>"
    L"function $(id){return document.getElementById(id);}"
    L"function fmt(n){n=Math.round(n||0);var s=''+Math.abs(n);var o='';"
    L"  for(var i=0;i<s.length;i++){if(i>0&&(s.length-i)%3===0)o+=',';o+=s.charAt(i);}return(n<0?'-':'')+o;}"
    L"function fa(s){var m={'0':'\u06f0','1':'\u06f1','2':'\u06f2','3':'\u06f3','4':'\u06f4','5':'\u06f5','6':'\u06f6','7':'\u06f7','8':'\u06f8','9':'\u06f9'};"
    L"  return (''+s).replace(/[0-9]/g,function(d){return m[d];});}"
    L"function num(s){return parseInt(((''+s).replace(/[^0-9]/g,''))||'0',10);}"
    L"function fillSel(sel,arr){sel.innerHTML='';for(var i=0;i<arr.length;i++){var o=document.createElement('option');o.value=arr[i].idx;o.text=arr[i].name;sel.appendChild(o);}}"
    L"function azBoot(d){"
    L"  $('meta').innerHTML=d.userName+' \u2014 '+d.userDept+' \u2014 '+d.shift+' \u2014 '+d.today;"
    L"  fillSel($('ins'),d.insurances); fillSel($('supp'),d.supp);"
    L"  recalc(); $('nid').focus();"
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
    L"function lookup(){var nid=$('nid').value;if(nid.length<10)return;"
    L"  var r=call('lookup',{nid:nid});"
    L"  if(r.found){ $('first').value=r.first||''; $('last').value=r.last||'';"
    L"    $('father').value=r.father||''; $('birth').value=r.birth||''; $('mobile').value=r.mobile||'';"
    L"    if(r.gender){$('gender').value=r.gender;} toast('\u0628\u06cc\u0645\u0627\u0631 \u06cc\u0627\u0641\u062a \u0634\u062f'); $('ptype').focus(); }"
    L"  else if(!r.idValid){ toast('\u06a9\u062f \u0645\u0644\u06cc \u0646\u0627\u0645\u0639\u062a\u0628\u0631',1); } }"
    L"function clearForm(){var ids=['nid','first','last','father','birth','mobile','phone','address','price','discount'];"
    L"  for(var i=0;i<ids.length;i++)$(ids[i]).value=''; $('ptype').selectedIndex=1; recalc(); $('nid').focus();}"
    L"function azSubmit(){"
    L"  var r=call('saveReception',{first:$('first').value,last:$('last').value,nid:$('nid').value,"
    L"    father:$('father').value,birth:$('birth').value,gender:$('gender').value,mobile:$('mobile').value,"
    L"    phone:$('phone').value,address:$('address').value,"
    L"    ptypeName:$('ptype').value,ntypeName:$('ntype').value,"
    L"    insName:$('ins').options[$('ins').selectedIndex].text,suppName:$('supp').options[$('supp').selectedIndex].text,"
    L"    ins:parseInt($('ins').value||'0',10),supp:parseInt($('supp').value||'0',10),"
    L"    price:num($('price').value),discount:num($('discount').value)});"
    L"  if(r.ok){ toast('\u067e\u0630\u06cc\u0631\u0634 \u062b\u0628\u062a \u0634\u062f \u2014 \u0646\u0648\u0628\u062a '+fa(r.queue)); clearForm(); }"
    L"  else { toast(r.error||'\u062e\u0637\u0627 \u062f\u0631 \u062b\u0628\u062a',1); } }"
    L"window.azSubmit=azSubmit;"
    L"function wire(){"
    L"  $('nid').onkeyup=function(e){var k=(e||window.event).keyCode; if($('nid').value.length===10&&k!==13)lookup();};"
    L"  $('price').onkeyup=recalc; $('discount').onkeyup=recalc;"
    L"  $('ptype').onchange=function(){$('price').value='';recalc();};"
    L"  $('ntype').onchange=function(){$('price').value='';recalc();};"
    L"  $('ins').onchange=recalc; $('supp').onchange=recalc;"
    L"  $('btnSave').onclick=azSubmit; $('btnNew').onclick=clearForm;"
    L"  $('p_ins').onclick=function(){call('print',{kind:0});};"
    L"  $('p_rx').onclick=function(){call('print',{kind:1});};"
    L"  $('p_last').onclick=function(){call('printLast');};"
    L"}"
    L"wire(); call('ping');"
    L"</script>";
}

// --------------------------------------------------------- appointment JS ----
static std::wstring wh_appointmentScript(){
    return
    L"<script type='text/javascript'>"
    L"function $(id){return document.getElementById(id);}"
    L"function fa(s){var m={'0':'\u06f0','1':'\u06f1','2':'\u06f2','3':'\u06f3','4':'\u06f4','5':'\u06f5','6':'\u06f6','7':'\u06f7','8':'\u06f8','9':'\u06f9'};"
    L"  return (''+s).replace(/[0-9]/g,function(d){return m[d];});}"
    L"var DOCS=[];"
    L"function fillDoctors(){var sel=$('doctor');sel.innerHTML='';for(var i=0;i<DOCS.length;i++){var o=document.createElement('option');o.value=i;o.text=DOCS[i].name+' \u2014 '+DOCS[i].specialty;sel.appendChild(o);}fillServices();}"
    L"function fillServices(){var sel=$('service');sel.innerHTML='';var d=DOCS[$('doctor').selectedIndex];if(!d)return;for(var i=0;i<d.services.length;i++){var o=document.createElement('option');o.text=d.services[i];sel.appendChild(o);}}"
    L"function azBoot(d){"
    L"  $('meta').innerHTML=d.userName+' \u2014 '+d.userDept+' \u2014 '+d.shift+' \u2014 '+d.today;"
    L"  $('date').value=d.today; DOCS=d.doctors||[]; fillDoctors(); refresh(); $('nid').focus();"
    L"}"
    L"function refresh(){var rows=call('apptList'); if(!rows.length&&rows.length!==0){rows=[];}"
    L"  var tb=$('gbody'); tb.innerHTML='';"
    L"  for(var i=0;i<rows.length;i++){var r=rows[i];var tr=document.createElement('tr'); if(r.cancelled)tr.className='cancel';"
    L"    var btn=r.cancelled?'\u2014':'<button class=\"btn danger\" style=\"height:26px;padding:0 8px\" onclick=\"cancelRow('+r.idx+')\">\u2716</button>';"
    L"    tr.innerHTML='<td>'+btn+'</td><td>'+fa(r.queue)+'</td><td>'+fa(r.time)+'</td><td>'+fa(r.date)+'</td><td>'+r.first+'</td><td>'+r.last+'</td><td>'+r.doctor+'</td><td>'+r.service+'</td><td>'+r.kind+'</td>';"
    L"    tb.appendChild(tr);} }"
    L"function cancelRow(idx){var r=call('cancelAppointment',{idx:idx}); if(r.ok){toast('\u0646\u0648\u0628\u062a \u0627\u0628\u0637\u0627\u0644 \u0634\u062f');refresh();}}"
    L"window.cancelRow=cancelRow;"
    L"function lookup(){var nid=$('nid').value;if(nid.length<10)return;var r=call('lookup',{nid:nid});"
    L"  if(r.found){$('first').value=r.first||'';$('last').value=r.last||'';$('mobile').value=r.mobile||'';toast('\u0628\u06cc\u0645\u0627\u0631 \u06cc\u0627\u0641\u062a \u0634\u062f');}}"
    L"function clearForm(){var ids=['nid','first','last','mobile','time'];for(var i=0;i<ids.length;i++)$(ids[i]).value='';$('nid').focus();}"
    L"function azSubmit(){var d=DOCS[$('doctor').selectedIndex]||{};"
    L"  var r=call('saveAppointment',{nid:$('nid').value,first:$('first').value,last:$('last').value,mobile:$('mobile').value,"
    L"    doctor:d.name||'',service:$('service').value,date:$('date').value,time:$('time').value,kind:$('kind').value});"
    L"  if(r.ok){toast('\u0646\u0648\u0628\u062a \u062b\u0628\u062a \u0634\u062f \u2014 \u0634\u0645\u0627\u0631\u0647 '+fa(r.queue));clearForm();refresh();}"
    L"  else{toast(r.error||'\u062e\u0637\u0627 \u062f\u0631 \u062b\u0628\u062a',1);} }"
    L"window.azSubmit=azSubmit;"
    L"function wire(){"
    L"  $('doctor').onchange=fillServices;"
    L"  $('nid').onkeyup=function(e){var k=(e||window.event).keyCode;if($('nid').value.length===10&&k!==13)lookup();};"
    L"  $('btnReserve').onclick=azSubmit; $('btnNew').onclick=clearForm;"
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
    h += L"<div class='topbar'><div class='title'>\u067e\u0630\u06cc\u0631\u0634 \u0628\u06cc\u0645\u0627\u0631</div>"
         L"<div class='meta' id='meta'></div></div>";
    h += L"<div class='grid'>";
    // form card (right / wide)
    h += L"<div class='card formcard'>";
    h += L"<div class='sect'>\u0645\u0634\u062e\u0635\u0627\u062a \u0628\u06cc\u0645\u0627\u0631</div>";
    h += L"<div class='row'>"
         L"<div class='fld sm'><label>\u06a9\u062f \u0645\u0644\u06cc</label><input id='nid' data-nav maxlength='10' inputmode='numeric'></div>"
         L"<div class='fld'><label>\u0646\u0627\u0645</label><input id='first' data-nav></div>"
         L"<div class='fld'><label>\u0646\u0627\u0645 \u062e\u0627\u0646\u0648\u0627\u062f\u06af\u06cc</label><input id='last' data-nav></div>"
         L"</div>";
    h += L"<div class='row'>"
         L"<div class='fld'><label>\u0646\u0627\u0645 \u067e\u062f\u0631</label><input id='father' data-nav></div>"
         L"<div class='fld sm'><label>\u062a\u0627\u0631\u06cc\u062e \u062a\u0648\u0644\u062f</label><input id='birth' data-nav placeholder='____/__/__'></div>"
         L"<div class='fld sm'><label>\u062c\u0646\u0633\u06cc\u062a</label><select id='gender' data-nav><option>\u0645\u0631\u062f</option><option>\u0632\u0646</option></select></div>"
         L"</div>";
    h += L"<div class='row'>"
         L"<div class='fld sm'><label>\u0645\u0648\u0628\u0627\u06cc\u0644</label><input id='mobile' data-nav inputmode='numeric'></div>"
         L"<div class='fld sm'><label>\u062a\u0644\u0641\u0646 \u062b\u0627\u0628\u062a</label><input id='phone' data-nav inputmode='numeric'></div>"
         L"<div class='fld'><label>\u0622\u062f\u0631\u0633</label><input id='address' data-nav></div>"
         L"</div>";
    h += L"<div class='sect'>\u0628\u06cc\u0645\u0647 \u0648 \u0646\u0648\u0628\u062a</div>";
    h += L"<div class='row'>"
         L"<div class='fld'><label>\u0646\u0648\u0639 \u0628\u06cc\u0645\u0627\u0631</label><select id='ptype' data-nav><option>\u0639\u0627\u062f\u06cc</option><option>\u0633\u0631\u067e\u0627\u06cc\u06cc</option><option>\u0628\u0633\u062a\u0631\u06cc</option></select></div>"
         L"<div class='fld'><label>\u0646\u0648\u0639 \u0646\u0648\u0628\u062a</label><select id='ntype' data-nav><option>\u0639\u0627\u062f\u06cc</option><option>\u0627\u0648\u0631\u0698\u0627\u0646\u0633</option><option>\u067e\u0631\u0633\u0646\u0644\u06cc</option></select></div>"
         L"</div>";
    h += L"<div class='row'>"
         L"<div class='fld'><label>\u0628\u06cc\u0645\u0647 \u067e\u0627\u06cc\u0647</label><select id='ins' data-nav></select></div>"
         L"<div class='fld'><label>\u0628\u06cc\u0645\u0647 \u0645\u06a9\u0645\u0644</label><select id='supp' data-nav></select></div>"
         L"</div>";
    h += L"<div class='row'>"
         L"<div class='fld sm'><label>\u0645\u0628\u0644\u063a \u062e\u062f\u0645\u062a (\u0631\u06cc\u0627\u0644)</label><input id='price' data-nav inputmode='numeric'></div>"
         L"<div class='fld sm'><label>\u062a\u062e\u0641\u06cc\u0641 (\u0631\u06cc\u0627\u0644)</label><input id='discount' data-nav inputmode='numeric'></div>"
         L"</div>";
    h += L"<div class='actions'>"
         L"<button class='btn primary' id='btnSave' data-nav data-enter='submit'>\u062b\u0628\u062a \u067e\u0630\u06cc\u0631\u0634 \u0648 \u0635\u062f\u0648\u0631 \u0642\u0628\u0636</button>"
         L"<button class='btn' id='btnNew' data-nav>\u067e\u0630\u06cc\u0631\u0634 \u062c\u062f\u06cc\u062f</button>"
         L"</div>";
    h += L"</div>";   // formcard
    // side card (billing + print/management actions)
    h += L"<div class='sidecard'>";
    h += L"<div class='card bill'>"
         L"<div class='sect'>\u0635\u062f\u0648\u0631 \u0642\u0628\u0636</div>"
         L"<div class='line'><span>\u0645\u0628\u0644\u063a \u06a9\u0644</span><b id='b_total'>0</b></div>"
         L"<div class='line'><span>\u0633\u0647\u0645 \u0628\u06cc\u0645\u0647 \u067e\u0627\u06cc\u0647</span><b id='b_main'>0</b></div>"
         L"<div class='line'><span>\u0645\u0627\u0628\u0647\u200c\u0627\u0644\u062a\u0641\u0627\u0648\u062a</span><b id='b_diff'>0</b></div>"
         L"<div class='line'><span>\u0633\u0647\u0645 \u0628\u06cc\u0645\u0647 \u0645\u06a9\u0645\u0644</span><b id='b_org'>0</b></div>"
         L"<div class='pay'><span>\u067e\u0631\u062f\u0627\u062e\u062a\u06cc \u0628\u06cc\u0645\u0627\u0631</span><b id='b_paid'>0</b></div>"
         L"</div>";
    h += L"<div class='card'>"
         L"<div class='sect'>\u0686\u0627\u067e \u0648 \u0645\u062f\u06cc\u0631\u06cc\u062a</div>"
         L"<button class='btn block' id='p_ins'>\u0631\u0633\u06cc\u062f \u0628\u06cc\u0645\u0647</button>"
         L"<button class='btn block' id='p_rx'>\u0686\u0627\u067e \u0646\u0633\u062e\u0647</button>"
         L"<button class='btn block' id='p_last'>\u0686\u0627\u067e \u0622\u062e\u0631\u06cc\u0646 \u0642\u0628\u0636 (F8)</button>"
         L"</div>";
    h += L"</div>";   // sidecard
    h += L"</div>";   // grid
    h += L"<div class='toast' id='toast'></div>";
    h += L"</div>";   // wrap
    h += wh_commonScript();
    h += wh_receptionScript();
    h += L"</body></html>";
    return h;
}

// Appointment HTML doc.
static std::wstring wh_appointmentHtml(){
    std::wstring h;
    h += L"<!DOCTYPE html><html><head>";
    h += wh_commonHead();
    h += L"</head><body><div class='wrap'>";
    h += L"<div class='topbar'><div class='title'>\u0646\u0648\u0628\u062a\u200c\u062f\u0647\u06cc</div>"
         L"<div class='meta' id='meta'></div></div>";
    h += L"<div class='grid'>";
    // left: appointment + patient form
    h += L"<div class='sidecard' style='width:380px'>";
    h += L"<div class='card'>"
         L"<div class='sect'>\u0645\u0634\u062e\u0635\u0627\u062a \u0646\u0648\u0628\u062a</div>"
         L"<div class='row'><div class='fld'><label>\u067e\u0632\u0634\u06a9</label><select id='doctor' data-nav></select></div></div>"
         L"<div class='row'><div class='fld'><label>\u062e\u062f\u0645\u062a</label><select id='service' data-nav></select></div></div>"
         L"<div class='row'>"
         L"<div class='fld sm'><label>\u062a\u0627\u0631\u06cc\u062e \u0646\u0648\u0628\u062a</label><input id='date' data-nav placeholder='____/__/__'></div>"
         L"<div class='fld sm'><label>\u0633\u0627\u0639\u062a</label><input id='time' data-nav placeholder='__:__'></div>"
         L"<div class='fld sm'><label>\u0646\u0648\u0639</label><select id='kind' data-nav><option>\u062d\u0636\u0648\u0631\u06cc</option><option>\u063a\u06cc\u0631\u062d\u0636\u0648\u0631\u06cc</option></select></div>"
         L"</div>";
    h += L"</div>";
    h += L"<div class='card'>"
         L"<div class='sect'>\u0645\u0634\u062e\u0635\u0627\u062a \u0628\u06cc\u0645\u0627\u0631</div>"
         L"<div class='row'>"
         L"<div class='fld sm'><label>\u06a9\u062f \u0645\u0644\u06cc</label><input id='nid' data-nav maxlength='10' inputmode='numeric'></div>"
         L"<div class='fld'><label>\u0646\u0627\u0645</label><input id='first' data-nav></div>"
         L"</div>"
         L"<div class='row'>"
         L"<div class='fld'><label>\u0646\u0627\u0645 \u062e\u0627\u0646\u0648\u0627\u062f\u06af\u06cc</label><input id='last' data-nav></div>"
         L"<div class='fld sm'><label>\u0645\u0648\u0628\u0627\u06cc\u0644</label><input id='mobile' data-nav inputmode='numeric'></div>"
         L"</div>";
    h += L"<div class='actions'>"
         L"<button class='btn primary' id='btnReserve' data-nav data-enter='submit'>\u062b\u0628\u062a \u0646\u0648\u0628\u062a</button>"
         L"<button class='btn' id='btnNew' data-nav>\u0646\u0648\u0628\u062a \u062c\u062f\u06cc\u062f</button>"
         L"</div>";
    h += L"</div>";
    h += L"</div>";   // sidecard
    // right: appointments grid
    h += L"<div class='card formcard'>";
    h += L"<div class='sect'>\u0646\u0648\u0628\u062a\u200c\u0647\u0627\u06cc \u0627\u0645\u0631\u0648\u0632</div>";
    h += L"<div class='tablewrap'><table id='grid'><thead><tr>"
         L"<th>\u0627\u0628\u0637\u0627\u0644</th><th>\u0634 \u0646\u0648\u0628\u062a</th><th>\u0633\u0627\u0639\u062a</th>"
         L"<th>\u062a\u0627\u0631\u06cc\u062e</th><th>\u0646\u0627\u0645</th><th>\u0646\u0627\u0645 \u062e\u0627\u0646\u0648\u0627\u062f\u06af\u06cc</th>"
         L"<th>\u067e\u0632\u0634\u06a9</th><th>\u062e\u062f\u0645\u062a</th><th>\u0646\u0648\u0639</th>"
         L"</tr></thead><tbody id='gbody'></tbody></table></div>";
    h += L"</div>";
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
