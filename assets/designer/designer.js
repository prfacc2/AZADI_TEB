/* ===========================================================================
   designer.js — Azadi-Teb professional print designer engine (v1.20.0)
   Dependency-free, RTL-aware WYSIWYG editor. Talks to C++ over the loopback
   HTTP host (/api/*) using ASYNCHRONOUS XHR (no sync XHR, no sync-timeout —
   fixes the deprecated-sync console warnings AND the "save error" bug where a
   synchronous request with a timeout attribute threw in some browsers).
   =========================================================================== */
(function () {
  "use strict";

  /* ------------------------------------------------------------- bridge --- */
  // Async request to the loopback host. cb(resultObjOrNull). Falls back to
  // standalone (no host) when not served over http(s).
  var Bridge = {
    _on: (location.protocol === "http:" || location.protocol === "https:"),
    has: function () { return this._on; },
    request: function (verb, args, cb) {
      cb = cb || function () {};
      if (!this._on) { cb(null); return; }
      try {
        var xhr = new XMLHttpRequest();
        xhr.open("POST", "api/" + verb, true);   // ASYNC — no UI block, no warnings
        xhr.setRequestHeader("Content-Type", "application/json;charset=utf-8");
        xhr.onreadystatechange = function () {
          if (xhr.readyState !== 4) return;
          if (xhr.status >= 200 && xhr.status < 300) {
            var r = null;
            try { r = xhr.responseText ? JSON.parse(xhr.responseText) : {}; } catch (e) { r = null; }
            cb(r);
          } else { cb(null); }
        };
        xhr.onerror = function () { cb(null); };
        xhr.send(JSON.stringify(args || {}));
      } catch (e) { cb(null); }
    }
  };

  /* --------------------------------------------------------- paper data --- */
  var PAPER = {
    A4: [210, 297], A5: [148, 210], A6: [105, 148], B5: [176, 250],
    Letter: [215.9, 279.4], R80: [80, 200], R58: [58, 200]
  };
  function paperDims(p, orient) {
    var d = PAPER[p] || [148, 210], w = d[0], h = d[1];
    if (orient === 1) { var t = w; w = h; h = t; }
    return [w, h];
  }

  /* ---------------------------------------------------------- app state --- */
  var S = {
    design: null, selId: 0,
    pxPerMM: 3.7795, scale: 1,
    panX: 0, panY: 0,
    undo: [], redo: [], dirty: false,
    templates: window.AZ_TEMPLATES || []
  };

  var $paper, $scroll, $selBox, $stage;

  /* ------------------------------------------------------------ helpers --- */
  function faDigits(s) {
    s = String(s); var f = "۰۱۲۳۴۵۶۷۸۹";
    return s.replace(/[0-9]/g, function (d) { return f[+d]; });
  }
  function clone(o) { return JSON.parse(JSON.stringify(o)); }
  function genId() { var m = 0; (S.design.items || []).forEach(function (it) { if (it.id > m) m = it.id; }); return m + 1; }
  function findItem(id) { return (S.design.items || []).find(function (it) { return it.id === id; }); }
  function selItem() { return S.selId ? findItem(S.selId) : null; }
  function toast(msg, isErr) {
    var t = document.getElementById("toast"); if (!t) return;
    t.textContent = msg;
    t.className = "toast-msg show" + (isErr ? " err" : "");
    clearTimeout(toast._t);
    toast._t = setTimeout(function () { t.className = "toast-msg"; }, 2200);
  }

  /* --------------------------------------------------------- undo stack --- */
  function pushUndo() {
    S.undo.push(clone(S.design)); if (S.undo.length > 80) S.undo.shift();
    S.redo.length = 0; S.dirty = true; updateUndoButtons();
  }
  function doUndo() {
    if (!S.undo.length) return;
    S.redo.push(clone(S.design)); S.design = S.undo.pop(); S.selId = 0;
    renderAll(); updateUndoButtons(); toast("بازگشت");
  }
  function doRedo() {
    if (!S.redo.length) return;
    S.undo.push(clone(S.design)); S.design = S.redo.pop(); S.selId = 0;
    renderAll(); updateUndoButtons(); toast("جلو");
  }
  function updateUndoButtons() {
    var u = document.getElementById("btnUndo"), r = document.getElementById("btnRedo");
    if (u) u.disabled = !S.undo.length;
    if (r) r.disabled = !S.redo.length;
  }

  /* --------------------------------------------------------- rendering ---- */
  var ITEM_LABELS = {
    label: "متن ثابت", field: "فیلد داده", hline: "خط افقی", vline: "خط عمودی",
    rect: "کادر", frame: "حاشیه صفحه", logo: "لوگو", photo: "عکس بیمار",
    qr: "بارکد / QR", apptno: "شماره نوبت", image: "تصویر"
  };
  var ITEM_ICONS = {
    label: "T", field: "{}", hline: "—", vline: "│", rect: "▭", frame: "⬚",
    logo: "★", photo: "👤", qr: "▦", apptno: "#", image: "🖼"
  };

  function mm(v) { return v * S.pxPerMM * S.scale; }

  function styleItem(el, it) {
    el.style.left = mm(it.x) + "px";
    el.style.top = mm(it.y) + "px";
    el.style.width = mm(it.w) + "px";
    el.style.height = mm(it.h) + "px";
    el.style.transform = it.rot ? "rotate(" + it.rot + "deg)" : "";
    el.style.opacity = (it.opacity == null ? 1 : it.opacity);
    el.style.zIndex = it.z || 0;
  }

  // What text to SHOW on the canvas. Fields show their Persian label inside a
  // soft pill (NOT a fake sample value — the user asked for no examples). The
  // real patient data is substituted only at print time in C++.
  function displayText(it) {
    if (it.type === "label") return it.text || "";
    if (it.type === "apptno") return faDigits(String(it.startValue || 1));
    if (it.type === "field") {
      var f = window.AZ_FIELDS[it.field];
      var lbl = f ? f.label : (it.field || "فیلد");
      return (it.prefix || "") + "［" + lbl + "］" + (it.suffix || "");
    }
    return it.text || "";
  }

  function buildItemEl(it) {
    var el = document.createElement("div");
    el.className = "pi pi-" + it.type + (it.id === S.selId ? " sel" : "");
    el.dataset.id = it.id;
    styleItem(el, it);

    if (it.type === "label" || it.type === "field" || it.type === "apptno") {
      var t = document.createElement("div");
      t.className = "pi-text" + (it.type === "field" ? " pi-fieldtext" : "");
      t.textContent = displayText(it);
      t.style.color = it.textColor || "#000";
      t.style.fontSize = mm((it.pt || 10) * 0.3528) + "px";
      t.style.fontWeight = it.bold ? "700" : "400";
      t.style.fontStyle = it.italic ? "italic" : "normal";
      t.style.fontFamily = it.font || "Vazirmatn";
      t.style.textAlign = it.align === 1 ? "center" : (it.align === 2 ? "left" : "right");
      t.style.lineHeight = (it.lineSpacing && it.lineSpacing > 0) ? it.lineSpacing : 1.25;
      el.appendChild(t);
    } else if (it.type === "hline") {
      el.style.height = Math.max(1, mm((it.borderWidth || 1) * 0.3528)) + "px";
      el.style.background = it.borderColor || "#222";
    } else if (it.type === "vline") {
      el.style.width = Math.max(1, mm((it.borderWidth || 1) * 0.3528)) + "px";
      el.style.background = it.borderColor || "#222";
    } else if (it.type === "rect" || it.type === "frame") {
      el.style.border = Math.max(1, (it.borderWidth || 1)) + "px solid " + (it.borderColor || "#222");
      el.style.borderRadius = mm(it.corner || 0) + "px";
      if (!it.fillTransparent && it.fillColor) el.style.background = it.fillColor;
    } else if (it.type === "logo" || it.type === "photo" || it.type === "image" || it.type === "qr") {
      el.style.border = "1px dashed #9aa7c2";
      el.style.borderRadius = mm(it.corner || 0) + "px";
      if (it.imgPath && /^data:/.test(it.imgPath)) {
        var img = document.createElement("img");
        img.src = it.imgPath; img.className = "pi-img";
        el.appendChild(img);
      } else {
        var ph = document.createElement("div");
        ph.className = "pi-ph";
        ph.textContent = ITEM_ICONS[it.type] + " " + (ITEM_LABELS[it.type] || it.type);
        el.appendChild(ph);
      }
    }
    return el;
  }

  function renderAll() {
    if (!S.design) return;
    var dims = paperDims(S.design.paper, S.design.orientation);
    S.design.paperW = dims[0]; S.design.paperH = dims[1];

    $paper.style.width = mm(dims[0]) + "px";
    $paper.style.height = mm(dims[1]) + "px";

    // wipe items (keep selBox)
    Array.prototype.slice.call($paper.querySelectorAll(".pi")).forEach(function (e) { e.remove(); });

    var items = (S.design.items || []).slice().sort(function (a, b) { return (a.z || 0) - (b.z || 0); });
    items.forEach(function (it) { $paper.appendChild(buildItemEl(it)); });

    updateSelBox();
    var pl = document.getElementById("paperLbl");
    if (pl) pl.textContent = S.design.paper + " · " + faDigits(Math.round(dims[0])) + "×" + faDigits(Math.round(dims[1])) + " mm";
  }

  function updateSelBox() {
    var it = selItem();
    if (!it) { $selBox.classList.add("hidden"); return; }
    $selBox.classList.remove("hidden");
    $selBox.style.left = mm(it.x) + "px";
    $selBox.style.top = mm(it.y) + "px";
    $selBox.style.width = mm(it.w) + "px";
    $selBox.style.height = mm(it.h) + "px";
    $selBox.style.transform = it.rot ? "rotate(" + it.rot + "deg)" : "";
  }

  function select(id) {
    S.selId = id;
    Array.prototype.slice.call($paper.querySelectorAll(".pi")).forEach(function (e) {
      e.classList.toggle("sel", +e.dataset.id === id);
    });
    updateSelBox();
    renderInspector(); renderLayers();
    if (id) switchTab("inspector");
  }

  /* -------------------------------------------------------- new items ----- */
  function defaultItem(type) {
    var it = {
      id: genId(), type: type, x: 10, y: 10, w: 40, h: 8, rot: 0, z: (S.design.items || []).length + 1,
      locked: false, isFrame: false, text: "", field: "", prefix: "", suffix: "",
      font: "Vazirmatn", pt: 11, bold: false, italic: false, align: 0, lineSpacing: 1.25,
      textColor: "#111111", fillColor: "#ffffff", fillTransparent: true,
      borderColor: "#333333", borderWidth: 1, corner: 0, padding: 1, opacity: 1,
      visibility: 0, startValue: 1, step: 1, imgPath: ""
    };
    if (type === "label") { it.text = "متن"; it.w = 40; it.h = 8; }
    else if (type === "field") { it.w = 50; it.h = 8; }
    else if (type === "hline") { it.w = 80; it.h = 1; it.borderWidth = 1; }
    else if (type === "vline") { it.w = 1; it.h = 40; it.borderWidth = 1; }
    else if (type === "rect") { it.w = 50; it.h = 25; }
    else if (type === "frame") {
      var dm = paperDims(S.design.paper, S.design.orientation);
      it.x = 4; it.y = 4; it.w = dm[0] - 8; it.h = dm[1] - 8; it.isFrame = true; it.borderWidth = 1.4;
    }
    else if (type === "logo") { it.w = 28; it.h = 28; }
    else if (type === "photo") { it.w = 25; it.h = 32; }
    else if (type === "image") { it.w = 40; it.h = 25; }
    else if (type === "qr") { it.w = 24; it.h = 24; }
    else if (type === "apptno") { it.w = 30; it.h = 14; it.pt = 22; it.bold = true; it.align = 1; }
    return it;
  }

  function addItem(type, field) {
    pushUndo();
    var it = defaultItem(type);
    if (type === "field" && field) {
      it.field = field;
      var dm = paperDims(S.design.paper, S.design.orientation);
      it.x = Math.round((dm[0] - it.w) / 2);
      it.y = 20 + ((S.design.items.length * 3) % 60);
    }
    S.design.items.push(it);
    renderAll(); select(it.id);
    toast("افزوده شد: " + (ITEM_LABELS[type] || type));
  }

  function deleteItem(id) {
    var i = S.design.items.findIndex(function (x) { return x.id === id; });
    if (i < 0) return;
    pushUndo(); S.design.items.splice(i, 1);
    if (S.selId === id) S.selId = 0;
    renderAll(); renderInspector(); renderLayers();
  }
  function duplicateItem(id) {
    var it = findItem(id); if (!it) return;
    pushUndo(); var c = clone(it); c.id = genId(); c.x += 4; c.y += 4; c.z = S.design.items.length + 1;
    S.design.items.push(c); renderAll(); select(c.id);
  }

  /* ------------------------------------------------------------ palette --- */
  function buildPalette() {
    var host = document.getElementById("paletteList");
    if (!host) return; host.innerHTML = "";

    // 1) shapes / objects
    var objs = [
      ["label", "متن ثابت"], ["hline", "خط افقی"], ["vline", "خط عمودی"],
      ["rect", "کادر"], ["frame", "حاشیه صفحه"], ["logo", "لوگو"],
      ["photo", "عکس بیمار"], ["image", "تصویر"], ["qr", "بارکد / QR"], ["apptno", "شماره نوبت"]
    ];
    var sec = document.createElement("div"); sec.className = "pl-cat";
    sec.innerHTML = "<div class='pl-cat-h'>عناصر طراحی</div>";
    var grid = document.createElement("div"); grid.className = "pl-grid";
    objs.forEach(function (o) {
      var b = document.createElement("button");
      b.className = "pl-tile"; b.dataset.kind = "obj"; b.dataset.type = o[0];
      b.innerHTML = "<span class='pl-ic'>" + (ITEM_ICONS[o[0]] || "•") + "</span><span class='pl-lb'>" + o[1] + "</span>";
      b.title = o[1];
      b.addEventListener("click", function () { addItem(o[0]); });
      grid.appendChild(b);
    });
    sec.appendChild(grid); host.appendChild(sec);

    // 2) bindable fields (grouped) — these are BLUE tiles
    (window.AZ_FIELD_CATS || []).forEach(function (cat) {
      var c = document.createElement("div"); c.className = "pl-cat";
      c.innerHTML = "<div class='pl-cat-h'>" + cat.title + "</div>";
      var g = document.createElement("div"); g.className = "pl-fields";
      cat.items.forEach(function (f) {
        var b = document.createElement("button");
        b.className = "pl-field"; b.dataset.kind = "field"; b.dataset.label = f.label;
        b.textContent = f.label;
        b.title = "افزودن فیلد: " + f.label;
        b.addEventListener("click", function () { addItem("field", f.key); });
        g.appendChild(b);
      });
      c.appendChild(g); host.appendChild(c);
    });
  }

  function filterPalette(q) {
    q = (q || "").trim();
    Array.prototype.slice.call(document.querySelectorAll("#paletteList .pl-field,#paletteList .pl-tile")).forEach(function (b) {
      var txt = (b.dataset.label || b.querySelector(".pl-lb") && b.querySelector(".pl-lb").textContent || "");
      b.style.display = (!q || txt.indexOf(q) >= 0) ? "" : "none";
    });
  }

  /* ---------------------------------------------------------- inspector --- */
  function row(label, ctrl) {
    var r = document.createElement("div"); r.className = "insp-row";
    var l = document.createElement("label"); l.textContent = label;
    r.appendChild(l); r.appendChild(ctrl); return r;
  }
  function numInput(val, min, max, step, onCh) {
    var i = document.createElement("input"); i.type = "number";
    i.className = "form-control form-control-sm"; i.value = (val == null ? 0 : val);
    if (min != null) i.min = min; if (max != null) i.max = max; if (step != null) i.step = step;
    i.addEventListener("change", function () { onCh(parseFloat(i.value) || 0); });
    return i;
  }
  function textInput(val, onCh, ph) {
    var i = document.createElement("input"); i.type = "text";
    i.className = "form-control form-control-sm"; i.value = val || ""; if (ph) i.placeholder = ph;
    i.addEventListener("input", function () { onCh(i.value); });
    return i;
  }
  function colorInput(val, onCh) {
    var i = document.createElement("input"); i.type = "color";
    i.className = "form-color"; i.value = val || "#000000";
    i.addEventListener("input", function () { onCh(i.value); });
    return i;
  }
  function checkInput(val, onCh) {
    var i = document.createElement("input"); i.type = "checkbox"; i.className = "form-check";
    i.checked = !!val; i.addEventListener("change", function () { onCh(i.checked); });
    return i;
  }
  function selectInput(opts, val, onCh) {
    var s = document.createElement("select"); s.className = "form-select form-select-sm";
    opts.forEach(function (o) {
      var op = document.createElement("option"); op.value = o[0]; op.textContent = o[1];
      if (String(o[0]) === String(val)) op.selected = true; s.appendChild(op);
    });
    s.addEventListener("change", function () { onCh(s.value); });
    return s;
  }

  function renderInspector() {
    var empty = document.getElementById("inspectorEmpty");
    var body = document.getElementById("inspectorBody");
    var it = selItem();
    if (!it) { empty.classList.remove("hidden"); body.classList.add("hidden"); body.innerHTML = ""; return; }
    empty.classList.add("hidden"); body.classList.remove("hidden"); body.innerHTML = "";

    function up(noUndo) { if (!noUndo) pushUndo(); renderAll(); updateSelBox(); }
    function grp(title) { var g = document.createElement("div"); g.className = "insp-grp"; g.innerHTML = "<div class='insp-grp-h'>" + title + "</div>"; body.appendChild(g); return g; }

    // header
    var hd = document.createElement("div"); hd.className = "insp-head";
    hd.innerHTML = "<span class='insp-type'>" + (ITEM_LABELS[it.type] || it.type) + "</span>";
    var del = document.createElement("button"); del.className = "btn btn-sm btn-danger"; del.textContent = "حذف";
    del.addEventListener("click", function () { deleteItem(it.id); });
    var dup = document.createElement("button"); dup.className = "btn btn-sm btn-outline"; dup.textContent = "تکثیر";
    dup.addEventListener("click", function () { duplicateItem(it.id); });
    hd.appendChild(dup); hd.appendChild(del);
    body.appendChild(hd);

    // content
    if (it.type === "label") {
      var gc = grp("متن");
      gc.appendChild(row("متن", textInput(it.text, function (v) { it.text = v; up(true); }, "متن دلخواه")));
    }
    if (it.type === "field") {
      var gf = grp("فیلد داده");
      var opts = [];
      (window.AZ_FIELD_CATS || []).forEach(function (c) { c.items.forEach(function (f) { opts.push([f.key, c.title + " › " + f.label]); }); });
      gf.appendChild(row("نوع داده", selectInput(opts, it.field, function (v) { it.field = v; up(); })));
      gf.appendChild(row("پیشوند", textInput(it.prefix, function (v) { it.prefix = v; up(true); }, "مثلاً: نام بیمار: ")));
      gf.appendChild(row("پسوند", textInput(it.suffix, function (v) { it.suffix = v; up(true); }, "")));
      gf.appendChild(row("فقط وقتی پر است", checkInput(it.visibility === 1, function (v) { it.visibility = v ? 1 : 0; up(); })));
    }
    if (it.type === "apptno") {
      var ga = grp("شمارنده نوبت");
      ga.appendChild(row("مقدار شروع", numInput(it.startValue, 0, 99999, 1, function (v) { it.startValue = v; up(); })));
      ga.appendChild(row("گام", numInput(it.step, 1, 100, 1, function (v) { it.step = v; up(); })));
      ga.appendChild(row("پیشوند", textInput(it.prefix, function (v) { it.prefix = v; up(true); }, "نوبت ")));
    }
    if (it.type === "logo" || it.type === "photo" || it.type === "image") {
      var gi = grp("تصویر");
      var up2 = document.createElement("button"); up2.className = "btn btn-sm btn-primary"; up2.style.width = "100%";
      up2.textContent = it.imgPath ? "تغییر تصویر…" : "بارگذاری تصویر…";
      up2.addEventListener("click", function () { pickImageFor(it); });
      gi.appendChild(up2);
      if (it.imgPath) {
        var clr = document.createElement("button"); clr.className = "btn btn-sm btn-outline"; clr.style.width = "100%"; clr.style.marginTop = "6px";
        clr.textContent = "حذف تصویر";
        clr.addEventListener("click", function () { pushUndo(); it.imgPath = ""; up(true); renderInspector(); });
        gi.appendChild(clr);
      }
    }

    // typography (text-bearing)
    if (it.type === "label" || it.type === "field" || it.type === "apptno") {
      var gt = grp("قلم و متن");
      gt.appendChild(row("فونت", selectInput([["Vazirmatn", "وزیر"], ["Tahoma", "تاهوما"], ["IRANSans", "ایران‌سنس"]], it.font, function (v) { it.font = v; up(); })));
      gt.appendChild(row("اندازه (pt)", numInput(it.pt, 5, 96, 0.5, function (v) { it.pt = v; up(); })));
      gt.appendChild(row("ضخیم", checkInput(it.bold, function (v) { it.bold = v; up(); })));
      gt.appendChild(row("کج", checkInput(it.italic, function (v) { it.italic = v; up(); })));
      gt.appendChild(row("چینش", selectInput([["0", "راست"], ["1", "وسط"], ["2", "چپ"]], it.align, function (v) { it.align = +v; up(); })));
      gt.appendChild(row("رنگ متن", colorInput(it.textColor, function (v) { it.textColor = v; up(); })));
      gt.appendChild(row("فاصله خطوط", numInput(it.lineSpacing, 1, 3, 0.05, function (v) { it.lineSpacing = v; up(); })));
    }

    // box / line styling
    if (it.type === "rect" || it.type === "frame" || it.type === "hline" || it.type === "vline" || it.type === "qr" || it.type === "logo" || it.type === "photo" || it.type === "image") {
      var gb = grp("کادر و خط");
      gb.appendChild(row("رنگ خط", colorInput(it.borderColor, function (v) { it.borderColor = v; up(); })));
      gb.appendChild(row("ضخامت خط", numInput(it.borderWidth, 0, 10, 0.2, function (v) { it.borderWidth = v; up(); })));
      if (it.type === "rect" || it.type === "frame") {
        gb.appendChild(row("گردی گوشه", numInput(it.corner, 0, 30, 0.5, function (v) { it.corner = v; up(); })));
        gb.appendChild(row("بدون پُرکننده", checkInput(it.fillTransparent, function (v) { it.fillTransparent = v; up(); })));
        gb.appendChild(row("رنگ پُرکننده", colorInput(it.fillColor, function (v) { it.fillColor = v; up(); })));
      }
    }

    // geometry
    var gg = grp("اندازه و موقعیت (mm)");
    gg.appendChild(row("X", numInput(it.x, 0, 1000, 0.5, function (v) { it.x = v; up(); })));
    gg.appendChild(row("Y", numInput(it.y, 0, 1000, 0.5, function (v) { it.y = v; up(); })));
    gg.appendChild(row("عرض", numInput(it.w, 1, 1000, 0.5, function (v) { it.w = v; up(); })));
    gg.appendChild(row("ارتفاع", numInput(it.h, 1, 1000, 0.5, function (v) { it.h = v; up(); })));
    gg.appendChild(row("چرخش°", numInput(it.rot, -180, 180, 1, function (v) { it.rot = v; up(); })));
    gg.appendChild(row("شفافیت", numInput(it.opacity, 0, 1, 0.05, function (v) { it.opacity = v; up(); })));
  }

  /* ------------------------------------------------------------- layers --- */
  function renderLayers() {
    var host = document.getElementById("layerList"); if (!host) return; host.innerHTML = "";
    var items = (S.design.items || []).slice().sort(function (a, b) { return (b.z || 0) - (a.z || 0); });
    items.forEach(function (it) {
      var r = document.createElement("div");
      r.className = "lyr" + (it.id === S.selId ? " sel" : "");
      var nm = it.type === "label" ? (it.text || "متن") :
        it.type === "field" ? (window.AZ_FIELDS[it.field] ? window.AZ_FIELDS[it.field].label : "فیلد") :
          (ITEM_LABELS[it.type] || it.type);
      r.innerHTML = "<span class='lyr-ic'>" + (ITEM_ICONS[it.type] || "•") + "</span><span class='lyr-nm'>" + nm + "</span>";
      var up = document.createElement("button"); up.className = "lyr-b"; up.textContent = "▲"; up.title = "بالا";
      up.addEventListener("click", function (e) { e.stopPropagation(); pushUndo(); it.z = (it.z || 0) + 1; renderAll(); renderLayers(); });
      var dn = document.createElement("button"); dn.className = "lyr-b"; dn.textContent = "▼"; dn.title = "پایین";
      dn.addEventListener("click", function (e) { e.stopPropagation(); pushUndo(); it.z = Math.max(0, (it.z || 0) - 1); renderAll(); renderLayers(); });
      var del = document.createElement("button"); del.className = "lyr-b del"; del.textContent = "✕";
      del.addEventListener("click", function (e) { e.stopPropagation(); deleteItem(it.id); });
      r.appendChild(up); r.appendChild(dn); r.appendChild(del);
      r.addEventListener("click", function () { select(it.id); });
      host.appendChild(r);
    });
  }

  /* ----------------------------------------------------------- tabs ------- */
  function switchTab(name) {
    Array.prototype.slice.call(document.querySelectorAll(".rp-tab")).forEach(function (t) {
      t.classList.toggle("active", t.dataset.tab === name);
    });
    Array.prototype.slice.call(document.querySelectorAll(".rp-pane")).forEach(function (p) {
      p.classList.toggle("active", p.id === "pane-" + name);
    });
  }

  /* ------------------------------------------------ zoom / pan / fit ------ */
  function setScale(s) {
    S.scale = Math.max(0.2, Math.min(4, s));
    var z = document.getElementById("zoomLbl"); if (z) z.textContent = faDigits(Math.round(S.scale * 100)) + "٪";
    renderAll();
  }
  function fitZoom() {
    var dm = paperDims(S.design.paper, S.design.orientation);
    var availW = $scroll.clientWidth - 60, availH = $scroll.clientHeight - 60;
    var sw = availW / (dm[0] * S.pxPerMM), sh = availH / (dm[1] * S.pxPerMM);
    setScale(Math.max(0.25, Math.min(sw, sh)));
    centerPaper();
  }
  function centerPaper() {
    // center the paper within the scroll viewport
    $scroll.scrollLeft = ($stage.clientWidth - $scroll.clientWidth) / 2;
    $scroll.scrollTop = 40;
  }

  /* --------------------------------------------- canvas interactions ------ */
  function clientToMM(clientX, clientY) {
    var r = $paper.getBoundingClientRect();
    return { x: (clientX - r.left) / (S.pxPerMM * S.scale), y: (clientY - r.top) / (S.pxPerMM * S.scale) };
  }

  function wireCanvas() {
    // click empty space -> deselect ; drag empty space -> PAN the page
    var panning = false, panSX = 0, panSY = 0, scL = 0, scT = 0, moved = false;

    $scroll.addEventListener("mousedown", function (e) {
      var pi = e.target.closest && e.target.closest(".pi");
      var handle = e.target.closest && e.target.closest(".handle");
      if (pi || handle) return;     // item / handle drag handled elsewhere
      panning = true; moved = false;
      panSX = e.clientX; panSY = e.clientY; scL = $scroll.scrollLeft; scT = $scroll.scrollTop;
      $scroll.classList.add("panning");
      e.preventDefault();
    });
    window.addEventListener("mousemove", function (e) {
      if (!panning) return;
      var dx = e.clientX - panSX, dy = e.clientY - panSY;
      if (Math.abs(dx) + Math.abs(dy) > 3) moved = true;
      $scroll.scrollLeft = scL - dx; $scroll.scrollTop = scT - dy;
    });
    window.addEventListener("mouseup", function () {
      if (panning) { panning = false; $scroll.classList.remove("panning"); if (!moved) select(0); }
    });

    // wheel = scroll ; ctrl+wheel = zoom
    $scroll.addEventListener("wheel", function (e) {
      if (e.ctrlKey) { e.preventDefault(); setScale(S.scale * (e.deltaY < 0 ? 1.1 : 0.9)); }
    }, { passive: false });

    // item drag (move) + resize + rotate
    var drag = null;
    $paper.addEventListener("mousedown", function (e) {
      var handle = e.target.closest(".handle");
      var piEl = e.target.closest(".pi");
      if (handle) {
        var it = selItem(); if (!it) return;
        drag = { mode: "resize", dir: handle.className.replace("handle ", "").trim(), it: it, start: clientToMM(e.clientX, e.clientY), o: clone(it) };
        if (handle.classList.contains("h-rot")) drag.mode = "rotate";
        pushUndo(); e.stopPropagation(); e.preventDefault(); return;
      }
      if (piEl) {
        var id = +piEl.dataset.id; select(id);
        var it2 = findItem(id); if (!it2 || it2.locked) return;
        drag = { mode: "move", it: it2, start: clientToMM(e.clientX, e.clientY), o: clone(it2) };
        pushUndo(); e.preventDefault();
      }
    });
    window.addEventListener("mousemove", function (e) {
      if (!drag) return;
      var p = clientToMM(e.clientX, e.clientY);
      var dx = p.x - drag.start.x, dy = p.y - drag.start.y;
      var it = drag.it, o = drag.o;
      if (drag.mode === "move") { it.x = Math.max(0, Math.round((o.x + dx) * 2) / 2); it.y = Math.max(0, Math.round((o.y + dy) * 2) / 2); }
      else if (drag.mode === "rotate") {
        var cx = o.x + o.w / 2, cy = o.y + o.h / 2;
        it.rot = Math.round(Math.atan2(p.y - cy, p.x - cx) * 180 / Math.PI + 90);
      } else { // resize
        var d = drag.dir;
        if (d.indexOf("e") >= 0) it.w = Math.max(2, o.w + dx);
        if (d.indexOf("s") >= 0) it.h = Math.max(2, o.h + dy);
        if (d.indexOf("w") >= 0) { it.w = Math.max(2, o.w - dx); it.x = o.x + dx; }
        if (d.indexOf("n") >= 0) { it.h = Math.max(2, o.h - dy); it.y = o.y + dy; }
      }
      renderAll();
    });
    window.addEventListener("mouseup", function () { if (drag) { drag = null; renderInspector(); } });

    // keyboard nudges + delete
    window.addEventListener("keydown", function (e) {
      if (/INPUT|TEXTAREA|SELECT/.test(document.activeElement.tagName)) return;
      var it = selItem();
      if ((e.ctrlKey || e.metaKey) && e.key.toLowerCase() === "z") { e.preventDefault(); doUndo(); return; }
      if ((e.ctrlKey || e.metaKey) && e.key.toLowerCase() === "y") { e.preventDefault(); doRedo(); return; }
      if ((e.ctrlKey || e.metaKey) && e.key.toLowerCase() === "s") { e.preventDefault(); saveDesign(); return; }
      if (!it) return;
      var step = e.shiftKey ? 5 : 1;
      if (e.key === "Delete") { e.preventDefault(); deleteItem(it.id); }
      else if (e.key === "ArrowLeft") { e.preventDefault(); pushUndo(); it.x -= step; renderAll(); }
      else if (e.key === "ArrowRight") { e.preventDefault(); pushUndo(); it.x += step; renderAll(); }
      else if (e.key === "ArrowUp") { e.preventDefault(); pushUndo(); it.y -= step; renderAll(); }
      else if (e.key === "ArrowDown") { e.preventDefault(); pushUndo(); it.y += step; renderAll(); }
    });
  }

  /* ----------------------------------------------------- image upload ----- */
  function pickImageFor(it) {
    var inp = document.createElement("input");
    inp.type = "file"; inp.accept = "image/*";
    inp.addEventListener("change", function () {
      var f = inp.files[0]; if (!f) return;
      var rd = new FileReader();
      rd.onload = function () {
        pushUndo(); it.imgPath = rd.result; renderAll(); renderInspector();
        toast("تصویر بارگذاری شد");
      };
      rd.readAsDataURL(f);
    });
    inp.click();
  }

  /* -------------------------------------------------- templates gallery --- */
  var TPL_GROUPS = [
    { key: "reception", title: "پذیرش" },
    { key: "appointment", title: "نوبت‌دهی" },
    { key: "lab", title: "آزمایشگاه و رادیولوژی" }
  ];
  function tplGroupOf(t) {
    var g = (t.group || "").toLowerCase();
    if (g) return g;
    var n = (t.name || "") + (t.kind || "");
    if (/نوبت|appoint|queue/i.test(n)) return "appointment";
    if (/آزمایش|رادیو|lab|radio/i.test(n)) return "lab";
    return "reception";
  }
  function openTemplateGallery() {
    var ov = document.getElementById("tplOverlay");
    var grid = document.getElementById("tplGrid");
    var tabs = document.getElementById("tplTabs");
    grid.innerHTML = ""; tabs.innerHTML = "";

    var current = "reception";
    function renderTab() {
      Array.prototype.slice.call(tabs.children).forEach(function (b) { b.classList.toggle("active", b.dataset.g === current); });
      grid.innerHTML = "";
      var list = (S.templates || []).filter(function (t) { return tplGroupOf(t) === current; });
      if (!list.length) { grid.innerHTML = "<div class='muted' style='padding:24px'>طرحی در این دسته نیست.</div>"; return; }
      list.forEach(function (t) {
        var card = document.createElement("div"); card.className = "tpl-card";
        var thumb = document.createElement("div"); thumb.className = "tpl-thumb";
        thumb.appendChild(buildThumb(t));
        var nm = document.createElement("div"); nm.className = "tpl-nm"; nm.textContent = t.name || "طرح";
        var meta = document.createElement("div"); meta.className = "tpl-meta"; meta.textContent = (t.paper || "A5") + " · " + ((t.items || []).length) + " آیتم";
        card.appendChild(thumb); card.appendChild(nm); card.appendChild(meta);
        card.addEventListener("click", function () { applyTemplate(t); ov.classList.add("hidden"); });
        grid.appendChild(card);
      });
    }
    TPL_GROUPS.forEach(function (g) {
      var b = document.createElement("button"); b.className = "tpl-tab"; b.dataset.g = g.key; b.textContent = g.title;
      b.addEventListener("click", function () { current = g.key; renderTab(); });
      tabs.appendChild(b);
    });
    renderTab();
    ov.classList.remove("hidden");
  }

  // simple SVG-ish DOM thumbnail (scaled mini paper)
  function buildThumb(t) {
    var dm = paperDims(t.paper || "A5", t.orientation || 0);
    var scale = 150 / dm[1];
    var p = document.createElement("div"); p.className = "mini-paper";
    p.style.width = (dm[0] * scale) + "px"; p.style.height = (dm[1] * scale) + "px";
    (t.items || []).forEach(function (it) {
      var e = document.createElement("div"); e.className = "mini-it mini-" + it.type;
      e.style.left = (it.x * scale) + "px"; e.style.top = (it.y * scale) + "px";
      e.style.width = (it.w * scale) + "px"; e.style.height = Math.max(1, it.h * scale) + "px";
      if (it.type === "hline" || it.type === "vline") e.style.background = "#445";
      else if (it.type === "label" || it.type === "field" || it.type === "apptno") { e.style.background = it.type === "field" ? "#cfe0ff" : "#dfe3ea"; }
      else e.style.border = "0.5px solid #889";
      p.appendChild(e);
    });
    return p;
  }

  function applyTemplate(t) {
    pushUndo();
    var d = clone(t);
    d.id = (S.design && S.design.id) || 0;   // keep binding id so save UPDATES current
    d.kind = "user";
    if (!d.name) d.name = t.name || "طرح";
    var k = 1; (d.items || []).forEach(function (it) { it.id = k++; });
    S.design = d; S.selId = 0;
    renderAll(); fitZoom(); renderLayers(); renderInspector();
    toast("طرح اعمال شد: " + (t.name || ""));
  }

  /* ------------------------------------------------------------ save ------ */
  function saveDesign() {
    if (!S.design) return;
    if (!S.design.name || S.design.kind === "builtin") {
      var nm = prompt("نام طرح را وارد کنید:", S.design.name || "طرح سفارشی");
      if (nm === null) return;
      S.design.name = nm || "طرح سفارشی"; S.design.kind = "user";
    }
    if (Bridge.has()) {
      var btn = document.getElementById("btnSave"); if (btn) { btn.disabled = true; btn.textContent = "در حال ذخیره…"; }
      Bridge.request("save", { design: S.design }, function (res) {
        if (btn) { btn.disabled = false; btn.innerHTML = "💾 ذخیره و اعمال"; }
        if (res && res.ok) { if (res.id) S.design.id = res.id; S.dirty = false; toast("ذخیره شد و بر بخش اعمال گردید ✓"); }
        else toast("خطا در ذخیره — دوباره تلاش کنید", true);
      });
    } else {
      try { localStorage.setItem("az_design", JSON.stringify(S.design)); } catch (_) {}
      S.dirty = false; toast("ذخیره شد (حالت آزمایشی)");
    }
  }
  function downloadDesign() {
    var data = JSON.stringify(S.design);
    if (Bridge.has()) { Bridge.request("download", { design: S.design, name: S.design.name }, function () {}); toast("درخواست دانلود ارسال شد"); return; }
    var blob = new Blob([data], { type: "application/json" });
    var a = document.createElement("a"); a.href = URL.createObjectURL(blob);
    a.download = (S.design.name || "design") + ".aztpl"; a.click();
    setTimeout(function () { URL.revokeObjectURL(a.href); }, 500);
  }
  function uploadDesign() {
    document.getElementById("fileInput").click();
  }

  /* ------------------------------------------------------------ wire ------ */
  function wire() {
    document.getElementById("paperSel").addEventListener("change", function () { pushUndo(); S.design.paper = this.value; renderAll(); fitZoom(); });
    document.getElementById("orientSel").addEventListener("change", function () { pushUndo(); S.design.orientation = +this.value; renderAll(); fitZoom(); });
    document.getElementById("btnUndo").addEventListener("click", doUndo);
    document.getElementById("btnRedo").addEventListener("click", doRedo);
    document.getElementById("btnZoomIn").addEventListener("click", function () { setScale(S.scale * 1.15); });
    document.getElementById("btnZoomOut").addEventListener("click", function () { setScale(S.scale / 1.15); });
    document.getElementById("btnZoomFit").addEventListener("click", fitZoom);
    document.getElementById("btnSave").addEventListener("click", saveDesign);
    document.getElementById("btnDownload").addEventListener("click", downloadDesign);
    document.getElementById("btnUpload").addEventListener("click", uploadDesign);
    document.getElementById("btnTemplates").addEventListener("click", openTemplateGallery);
    document.getElementById("tplClose").addEventListener("click", function () { document.getElementById("tplOverlay").classList.add("hidden"); });
    document.getElementById("tplOverlay").addEventListener("click", function (e) { if (e.target.id === "tplOverlay") this.classList.add("hidden"); });
    document.getElementById("btnExit").addEventListener("click", function () {
      if (S.dirty && !confirm("تغییرات ذخیره‌نشده دارید. خارج می‌شوید؟")) return;
      if (Bridge.has()) Bridge.request("exit", {}, function () {});
      toast("جلسهٔ طراحی پایان یافت");
    });
    Array.prototype.slice.call(document.querySelectorAll(".rp-tab")).forEach(function (t) {
      t.addEventListener("click", function () {
        switchTab(t.dataset.tab);
        if (t.dataset.tab === "inspector") renderInspector();
        if (t.dataset.tab === "layers") renderLayers();
      });
    });
    document.getElementById("paletteSearch").addEventListener("input", function () { filterPalette(this.value); });

    document.getElementById("fileInput").addEventListener("change", function (e) {
      var f = e.target.files[0]; if (!f) return;
      var r = new FileReader();
      r.onload = function () {
        try { var d = JSON.parse(r.result); pushUndo(); d.id = (S.design && S.design.id) || 0; S.design = d; S.selId = 0; renderAll(); fitZoom(); toast("طرح بارگذاری شد"); }
        catch (_) { toast("فایل نامعتبر است", true); }
      };
      r.readAsText(f); e.target.value = "";
    });
  }

  /* ------------------------------------------------------------ init ------ */
  function loadInitial(cb) {
    if (Bridge.has()) {
      Bridge.request("init", {}, function (res) {
        var initial = null, secName = "";
        if (res) {
          if (res.design) initial = res.design;
          if (res.sectionName) secName = res.sectionName;
        }
        Bridge.request("templates", {}, function (tr) {
          if (tr && tr.templates && tr.templates.length) {
            // merge C++ builtins with the rich JS gallery (JS gallery preferred for UX)
            if (!S.templates || !S.templates.length) S.templates = tr.templates;
          }
          finish(initial, secName);
        });
      });
    } else { finish(null, ""); }

    function finish(initial, secName) {
      if (!initial) {
        initial = clone((S.templates && S.templates[0]) || { paper: "A5", orientation: 0, items: [] });
        var k = 1; (initial.items || []).forEach(function (it) { it.id = k++; });
      }
      S.design = initial;
      if (!S.design.paper) S.design.paper = "A5";
      if (!S.design.items) S.design.items = [];
      var sn = document.getElementById("secName");
      if (sn) sn.textContent = secName ? ("— " + secName) : "";
      document.getElementById("paperSel").value = S.design.paper;
      document.getElementById("orientSel").value = S.design.orientation || 0;
      cb && cb();
    }
  }

  function boot() {
    $paper = document.getElementById("paper");
    $scroll = document.getElementById("canvasScroll");
    $selBox = document.getElementById("selBox");
    $stage = document.getElementById("canvasStage");
    buildPalette(); wire(); wireCanvas();
    loadInitial(function () {
      renderAll(); fitZoom(); renderLayers(); updateUndoButtons();
      if (Bridge.has()) Bridge.request("ready", {}, function () {});
    });
  }
  if (document.readyState === "loading") document.addEventListener("DOMContentLoaded", boot);
  else boot();

  window.AZDesigner = { S: S, render: renderAll, save: saveDesign };
})();
