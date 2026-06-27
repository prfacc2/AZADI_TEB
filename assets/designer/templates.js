/* ===========================================================================
   templates.js — Azadi-Teb ready-made professional print templates (v1.20.0)
   60 templates total, grouped:
     • reception   (20) — admission slips / receipts / bills
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
      font: "Vazirmatn", pt: 11, bold: false, italic: false, align: 0, lineSpacing: 1.25,
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
   *  RECEPTION (20)
   * ====================================================================== */
  (function () {
    var names = [
      "پذیرش کلاسیک A5", "پذیرش رسمی با حاشیه", "پذیرش فشرده رول ۸", "پذیرش دو ستونه",
      "پذیرش با لوگو و QR", "پذیرش بیمارستانی A4", "پذیرش درمانگاه تخصصی", "پذیرش دندانپزشکی",
      "پذیرش با عکس بیمار", "صورتحساب تفکیکی", "رسید پرداخت ساده", "پذیرش شبانه‌روزی",
      "پذیرش با کادر اطلاعات", "پذیرش مدرن آبی", "پذیرش بیمه پایه و مکمل", "پذیرش سرپایی فوری",
      "قبض پذیرش رول ۵.۸", "پذیرش با شرح خدمت", "پذیرش کلینیک زیبایی", "پذیرش جامع A4"
    ];
    for (var i = 0; i < 20; i++) {
      var paper = (i === 2 || i === 16) ? "R80" : (i === 5 || i === 19) ? "A4" : "A5";
      var dm = PAPER[paper] || (paper === "R80" ? [80, 200] : [148, 210]);
      var W = dm[0], H = dm[1], cw = W - 16, x = 8, items = [];
      var y = 8;
      if (i === 1 || i === 12 || i === 13) items.push(FR(W, H));
      // header
      if (W >= 100) items.push(LOGO(x, y, 20));
      items.push(L(W >= 100 ? x + 24 : x, y, cw - (W >= 100 ? 24 : 0), 9, "درمانگاه آزادی طب", { pt: W >= 180 ? 18 : 14, bold: true, align: 1, textColor: "#1f4e8c" }));
      items.push(L(x, y + 9, cw, 6, "برگهٔ پذیرش بیمار", { pt: 10, align: 1, textColor: "#555" }));
      if (i === 4 || i === 14) items.push(QR(W - 28, y, 18));
      y += 18;
      items.push(HL(x, y, cw, { borderColor: "#1f4e8c", borderWidth: 0.8 })); y += 4;
      // date/time row
      items.push(F(x, y, cw / 2, 7, "{date}", { prefix: "تاریخ ", align: 0, pt: 9.5 }));
      items.push(F(x + cw / 2, y, cw / 2, 7, "{time}", { prefix: "ساعت ", align: 2, pt: 9.5 })); y += 8;
      // patient block
      if (i === 8) { items.push(PHOTO(W - 30, y, 22, 28)); cw -= 26; }
      items.push(pair(x, y, cw, "نام و نام خانوادگی:", "{full}", { bold: true, pt: 11 })); y += 8;
      items.push(pair(x, y, cw / 2, "کد ملی:", "{nid}")); items.push(pair(x + cw / 2, y, cw / 2, "نام پدر:", "{father}")); y += 7.5;
      items.push(pair(x, y, cw / 2, "تاریخ تولد:", "{birth}")); items.push(pair(x + cw / 2, y, cw / 2, "جنسیت:", "{gender}")); y += 7.5;
      items.push(pair(x, y, cw, "تلفن همراه:", "{mobile}")); y += 7.5;
      if (i === 14 || i === 19) { items.push(pair(x, y, cw / 2, "بیمه:", "{ins}")); items.push(pair(x + cw / 2, y, cw / 2, "بیمه مکمل:", "{supp}")); y += 7.5; }
      else { items.push(pair(x, y, cw, "بیمه:", "{ins}")); y += 7.5; }
      cw = W - 16;
      items.push(HL(x, y, cw)); y += 3;
      // bill
      if (i !== 10 && i !== 16) {
        items.push(L(x, y, cw, 6, "صورتحساب", { bold: true, pt: 10, textColor: "#1f4e8c" })); y += 7;
        if (i === 9 || i === 19 || i === 5) {
          items.push(pair(x, y, cw, "شرح خدمت:", "{service}")); y += 7;
          items.push(pair(x, y, cw / 2, "جمع کل:", "{total}")); items.push(pair(x + cw / 2, y, cw / 2, "سهم بیمه:", "{insshare}")); y += 7;
          items.push(pair(x, y, cw / 2, "تخفیف:", "{discount}")); items.push(pair(x + cw / 2, y, cw / 2, "پرداختی:", "{paid}", { bold: true })); y += 8;
        } else {
          items.push(pair(x, y, cw / 2, "جمع کل:", "{total}")); items.push(pair(x + cw / 2, y, cw / 2, "پرداختی:", "{paid}", { bold: true })); y += 8;
        }
      }
      // queue
      items.push(R(x, y, cw, 12, { borderColor: "#1f4e8c", borderWidth: 0.8, corner: 2 }));
      items.push(F(x + 2, y + 2.5, cw - 4, 7, "{queue}", { prefix: "شماره نوبت: ", bold: true, pt: 12, align: 1, textColor: "#1f4e8c" })); y += 14;
      // footer
      items.push(HL(x, y, cw, { borderColor: "#bbb" })); y += 2;
      items.push(F(x, y, cw, 5, "{issued}", { pt: 8, align: 0, textColor: "#777" }));
      items.push(L(x, y, cw, 5, "نشانی: تهران، خیابان آزادی — تلفن: ۰۲۱-۰۰۰۰۰۰۰۰", { pt: 7.5, align: 2, textColor: "#999" }));
      push("reception", names[i], paper, 0, items);
    }
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

  window.AZ_TEMPLATES = ALL;
})();
