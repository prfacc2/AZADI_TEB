/* ===========================================================================
   templates.js — Azadi-Teb ready-made professional print templates (v1.44.0)
   grouped gallery (filter «بخش = پذیرش» etc.):
     • reception   (30+) — admission slips / receipts / bills (incl. v1.44
                    services-table subtype + billing/insurance tokens)
     • appointment (20) — queue / appointment cards
     • lab         (20) — laboratory & radiology request/report headers
   Each template is mm-based (paper coordinates) so it reflows correctly when
   the paper size is switched (A4 ↔ A5 etc.). Layouts are built relative to the
   paper width/height, modelled after common Iranian clinic forms.
   =========================================================================== */
(function () {
  "use strict";

  var PAPER = { A4: [210, 297], A5: [148, 210], A6: [105, 148], Letter: [215.9, 279.4] };
  var _uid = 0;
  function nid() { return ++_uid; }

  function base() {
    return {
      id: nid(), type: "label", x: 10, y: 10, w: 40, h: 8, rot: 0, z: 1,
      locked: false, isFrame: false, text: "", field: "", prefix: "", suffix: "",
      font: "Vazirmatn", pt: 11, bold: false, italic: false, align: 0, dir: 0, lineSpacing: 1.25,
      textColor: "#111111", fillColor: "#ffffff", fillTransparent: true,
      borderColor: "#333333", borderWidth: 1, corner: 0, padding: 1, opacity: 1,
      visibility: 0, startValue: 1, step: 1, imgPath: ""
    };
  }
  function mk(o) { return Object.assign(base(), o); }

  function L(x, y, w, h, text, o) { return mk(Object.assign({ type: "label", x: x, y: y, w: w, h: h, text: text, align: 0, pt: 10 }, o || {})); }
  function F(x, y, w, h, field, o) { return mk(Object.assign({ type: "field", x: x, y: y, w: w, h: h, field: field, align: 0, pt: 10 }, o || {})); }
  function HL(x, y, w, o) { return mk(Object.assign({ type: "hline", x: x, y: y, w: w, h: 0.5, borderWidth: 0.5, borderColor: "#444" }, o || {})); }
  function VL(x, y, h, o) { return mk(Object.assign({ type: "vline", x: x, y: y, w: 0.5, h: h, borderWidth: 0.5, borderColor: "#444" }, o || {})); }
  function R(x, y, w, h, o) { return mk(Object.assign({ type: "rect", x: x, y: y, w: w, h: h, borderWidth: 0.8, borderColor: "#345" }, o || {})); }
  function FR(W, H) { return mk({ type: "frame", x: 4, y: 4, w: W - 8, h: H - 8, isFrame: true, borderWidth: 1.2, borderColor: "#1f4e8c" }); }
  function LOGO(x, y, s) { return mk({ type: "logo", x: x, y: y, w: s, h: s, borderColor: "#aab", borderWidth: 0.5 }); }
  function QR(x, y, s) { return mk({ type: "qr", x: x, y: y, w: s, h: s, borderColor: "#aab", borderWidth: 0.5 }); }
  function PHOTO(x, y, w, h) { return mk({ type: "photo", x: x, y: y, w: w, h: h, borderColor: "#aab", borderWidth: 0.6 }); }
  function APPT(x, y, w, h, o) { return mk(Object.assign({ type: "apptno", x: x, y: y, w: w, h: h, pt: 30, bold: true, align: 1, startValue: 1, step: 1 }, o || {})); }

  // labelled value: "label: [field]" laid on one row, RTL
  function pair(x, y, w, labelTxt, field, o) {
    o = o || {};
    return F(x, y, w, o.h || 7, field, Object.assign({ prefix: labelTxt + " ", align: 0, pt: o.pt || 9.5, bold: o.bold || false, textColor: o.color || "#111" }, o));
  }

  var ALL = [];
  function push(group, name, paper, orient, items) {
    items.forEach(function (it, i) { it.id = i + 1; });
    ALL.push({ id: 0, name: name, kind: "builtin", group: group, paper: paper, orientation: orient || 0, items: items });
  }

  /* ====================================================================== *
   *  SHARED PROFESSIONAL HELPERS (used by reception + premium)
   *  All text defaults to RTL (dir:0) right-aligned (align:0) so every form
   *  reads naturally right-to-left. Centered titles use dir:2.
   * ====================================================================== */
  // coloured fill band (header / accent strip)
  function band(x, y, w, h, color, o) {
    return mk(Object.assign({ type: "rect", x: x, y: y, w: w, h: h, fillTransparent: false,
      fillColor: color, borderWidth: (o && o.borderWidth) || 0, borderColor: (o && o.borderColor) || color,
      corner: (o && o.corner) != null ? o.corner : 1.5 }, o || {}));
  }
  // centered RTL title
  function title(x, y, w, h, txt, o) { return L(x, y, w, h, txt, Object.assign({ align: 1, dir: 2, bold: true }, o || {})); }
  // right-aligned RTL label:value pair (label first, value flows to its left)
  function PV(x, y, w, lbl, field, o) {
    o = o || {};
    return F(x, y, w, o.h || 7, field, Object.assign({
      prefix: lbl + " ", align: 0, dir: 0, pt: o.pt || 9.5, bold: o.bold || false, textColor: o.color || "#1a2433" }, o));
  }
  // soft info cell (small grey label on top, bold value below) — RTL
  function cell(x, y, w, h, lbl, field, o) {
    o = o || {}; var arr = [];
    arr.push(R(x, y, w, h, { borderColor: o.bc || "#cdd7e6", borderWidth: 0.4, corner: 1.2,
      fillTransparent: o.bg === false, fillColor: o.bg || "#f6f9ff" }));
    arr.push(L(x + 2, y + 1.1, w - 4, 4, lbl, { pt: 7.2, align: 0, dir: 0, textColor: o.lc || "#7a879c" }));
    if (field) arr.push(F(x + 2, y + 4.8, w - 4, h - 5.6, field, { pt: o.pt || 9.5, bold: o.bold !== false, align: 0, dir: 0, textColor: o.color || "#16233a" }));
    else arr.push(L(x + 2, y + 4.8, w - 4, h - 5.6, o.text || "", { pt: o.pt || 9.5, bold: o.bold !== false, align: 0, dir: 0, textColor: o.color || "#16233a" }));
    return arr;
  }
  function svcTable(x, y, w, h, o) {
    o = o || {};
    var cells = o.cells || [{ r: 0, c: 0, t: "شرح خدمت" }, { r: 0, c: 1, t: "تعداد" }, { r: 0, c: 2, t: "مبلغ (ریال)" }];
    var widths = o.widths || [0.55, 0.2, 0.25];
    var cols = widths.length, rows = o.rows || 4;
    return mk({ type: "table", x: x, y: y, w: w, h: h, borderColor: o.borderColor || "#2b5f9e", borderWidth: 0.5,
      text: JSON.stringify({ cols: cols, rows: rows, header: true, widths: widths, cells: cells }) });
  }

  /* ====================================================================== *
   *  RECEPTION (20) — twenty DISTINCT, professional, RTL admission forms.
   *  Each has its own colour identity, layout and field set (no two alike).
   * ====================================================================== */
  (function () {
    var AC; // accent colour per template

    // 1) کلاسیک آبی A5 — ساده، تمیز، سطری
    push("reception", "۱) پذیرش کلاسیک آبی", "A5", 0, (function () {
      AC = "#1f4e8c"; var W = 148, H = 210, x = 8, cw = W - 16, items = [], y = 8;
      items.push(LOGO(W - x - 18, y, 18));
      items.push(title(x, y + 1, cw - 22, 9, "درمانگاه آزادی طب", { pt: 16, textColor: AC, align: 0, dir: 0 }));
      items.push(L(x, y + 10, cw - 22, 6, "برگهٔ پذیرش بیمار", { pt: 9.5, align: 0, dir: 0, textColor: "#5a6b85" })); y += 20;
      items.push(HL(x, y, cw, { borderColor: AC, borderWidth: 0.8 })); y += 4;
      items.push(PV(x + cw / 2, y, cw / 2, "تاریخ:", "{date}"));
      items.push(PV(x, y, cw / 2, "ساعت:", "{time}")); y += 8;
      items.push(PV(x, y, cw, "نام و نام خانوادگی:", "{full}", { bold: true, pt: 11 })); y += 8;
      items.push(PV(x + cw / 2, y, cw / 2, "کد ملی:", "{nid}")); items.push(PV(x, y, cw / 2, "نام پدر:", "{father}")); y += 7.5;
      items.push(PV(x + cw / 2, y, cw / 2, "تاریخ تولد:", "{birth}")); items.push(PV(x, y, cw / 2, "جنسیت:", "{gender}")); y += 7.5;
      items.push(PV(x + cw / 2, y, cw / 2, "تلفن همراه:", "{mobile}")); items.push(PV(x, y, cw / 2, "بیمه:", "{ins}")); y += 9;
      items.push(L(x, y, cw, 6, "صورتحساب", { bold: true, pt: 10, align: 0, dir: 0, textColor: AC })); y += 7;
      items.push(PV(x + cw / 2, y, cw / 2, "جمع کل:", "{total}")); items.push(PV(x, y, cw / 2, "پرداختی:", "{paid}", { bold: true })); y += 10;
      items.push(R(x, y, cw, 13, { borderColor: AC, borderWidth: 0.9, corner: 2 }));
      items.push(F(x + 2, y + 3, cw - 4, 7, "{queue}", { prefix: "شماره نوبت: ", bold: true, pt: 13, align: 1, dir: 2, textColor: AC })); y += 16;
      items.push(HL(x, y, cw, { borderColor: "#cfd6e2" })); y += 1.5;
      items.push(L(x, y, cw, 5, "نشانی: تهران، خیابان آزادی — تلفن: ۰۲۱-۰۰۰۰۰۰۰۰", { pt: 7.5, align: 1, dir: 2, textColor: "#9aa3b4" }));
      return items;
    })());

    // 2) بنر رنگی مدرن A5 — هدر یکپارچه + سلول‌های نرم
    push("reception", "۲) پذیرش مدرن (هدر رنگی)", "A5", 0, (function () {
      AC = "#0e7c86"; var W = 148, H = 210, x = 8, cw = W - 16, items = [], y = 8;
      items.push(band(x, y, cw, 20, AC, { corner: 2 }));
      items.push(LOGO(x + 2, y + 2, 16));
      items.push(L(x + 20, y + 2.5, cw - 22, 7, "درمانگاه آزادی طب", { pt: 14, bold: true, align: 0, dir: 0, textColor: "#fff" }));
      items.push(L(x + 20, y + 10.5, cw - 22, 5, "پذیرش و صورتحساب بیمار", { pt: 8.5, align: 0, dir: 0, textColor: "#d6f3f5" })); y += 24;
      items.push(PV(x + cw / 2, y, cw / 2, "تاریخ:", "{date}")); items.push(PV(x, y, cw / 2, "شماره پذیرش:", "{receiptNo}")); y += 9;
      var c2 = (cw - 4) / 2;
      cell(x + c2 + 4, y, c2, 11, "نام و نام خانوادگی", "{full}", { bc: AC }).forEach(function (e) { items.push(e); });
      cell(x, y, c2, 11, "کد ملی", "{nid}", { bc: AC }).forEach(function (e) { items.push(e); }); y += 13;
      cell(x + c2 + 4, y, c2, 11, "نام پدر", "{father}", { bc: AC }).forEach(function (e) { items.push(e); });
      cell(x, y, c2, 11, "تاریخ تولد", "{birth}", { bc: AC }).forEach(function (e) { items.push(e); }); y += 13;
      cell(x + c2 + 4, y, c2, 11, "جنسیت", "{gender}", { bc: AC }).forEach(function (e) { items.push(e); });
      cell(x, y, c2, 11, "تلفن همراه", "{mobile}", { bc: AC }).forEach(function (e) { items.push(e); }); y += 15;
      items.push(band(x, y, cw * 0.5, 13, "#e7f6f7", { corner: 2, borderColor: AC, borderWidth: 0.5 }));
      items.push(F(x, y + 3, cw * 0.5, 7, "{queue}", { prefix: "نوبت: ", bold: true, pt: 13, align: 1, dir: 2, textColor: AC }));
      items.push(PV(x + cw * 0.5 + 3, y + 1, cw * 0.5 - 3, "جمع:", "{total}"));
      items.push(PV(x + cw * 0.5 + 3, y + 7, cw * 0.5 - 3, "پرداختی:", "{paid}", { bold: true, color: "#0f7a3a" })); y += 16;
      items.push(L(x, y, cw, 5, "نسخهٔ بیمار — لطفاً تا پایان درمان نزد خود نگه دارید.", { pt: 7.5, align: 1, dir: 2, textColor: "#9aa3b4" }));
      return items;
    })());

    // 3) رسید حرارتی فشرده رول ۸ سانت
    push("reception", "۳) رسید حرارتی ۸ سانت", "R80", 0, (function () {
      var W = 80, H = 200, x = 4, cw = W - 8, items = [], y = 5;
      items.push(L(x, y, cw, 7, "درمانگاه آزادی طب", { pt: 13, bold: true, align: 1, dir: 2 })); y += 8;
      items.push(F(x, y, cw, 4, "{clinicphone}", { prefix: "تلفن: ", pt: 7, align: 1, dir: 2, textColor: "#333" })); y += 5;
      items.push(HL(x, y, cw, { borderColor: "#000", borderWidth: 0.4 })); y += 2;
      items.push(PV(x, y, cw, "قبض:", "{receiptNo}", { pt: 8 }));
      items.push(F(x, y, cw, 5, "{date}", { align: 2, dir: 1, pt: 8 })); y += 6;
      items.push(PV(x, y, cw, "بیمار:", "{full}", { bold: true, pt: 9 })); y += 5.5;
      items.push(PV(x, y, cw, "کد ملی:", "{nid}", { pt: 8 })); y += 5.5;
      items.push(PV(x, y, cw, "بیمه:", "{ins}", { pt: 8 })); y += 6;
      items.push(HL(x, y, cw, { borderColor: "#000", borderWidth: 0.3 })); y += 2;
      items.push(PV(x, y, cw, "جمع کل:", "{total}", { pt: 8 })); y += 5;
      items.push(PV(x, y, cw, "سهم بیمه:", "{insshare}", { pt: 8 })); y += 5;
      items.push(PV(x, y, cw, "قابل پرداخت:", "{paid}", { bold: true, pt: 10 })); y += 7;
      items.push(R(x, y, cw, 11, { borderColor: "#000", borderWidth: 0.5, corner: 1 }));
      items.push(F(x, y + 2.5, cw, 6, "{queue}", { prefix: "نوبت: ", bold: true, pt: 13, align: 1, dir: 2 })); y += 13;
      items.push(QR(x + cw / 2 - 9, y, 18)); y += 20;
      items.push(L(x, y, cw, 4, "از انتخاب شما سپاسگزاریم.", { pt: 7.5, align: 1, dir: 2, textColor: "#333" }));
      return items;
    })());

    // 4) دو ستونه با خط جداکننده عمودی
    push("reception", "۴) پذیرش دو ستونه", "A5", 0, (function () {
      AC = "#5b3b8c"; var W = 148, H = 210, x = 8, cw = W - 16, items = [], y = 8;
      items.push(title(x, y, cw, 8, "درمانگاه آزادی طب", { pt: 15, textColor: AC })); y += 9;
      items.push(L(x, y, cw, 5, "برگهٔ پذیرش بیمار", { pt: 9, align: 1, dir: 2, textColor: "#777" })); y += 7;
      items.push(HL(x, y, cw, { borderColor: AC, borderWidth: 0.7 })); y += 4;
      var mid = x + cw / 2;
      items.push(VL(mid, y, 56, { borderColor: "#d7cdec", borderWidth: 0.4 }));
      var ry = y, ly = y, colw = cw / 2 - 4;
      items.push(PV(mid + 3, ry, colw, "نام:", "{first}")); ry += 8;
      items.push(PV(mid + 3, ry, colw, "خانوادگی:", "{last}")); ry += 8;
      items.push(PV(mid + 3, ry, colw, "نام پدر:", "{father}")); ry += 8;
      items.push(PV(mid + 3, ry, colw, "کد ملی:", "{nid}")); ry += 8;
      items.push(PV(x, ly, colw, "تاریخ تولد:", "{birth}")); ly += 8;
      items.push(PV(x, ly, colw, "جنسیت:", "{gender}")); ly += 8;
      items.push(PV(x, ly, colw, "تلفن:", "{mobile}")); ly += 8;
      items.push(PV(x, ly, colw, "بیمه:", "{ins}")); ly += 8;
      y = Math.max(ry, ly) + 4;
      items.push(HL(x, y, cw, { borderColor: "#ddd" })); y += 4;
      items.push(PV(x + cw / 2, y, cw / 2, "جمع کل:", "{total}")); items.push(PV(x, y, cw / 2, "پرداختی:", "{paid}", { bold: true })); y += 10;
      items.push(R(x, y, cw, 13, { borderColor: AC, borderWidth: 0.9, corner: 3 }));
      items.push(F(x, y + 3, cw, 7, "{queue}", { prefix: "شماره نوبت: ", bold: true, pt: 13, align: 1, dir: 2, textColor: AC }));
      return items;
    })());

    // 5) با QR و کد ملی (بارکد)
    push("reception", "۵) پذیرش با QR", "A5", 0, (function () {
      AC = "#0b6e4f"; var W = 148, H = 210, x = 8, cw = W - 16, items = [], y = 8;
      items.push(LOGO(W - x - 18, y, 18));
      items.push(QR(x, y, 18));
      items.push(title(x + 22, y + 2, cw - 44, 8, "درمانگاه آزادی طب", { pt: 14, textColor: AC })); y += 22;
      items.push(HL(x, y, cw, { borderColor: AC, borderWidth: 0.7 })); y += 4;
      items.push(PV(x + cw / 2, y, cw / 2, "تاریخ:", "{date}")); items.push(PV(x, y, cw / 2, "شماره:", "{receiptNo}")); y += 9;
      items.push(PV(x, y, cw, "نام و نام خانوادگی:", "{full}", { bold: true, pt: 11 })); y += 8;
      items.push(PV(x + cw / 2, y, cw / 2, "کد ملی:", "{nid}")); items.push(PV(x, y, cw / 2, "تلفن:", "{mobile}")); y += 7.5;
      items.push(PV(x + cw / 2, y, cw / 2, "بیمه:", "{ins}")); items.push(PV(x, y, cw / 2, "بخش:", "{dept}")); y += 9;
      items.push(L(x, y, cw, 5, "صورتحساب", { bold: true, pt: 10, align: 0, dir: 0, textColor: AC })); y += 6;
      items.push(PV(x + cw / 2, y, cw / 2, "جمع کل:", "{total}")); items.push(PV(x, y, cw / 2, "پرداختی:", "{paid}", { bold: true })); y += 10;
      items.push(R(x, y, cw, 13, { borderColor: AC, borderWidth: 0.9, corner: 2 }));
      items.push(F(x, y + 3, cw, 7, "{queue}", { prefix: "نوبت: ", bold: true, pt: 14, align: 1, dir: 2, textColor: AC })); y += 16;
      items.push(L(x, y, cw, 5, "برای پیگیری آنلاین، QR بالا را اسکن کنید.", { pt: 7.5, align: 1, dir: 2, textColor: "#9aa3b4" }));
      return items;
    })());

    // 6) بیمارستانی A4 کامل با جدول خدمات
    push("reception", "۶) پذیرش بیمارستانی A4", "A4", 0, (function () {
      AC = "#0e4d6e"; var W = 210, H = 297, x = 12, cw = W - 24, items = [], y = 12;
      items.push(band(x, y, cw, 26, AC, { corner: 2 }));
      items.push(LOGO(x + 3, y + 3, 20));
      items.push(L(x + 26, y + 3.5, cw - 50, 9, "مرکز درمانی و تخصصی آزادی طب", { pt: 18, bold: true, align: 0, dir: 0, textColor: "#fff" }));
      items.push(L(x + 26, y + 13, cw - 50, 5, "پذیرش، درمانگاه، تصویربرداری و آزمایشگاه", { pt: 9, align: 0, dir: 0, textColor: "#cfe6f0" }));
      items.push(QR(x + cw - 22, y + 2, 22)); y += 30;
      items.push(band(x, y, cw, 9, "#eaf3f7", { corner: 1.5, borderColor: AC, borderWidth: 0.4 }));
      items.push(L(x + 3, y + 1.8, cw - 6, 5, "برگهٔ پذیرش و صورتحساب بیمار", { pt: 12, bold: true, align: 1, dir: 2, textColor: AC })); y += 12;
      var c2 = (cw - 6) / 2;
      function rowA(l1, f1, l2, f2) {
        cell(x + c2 + 6, y, c2, 12, l1, f1, { bc: AC }).forEach(function (e) { items.push(e); });
        cell(x, y, c2, 12, l2, f2, { bc: AC }).forEach(function (e) { items.push(e); }); y += 14;
      }
      rowA("نام و نام خانوادگی", "{full}", "کد ملی", "{nid}");
      rowA("نام پدر", "{father}", "تاریخ تولد / سن", "{age}");
      rowA("جنسیت", "{gender}", "تلفن همراه", "{mobile}");
      rowA("پزشک معالج", "{doctor}", "بخش / دپارتمان", "{dept}");
      rowA("بیمهٔ پایه", "{ins}", "بیمهٔ مکمل", "{supp}");
      y += 1;
      items.push(L(x, y, cw, 5, "شرح خدمات و صورتحساب", { pt: 11, bold: true, align: 0, dir: 0, textColor: AC })); y += 6;
      items.push(svcTable(x, y, cw, 46, { borderColor: AC, rows: 7, widths: [0.45, 0.15, 0.2, 0.2],
        cells: [{ r: 0, c: 0, t: "شرح خدمت" }, { r: 0, c: 1, t: "تعداد" }, { r: 0, c: 2, t: "بهای واحد" }, { r: 0, c: 3, t: "مبلغ کل" }] })); y += 49;
      var tw = (cw - 9) / 4;
      cell(x + 3 * (tw + 3), y, tw, 12, "جمع کل", "{totalonly}").forEach(function (e) { items.push(e); });
      cell(x + 2 * (tw + 3), y, tw, 12, "سهم بیمه", "{insshareonly}").forEach(function (e) { items.push(e); });
      cell(x + tw + 3, y, tw, 12, "تخفیف", "{discount}").forEach(function (e) { items.push(e); });
      cell(x, y, tw, 12, "قابل پرداخت", "{paidonly}", { bg: "#dff0e3", color: "#0f7a3a" }).forEach(function (e) { items.push(e); }); y += 16;
      items.push(HL(x, y, cw, { borderColor: "#cdd7e6" })); y += 3;
      items.push(L(x + c2 + 6, y, c2, 6, "مهر و امضای پذیرش", { pt: 9, align: 0, dir: 0, textColor: "#6b7689" }));
      items.push(L(x, y, c2, 6, "مهر و امضای صندوق", { pt: 9, align: 0, dir: 0, textColor: "#6b7689" }));
      return items;
    })());

    // 7) درمانگاه تخصصی — با پزشک و بخش
    push("reception", "۷) پذیرش تخصصی (پزشک/بخش)", "A5", 0, (function () {
      AC = "#1f4e8c"; var W = 148, H = 210, x = 8, cw = W - 16, items = [], y = 8;
      items.push(band(x, y, cw, 11, AC, { corner: 1.5 }));
      items.push(L(x + 3, y + 2.5, cw - 6, 6, "درمانگاه تخصصی آزادی طب", { pt: 13, bold: true, align: 1, dir: 2, textColor: "#fff" })); y += 14;
      items.push(PV(x + cw / 2, y, cw / 2, "تاریخ:", "{date}")); items.push(PV(x, y, cw / 2, "ساعت:", "{time}")); y += 8;
      items.push(PV(x, y, cw, "نام و نام خانوادگی:", "{full}", { bold: true, pt: 11 })); y += 8;
      items.push(PV(x + cw / 2, y, cw / 2, "کد ملی:", "{nid}")); items.push(PV(x, y, cw / 2, "سن:", "{age}")); y += 7.5;
      items.push(PV(x + cw / 2, y, cw / 2, "پزشک معالج:", "{doctor}")); items.push(PV(x, y, cw / 2, "بخش:", "{dept}")); y += 7.5;
      items.push(PV(x + cw / 2, y, cw / 2, "بیمه:", "{ins}")); items.push(PV(x, y, cw / 2, "تلفن:", "{mobile}")); y += 9;
      items.push(L(x, y, cw, 5, "علائم حیاتی", { bold: true, pt: 9.5, align: 0, dir: 0, textColor: AC })); y += 6;
      var c3 = (cw - 8) / 3;
      cell(x + 2 * (c3 + 4), y, c3, 11, "فشار خون", "{bp}").forEach(function (e) { items.push(e); });
      cell(x + c3 + 4, y, c3, 11, "وزن", "{weight}").forEach(function (e) { items.push(e); });
      cell(x, y, c3, 11, "قد", "{height}").forEach(function (e) { items.push(e); }); y += 14;
      items.push(R(x, y, cw, 13, { borderColor: AC, borderWidth: 0.9, corner: 2 }));
      items.push(F(x, y + 3, cw, 7, "{queue}", { prefix: "شماره نوبت: ", bold: true, pt: 13, align: 1, dir: 2, textColor: AC }));
      return items;
    })());

    // 8) دندانپزشکی — با چارت ساده
    push("reception", "۸) پذیرش دندانپزشکی", "A5", 0, (function () {
      AC = "#0a7d6b"; var W = 148, H = 210, x = 8, cw = W - 16, items = [], y = 8;
      items.push(title(x, y, cw, 8, "کلینیک دندانپزشکی آزادی طب", { pt: 14, textColor: AC })); y += 10;
      items.push(HL(x, y, cw, { borderColor: AC, borderWidth: 0.7 })); y += 4;
      items.push(PV(x, y, cw, "نام و نام خانوادگی:", "{full}", { bold: true, pt: 11 })); y += 8;
      items.push(PV(x + cw / 2, y, cw / 2, "کد ملی:", "{nid}")); items.push(PV(x, y, cw / 2, "سن:", "{age}")); y += 7.5;
      items.push(PV(x + cw / 2, y, cw / 2, "تلفن:", "{mobile}")); items.push(PV(x, y, cw / 2, "بیمه:", "{ins}")); y += 9;
      items.push(L(x, y, cw, 5, "شرح خدمات دندانپزشکی", { bold: true, pt: 9.5, align: 0, dir: 0, textColor: AC })); y += 6;
      items.push(svcTable(x, y, cw, 34, { borderColor: AC, rows: 5, widths: [0.5, 0.25, 0.25],
        cells: [{ r: 0, c: 0, t: "خدمت / دندان" }, { r: 0, c: 1, t: "تعداد" }, { r: 0, c: 2, t: "مبلغ (ریال)" }] })); y += 37;
      items.push(PV(x + cw / 2, y, cw / 2, "جمع کل:", "{total}")); items.push(PV(x, y, cw / 2, "پرداختی:", "{paid}", { bold: true, color: "#0f7a3a" })); y += 9;
      items.push(R(x, y, cw, 12, { borderColor: AC, borderWidth: 0.9, corner: 2 }));
      items.push(F(x, y + 2.8, cw, 7, "{queue}", { prefix: "نوبت: ", bold: true, pt: 13, align: 1, dir: 2, textColor: AC }));
      return items;
    })());

    // 9) با عکس بیمار (سمت چپ کادر اطلاعات)
    push("reception", "۹) پذیرش با عکس بیمار", "A5", 0, (function () {
      AC = "#7a3b8c"; var W = 148, H = 210, x = 8, cw = W - 16, items = [], y = 8;
      items.push(title(x, y, cw, 8, "درمانگاه آزادی طب", { pt: 15, textColor: AC })); y += 11;
      items.push(HL(x, y, cw, { borderColor: AC, borderWidth: 0.7 })); y += 4;
      items.push(PHOTO(x, y, 24, 30));
      var ix = x + 28, iw = cw - 28;
      items.push(PV(ix, y, iw, "نام و نام خانوادگی:", "{full}", { bold: true, pt: 11 })); 
      items.push(PV(ix, y + 8, iw, "کد ملی:", "{nid}"));
      items.push(PV(ix, y + 15.5, iw, "تاریخ تولد:", "{birth}"));
      items.push(PV(ix, y + 23, iw, "جنسیت:", "{gender}")); y += 33;
      items.push(PV(x + cw / 2, y, cw / 2, "تلفن:", "{mobile}")); items.push(PV(x, y, cw / 2, "بیمه:", "{ins}")); y += 9;
      items.push(PV(x + cw / 2, y, cw / 2, "جمع کل:", "{total}")); items.push(PV(x, y, cw / 2, "پرداختی:", "{paid}", { bold: true })); y += 10;
      items.push(R(x, y, cw, 13, { borderColor: AC, borderWidth: 0.9, corner: 3 }));
      items.push(F(x, y + 3, cw, 7, "{queue}", { prefix: "شماره نوبت: ", bold: true, pt: 13, align: 1, dir: 2, textColor: AC }));
      return items;
    })());

    // 10) صورتحساب تفکیکی کامل (مالی محور)
    push("reception", "۱۰) صورتحساب تفکیکی", "A5", 0, (function () {
      AC = "#b45309"; var W = 148, H = 210, x = 8, cw = W - 16, items = [], y = 8;
      items.push(band(x, y, cw, 12, AC, { corner: 1.5 }));
      items.push(L(x + 3, y + 3, cw - 6, 6, "صورتحساب بیمار", { pt: 14, bold: true, align: 1, dir: 2, textColor: "#fff" })); y += 15;
      items.push(PV(x + cw / 2, y, cw / 2, "تاریخ:", "{date}")); items.push(PV(x, y, cw / 2, "شماره قبض:", "{receiptNo}")); y += 8;
      items.push(PV(x, y, cw, "بیمار:", "{full}", { bold: true })); y += 7;
      items.push(PV(x + cw / 2, y, cw / 2, "کد ملی:", "{nid}")); items.push(PV(x, y, cw / 2, "بیمه:", "{ins}")); y += 9;
      items.push(svcTable(x, y, cw, 40, { borderColor: AC, rows: 6, widths: [0.5, 0.2, 0.3],
        cells: [{ r: 0, c: 0, t: "شرح خدمت" }, { r: 0, c: 1, t: "تعداد" }, { r: 0, c: 2, t: "مبلغ (ریال)" }] })); y += 43;
      items.push(HL(x, y, cw, { borderColor: "#e0c9a6" })); y += 3;
      items.push(PV(x, y, cw, "جمع کل:", "{total}")); y += 7;
      items.push(PV(x, y, cw, "سهم بیمه:", "{insshare}")); y += 7;
      items.push(PV(x, y, cw, "تخفیف:", "{discount}")); y += 7;
      items.push(band(x, y, cw, 11, "#fff7ed", { corner: 2, borderColor: AC, borderWidth: 0.6 }));
      items.push(F(x + 2, y + 2.6, cw - 4, 7, "{paid}", { prefix: "مبلغ قابل پرداخت: ", bold: true, pt: 12, align: 0, dir: 0, textColor: AC }));
      return items;
    })());

    // 11) رسید پرداخت ساده (مینیمال)
    push("reception", "۱۱) رسید پرداخت ساده", "A6", 0, (function () {
      AC = "#334155"; var W = 105, H = 148, x = 7, cw = W - 14, items = [], y = 8;
      items.push(title(x, y, cw, 7, "درمانگاه آزادی طب", { pt: 12, textColor: AC })); y += 9;
      items.push(L(x, y, cw, 5, "رسید پرداخت", { pt: 9, align: 1, dir: 2, textColor: "#777" })); y += 7;
      items.push(HL(x, y, cw, { borderColor: AC, borderWidth: 0.6 })); y += 3;
      items.push(PV(x, y, cw, "بیمار:", "{full}", { bold: true, pt: 10 })); y += 7;
      items.push(PV(x + cw / 2, y, cw / 2, "تاریخ:", "{date}")); items.push(PV(x, y, cw / 2, "قبض:", "{receiptNo}")); y += 8;
      items.push(HL(x, y, cw, { borderColor: "#ddd" })); y += 3;
      items.push(PV(x, y, cw, "جمع کل:", "{total}", { pt: 10 })); y += 7;
      items.push(band(x, y, cw, 12, "#eef2f7", { corner: 2, borderColor: AC, borderWidth: 0.5 }));
      items.push(F(x + 2, y + 3, cw - 4, 7, "{paid}", { prefix: "پرداختی: ", bold: true, pt: 12, align: 1, dir: 2, textColor: AC })); y += 16;
      items.push(L(x, y, cw, 4.5, "با تشکر از پرداخت شما.", { pt: 8, align: 1, dir: 2, textColor: "#999" }));
      return items;
    })());

    // 12) شبانه‌روزی با کادر کامل و شیفت
    push("reception", "۱۲) پذیرش شبانه‌روزی", "A5", 0, (function () {
      AC = "#1d4ed8"; var W = 148, H = 210, x = 8, cw = W - 16, items = [], y = 6;
      items.push(FR(W, H));
      items.push(title(x, y, cw, 8, "درمانگاه شبانه‌روزی آزادی طب", { pt: 13, textColor: AC })); y += 10;
      items.push(HL(x, y, cw, { borderColor: AC, borderWidth: 0.7 })); y += 4;
      items.push(PV(x + cw / 2, y, cw / 2, "تاریخ:", "{date}")); items.push(PV(x, y, cw / 2, "شیفت:", "{shift}")); y += 8;
      items.push(PV(x, y, cw, "نام و نام خانوادگی:", "{full}", { bold: true, pt: 11 })); y += 8;
      items.push(PV(x + cw / 2, y, cw / 2, "کد ملی:", "{nid}")); items.push(PV(x, y, cw / 2, "نوع بیمار:", "{ptype}")); y += 7.5;
      items.push(PV(x + cw / 2, y, cw / 2, "تلفن:", "{mobile}")); items.push(PV(x, y, cw / 2, "بیمه:", "{ins}")); y += 9;
      items.push(PV(x + cw / 2, y, cw / 2, "جمع کل:", "{total}")); items.push(PV(x, y, cw / 2, "پرداختی:", "{paid}", { bold: true })); y += 10;
      items.push(R(x, y, cw, 13, { borderColor: AC, borderWidth: 0.9, corner: 2 }));
      items.push(F(x, y + 3, cw, 7, "{queue}", { prefix: "شماره نوبت: ", bold: true, pt: 13, align: 1, dir: 2, textColor: AC })); y += 16;
      items.push(F(x, y, cw, 5, "{shiftuser}", { prefix: "ثبت: ", pt: 7.5, align: 0, dir: 0, textColor: "#888" }));
      return items;
    })());

    // 13) کادر اطلاعات سلولی (شبکه‌ای)
    push("reception", "۱۳) پذیرش شبکه‌ای", "A5", 0, (function () {
      AC = "#0f766e"; var W = 148, H = 210, x = 8, cw = W - 16, items = [], y = 8;
      items.push(LOGO(W - x - 16, y, 16));
      items.push(title(x, y + 2, cw - 20, 8, "درمانگاه آزادی طب", { pt: 14, textColor: AC, align: 0, dir: 0 })); y += 19;
      var c2 = (cw - 4) / 2;
      function g(l1, f1, l2, f2) {
        cell(x + c2 + 4, y, c2, 11, l1, f1, { bc: AC, bg: "#f0fdfa" }).forEach(function (e) { items.push(e); });
        cell(x, y, c2, 11, l2, f2, { bc: AC, bg: "#f0fdfa" }).forEach(function (e) { items.push(e); }); y += 12.5;
      }
      g("نام و نام خانوادگی", "{full}", "کد ملی", "{nid}");
      g("نام پدر", "{father}", "تاریخ تولد", "{birth}");
      g("جنسیت", "{gender}", "تلفن همراه", "{mobile}");
      g("بیمهٔ پایه", "{ins}", "بیمهٔ مکمل", "{supp}");
      g("جمع کل", "{total}", "پرداختی", "{paid}");
      y += 2;
      items.push(R(x, y, cw, 13, { borderColor: AC, borderWidth: 0.9, corner: 2 }));
      items.push(F(x, y + 3, cw, 7, "{queue}", { prefix: "شماره نوبت: ", bold: true, pt: 13, align: 1, dir: 2, textColor: AC }));
      return items;
    })());

    // 14) مدرن مینیمال با نوار کناری رنگی
    push("reception", "۱۴) پذیرش نوار کناری", "A5", 0, (function () {
      AC = "#dc2626"; var W = 148, H = 210, x = 8, cw = W - 16, items = [], y = 8;
      items.push(band(W - 6, 0, 6, H, AC, { corner: 0 }));     // right colour stripe (RTL leading edge)
      items.push(title(x, y, cw - 4, 8, "درمانگاه آزادی طب", { pt: 15, textColor: AC, align: 0, dir: 0 })); y += 10;
      items.push(L(x, y, cw - 4, 5, "برگهٔ پذیرش بیمار", { pt: 9, align: 0, dir: 0, textColor: "#777" })); y += 7;
      items.push(HL(x, y, cw - 4, { borderColor: AC, borderWidth: 0.6 })); y += 4;
      items.push(PV(x, y, cw - 4, "نام و نام خانوادگی:", "{full}", { bold: true, pt: 11 })); y += 8;
      items.push(PV(x + (cw - 4) / 2, y, (cw - 4) / 2, "کد ملی:", "{nid}")); items.push(PV(x, y, (cw - 4) / 2, "تلفن:", "{mobile}")); y += 7.5;
      items.push(PV(x + (cw - 4) / 2, y, (cw - 4) / 2, "بیمه:", "{ins}")); items.push(PV(x, y, (cw - 4) / 2, "بخش:", "{dept}")); y += 9;
      items.push(PV(x + (cw - 4) / 2, y, (cw - 4) / 2, "جمع کل:", "{total}")); items.push(PV(x, y, (cw - 4) / 2, "پرداختی:", "{paid}", { bold: true })); y += 10;
      items.push(R(x, y, cw - 4, 13, { borderColor: AC, borderWidth: 0.9, corner: 2 }));
      items.push(F(x, y + 3, cw - 4, 7, "{queue}", { prefix: "نوبت: ", bold: true, pt: 14, align: 1, dir: 2, textColor: AC }));
      return items;
    })());

    // 15) بیمه پایه و مکمل (بیمه محور)
    push("reception", "۱۵) پذیرش بیمه‌ای", "A5", 0, (function () {
      AC = "#155e75"; var W = 148, H = 210, x = 8, cw = W - 16, items = [], y = 8;
      items.push(title(x, y, cw, 8, "درمانگاه آزادی طب", { pt: 15, textColor: AC })); y += 11;
      items.push(HL(x, y, cw, { borderColor: AC, borderWidth: 0.7 })); y += 4;
      items.push(PV(x, y, cw, "نام و نام خانوادگی:", "{full}", { bold: true, pt: 11 })); y += 8;
      items.push(PV(x + cw / 2, y, cw / 2, "کد ملی:", "{nid}")); items.push(PV(x, y, cw / 2, "تاریخ تولد:", "{birth}")); y += 9;
      items.push(L(x, y, cw, 5, "اطلاعات بیمه", { bold: true, pt: 9.5, align: 0, dir: 0, textColor: AC })); y += 6;
      var c2 = (cw - 4) / 2;
      cell(x + c2 + 4, y, c2, 11, "بیمهٔ پایه", "{ins}", { bc: AC }).forEach(function (e) { items.push(e); });
      cell(x, y, c2, 11, "شماره دفترچه", "{insno}", { bc: AC }).forEach(function (e) { items.push(e); }); y += 13;
      cell(x + c2 + 4, y, c2, 11, "بیمهٔ مکمل", "{supp}", { bc: AC }).forEach(function (e) { items.push(e); });
      cell(x, y, c2, 11, "اعتبار بیمه", "{insexp}", { bc: AC }).forEach(function (e) { items.push(e); }); y += 14;
      items.push(L(x, y, cw, 5, "صورتحساب", { bold: true, pt: 9.5, align: 0, dir: 0, textColor: AC })); y += 6;
      items.push(PV(x + cw / 2, y, cw / 2, "جمع کل:", "{total}")); items.push(PV(x, y, cw / 2, "سهم بیمه:", "{insshare}")); y += 7;
      items.push(PV(x, y, cw, "قابل پرداخت:", "{paid}", { bold: true, color: "#0f7a3a" })); y += 9;
      items.push(R(x, y, cw, 12, { borderColor: AC, borderWidth: 0.9, corner: 2 }));
      items.push(F(x, y + 2.8, cw, 7, "{queue}", { prefix: "نوبت: ", bold: true, pt: 13, align: 1, dir: 2, textColor: AC }));
      return items;
    })());

    // 16) سرپایی فوری (اورژانس) — قرمز/برجسته
    push("reception", "۱۶) پذیرش سرپایی فوری", "A6", 0, (function () {
      AC = "#b91c1c"; var W = 105, H = 148, x = 7, cw = W - 14, items = [], y = 7;
      items.push(band(x, y, cw, 11, AC, { corner: 2 }));
      items.push(L(x + 2, y + 2.5, cw - 4, 6, "پذیرش سرپایی فوری", { pt: 12, bold: true, align: 1, dir: 2, textColor: "#fff" })); y += 14;
      items.push(PV(x, y, cw, "بیمار:", "{full}", { bold: true, pt: 11 })); y += 8;
      items.push(PV(x + cw / 2, y, cw / 2, "کد ملی:", "{nid}")); items.push(PV(x, y, cw / 2, "سن:", "{age}")); y += 7.5;
      items.push(PV(x + cw / 2, y, cw / 2, "تاریخ:", "{date}")); items.push(PV(x, y, cw / 2, "ساعت:", "{time}")); y += 9;
      items.push(R(x, y, cw, 18, { borderColor: AC, borderWidth: 1.1, corner: 3 }));
      items.push(F(x, y + 4, cw, 11, "{queue}", { prefix: "", bold: true, pt: 30, align: 1, dir: 2, textColor: AC })); y += 21;
      items.push(L(x, y, cw, 5, "لطفاً سریعاً به اتاق درمان مراجعه کنید.", { pt: 8, align: 1, dir: 2, textColor: "#777" }));
      return items;
    })());

    // 17) قبض رول ۵.۸ سانت (باریک)
    push("reception", "۱۷) قبض رول ۵.۸ سانت", "R58", 0, (function () {
      var W = 58, H = 200, x = 3, cw = W - 6, items = [], y = 4;
      items.push(L(x, y, cw, 6, "آزادی طب", { pt: 12, bold: true, align: 1, dir: 2 })); y += 7;
      items.push(HL(x, y, cw, { borderColor: "#000", borderWidth: 0.4 })); y += 2;
      items.push(PV(x, y, cw, "بیمار:", "{full}", { bold: true, pt: 8 })); y += 5;
      items.push(PV(x, y, cw, "کد ملی:", "{nid}", { pt: 7 })); y += 5;
      items.push(F(x, y, cw, 4, "{date}", { align: 1, dir: 2, pt: 7 })); y += 5;
      items.push(HL(x, y, cw, { borderColor: "#000", borderWidth: 0.3 })); y += 2;
      items.push(PV(x, y, cw, "جمع:", "{total}", { pt: 7.5 })); y += 5;
      items.push(PV(x, y, cw, "پرداختی:", "{paid}", { bold: true, pt: 9 })); y += 7;
      items.push(R(x, y, cw, 10, { borderColor: "#000", borderWidth: 0.5, corner: 1 }));
      items.push(F(x, y + 2.5, cw, 6, "{queue}", { prefix: "نوبت ", bold: true, pt: 12, align: 1, dir: 2 })); y += 12;
      items.push(L(x, y, cw, 4, "سپاسگزاریم", { pt: 7, align: 1, dir: 2, textColor: "#333" }));
      return items;
    })());

    // 18) با شرح خدمت و کد خدمت (جدول دو ستونه)
    push("reception", "۱۸) پذیرش با شرح خدمت", "A5", 0, (function () {
      AC = "#4338ca"; var W = 148, H = 210, x = 8, cw = W - 16, items = [], y = 8;
      items.push(title(x, y, cw, 8, "درمانگاه آزادی طب", { pt: 15, textColor: AC })); y += 11;
      items.push(HL(x, y, cw, { borderColor: AC, borderWidth: 0.7 })); y += 4;
      items.push(PV(x, y, cw, "نام و نام خانوادگی:", "{full}", { bold: true, pt: 11 })); y += 8;
      items.push(PV(x + cw / 2, y, cw / 2, "کد ملی:", "{nid}")); items.push(PV(x, y, cw / 2, "پزشک:", "{doctor}")); y += 9;
      items.push(L(x, y, cw, 5, "خدمات ارائه‌شده", { bold: true, pt: 9.5, align: 0, dir: 0, textColor: AC })); y += 6;
      items.push(svcTable(x, y, cw, 42, { borderColor: AC, rows: 6, widths: [0.45, 0.2, 0.15, 0.2],
        cells: [{ r: 0, c: 0, t: "شرح خدمت" }, { r: 0, c: 1, t: "کد" }, { r: 0, c: 2, t: "تعداد" }, { r: 0, c: 3, t: "مبلغ" }] })); y += 45;
      items.push(PV(x + cw / 2, y, cw / 2, "جمع کل:", "{total}")); items.push(PV(x, y, cw / 2, "پرداختی:", "{paid}", { bold: true })); y += 10;
      items.push(R(x, y, cw, 12, { borderColor: AC, borderWidth: 0.9, corner: 2 }));
      items.push(F(x, y + 2.8, cw, 7, "{queue}", { prefix: "نوبت: ", bold: true, pt: 13, align: 1, dir: 2, textColor: AC }));
      return items;
    })());

    // 19) کلینیک زیبایی — لوکس صورتی/طلایی
    push("reception", "۱۹) پذیرش کلینیک زیبایی", "A5", 0, (function () {
      AC = "#be185d"; var W = 148, H = 210, x = 9, cw = W - 18, items = [], y = 8;
      items.push(FR(W, H));
      items.push(band(x, y, cw, 18, AC, { corner: 3 }));
      items.push(LOGO(x + 2, y + 2, 14));
      items.push(L(x + 18, y + 2.5, cw - 20, 7, "کلینیک زیبایی آزادی طب", { pt: 13, bold: true, align: 0, dir: 0, textColor: "#fff" }));
      items.push(L(x + 18, y + 10, cw - 20, 5, "پوست، مو و زیبایی", { pt: 8, align: 0, dir: 0, textColor: "#fce7f3" })); y += 22;
      items.push(PV(x + cw / 2, y, cw / 2, "تاریخ:", "{date}")); items.push(PV(x, y, cw / 2, "نوبت بعدی:", "{nextvisit}")); y += 8;
      items.push(PV(x, y, cw, "نام و نام خانوادگی:", "{full}", { bold: true, pt: 11 })); y += 8;
      items.push(PV(x + cw / 2, y, cw / 2, "تلفن:", "{mobile}")); items.push(PV(x, y, cw / 2, "پزشک:", "{doctor}")); y += 9;
      items.push(L(x, y, cw, 5, "خدمات", { bold: true, pt: 9.5, align: 0, dir: 0, textColor: AC })); y += 6;
      items.push(svcTable(x, y, cw, 34, { borderColor: AC, rows: 5, widths: [0.6, 0.4],
        cells: [{ r: 0, c: 0, t: "خدمت" }, { r: 0, c: 1, t: "مبلغ (ریال)" }] })); y += 37;
      items.push(band(x, y, cw, 11, "#fdf2f8", { corner: 2, borderColor: AC, borderWidth: 0.6 }));
      items.push(F(x + 2, y + 2.6, cw - 4, 7, "{paid}", { prefix: "قابل پرداخت: ", bold: true, pt: 12, align: 0, dir: 0, textColor: AC }));
      return items;
    })());

    // 20) جامع A4 (همه‌چیز: عکس، QR، علائم حیاتی، جدول)
    push("reception", "۲۰) پذیرش جامع A4", "A4", 0, (function () {
      AC = "#1e3a8a"; var W = 210, H = 297, x = 12, cw = W - 24, items = [], y = 12;
      items.push(band(x, y, cw, 24, AC, { corner: 2 }));
      items.push(LOGO(x + 3, y + 3, 18));
      items.push(L(x + 24, y + 3, cw - 70, 8, "درمانگاه جامع آزادی طب", { pt: 17, bold: true, align: 0, dir: 0, textColor: "#fff" }));
      items.push(F(x + 24, y + 12, cw - 70, 5, "{clinicaddr}", { prefix: "نشانی: ", pt: 8, align: 0, dir: 0, textColor: "#cdd9f5" }));
      items.push(F(x + 24, y + 17, cw - 70, 5, "{clinicphone}", { prefix: "تلفن: ", pt: 8, align: 0, dir: 0, textColor: "#cdd9f5" }));
      items.push(QR(x + cw - 22, y + 1.5, 21)); y += 28;
      items.push(band(x, y, cw, 8, "#eef3ff", { corner: 1.5, borderColor: AC, borderWidth: 0.4 }));
      items.push(L(x + 3, y + 1.4, cw - 6, 5, "برگهٔ پذیرش، صورتحساب و علائم حیاتی", { pt: 11, bold: true, align: 1, dir: 2, textColor: AC })); y += 11;
      items.push(PHOTO(x + cw - 26, y, 26, 32));
      var iw = cw - 30, c2 = (iw - 4) / 2;
      cell(x + c2 + 4, y, c2, 12, "نام و نام خانوادگی", "{full}", { bc: AC }).forEach(function (e) { items.push(e); });
      cell(x, y, c2, 12, "کد ملی", "{nid}", { bc: AC }).forEach(function (e) { items.push(e); });
      cell(x + c2 + 4, y + 13, c2, 12, "نام پدر", "{father}", { bc: AC }).forEach(function (e) { items.push(e); });
      cell(x, y + 13, c2, 12, "تاریخ تولد / سن", "{age}", { bc: AC }).forEach(function (e) { items.push(e); });
      cell(x + c2 + 4, y + 26, c2, 12, "جنسیت", "{gender}", { bc: AC }).forEach(function (e) { items.push(e); });
      cell(x, y + 26, c2, 12, "تلفن همراه", "{mobile}", { bc: AC }).forEach(function (e) { items.push(e); }); y += 41;
      var c4 = (cw - 9) / 4;
      cell(x + 3 * (c4 + 3), y, c4, 11, "فشار خون", "{bp}", { bg: "#eef3ff" }).forEach(function (e) { items.push(e); });
      cell(x + 2 * (c4 + 3), y, c4, 11, "وزن", "{weight}", { bg: "#eef3ff" }).forEach(function (e) { items.push(e); });
      cell(x + c4 + 3, y, c4, 11, "قد", "{height}", { bg: "#eef3ff" }).forEach(function (e) { items.push(e); });
      cell(x, y, c4, 11, "بخش", "{dept}", { bg: "#eef3ff" }).forEach(function (e) { items.push(e); }); y += 14;
      items.push(L(x, y, cw, 5, "شرح خدمات و صورتحساب", { pt: 11, bold: true, align: 0, dir: 0, textColor: AC })); y += 6;
      items.push(svcTable(x, y, cw, 50, { borderColor: AC, rows: 8, widths: [0.45, 0.15, 0.2, 0.2],
        cells: [{ r: 0, c: 0, t: "شرح خدمت" }, { r: 0, c: 1, t: "تعداد" }, { r: 0, c: 2, t: "بهای واحد" }, { r: 0, c: 3, t: "مبلغ کل" }] })); y += 53;
      cell(x + 3 * (c4 + 3), y, c4, 12, "جمع کل", "{totalonly}").forEach(function (e) { items.push(e); });
      cell(x + 2 * (c4 + 3), y, c4, 12, "سهم بیمه", "{insshareonly}").forEach(function (e) { items.push(e); });
      cell(x + c4 + 3, y, c4, 12, "تخفیف", "{discount}").forEach(function (e) { items.push(e); });
      cell(x, y, c4, 12, "قابل پرداخت", "{paidonly}", { bg: "#dff0e3", color: "#0f7a3a" }).forEach(function (e) { items.push(e); }); y += 15;
      items.push(HL(x, y, cw, { borderColor: "#cdd7e6" })); y += 2;
      items.push(F(x, y, cw, 5, "{issued}", { pt: 8, align: 0, dir: 0, textColor: "#8a93a6" }));
      return items;
    })());
  })();

  /* ====================================================================== *
   *  APPOINTMENT (20)
   * ====================================================================== */
  (function () {
    var names = [
      "نوبت بزرگ A6", "کارت نوبت کلاسیک", "نوبت رول ۸ سانت", "نوبت با لوگو",
      "نوبت تخصصی پزشک", "نوبت با تاریخ و ساعت", "نوبت دندانپزشکی", "نوبت آزمایشگاه",
      "نوبت با QR", "کارت نوبت VIP", "نوبت ساده فوری", "نوبت با نام بیمار",
      "نوبت دو زبانه", "نوبت با شیفت", "نوبت کلینیک پوست", "نوبت اطفال",
      "نوبت با کادر رنگی", "نوبت بخش تخصصی", "نوبت رول ۵.۸", "نوبت جامع A5"
    ];
    for (var i = 0; i < 20; i++) {
      var paper = (i === 2) ? "R80" : (i === 18) ? "R58" : (i === 0) ? "A6" : (i === 19) ? "A5" : "A6";
      var dm = PAPER[paper] || (paper === "R80" ? [80, 200] : paper === "R58" ? [58, 200] : [105, 148]);
      var W = dm[0], H = dm[1], cw = W - 12, x = 6, items = [], y = 6;
      if (i === 1 || i === 16) items.push(FR(W, H));
      if (W >= 90 && (i === 3 || i === 4 || i === 9 || i === 19)) items.push(LOGO(x, y, 16));
      items.push(L(x, y, cw, 8, "درمانگاه آزادی طب", { pt: W >= 140 ? 14 : 11, bold: true, align: 1, textColor: "#1f4e8c" })); y += 9;
      items.push(L(x, y, cw, 5, "کارت نوبت", { pt: 9, align: 1, textColor: "#666" })); y += 6;
      items.push(HL(x, y, cw, { borderColor: "#1f4e8c", borderWidth: 0.7 })); y += 3;
      // big number
      var nh = (i === 0 || i === 9) ? 30 : 22;
      items.push(L(x, y, cw, 5, "شماره نوبت شما", { pt: 9, align: 1, textColor: "#777" })); y += 6;
      items.push(APPT(x, y, cw, nh, { pt: (i === 0 || i === 9) ? 48 : 36, textColor: "#1f4e8c", prefix: "" })); y += nh + 2;
      items.push(HL(x, y, cw, { borderColor: "#ccc" })); y += 3;
      if (i === 11 || i === 19 || i === 4) { items.push(pair(x, y, cw, "بیمار:", "{full}", { align: 1, pt: 9.5 })); y += 7; }
      if (i === 4 || i === 17 || i === 19) { items.push(pair(x, y, cw, "پزشک:", "{doctor}", { align: 1, pt: 9 })); y += 6.5; }
      if (i === 7) { items.push(pair(x, y, cw, "بخش:", "{dept}", { align: 1, pt: 9 })); y += 6.5; }
      items.push(F(x, y, cw / 2, 6, "{date}", { prefix: "تاریخ ", align: 0, pt: 9 }));
      items.push(F(x + cw / 2, y, cw / 2, 6, "{time}", { prefix: "ساعت ", align: 2, pt: 9 })); y += 7;
      if (i === 13) { items.push(pair(x, y, cw, "شیفت:", "{shift}", { align: 1, pt: 9 })); y += 6.5; }
      if (i === 8) { items.push(QR(x + cw / 2 - 9, y, 18)); y += 20; }
      items.push(L(x, y, cw, 5, "لطفاً تا اعلام شماره منتظر بمانید.", { pt: 8, align: 1, textColor: "#888" }));
      push("appointment", names[i], paper, 0, items);
    }
  })();

  /* ====================================================================== *
   *  LAB & RADIOLOGY (20)
   * ====================================================================== */
  (function () {
    var names = [
      "درخواست آزمایش A5", "جواب آزمایش A4", "سربرگ آزمایشگاه", "رادیولوژی درخواست",
      "سونوگرافی گزارش", "آزمایش با کادر نتایج", "پاتولوژی سربرگ", "آزمایش خون CBC",
      "رادیولوژی با عکس", "MRI درخواست", "CT اسکن گزارش", "آزمایش هورمونی",
      "آزمایش با QR و بارکد", "گزارش رادیولوژی A4", "سربرگ تخصصی آزمایشگاه", "درخواست تصویربرداری",
      "آزمایش با لوگو", "نتایج آزمایشگاه دو ستونه", "رول آزمایشگاه ۸ سانت", "آزمایشگاه جامع A4"
    ];
    for (var i = 0; i < 20; i++) {
      var isA4 = (i === 1 || i === 13 || i === 19 || i === 4 || i === 10);
      var paper = (i === 18) ? "R80" : isA4 ? "A4" : "A5";
      var dm = PAPER[paper] || (paper === "R80" ? [80, 200] : [148, 210]);
      var W = dm[0], H = dm[1], cw = W - 16, x = 8, items = [], y = 8;
      if (i === 2 || i === 14) items.push(FR(W, H));
      if (W >= 100) items.push(LOGO(x, y, 20));
      if (i === 12) items.push(QR(W - 28, y, 18));
      items.push(L(x + (W >= 100 ? 24 : 0), y, cw - (W >= 100 ? 24 : 0), 8, "آزمایشگاه و تصویربرداری آزادی طب", { pt: W >= 180 ? 16 : 12, bold: true, align: 1, textColor: "#0e6655" }));
      items.push(L(x, y + 9, cw, 5, /رادیو|MRI|CT|سونو|تصویر/.test(names[i]) ? "فرم تصویربرداری پزشکی" : "فرم آزمایشگاه تشخیص طبی", { pt: 9, align: 1, textColor: "#555" })); y += 17;
      items.push(HL(x, y, cw, { borderColor: "#0e6655", borderWidth: 0.8 })); y += 4;
      items.push(F(x, y, cw / 2, 7, "{date}", { prefix: "تاریخ ", pt: 9.5 }));
      items.push(F(x + cw / 2, y, cw / 2, 7, "{receiptNo}", { prefix: "شماره پذیرش ", align: 2, pt: 9.5 })); y += 8;
      items.push(pair(x, y, cw, "نام و نام خانوادگی:", "{full}", { bold: true, pt: 11 })); y += 8;
      items.push(pair(x, y, cw / 2, "کد ملی:", "{nid}")); items.push(pair(x + cw / 2, y, cw / 2, "سن/جنسیت:", "{gender}")); y += 7.5;
      items.push(pair(x, y, cw / 2, "پزشک معالج:", "{doctor}")); items.push(pair(x + cw / 2, y, cw / 2, "بخش:", "{dept}")); y += 7.5;
      items.push(pair(x, y, cw, "بیمه:", "{ins}")); y += 8;
      items.push(HL(x, y, cw)); y += 3;
      var areaH = isA4 ? (H - y - 40) : (H - y - 30);
      items.push(L(x, y, cw, 6, /رادیو|MRI|CT|سونو|تصویر/.test(names[i]) ? "شرح درخواست / گزارش تصویربرداری:" : "آزمایش‌های درخواستی / نتایج:", { bold: true, pt: 10, textColor: "#0e6655" })); y += 7;
      items.push(R(x, y, cw, areaH, { borderColor: "#0e6655", borderWidth: 0.6, corner: 2 }));
      if (i === 5 || i === 17) {
        items.push(VL(x + cw * 0.6, y, areaH, { borderColor: "#9bbfb6", borderWidth: 0.4 }));
        items.push(L(x + cw * 0.6 + 2, y + 2, cw * 0.4 - 4, 5, "نتیجه", { pt: 8, align: 1, textColor: "#0e6655" }));
        items.push(L(x + 2, y + 2, cw * 0.6 - 4, 5, "شرح آزمایش", { pt: 8, align: 1, textColor: "#0e6655" }));
      }
      y += areaH + 3;
      items.push(HL(x, y, cw, { borderColor: "#bbb" })); y += 2;
      items.push(L(x, y, cw / 2, 5, "مهر و امضای مسئول فنی", { pt: 8, align: 0, textColor: "#777" }));
      items.push(F(x + cw / 2, y, cw / 2, 5, "{issued}", { pt: 8, align: 2, textColor: "#777" }));
      push("lab", names[i], paper, 0, items);
    }
  })();

  /* ====================================================================== *
   *  PREMIUM — richly detailed forms modelled on real Iranian clinics.
   *  Coloured header bands, info boxes, service tables, signature blocks.
   * ====================================================================== */
  function BAND(x, y, w, h, color, o) {
    return mk(Object.assign({ type: "rect", x: x, y: y, w: w, h: h, fillTransparent: false,
      fillColor: color, borderWidth: 0, borderColor: color, corner: (o && o.corner) || 1.5 }, o || {}));
  }
  // a labelled cell with a soft box (label small grey on top, value bold below)
  function CELL(x, y, w, h, labelTxt, field, o) {
    o = o || {};
    var arr = [];
    arr.push(R(x, y, w, h, { borderColor: "#cdd7e6", borderWidth: 0.4, corner: 1.2,
      fillTransparent: false, fillColor: o.bg || "#f7faff" }));
    arr.push(L(x + 2, y + 1.2, w - 4, 4, labelTxt, { pt: 7.5, align: 0, dir: 0, textColor: "#7a879c" }));
    if (field) arr.push(F(x + 2, y + 5, w - 4, 5.5, field, { pt: o.pt || 9.5, bold: o.bold !== false, align: 0, dir: 0, textColor: o.color || "#16233a" }));
    else arr.push(L(x + 2, y + 5, w - 4, 5.5, o.text || "", { pt: o.pt || 9.5, bold: o.bold !== false, align: 0, dir: 0, textColor: o.color || "#16233a" }));
    return arr;
  }
  function tableJson(cols, rows, header, widths) {
    return JSON.stringify({ cols: cols, rows: rows, header: !!header,
      widths: widths || null, cells: [] });
  }
  function TABLE(x, y, w, h, cols, rows, o) {
    o = o || {};
    return mk(Object.assign({ type: "table", x: x, y: y, w: w, h: h,
      borderColor: o.borderColor || "#2b5f9e", borderWidth: 0.5,
      text: o.text || tableJson(cols, rows, o.header !== false, o.widths) }, o));
  }

  (function () {
    // ---- 1. Premium reception A5 (blue, با کادر و جدول خدمات) -------------
    (function () {
      var W = 148, H = 210, x = 9, cw = W - 18, items = [], y = 8;
      items.push(FR(W, H));
      items.push(BAND(x, y, cw, 22, "#1f4e8c", { corner: 2 }));
      items.push(LOGO(x + 2, y + 2, 18));
      items.push(L(x + 22, y + 2.5, cw - 24, 8, "درمانگاه شبانه‌روزی آزادی طب", { pt: 14, bold: true, align: 0, dir: 0, textColor: "#ffffff" }));
      items.push(L(x + 22, y + 11, cw - 24, 5, "{clinicaddr}", { field: "{clinicaddr}", type: "field", prefix: "نشانی: ", pt: 7.5, align: 0, dir: 0, textColor: "#dbe7fb" }));
      items.push(F(x + 22, y + 16, cw - 24, 5, "{clinicphone}", { prefix: "تلفن: ", pt: 7.5, align: 0, dir: 0, textColor: "#dbe7fb" }));
      y += 24;
      items.push(L(x, y, cw, 6, "برگهٔ پذیرش بیمار", { pt: 11, bold: true, align: 1, dir: 2, textColor: "#1f4e8c" }));
      items.push(F(x, y + 1, 36, 5, "{receiptNo}", { prefix: "شماره: ", pt: 8.5, align: 0, dir: 0, textColor: "#555" }));
      items.push(F(x + cw - 40, y + 1, 40, 5, "{date}", { prefix: "تاریخ ", pt: 8.5, align: 2, dir: 1, textColor: "#555" }));
      y += 9;
      // patient info cells (2 columns)
      var colW = (cw - 4) / 2;
      function row(lbl1, f1, lbl2, f2) {
        CELL(x, y, colW, 11, lbl1, f1).forEach(function (e) { items.push(e); });
        if (lbl2) CELL(x + colW + 4, y, colW, 11, lbl2, f2).forEach(function (e) { items.push(e); });
        y += 13;
      }
      row("نام و نام خانوادگی", "{full}", "کد ملی", "{nid}");
      row("نام پدر", "{father}", "تاریخ تولد", "{birth}");
      row("جنسیت", "{gender}", "تلفن همراه", "{mobile}");
      row("بیمهٔ پایه", "{ins}", "بیمهٔ مکمل", "{supp}");
      // services table
      items.push(L(x, y, cw, 5, "شرح خدمات", { pt: 9.5, bold: true, align: 0, dir: 0, textColor: "#1f4e8c" })); y += 6;
      items.push(TABLE(x, y, cw, 26, 3, 4, { header: true, widths: [0.5, 0.25, 0.25],
        text: JSON.stringify({ cols: 3, rows: 4, header: true, widths: [0.5, 0.25, 0.25],
          cells: [{ r: 0, c: 0, t: "شرح خدمت" }, { r: 0, c: 1, t: "تعداد" }, { r: 0, c: 2, t: "مبلغ (ریال)" }] }) })); y += 28;
      // totals
      var tw = (cw - 6) / 3;
      CELL(x, y, tw, 11, "جمع کل", "{totalonly}", { bg: "#eef4ff" }).forEach(function (e) { items.push(e); });
      CELL(x + tw + 3, y, tw, 11, "سهم بیمه", "{insshareonly}", { bg: "#eef4ff" }).forEach(function (e) { items.push(e); });
      CELL(x + 2 * (tw + 3), y, tw, 11, "قابل پرداخت", "{paidonly}", { bg: "#dff0e3", color: "#0f7a3a", bold: true }).forEach(function (e) { items.push(e); });
      y += 14;
      // queue badge + footer
      items.push(BAND(x, y, cw * 0.55, 12, "#eaf1fc", { corner: 2, borderColor: "#1f4e8c", borderWidth: 0.5 }));
      items.push(F(x, y + 2.5, cw * 0.55, 7, "{queue}", { prefix: "نوبت شما: ", bold: true, pt: 13, align: 1, dir: 2, textColor: "#1f4e8c" }));
      items.push(QR(x + cw - 18, y - 4, 18));
      y += 16;
      items.push(HL(x, y, cw, { borderColor: "#cdd7e6" })); y += 1.5;
      items.push(F(x, y, cw, 4.5, "{issued}", { pt: 7.5, align: 0, dir: 0, textColor: "#8a93a6" }));
      items.push(L(x, y, cw, 4.5, "نسخهٔ بیمار — لطفاً تا پایان درمان نزد خود نگه دارید.", { pt: 7, align: 2, dir: 0, textColor: "#9aa3b4" }));
      push("reception", "★ پذیرش حرفه‌ای آبی (جدول خدمات)", "A5", 0, items);
    })();

    // ---- 2. Premium A4 hospital admission (دو ستونه با کادر) -------------
    (function () {
      var W = 210, H = 297, x = 12, cw = W - 24, items = [], y = 12;
      items.push(BAND(x, y, cw, 26, "#0e4d6e", { corner: 2 }));
      items.push(LOGO(x + 3, y + 3, 20));
      items.push(L(x + 26, y + 3.5, cw - 28, 9, "مرکز درمانی و تخصصی آزادی طب", { pt: 18, bold: true, align: 0, dir: 0, textColor: "#ffffff" }));
      items.push(L(x + 26, y + 13, cw - 28, 5, "پذیرش، درمانگاه تخصصی، تصویربرداری و آزمایشگاه", { pt: 9, align: 0, dir: 0, textColor: "#cfe6f0" }));
      items.push(F(x + 26, y + 19, cw - 28, 5, "{clinicphone}", { prefix: "تلفن: ", pt: 8.5, align: 0, dir: 0, textColor: "#cfe6f0" }));
      items.push(QR(x + cw - 22, y + 2, 22));
      y += 30;
      items.push(BAND(x, y, cw, 9, "#eaf3f7", { corner: 1.5, borderColor: "#0e4d6e", borderWidth: 0.4 }));
      items.push(L(x + 3, y + 1.8, cw - 6, 5, "برگهٔ پذیرش و صورتحساب بیمار", { pt: 12, bold: true, align: 1, dir: 2, textColor: "#0e4d6e" }));
      y += 12;
      var colW = (cw - 6) / 2;
      function rowA(lbl1, f1, lbl2, f2) {
        CELL(x, y, colW, 12, lbl1, f1).forEach(function (e) { items.push(e); });
        if (lbl2) CELL(x + colW + 6, y, colW, 12, lbl2, f2).forEach(function (e) { items.push(e); });
        y += 14;
      }
      rowA("نام و نام خانوادگی", "{full}", "کد ملی", "{nid}");
      rowA("نام پدر", "{father}", "تاریخ تولد / سن", "{age}");
      rowA("جنسیت", "{gender}", "تلفن همراه", "{mobile}");
      rowA("پزشک معالج", "{doctor}", "بخش / دپارتمان", "{dept}");
      rowA("بیمهٔ پایه", "{ins}", "بیمهٔ مکمل", "{supp}");
      rowA("نشانی", "{address}", "نوع پذیرش", "{ptype}");
      y += 1;
      items.push(L(x, y, cw, 5, "شرح خدمات و صورتحساب", { pt: 11, bold: true, align: 0, dir: 0, textColor: "#0e4d6e" })); y += 6;
      items.push(TABLE(x, y, cw, 46, 4, 7, { borderColor: "#0e4d6e", header: true, widths: [0.45, 0.15, 0.2, 0.2],
        text: JSON.stringify({ cols: 4, rows: 7, header: true, widths: [0.45, 0.15, 0.2, 0.2],
          cells: [{ r: 0, c: 0, t: "شرح خدمت" }, { r: 0, c: 1, t: "تعداد" }, { r: 0, c: 2, t: "بهای واحد" }, { r: 0, c: 3, t: "مبلغ کل" }] }) })); y += 49;
      var tw = (cw - 9) / 4;
      CELL(x, y, tw, 12, "جمع کل", "{totalonly}").forEach(function (e) { items.push(e); });
      CELL(x + tw + 3, y, tw, 12, "سهم بیمه", "{insshareonly}").forEach(function (e) { items.push(e); });
      CELL(x + 2 * (tw + 3), y, tw, 12, "تخفیف", "{discount}").forEach(function (e) { items.push(e); });
      CELL(x + 3 * (tw + 3), y, tw, 12, "قابل پرداخت", "{paidonly}", { bg: "#dff0e3", color: "#0f7a3a" }).forEach(function (e) { items.push(e); });
      y += 16;
      items.push(HL(x, y, cw, { borderColor: "#cdd7e6" })); y += 3;
      items.push(L(x, y, colW, 6, "مهر و امضای پذیرش", { pt: 9, align: 0, dir: 0, textColor: "#6b7689" }));
      items.push(L(x + colW + 6, y, colW, 6, "مهر و امضای صندوق", { pt: 9, align: 2, dir: 0, textColor: "#6b7689" }));
      y += 8;
      items.push(F(x, y, cw, 5, "{issued}", { pt: 8, align: 0, dir: 0, textColor: "#9aa3b4" }));
      push("reception", "★ پذیرش بیمارستانی A4 (کامل)", "A4", 0, items);
    })();

    // ---- 3. Premium thermal receipt R80 (رول حرارتی حرفه‌ای) -------------
    (function () {
      var W = 80, H = 200, x = 4, cw = W - 8, items = [], y = 5;
      items.push(L(x, y, cw, 7, "درمانگاه آزادی طب", { pt: 13, bold: true, align: 1, dir: 2, textColor: "#000" })); y += 8;
      items.push(L(x, y, cw, 4, "{clinicaddr}", { field: "{clinicaddr}", type: "field", pt: 7, align: 1, dir: 2, textColor: "#333" })); y += 4.5;
      items.push(F(x, y, cw, 4, "{clinicphone}", { prefix: "تلفن: ", pt: 7, align: 1, dir: 2, textColor: "#333" })); y += 5;
      items.push(HL(x, y, cw, { borderColor: "#000", borderWidth: 0.4 })); y += 2;
      items.push(F(x, y, cw / 2, 5, "{receiptNo}", { prefix: "قبض ", pt: 8, align: 0, dir: 0 }));
      items.push(F(x + cw / 2, y, cw / 2, 5, "{date}", { prefix: "", pt: 8, align: 2, dir: 1 })); y += 6;
      items.push(F(x, y, cw, 5, "{full}", { prefix: "بیمار: ", bold: true, pt: 9, align: 0, dir: 0 })); y += 5.5;
      items.push(F(x, y, cw, 5, "{nid}", { prefix: "کد ملی: ", pt: 8, align: 0, dir: 0 })); y += 5.5;
      items.push(F(x, y, cw, 5, "{ins}", { prefix: "بیمه: ", pt: 8, align: 0, dir: 0 })); y += 6;
      items.push(HL(x, y, cw, { borderColor: "#000", borderWidth: 0.3, borderStyle: 1 })); y += 2;
      items.push(TABLE(x, y, cw, 24, 2, 4, { borderColor: "#000", header: true, widths: [0.65, 0.35],
        text: JSON.stringify({ cols: 2, rows: 4, header: true, widths: [0.65, 0.35],
          cells: [{ r: 0, c: 0, t: "شرح خدمت" }, { r: 0, c: 1, t: "مبلغ" }] }) })); y += 26;
      items.push(HL(x, y, cw, { borderColor: "#000", borderWidth: 0.3 })); y += 2;
      items.push(F(x, y, cw, 5, "{total}", { prefix: "جمع کل: ", pt: 8, align: 0, dir: 0 })); y += 5;
      items.push(F(x, y, cw, 5, "{insshare}", { prefix: "سهم بیمه: ", pt: 8, align: 0, dir: 0 })); y += 5;
      items.push(F(x, y, cw, 6, "{paid}", { prefix: "قابل پرداخت: ", bold: true, pt: 10, align: 0, dir: 0 })); y += 7;
      items.push(R(x, y, cw, 10, { borderColor: "#000", borderWidth: 0.5, corner: 1 }));
      items.push(F(x, y + 2, cw, 6, "{queue}", { prefix: "نوبت: ", bold: true, pt: 12, align: 1, dir: 2 })); y += 12;
      items.push(QR(x + cw / 2 - 9, y, 18)); y += 20;
      items.push(L(x, y, cw, 4, "از انتخاب شما سپاسگزاریم.", { pt: 7.5, align: 1, dir: 2, textColor: "#333" }));
      push("reception", "★ رسید حرارتی حرفه‌ای ۸ سانت", "R80", 0, items);
    })();

    // ---- 4. Premium appointment card A6 (کارت نوبت لوکس) ----------------
    (function () {
      var W = 105, H = 148, x = 7, cw = W - 14, items = [], y = 7;
      items.push(FR(W, H));
      items.push(BAND(x, y, cw, 18, "#6d28d9", { corner: 2 }));
      items.push(LOGO(x + 2, y + 2, 14));
      items.push(L(x + 18, y + 2.5, cw - 20, 7, "درمانگاه آزادی طب", { pt: 12, bold: true, align: 0, dir: 0, textColor: "#fff" }));
      items.push(L(x + 18, y + 10, cw - 20, 5, "کارت نوبت", { pt: 8, align: 0, dir: 0, textColor: "#e9d8fd" }));
      y += 21;
      items.push(L(x, y, cw, 5, "شماره نوبت شما", { pt: 9, align: 1, dir: 2, textColor: "#7a879c" })); y += 6;
      items.push(BAND(x + cw / 2 - 22, y, 44, 26, "#f5f0ff", { corner: 3, borderColor: "#6d28d9", borderWidth: 0.6 }));
      items.push(APPT(x + cw / 2 - 22, y + 1, 44, 24, { pt: 40, textColor: "#6d28d9", dir: 2, prefix: "" })); y += 29;
      items.push(HL(x, y, cw, { borderColor: "#ddd" })); y += 3;
      items.push(F(x, y, cw, 6, "{full}", { prefix: "بیمار: ", bold: true, pt: 10, align: 1, dir: 2 })); y += 7;
      items.push(F(x, y, cw, 5.5, "{doctor}", { prefix: "پزشک: ", pt: 9, align: 1, dir: 2, textColor: "#444" })); y += 6.5;
      items.push(F(x, y, cw / 2, 5.5, "{apptdate}", { prefix: "تاریخ ", pt: 9, align: 0, dir: 0 }));
      items.push(F(x + cw / 2, y, cw / 2, 5.5, "{appttime}", { prefix: "ساعت ", pt: 9, align: 2, dir: 1 })); y += 7;
      items.push(QR(x + cw / 2 - 9, y, 16)); y += 18;
      items.push(L(x, y, cw, 4.5, "لطفاً تا اعلام شماره منتظر بمانید.", { pt: 7.5, align: 1, dir: 2, textColor: "#888" }));
      push("appointment", "★ کارت نوبت لوکس (بنفش)", "A6", 0, items);
    })();

    // ---- 5. Premium lab request A5 (آزمایشگاه با جدول) -----------------
    (function () {
      var W = 148, H = 210, x = 9, cw = W - 18, items = [], y = 8;
      items.push(FR(W, H));
      items.push(BAND(x, y, cw, 20, "#0e6655", { corner: 2 }));
      items.push(LOGO(x + 2, y + 2, 16));
      items.push(L(x + 20, y + 2.5, cw - 22, 7, "آزمایشگاه تشخیص طبی آزادی طب", { pt: 12, bold: true, align: 0, dir: 0, textColor: "#fff" }));
      items.push(L(x + 20, y + 10, cw - 22, 5, "فرم درخواست و جوابدهی آزمایش", { pt: 8, align: 0, dir: 0, textColor: "#cdeae3" }));
      items.push(QR(x + cw - 18, y + 1, 17));
      y += 23;
      items.push(F(x, y, cw / 2, 6, "{date}", { prefix: "تاریخ ", pt: 9, align: 0, dir: 0 }));
      items.push(F(x + cw / 2, y, cw / 2, 6, "{receiptNo}", { prefix: "شماره پذیرش ", pt: 9, align: 2, dir: 1 })); y += 7;
      var colW = (cw - 4) / 2;
      function rowL(lbl1, f1, lbl2, f2) {
        CELL(x, y, colW, 11, lbl1, f1).forEach(function (e) { items.push(e); });
        if (lbl2) CELL(x + colW + 4, y, colW, 11, lbl2, f2).forEach(function (e) { items.push(e); });
        y += 13;
      }
      rowL("نام و نام خانوادگی", "{full}", "کد ملی", "{nid}");
      rowL("سن", "{age}", "جنسیت", "{gender}");
      rowL("پزشک درخواست‌کننده", "{doctor}", "بیمه", "{ins}");
      items.push(L(x, y, cw, 5, "آزمایش‌های درخواستی / نتایج", { pt: 9.5, bold: true, align: 0, dir: 0, textColor: "#0e6655" })); y += 6;
      items.push(TABLE(x, y, cw, 46, 4, 8, { borderColor: "#0e6655", header: true, widths: [0.4, 0.2, 0.2, 0.2],
        text: JSON.stringify({ cols: 4, rows: 8, header: true, widths: [0.4, 0.2, 0.2, 0.2],
          cells: [{ r: 0, c: 0, t: "نام آزمایش" }, { r: 0, c: 1, t: "نتیجه" }, { r: 0, c: 2, t: "واحد" }, { r: 0, c: 3, t: "محدودهٔ مرجع" }] }) })); y += 49;
      items.push(HL(x, y, cw, { borderColor: "#bbb" })); y += 2;
      items.push(L(x, y, cw / 2, 5, "مهر و امضای مسئول فنی", { pt: 8, align: 0, dir: 0, textColor: "#777" }));
      items.push(F(x + cw / 2, y, cw / 2, 5, "{issued}", { pt: 8, align: 2, dir: 0, textColor: "#777" }));
      push("lab", "★ آزمایشگاه حرفه‌ای (جدول نتایج)", "A5", 0, items);
    })();
  })();

  /* ====================================================================== *
   *  v1.44.0 §5 — new premium reception templates that showcase the
   *  professional TABLE element (services subtype + zebra + header bg) and the
   *  new billing/insurance tokens ({{svc.row.*}}, {{bill.*}}, {{ins.*}}).
   * ====================================================================== */
  (function () {
    // services table model (repeating body row) — matches the C++ pdParseTable shape.
    function svcTableJson() {
      return JSON.stringify({
        cols: 5, rows: 2, header: true, kind: "services", stripe: true,
        headerBg: "#eef2fb", padding: 6, border: { w: 1, color: "#2b5f9e" },
        widths: [1, 4, 1, 2, 2], colAlign: ["center", "right", "center", "left", "left"],
        cells: [["ردیف", "شرح خدمت", "تعداد", "سهم بیمه", "سهم بیمار"],
                ["{{svc.row.idx}}", "{{svc.row.name}}", "{{svc.row.qty}}", "{{svc.row.insShare}}", "{{svc.row.patShare}}"]]
      });
    }
    function billTableJson() {
      return JSON.stringify({
        cols: 2, rows: 6, header: false, kind: "static", stripe: true,
        headerBg: "#eef2fb", padding: 5, border: { w: 1, color: "#2b5f9e" },
        widths: [3, 2], colAlign: ["right", "left"],
        cells: [["جمع کل", "{{bill.gross}}"], ["تخفیف", "{{bill.disc}}"],
                ["بیمهٔ پایه ({{ins.base.pct}})", "{{bill.org}}"],
                ["بیمهٔ تکمیلی ({{ins.supp.pct}})", "{{bill.supp}}"],
                ["سهم بیمار", "{{bill.pat}}"], ["تاریخ", "{{appt.date}}"]]
      });
    }
    // T-A: full professional bill A5 with services table + insurance summary.
    (function () {
      var W = 148, H = 210, x = 9, cw = W - 18, items = [], y = 8;
      items.push(FR(W, H));
      items.push(BAND(x, y, cw, 20, "#1f4e8c", { corner: 2 }));
      items.push(L(x + 2, y + 2.5, cw - 4, 8, "درمانگاه آزادی طب", { pt: 14, bold: true, align: 1, dir: 2, textColor: "#fff" }));
      items.push(L(x + 2, y + 11, cw - 4, 5, "صورتحساب پذیرش", { pt: 8, align: 1, dir: 2, textColor: "#dbe7fb" }));
      y += 23;
      items.push(F(x, y, cw / 2, 6, "{{patient.fullname}}", { prefix: "بیمار: ", bold: true, pt: 10, align: 0, dir: 0 }));
      items.push(F(x + cw / 2, y, cw / 2, 6, "{{appt.no}}", { prefix: "شماره نوبت: ", pt: 10, align: 2, dir: 1 })); y += 8;
      items.push(L(x, y, cw, 5, "بیمهٔ پایه: {{ins.base.name}} ({{ins.base.pct}}) — تکمیلی: {{ins.supp.name}} ({{ins.supp.pct}})", { pt: 8, align: 0, dir: 0, textColor: "#555" })); y += 7;
      items.push(TABLE(x, y, cw, 44, 5, 2, { borderColor: "#2b5f9e", text: svcTableJson() })); y += 47;
      items.push(TABLE(x, y, cw, 40, 2, 6, { borderColor: "#2b5f9e", text: billTableJson() }));
      push("reception", "★ صورتحساب حرفه‌ای (جدول خدمات + بیمه)", "A5", 0, items);
    })();
    // T-B: thermal 80mm bill with services table.
    (function () {
      var W = 80, H = 200, x = 4, cw = W - 8, items = [], y = 5;
      items.push(L(x, y, cw, 7, "درمانگاه آزادی طب", { pt: 13, bold: true, align: 1, dir: 2, textColor: "#000" })); y += 8;
      items.push(HL(x, y, cw, { borderColor: "#000", borderWidth: 0.4 })); y += 2;
      items.push(F(x, y, cw, 5, "{{patient.fullname}}", { prefix: "بیمار: ", bold: true, pt: 9, align: 0, dir: 0 })); y += 6;
      items.push(TABLE(x, y, cw, 40, 5, 2, { borderColor: "#000", text: svcTableJson() })); y += 43;
      items.push(F(x, y, cw, 6, "{{bill.pat}}", { prefix: "قابل پرداخت: ", bold: true, pt: 11, align: 0, dir: 0 }));
      push("reception", "★ رسید حرارتی ۸ سانت (جدول خدمات)", "R80", 0, items);
    })();
    // T-C: A4 detailed invoice with large services table.
    (function () {
      var W = 210, H = 297, x = 14, cw = W - 28, items = [], y = 12;
      items.push(FR(W, H));
      items.push(BAND(x, y, cw, 22, "#0e6655", { corner: 2 }));
      items.push(L(x + 2, y + 3, cw - 4, 9, "درمانگاه آزادی طب", { pt: 18, bold: true, align: 1, dir: 2, textColor: "#fff" }));
      items.push(L(x + 2, y + 13, cw - 4, 6, "صورتحساب تفصیلی خدمات", { pt: 10, align: 1, dir: 2, textColor: "#cdeae3" }));
      y += 26;
      items.push(F(x, y, cw / 2, 7, "{{patient.fullname}}", { prefix: "بیمار: ", bold: true, pt: 12, align: 0, dir: 0 }));
      items.push(F(x + cw / 2, y, cw / 2, 7, "{{patient.nid}}", { prefix: "کد ملی: ", pt: 12, align: 2, dir: 1 })); y += 10;
      items.push(TABLE(x, y, cw, 120, 5, 2, { borderColor: "#0e6655", text: svcTableJson() })); y += 124;
      items.push(TABLE(x, y, cw, 48, 2, 6, { borderColor: "#0e6655", text: billTableJson() }));
      push("reception", "★ صورتحساب تفصیلی A4 (جدول بزرگ)", "A4", 0, items);
    })();
  })();

  window.AZ_TEMPLATES = ALL;
})();
