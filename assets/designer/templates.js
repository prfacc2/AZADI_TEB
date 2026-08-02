/* ===========================================================================
   templates.js — Azadi-Teb ready-made print designs  (v1.60.0 — FULL REBUILD)

   ★ این فایل دقیقاً همان ۳۰ قالب داخلی C++ (print_designer_templates.inc) است
     تا گالری دیزاینر وب با آنچه موتور چاپ واقعاً seed می‌کند یکی باشد.

   مرکز هر ۳۰ قالب: جدول پویای خدمات (type:"services") که هنگام چاپ از
   ReceptionRecord.services پر می‌شود — نام خدمت | شرح خدمت | تعداد، سطر به سطر
   و زنده، مستقیم از مدیریت ← خدمات. نه لیبل ثابت، نه متن آزمایشی.
   =========================================================================== */
(function () {
  "use strict";

  var INK      = "#000000";
  var RULE     = "#000000";
  var RULE_DIM = "#8a8a8a";
  var BAND     = "#e9e9e9";
  var PAPER_BG = "#ffffff";

  var _uid = 0;
  function nid() { return ++_uid; }

  function base() {
    return {
      id: nid(), type: "label", x: 10, y: 10, w: 40, h: 7, rot: 0, z: 1,
      locked: false, isFrame: false, text: "", field: "", prefix: "", suffix: "",
      font: "Vazirmatn", pt: 10, bold: false, italic: false,
      align: 0, dir: 0, valign: 1, lineSpacing: 1.25,
      textColor: INK, fillColor: PAPER_BG, fillTransparent: true,
      borderColor: RULE, borderWidth: 0.3, corner: 0, padding: 0.8, opacity: 1,
      visibility: 0, startValue: 1, step: 1, imgPath: "",
      rowH: 0, headerH: 0
    };
  }
  function mk(o) { return Object.assign(base(), o); }

  /* ---------------------------------------------------------- primitives */
  function L(x, y, w, h, text, pt, bold, align, o) {
    return mk(Object.assign({ type: "label", x: x, y: y, w: w, h: h, text: text,
      pt: pt, bold: !!bold, align: align, dir: (align === 1 ? 2 : 0) }, o || {}));
  }
  function F(x, y, w, h, field, prefix, pt, align, o) {
    return mk(Object.assign({ type: "field", x: x, y: y, w: w, h: h,
      field: field, prefix: prefix || "", pt: pt, align: align,
      dir: (align === 1 ? 2 : 0) }, o || {}));
  }
  function HL(x, y, w, bw) {
    return mk({ type: "hline", x: x, y: y, w: w, h: 0.2, borderWidth: bw });
  }
  function FRAME(pw, ph, m, col, bw) {
    return mk({ type: "frame", x: m, y: m, w: pw - 2 * m, h: ph - 2 * m,
      isFrame: true, borderColor: col, borderWidth: bw, fillTransparent: true });
  }
  function LOGO(x, y, w, h) {
    return mk({ type: "logo", x: x, y: y, w: w, h: h,
      borderColor: RULE_DIM, borderWidth: 0.3 });
  }
  function QR(x, y, s) {
    return mk({ type: "qr", x: x, y: y, w: s, h: s, field: "receiptNo",
      borderColor: RULE_DIM, borderWidth: 0.3 });
  }
  function PHOTO(x, y, w, h) {
    return mk({ type: "photo", x: x, y: y, w: w, h: h,
      borderColor: RULE_DIM, borderWidth: 0.3 });
  }
  function BARCODE(x, y, w, h) {
    return mk({ type: "barcode", x: x, y: y, w: w, h: h,
      field: "receiptbarcode",
      text: JSON.stringify({ sym: "code128", hri: true, quiet: 2 }),
      pt: 8, align: 1, dir: 1, textColor: INK, borderWidth: 0 });
  }
  function APPTNO(x, y, w, h, pt) {
    return mk({ type: "apptno", x: x, y: y, w: w, h: h, pt: pt, bold: true,
      align: 1, dir: 1, startValue: 1, step: 1 });
  }

  /* ----------------------------------------- THE CORE: live services table */
  // ستون‌ها (RTL، ستون ۰ سمت راست): نام خدمت | شرح خدمت | تعداد
  // headerH/rowH قفل‌شده (میلی‌متر) تا ~۱۰ خدمت تمیز چاپ شود؛ در صورت نیاز
  // جدول به‌طور یکنواخت کوچک می‌شود تا در قاب خود جا شود.
  function SERVICES(x, y, w, h, pt, banded, bw) {
    return mk({
      type: "services", x: x, y: y, w: w, h: h, pt: pt, align: 1, dir: 2,
      borderColor: RULE, borderWidth: bw, textColor: INK,
      fillColor: banded ? BAND : PAPER_BG, fillTransparent: !banded,
      padding: 0.8, headerH: 7.5, rowH: 7.0,
      text: JSON.stringify({ cols: 3, header: true,
        widths: [0.55, 0.30, 0.15],
        labels: ["نام خدمت", "شرح خدمت", "تعداد"] })
    });
  }

  /* ------------------------------------------------------------- captions */
  var FA_CLINIC   = "درمانگاه شبانه‌روزی آزادی طب";
  var FA_SVCLIST  = "لیست خدمات";
  var FA_NAME     = "نام: ";
  var FA_FAMILY   = "نام خانوادگی: ";
  var FA_NID      = "کد ملی: ";
  var FA_DATE     = "تاریخ: ";
  var FA_RECEIPT  = "شماره قبض: ";
  var FA_APPT     = "شماره نوبت";
  var FA_TOTAL    = "جمع کل: ";
  var FA_PAID     = "پرداختی: ";
  var FA_PATIENT  = "سهم بیمار: ";
  var FA_INS      = "بیمه: ";
  var FA_DOCTOR   = "پزشک: ";
  var FA_PHONE    = "تلفن: ";
  var FA_SHIFT    = "شیفت: ";
  var FA_BARCODE  = "بارکد رسید";

  /* ======================================================== block builders */
  function addHeader(d, m, cw, style) {
    var y = m, pw = 210;
    if (style === 1) {
      d.push(LOGO(m + cw - 20, y, 20, 20));
      d.push(L(m, y + 3, cw - 24, 12, FA_CLINIC, 17, true, 0));
      y = m + 24;
    } else if (style === 2) {
      d.push(LOGO((pw - 22) / 2, y, 22, 22));
      d.push(L(m, y + 24, cw, 12, FA_CLINIC, 18, true, 1));
      y = m + 40;
    } else {
      d.push(L(m, y, cw, 12, FA_CLINIC, 18, true, 1));
      y = m + 16;
    }
    d.push(HL(m, y, cw, 0.6));
    return y + 4;
  }

  function addMetaStrip(d, m, y, cw, pt) {
    d.push(F(m, y, cw / 3, 7, "receiptNo", FA_RECEIPT, pt, 2));
    d.push(F(m + cw / 3, y, cw / 3, 7, "date", FA_DATE, pt, 1));
    d.push(F(m + 2 * cw / 3, y, cw / 3, 7, "shift", FA_SHIFT, pt, 0));
    return y + 9;
  }

  function addPatient(d, m, y, cw, pt) {
    var col = cw / 2 - 3;
    d.push(F(m + col + 6, y, col, 7.5, "firstName", FA_NAME, pt, 0));
    d.push(F(m, y, col, 7.5, "lastName", FA_FAMILY, pt, 0)); y += 8.5;
    d.push(F(m + col + 6, y, col, 7.5, "nationalCode", FA_NID, pt, 0));
    d.push(F(m, y, col, 7.5, "insurance", FA_INS, pt, 0)); y += 8.5;
    d.push(F(m + col + 6, y, col, 7.5, "doctor", FA_DOCTOR, pt, 0));
    d.push(F(m, y, col, 7.5, "mobile", FA_PHONE, pt, 0)); y += 8.5;
    return y + 2;
  }

  function addServices(d, m, y, cw, pt, svcH, banded, bw) {
    d.push(L(m, y, cw * 0.6, 6.5, FA_SVCLIST, pt + 1, true, 0));
    d.push(F(m + cw * 0.6, y, cw * 0.4, 6.5, "servicescount", "تعداد ردیف: ",
      pt - 0.5, 2));
    y += 8;
    d.push(SERVICES(m, y, cw, svcH, pt - 0.5, banded, bw));
    return y + svcH + 4;
  }

  function addTotals(d, m, y, cw, pt) {
    d.push(HL(m, y, cw, 0.3)); y += 2.5;
    d.push(F(m + 2 * cw / 3, y, cw / 3, 8, "total", FA_TOTAL, pt + 0.5, 0));
    d.push(F(m + cw / 3, y, cw / 3, 8, "patientshare", FA_PATIENT, pt + 0.5, 1));
    d.push(F(m, y, cw / 3, 8, "paid", FA_PAID, pt + 0.5, 2));
    return y + 10;
  }

  function addFooter(d, m, cw, paperH, withAppt) {
    var y = paperH - 32;
    d.push(HL(m, y - 3, cw, 0.3));
    var bcW = cw * 0.42, bcH = 16;
    var bcX = m + cw - bcW;
    d.push(BARCODE(bcX, y, bcW, bcH));
    d.push(L(bcX, y + bcH + 0.5, bcW, 4, FA_BARCODE, 8, false, 1));
    if (withAppt) {
      d.push(APPTNO(m, y - 1, cw * 0.4, 12, 22));
      d.push(L(m, y + 13, cw * 0.4, 5, FA_APPT, 9, false, 1));
    }
  }

  /* =================================================== 30 distinct layouts */
  //  header: 0 plain centred / 1 logo top-right / 2 centred logo above title
  //  svcH: services-table box height (mm); banded: shaded rows; bw: border
  //  frame: 0=none else 0xRRGGBB; appt: show appointment number; photo/qr.
  var SP = [
    { h: 0, svcH: 96,  banded: true,  bw: 0.40, frame: 0,          appt: true,  photo: false, qr: false }, // 01
    { h: 1, svcH: 92,  banded: true,  bw: 0.40, frame: 0,          appt: true,  photo: false, qr: false }, // 02
    { h: 2, svcH: 84,  banded: false, bw: 0.30, frame: 0,          appt: true,  photo: false, qr: false }, // 03
    { h: 0, svcH: 90,  banded: false, bw: 0.30, frame: "#1F6FEB",  appt: true,  photo: false, qr: false }, // 04
    { h: 1, svcH: 78,  banded: true,  bw: 0.40, frame: 0,          appt: true,  photo: true,  qr: false }, // 05
    { h: 0, svcH: 88,  banded: false, bw: 0.35, frame: 0,          appt: false, photo: false, qr: true  }, // 06
    { h: 2, svcH: 72,  banded: true,  bw: 0.45, frame: "#0B5ED7",  appt: true,  photo: false, qr: false }, // 07
    { h: 0, svcH: 100, banded: true,  bw: 0.50, frame: 0,          appt: true,  photo: false, qr: false }, // 08
    { h: 1, svcH: 86,  banded: false, bw: 0.30, frame: 0,          appt: false, photo: false, qr: false }, // 09
    { h: 0, svcH: 82,  banded: true,  bw: 0.40, frame: "#6A0DAD",  appt: true,  photo: false, qr: false }, // 10
    { h: 1, svcH: 90,  banded: true,  bw: 0.35, frame: 0,          appt: true,  photo: false, qr: false }, // 11
    { h: 0, svcH: 74,  banded: false, bw: 0.30, frame: 0,          appt: true,  photo: true,  qr: false }, // 12
    { h: 2, svcH: 80,  banded: true,  bw: 0.40, frame: 0,          appt: false, photo: false, qr: false }, // 13
    { h: 0, svcH: 94,  banded: false, bw: 0.45, frame: 0,          appt: true,  photo: false, qr: false }, // 14
    { h: 1, svcH: 68,  banded: true,  bw: 0.50, frame: "#0B5ED7",  appt: true,  photo: true,  qr: false }, // 15
    { h: 0, svcH: 98,  banded: true,  bw: 0.40, frame: 0,          appt: true,  photo: false, qr: false }, // 16
    { h: 2, svcH: 76,  banded: false, bw: 0.35, frame: 0,          appt: true,  photo: false, qr: true  }, // 17
    { h: 1, svcH: 88,  banded: false, bw: 0.30, frame: "#1F6FEB",  appt: false, photo: false, qr: false }, // 18
    { h: 0, svcH: 84,  banded: true,  bw: 0.45, frame: 0,          appt: true,  photo: false, qr: false }, // 19
    { h: 1, svcH: 92,  banded: false, bw: 0.35, frame: 0,          appt: true,  photo: false, qr: false }, // 20
    { h: 0, svcH: 70,  banded: true,  bw: 0.40, frame: 0,          appt: true,  photo: true,  qr: true  }, // 21
    { h: 2, svcH: 90,  banded: true,  bw: 0.50, frame: "#6A0DAD",  appt: true,  photo: false, qr: false }, // 22
    { h: 1, svcH: 78,  banded: false, bw: 0.30, frame: 0,          appt: false, photo: false, qr: false }, // 23
    { h: 0, svcH: 96,  banded: true,  bw: 0.40, frame: 0,          appt: true,  photo: false, qr: false }, // 24
    { h: 1, svcH: 82,  banded: true,  bw: 0.45, frame: "#0B5ED7",  appt: true,  photo: false, qr: false }, // 25
    { h: 0, svcH: 86,  banded: false, bw: 0.35, frame: 0,          appt: true,  photo: false, qr: false }, // 26
    { h: 2, svcH: 74,  banded: true,  bw: 0.40, frame: 0,          appt: true,  photo: true,  qr: false }, // 27
    { h: 1, svcH: 90,  banded: false, bw: 0.30, frame: "#1F6FEB",  appt: true,  photo: false, qr: false }, // 28
    { h: 0, svcH: 94,  banded: true,  bw: 0.45, frame: 0,          appt: true,  photo: false, qr: false }, // 29
    { h: 1, svcH: 88,  banded: true,  bw: 0.40, frame: "#6A0DAD",  appt: true,  photo: false, qr: false }  // 30
  ];

  var NAMES = [
    "۰۱) رسید پذیرش — استاندارد",
    "۰۲) رسید پذیرش — لوگو کنار عنوان",
    "۰۳) رسید پذیرش — لوگوی وسط",
    "۰۴) رسید پذیرش — قاب آبی",
    "۰۵) رسید پذیرش — عکس بیمار",
    "۰۶) رسید پذیرش — QR گوشه",
    "۰۷) رسید پذیرش — لوگو + قاب",
    "۰۸) رسید پذیرش — جدول بلند",
    "۰۹) رسید پذیرش — خط‌کشی ساده",
    "۱۰) رسید پذیرش — قاب بنفش",
    "۱۱) رسید پذیرش — لوگو باند روشن",
    "۱۲) رسید پذیرش — عکس + خط",
    "۱۳) رسید پذیرش — وسط‌چین",
    "۱۴) رسید پذیرش — خط ضخیم",
    "۱۵) رسید پذیرش — فشرده عکس‌دار",
    "۱۶) رسید پذیرش — جدول باند بلند",
    "۱۷) رسید پذیرش — وسط + QR",
    "۱۸) رسید پذیرش — قاب آبی لوگو",
    "۱۹) رسید پذیرش — متعادل",
    "۲۰) رسید پذیرش — لوگو خطی",
    "۲۱) رسید پذیرش — عکس + QR",
    "۲۲) رسید پذیرش — وسط قاب بنفش",
    "۲۳) رسید پذیرش — مینیمال",
    "۲۴) رسید پذیرش — استاندارد باند",
    "۲۵) رسید پذیرش — لوگو قاب‌دار",
    "۲۶) رسید پذیرش — تمیز",
    "۲۷) رسید پذیرش — وسط عکس‌دار",
    "۲۸) رسید پذیرش — لوگو آبی",
    "۲۹) رسید پذیرش — باند ضخیم",
    "۳۰) رسید پذیرش — لوگو بنفش"
  ];

  function buildTemplate(idx) {
    var sp = SP[idx];
    var d = [];
    var pw = 210, ph = 297, m = 14, cw = pw - 2 * m;   // 182mm content width
    var pt = 11.0;
    if (sp.frame) d.push(FRAME(pw, ph, 9, sp.frame, 0.9));
    var y = addHeader(d, m, cw, sp.h);
    y = addMetaStrip(d, m, y, cw, pt - 0.5);
    if (sp.photo) d.push(PHOTO(pw - m - 24, y, 24, 30));
    if (sp.qr)    d.push(QR(pw - m - 24, y, 24));
    y = addPatient(d, m, y, cw, pt);
    y = addServices(d, m, y, cw, pt, sp.svcH, sp.banded, sp.bw);
    y = addTotals(d, m, y, cw, pt);
    addFooter(d, m, cw, ph, sp.appt);
    return d;
  }

  var ALL = [];
  for (var i = 0; i < 30; i++) {
    var items = buildTemplate(i);
    items.forEach(function (it, j) { it.id = j + 1; it.z = j + 1; });
    ALL.push({ id: 0, name: NAMES[i], kind: "builtin", group: "reception",
      paper: "A4", orientation: 0, items: items });
  }

  window.AZ_TEMPLATES = ALL;
})();
