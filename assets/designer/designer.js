/* ===========================================================================
   designer.js — Azadi-Teb professional print designer engine (v1.21.0)
   Dependency-free, RTL-aware WYSIWYG editor. Talks to C++ over the loopback
   HTTP host (/api/*) using ASYNCHRONOUS XHR.

   v1.21.0 changes:
     • Right-side tools panel, LEFT live preview (matches app spec).
     • Real DOWNLOAD: writes a .aztpl file via a browser Blob (works because the
       designer now opens in the default browser). No more silent "sent" toast.
     • Robust SAVE with explicit console diagnostics on every failure.
     • TABLE designer (rows/cols/header + per-cell text & {field} bindings).
       Stored in the item's `text` field as compact JSON so it round-trips
       through C++ unchanged and prints/previews identically (true WYSIWYG).
     • WYSIWYG text rendering: top-aligned, pt-based font sizes — identical to
       the GDI print path (DT_TOP, lf = pt*dpi/72).
     • Paper-size dropdown populated from a shared PAPER table (incl. small /
       laser sizes) so designer + printer agree.
   =========================================================================== */
(function () {
  "use strict";

  /* ------------------------------------------------------------- bridge --- */
  var Bridge = {
    _on: (location.protocol === "http:" || location.protocol === "https:"),
    has: function () { return this._on; },
    request: function (verb, args, cb) {
      cb = cb || function () {};
      if (!this._on) { cb(null); return; }
      try {
        var xhr = new XMLHttpRequest();
        xhr.open("POST", "api/" + verb, true);   // ASYNC
        xhr.setRequestHeader("Content-Type", "application/json;charset=utf-8");
        xhr.onreadystatechange = function () {
          if (xhr.readyState !== 4) return;
          if (xhr.status >= 200 && xhr.status < 300) {
            var r = null;
            try { r = xhr.responseText ? JSON.parse(xhr.responseText) : {}; }
            catch (e) { console.error("[designer] bad JSON from /api/" + verb, e, xhr.responseText); r = null; }
            cb(r);
          } else { console.error("[designer] /api/" + verb + " HTTP " + xhr.status); cb(null); }
        };
        xhr.onerror = function () { console.error("[designer] network error on /api/" + verb); cb(null); };
        xhr.send(JSON.stringify(args || {}));
      } catch (e) { console.error("[designer] request threw on /api/" + verb, e); cb(null); }
    }
  };

  /* --------------------------------------------------------- paper data --- */
  // Shared with C++ Paper_Dims(). Portrait mm. Includes small / laser sizes.
  var PAPER = {
    A3: [297, 420], A4: [210, 297], A5: [148, 210], A6: [105, 148],
    A7: [74, 105], B4: [250, 353], B5: [176, 250], B6: [125, 176],
    Letter: [215.9, 279.4], Legal: [215.9, 355.6], HalfLetter: [139.7, 215.9],
    R80: [80, 200], R80L: [80, 297], R72: [72, 200], R58: [58, 200], R58L: [58, 297],
    R44: [44, 150], L90: [90, 130], L100: [100, 150], L102: [102, 152], L130: [130, 180]
  };
  var PAPER_LABELS = {
    A3: "A3 (۲۹۷×۴۲۰)", A4: "A4 (۲۱۰×۲۹۷)", A5: "A5 (۱۴۸×۲۱۰)", A6: "A6 (۱۰۵×۱۴۸)",
    A7: "A7 (۷۴×۱۰۵)", B4: "B4 (۲۵۰×۳۵۳)", B5: "B5 (۱۷۶×۲۵۰)", B6: "B6 (۱۲۵×۱۷۶)",
    Letter: "Letter (نامه)", Legal: "Legal (حقوقی)", HalfLetter: "نیم‌نامه (۱۴×۲۱.۶)",
    R80: "رول حرارتی ۸ سانت", R80L: "رول حرارتی ۸ سانت بلند",
    R72: "رول حرارتی ۷.۲ سانت", R58: "رول حرارتی ۵.۸ سانت",
    R58L: "رول حرارتی ۵.۸ سانت بلند", R44: "رول حرارتی ۴.۴ سانت",
    L90: "لیبل/لیزری ۹×۱۳", L100: "لیبل/لیزری ۱۰×۱۵",
    L102: "لیبل ۱۰.۲×۱۵.۲", L130: "لیبل بزرگ ۱۳×۱۸"
  };
  function paperDims(p, orient) {
    var d = PAPER[p] || [148, 210], w = d[0], h = d[1];
    if (orient === 1) { var t = w; w = h; h = t; }
    return [w, h];
  }

  // v1.22.0 RESPONSIVE: proportionally reflow every item from one page size to
  // another so a design keeps its relative layout when the paper changes. Font
  // sizes and border widths scale by the geometric-mean factor; clamped so they
  // never run off the new page.
  function reflowItems(fromW, fromH, toW, toH) {
    if (!S.design || !S.design.items) return;
    if (fromW <= 0 || fromH <= 0 || toW <= 0 || toH <= 0) return;
    var sx = toW / fromW, sy = toH / fromH;
    var sf = Math.sqrt(sx * sy);            // uniform factor for fonts/strokes
    S.design.items.forEach(function (it) {
      it.x = Math.round(it.x * sx * 100) / 100;
      it.y = Math.round(it.y * sy * 100) / 100;
      it.w = Math.round(it.w * sx * 100) / 100;
      it.h = Math.round(it.h * sy * 100) / 100;
      if (typeof it.fontSize === "number" && it.fontSize > 0)
        it.fontSize = Math.max(4, Math.round(it.fontSize * sf * 10) / 10);
      if (typeof it.borderWidth === "number" && it.borderWidth > 0)
        it.borderWidth = Math.max(0.1, Math.round(it.borderWidth * sf * 100) / 100);
      // keep inside new page
      if (it.w > toW) it.w = toW;
      if (it.h > toH) it.h = toH;
      if (it.x < 0) it.x = 0; if (it.y < 0) it.y = 0;
      if (it.x + it.w > toW) it.x = Math.max(0, toW - it.w);
      if (it.y + it.h > toH) it.y = Math.max(0, toH - it.h);
    });
  }

  function changePaper(newPaper, newOrient) {
    if (!S.design) return;
    var oldDims = paperDims(S.design.paper, S.design.orientation);
    if (typeof newPaper === "string") S.design.paper = newPaper;
    if (typeof newOrient === "number") S.design.orientation = newOrient;
    var newDims = paperDims(S.design.paper, S.design.orientation);
    pushUndo();
    if (S.reflowOnResize !== false)
      reflowItems(oldDims[0], oldDims[1], newDims[0], newDims[1]);
    renderAll(); fitZoom();
  }

  /* ---------------------------------------------------------- app state --- */
  var S = {
    design: null, selId: 0,
    pxPerMM: 3.7795, scale: 1,
    undo: [], redo: [], dirty: false,
    reflowOnResize: true,                 // v1.22.0 responsive paper resize
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
  function toast(msg, kind) {
    var t = document.getElementById("toast"); if (!t) return;
    t.textContent = msg;
    t.className = "toast-msg show" + (kind ? (" " + kind) : "");
    clearTimeout(toast._t);
    toast._t = setTimeout(function () { t.className = "toast-msg"; }, 2400);
  }

  /* --------------------------------------------------------- undo stack --- */
  function pushUndo() {
    S.undo.push(clone(S.design)); if (S.undo.length > 100) S.undo.shift();
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
    qr: "بارکد / QR", apptno: "شماره نوبت", image: "تصویر", table: "جدول"
  };
  var ITEM_ICONS = {
    label: "T", field: "{}", hline: "—", vline: "│", rect: "▭", frame: "⬚",
    logo: "★", photo: "👤", qr: "▦", apptno: "#", image: "🖼", table: "▦"
  };

  function mm(v) { return v * S.pxPerMM * S.scale; }
  // Font size in px so that on screen it equals the printed point size.
  // 1pt = 1/72 inch; CSS px = 96/inch ⇒ 1pt = 96/72 px = 1.3333px, then * zoom.
  function ptPx(pt) { return (pt || 10) * (96 / 72) * S.scale; }

  function styleItem(el, it) {
    el.style.left = mm(it.x) + "px";
    el.style.top = mm(it.y) + "px";
    el.style.width = mm(it.w) + "px";
    el.style.height = mm(it.h) + "px";
    el.style.transform = it.rot ? "rotate(" + it.rot + "deg)" : "";
    el.style.opacity = (it.opacity == null ? 1 : it.opacity);
    el.style.zIndex = it.z || 0;
  }

  function displayText(it) {
    if (it.type === "label") return it.text || "";
    if (it.type === "apptno") return (it.prefix || "") + faDigits(String(it.startValue || 1));
    if (it.type === "field") {
      var f = window.AZ_FIELDS[it.field];
      var lbl = f ? f.label : (it.field || "فیلد");
      return (it.prefix || "") + "［" + lbl + "］" + (it.suffix || "");
    }
    return it.text || "";
  }

  /* -------------------------------------------------- table data helpers -- */
  // A table item stores its model as JSON inside it.text:
  //   {cols:n, rows:n, header:true, widths:[..], cells:[[..]]}
  function parseTable(it) {
    try {
      var t = JSON.parse(it.text || "");
      if (t && t.cells) {
        if (!t.widths || t.widths.length !== t.cols) {
          t.widths = []; for (var i = 0; i < t.cols; i++) t.widths.push(1);
        }
        return t;
      }
    } catch (e) {}
    return { cols: 3, rows: 3, header: true, widths: [1, 1, 1],
      cells: [["ستون ۱", "ستون ۲", "ستون ۳"], ["", "", ""], ["", "", ""]] };
  }
  function tableHtml(it, forThumb) {
    var t = parseTable(it);
    var sum = 0; t.widths.forEach(function (w) { sum += (w || 1); });
    var tdir = (it.dir === 1) ? "ltr" : "rtl";
    var html = "<table style='font-size:" + (forThumb ? "100%" : ptPx(it.pt || 9) + "px") +
      ";color:" + (it.textColor || "#000") + ";direction:" + tdir + "'>";
    for (var r = 0; r < t.rows; r++) {
      html += "<tr>";
      for (var c = 0; c < t.cols; c++) {
        var isHd = (t.header && r === 0);
        var wpc = ((t.widths[c] || 1) / sum * 100).toFixed(3);
        var v = (t.cells[r] && t.cells[r][c] != null) ? t.cells[r][c] : "";
        // show field labels for {tokens}
        v = String(v).replace(/\{[a-zA-Z]+\}/g, function (tok) {
          var f = window.AZ_FIELDS[tok]; return f ? ("［" + f.label + "］") : tok;
        });
        html += "<td class='" + (isHd ? "th" : "") + "' style='width:" + wpc + "%;border-color:" +
          (it.borderColor || "#333") + "'>" + escapeHtml(v) + "</td>";
      }
      html += "</tr>";
    }
    html += "</table>";
    return html;
  }
  function escapeHtml(s) {
    return String(s).replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;");
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
      t.style.fontSize = ptPx(it.pt) + "px";
      t.style.fontWeight = it.bold ? "700" : "400";
      t.style.fontStyle = it.italic ? "italic" : "normal";
      t.style.fontFamily = (it.font || "Vazirmatn") + ",Tahoma,sans-serif";
      t.style.justifyContent = it.align === 1 ? "center" : (it.align === 2 ? "flex-start" : "flex-end");
      t.style.textAlign = it.align === 1 ? "center" : (it.align === 2 ? "left" : "right");
      t.style.alignItems = "flex-start";   // matches GDI DT_TOP
      t.style.lineHeight = (it.lineSpacing && it.lineSpacing > 0) ? it.lineSpacing : 1.25;
      // v1.22.0: text direction (RTL/LTR/Center). dir 0=RTL 1=LTR 2=center.
      if (it.dir === 1) { t.style.direction = "ltr"; if (it.align == null || it.align === 0) { t.style.justifyContent = "flex-start"; t.style.textAlign = "left"; } }
      else if (it.dir === 2) { t.style.direction = "rtl"; t.style.justifyContent = "center"; t.style.textAlign = "center"; }
      else { t.style.direction = "rtl"; }
      el.appendChild(t);
    } else if (it.type === "table") {
      el.innerHTML = tableHtml(it, false);
    } else if (it.type === "hline") {
      el.style.height = Math.max(1, mm(it.borderWidth || 0.4)) + "px";
      el.style.background = it.borderColor || "#222";
    } else if (it.type === "vline") {
      el.style.width = Math.max(1, mm(it.borderWidth || 0.4)) + "px";
      el.style.background = it.borderColor || "#222";
    } else if (it.type === "rect" || it.type === "frame") {
      el.style.border = Math.max(1, mm(it.borderWidth || 0.4)) + "px solid " + (it.borderColor || "#222");
      el.style.borderRadius = mm(it.corner || 0) + "px";
      if (!it.fillTransparent && it.fillColor) el.style.background = it.fillColor;
      // v1.22.0 fix: a frame / transparent box must NOT capture clicks on its
      // empty center — otherwise clicking the blank page selects the frame.
      // We make the element click-through and rely on edge hit-testing
      // (hitItemAt) so it is only selected when its border/handle is clicked.
      if (it.type === "frame" || it.fillTransparent) el.style.pointerEvents = "none";
    } else if (it.type === "logo" || it.type === "photo" || it.type === "image" || it.type === "qr") {
      el.style.borderRadius = mm(it.corner || 0) + "px";
      if (it.imgPath && /^data:/.test(it.imgPath)) {
        var img = document.createElement("img");
        img.src = it.imgPath; img.className = "pi-img";
        el.appendChild(img);
      } else {
        el.style.border = "1px dashed #9aa7c2";
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

    Array.prototype.slice.call($paper.querySelectorAll(".pi")).forEach(function (e) { e.remove(); });

    var items = (S.design.items || []).slice().sort(function (a, b) { return (a.z || 0) - (b.z || 0); });
    items.forEach(function (it) { $paper.appendChild(buildItemEl(it)); });

    updateSelBox();
    var pl = document.getElementById("paperLbl");
    if (pl) pl.textContent = (PAPER_LABELS[S.design.paper] || S.design.paper) + " · " +
      faDigits(Math.round(dims[0])) + "×" + faDigits(Math.round(dims[1])) + " mm";
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
      font: "Vazirmatn", pt: 11, bold: false, italic: false, align: 0, dir: 0, lineSpacing: 1.25,
      textColor: "#111111", fillColor: "#ffffff", fillTransparent: true,
      borderColor: "#333333", borderWidth: 0.4, corner: 0, padding: 1, opacity: 1,
      visibility: 0, startValue: 1, step: 1, imgPath: ""
    };
    if (type === "label") { it.text = "متن"; it.w = 40; it.h = 8; }
    else if (type === "field") { it.w = 50; it.h = 8; }
    else if (type === "hline") { it.w = 80; it.h = 1; it.borderWidth = 0.4; }
    else if (type === "vline") { it.w = 1; it.h = 40; it.borderWidth = 0.4; }
    else if (type === "rect") { it.w = 50; it.h = 25; }
    else if (type === "frame") {
      var dm = paperDims(S.design.paper, S.design.orientation);
      it.x = 4; it.y = 4; it.w = dm[0] - 8; it.h = dm[1] - 8; it.isFrame = true; it.borderWidth = 0.6;
    }
    else if (type === "logo") { it.w = 28; it.h = 28; }
    else if (type === "photo") { it.w = 25; it.h = 32; }
    else if (type === "image") { it.w = 40; it.h = 25; }
    else if (type === "qr") { it.w = 24; it.h = 24; }
    else if (type === "apptno") { it.w = 30; it.h = 14; it.pt = 22; it.bold = true; it.align = 1; }
    else if (type === "table") {
      it.w = 90; it.h = 30; it.pt = 9; it.borderWidth = 0.4;
      it.text = JSON.stringify({ cols: 3, rows: 4, header: true, widths: [1, 1, 1],
        cells: [["ردیف", "شرح", "مبلغ"], ["", "", ""], ["", "", ""], ["", "", ""]] });
    }
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

    var objs = [
      ["label", "متن ثابت"], ["field", "فیلد داده"], ["table", "جدول"],
      ["hline", "خط افقی"], ["vline", "خط عمودی"], ["rect", "کادر"],
      ["frame", "حاشیه صفحه"], ["logo", "لوگو"], ["photo", "عکس بیمار"],
      ["image", "تصویر"], ["qr", "بارکد / QR"], ["apptno", "شماره نوبت"]
    ];
    var sec = document.createElement("div"); sec.className = "pl-cat";
    sec.innerHTML = "<div class='pl-cat-h'>عناصر طراحی</div>";
    var grid = document.createElement("div"); grid.className = "pl-grid";
    objs.forEach(function (o) {
      var b = document.createElement("button");
      b.className = "pl-tile"; b.dataset.kind = "obj"; b.dataset.type = o[0];
      b.innerHTML = "<span class='pl-ic'>" + (ITEM_ICONS[o[0]] || "•") + "</span><span class='pl-lb'>" + o[1] + "</span>";
      b.title = o[1];
      b.addEventListener("click", function () {
        if (o[0] === "table") { openTableBuilder(null); return; }
        if (o[0] === "field") { switchTab("palette"); toast("یک فیلد از فهرست زیر را انتخاب کنید"); return; }
        addItem(o[0]);
      });
      grid.appendChild(b);
    });
    sec.appendChild(grid); host.appendChild(sec);

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
      var txt = (b.dataset.label || (b.querySelector(".pl-lb") && b.querySelector(".pl-lb").textContent) || "");
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

    var hd = document.createElement("div"); hd.className = "insp-head";
    hd.innerHTML = "<span class='insp-type'>" + (ITEM_LABELS[it.type] || it.type) + "</span>";
    var del = document.createElement("button"); del.className = "btn btn-sm btn-danger"; del.textContent = "حذف";
    del.addEventListener("click", function () { deleteItem(it.id); });
    var dup = document.createElement("button"); dup.className = "btn btn-sm btn-outline"; dup.textContent = "تکثیر";
    dup.addEventListener("click", function () { duplicateItem(it.id); });
    hd.appendChild(dup); hd.appendChild(del);
    body.appendChild(hd);

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
    if (it.type === "table") {
      var gtb = grp("جدول");
      var edit = document.createElement("button"); edit.className = "btn btn-sm btn-primary"; edit.style.width = "100%";
      edit.textContent = "ویرایش محتوای جدول…";
      edit.addEventListener("click", function () { openTableBuilder(it); });
      gtb.appendChild(edit);
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

    if (it.type === "label" || it.type === "field" || it.type === "apptno" || it.type === "table") {
      var gt = grp("قلم و متن");
      if (it.type !== "table")
        gt.appendChild(row("فونت", selectInput([["Vazirmatn", "وزیر"], ["Tahoma", "تاهوما"], ["IRANSans", "ایران‌سنس"]], it.font, function (v) { it.font = v; up(); })));
      gt.appendChild(row("اندازه (pt)", numInput(it.pt, 5, 96, 0.5, function (v) { it.pt = v; up(); })));
      if (it.type !== "table") {
        gt.appendChild(row("ضخیم", checkInput(it.bold, function (v) { it.bold = v; up(); })));
        gt.appendChild(row("کج", checkInput(it.italic, function (v) { it.italic = v; up(); })));
        gt.appendChild(row("چینش", selectInput([["0", "راست"], ["1", "وسط"], ["2", "چپ"]], it.align, function (v) { it.align = +v; up(); })));
        gt.appendChild(row("جهت متن", selectInput([["0", "راست‌به‌چپ (RTL)"], ["1", "چپ‌به‌راست (LTR)"], ["2", "وسط‌چین (Center)"]], it.dir || 0, function (v) { it.dir = +v; up(); })));
      }
      gt.appendChild(row("رنگ متن", colorInput(it.textColor, function (v) { it.textColor = v; up(); })));
      if (it.type !== "table")
        gt.appendChild(row("فاصله خطوط", numInput(it.lineSpacing, 1, 3, 0.05, function (v) { it.lineSpacing = v; up(); })));
    }

    if (it.type === "rect" || it.type === "frame" || it.type === "hline" || it.type === "vline" ||
        it.type === "qr" || it.type === "logo" || it.type === "photo" || it.type === "image" || it.type === "table") {
      var gb = grp("کادر و خط");
      gb.appendChild(row("رنگ خط", colorInput(it.borderColor, function (v) { it.borderColor = v; up(); })));
      gb.appendChild(row("ضخامت (mm)", numInput(it.borderWidth, 0, 5, 0.1, function (v) { it.borderWidth = v; up(); })));
      if (it.type === "rect" || it.type === "frame") {
        gb.appendChild(row("گردی گوشه", numInput(it.corner, 0, 30, 0.5, function (v) { it.corner = v; up(); })));
        gb.appendChild(row("بدون پُرکننده", checkInput(it.fillTransparent, function (v) { it.fillTransparent = v; up(); })));
        gb.appendChild(row("رنگ پُرکننده", colorInput(it.fillColor, function (v) { it.fillColor = v; up(); })));
      }
    }

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
      r.innerHTML = "<span class='lyr-ic'>" + (ITEM_ICONS[it.type] || "•") + "</span><span class='lyr-nm'>" + escapeHtml(nm) + "</span>";
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
    var availW = $scroll.clientWidth - 80, availH = $scroll.clientHeight - 90;
    var sw = availW / (dm[0] * S.pxPerMM), sh = availH / (dm[1] * S.pxPerMM);
    setScale(Math.max(0.2, Math.min(sw, sh)));
    centerPaper();
  }
  function centerPaper() {
    $scroll.scrollLeft = ($stage.clientWidth - $scroll.clientWidth) / 2;
    $scroll.scrollTop = 40;
  }

  /* --------------------------------------------- canvas interactions ------ */
  function clientToMM(clientX, clientY) {
    var r = $paper.getBoundingClientRect();
    return { x: (clientX - r.left) / (S.pxPerMM * S.scale), y: (clientY - r.top) / (S.pxPerMM * S.scale) };
  }

  // v1.22.0: topmost item hit at a paper coordinate (mm). For frames and
  // transparent rectangles only the BORDER band counts as a hit, so their
  // empty interior is click-through (lets you click items/page behind them).
  function hitItemAt(mmx, mmy) {
    var items = (S.design.items || []).slice().sort(function (a, b) { return (b.z || 0) - (a.z || 0); });
    for (var i = 0; i < items.length; i++) {
      var it = items[i];
      if (it.locked) continue;
      if (mmx < it.x || mmx > it.x + it.w || mmy < it.y || mmy > it.y + it.h) continue;
      var hollow = (it.type === "frame") || ((it.type === "rect") && it.fillTransparent);
      if (hollow) {
        // border band thickness in mm (min 1.5mm so it's easy to grab)
        var band = Math.max(1.5, (it.borderWidth || 0.4) + 1.2);
        var inside = (mmx > it.x + band && mmx < it.x + it.w - band &&
                      mmy > it.y + band && mmy < it.y + it.h - band);
        if (inside) continue;   // clicked the hollow center → pass through
      }
      return it;
    }
    return null;
  }

  function wireCanvas() {
    var panning = false, panSX = 0, panSY = 0, scL = 0, scT = 0, moved = false;
    var drag = null;

    $scroll.addEventListener("mousedown", function (e) {
      var pi = e.target.closest && e.target.closest(".pi");
      var handle = e.target.closest && e.target.closest(".handle");
      if (pi || handle) return;
      // v1.22.0: frames/transparent-rects have pointer-events:none in their
      // hollow center, so e.target won't be a .pi even when the click lands on
      // their border band. Hit-test by coordinate so the border is selectable.
      var p = clientToMM(e.clientX, e.clientY);
      var hit = hitItemAt(p.x, p.y);
      if (hit) {
        select(hit.id);
        if (!hit.locked) drag = { mode: "move", it: hit, start: p, o: clone(hit) };
        pushUndo(); e.preventDefault(); return;
      }
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

    $scroll.addEventListener("wheel", function (e) {
      if (e.ctrlKey) { e.preventDefault(); setScale(S.scale * (e.deltaY < 0 ? 1.1 : 0.9)); }
    }, { passive: false });

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
        pushUndo(); e.preventDefault(); return;
      }
      // v1.22.0: no .pi under cursor — could be a hollow frame/rect border.
      var p = clientToMM(e.clientX, e.clientY);
      var hit = hitItemAt(p.x, p.y);
      if (hit) {
        select(hit.id);
        if (!hit.locked) drag = { mode: "move", it: hit, start: p, o: clone(hit) };
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
      } else {
        var d = drag.dir;
        if (d.indexOf("e") >= 0) it.w = Math.max(2, o.w + dx);
        if (d.indexOf("s") >= 0) it.h = Math.max(2, o.h + dy);
        if (d.indexOf("w") >= 0) { it.w = Math.max(2, o.w - dx); it.x = o.x + dx; }
        if (d.indexOf("n") >= 0) { it.h = Math.max(2, o.h - dy); it.y = o.y + dy; }
      }
      renderAll();
    });
    window.addEventListener("mouseup", function () { if (drag) { drag = null; renderInspector(); } });

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
        toast("تصویر بارگذاری شد", "ok");
      };
      rd.readAsDataURL(f);
    });
    inp.click();
  }

  /* -------------------------------------------------- table builder ------- */
  var tblTarget = null;        // existing item being edited, or null = new
  function openTableBuilder(it) {
    tblTarget = it;
    tblSel = { r: -1, c: -1 };
    var model = it ? parseTable(it) : { cols: 3, rows: 4, header: true, widths: [1, 1, 1],
      cells: [["ردیف", "شرح", "مبلغ"], ["", "", ""], ["", "", ""], ["", "", ""]] };
    document.getElementById("tblCols").value = model.cols;
    document.getElementById("tblRows").value = model.rows;
    document.getElementById("tblHeader").checked = !!model.header;
    renderTableEditor(model);
    document.getElementById("tblOverlay").classList.remove("hidden");
  }
  // v1.22.0: track the currently selected cell (row,col) in the table editor.
  var tblSel = { r: -1, c: -1 };
  function readTableModel() {
    var cols = Math.max(1, Math.min(10, +document.getElementById("tblCols").value || 3));
    var rows = Math.max(1, Math.min(40, +document.getElementById("tblRows").value || 4));
    var header = document.getElementById("tblHeader").checked;
    var cells = [], widths = [];
    var inputs = document.querySelectorAll("#tblEditor input.cell");
    var k = 0;
    for (var r = 0; r < rows; r++) { cells[r] = []; for (var c = 0; c < cols; c++) { cells[r][c] = inputs[k] ? inputs[k].value : ""; k++; } }
    for (var c2 = 0; c2 < cols; c2++) widths[c2] = 1;
    return { cols: cols, rows: rows, header: header, widths: widths, cells: cells };
  }
  function renderTableEditor(model) {
    var host = document.getElementById("tblEditor");
    var html = "<table><tbody>";
    for (var r = 0; r < model.rows; r++) {
      html += "<tr>";
      for (var c = 0; c < model.cols; c++) {
        var v = (model.cells[r] && model.cells[r][c] != null) ? model.cells[r][c] : "";
        var hd = (model.header && r === 0) ? "hd" : "";
        var selCls = (r === tblSel.r && c === tblSel.c) ? " cellsel" : "";
        html += "<td data-r='" + r + "' data-c='" + c + "'><input class='cell " + hd + selCls +
          "' data-r='" + r + "' data-c='" + c + "' value=\"" + escapeHtml(v).replace(/"/g, "&quot;") + "\"></td>";
      }
      html += "</tr>";
    }
    html += "</tbody></table>";
    host.innerHTML = html;
    // wire cell focus → remember selection (so add/del row/col + field-insert target it)
    Array.prototype.slice.call(host.querySelectorAll("input.cell")).forEach(function (inp) {
      inp.addEventListener("focus", function () {
        tblSel = { r: +this.dataset.r, c: +this.dataset.c };
        Array.prototype.slice.call(host.querySelectorAll("input.cell")).forEach(function (x) { x.classList.remove("cellsel"); });
        this.classList.add("cellsel");
      });
    });
  }

  // build the field dropdown for in-cell insertion
  function populateTblFieldSelect() {
    var sel = document.getElementById("tblFieldSel");
    if (!sel) return;
    sel.innerHTML = "<option value=''>— انتخاب فیلد —</option>";
    (window.AZ_FIELD_CATS || []).forEach(function (cat) {
      var og = document.createElement("optgroup"); og.label = cat.title;
      cat.items.forEach(function (f) {
        var op = document.createElement("option"); op.value = f.key; op.textContent = f.label + "  " + f.key;
        og.appendChild(op);
      });
      sel.appendChild(og);
    });
  }

  function wireTableBuilder() {
    document.getElementById("tblClose").addEventListener("click", function () { document.getElementById("tblOverlay").classList.add("hidden"); });
    document.getElementById("tblCancel").addEventListener("click", function () { document.getElementById("tblOverlay").classList.add("hidden"); });
    populateTblFieldSelect();
    function rebuild(model) {
      document.getElementById("tblCols").value = model.cols;
      document.getElementById("tblRows").value = model.rows;
      renderTableEditor(model);
    }
    document.getElementById("tblRebuild").addEventListener("click", function () {
      var cur = readTableModel();
      var cols = Math.max(1, Math.min(10, +document.getElementById("tblCols").value || 3));
      var rows = Math.max(1, Math.min(40, +document.getElementById("tblRows").value || 4));
      var m = { cols: cols, rows: rows, header: document.getElementById("tblHeader").checked, widths: [], cells: [] };
      for (var r = 0; r < rows; r++) { m.cells[r] = []; for (var c = 0; c < cols; c++) { m.cells[r][c] = (cur.cells[r] && cur.cells[r][c]) || ""; } }
      renderTableEditor(m);
    });
    // ---- v1.22.0 add/delete row & column ----
    document.getElementById("tblAddRow").addEventListener("click", function () {
      var m = readTableModel();
      var at = (tblSel.r >= 0 ? tblSel.r + 1 : m.rows);
      var nr = []; for (var c = 0; c < m.cols; c++) nr.push("");
      m.cells.splice(at, 0, nr); m.rows++;
      tblSel = { r: at, c: 0 }; rebuild(m);
    });
    document.getElementById("tblAddCol").addEventListener("click", function () {
      var m = readTableModel();
      var at = (tblSel.c >= 0 ? tblSel.c + 1 : m.cols);
      for (var r = 0; r < m.rows; r++) m.cells[r].splice(at, 0, "");
      m.cols++; m.widths.splice(at, 0, 1);
      tblSel = { r: 0, c: at }; rebuild(m);
    });
    document.getElementById("tblDelRow").addEventListener("click", function () {
      var m = readTableModel(); if (m.rows <= 1) { toast("حداقل یک ردیف لازم است", "err"); return; }
      var at = (tblSel.r >= 0 ? tblSel.r : m.rows - 1);
      m.cells.splice(at, 1); m.rows--;
      tblSel = { r: Math.min(at, m.rows - 1), c: tblSel.c }; rebuild(m);
    });
    document.getElementById("tblDelCol").addEventListener("click", function () {
      var m = readTableModel(); if (m.cols <= 1) { toast("حداقل یک ستون لازم است", "err"); return; }
      var at = (tblSel.c >= 0 ? tblSel.c : m.cols - 1);
      for (var r = 0; r < m.rows; r++) m.cells[r].splice(at, 1);
      m.cols--; m.widths.splice(at, 1);
      tblSel = { r: tblSel.r, c: Math.min(at, m.cols - 1) }; rebuild(m);
    });
    // ---- v1.22.0 insert a {field} token into the selected cell ----
    document.getElementById("tblFieldInsert").addEventListener("click", function () {
      var key = document.getElementById("tblFieldSel").value;
      if (!key) { toast("ابتدا یک فیلد را انتخاب کنید", "err"); return; }
      if (tblSel.r < 0 || tblSel.c < 0) { toast("ابتدا یک سلول را انتخاب کنید", "err"); return; }
      var inp = document.querySelector("#tblEditor input.cell[data-r='" + tblSel.r + "'][data-c='" + tblSel.c + "']");
      if (!inp) { toast("سلول یافت نشد", "err"); return; }
      inp.value = (inp.value ? inp.value + " " : "") + key;
      inp.focus();
      toast("فیلد در سلول درج شد", "ok");
    });
    document.getElementById("tblInsert").addEventListener("click", function () {
      var model = readTableModel();
      if (tblTarget) {
        pushUndo();
        tblTarget.text = JSON.stringify(model);
        renderAll(); renderInspector();
      } else {
        pushUndo();
        var it = defaultItem("table");
        it.text = JSON.stringify(model);
        var dm = paperDims(S.design.paper, S.design.orientation);
        it.w = Math.min(dm[0] - 16, 120); it.x = 8; it.y = 40;
        it.h = Math.min(dm[1] - 50, model.rows * 8 + 2);
        S.design.items.push(it); renderAll(); select(it.id);
      }
      document.getElementById("tblOverlay").classList.add("hidden");
      toast("جدول اعمال شد", "ok");
    });
  }

  /* -------------------------------------------------- templates gallery --- */
  var TPL_GROUPS = [
    { key: "reception", title: "پذیرش و صورتحساب" },
    { key: "appointment", title: "نوبت‌دهی" },
    { key: "lab", title: "آزمایشگاه و تصویربرداری" }
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
        var meta = document.createElement("div"); meta.className = "tpl-meta"; meta.textContent = (PAPER_LABELS[t.paper] || t.paper || "A5") + " · " + faDigits((t.items || []).length) + " آیتم";
        card.appendChild(thumb); card.appendChild(nm); card.appendChild(meta);
        card.addEventListener("click", function () { applyTemplate(t); ov.classList.add("hidden"); });
        grid.appendChild(card);
      });
    }
    TPL_GROUPS.forEach(function (g) {
      var b = document.createElement("button"); b.className = "tpl-tab"; b.dataset.g = g.key;
      var cnt = (S.templates || []).filter(function (t) { return tplGroupOf(t) === g.key; }).length;
      b.textContent = g.title + " (" + faDigits(cnt) + ")";
      b.addEventListener("click", function () { current = g.key; renderTab(); });
      tabs.appendChild(b);
    });
    renderTab();
    ov.classList.remove("hidden");
  }

  // v1.22.0 "طراحی‌های من" — list user-saved designs from the C++ DB.
  function openMyDesigns() {
    var ov = document.getElementById("myOverlay");
    var grid = document.getElementById("myGrid");
    grid.innerHTML = "<div class='muted' style='padding:24px'>در حال بارگذاری…</div>";
    ov.classList.remove("hidden");

    function render(list) {
      grid.innerHTML = "";
      if (!list || !list.length) {
        grid.innerHTML = "<div class='muted' style='padding:24px'>هنوز طرحی ذخیره نکرده‌اید. پس از طراحی، دکمهٔ «ذخیره و اعمال» را بزنید و نامی برای طرح وارد کنید تا اینجا نمایش داده شود.</div>";
        return;
      }
      list.forEach(function (t) {
        var card = document.createElement("div"); card.className = "tpl-card my-card";
        var thumb = document.createElement("div"); thumb.className = "tpl-thumb";
        thumb.appendChild(buildThumb(t));
        var nm = document.createElement("div"); nm.className = "tpl-nm"; nm.textContent = t.name || "طرح من";
        var meta = document.createElement("div"); meta.className = "tpl-meta";
        meta.textContent = (PAPER_LABELS[t.paper] || t.paper || "A5") + " · " + faDigits((t.items || []).length) + " آیتم";
        var del = document.createElement("button"); del.className = "my-del"; del.title = "حذف طرح"; del.textContent = "🗑";
        del.addEventListener("click", function (e) {
          e.stopPropagation();
          if (!confirm("این طرح حذف شود؟ «" + (t.name || "") + "»")) return;
          Bridge.request("delete", { id: t.id }, function (res) {
            if (res && res.ok) { card.remove(); toast("طرح حذف شد", "ok"); }
            else toast("خطا در حذف طرح", "err");
          });
        });
        card.appendChild(del);
        card.appendChild(thumb); card.appendChild(nm); card.appendChild(meta);
        card.addEventListener("click", function () { applyTemplate(t); ov.classList.add("hidden"); });
        grid.appendChild(card);
      });
    }

    if (Bridge.has()) {
      Bridge.request("mydesigns", {}, function (res) {
        render((res && res.designs) || []);
      });
    } else {
      var local = [];
      try { var raw = localStorage.getItem("az_design"); if (raw) local = [JSON.parse(raw)]; } catch (e) {}
      render(local);
    }
  }

  // High-fidelity mini preview: render real text/lines/boxes/tables.
  function buildThumb(t) {
    var dm = paperDims(t.paper || "A5", t.orientation || 0);
    var maxH = 184, maxW = 168;
    var scale = Math.min(maxH / dm[1], maxW / dm[0]);
    var p = document.createElement("div"); p.className = "mini-paper";
    p.style.width = (dm[0] * scale) + "px"; p.style.height = (dm[1] * scale) + "px";
    var items = (t.items || []).slice().sort(function (a, b) { return (a.z || 0) - (b.z || 0); });
    items.forEach(function (it) {
      var e = document.createElement("div"); e.className = "mini-it";
      e.style.left = (it.x * scale) + "px"; e.style.top = (it.y * scale) + "px";
      e.style.width = (it.w * scale) + "px"; e.style.height = Math.max(0.6, it.h * scale) + "px";
      if (it.type === "hline") { e.style.borderTop = "0.7px solid " + (it.borderColor || "#444"); }
      else if (it.type === "vline") { e.style.borderRight = "0.7px solid " + (it.borderColor || "#444"); }
      else if (it.type === "rect" || it.type === "frame" || it.type === "table") {
        e.style.border = "0.6px solid " + (it.borderColor || "#789");
        if (!it.fillTransparent && it.fillColor && it.type === "rect") e.style.background = it.fillColor;
      }
      else if (it.type === "logo" || it.type === "photo" || it.type === "image" || it.type === "qr") {
        e.style.border = "0.6px dashed #9aa7c2"; e.style.background = "#f3f6fb";
      }
      else if (it.type === "label" || it.type === "field" || it.type === "apptno") {
        e.className += " mini-tx";
        var txt = it.type === "label" ? (it.text || "") :
          it.type === "apptno" ? "۱۲" :
          ((it.prefix || "") + (window.AZ_FIELDS[it.field] ? window.AZ_FIELDS[it.field].label : "···"));
        e.textContent = txt;
        e.style.color = it.textColor || "#222";
        e.style.fontSize = Math.max(3.5, (it.pt || 9) * scale * 1.05) + "px";
        e.style.fontWeight = it.bold ? "700" : "400";
        e.style.justifyContent = it.align === 1 ? "center" : (it.align === 2 ? "flex-start" : "flex-end");
        if (it.type === "field") e.style.background = "rgba(207,224,255,.5)";
      }
      p.appendChild(e);
    });
    return p;
  }

  function applyTemplate(t) {
    pushUndo();
    var d = clone(t);
    d.id = (S.design && S.design.id) || 0;   // keep binding so save UPDATES current
    d.kind = "user";
    if (!d.name) d.name = t.name || "طرح";
    var k = 1; (d.items || []).forEach(function (it) { it.id = k++; });
    S.design = d; S.selId = 0;
    document.getElementById("paperSel").value = S.design.paper;
    document.getElementById("orientSel").value = S.design.orientation || 0;
    renderAll(); fitZoom(); renderLayers(); renderInspector();
    toast("طرح اعمال شد: " + (t.name || ""), "ok");
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
        if (res && res.ok) {
          if (res.id) S.design.id = res.id;
          S.dirty = false;
          toast("ذخیره شد و بر بخش اعمال گردید ✓", "ok");
        } else {
          var why = (res && res.err) ? (" — " + res.err) : "";
          console.error("[designer] save failed", res);
          toast("خطا در ذخیره" + why + " (کنسول را ببینید)", "err");
        }
      });
    } else {
      try { localStorage.setItem("az_design", JSON.stringify(S.design)); } catch (e) { console.error(e); }
      S.dirty = false; toast("ذخیره شد (حالت آزمایشی)", "ok");
    }
  }

  // Real download: build a .aztpl file and let the browser save it. Works in
  // the default browser the designer now opens in.
  function downloadDesign() {
    try {
      var data = JSON.stringify(S.design, null, 0);
      var blob = new Blob([data], { type: "application/octet-stream" });
      var url = URL.createObjectURL(blob);
      var a = document.createElement("a");
      a.href = url;
      a.download = (S.design.name || "design").replace(/[\\/:*?"<>|]/g, "_") + ".aztpl";
      document.body.appendChild(a); a.click(); document.body.removeChild(a);
      setTimeout(function () { URL.revokeObjectURL(url); }, 1000);
      toast("فایل طرح دانلود شد", "ok");
    } catch (e) {
      console.error("[designer] download failed", e);
      toast("دانلود ناموفق بود (کنسول را ببینید)", "err");
    }
  }
  function uploadDesign() { document.getElementById("fileInput").click(); }

  /* ------------------------------------------------------------ wire ------ */
  function populatePaperSelect() {
    var sel = document.getElementById("paperSel");
    sel.innerHTML = "";
    Object.keys(PAPER).forEach(function (k) {
      var op = document.createElement("option"); op.value = k; op.textContent = PAPER_LABELS[k] || k;
      sel.appendChild(op);
    });
  }

  function wire() {
    populatePaperSelect();
    document.getElementById("paperSel").addEventListener("change", function () { changePaper(this.value, undefined); });
    document.getElementById("orientSel").addEventListener("change", function () { changePaper(undefined, +this.value); });
    var rf = document.getElementById("chkReflow");
    if (rf) { rf.checked = S.reflowOnResize; rf.addEventListener("change", function () { S.reflowOnResize = this.checked; }); }
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
    var bMy = document.getElementById("btnMyDesigns");
    if (bMy) bMy.addEventListener("click", openMyDesigns);
    var myClose = document.getElementById("myClose");
    if (myClose) myClose.addEventListener("click", function () { document.getElementById("myOverlay").classList.add("hidden"); });
    var myOv = document.getElementById("myOverlay");
    if (myOv) myOv.addEventListener("click", function (e) { if (e.target.id === "myOverlay") this.classList.add("hidden"); });
    document.getElementById("btnExit").addEventListener("click", function () {
      if (S.dirty && !confirm("تغییرات ذخیره‌نشده دارید. خارج می‌شوید؟")) return;
      if (Bridge.has()) Bridge.request("exit", {}, function () {});
      toast("جلسهٔ طراحی پایان یافت");
      setTimeout(function () { try { window.close(); } catch (e) {} }, 400);
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
        try {
          var d = JSON.parse(r.result);
          pushUndo(); d.id = (S.design && S.design.id) || 0; S.design = d; S.selId = 0;
          if (!S.design.paper) S.design.paper = "A5"; if (!S.design.items) S.design.items = [];
          document.getElementById("paperSel").value = S.design.paper;
          document.getElementById("orientSel").value = S.design.orientation || 0;
          renderAll(); fitZoom(); renderLayers(); toast("طرح بارگذاری شد", "ok");
        } catch (err) { console.error("[designer] invalid file", err); toast("فایل نامعتبر است", "err"); }
      };
      r.readAsText(f); e.target.value = "";
    });

    wireTableBuilder();
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
        // Always prefer the rich JS gallery for the gallery itself.
        if (!S.templates || !S.templates.length) {
          Bridge.request("templates", {}, function (tr) {
            if (tr && tr.templates && tr.templates.length) S.templates = tr.templates;
            finish(initial, secName);
          });
        } else { finish(initial, secName); }
      });
    } else { finish(null, ""); }

    function finish(initial, secName) {
      if (!initial || !(initial.items && initial.items.length)) {
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
