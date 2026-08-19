/* ===========================================================================
   templates.js — DarmanPlus ready-made print designs  (v1.62.0 — FULL REWRITE)

   ★ این فایل دقیقاً همان ۳۰ قالب داخلی C++ (src/print_designer_templates.inc)
     است تا گالری دیزاینر وب با آنچه موتور چاپ واقعاً seed می‌کند یکی باشد.
     هر تغییری در .inc باید عیناً اینجا هم اعمال شود.

   مرکز هر ۳۰ قالب: جدول پویای خدمات (type:"services") که هنگام چاپ از
   ReceptionRecord.services پر می‌شود — ردیف/نام خدمت/شرح/تعداد/مبلغ، سطر به
   سطر و زنده، مستقیم از مدیریت ← خدمات. نه لیبل ثابت، نه متن آزمایشی.

   v1.62.0:
     • ده خانوادهٔ چیدمان کاملاً متفاوت × ۳ گونه = ۳۰ طرح واقعاً متمایز
     • قاب امن جدول از فضای آزاد صفحه محاسبه می‌شود؛ موتور چاپ فقط سطرهای
       واقعی را می‌کشد و فضای استفاده‌نشدهٔ صفحه را خالی می‌گذارد
     • هشت پیش‌تنظیم ستونی (۴ تا ۷ ستون) با عنوان‌های تأییدشده در pdSvcColOf

   v1.69.0 (تمایز بصری هر ۳۰ طرح):
     • هر گونه در هر خانواده حالا متا/بلاک بیمار/جمع‌بندی/پاورقی مخصوص خود
       را دارد تا هر ۳۰ طرح اساسی متفاوت باشند، نه فقط تعویض رنگ.
     • تنوع بارکد: ۲۱ طرح یک بارکد Code128 (چهار سبک پاورقی)، ۹ طرح بدون
       کد (footClean). هر صفحه نهایتًا یک کد (۰ یا ۱، هرگز دو).
     •compact و درون کادر A4 تا pscale موتور چاپ آن را روی A5/A6/رول کوچک
       متناسب و بدون برش کوچک کند.
   =========================================================================== */
