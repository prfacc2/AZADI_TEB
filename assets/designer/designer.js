/* designer.js — print designer engine (lightweight, dependency-free).
   Talks to C++ via window.azBridge when embedded; falls back to standalone
   browser mode (localStorage) for testing. */
(function(){
"use strict";

// ----------------------------------------------------------------- bridge ---
// In the embedded host, C++ injects window.azBridge.call(verb,jsonArgs)->jsonStr
// (WebView2 / native message channel). We also support a postMessage fallback.
// The designer is served by the app's loopback HTTP host. The bridge talks to
// it via synchronous same-origin requests to /api/* — this is engine-agnostic
// (works in WebView2, Trident, or any browser) and needs no script injection.
// When the page is opened from a plain file:// (developer mode) the bridge is
// considered absent and the designer runs standalone (localStorage).
// §1.19.1 — the page is served by the app's loopback HTTP host and opened in
// the system browser. The bridge talks to /api/* over same-origin requests.
// We keep a SHORT synchronous request (with a hard timeout) because every call
// site is simple request/response; the host replies instantly from memory so
// there is no perceptible block, and this keeps the call sites trivial. If the
// page is opened from file:// (developer mode) the bridge is absent and the
// designer runs fully standalone (localStorage + file download/upload).
var Bridge = {
  _on: (location.protocol === "http:" || location.protocol === "https:"),
  request: function(verb, args){
    if(!this._on) return null;
    try{
      var xhr = new XMLHttpRequest();
      xhr.open("POST", "api/"+verb, false);   // same-origin, loopback, instant
      xhr.timeout = 4000;                       // never hang the UI
      xhr.setRequestHeader("Content-Type","application/json;charset=utf-8");
      xhr.send(JSON.stringify(args||{}));
      if(xhr.status>=200 && xhr.status<300){
        return xhr.responseText ? JSON.parse(xhr.responseText) : {};
      }
    }catch(e){ /* host not reachable → behave like standalone */ }
    return null;
  },
  has: function(){ return this._on; }
};

// ------------------------------------------------------------- paper data ---
var PAPER = {A4:[210,297],A5:[148,210],A6:[105,148],B5:[176,250],
             Letter:[215.9,279.4],R80:[80,200],R58:[58,200]};
function paperDims(p,orient){ var d=PAPER[p]||[148,210]; var w=d[0],h=d[1];
  if(orient===1){var t=w;w=h;h=t;} return [w,h]; }

// ------------------------------------------------------------- app state ----
var S = {
  design: null,        // current PrintDesign
  selId: 0,
  zoom: 1,             // px per mm baseline computed in fitZoom
  pxPerMM: 3.78,       // base (96dpi: 1mm≈3.7795px)
  scale: 1,            // zoom multiplier
  undo: [], redo: [],
  dirty: false,
  templates: window.AZ_TEMPLATES || []
};

var $paper = document.getElementById("paper");
var $scroll = document.getElementById("canvasScroll");
var $selBox = document.getElementById("selBox");

// ---------------------------------------------------------------- helpers ---
function faDigits(s){ s=String(s); var f="۰۱۲۳۴۵۶۷۸۹";
  return s.replace(/[0-9]/g,function(d){return f[+d];}); }
function clone(o){ return JSON.parse(JSON.stringify(o)); }
function genId(){ var m=0; (S.design.items||[]).forEach(function(it){ if(it.id>m)m=it.id; }); return m+1; }
function findItem(id){ return (S.design.items||[]).find(function(it){return it.id===id;}); }
function toast(msg){ var t=document.getElementById("toast"); t.textContent=msg;
  t.classList.remove("hidden"); t.classList.add("show");
  clearTimeout(toast._t); toast._t=setTimeout(function(){ t.classList.remove("show");
    setTimeout(function(){t.classList.add("hidden");},220); },1800); }

// ------------------------------------------------------------- undo stack ---
function pushUndo(){ S.undo.push(clone(S.design)); if(S.undo.length>60) S.undo.shift();
  S.redo.length=0; S.dirty=true; updateUndoButtons(); }
function doUndo(){ if(!S.undo.length) return; S.redo.push(clone(S.design));
  S.design=S.undo.pop(); S.selId=0; renderAll(); updateUndoButtons(); toast("بازگشت"); }
function doRedo(){ if(!S.redo.length) return; S.undo.push(clone(S.design));
  S.design=S.redo.pop(); S.selId=0; renderAll(); updateUndoButtons(); toast("جلو"); }
function updateUndoButtons(){
  document.getElementById("btnUndo").disabled=!S.undo.length;
  document.getElementById("btnRedo").disabled=!S.redo.length; }

// ------------------------------------------------------------- rendering ----
var ITEM_LABELS = {label:"برچسب",field:"داده",hline:"خط افقی",vline:"خط عمودی",
  rect:"کادر",frame:"حاشیه صفحه",logo:"لوگو",photo:"عکس بیمار",qr:"بارکد/QR",apptno:"شماره نوبت"};

function mm(v){ return v*S.pxPerMM*S.scale; }

function styleItem(el,it){
  el.style.left=mm(it.x)+"px"; el.style.top=mm(it.y)+"px";
  el.style.width=mm(it.w)+"px"; el.style.height=mm(it.h)+"px";
  el.style.transform = it.rot ? "rotate("+it.rot+"deg)" : "";
  el.style.opacity = (it.opacity==null?1:it.opacity);
  el.style.zIndex = it.z||0;
}

function sampleFor(it){
  if(it.type==="field"){
    var f=window.AZ_FIELDS[it.field];
    return (it.prefix||"") + (f?f.sample:(it.field||"داده"));
  }
  if(it.type==="apptno") return faDigits(it.startValue||1);
  return it.text||"";
}

function buildItemEl(it){
  var el=document.createElement("div");
  el.className="pi pi-"+it.type; el.dataset.id=it.id;
  styleItem(el,it);

  if(it.type==="label"||it.type==="field"||it.type==="apptno"){
    var t=document.createElement("div"); t.className="pi-text";
    t.textContent = it.type==="label" ? (it.text||"برچسب") : sampleFor(it);
    t.style.color=it.textColor||"#000";
    t.style.fontSize=mm(it.pt*0.3528)+"px"; // pt->mm->px
    t.style.fontWeight=it.bold?"700":"400";
    t.style.fontStyle=it.italic?"italic":"normal";
    t.style.fontFamily=it.font||"Vazirmatn";
    t.style.lineHeight=it.lineSpacing||1.2;
    t.style.textAlign = it.align===1?"center":(it.align===2?"left":"right");
    el.style.justifyContent = it.align===1?"center":(it.align===2?"flex-start":"flex-end");
    if(!it.fillTransparent){ el.style.background=it.fillColor||"#fff"; }
    if(it.borderWidth>0){ el.style.border=mm(it.borderWidth)+"px "+borderCss(it.borderStyle)+" "+(it.borderColor||"#000"); }
    if(it.corner>0) el.style.borderRadius=mm(it.corner)+"px";
    el.appendChild(t);
  } else if(it.type==="hline"){
    el.style.borderTop=Math.max(1,mm(it.borderWidth))+"px "+borderCss(it.borderStyle)+" "+(it.borderColor||"#000");
    el.style.height="0";
  } else if(it.type==="vline"){
    el.style.borderInlineStart=Math.max(1,mm(it.borderWidth))+"px "+borderCss(it.borderStyle)+" "+(it.borderColor||"#000");
    el.style.width="0";
  } else if(it.type==="rect"||it.type==="frame"){
    el.style.border=Math.max(1,mm(it.borderWidth))+"px "+borderCss(it.borderStyle)+" "+(it.borderColor||"#000");
    if(it.corner>0) el.style.borderRadius=mm(it.corner)+"px";
    if(!it.fillTransparent) el.style.background=it.fillColor;
  } else if(it.type==="logo"||it.type==="photo"||it.type==="qr"){
    el.textContent = it.type==="logo"?"لوگو":(it.type==="qr"?"QR":"عکس");
    if(it.borderWidth>0) el.style.border=Math.max(1,mm(it.borderWidth))+"px solid "+(it.borderColor||"#aaa");
  }
  return el;
}
function borderCss(s){ return s===1?"dashed":(s===2?"dotted":(s===3?"double":"solid")); }

function renderAll(){
  if(!S.design) return;
  var d=paperDims(S.design.paper,S.design.orientation);
  S.design.paperW=d[0]; S.design.paperH=d[1];
  $paper.style.width=mm(S.design.paperW)+"px";
  $paper.style.height=mm(S.design.paperH)+"px";
  // remove old items (keep selBox)
  Array.prototype.slice.call($paper.querySelectorAll(".pi")).forEach(function(n){n.remove();});
  var items=(S.design.items||[]).slice().sort(function(a,b){return (a.z||0)-(b.z||0);});
  items.forEach(function(it){ $paper.appendChild(buildItemEl(it)); });
  syncToolbar(); renderSelection(); renderLayers();
}

function renderSelection(){
  var it=findItem(S.selId);
  if(!it){ $selBox.classList.add("hidden");
    Array.prototype.slice.call($paper.querySelectorAll(".pi.sel")).forEach(function(n){n.classList.remove("sel");});
    return; }
  Array.prototype.slice.call($paper.querySelectorAll(".pi")).forEach(function(n){
    n.classList.toggle("sel", +n.dataset.id===S.selId); });
  $selBox.classList.remove("hidden");
  $selBox.style.left=mm(it.x)+"px"; $selBox.style.top=mm(it.y)+"px";
  $selBox.style.width=mm(it.w)+"px"; $selBox.style.height=mm(it.h)+"px";
  $selBox.style.transform = it.rot?"rotate("+it.rot+"deg)":"";
}

// ------------------------------------------------------------- toolbar ------
function syncToolbar(){
  document.getElementById("paperSel").value=S.design.paper;
  document.getElementById("orientSel").value=String(S.design.orientation||0);
  document.getElementById("zoomLbl").textContent=faDigits(Math.round(S.scale*100))+"٪";
}

// ------------------------------------------------------------- zoom/pan -----
function setScale(s, anchorClientX, anchorClientY){
  s=Math.max(0.25,Math.min(4,s));
  S.scale=s; renderAll();
}
function fitZoom(){
  var availW=$scroll.clientWidth-60, availH=$scroll.clientHeight-80;
  var d=paperDims(S.design.paper,S.design.orientation);
  var sw=availW/(d[0]*S.pxPerMM), sh=availH/(d[1]*S.pxPerMM);
  S.scale=Math.max(0.25,Math.min(2,Math.min(sw,sh)));
  renderAll();
}

// ----------------------------------------------------------- palette --------
function buildPalette(){
  var list=document.getElementById("paletteList"); list.innerHTML="";
  // generic tools category first
  var tools=[
    {t:"label",ic:"🇹",l:"برچسب / متن"},
    {t:"hline",ic:"—",l:"خط افقی"},
    {t:"vline",ic:"│",l:"خط عمودی"},
    {t:"rect",ic:"▭",l:"کادر / مستطیل"},
    {t:"frame",ic:"⬚",l:"حاشیه صفحه"},
    {t:"logo",ic:"🖼",l:"لوگو درمانگاه"},
    {t:"photo",ic:"👤",l:"عکس بیمار"},
    {t:"qr",ic:"▦",l:"بارکد / QR"},
    {t:"apptno",ic:"#",l:"شماره نوبت"},
  ];
  list.appendChild(catEl("ابزارهای طراحی", tools.map(function(o){
    return palItem(o.l,o.ic,function(){ addItem(o.t); }); })));

  // data-field categories
  (window.AZ_FIELD_CATS||[]).forEach(function(cat){
    var els=cat.items.map(function(f){
      return palItem(f.label,"⬡",function(){ addField(f.key,f.label); }); });
    list.appendChild(catEl(cat.title, els));
  });
}
function catEl(title, children){
  var c=document.createElement("div"); c.className="pal-cat";
  var h=document.createElement("h6"); h.textContent=title; c.appendChild(h);
  var g=document.createElement("div"); g.className="pal-grid";
  children.forEach(function(ch){ g.appendChild(ch); });
  c.appendChild(g); return c;
}
function palItem(label, ic, on){
  var d=document.createElement("div"); d.className="pal-item"; d.title=label;
  d.dataset.search=label.toLowerCase();
  var i=document.createElement("span"); i.className="ic"; i.textContent=ic;
  var s=document.createElement("span"); s.textContent=label;
  d.appendChild(i); d.appendChild(s);
  d.addEventListener("click",on); return d;
}
function filterPalette(q){
  q=(q||"").trim().toLowerCase();
  Array.prototype.slice.call(document.querySelectorAll("#paletteList .pal-item")).forEach(function(el){
    el.style.display = (!q || el.dataset.search.indexOf(q)>=0) ? "" : "none"; });
  Array.prototype.slice.call(document.querySelectorAll("#paletteList .pal-cat")).forEach(function(c){
    var any=Array.prototype.slice.call(c.querySelectorAll(".pal-item")).some(function(e){return e.style.display!=="none";});
    c.style.display=any?"":"none"; });
}

// ------------------------------------------------------------- add items ----
function defaultsFor(type){
  var base={id:genId(),type:type,x:20,y:20,w:50,h:8,rot:0,z:(S.design.items.length+1),
    locked:false,text:"",field:"",prefix:"",suffix:"",font:"Vazirmatn",pt:11,
    bold:false,italic:false,align:0,lineSpacing:1.2,textColor:"#000000",
    fillColor:"#ffffff",fillTransparent:true,borderColor:"#000000",borderWidth:0,
    borderStyle:0,corner:0,padding:1,opacity:1,visibility:0,startValue:1,step:1};
  if(type==="label"){ base.text="متن نمونه"; }
  if(type==="hline"){ base.w=60; base.h=0.4; base.borderWidth=0.4; }
  if(type==="vline"){ base.w=0.4; base.h=40; base.borderWidth=0.4; }
  if(type==="rect"){ base.w=50; base.h=30; base.borderWidth=0.4; }
  if(type==="frame"){ base.x=8;base.y=8;base.isFrame=true;
    base.w=S.design.paperW-16; base.h=S.design.paperH-16; base.borderWidth=0.6; }
  if(type==="logo"){ base.w=22; base.h=22; }
  if(type==="photo"){ base.w=24; base.h=30; base.borderWidth=0.3; }
  if(type==="qr"){ base.w=24; base.h=24; base.field="{receiptNo}"; }
  if(type==="apptno"){ base.w=40; base.h=12; base.pt=22; base.bold=true; base.align=1; }
  return base;
}
function addItem(type){
  pushUndo(); var it=defaultsFor(type);
  S.design.items.push(it); S.selId=it.id; renderAll(); switchTab("inspector"); }
function addField(key,label){
  pushUndo(); var it=defaultsFor("field");
  it.field=key; it.prefix=(label?label+": ":""); it.w=60;
  S.design.items.push(it); S.selId=it.id; renderAll(); switchTab("inspector"); }

// ------------------------------------------------------------- inspector ----
function switchTab(name){
  Array.prototype.slice.call(document.querySelectorAll(".rp-tab")).forEach(function(t){
    t.classList.toggle("active",t.dataset.tab===name); });
  Array.prototype.slice.call(document.querySelectorAll(".rp-pane")).forEach(function(p){
    p.classList.toggle("active",p.id==="pane-"+name); });
  if(name==="inspector") renderInspector();
  else if(name==="layers") renderLayers();
}
function row(label, control){
  var r=document.createElement("div"); r.className="insp-row";
  if(label){ var l=document.createElement("label"); l.textContent=label; r.appendChild(l); }
  r.appendChild(control); return r;
}
function inp(type,val,on){ var i=document.createElement("input"); i.type=type;
  i.className="form-control form-control-sm"; if(val!=null)i.value=val;
  i.addEventListener("input",function(){on(i.value,i);}); return i; }
function numInp(val,on,step){ var i=inp("number",val,function(v){on(parseFloat(v)||0);});
  i.step=step||"0.5"; return i; }
function colorInp(val,on){ return inp("color",val||"#000000",function(v){on(v);}); }
function sel(opts,val,on){ var s=document.createElement("select");
  s.className="form-select form-select-sm";
  opts.forEach(function(o){ var op=document.createElement("option");
    op.value=o[0]; op.textContent=o[1]; if(String(o[0])===String(val))op.selected=true;
    s.appendChild(op); });
  s.addEventListener("change",function(){on(s.value);}); return s; }
function chk(label,val,on){ var w=document.createElement("label"); w.className="row-inline";
  var c=document.createElement("input"); c.type="checkbox"; c.checked=!!val;
  c.addEventListener("change",function(){on(c.checked);});
  var t=document.createElement("span"); t.textContent=label;
  w.appendChild(c); w.appendChild(t); return w; }
function grid2(a,b){ var g=document.createElement("div"); g.className="insp-grid2";
  g.appendChild(a); g.appendChild(b); return g; }
function secEl(title){ var s=document.createElement("div"); s.className="insp-sec";
  var h=document.createElement("h6"); h.textContent=title; s.appendChild(h); return s; }

function renderInspector(){
  var body=document.getElementById("inspectorBody");
  var empty=document.getElementById("inspectorEmpty");
  var it=findItem(S.selId);
  if(!it){ body.classList.add("hidden"); empty.classList.remove("hidden"); return; }
  empty.classList.add("hidden"); body.classList.remove("hidden"); body.innerHTML="";

  function commit(){ S.dirty=true; renderAll(); }

  // header / type
  var head=document.createElement("div"); head.className="insp-row";
  head.innerHTML='<label>نوع آیتم</label><b>'+(ITEM_LABELS[it.type]||it.type)+'</b>';
  body.appendChild(head);

  // text / field binding
  if(it.type==="label"){
    body.appendChild(row("متن", (function(){ var ta=document.createElement("textarea");
      ta.className="form-control form-control-sm"; ta.rows=2; ta.value=it.text||"";
      ta.addEventListener("input",function(){ it.text=ta.value; commit(); }); return ta; })()));
  }
  if(it.type==="field"){
    var opts=[]; (window.AZ_FIELD_CATS||[]).forEach(function(c){ c.items.forEach(function(f){
      opts.push([f.key, c.title+" › "+f.label]); }); });
    body.appendChild(row("داده", sel(opts,it.field,function(v){ it.field=v; commit(); })));
    body.appendChild(row("پیشوند", inp("text",it.prefix,function(v){ it.prefix=v; commit(); })));
    body.appendChild(row("پسوند", inp("text",it.suffix,function(v){ it.suffix=v; commit(); })));
  }
  if(it.type==="apptno"){
    body.appendChild(grid2(
      row("شروع از", numInp(it.startValue,function(v){it.startValue=Math.round(v);commit();},"1")),
      row("گام", numInp(it.step,function(v){it.step=Math.round(v)||1;commit();},"1"))));
  }

  // position & size
  var pos=secEl("موقعیت و اندازه (میلی‌متر)");
  pos.appendChild(grid2(row("X", numInp(round1(it.x),function(v){it.x=v;commit();})),
                         row("Y", numInp(round1(it.y),function(v){it.y=v;commit();}))));
  pos.appendChild(grid2(row("عرض", numInp(round1(it.w),function(v){it.w=Math.max(0.5,v);commit();})),
                         row("ارتفاع", numInp(round1(it.h),function(v){it.h=Math.max(0.2,v);commit();}))));
  pos.appendChild(grid2(row("چرخش°", numInp(round1(it.rot),function(v){it.rot=v;commit();},"1")),
                         row("لایه (Z)", numInp(it.z||0,function(v){it.z=Math.round(v);commit();},"1"))));
  body.appendChild(pos);

  // text styling
  if(it.type==="label"||it.type==="field"||it.type==="apptno"){
    var ts=secEl("متن");
    ts.appendChild(grid2(
      row("فونت", sel([["Vazirmatn","وزیر"],["Tahoma","تهوما"],["Arial","Arial"],["Times New Roman","Times"]],it.font,function(v){it.font=v;commit();})),
      row("اندازه (pt)", numInp(it.pt,function(v){it.pt=Math.max(4,v);commit();},"0.5"))));
    var styRow=document.createElement("div"); styRow.className="insp-grid3";
    styRow.appendChild(chk("توپر",it.bold,function(v){it.bold=v;commit();}));
    styRow.appendChild(chk("مورب",it.italic,function(v){it.italic=v;commit();}));
    styRow.appendChild(sel([[0,"راست"],[1,"وسط"],[2,"چپ"]],it.align,function(v){it.align=+v;commit();}));
    ts.appendChild(row("سبک / چینش",styRow));
    ts.appendChild(grid2(
      row("رنگ متن", colorInp(it.textColor,function(v){it.textColor=v;commit();})),
      row("فاصله خطوط", numInp(it.lineSpacing,function(v){it.lineSpacing=v||1.2;commit();},"0.1"))));
    body.appendChild(ts);
  }

  // fill & border (for boxes/labels)
  if(it.type!=="hline"&&it.type!=="vline"){
    var fb=secEl("پس‌زمینه و کادر");
    fb.appendChild(row("شفاف بودن پس‌زمینه", chk("بدون پس‌زمینه",it.fillTransparent,function(v){it.fillTransparent=v;commit();})));
    fb.appendChild(grid2(
      row("رنگ پس‌زمینه", colorInp(it.fillColor,function(v){it.fillColor=v;commit();})),
      row("گردی گوشه (mm)", numInp(it.corner,function(v){it.corner=Math.max(0,v);commit();},"0.5"))));
    fb.appendChild(grid2(
      row("ضخامت کادر (mm)", numInp(it.borderWidth,function(v){it.borderWidth=Math.max(0,v);commit();},"0.1")),
      row("رنگ کادر", colorInp(it.borderColor,function(v){it.borderColor=v;commit();}))));
    fb.appendChild(row("نوع کادر", sel([[0,"پیوسته"],[1,"چین‌چین"],[2,"نقطه‌چین"],[3,"دوخطه"]],it.borderStyle,function(v){it.borderStyle=+v;commit();})));
    body.appendChild(fb);
  } else {
    var lb=secEl("خط");
    lb.appendChild(grid2(
      row("ضخامت (mm)", numInp(it.borderWidth,function(v){it.borderWidth=Math.max(0.1,v);commit();},"0.1")),
      row("رنگ", colorInp(it.borderColor,function(v){it.borderColor=v;commit();}))));
    lb.appendChild(row("نوع خط", sel([[0,"پیوسته"],[1,"چین‌چین"],[2,"نقطه‌چین"]],it.borderStyle,function(v){it.borderStyle=+v;commit();})));
    body.appendChild(lb);
  }

  // advanced
  var adv=secEl("پیشرفته");
  adv.appendChild(row("شفافیت (٪)", numInp(Math.round((it.opacity==null?1:it.opacity)*100),function(v){it.opacity=Math.max(0,Math.min(1,v/100));commit();},"5")));
  adv.appendChild(row("نمایش", sel([[0,"همیشه"],[1,"فقط وقتی داده خالی نیست"]],it.visibility,function(v){it.visibility=+v;commit();})));
  adv.appendChild(chk("قفل (غیرقابل جابه‌جایی)",it.locked,function(v){it.locked=v;commit();}));
  body.appendChild(adv);

  // actions
  var act=document.createElement("div"); act.className="insp-sec";
  var dup=document.createElement("button"); dup.className="btn btn-sm btn-outline btn-block";
  dup.textContent="تکثیر آیتم"; dup.style.marginBottom="6px";
  dup.addEventListener("click",function(){ pushUndo(); var c=clone(it); c.id=genId(); c.x+=4; c.y+=4; c.z=(it.z||0)+1;
    S.design.items.push(c); S.selId=c.id; renderAll(); renderInspector(); });
  var del=document.createElement("button"); del.className="btn btn-sm btn-del btn-block";
  del.textContent="حذف آیتم";
  del.addEventListener("click",function(){ deleteSelected(); });
  act.appendChild(dup); act.appendChild(del); body.appendChild(act);
}
function round1(v){ return Math.round(v*10)/10; }

function deleteSelected(){ // delete selected
  var it=findItem(S.selId); if(!it) return;
  pushUndo(); S.design.items=S.design.items.filter(function(x){return x.id!==S.selId;});
  S.selId=0; renderAll(); renderInspector(); }

// re-render inspector when selection changes
var _origRenderSel=renderSelection;
renderSelection=function(){ _origRenderSel(); if(document.getElementById("pane-inspector").classList.contains("active")) renderInspector(); };

// ------------------------------------------------------------- layers -------
function renderLayers(){
  var box=document.getElementById("layerList"); if(!box) return; box.innerHTML="";
  var items=(S.design.items||[]).slice().sort(function(a,b){return (b.z||0)-(a.z||0);});
  items.forEach(function(it){
    var d=document.createElement("div"); d.className="layer-item"+(it.id===S.selId?" sel":"");
    var name=document.createElement("span"); name.className="ly-name";
    name.textContent=(ITEM_LABELS[it.type]||it.type)+(it.type==="label"?(" — "+(it.text||"").slice(0,12)):(it.type==="field"?(" — "+(window.AZ_FIELDS[it.field]?window.AZ_FIELDS[it.field].label:it.field)):""));
    d.appendChild(name);
    var up=document.createElement("button"); up.textContent="▲"; up.title="بالا";
    up.addEventListener("click",function(e){e.stopPropagation(); pushUndo(); it.z=(it.z||0)+1; renderAll();});
    var dn=document.createElement("button"); dn.textContent="▼"; dn.title="پایین";
    dn.addEventListener("click",function(e){e.stopPropagation(); pushUndo(); it.z=(it.z||0)-1; renderAll();});
    d.appendChild(up); d.appendChild(dn);
    d.addEventListener("click",function(){ S.selId=it.id; renderAll(); switchTab("inspector"); });
    box.appendChild(d);
  });
}

// ------------------------------------------------------------ templates -----
function buildTemplateStrip(){
  var strip=document.getElementById("templateStrip"); strip.innerHTML="";
  S.templates.forEach(function(t,i){
    var th=document.createElement("div"); th.className="tpl-thumb"; th.title=t.name;
    var cv=document.createElement("canvas"); cv.width=92; cv.height=68; th.appendChild(cv);
    var b=document.createElement("b"); b.textContent=t.name; th.appendChild(b);
    drawThumb(cv,t);
    th.addEventListener("click",function(){ applyTemplate(i); });
    strip.appendChild(th);
  });
}
function drawThumb(cv,t){
  var ctx=cv.getContext("2d"); var d=paperDims(t.paper,t.orientation);
  var pad=4, aw=cv.width-pad*2, ah=cv.height-pad*2;
  var s=Math.min(aw/d[0],ah/d[1]); var ox=(cv.width-d[0]*s)/2, oy=pad;
  ctx.fillStyle="#fff"; ctx.fillRect(ox,oy,d[0]*s,d[1]*s);
  ctx.strokeStyle="#ccc"; ctx.strokeRect(ox,oy,d[0]*s,d[1]*s);
  (t.items||[]).forEach(function(it){
    var x=ox+it.x*s,y=oy+it.y*s,w=it.w*s,h=it.h*s;
    if(it.type==="label"||it.type==="field"||it.type==="apptno"){
      ctx.fillStyle=it.bold?"#1f7ae0":"#555";
      ctx.fillRect(x,y+h*0.3,Math.max(2,w),Math.max(1,h*0.4));
    } else if(it.type==="hline"){ ctx.strokeStyle="#888"; ctx.beginPath(); ctx.moveTo(x,y); ctx.lineTo(x+w,y); ctx.stroke(); }
    else if(it.type==="vline"){ ctx.strokeStyle="#888"; ctx.beginPath(); ctx.moveTo(x,y); ctx.lineTo(x,y+h); ctx.stroke(); }
    else { ctx.strokeStyle="#aaa"; ctx.strokeRect(x,y,w,h); }
  });
}
function applyTemplate(i){
  var t=S.templates[i]; if(!t) return;
  pushUndo();
  var nd=clone(t); nd.id=S.design.id; nd.name=S.design.name||t.name; nd.kind="user";
  // keep section target; assign fresh ids
  var k=1; (nd.items||[]).forEach(function(it){ it.id=k++; });
  S.design=nd; S.selId=0; renderAll(); fitZoom();
  toast("طرح آماده اعمال شد — می‌توانید آن را ویرایش و ذخیره کنید");
}

// ------------------------------------------------------------ interactions --
// click-select + drag-move on items; resize/rotate via handles
var drag=null;
$paper.addEventListener("pointerdown",function(e){
  var piEl=e.target.closest(".pi");
  var handle=e.target.classList.contains("handle")?e.target:null;
  if(handle){
    var it=findItem(S.selId); if(!it||it.locked) return;
    pushUndo();
    drag={mode:"resize",handle:handleDir(handle),it:it,sx:e.clientX,sy:e.clientY,
      ox:it.x,oy:it.y,ow:it.w,oh:it.h,orot:it.rot};
    if(handle.classList.contains("h-rot")) drag.mode="rotate";
    $paper.setPointerCapture(e.pointerId); e.preventDefault(); return;
  }
  if(piEl){
    var id=+piEl.dataset.id; S.selId=id; renderAll(); switchTab("inspector");
    var sel=findItem(id);
    if(sel && !sel.locked){
      pushUndo();
      drag={mode:"move",it:sel,sx:e.clientX,sy:e.clientY,ox:sel.x,oy:sel.y};
      $paper.setPointerCapture(e.pointerId);
    }
    e.preventDefault();
  } else {
    // empty paper click: deselect
    S.selId=0; renderAll();
  }
});
$paper.addEventListener("pointermove",function(e){
  if(!drag) return;
  var dx=(e.clientX-drag.sx)/(S.pxPerMM*S.scale);
  var dy=(e.clientY-drag.sy)/(S.pxPerMM*S.scale);
  var it=drag.it;
  if(drag.mode==="move"){ it.x=snap(drag.ox+dx); it.y=snap(drag.oy+dy); }
  else if(drag.mode==="rotate"){
    var cx=mm(it.x+it.w/2), cy=mm(it.y+it.h/2);
    var rect=$paper.getBoundingClientRect();
    var ang=Math.atan2((e.clientY-rect.top)-cy,(e.clientX-rect.left)-cx)*180/Math.PI+90;
    it.rot=Math.round(ang);
  } else if(drag.mode==="resize"){
    var h=drag.handle;
    if(h.indexOf("e")>=0) it.w=Math.max(1,drag.ow+dx);
    if(h.indexOf("s")>=0) it.h=Math.max(0.5,drag.oh+dy);
    if(h.indexOf("w")>=0){ it.w=Math.max(1,drag.ow-dx); it.x=drag.ox+dx; }
    if(h.indexOf("n")>=0){ it.h=Math.max(0.5,drag.oh-dy); it.y=drag.oy+dy; }
  }
  renderAll();
});
$paper.addEventListener("pointerup",function(e){ if(drag){ drag=null; renderInspector(); } });
function handleDir(el){ var c=el.className; var m=c.match(/h-(\w+)/); return m?m[1]:""; }
function snap(v){ return Math.round(v*2)/2; } // 0.5mm snap

// pan with drag on empty area
var pan=null;
$scroll.addEventListener("pointerdown",function(e){
  if(e.target===$scroll || e.target===$paper){
    if(e.target===$paper){ /* handled above for items; empty paper still pans */ }
    pan={sx:e.clientX,sy:e.clientY,sl:$scroll.scrollLeft,st:$scroll.scrollTop};
    $scroll.classList.add("panning"); $scroll.setPointerCapture(e.pointerId);
  }
});
$scroll.addEventListener("pointermove",function(e){
  if(!pan) return;
  $scroll.scrollLeft=pan.sl-(e.clientX-pan.sx);
  $scroll.scrollTop=pan.st-(e.clientY-pan.sy);
});
$scroll.addEventListener("pointerup",function(){ pan=null; $scroll.classList.remove("panning"); });

// ctrl+wheel = zoom, wheel = scroll (native)
$scroll.addEventListener("wheel",function(e){
  if(e.ctrlKey){ e.preventDefault();
    var f=e.deltaY<0?1.1:0.9; setScale(S.scale*f); }
},{passive:false});

// ------------------------------------------------------------ keyboard ------
document.addEventListener("keydown",function(e){
  var tag=(e.target.tagName||"").toLowerCase();
  var typing=(tag==="input"||tag==="textarea"||tag==="select");
  if((e.ctrlKey||e.metaKey)&&e.key.toLowerCase()==="z"){ e.preventDefault(); doUndo(); return; }
  if((e.ctrlKey||e.metaKey)&&(e.key.toLowerCase()==="y"||(e.shiftKey&&e.key.toLowerCase()==="z"))){ e.preventDefault(); doRedo(); return; }
  if((e.ctrlKey||e.metaKey)&&e.key.toLowerCase()==="s"){ e.preventDefault(); saveDesign(); return; }
  if(typing) return;
  var it=findItem(S.selId);
  if((e.key==="Delete"||e.key==="Backspace")&&it){ e.preventDefault(); deleteSelected(); return; }
  if(it && !it.locked){
    var step=e.shiftKey?5:0.5;
    if(e.key==="ArrowLeft"){ e.preventDefault(); pushUndo(); it.x-=step; renderAll(); }
    if(e.key==="ArrowRight"){ e.preventDefault(); pushUndo(); it.x+=step; renderAll(); }
    if(e.key==="ArrowUp"){ e.preventDefault(); pushUndo(); it.y-=step; renderAll(); }
    if(e.key==="ArrowDown"){ e.preventDefault(); pushUndo(); it.y+=step; renderAll(); }
  }
});

// ------------------------------------------------------------ save/load -----
function designToJsonStr(){ return JSON.stringify(S.design); }

function saveDesign(){
  // ask for name if needed
  var name=S.design.name;
  if(!name || S.design.kind==="builtin"){
    name=prompt("نام طرح را وارد کنید:", (name||"طرح جدید"));
    if(name===null) return; S.design.name=name; S.design.kind="user";
  }
  if(Bridge.has()){
    var res=Bridge.request("save",{design:S.design});
    if(res && res.ok){ if(res.id) S.design.id=res.id; S.dirty=false;
      toast("ذخیره و بر بخش اعمال شد"); }
    else toast("خطا در ذخیره");
  } else {
    try{ localStorage.setItem("az_design_"+(S.design.id||"new"), designToJsonStr()); }catch(_){}
    S.dirty=false; toast("ذخیره شد (حالت آزمایشی مرورگر)");
  }
}
function downloadDesign(){
  var data=designToJsonStr();
  if(Bridge.has()){ Bridge.request("download",{design:S.design,name:S.design.name}); toast("فایل طرح ذخیره شد"); return; }
  var blob=new Blob([data],{type:"application/json"});
  var a=document.createElement("a"); a.href=URL.createObjectURL(blob);
  a.download=(S.design.name||"design")+".aztpl"; a.click();
  setTimeout(function(){URL.revokeObjectURL(a.href);},500);
}
function uploadDesign(){
  if(Bridge.has()){
    var res=Bridge.request("upload",{});
    if(res && res.design){ pushUndo(); S.design=res.design; S.selId=0; renderAll(); fitZoom(); toast("طرح بارگذاری شد"); }
    return;
  }
  document.getElementById("fileInput").click();
}
document.getElementById("fileInput").addEventListener("change",function(e){
  var f=e.target.files[0]; if(!f) return;
  var r=new FileReader();
  r.onload=function(){ try{ var d=JSON.parse(r.result); pushUndo(); S.design=d; S.selId=0;
    renderAll(); fitZoom(); toast("طرح بارگذاری شد"); }catch(_){ toast("فایل نامعتبر"); } };
  r.readAsText(f); e.target.value="";
});

// ------------------------------------------------------------ wire ui -------
function wire(){
  document.getElementById("paperSel").addEventListener("change",function(){ pushUndo(); S.design.paper=this.value; renderAll(); fitZoom(); });
  document.getElementById("orientSel").addEventListener("change",function(){ pushUndo(); S.design.orientation=+this.value; renderAll(); fitZoom(); });
  document.getElementById("btnUndo").addEventListener("click",doUndo);
  document.getElementById("btnRedo").addEventListener("click",doRedo);
  document.getElementById("btnZoomIn").addEventListener("click",function(){ setScale(S.scale*1.15); });
  document.getElementById("btnZoomOut").addEventListener("click",function(){ setScale(S.scale/1.15); });
  document.getElementById("btnZoomFit").addEventListener("click",fitZoom);
  document.getElementById("btnSave").addEventListener("click",saveDesign);
  document.getElementById("btnDownload").addEventListener("click",downloadDesign);
  document.getElementById("btnUpload").addEventListener("click",uploadDesign);
  document.getElementById("btnExit").addEventListener("click",function(){
    if(S.dirty && !confirm("تغییرات ذخیره‌نشده دارید. خارج می‌شوید؟")) return;
    if(Bridge.has()) Bridge.request("exit",{}); else toast("خروج (حالت مرورگر)");
  });
  Array.prototype.slice.call(document.querySelectorAll(".rp-tab")).forEach(function(t){
    t.addEventListener("click",function(){ switchTab(t.dataset.tab); if(t.dataset.tab==="inspector")renderInspector(); if(t.dataset.tab==="layers")renderLayers(); }); });
  document.getElementById("paletteSearch").addEventListener("input",function(){ filterPalette(this.value); });
}

// ------------------------------------------------------------ init ----------
function loadInitial(){
  var initial=null, secName="";
  if(Bridge.has()){
    var res=Bridge.request("init",{});
    if(res){ if(res.design) initial=res.design; if(res.sectionName) secName=res.sectionName;
      if(res.templates && res.templates.length) S.templates=res.templates; }
  }
  if(!initial){ initial = clone(S.templates[0]); var k=1; initial.items.forEach(function(it){it.id=k++;}); }
  S.design=initial;
  if(!S.design.paper) S.design.paper="A5";
  document.getElementById("secName").textContent = secName?("— "+secName):"";
}

// C++ may push messages to JS via window.azReceive(verb,jsonStr)
window.azReceive=function(verb,jsonStr){
  try{
    var data=jsonStr?JSON.parse(jsonStr):{};
    if(verb==="loadDesign" && data.design){ S.design=data.design; S.selId=0; S.undo.length=0; S.redo.length=0; renderAll(); fitZoom(); }
    if(verb==="setSectionName"){ document.getElementById("secName").textContent=data.name?("— "+data.name):""; }
  }catch(e){ console.warn("azReceive",e); }
};

function boot(){
  buildPalette(); wire();
  loadInitial(); buildTemplateStrip(); renderAll(); fitZoom();
  updateUndoButtons();
  // tell host we're ready
  if(Bridge.has()) Bridge.request("ready",{});
}
if(document.readyState==="loading") document.addEventListener("DOMContentLoaded",boot);
else boot();

// expose for debugging / host
window.AZDesigner={ S:S, render:renderAll, save:saveDesign };
})();