(function () {
  "use strict";

  var INK      = "#000000";
  var RULE     = "#000000";
  var RULE_DIM = "#8a8a8a";
  var PAPER_BG = "#ffffff";

  /* ------------------------------------------------- A4 page geometry (mm) */
  var PG_W   = 210.0;
  var PG_H   = 297.0;
  var PG_M   = 12.0;
  var PG_CW  = PG_W - 2 * PG_M;      /* 186 */
  var FOOT_Y = PG_H - 34.0;          /* 263 */

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
      visibility: 0, imgPath: "",
      rowH: 0, headerH: 0
    };
  }
  function extend(dst, src) {
    var k;
    if (!src) return dst;
    for (k in src) if (Object.prototype.hasOwnProperty.call(src, k)) dst[k] = src[k];
    return dst;
  }
  function mk(o) { return extend(base(), o); }

  /* ---------------------------------------------------------- primitives */
  function L(x, y, w, h, text, pt, bold, align, o) {
    return mk(extend({ type: "label", x: x, y: y, w: w, h: h, text: text,
      pt: pt, bold: !!bold, align: align, dir: (align === 1 ? 2 : 0) }, o || {}));
  }
  function LC(x, y, w, h, text, pt, bold, align, colour) {
    return L(x, y, w, h, text, pt, bold, align, { textColor: colour });
  }
  function F(x, y, w, h, field, prefix, pt, align, o) {
    return mk(extend({ type: "field", x: x, y: y, w: w, h: h,
      field: field, prefix: prefix || "", pt: pt, align: align,
      dir: (align === 1 ? 2 : 0) }, o || {}));
  }
  function FB(x, y, w, h, field, prefix, pt, align, o) {
    return F(x, y, w, h, field, prefix, pt, align, extend({ bold: true }, o || {}));
  }
  function HL(x, y, w, bw, col) {
    return mk({ type: "hline", x: x, y: y, w: w, h: 0.2, borderWidth: bw,
      borderColor: col || RULE });
  }
  function VL(x, y, h, bw) {
    return mk({ type: "vline", x: x, y: y, w: 0.2, h: h, borderWidth: bw });
  }
  function RECT(x, y, w, h, bw, corner, border) {
    return mk({ type: "rect", x: x, y: y, w: w, h: h, borderWidth: bw,
      corner: corner, fillTransparent: true, borderColor: border || RULE });
  }
  function BAND(x, y, w, h, fill) {
    return mk({ type: "rect", x: x, y: y, w: w, h: h, fillColor: fill,
      fillTransparent: false, borderColor: fill, borderWidth: 0 });
  }
  function TINT(x, y, w, h, fill, border, bw, corner) {
    return mk({ type: "rect", x: x, y: y, w: w, h: h, fillColor: fill,
      fillTransparent: false, borderColor: border, borderWidth: bw,
      corner: corner });
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

  /* ------------------------------- THE CORE: live services table presets */
  /*  عنوان هر ستون معنایش را تعیین می‌کند (printer.cpp::pdSvcColOf)، پس
      کاربر می‌تواند ستون‌ها را جابه‌جا یا حذف کند و داده باز هم درست بنشیند. */
  var SVC3 = 0, SVC4_ROW = 1, SVC4_CAT = 2, SVC5 = 3, SVC5_CODE = 4,
      SVC6_FIN = 5, SVC6_INS = 6, SVC7 = 7;

  function svcModel(preset) {
    switch (preset) {
      case SVC4_ROW: return { cols: 5, header: true,
        widths: [0.07, 0.36, 0.25, 0.10, 0.22],
        labels: ["ردیف", "نام خدمت", "شرح خدمت", "تعداد", "مبلغ کل"] };
      case SVC4_CAT: return { cols: 5, header: true,
        widths: [0.32, 0.18, 0.22, 0.09, 0.19],
        labels: ["نام خدمت", "نوع خدمت", "شرح خدمت", "تعداد", "مبلغ کل"] };
      case SVC5: return { cols: 5, header: true,
        widths: [0.07, 0.36, 0.25, 0.10, 0.22],
        labels: ["ردیف", "نام خدمت", "شرح خدمت", "تعداد", "مبلغ کل"] };
      case SVC5_CODE: return { cols: 6, header: true,
        widths: [0.06, 0.12, 0.29, 0.22, 0.09, 0.22],
        labels: ["ردیف", "کد خدمت", "نام خدمت", "شرح خدمت", "تعداد", "مبلغ کل"] };
      case SVC6_FIN: return { cols: 7, header: true,
        widths: [0.05, 0.27, 0.20, 0.08, 0.14, 0.11, 0.15],
        labels: ["ردیف", "نام خدمت", "شرح خدمت", "تعداد", "مبلغ واحد", "تخفیف", "مبلغ کل"] };
      case SVC6_INS: return { cols: 7, header: true,
        widths: [0.05, 0.26, 0.20, 0.08, 0.15, 0.13, 0.13],
        labels: ["ردیف", "نام خدمت", "شرح خدمت", "تعداد", "مبلغ کل", "سهم بیمه", "سهم بیمار"] };
      case SVC7: return { cols: 7, header: true,
        widths: [0.05, 0.11, 0.27, 0.20, 0.08, 0.14, 0.15],
        labels: ["ردیف", "کد خدمت", "نام خدمت", "شرح خدمت", "تعداد",
                 "مبلغ واحد", "مبلغ کل"] };
      default: return { cols: 4, header: true,
        widths: [0.39, 0.29, 0.10, 0.22],
        labels: ["نام خدمت", "شرح خدمت", "تعداد", "مبلغ کل"] };
    }
  }

  function SERVICES(x, y, w, h, pt, preset, headFill, bw, rowH, headerH) {
    return mk({
      type: "services", x: x, y: y, w: w, h: h, pt: pt, align: 1, dir: 2,
      borderColor: RULE, borderWidth: bw, textColor: INK,
      fillColor: headFill ? headFill : PAPER_BG,
      fillTransparent: !headFill,
      padding: 0.8, headerH: headerH, rowH: rowH,
      text: JSON.stringify(svcModel(preset))
    });
  }

  /* ------------------------------------------------------------- captions */
  var FA_CLINIC   = "درمانگاه شبانه‌روزی درمان پلاس";
  var FA_SUBTITLE = "سامانه پذیرش و مدیریت درمانگاه";
  var FA_SVCLIST  = "لیست خدمات ارائه‌شده";
  var FA_NAME     = "نام: ";
  var FA_FAMILY   = "نام خانوادگی: ";
  var FA_FULL     = "نام بیمار: ";
  var FA_NID      = "کد ملی: ";
  var FA_FATHER   = "نام پدر: ";
  var FA_BIRTH    = "تاریخ تولد: ";
  var FA_GENDER   = "جنسیت: ";
  var FA_DATE     = "تاریخ: ";
  var FA_TIME     = "ساعت: ";
  var FA_RECEIPT  = "شماره قبض: ";
  var FA_TOTAL    = "جمع کل: ";
  var FA_PAID     = "پرداختی: ";
  var FA_PATIENT  = "سهم بیمار: ";
  var FA_INSSHARE = "سهم بیمه: ";
  var FA_DISCOUNT = "تخفیف: ";
  var FA_FINAL    = "مبلغ نهایی: ";
  var FA_INS      = "بیمه: ";
  var FA_SUPP     = "بیمه مکمل: ";
  var FA_DOCTOR   = "پزشک: ";
  var FA_PERF     = "انجام‌دهنده: ";
  var FA_PHONE    = "تلفن: ";
  var FA_SHIFT    = "شیفت: ";
  var FA_DEPT     = "بخش: ";
  var FA_PTYPE    = "نوع بیمار: ";
  var FA_USER     = "کاربر: ";
  var FA_ROWS     = "تعداد ردیف: ";
  var FA_BARCODE  = "بارکد رسید";
  var FA_SIGN     = "امضا و مهر پذیرش";
  var FA_SIGNPAT  = "امضای بیمار";
  var FA_KEEP     = "این رسید را تا پایان درمان نزد خود نگه دارید.";

  /* ========================================================= header blocks */
  function hdrPlain(d, title, titlePt, subtitle, rule) {
    var y = PG_M;
    d.push(L(PG_M, y, PG_CW, 11, title, titlePt, true, 1)); y += 13;
    if (subtitle) { d.push(L(PG_M, y, PG_CW, 6, FA_SUBTITLE, 9, false, 1)); y += 7; }
    if (rule > 0) { d.push(HL(PG_M, y, PG_CW, rule)); y += 3.5; }
    return y;
  }
  function hdrLogoRight(d, title, titlePt, subtitle, rule) {
    d.push(LOGO(PG_M + PG_CW - 21, PG_M, 21, 21));
    d.push(L(PG_M, PG_M + 1, PG_CW - 25, 11, title, titlePt, true, 0));
    if (subtitle) d.push(L(PG_M, PG_M + 12, PG_CW - 25, 6, FA_SUBTITLE, 9, false, 0));
    var y = PG_M + 23;
    if (rule > 0) { d.push(HL(PG_M, y, PG_CW, rule)); y += 3.5; }
    return y;
  }
  function hdrCenterLogo(d, title, titlePt, subtitle, rule) {
    d.push(LOGO((PG_W - 22) / 2, PG_M, 22, 22));
    var y = PG_M + 24;
    d.push(L(PG_M, y, PG_CW, 11, title, titlePt, true, 1)); y += 12.5;
    if (subtitle) { d.push(L(PG_M, y, PG_CW, 6, FA_SUBTITLE, 9, false, 1)); y += 7; }
    if (rule > 0) { d.push(HL(PG_M, y, PG_CW, rule)); y += 3.5; }
    return y;
  }
  function hdrBand(d, title, accent, logo, docLabel) {
    var bh = 22;
    d.push(BAND(PG_M, PG_M, PG_CW, bh, accent));
    if (logo) d.push(LOGO(PG_M + 2.5, PG_M + 2.5, 17, 17));
    var tx = logo ? (PG_M + 22) : (PG_M + 4);
    var tw = PG_CW - (tx - PG_M) - 4;
    d.push(LC(tx, PG_M + 3.0, tw, 9, title, 16, true, 0, "#ffffff"));
    d.push(LC(tx, PG_M + 12.5, tw, 6, FA_SUBTITLE, 8.5, false, 0, "#ffffff"));
    var y = PG_M + bh + 3;
    if (docLabel) { d.push(L(PG_M, y, PG_CW, 7, docLabel, 12.5, true, 1)); y += 9; }
    return y;
  }
  /* v1.65.0: receipt number removed — every hdrSplit family also prints it
     in its meta strip/grid, so it appeared twice on the page. */
  function hdrSplit(d, title, accent, docLabel) {
    d.push(L(PG_M + PG_CW * 0.40, PG_M, PG_CW * 0.60, 10, title, 15.5, true, 0));
    d.push(L(PG_M + PG_CW * 0.40, PG_M + 10, PG_CW * 0.60, 5.5, FA_SUBTITLE, 8.5, false, 0));
    d.push(LC(PG_M, PG_M + 4, PG_CW * 0.38, 9, docLabel, 13.5, true, 2, accent));
    var y = PG_M + 19;
    d.push(HL(PG_M, y, PG_CW, 1.2, accent)); y += 4;
    return y;
  }

  /* =========================================================== meta blocks */
  function metaStrip4(d, y, pt) {
    var c = PG_CW / 4;
    d.push(FB(PG_M + 3 * c, y, c, 7, "receiptNo", FA_RECEIPT, pt, 0));
    d.push(F (PG_M + 2 * c, y, c, 7, "date",      FA_DATE,    pt, 1));
    d.push(F (PG_M +     c, y, c, 7, "time",      FA_TIME,    pt, 1));
    d.push(F (PG_M        , y, c, 7, "shift",     FA_SHIFT,   pt, 2));
    return y + 9;
  }
  function metaStripBoxed(d, y, pt, tint, border) {
    var h = 11, c = PG_CW / 4, i;
    d.push(TINT(PG_M, y, PG_CW, h, tint, border, 0.3, 1.2));
    d.push(FB(PG_M + 3 * c + 1, y + 2, c - 2, 7, "receiptNo", FA_RECEIPT, pt, 1));
    d.push(F (PG_M + 2 * c + 1, y + 2, c - 2, 7, "date",      FA_DATE,    pt, 1));
    d.push(F (PG_M +     c + 1, y + 2, c - 2, 7, "time",      FA_TIME,    pt, 1));
    d.push(F (PG_M         + 1, y + 2, c - 2, 7, "shift",     FA_SHIFT,   pt, 1));
    for (i = 1; i < 4; i++) d.push(VL(PG_M + i * c, y, h, 0.25));
    return y + h + 3;
  }
  function metaGrid6(d, y, pt, border) {
    var h = 9, c = PG_CW / 3, r, i;
    for (r = 0; r < 2; r++) d.push(RECT(PG_M, y + r * h, PG_CW, h, 0.3, 0, border));
    for (i = 1; i < 3; i++) d.push(VL(PG_M + i * c, y, 2 * h, 0.25));
    d.push(FB(PG_M + 2 * c + 1.5, y + 1, c - 3, 6.5, "receiptNo", FA_RECEIPT, pt, 0));
    d.push(F (PG_M +     c + 1.5, y + 1, c - 3, 6.5, "date",      FA_DATE,    pt, 0));
    d.push(F (PG_M         + 1.5, y + 1, c - 3, 6.5, "time",      FA_TIME,    pt, 0));
    d.push(F (PG_M + 2 * c + 1.5, y + h + 1, c - 3, 6.5, "shift",    FA_SHIFT, pt, 0));
    d.push(F (PG_M +     c + 1.5, y + h + 1, c - 3, 6.5, "dept",     FA_DEPT,  pt, 0));
    d.push(F (PG_M         + 1.5, y + h + 1, c - 3, 6.5, "userName", FA_USER,  pt, 0));
    return y + 2 * h + 3;
  }

  /* ======================================================== patient blocks */
  function patient2x3(d, y, pt) {
    var col = PG_CW / 2 - 3;
    d.push(FB(PG_M + col + 6, y, col, 7.5, "firstName", FA_NAME, pt, 0));
    d.push(FB(PG_M, y, col, 7.5, "lastName", FA_FAMILY, pt, 0)); y += 8.5;
    d.push(F(PG_M + col + 6, y, col, 7.5, "nationalCode", FA_NID, pt, 0));
    d.push(F(PG_M, y, col, 7.5, "insurance", FA_INS, pt, 0)); y += 8.5;
    d.push(F(PG_M + col + 6, y, col, 7.5, "doctor", FA_DOCTOR, pt, 0));
    d.push(F(PG_M, y, col, 7.5, "mobile", FA_PHONE, pt, 0)); y += 8.5;
    return y + 2;
  }
  function patient3x3(d, y, pt) {
    var c = PG_CW / 3;
    d.push(FB(PG_M + 2 * c, y, c - 2, 7, "fullName", FA_FULL, pt, 0));
    d.push(F (PG_M +     c, y, c - 2, 7, "nationalCode", FA_NID, pt, 0));
    d.push(F (PG_M        , y, c - 2, 7, "fatherName", FA_FATHER, pt, 0)); y += 8;
    d.push(F (PG_M + 2 * c, y, c - 2, 7, "birthDate", FA_BIRTH, pt, 0));
    d.push(F (PG_M +     c, y, c - 2, 7, "gender", FA_GENDER, pt, 0));
    d.push(F (PG_M        , y, c - 2, 7, "mobile", FA_PHONE, pt, 0)); y += 8;
    d.push(F (PG_M + 2 * c, y, c - 2, 7, "insurance", FA_INS, pt, 0));
    d.push(F (PG_M +     c, y, c - 2, 7, "suppInsurance", FA_SUPP, pt, 0));
    d.push(F (PG_M        , y, c - 2, 7, "doctor", FA_DOCTOR, pt, 0)); y += 8;
    return y + 2;
  }
  function patientCard(d, y, pt, tint, border, accent) {
    var h = 32, iy, c = PG_CW / 3;
    d.push(TINT(PG_M, y, PG_CW, h, tint, border, 0.35, 1.5));
    d.push(BAND(PG_M, y, PG_CW, 7, accent));
    d.push(LC(PG_M + 3, y + 0.8, PG_CW - 6, 5.5, "مشخصات بیمار", 9.5, true, 0, "#ffffff"));
    iy = y + 8.5;
    d.push(FB(PG_M + 2 * c + 1.5, iy, c - 3, 7, "fullName", FA_FULL, pt, 0));
    d.push(F (PG_M +     c + 1.5, iy, c - 3, 7, "nationalCode", FA_NID, pt, 0));
    d.push(F (PG_M         + 1.5, iy, c - 3, 7, "mobile", FA_PHONE, pt, 0)); iy += 7.6;
    d.push(F (PG_M + 2 * c + 1.5, iy, c - 3, 7, "insurance", FA_INS, pt, 0));
    d.push(F (PG_M +     c + 1.5, iy, c - 3, 7, "suppInsurance", FA_SUPP, pt, 0));
    d.push(F (PG_M         + 1.5, iy, c - 3, 7, "patientType", FA_PTYPE, pt, 0)); iy += 7.6;
    d.push(F (PG_M + 2 * c + 1.5, iy, c - 3, 7, "doctor", FA_DOCTOR, pt, 0));
    d.push(F (PG_M +     c + 1.5, iy, c - 3, 7, "performer", FA_PERF, pt, 0));
    d.push(F (PG_M         + 1.5, iy, c - 3, 7, "dept", FA_DEPT, pt, 0));
    return y + h + 3;
  }
  /* v1.65.0: QR removed — family 6 already prints the receipt barcode in its
     footer, so the QR duplicated the same code on the page. */
  function patientPhoto(d, y, pt) {
    var pw = 24, ph = 30, gx, gw, c;
    d.push(PHOTO(PG_M, y, pw, ph));
    d.push(L(PG_M, y + ph + 0.5, pw, 4, "عکس بیمار", 7.5, false, 1));
    gx = PG_M + pw + 4; gw = PG_CW - pw - 4;
    c = gw / 2;
    d.push(FB(gx + c, y, c - 2, 7, "fullName", FA_FULL, pt, 0));
    d.push(F (gx    , y, c - 2, 7, "nationalCode", FA_NID, pt, 0));
    d.push(F (gx + c, y + 7.8, c - 2, 7, "fatherName", FA_FATHER, pt, 0));
    d.push(F (gx    , y + 7.8, c - 2, 7, "birthDate", FA_BIRTH, pt, 0));
    d.push(F (gx + c, y + 15.6, c - 2, 7, "insurance", FA_INS, pt, 0));
    d.push(F (gx    , y + 15.6, c - 2, 7, "mobile", FA_PHONE, pt, 0));
    d.push(F (gx + c, y + 23.4, c - 2, 7, "doctor", FA_DOCTOR, pt, 0));
    d.push(F (gx    , y + 23.4, c - 2, 7, "dept", FA_DEPT, pt, 0));
    return y + ph + 6;
  }

  /* ================================= SERVICES BLOCK — the centrepiece ==== */
  function servicesBlockAt(d, x, w, y, pt, preset, headFill, bw, rowH,
                           reserve, caption, footY) {
    var h, headerH;
    if (caption) {
      d.push(L(x, y, w * 0.60, 6.5, FA_SVCLIST, pt + 1, true, 0));
      d.push(F(x + w * 0.60, y, w * 0.40, 6.5, "servicescount", FA_ROWS, pt - 0.5, 2));
      y += 8;
    }
    h = footY - reserve - y;
    if (h < 18) h = 18;
    headerH = rowH + 0.8;
    d.push(SERVICES(x, y, w, h, pt - 0.5, preset, headFill, bw, rowH, headerH));
    return y + h + 3;
  }
  function servicesBlock(d, y, pt, preset, headFill, bw, rowH, reserve, caption, footY) {
    return servicesBlockAt(d, PG_M, PG_CW, y, pt, preset, headFill, bw, rowH,
                           reserve, caption, footY);
  }

  /* ========================================================= totals blocks */
  function totalsRow(d, y, pt) {
    var c = PG_CW / 3;
    d.push(HL(PG_M, y, PG_CW, 0.35)); y += 2.5;
    d.push(F (PG_M + 2 * c, y, c, 8, "total", FA_TOTAL, pt + 0.5, 0));
    d.push(F (PG_M +     c, y, c, 8, "patientshare", FA_PATIENT, pt + 0.5, 1));
    d.push(FB(PG_M        , y, c, 8, "paid", FA_PAID, pt + 1.0, 2));
    return y + 10.5;
  }
  function totalsBox(d, y, pt, tint, border, accent) {
    var bw_ = PG_CW * 0.46, bx = PG_M, h = 28, iy = y + 1.5, sx, sw;
    d.push(TINT(bx, y, bw_, h, tint, border, 0.35, 1.5));
    d.push(F (bx + 1.5, iy, bw_ - 3, 6, "total", FA_TOTAL, pt, 0));       iy += 6.2;
    d.push(F (bx + 1.5, iy, bw_ - 3, 6, "discount", FA_DISCOUNT, pt, 0)); iy += 6.2;
    d.push(F (bx + 1.5, iy, bw_ - 3, 6, "insshare", FA_INSSHARE, pt, 0)); iy += 6.2;
    d.push(HL(bx + 1.5, iy - 0.6, bw_ - 3, 0.4, border));
    d.push(FB(bx + 1.5, iy, bw_ - 3, 7, "paid", FA_FINAL, pt + 1.5, 0));
    sx = PG_M + PG_CW * 0.54; sw = PG_CW * 0.46;
    d.push(LC(sx, y + 1, sw, 6, FA_KEEP, 8.5, false, 2, accent));
    d.push(HL(sx + sw * 0.30, y + 18, sw * 0.70, 0.3));
    d.push(L (sx + sw * 0.30, y + 18.5, sw * 0.70, 5, FA_SIGN, 8.5, false, 1));
    return y + h + 3;
  }
  function totalsLadder(d, y, pt, accent) {
    var lw = PG_CW * 0.44, lx, R, i, last, f, sx, sw;
    d.push(HL(PG_M, y, PG_CW, 0.8, accent)); y += 2.5;
    lx = PG_M + PG_CW - lw;
    R = [[FA_TOTAL, "total"], [FA_DISCOUNT, "discount"], [FA_INSSHARE, "insshare"],
         [FA_PATIENT, "patientshare"], [FA_PAID, "paid"]];
    for (i = 0; i < 5; i++) {
      last = (i === 4);
      if (last) d.push(HL(lx, y - 0.5, lw, 0.5, accent));
      f = F(lx, y, lw, 6.4, R[i][1], R[i][0], last ? (pt + 1.5) : pt, 0);
      f.bold = last; if (last) f.textColor = accent;
      d.push(f);
      y += 6.4;
    }
    sx = PG_M; sw = PG_CW - lw - 6;
    d.push(L(sx, y - 31, sw, 5.5, FA_KEEP, 8.5, false, 0));
    d.push(HL(sx, y - 9, sw * 0.62, 0.3));
    d.push(L(sx, y - 8.5, sw * 0.62, 5, FA_SIGNPAT, 8.5, false, 1));
    return y + 3;
  }
  function totalsBar(d, y, pt, accent) {
    var c = PG_CW / 4, f0, f1, f2, f3;
    d.push(BAND(PG_M, y, PG_CW, 11, accent));
    f0 = F(PG_M + 3 * c + 1.5, y + 2, c - 3, 7, "total", FA_TOTAL, pt, 0);
    f1 = F(PG_M + 2 * c + 1.5, y + 2, c - 3, 7, "discount", FA_DISCOUNT, pt, 0);
    f2 = F(PG_M +     c + 1.5, y + 2, c - 3, 7, "insshare", FA_INSSHARE, pt, 0);
    f3 = F(PG_M         + 1.5, y + 2, c - 3, 7, "paid", FA_FINAL, pt + 1, 0);
    f0.textColor = f1.textColor = f2.textColor = f3.textColor = "#ffffff";
    f3.bold = true;
    d.push(f0); d.push(f1); d.push(f2); d.push(f3);
    y += 13.5;
    d.push(HL(PG_M + PG_CW * 0.58, y + 3, PG_CW * 0.42, 0.3));
    d.push(L(PG_M + PG_CW * 0.58, y + 3.5, PG_CW * 0.42, 5, FA_SIGN, 8.5, false, 1));
    return y + 9;
  }

  /* ================================================================ footers */
  function footBarcode(d, footY) {
    var y = footY, bcW = PG_CW * 0.40, bcH = 15, bcX = PG_M + PG_CW - bcW;
    d.push(HL(PG_M, y - 3, PG_CW, 0.3));
    d.push(BARCODE(bcX, y, bcW, bcH));
    d.push(L(bcX, y + bcH + 0.5, bcW, 4, FA_BARCODE, 8, false, 1));
    d.push(F(PG_M, y + 1, PG_CW * 0.52, 6, "clinicphone", FA_PHONE, 8.5, 0));
    d.push(F(PG_M, y + 7.5, PG_CW * 0.52, 6, "clinicaddr", "نشانی: ", 8.5, 0));
  }
  function footCentered(d, footY) {
    var y = footY, bcW = PG_CW * 0.46, bcX = PG_M + (PG_CW - bcW) / 2;
    d.push(HL(PG_M, y - 3, PG_CW, 0.3));
    d.push(BARCODE(bcX, y, bcW, 14));
    d.push(F(PG_M, y + 15, PG_CW, 5.5, "clinicaddr", "", 8.5, 1));
    d.push(F(PG_M, y + 20.5, PG_CW, 5.5, "clinicphone", FA_PHONE, 8.5, 1));
  }
  /* v1.65.0: the QR was removed — one deterministic barcode per page. */
  function footDual(d, footY, tint, border) {
    var y = footY - 1, bcW = PG_CW * 0.42, bcX = PG_M + PG_CW - bcW - 2.5;
    d.push(TINT(PG_M, y, PG_CW, 30, tint, border, 0.3, 1.5));
    d.push(BARCODE(bcX, y + 3, bcW, 15));
    d.push(L(bcX, y + 18.5, bcW, 4, FA_BARCODE, 8, false, 1));
    d.push(F(PG_M + 2.5, y + 3, PG_CW - bcW - 10, 6, "clinicphone", FA_PHONE, 8.5, 0));
    d.push(F(PG_M + 2.5, y + 10, PG_CW - bcW - 10, 6, "clinicaddr", "نشانی: ", 8.5, 0));
    d.push(L(PG_M + 2.5, y + 18, PG_CW - bcW - 10, 5, FA_KEEP, 8, false, 0));
  }
  function footMinimal(d, footY) {
    var y = footY + 4, bcW = PG_CW * 0.32, bcX = PG_M + PG_CW - bcW;
    d.push(HL(PG_M, y - 3, PG_CW, 0.25));
    d.push(BARCODE(bcX, y, bcW, 13));
    d.push(L(PG_M, y + 3, PG_CW * 0.60, 5.5, FA_KEEP, 8.5, false, 0));
  }
  /* v1.69.0 — NO code carrier. Clean professional footer: hairline + clinic
     contact left + keep-note / signature right. One deterministic code per page
     is still the rule — here it is simply zero, never two. */
  function footClean(d, footY, accent) {
    var y = footY;
    d.push(HL(PG_M, y - 3, PG_CW, 0.3));
    d.push(F(PG_M, y + 1, PG_CW * 0.55, 6, "clinicphone", FA_PHONE, 8.5, 0));
    d.push(F(PG_M, y + 7.5, PG_CW * 0.55, 6, "clinicaddr", "نشانی: ", 8.5, 0));
    d.push(LC(PG_M + PG_CW * 0.58, y + 1, PG_CW * 0.42, 6, FA_KEEP, 8.5, false, 2, accent));
    d.push(HL(PG_M + PG_CW * 0.58, y + 9, PG_CW * 0.42, 0.3));
    d.push(L(PG_M + PG_CW * 0.58, y + 9.5, PG_CW * 0.42, 5, FA_SIGN, 8.5, false, 1));
  }

  /* ============================================ 30 designs / 10 families == */
  /* family, variant, svc preset, accent, tint, headFill(0=line-art), bw, rowH, frame */
  var TPL = [
    { f: 0, v: 0, s: SVC4_ROW,  a: "#1F6FEB", t: "#F2F6FC", hf: "#E8EDF4", bw: 0.40, rh: 4.4, fr: false },
    { f: 0, v: 1, s: SVC5,      a: "#0B5ED7", t: "#F1F5FB", hf: "#E4EBF5", bw: 0.40, rh: 4.3, fr: false },
    { f: 0, v: 2, s: SVC3,      a: "#2F6F4E", t: "#F1F7F3", hf: 0,         bw: 0.30, rh: 4.6, fr: false },
    { f: 1, v: 0, s: SVC5,      a: "#0B5ED7", t: "#F3F7FD", hf: "#DDE7F6", bw: 0.40, rh: 4.3, fr: false },
    { f: 1, v: 1, s: SVC6_INS,  a: "#1F6FEB", t: "#F2F6FC", hf: "#E1E9F5", bw: 0.40, rh: 4.2, fr: false },
    { f: 1, v: 2, s: SVC4_CAT,  a: "#6A0DAD", t: "#F6F2FA", hf: "#EAE1F4", bw: 0.40, rh: 4.4, fr: false },
    { f: 2, v: 0, s: SVC7,      a: "#14532D", t: "#F2F7F3", hf: "#E6EDE8", bw: 0.35, rh: 4.1, fr: true  },
    { f: 2, v: 1, s: SVC6_FIN,  a: "#0B5ED7", t: "#F2F6FC", hf: "#E4EBF5", bw: 0.35, rh: 4.1, fr: true  },
    { f: 2, v: 2, s: SVC5_CODE, a: "#8A2E2E", t: "#FAF2F2", hf: "#F0E3E3", bw: 0.35, rh: 4.2, fr: true  },
    { f: 3, v: 0, s: SVC5,      a: "#0284C7", t: "#EFF7FC", hf: "#DCEEF8", bw: 0.35, rh: 4.3, fr: false },
    { f: 3, v: 1, s: SVC4_ROW,  a: "#0F766E", t: "#EFF7F6", hf: "#E0EEEC", bw: 0.35, rh: 4.4, fr: false },
    { f: 3, v: 2, s: SVC6_FIN,  a: "#6A0DAD", t: "#F5F1FA", hf: "#E9E0F3", bw: 0.35, rh: 4.2, fr: false },
    { f: 4, v: 0, s: SVC4_ROW,  a: "#000000", t: "#FFFFFF", hf: 0,         bw: 0.30, rh: 4.5, fr: false },
    { f: 4, v: 1, s: SVC5,      a: "#000000", t: "#FFFFFF", hf: 0,         bw: 0.45, rh: 4.4, fr: true  },
    { f: 4, v: 2, s: SVC7,      a: "#000000", t: "#FFFFFF", hf: 0,         bw: 0.30, rh: 4.1, fr: false },
    { f: 5, v: 0, s: SVC5,      a: "#4F46E5", t: "#F2F2FC", hf: "#E2E1F7", bw: 0.35, rh: 4.3, fr: false },
    { f: 5, v: 1, s: SVC4_CAT,  a: "#0F766E", t: "#F0F7F6", hf: "#E1EFEC", bw: 0.35, rh: 4.4, fr: false },
    { f: 5, v: 2, s: SVC6_INS,  a: "#B45309", t: "#FDF6EC", hf: "#F6E9D2", bw: 0.35, rh: 4.2, fr: false },
    { f: 6, v: 0, s: SVC4_CAT,  a: "#0E7490", t: "#EFF8FA", hf: "#DFF0F4", bw: 0.40, rh: 4.4, fr: false },
    { f: 6, v: 1, s: SVC5,      a: "#14532D", t: "#F1F6F2", hf: "#E4EDE7", bw: 0.40, rh: 4.3, fr: false },
    { f: 6, v: 2, s: SVC3,      a: "#6A0DAD", t: "#F6F2FA", hf: 0,         bw: 0.30, rh: 4.6, fr: false },
    { f: 7, v: 0, s: SVC6_FIN,  a: "#0B5ED7", t: "#F2F6FC", hf: "#E2EAF5", bw: 0.35, rh: 4.2, fr: false },
    { f: 7, v: 1, s: SVC6_INS,  a: "#8A2E2E", t: "#FAF2F2", hf: "#EFE2E2", bw: 0.35, rh: 4.2, fr: false },
    { f: 7, v: 2, s: SVC7,      a: "#14532D", t: "#F1F6F2", hf: "#E3ECE6", bw: 0.30, rh: 4.0, fr: true  },
    { f: 8, v: 0, s: SVC4_ROW,  a: "#334155", t: "#F4F6F8", hf: "#E5E9EE", bw: 0.40, rh: 4.3, fr: false },
    { f: 8, v: 1, s: SVC5,      a: "#0F766E", t: "#EFF7F6", hf: "#DEEDEA", bw: 0.40, rh: 4.2, fr: false },
    { f: 8, v: 2, s: SVC3,      a: "#6A0DAD", t: "#F6F2FA", hf: "#E9E0F3", bw: 0.40, rh: 4.4, fr: false },
    { f: 9, v: 0, s: SVC5,      a: "#1F6FEB", t: "#F2F6FC", hf: "#E4ECF7", bw: 0.40, rh: 4.3, fr: false },
    { f: 9, v: 1, s: SVC6_FIN,  a: "#B45309", t: "#FDF6EC", hf: "#F6E9D2", bw: 0.40, rh: 4.2, fr: false },
    { f: 9, v: 2, s: SVC5_CODE, a: "#6A0DAD", t: "#F6F2FA", hf: "#E9E0F3", bw: 0.40, rh: 4.3, fr: true  }
  ];

  var NAMES = [
    "۰۱) کلاسیک — جدول خدمات بلند",
    "۰۲) کلاسیک — لوگو کنار عنوان + مبلغ سطری",
    "۰۳) کلاسیک — تک‌رنگ وسط‌چین",
    "۰۴) باند رنگی — قبض پذیرش آبی",
    "۰۵) باند رنگی — سهم بیمه و بیمار",
    "۰۶) باند رنگی — بنفش با نوع خدمت",
    "۰۷) فاکتور — هفت‌ستونه کامل",
    "۰۸) فاکتور — ریز مالی با تخفیف",
    "۰۹) فاکتور — کد خدمت و مبلغ",
    "۱۰) ستون کناری — آبی آسمانی",
    "۱۱) ستون کناری — سبز آبی",
    "۱۲) ستون کناری — ریز مالی",
    "۱۳) خط‌کشی — تک‌رنگ چهارستونه",
    "۱۴) خط‌کشی — قاب‌دار پنج‌ستونه",
    "۱۵) خط‌کشی — هفت‌ستونه فشرده",
    "۱۶) کارتی — گرد نیلی",
    "۱۷) کارتی — فیروزه‌ای نوع خدمت",
    "۱۸) کارتی — کهربایی سهم بیمه",
    "۱۹) عکس بیمار — سبز دریایی نوع خدمت",
    "۲۰) عکس بیمار — باند سبز",
    "۲۱) عکس بیمار — بنفش تک‌رنگ",
    "۲۲) مالی — تخفیف و مبلغ واحد",
    "۲۳) مالی — سهم بیمه و بیمار",
    "۲۴) مالی — هفت‌ستونه قاب‌دار",
    "۲۵) فشرده — ته‌برگ دودی",
    "۲۶) فشرده — ته‌برگ فیروزه‌ای",
    "۲۷) فشرده — ته‌برگ سه‌ستونه",
    "۲۸) دو بلوکی — باند آبی",
    "۲۹) دو بلوکی — ریز مالی کهربایی",
    "۳۰) دو بلوکی — بنفش قاب‌دار"
  ];

  function buildTemplate(idx) {
    var sp = TPL[idx], d = [], pt = 10.5, y = PG_M, footY = FOOT_Y;
    var doc, c, sbW, sbX, mainX, mainW, my, ly, lw, lx, f;
    var gw, rx, lx2, bh, ry, ly2, tear, sy, pwB, phB;

    if (sp.fr) d.push(FRAME(PG_W, PG_H, 8, sp.a, 0.8));

    switch (sp.f) {

    /* ---------------- 0 — کلاسیک ---------------------------------------- */
    case 0:
      if (sp.v === 0)      y = hdrPlain(d, FA_CLINIC, 18, true, 0.7);
      else if (sp.v === 1) y = hdrLogoRight(d, FA_CLINIC, 17, true, 0.7);
      else                 y = hdrCenterLogo(d, FA_CLINIC, 18, false, 0.5);
      if (sp.v === 0)      y = metaStrip4(d, y, pt - 0.5);
      else if (sp.v === 1) y = metaStripBoxed(d, y, pt - 0.5, sp.t, sp.a);
      else                 y = metaGrid6(d, y, pt - 1, sp.a);
      if (sp.v === 0)      y = patient2x3(d, y, pt);
      else if (sp.v === 1) y = patient3x3(d, y, pt);
      else                 y = patientCard(d, y, pt - 0.5, sp.t, sp.a, sp.a);
      d.push(HL(PG_M, y - 1, PG_CW, 0.25));
      var reserve0 = (sp.v === 2) ? 32 : 15;
      y = servicesBlock(d, y + 1.5, pt, sp.s, sp.hf, sp.bw, sp.rh, reserve0, true, footY);
      if (sp.v === 2) totalsBox(d, footY - 31, pt, sp.t, sp.a, sp.a);
      else            totalsRow(d, footY - 13, pt);
      if (sp.v === 0)      footBarcode(d, footY + 2);
      else if (sp.v === 1) footCentered(d, footY + 3);
      else                 footClean(d, footY + 2, sp.a);
      break;

    /* ---------------- 1 — باند رنگی ------------------------------------- */
    case 1:
      doc = (sp.v === 0) ? "قبض پذیرش و صورت‌حساب خدمات"
          : (sp.v === 1) ? "صورت‌حساب خدمات درمانی"
                         : "رسید پذیرش بیمار";
      y = hdrBand(d, FA_CLINIC, sp.a, sp.v !== 2, doc);
      if (sp.v === 0)      y = metaStripBoxed(d, y, pt - 0.5, sp.t, sp.a);
      else if (sp.v === 1) y = metaStrip4(d, y, pt - 0.5);
      else                 y = metaGrid6(d, y, pt - 1, sp.a);
      y = patientCard(d, y, pt - 0.5, sp.t, sp.a, sp.a);
      var reserve1 = (sp.v === 1) ? 36 : (sp.v === 2) ? 32 : 24;
      y = servicesBlock(d, y, pt, sp.s, sp.hf, sp.bw, sp.rh, reserve1, true, footY);
      if (sp.v === 0)      totalsBar(d, footY - 22, pt, sp.a);
      else if (sp.v === 1) totalsLadder(d, footY - 34, pt, sp.a);
      else                 totalsBox(d, footY - 31, pt, sp.t, sp.a, sp.a);
      if (sp.v === 0)      footCentered(d, footY + 3);
      else if (sp.v === 1) footBarcode(d, footY + 2);
      else                 footClean(d, footY + 2, sp.a);
      break;

    /* ---------------- 2 — فاکتور --------------------------------------- */
    case 2:
      doc = (sp.v === 0) ? "فاکتور خدمات درمانی"
          : (sp.v === 1) ? "فاکتور رسمی خدمات"
                         : "صورت‌حساب تفصیلی خدمات";
      y = hdrSplit(d, FA_CLINIC, sp.a, doc);
      if (sp.v === 0)      y = metaGrid6(d, y, pt - 1, sp.a);
      else if (sp.v === 1) y = metaStripBoxed(d, y, pt - 0.5, sp.t, sp.a);
      else                 y = metaStrip4(d, y, pt - 0.5);
      c = PG_CW / 3;
      d.push(FB(PG_M + 2 * c, y, c - 2, 7, "fullName", FA_FULL, pt, 0));
      d.push(F (PG_M +     c, y, c - 2, 7, "nationalCode", FA_NID, pt, 0));
      d.push(F (PG_M        , y, c - 2, 7, "insurance", FA_INS, pt, 0)); y += 8;
      d.push(FB(PG_M + 2 * c, y, c - 2, 7, "doctor", FA_DOCTOR, pt, 0));
      d.push(F (PG_M +     c, y, c - 2, 7, "performer", FA_PERF, pt, 0));
      d.push(F (PG_M        , y, c - 2, 7, "patientType", FA_PTYPE, pt, 0)); y += 10;
      var reserve2 = (sp.v === 0) ? 36 : (sp.v === 1) ? 24 : 32;
      y = servicesBlock(d, y, pt, sp.s, sp.hf, sp.bw, sp.rh, reserve2, true, footY);
      if (sp.v === 0)      totalsLadder(d, footY - 34, pt, sp.a);
      else if (sp.v === 1) totalsBar(d, footY - 22, pt, sp.a);
      else                 totalsBox(d, footY - 31, pt, sp.t, sp.a, sp.a);
      if (sp.v === 0)      footMinimal(d, footY + 2);
      else if (sp.v === 1) footBarcode(d, footY + 2);
      else                 footClean(d, footY + 2, sp.a);
      break;

    /* ---------------- 3 — ستون کناری ----------------------------------- */
    case 3:
      sbW = 44; sbX = PG_M; mainX = PG_M + sbW + 5; mainW = PG_CW - sbW - 5;
      d.push(TINT(sbX, PG_M, sbW, footY - PG_M - 2, sp.t, sp.a, 0.35, 2.0));
      d.push(LOGO(sbX + (sbW - 26) / 2, PG_M + 4, 26, 26));
      d.push(LC(sbX + 2, PG_M + 33, sbW - 4, 10, FA_CLINIC, 10, true, 1, sp.a));
      /* v1.65.0: the sidebar QR was removed (double code carrier); the freed
         space carries the clinic contact block. ONE barcode remains. */
      d.push(HL(sbX + 3, PG_M + 45, sbW - 6, 0.35, sp.a));
      d.push(F(sbX + 2, PG_M + 48, sbW - 4, 6, "receiptNo", FA_RECEIPT, 9, 1));
      d.push(F(sbX + 2, PG_M + 54.5, sbW - 4, 6, "date", FA_DATE, 9, 1));
      d.push(F(sbX + 2, PG_M + 61, sbW - 4, 6, "time", FA_TIME, 9, 1));
      d.push(F(sbX + 2, PG_M + 67.5, sbW - 4, 6, "shift", FA_SHIFT, 9, 1));
      d.push(HL(sbX + 3, PG_M + 76, sbW - 6, 0.3, sp.a));
      d.push(F(sbX + 2, PG_M + 79, sbW - 4, 6, "dept", FA_DEPT, 9, 1));
      d.push(F(sbX + 2, PG_M + 85.5, sbW - 4, 6, "userName", FA_USER, 9, 1));
      d.push(HL(sbX + 3, PG_M + 94, sbW - 6, 0.3, sp.a));
      d.push(F(sbX + 2, PG_M + 97, sbW - 4, 6, "clinicphone", FA_PHONE, 8.5, 1));
      d.push(F(sbX + 2, PG_M + 103.5, sbW - 4, 10, "clinicaddr", "نشانی: ", 8.5, 1));
      d.push(L(sbX + 2, PG_M + 115, sbW - 4, 10, FA_KEEP, 8, false, 1));
      d.push(BARCODE(sbX + 2, footY - 48, sbW - 4, 26));
      d.push(L(sbX + 2, footY - 19, sbW - 4, 4, FA_BARCODE, 7.5, false, 1));
      my = PG_M + 1;
      doc = (sp.v === 0) ? "قبض پذیرش و خدمات"
          : (sp.v === 1) ? "صورت‌حساب خدمات"
                         : "فاکتور خدمات درمانی";
      d.push(LC(mainX, my, mainW, 9, doc, 15, true, 0, sp.a)); my += 11;
      d.push(HL(mainX, my, mainW, 0.7, sp.a)); my += 3.5;
      if (sp.v === 0) {                 /* plain two-column identity grid */
        c = mainW / 2;
        d.push(FB(mainX + c, my, c - 2, 7, "fullName", FA_FULL, pt, 0));
        d.push(F (mainX    , my, c - 2, 7, "nationalCode", FA_NID, pt, 0)); my += 7.8;
        d.push(F (mainX + c, my, c - 2, 7, "fatherName", FA_FATHER, pt, 0));
        d.push(F (mainX    , my, c - 2, 7, "birthDate", FA_BIRTH, pt, 0)); my += 7.8;
        d.push(F (mainX + c, my, c - 2, 7, "insurance", FA_INS, pt, 0));
        d.push(F (mainX    , my, c - 2, 7, "suppInsurance", FA_SUPP, pt, 0)); my += 7.8;
        d.push(F (mainX + c, my, c - 2, 7, "doctor", FA_DOCTOR, pt, 0));
        d.push(F (mainX    , my, c - 2, 7, "mobile", FA_PHONE, pt, 0)); my += 10;
      } else if (sp.v === 1) {          /* three-column identity grid */
        c = mainW / 3;
        d.push(FB(mainX + 2 * c, my, c - 2, 7, "fullName", FA_FULL, pt, 0));
        d.push(F (mainX +     c, my, c - 2, 7, "nationalCode", FA_NID, pt, 0));
        d.push(F (mainX        , my, c - 2, 7, "fatherName", FA_FATHER, pt, 0)); my += 7.8;
        d.push(F (mainX + 2 * c, my, c - 2, 7, "birthDate", FA_BIRTH, pt, 0));
        d.push(F (mainX +     c, my, c - 2, 7, "insurance", FA_INS, pt, 0));
        d.push(F (mainX        , my, c - 2, 7, "suppInsurance", FA_SUPP, pt, 0)); my += 7.8;
        d.push(F (mainX + 2 * c, my, c - 2, 7, "doctor", FA_DOCTOR, pt, 0));
        d.push(F (mainX +     c, my, c - 2, 7, "mobile", FA_PHONE, pt, 0));
        d.push(F (mainX        , my, c - 2, 7, "dept", FA_DEPT, pt, 0)); my += 10;
      } else {                          /* boxed tinted identity card */
        c = mainW / 2; var bh3 = 34;
        d.push(TINT(mainX, my, mainW, bh3, sp.t, sp.a, 0.3, 1.5));
        d.push(BAND(mainX, my, mainW, 6.5, sp.a));
        d.push(LC(mainX + 2, my + 0.6, mainW - 4, 5, "مشخصات بیمار", 9, true, 0, "#ffffff"));
        var py = my + 8;
        d.push(FB(mainX + c, py, c - 2, 6.5, "fullName", FA_FULL, pt, 0));
        d.push(F (mainX    , py, c - 2, 6.5, "nationalCode", FA_NID, pt, 0)); py += 7;
        d.push(F (mainX + c, py, c - 2, 6.5, "fatherName", FA_FATHER, pt, 0));
        d.push(F (mainX    , py, c - 2, 6.5, "birthDate", FA_BIRTH, pt, 0)); py += 7;
        d.push(F (mainX + c, py, c - 2, 6.5, "insurance", FA_INS, pt, 0));
        d.push(F (mainX    , py, c - 2, 6.5, "mobile", FA_PHONE, pt, 0)); py += 7;
        d.push(F (mainX + c, py, c - 2, 6.5, "doctor", FA_DOCTOR, pt, 0));
        d.push(F (mainX    , py, c - 2, 6.5, "dept", FA_DEPT, pt, 0));
        my += bh3 + 4;
      }
      my = servicesBlockAt(d, mainX, mainW, my, pt, sp.s, sp.hf, sp.bw, sp.rh,
                           30, true, footY - 2);
      ly = footY - 30; lw = mainW * 0.60; lx = mainX + mainW - lw;
      d.push(HL(mainX, ly, mainW, 0.6, sp.a)); ly += 2;
      d.push(F(lx, ly, lw, 6.2, "total", FA_TOTAL, pt, 0));       ly += 6.2;
      d.push(F(lx, ly, lw, 6.2, "insshare", FA_INSSHARE, pt, 0)); ly += 6.2;
      d.push(F(lx, ly, lw, 6.2, "discount", FA_DISCOUNT, pt, 0)); ly += 6.2;
      d.push(HL(lx, ly - 0.5, lw, 0.5, sp.a));
      f = FB(lx, ly, lw, 7, "paid", FA_FINAL, pt + 1.5, 0);
      f.textColor = sp.a; d.push(f);
      d.push(HL(mainX, footY - 10, mainW * 0.34, 0.3));
      d.push(L(mainX, footY - 9.5, mainW * 0.34, 5, FA_SIGN, 8.5, false, 1));
      break;

    /* ---------------- 4 — خط‌کشی (mono) -------------------------------- */
    case 4:
      if (sp.v === 0)      y = hdrPlain(d, FA_CLINIC, 17, false, 0.9);
      else if (sp.v === 1) y = hdrPlain(d, FA_CLINIC, 19, true, 1.2);
      else                 y = hdrLogoRight(d, FA_CLINIC, 16, false, 0.9);
      if (sp.v === 0) {                 /* ruled meta line (pure line-art) */
        c = PG_CW / 4;
        d.push(FB(PG_M + 3 * c, y, c, 6.5, "receiptNo", FA_RECEIPT, pt - 0.5, 0));
        d.push(F (PG_M + 2 * c, y, c, 6.5, "date", FA_DATE, pt - 0.5, 1));
        d.push(F (PG_M +     c, y, c, 6.5, "time", FA_TIME, pt - 0.5, 1));
        d.push(F (PG_M        , y, c, 6.5, "shift", FA_SHIFT, pt - 0.5, 2));
        y += 7.5;
        d.push(HL(PG_M, y, PG_CW, 0.25)); y += 3;
      } else if (sp.v === 1) {
        y = metaStripBoxed(d, y, pt - 0.5, sp.t, sp.a);
      } else {
        y = metaGrid6(d, y, pt - 1, sp.a);
      }
      if (sp.v === 0)      y = patient3x3(d, y, pt);
      else if (sp.v === 1) y = patient2x3(d, y, pt);
      else                 y = patientCard(d, y, pt - 0.5, sp.t, sp.a, sp.a);
      d.push(HL(PG_M, y - 1, PG_CW, 0.25)); y += 2;
      var reserve4 = (sp.v === 0) ? 16 : (sp.v === 1) ? 24 : 32;
      y = servicesBlock(d, y, pt, sp.s, 0, sp.bw, sp.rh, reserve4, true, footY);
      if (sp.v === 0)      totalsRow(d, footY - 14, pt);
      else if (sp.v === 1) totalsBar(d, footY - 22, pt, sp.a);
      else                 totalsBox(d, footY - 31, pt, sp.t, sp.a, sp.a);
      if (sp.v === 0)      footMinimal(d, footY + 1);
      else if (sp.v === 1) footClean(d, footY + 2, sp.a);
      else                 footBarcode(d, footY + 2);
      break;

    /* ---------------- 5 — کارتی ---------------------------------------- */
    case 5:
      d.push(RECT(PG_M - 2, PG_M - 2, PG_CW + 4, PG_H - 2 * PG_M + 4, 0.5, 4.0, sp.a));
      y = PG_M + 2;
      if (sp.v === 0) {
        d.push(LOGO(PG_M + PG_CW - 22, y, 20, 20));
        d.push(LC(PG_M + 2, y + 1, PG_CW - 26, 10, FA_CLINIC, 16.5, true, 0, sp.a));
        d.push(L(PG_M + 2, y + 11.5, PG_CW - 26, 6, FA_SUBTITLE, 8.5, false, 0));
        y += 23;
      } else if (sp.v === 1) {
        d.push(LC(PG_M + 2, y, PG_CW - 4, 10, FA_CLINIC, 17, true, 1, sp.a));
        d.push(L(PG_M + 2, y + 11, PG_CW - 4, 6, FA_SUBTITLE, 8.5, false, 1));
        y += 20;
      } else {
        d.push(BAND(PG_M, y, PG_CW, 15, sp.a));
        d.push(LC(PG_M + 3, y + 3.2, PG_CW - 6, 8, FA_CLINIC, 15, true, 0, "#ffffff"));
        y += 18;
      }
      if (sp.v === 0)      y = metaStripBoxed(d, y, pt - 0.5, sp.t, sp.a);
      else if (sp.v === 1) y = metaStrip4(d, y, pt - 0.5);
      else                 y = metaGrid6(d, y, pt - 1, sp.a);
      y = patientCard(d, y, pt - 0.5, sp.t, sp.a, sp.a);
      var reserve5 = (sp.v === 0) ? 32 : (sp.v === 1) ? 24 : 36;
      y = servicesBlock(d, y, pt, sp.s, sp.hf, sp.bw, sp.rh, reserve5, true, footY);
      if (sp.v === 0)      totalsBox(d, footY - 31, pt, sp.t, sp.a, sp.a);
      else if (sp.v === 1) totalsBar(d, footY - 22, pt, sp.a);
      else                 totalsLadder(d, footY - 34, pt, sp.a);
      if (sp.v === 0)      footDual(d, footY + 1, sp.t, sp.a);
      else if (sp.v === 1) footBarcode(d, footY + 2);
      else                 footClean(d, footY + 2, sp.a);
      break;

    /* ---------------- 6 — عکس و هویت ----------------------------------- */
    case 6:
      if (sp.v === 0)      y = hdrLogoRight(d, FA_CLINIC, 17, true, 0.7);
      else if (sp.v === 1) y = hdrBand(d, FA_CLINIC, sp.a, true, "پروندهٔ پذیرش بیمار");
      else                 y = hdrCenterLogo(d, FA_CLINIC, 17, true, 0.5);
      if (sp.v === 0)      y = metaStrip4(d, y, pt - 0.5);
      else if (sp.v === 1) y = metaStripBoxed(d, y, pt - 0.5, sp.t, sp.a);
      else                 y = metaGrid6(d, y, pt - 1, sp.a);
      y = patientPhoto(d, y, pt - 0.5);
      d.push(HL(PG_M, y - 1, PG_CW, 0.4, sp.a)); y += 2;
      var reserve6 = (sp.v === 0) ? 32 : (sp.v === 1) ? 36 : 24;
      y = servicesBlock(d, y, pt, sp.s, sp.hf, sp.bw, sp.rh, reserve6, true, footY);
      if (sp.v === 0)      totalsBox(d, footY - 31, pt, sp.t, sp.a, sp.a);
      else if (sp.v === 1) totalsLadder(d, footY - 34, pt, sp.a);
      else                 totalsBar(d, footY - 22, pt, sp.a);
      if (sp.v === 0)      footBarcode(d, footY + 2);
      else if (sp.v === 1) footCentered(d, footY + 3);
      else                 footClean(d, footY + 2, sp.a);
      break;

    /* ---------------- 7 — مالی ----------------------------------------- */
    case 7:
      doc = (sp.v === 0) ? "صورت‌حساب مالی خدمات"
          : (sp.v === 1) ? "ریز صورت‌حساب و سهم بیمه"
                         : "صورت‌حساب تفصیلی مالی";
      y = hdrSplit(d, FA_CLINIC, sp.a, doc);
      if (sp.v === 0)      y = metaStripBoxed(d, y, pt - 0.5, sp.t, sp.a);
      else if (sp.v === 1) y = metaStrip4(d, y, pt - 0.5);
      else                 y = metaGrid6(d, y, pt - 1, sp.a);
      c = PG_CW / 3;
      d.push(FB(PG_M + 2 * c, y, c - 2, 7, "fullName", FA_FULL, pt, 0));
      d.push(F (PG_M +     c, y, c - 2, 7, "nationalCode", FA_NID, pt, 0));
      d.push(F (PG_M        , y, c - 2, 7, "patientType", FA_PTYPE, pt, 0)); y += 8;
      d.push(F (PG_M + 2 * c, y, c - 2, 7, "insurance", FA_INS, pt, 0));
      d.push(F (PG_M +     c, y, c - 2, 7, "ins_percent", "درصد پایه: ", pt, 0));
      d.push(F (PG_M        , y, c - 2, 7, "insNo", "شماره بیمه: ", pt, 0)); y += 8;
      d.push(F (PG_M + 2 * c, y, c - 2, 7, "suppInsurance", FA_SUPP, pt, 0));
      d.push(F (PG_M +     c, y, c - 2, 7, "supp_percent", "درصد مکمل: ", pt, 0));
      d.push(F (PG_M        , y, c - 2, 7, "doctor", FA_DOCTOR, pt, 0)); y += 10;
      var reserve7 = (sp.v === 0) ? 36 : (sp.v === 1) ? 24 : 32;
      y = servicesBlock(d, y, pt, sp.s, sp.hf, sp.bw, sp.rh, reserve7, true, footY);
      if (sp.v === 0)      totalsLadder(d, footY - 34, pt, sp.a);
      else if (sp.v === 1) totalsBar(d, footY - 22, pt, sp.a);
      else                 totalsBox(d, footY - 31, pt, sp.t, sp.a, sp.a);
      if (sp.v === 0)      footBarcode(d, footY + 2);
      else if (sp.v === 1) footCentered(d, footY + 3);
      else                 footClean(d, footY + 2, sp.a);
      break;

    /* ---------------- 8 — فشرده + ته‌برگ ------------------------------- */
    case 8:
      tear = 196.0;
      if (sp.v === 0)      y = hdrPlain(d, FA_CLINIC, 16.5, false, 0.6);
      else if (sp.v === 1) y = hdrLogoRight(d, FA_CLINIC, 16, false, 0.6);
      else                 y = hdrBand(d, FA_CLINIC, sp.a, true, "");
      if (sp.v === 0)      y = metaStrip4(d, y, pt - 1);
      else if (sp.v === 1) y = metaStripBoxed(d, y, pt - 1, sp.t, sp.a);
      else                 y = metaGrid6(d, y, pt - 1, sp.a);
      if (sp.v === 0)      y = patient2x3(d, y, pt - 0.5);
      else if (sp.v === 1) y = patient3x3(d, y, pt - 0.5);
      else                 y = patientCard(d, y, pt - 0.5, sp.t, sp.a, sp.a);
      var reserve8 = (sp.v === 0) ? 15 : (sp.v === 1) ? 24 : 32;
      y = servicesBlock(d, y, pt - 0.5, sp.s, sp.hf, sp.bw, sp.rh, reserve8, true, tear - 4);
      if (sp.v === 0)      totalsRow(d, tear - 17, pt - 0.5);
      else if (sp.v === 1) totalsBar(d, tear - 24, pt - 0.5, sp.a);
      else                 totalsBox(d, tear - 33, pt - 0.5, sp.t, sp.a, sp.a);
      d.push(HL(PG_M, tear, PG_CW, 0.3));
      d.push(L(PG_M, tear + 0.8, PG_CW, 5, "— — — محل جدا کردن — — —", 8, false, 1));
      sy = tear + 8;
      d.push(LC(PG_M, sy, PG_CW, 8, "نسخهٔ بیمار — رسید پرداخت", 12.5, true, 1, sp.a));
      sy += 10;
      d.push(TINT(PG_M, sy, PG_CW, 26, sp.t, sp.a, 0.35, 1.5));
      c = PG_CW / 3;
      d.push(FB(PG_M + 2 * c + 1.5, sy + 2, c - 3, 6.5, "fullName", FA_FULL, pt - 0.5, 0));
      d.push(F (PG_M +     c + 1.5, sy + 2, c - 3, 6.5, "receiptNo", FA_RECEIPT, pt - 0.5, 0));
      d.push(F (PG_M         + 1.5, sy + 2, c - 3, 6.5, "date", FA_DATE, pt - 0.5, 0));
      d.push(F (PG_M + 2 * c + 1.5, sy + 9.5, c - 3, 6.5, "servicescount", FA_ROWS, pt - 0.5, 0));
      d.push(F (PG_M +     c + 1.5, sy + 9.5, c - 3, 6.5, "total", FA_TOTAL, pt - 0.5, 0));
      d.push(F (PG_M         + 1.5, sy + 9.5, c - 3, 6.5, "insshare", FA_INSSHARE, pt - 0.5, 0));
      f = FB(PG_M + 1.5, sy + 17, PG_CW - 3, 7, "paid", FA_FINAL, pt + 1, 0);
      f.textColor = sp.a; d.push(f);
      /* v1.65.0: barcode is part of the patient stub now (used to float alone
         at the very bottom, detached from both copies). v1.69.0: variant 1
         carries NO code — a clean contact + keep-note line instead. */
      ly = footY - 20;
      if (sp.v !== 1) {
        lw = PG_CW * 0.40; lx = PG_M + PG_CW - lw;
        d.push(BARCODE(lx, ly, lw, 14));
        d.push(L(lx, ly + 14.5, lw, 4, FA_BARCODE, 8, false, 1));
        d.push(F(PG_M, ly + 1, PG_CW * 0.52, 6, "clinicphone", FA_PHONE, 8.5, 0));
        d.push(F(PG_M, ly + 8, PG_CW * 0.52, 6, "clinicaddr", "نشانی: ", 8.5, 0));
      } else {
        d.push(F(PG_M, ly + 1, PG_CW * 0.60, 6, "clinicphone", FA_PHONE, 8.5, 0));
        d.push(F(PG_M, ly + 8, PG_CW * 0.60, 6, "clinicaddr", "نشانی: ", 8.5, 0));
        d.push(LC(PG_M + PG_CW * 0.62, ly + 1, PG_CW * 0.38, 6, FA_KEEP, 8.5, false, 2, sp.a));
      }
      break;

    /* ---------------- 9 — دو بلوکی ------------------------------------- */
    default:
      if (sp.v === 0)      y = hdrBand(d, FA_CLINIC, sp.a, true, "");
      else if (sp.v === 1) y = hdrPlain(d, FA_CLINIC, 18, true, 0.8);
      else                 y = hdrCenterLogo(d, FA_CLINIC, 17, true, 0.6);
      gw = PG_CW / 2 - 3; lx2 = PG_M; rx = PG_M + gw + 6; bh = 38;
      d.push(TINT(rx, y, gw, bh, sp.t, sp.a, 0.35, 1.5));
      d.push(BAND(rx, y, gw, 6.5, sp.a));
      d.push(LC(rx + 2, y + 0.6, gw - 4, 5, "مشخصات بیمار", 9, true, 0, "#ffffff"));
      ry = y + 8;
      d.push(FB(rx + 2, ry, gw - 4, 6.5, "fullName", FA_FULL, pt - 0.5, 0)); ry += 7;
      d.push(F (rx + 2, ry, gw - 4, 6.5, "nationalCode", FA_NID, pt - 0.5, 0)); ry += 7;
      d.push(F (rx + 2, ry, gw - 4, 6.5, "birthDate", FA_BIRTH, pt - 0.5, 0)); ry += 7;
      d.push(F (rx + 2, ry, gw - 4, 6.5, "mobile", FA_PHONE, pt - 0.5, 0));
      d.push(TINT(lx2, y, gw, bh, sp.t, sp.a, 0.35, 1.5));
      d.push(BAND(lx2, y, gw, 6.5, sp.a));
      d.push(LC(lx2 + 2, y + 0.6, gw - 4, 5, "اطلاعات پذیرش", 9, true, 0, "#ffffff"));
      ly2 = y + 8;
      d.push(FB(lx2 + 2, ly2, gw - 4, 6.5, "receiptNo", FA_RECEIPT, pt - 0.5, 0)); ly2 += 7;
      d.push(F (lx2 + 2, ly2, gw - 4, 6.5, "date", FA_DATE, pt - 0.5, 0)); ly2 += 7;
      d.push(F (lx2 + 2, ly2, gw - 4, 6.5, "shift", FA_SHIFT, pt - 0.5, 0)); ly2 += 7;
      d.push(F (lx2 + 2, ly2, gw - 4, 6.5, "doctor", FA_DOCTOR, pt - 0.5, 0));
      y += bh + 4;
      c = PG_CW / 3;
      d.push(F(PG_M + 2 * c, y, c - 2, 7, "insurance", FA_INS, pt, 0));
      d.push(F(PG_M +     c, y, c - 2, 7, "suppInsurance", FA_SUPP, pt, 0));
      d.push(F(PG_M        , y, c - 2, 7, "patientType", FA_PTYPE, pt, 0));
      y += 9;
      var reserve9 = (sp.v === 0) ? 24 : (sp.v === 1) ? 36 : 32;
      y = servicesBlock(d, y, pt, sp.s, sp.hf, sp.bw, sp.rh, reserve9, true, footY);
      if (sp.v === 0)      totalsBar(d, footY - 22, pt, sp.a);
      else if (sp.v === 1) totalsLadder(d, footY - 34, pt, sp.a);
      else                 totalsBox(d, footY - 31, pt, sp.t, sp.a, sp.a);
      if (sp.v === 0)      footBarcode(d, footY + 3);
      else if (sp.v === 1) footCentered(d, footY + 3);
      else                 footClean(d, footY + 2, sp.a);
      break;
    }

    /* silence unused-var linters in ES5 strict mode */
    pwB = PG_W; phB = PG_H; if (pwB && phB) { /* noop */ }
    return d;
  }

  var ALL = [];
  for (var i = 0; i < 30; i++) {
    _uid = 0;
    var items = buildTemplate(i);
    for (var j = 0; j < items.length; j++) { items[j].id = j + 1; items[j].z = j + 1; }
    ALL.push({ id: 0, name: NAMES[i], kind: "builtin", group: "reception",
      paper: "A4", orientation: 0, items: items });
  }

  window.AZ_TEMPLATES = ALL;
})();
