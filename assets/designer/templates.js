/* ===========================================================================
   templates.js — Azadi-Teb ready-made print designs  (v1.55.0 — FULL REWRITE)

   ★ همهٔ طرح‌های آمادهٔ قبلی حذف شده‌اند و ۳۰ طرح کاملاً جدید ساخته شده است.

   مشخصات مشترک هر ۳۰ طرح (بر اساس رسید کاغذی واقعی «درمانگاه شبانه‌روزی
   ثامن‌الائمه»):
     • گروه: reception            — همه در دستهٔ «پذیرش»
     • کاغذ: A4 عمودی (portrait)  — با موتور واکنش‌گرا به A5/A6 هم می‌ریزد
     • تک‌رنگ (MONOCHROME)        — فقط مشکی/خاکستری/سفید؛ هیچ رنگی به کار
                                     نرفته تا روی هر چاپگر سیاه‌وسفید عیناً
                                     مانند پیش‌نمایش چاپ شود
     • جدول خدمات صحیح            — ستون‌ها از راست به چپ:
                                     «نام خدمت | شرح خدمت | تعداد»
                                     با عرض‌های [۰٫۵۵ ، ۰٫۳۰ ، ۰٫۱۵]
     • بدون «نام پدر» و بدون «آدرس»
     • RTL واقعی                  — همهٔ آیتم‌ها align:0 (راست‌چین) و dir:0
                                     (راست‌به‌چپ)؛ فقط عنوان‌های وسط‌چین
                                     align:1 / dir:2 هستند
     • هیچ مقدار تصادفی‌ای وجود ندارد — هر مقدار از توکن دادهٔ زندهٔ رکورد
       پذیرش می‌آید و در printer.cpp → pdFieldValue() پاسخ داده می‌شود
   =========================================================================== */
(function () {
  "use strict";

  /* ---------------------------------------------------------------- palette */
  // تک‌رنگ: تنها این هشت مقدار در کل فایل استفاده می‌شود (هیچ رنگی نیست).
  var INK      = "#000000";   // متن اصلی / خطوط اصلی
  var INK_SOFT = "#333333";   // متن ثانویه
  var INK_DIM  = "#5a5a5a";   // برچسب‌های کوچک
  var RULE     = "#000000";   // خط‌کشی
  var RULE_DIM = "#8a8a8a";   // خط‌کشی نازک
  var BAND     = "#e9e9e9";   // نوار خاکستری روشن (سربرگ جدول/باندها)
  var BAND_DK  = "#d0d0d0";   // نوار خاکستری تیره‌تر
  var PAPER_BG = "#ffffff";

  var _uid = 0;
  function nid() { return ++_uid; }

  /* ------------------------------------------------------------- primitives */
  function base() {
    return {
      id: nid(), type: "label", x: 10, y: 10, w: 40, h: 7, rot: 0, z: 1,
      locked: false, isFrame: false, text: "", field: "", prefix: "", suffix: "",
      font: "Vazirmatn", pt: 9.5, bold: false, italic: false,
      align: 0, dir: 0, valign: 1, lineSpacing: 1.25,
      textColor: INK, fillColor: PAPER_BG, fillTransparent: true,
      borderColor: RULE, borderWidth: 0.3, corner: 0, padding: 0.8, opacity: 1,
      visibility: 0, startValue: 1, step: 1, imgPath: "",
      rowH: 0, headerH: 0
    };
  }
  function mk(o) { return Object.assign(base(), o); }

  // متن ثابت راست‌چین
  function L(x, y, w, h, text, o) {
    return mk(Object.assign({ type: "label", x: x, y: y, w: w, h: h, text: text, align: 0, dir: 0 }, o || {}));
  }
  // عنوان وسط‌چین
  function T(x, y, w, h, text, o) {
    return mk(Object.assign({ type: "label", x: x, y: y, w: w, h: h, text: text, align: 1, dir: 2, bold: true }, o || {}));
  }
  // فیلد دادهٔ راست‌چین
  function F(x, y, w, h, field, o) {
    return mk(Object.assign({ type: "field", x: x, y: y, w: w, h: h, field: field, align: 0, dir: 0 }, o || {}));
  }
  // «برچسب: مقدار» در یک سطر — راست‌چین (برچسب اول، مقدار به سمت چپ)
  function PV(x, y, w, lbl, field, o) {
    o = o || {};
    return F(x, y, w, o.h || 6.2, field, Object.assign({
      prefix: lbl + " ", align: 0, dir: 0, pt: o.pt || 9,
      bold: !!o.bold, textColor: o.color || INK, visibility: o.visibility || 0
    }, o));
  }
  function HL(x, y, w, o) {
    return mk(Object.assign({ type: "hline", x: x, y: y, w: w, h: 0.4,
      borderWidth: (o && o.bw) || 0.3, borderColor: (o && o.rc) || RULE }, o || {}));
  }
  function VL(x, y, h, o) {
    return mk(Object.assign({ type: "vline", x: x, y: y, w: 0.4, h: h,
      borderWidth: (o && o.bw) || 0.3, borderColor: (o && o.rc) || RULE }, o || {}));
  }
  function R(x, y, w, h, o) {
    return mk(Object.assign({ type: "rect", x: x, y: y, w: w, h: h,
      borderWidth: (o && o.bw) || 0.3, borderColor: (o && o.rc) || RULE }, o || {}));
  }
  // نوار پرشدهٔ خاکستری (تک‌رنگ)
  function BANDBOX(x, y, w, h, tone, o) {
    return mk(Object.assign({ type: "rect", x: x, y: y, w: w, h: h,
      fillTransparent: false, fillColor: tone || BAND,
      borderWidth: (o && o.bw) != null ? o.bw : 0, borderColor: (o && o.rc) || RULE,
      corner: (o && o.corner) != null ? o.corner : 0 }, o || {}));
  }
  function FRAME(W, H, m, o) {
    m = m || 5;
    return mk(Object.assign({ type: "frame", x: m, y: m, w: W - 2 * m, h: H - 2 * m,
      isFrame: true, borderWidth: (o && o.bw) || 0.5, borderColor: RULE }, o || {}));
  }
  function LOGO(x, y, w, h) {
    return mk({ type: "logo", x: x, y: y, w: w, h: h || w, borderColor: RULE_DIM, borderWidth: 0.3 });
  }
  function QR(x, y, s) {
    return mk({ type: "qr", x: x, y: y, w: s, h: s, field: "{receiptbarcode}",
      borderColor: RULE_DIM, borderWidth: 0.3 });
  }
  function PHOTO(x, y, w, h) {
    return mk({ type: "photo", x: x, y: y, w: w, h: h, borderColor: RULE_DIM, borderWidth: 0.3 });
  }
  function APPTNO(x, y, w, h, o) {
    return mk(Object.assign({ type: "apptno", x: x, y: y, w: w, h: h,
      pt: (o && o.pt) || 26, bold: true, align: 1, dir: 1, startValue: 1, step: 1 }, o || {}));
  }

  /* --------------------------------------------------- REAL barcode (v1.55) */
  // بارکد خطی واقعی و قابل اسکن. payload از توکن دادهٔ زنده می‌آید (پیش‌فرض
  // {receiptbarcode} که به‌طور قطعی از شمارهٔ قبض ساخته می‌شود) و عدد آن هم
  // زیر میله‌ها چاپ می‌شود. هرگز تصادفی نیست.
  function BARCODE(x, y, w, h, o) {
    o = o || {};
    return mk({
      type: "barcode", x: x, y: y, w: w, h: h,
      field: o.field || "{receiptbarcode}",
      text: JSON.stringify({ sym: o.sym || "code128", hri: o.hri !== false, quiet: o.quiet != null ? o.quiet : 2 }),
      pt: o.pt || 8, align: 1, dir: 1,
      textColor: INK, fillTransparent: true, borderWidth: 0
    });
  }

  /* ------------------------------------------- SERVICES LIST (v1.55 — core) */
  // جدول پویای خدمات. ستون‌ها از راست به چپ دقیقاً:
  //     «نام خدمت | شرح خدمت | تعداد»   با عرض [0.55, 0.30, 0.15]
  // سطرها هنگام چاپ از ReceptionRecord.services پر می‌شوند (تعداد متغیر).
  // rowH/headerH ارتفاع دقیق سطر و سرستون را (میلی‌متر) قفل می‌کند تا جدول
  // در هر کاغذی یکسان چاپ شود؛ ۰ به معنی «خودکار» است.
  var SVC_LABELS = ["نام خدمت", "شرح خدمت", "تعداد"];
  var SVC_WIDTHS = [0.55, 0.30, 0.15];
  function SERVICES(x, y, w, h, o) {
    o = o || {};
    var labels = o.labels || SVC_LABELS;
    var widths = o.widths || SVC_WIDTHS;
    return mk({
      type: "services", x: x, y: y, w: w, h: h,
      pt: o.pt || 9,
      borderColor: o.rc || RULE, borderWidth: o.bw != null ? o.bw : 0.3,
      fillColor: o.band || BAND,
      fillTransparent: o.band === false,       // true → جدول کاملاً خط‌کشی، بدون سایه
      textColor: INK,
      rowH: o.rowH != null ? o.rowH : 6.5,
      headerH: o.headerH != null ? o.headerH : 7.5,
      text: JSON.stringify({ cols: labels.length, header: o.header !== false, widths: widths, labels: labels })
    });
  }

  /* ------------------------------------------------------------ collections */
  var ALL = [];
  function push(group, name, paper, orient, items) {
    var flat = [];
    (function walk(a) {
      for (var i = 0; i < a.length; i++) {
        if (Array.isArray(a[i])) walk(a[i]);
        else if (a[i]) flat.push(a[i]);
      }
    })(items);
    flat.forEach(function (it, i) { it.id = i + 1; it.z = i + 1; });
    ALL.push({ id: 0, name: name, kind: "builtin", group: group,
      paper: paper, orientation: orient || 0, items: flat });
  }

  /* ======================================================================== *
   *  BLOCK BUILDERS  (T_HEADER / T_APPT / T_PATIENT / T_DOCTOR /
   *                   T_SERVICES / T_FINANCE / T_FOOTER)
   *  هر بلوک آرایه‌ای از آیتم‌ها برمی‌گرداند و ارتفاع مصرفی‌اش را در
   *  ctx.y جلو می‌برد، بنابراین طرح‌ها به‌سادگی از ترکیب بلوک‌ها ساخته
   *  می‌شوند و همه دقیقاً هم‌تراز می‌مانند.
   * ======================================================================== */

  /* ---- سربرگ درمانگاه ---------------------------------------------------- */
  //  style: 0 = خط‌کشی ساده، 1 = نوار خاکستری، 2 = دو خط موازی،
  //         3 = کادر دور سربرگ، 4 = بدون لوگو/مینیمال
  function T_HEADER(c, style, o) {
    o = o || {};
    var x = c.x, w = c.w, y = c.y, it = [];
    var hh = o.h || 26;

    if (style === 1) it.push(BANDBOX(x, y, w, hh, BAND));
    if (style === 3) it.push(R(x, y, w, hh, { bw: 0.5 }));

    var logoW = (style === 4) ? 0 : (o.logo || 17);
    if (logoW) it.push(LOGO(x + w - logoW - 1.5, y + 1.5, logoW, logoW));

    var txtW = w - logoW - 4;
    it.push(T(x + 2, y + 2, txtW, 8, "درمانگاه شبانه‌روزی ثامن‌الائمه",
      { pt: o.pt || 15, bold: true, textColor: INK }));
    it.push(T(x + 2, y + 10, txtW, 5, "مرکز جامع سلامت — پذیرش و درمان ۲۴ ساعته",
      { pt: 8, bold: false, textColor: INK_DIM }));
    it.push(PV(x + 2, y + 15.5, txtW * 0.5, "تلفن:", "{clinicphone}", { pt: 7.6, color: INK_DIM }));
    it.push(PV(x + 2 + txtW * 0.52, y + 15.5, txtW * 0.46, "پروانه:", "{cliniclic}", { pt: 7.6, color: INK_DIM }));
    it.push(PV(x + 2, y + 20, txtW, "مسئول فنی:", "{clinicmgr}", { pt: 7.6, color: INK_DIM, visibility: 1 }));

    if (style === 0) it.push(HL(x, y + hh, w, { bw: 0.6 }));
    if (style === 2) { it.push(HL(x, y + hh, w, { bw: 0.7 })); it.push(HL(x, y + hh + 1.2, w, { bw: 0.25 })); }

    c.y = y + hh + (style === 2 ? 4 : 3);
    return it;
  }

  /* ---- عنوان سند -------------------------------------------------------- */
  function T_TITLE(c, txt, style) {
    var x = c.x, w = c.w, y = c.y, it = [];
    if (style === 1) {
      it.push(BANDBOX(x, y, w, 8.5, BAND_DK));
      it.push(T(x, y + 0.6, w, 7.4, txt, { pt: 12 }));
    } else if (style === 2) {
      it.push(R(x + w * 0.22, y, w * 0.56, 8.5, { bw: 0.5 }));
      it.push(T(x + w * 0.22, y + 0.6, w * 0.56, 7.4, txt, { pt: 12 }));
    } else {
      it.push(T(x, y, w, 8, txt, { pt: 13 }));
      it.push(HL(x + w * 0.3, y + 8.2, w * 0.4, { bw: 0.4 }));
    }
    c.y = y + 11.5;
    return it;
  }

  /* ---- نوار نوبت / شمارهٔ قبض / زمان ------------------------------------ */
  //  style: 0 = سه سلول کادردار، 1 = نوار خاکستری، 2 = جعبهٔ بزرگ شمارهٔ نوبت
  function T_APPT(c, style) {
    var x = c.x, w = c.w, y = c.y, it = [];
    if (style === 2) {
      var bw2 = w * 0.3;
      it.push(R(x + w - bw2, y, bw2, 24, { bw: 0.6 }));
      it.push(L(x + w - bw2, y + 1.2, bw2, 4.5, "شمارهٔ نوبت", { align: 1, dir: 2, pt: 8, textColor: INK_DIM }));
      it.push(APPTNO(x + w - bw2, y + 5.5, bw2, 13, { pt: 30 }));
      it.push(PV(x + w - bw2 + 2, y + 19, bw2 - 4, "ش.قبض:", "{receiptNo}", { pt: 8 }));
      var rw = w - bw2 - 3;
      it.push(PV(x, y + 1, rw, "تاریخ و ساعت نوبت:", "{apptdatetime}", { pt: 9.5, bold: true }));
      it.push(PV(x, y + 7, rw, "ثبت پذیرش:", "{reg_ts}", { pt: 8.6 }));
      it.push(PV(x, y + 12.5, rw * 0.5, "شیفت:", "{shift}", { pt: 8.6 }));
      it.push(PV(x + rw * 0.52, y + 12.5, rw * 0.46, "ش.ص:", "{scnum}", { pt: 8.6 }));
      it.push(PV(x, y + 18, rw, "کد رسید:", "{receiptcode}", { pt: 8.6 }));
      c.y = y + 27;
    } else if (style === 1) {
      it.push(BANDBOX(x, y, w, 9.5, BAND));
      var cw = w / 4;
      it.push(PV(x + 3 * cw + 1.5, y + 1.4, cw - 3, "نوبت:", "{queue}", { pt: 9, bold: true }));
      it.push(PV(x + 2 * cw + 1.5, y + 1.4, cw - 3, "ش.قبض:", "{receiptNo}", { pt: 9 }));
      it.push(PV(x + 1 * cw + 1.5, y + 1.4, cw - 3, "تاریخ:", "{apptdate}", { pt: 9 }));
      it.push(PV(x + 1.5, y + 1.4, cw - 3, "ساعت:", "{apptsec}", { pt: 9 }));
      it.push(PV(x, y + 10.5, w * 0.48, "ثبت پذیرش:", "{reg_ts}", { pt: 8.4, color: INK_DIM }));
      it.push(PV(x + w * 0.52, y + 10.5, w * 0.48, "شیفت / ش.ص:", "{shiftuser}", { pt: 8.4, color: INK_DIM }));
      c.y = y + 18;
    } else {
      var n = 4, cw2 = w / n;
      for (var i = 0; i < n; i++) it.push(R(x + i * cw2, y, cw2, 13, { bw: 0.35 }));
      function box(idx, lbl, fld, big) {
        var bx = x + (n - 1 - idx) * cw2;
        it.push(L(bx + 1.5, y + 1, cw2 - 3, 4, lbl, { pt: 7.2, align: 1, dir: 2, textColor: INK_DIM }));
        it.push(F(bx + 1.5, y + 5.2, cw2 - 3, 6.6, fld, { pt: big ? 13 : 9.6, bold: true, align: 1, dir: 2 }));
      }
      box(0, "شمارهٔ نوبت", "{queue}", true);
      box(1, "شمارهٔ قبض", "{receiptNo}", false);
      box(2, "تاریخ نوبت", "{apptdate}", false);
      box(3, "ساعت", "{apptsec}", false);
      it.push(PV(x, y + 14.4, w * 0.48, "ثبت پذیرش:", "{reg_ts}", { pt: 8.2, color: INK_DIM }));
      it.push(PV(x + w * 0.52, y + 14.4, w * 0.48, "کد رسید:", "{receiptcode}", { pt: 8.2, color: INK_DIM }));
      c.y = y + 22;
    }
    return it;
  }

  /* ---- مشخصات بیمار (بدون نام پدر و بدون آدرس) ------------------------- */
  //  style: 0 = دو ستونه کادردار، 1 = سطری با خط‌چین، 2 = شبکه‌ای ۳ ستونه،
  //         3 = با عکس پرسنلی
  function T_PATIENT(c, style, o) {
    o = o || {};
    var x = c.x, w = c.w, y = c.y, it = [];
    var head = o.head !== false;
    if (head) {
      if (o.headBand) { it.push(BANDBOX(x, y, w, 6.4, BAND)); it.push(L(x + 2, y + 0.7, w - 4, 5, "مشخصات بیمار", { pt: 9.6, bold: true })); }
      else { it.push(L(x, y, w, 5.6, "مشخصات بیمار", { pt: 9.6, bold: true })); it.push(HL(x, y + 6, w, { bw: 0.4 })); }
      y += 8;
    }

    if (style === 3) {
      var pw = 22, ph = 28;
      it.push(PHOTO(x + w - pw, y, pw, ph));
      it.push(L(x + w - pw, y + ph + 0.4, pw, 3.6, "عکس بیمار", { pt: 6.6, align: 1, dir: 2, textColor: INK_DIM }));
      var iw = w - pw - 3, ic = iw / 2 - 1.5;
      var rows = [
        ["نام و نام خانوادگی:", "{full}", true], ["کد ملی:", "{nid}", false],
        ["تاریخ تولد:", "{birth}", false], ["سن:", "{age}", false],
        ["جنسیت:", "{gender}", false], ["تلفن همراه:", "{mobile}", false],
        ["نوع بیمار:", "{ptype}", false], ["تلفن ثابت:", "{landline}", false]
      ];
      for (var r = 0; r < rows.length; r++) {
        var cx = x + (r % 2 === 0 ? iw - ic : 0), cy = y + Math.floor(r / 2) * 6.6;
        it.push(PV(cx, cy, ic, rows[r][0], rows[r][1], { pt: 9, bold: rows[r][2] }));
      }
      c.y = y + Math.max(ph + 5, Math.ceil(rows.length / 2) * 6.6) + 3;
      return it;
    }

    if (style === 2) {
      var n3 = 3, cw3 = w / n3;
      var g = [
        ["نام و نام خانوادگی", "{full}"], ["کد ملی", "{nid}"], ["سن", "{age}"],
        ["تاریخ تولد", "{birth}"], ["جنسیت", "{gender}"], ["نوع بیمار", "{ptype}"],
        ["تلفن همراه", "{mobile}"], ["تلفن ثابت", "{landline}"], ["کد رهگیری", "{eprescription}"]
      ];
      for (var k = 0; k < g.length; k++) {
        var rr = Math.floor(k / n3), cc = k % n3;
        var bx = x + (n3 - 1 - cc) * cw3, by = y + rr * 11.5;
        it.push(R(bx, by, cw3, 11.5, { bw: 0.3, rc: RULE_DIM }));
        it.push(L(bx + 1.6, by + 0.9, cw3 - 3.2, 3.8, g[k][0], { pt: 6.8, textColor: INK_DIM }));
        it.push(F(bx + 1.6, by + 4.6, cw3 - 3.2, 6, g[k][1], { pt: 9.2, bold: true }));
      }
      c.y = y + Math.ceil(g.length / n3) * 11.5 + 3;
      return it;
    }

    if (style === 1) {
      var lines = [
        [["نام و نام خانوادگی:", "{full}", 0.52, true], ["کد ملی:", "{nid}", 0.48, false]],
        [["تاریخ تولد:", "{birth}", 0.34, false], ["سن:", "{age}", 0.2, false], ["جنسیت:", "{gender}", 0.22, false], ["نوع بیمار:", "{ptype}", 0.24, false]],
        [["تلفن همراه:", "{mobile}", 0.5, false], ["تلفن ثابت:", "{landline}", 0.5, false]]
      ];
      var yy = y;
      for (var li = 0; li < lines.length; li++) {
        var acc = 0;
        for (var ci = 0; ci < lines[li].length; ci++) {
          var seg = lines[li][ci], sw = w * seg[2];
          it.push(PV(x + w - acc - sw, yy, sw - 2, seg[0], seg[1], { pt: 9.2, bold: seg[3] }));
          acc += sw;
        }
        it.push(HL(x, yy + 6.6, w, { bw: 0.2, rc: RULE_DIM }));
        yy += 8.2;
      }
      c.y = yy + 2;
      return it;
    }

    // style 0 — دو ستونه کادردار
    var half = w / 2 - 1.5;
    var p0 = [
      ["نام و نام خانوادگی:", "{full}", true], ["کد ملی:", "{nid}", false],
      ["تاریخ تولد:", "{birth}", false], ["سن:", "{age}", false],
      ["جنسیت:", "{gender}", false], ["نوع بیمار:", "{ptype}", false],
      ["تلفن همراه:", "{mobile}", false], ["تلفن ثابت:", "{landline}", false]
    ];
    var rowsN = Math.ceil(p0.length / 2), bh = rowsN * 6.6 + 2.4;
    it.push(R(x, y, w, bh, { bw: 0.35 }));
    it.push(VL(x + w / 2, y, bh, { bw: 0.25, rc: RULE_DIM }));
    for (var q = 0; q < p0.length; q++) {
      var col = q % 2, row = Math.floor(q / 2);
      var qx = x + (col === 0 ? w / 2 + 1.5 : 1.5);
      it.push(PV(qx, y + 1.4 + row * 6.6, half - 1.5, p0[q][0], p0[q][1], { pt: 9, bold: p0[q][2] }));
    }
    c.y = y + bh + 3;
    return it;
  }

  /* ---- بیمه (پایه + مکمل با درصد کنار آن) ------------------------------ */
  //  style: 0 = دو سلول کادردار، 1 = نوار خاکستری، 2 = سطری
  function T_INS(c, style) {
    var x = c.x, w = c.w, y = c.y, it = [];
    if (style === 1) {
      it.push(BANDBOX(x, y, w, 15, BAND));
      it.push(L(x + 2, y + 0.8, w - 4, 4.4, "پوشش بیمه‌ای", { pt: 8.6, bold: true, textColor: INK_SOFT }));
      it.push(PV(x + w / 2 + 1.5, y + 5.4, w / 2 - 3.5, "بیمهٔ پایه:", "{ins_full}", { pt: 9.4, bold: true }));
      it.push(PV(x + 2, y + 5.4, w / 2 - 3.5, "بیمهٔ مکمل:", "{supp_full}", { pt: 9.4, bold: true }));
      it.push(PV(x + w / 2 + 1.5, y + 10.4, w / 2 - 3.5, "شماره دفترچه:", "{insno}", { pt: 8.4 }));
      it.push(PV(x + 2, y + 10.4, w / 2 - 3.5, "اعتبار:", "{insexp}", { pt: 8.4 }));
      c.y = y + 19;
      return it;
    }
    if (style === 2) {
      it.push(PV(x + w * 0.5, y, w * 0.5 - 2, "بیمهٔ پایه:", "{ins}", { pt: 9.2, bold: true }));
      it.push(PV(x + w * 0.34, y, w * 0.14, "درصد:", "{ins_percent}", { pt: 9.2 }));
      it.push(PV(x, y, w * 0.32, "دفترچه:", "{insno}", { pt: 9.2 }));
      it.push(HL(x, y + 6.6, w, { bw: 0.2, rc: RULE_DIM }));
      it.push(PV(x + w * 0.5, y + 8, w * 0.5 - 2, "بیمهٔ مکمل:", "{supp}", { pt: 9.2, bold: true }));
      it.push(PV(x + w * 0.34, y + 8, w * 0.14, "درصد:", "{supp_percent}", { pt: 9.2 }));
      it.push(PV(x, y + 8, w * 0.32, "اعتبار:", "{insexp}", { pt: 9.2 }));
      it.push(HL(x, y + 14.6, w, { bw: 0.2, rc: RULE_DIM }));
      c.y = y + 18;
      return it;
    }
    // style 0
    var cw = w / 2 - 1.5;
    it.push(R(x + w - cw, y, cw, 20, { bw: 0.35 }));
    it.push(BANDBOX(x + w - cw, y, cw, 5.6, BAND));
    it.push(L(x + w - cw, y + 0.7, cw, 4.4, "بیمهٔ پایه", { pt: 8.4, bold: true, align: 1, dir: 2 }));
    it.push(PV(x + w - cw + 2, y + 6.4, cw - 4, "سازمان:", "{ins}", { pt: 9, bold: true }));
    it.push(PV(x + w - cw + 2, y + 11.4, cw - 4, "درصد پوشش:", "{ins_percent}", { pt: 9 }));
    it.push(PV(x + w - cw + 2, y + 15.6, cw - 4, "دفترچه:", "{insno}", { pt: 8.4 }));
    it.push(R(x, y, cw, 20, { bw: 0.35 }));
    it.push(BANDBOX(x, y, cw, 5.6, BAND));
    it.push(L(x, y + 0.7, cw, 4.4, "بیمهٔ مکمل", { pt: 8.4, bold: true, align: 1, dir: 2 }));
    it.push(PV(x + 2, y + 6.4, cw - 4, "سازمان:", "{supp}", { pt: 9, bold: true }));
    it.push(PV(x + 2, y + 11.4, cw - 4, "درصد پوشش:", "{supp_percent}", { pt: 9 }));
    it.push(PV(x + 2, y + 15.6, cw - 4, "اعتبار:", "{insexp}", { pt: 8.4 }));
    c.y = y + 23;
    return it;
  }

  /* ---- پزشک / تخصص / نوع خدمت ------------------------------------------ */
  //  style: 0 = چهار سلول، 1 = نوار خاکستری، 2 = سطری با خط‌چین
  function T_DOCTOR(c, style) {
    var x = c.x, w = c.w, y = c.y, it = [];
    if (style === 1) {
      it.push(BANDBOX(x, y, w, 15.5, BAND));
      it.push(PV(x + w / 2 + 1.5, y + 1.2, w / 2 - 3.5, "پزشک معالج:", "{doctor}", { pt: 9.6, bold: true }));
      it.push(PV(x + 2, y + 1.2, w / 2 - 3.5, "کد پزشک:", "{doctorcode}", { pt: 9.2 }));
      it.push(PV(x + w / 2 + 1.5, y + 6.4, w / 2 - 3.5, "شرح تخصص:", "{specialty}", { pt: 9.2 }));
      it.push(PV(x + 2, y + 6.4, w / 2 - 3.5, "نوع خدمت:", "{servicetype}", { pt: 9.2 }));
      it.push(PV(x + w / 2 + 1.5, y + 11, w / 2 - 3.5, "انجام‌دهنده:", "{performer}", { pt: 8.6 }));
      it.push(PV(x + 2, y + 11, w / 2 - 3.5, "بخش:", "{dept}", { pt: 8.6 }));
      c.y = y + 19;
      return it;
    }
    if (style === 2) {
      it.push(PV(x + w * 0.52, y, w * 0.48 - 2, "پزشک معالج:", "{doctor}", { pt: 9.6, bold: true }));
      it.push(PV(x + w * 0.26, y, w * 0.24, "کد پزشک:", "{doctorcode}", { pt: 9.2 }));
      it.push(PV(x, y, w * 0.24, "بخش:", "{dept}", { pt: 9.2 }));
      it.push(HL(x, y + 6.6, w, { bw: 0.2, rc: RULE_DIM }));
      it.push(PV(x + w * 0.52, y + 8, w * 0.48 - 2, "شرح تخصص:", "{specialty}", { pt: 9.2 }));
      it.push(PV(x + w * 0.26, y + 8, w * 0.24, "نوع خدمت:", "{servicetype}", { pt: 9.2 }));
      it.push(PV(x, y + 8, w * 0.24, "کد تخصص:", "{specialtycode}", { pt: 9.2 }));
      it.push(HL(x, y + 14.6, w, { bw: 0.2, rc: RULE_DIM }));
      c.y = y + 18;
      return it;
    }
    var n = 4, cw = w / n;
    var g = [["پزشک معالج", "{doctor}"], ["کد پزشک", "{doctorcode}"],
             ["شرح تخصص", "{specialty}"], ["نوع خدمت", "{servicetype}"]];
    for (var i = 0; i < n; i++) {
      var bx = x + (n - 1 - i) * cw;
      it.push(R(bx, y, cw, 12, { bw: 0.3, rc: RULE_DIM }));
      it.push(L(bx + 1.6, y + 1, cw - 3.2, 3.8, g[i][0], { pt: 6.8, textColor: INK_DIM }));
      it.push(F(bx + 1.6, y + 4.8, cw - 3.2, 6.4, g[i][1], { pt: 9.2, bold: i === 0 }));
    }
    it.push(PV(x + w * 0.52, y + 13.4, w * 0.48, "انجام‌دهنده:", "{performer}", { pt: 8.4, color: INK_DIM }));
    it.push(PV(x, y + 13.4, w * 0.48, "بخش / واحد:", "{dept}", { pt: 8.4, color: INK_DIM }));
    c.y = y + 22;
    return it;
  }

  /* ---- جدول خدمات ------------------------------------------------------- */
  //  همیشه «نام خدمت | شرح خدمت | تعداد». style فقط ظاهر خط‌کشی را عوض می‌کند:
  //    0 = سربرگ خاکستری + خط‌کشی کامل
  //    1 = تمام‌خط‌کشی بدون سایه (خالص تک‌رنگ)
  //    2 = سربرگ خاکستری تیره‌تر + کادر بیرونی ضخیم
  function T_SERVICES(c, style, o) {
    o = o || {};
    var x = c.x, w = c.w, y = c.y, it = [];
    var h = o.h || 62;
    it.push(L(x, y, w * 0.6, 5.6, o.title || "خدمات ارائه‌شده", { pt: 9.6, bold: true }));
    it.push(PV(x + w * 0.6, y, w * 0.4, "تعداد ردیف:", "{servicescount}", { pt: 8.4, color: INK_DIM }));
    y += 7;
    var so = { pt: o.pt || 9, rowH: o.rowH || 6.5, headerH: o.headerH || 7.5 };
    if (style === 1) { so.band = false; so.bw = 0.3; }
    else if (style === 2) { so.band = BAND_DK; so.bw = 0.6; }
    else { so.band = BAND; so.bw = 0.35; }
    it.push(SERVICES(x, y, w, h, so));
    c.y = y + h + 3.5;
    return it;
  }

  /* ---- مالی / صورتحساب -------------------------------------------------- */
  //  style: 0 = جدول دو ستونه راست‌چین، 1 = نوار خاکستری سه‌ستونه،
  //         2 = ستون باریک سمت چپ، 3 = فشرده تک‌سطری
  function T_FINANCE(c, style) {
    var x = c.x, w = c.w, y = c.y, it = [];
    var rows = [
      ["جمع کل خدمات", "{total}", false],
      ["سهم پایه (بیمهٔ اصلی)", "{basepay}", false],
      ["سهم مکمل", "{supppay}", false],
      ["تخفیف", "{discount_from}", false],
      ["نقدی (صندوق)", "{cash}", false],
      ["کارتخوان (POS)", "{pos}", false],
      ["سهم بیمار", "{patientshare}", false],
      ["مبلغ نهایی قابل پرداخت", "{finaltotal}", true]
    ];

    if (style === 3) {
      it.push(BANDBOX(x, y, w, 10, BAND_DK));
      it.push(PV(x + w * 0.66, y + 1.4, w * 0.32, "جمع کل:", "{total}", { pt: 9.4, bold: true }));
      it.push(PV(x + w * 0.34, y + 1.4, w * 0.3, "سهم بیمار:", "{patientshare}", { pt: 9.4 }));
      it.push(PV(x + 2, y + 1.4, w * 0.3, "پرداختی:", "{finaltotal}", { pt: 9.4, bold: true }));
      it.push(PV(x + w * 0.5, y + 11, w * 0.5 - 2, "نقدی:", "{cash}", { pt: 8.2, color: INK_DIM }));
      it.push(PV(x, y + 11, w * 0.48, "کارتخوان:", "{pos}", { pt: 8.2, color: INK_DIM }));
      c.y = y + 20;
      return it;
    }

    if (style === 2) {
      var cw = w * 0.44, bx = x;                     // ستون باریک، سمت چپ برگ
      it.push(R(bx, y, cw, rows.length * 6.4 + 2.4, { bw: 0.4 }));
      for (var i = 0; i < rows.length; i++) {
        var yy = y + 1.4 + i * 6.4;
        it.push(L(bx + cw * 0.4, yy, cw * 0.58, 5.6, rows[i][0], { pt: 8.2, bold: rows[i][2], textColor: rows[i][2] ? INK : INK_SOFT }));
        it.push(F(bx + 1.6, yy, cw * 0.4 - 2, 5.6, rows[i][1], { pt: 8.6, bold: rows[i][2], align: 2, dir: 0 }));
        if (i < rows.length - 1) it.push(HL(bx, yy + 6.1, cw, { bw: 0.15, rc: RULE_DIM }));
      }
      c.y = y + rows.length * 6.4 + 6;
      return it;
    }

    if (style === 1) {
      var n = 4, cwx = w / n, r2 = Math.ceil(rows.length / n);
      it.push(BANDBOX(x, y, w, r2 * 12.2, BAND));
      for (var k = 0; k < rows.length; k++) {
        var rr = Math.floor(k / n), cc = k % n;
        var px = x + (n - 1 - cc) * cwx;
        it.push(L(px + 1.6, y + rr * 12.2 + 1, cwx - 3.2, 4, rows[k][0], { pt: 6.8, textColor: INK_DIM }));
        it.push(F(px + 1.6, y + rr * 12.2 + 5, cwx - 3.2, 6.4, rows[k][1], { pt: 9, bold: rows[k][2] }));
        if (cc > 0) it.push(VL(px + cwx, y + rr * 12.2 + 1.5, 9.5, { bw: 0.2, rc: RULE_DIM }));
      }
      c.y = y + r2 * 12.2 + 4;
      return it;
    }

    // style 0 — جدول دو ستونه (برچسب راست، مبلغ چپ)
    var bh = rows.length * 6.4 + 2.4;
    it.push(R(x, y, w, bh, { bw: 0.4 }));
    it.push(VL(x + w * 0.42, y, bh, { bw: 0.25, rc: RULE_DIM }));
    for (var m = 0; m < rows.length; m++) {
      var my = y + 1.4 + m * 6.4;
      var isLast = (m === rows.length - 1);
      if (isLast) it.push(BANDBOX(x + 0.3, my - 0.8, w - 0.6, 6.8, BAND));
      it.push(L(x + w * 0.44, my, w * 0.54, 5.6, rows[m][0], { pt: 8.6, bold: rows[m][2] }));
      it.push(F(x + 1.6, my, w * 0.4, 5.6, rows[m][1], { pt: 9, bold: rows[m][2], align: 2, dir: 0 }));
      if (!isLast) it.push(HL(x, my + 6.1, w, { bw: 0.15, rc: RULE_DIM }));
    }
    c.y = y + bh + 4;
    return it;
  }

  /* ---- پانویس: پذیرش‌گر، رهگیری، معرفی‌نامه، بارکد ---------------------- */
  //  style: 0 = بارکد سمت چپ + اطلاعات راست، 1 = بارکد وسط، 2 = QR + بارکد،
  //         3 = بدون بارکد (فقط امضا)، 4 = نوار خاکستری + بارکد
  function T_FOOTER(c, style, W, H) {
    var x = c.x, w = c.w, y = c.y, it = [];
    var bcW = 56, bcH = 15;

    if (style === 4) it.push(BANDBOX(x, y, w, 22, BAND));
    else it.push(HL(x, y, w, { bw: 0.5 }));
    var iy = y + (style === 4 ? 1.6 : 2.2);

    var infoW = (style === 3) ? w : w - bcW - 4;
    it.push(PV(x + w - infoW, iy, infoW * 0.55, "پذیرشگر:", "{receptionist}", { pt: 8.4 }));
    it.push(PV(x + w - infoW * 0.44, iy, infoW * 0.42, "صندوق‌دار:", "{cashier_name}", { pt: 8.4 }));
    it.push(PV(x + w - infoW, iy + 5, infoW * 0.55, "کد رهگیری نسخه:", "{eprescription}", { pt: 8.2, visibility: 1 }));
    it.push(PV(x + w - infoW * 0.44, iy + 5, infoW * 0.42, "ش. معرفی‌نامه:", "{referralno}", { pt: 8.2, visibility: 1 }));
    it.push(PV(x + w - infoW, iy + 10, infoW * 0.55, "ش.ص:", "{scnum}", { pt: 8.2 }));
    it.push(PV(x + w - infoW * 0.44, iy + 10, infoW * 0.42, "کد رسید:", "{receiptcode}", { pt: 8.2 }));

    // EVERY footer carries exactly one real, scannable barcode — it is the
    // machine-readable key to the reception record, so no style may omit it.
    if (style === 0 || style === 4) {
      it.push(BARCODE(x, iy + 0.5, bcW, bcH));
      it.push(L(x, iy + bcH + 1, bcW, 3.6, "بارکد رسید", { pt: 6.4, align: 1, dir: 2, textColor: INK_DIM }));
    } else if (style === 1) {
      it.push(BARCODE(x + (w - bcW) / 2, iy + 16, bcW, bcH));
    } else if (style === 2) {
      it.push(QR(x, iy + 0.5, 17));
      it.push(BARCODE(x + 19, iy + 1.5, bcW - 12, 13));
    } else if (style === 3) {
      // style 3 spreads the clerk info full-width, so the barcode sits centred
      // on its own line beneath it (and the signature band moves down to suit).
      it.push(BARCODE(x + (w - bcW) / 2, iy + 16, bcW, bcH));
      it.push(T(x, iy + 16 + bcH + 0.8, w, 3.6, "بارکد رسید", { pt: 6.4, textColor: INK_DIM }));
    }

    var fy = y + ((style === 1 || style === 3) ? 34 : 24);
    it.push(L(x, fy, w * 0.42, 4.4, "امضا / مهر پذیرش", { pt: 7.6, textColor: INK_DIM }));
    it.push(HL(x, fy + 5.4, w * 0.4, { bw: 0.25, rc: RULE_DIM }));
    it.push(L(x + w * 0.58, fy, w * 0.42, 4.4, "امضای بیمار", { pt: 7.6, align: 2, dir: 0, textColor: INK_DIM }));
    it.push(HL(x + w * 0.6, fy + 5.4, w * 0.4, { bw: 0.25, rc: RULE_DIM }));
    it.push(T(x, fy + 7.4, w, 4, "این رسید سند پذیرش شماست؛ لطفاً تا پایان درمان نزد خود نگه دارید.",
      { pt: 6.8, bold: false, textColor: INK_DIM }));
    c.y = fy + 13;
    return it;
  }

  /* ---- شرایط / یادداشت -------------------------------------------------- */
  function T_NOTES(c, style) {
    var x = c.x, w = c.w, y = c.y, it = [];
    if (style === 1) it.push(BANDBOX(x, y, w, 17, BAND));
    else it.push(R(x, y, w, 17, { bw: 0.3, rc: RULE_DIM }));
    it.push(L(x + 2, y + 1, w - 4, 4.4, "توضیحات / تشخیص", { pt: 8.2, bold: true, textColor: INK_SOFT }));
    it.push(PV(x + 2, y + 5.8, w - 4, "تشخیص:", "{diagnosis}", { pt: 8.4, visibility: 1 }));
    it.push(PV(x + 2, y + 11, (w - 4) * 0.5, "حساسیت دارویی:", "{allergy}", { pt: 8.4, visibility: 1 }));
    it.push(PV(x + 2 + (w - 4) * 0.52, y + 11, (w - 4) * 0.46, "مراجعهٔ بعدی:", "{nextvisit}", { pt: 8.4, visibility: 1 }));
    c.y = y + 20;
    return it;
  }

  /* ---- علائم حیاتی ------------------------------------------------------ */
  function T_VITALS(c, style) {
    var x = c.x, w = c.w, y = c.y, it = [];
    var g = [["قد", "{height}"], ["وزن", "{weight}"], ["فشار خون", "{bp}"], ["دما", "{temp}"], ["نبض", "{pulse}"]];
    var n = g.length, cw = w / n;
    if (style === 1) it.push(BANDBOX(x, y, w, 11.5, BAND));
    for (var i = 0; i < n; i++) {
      var bx = x + (n - 1 - i) * cw;
      if (style !== 1) it.push(R(bx, y, cw, 11.5, { bw: 0.3, rc: RULE_DIM }));
      it.push(L(bx + 1.4, y + 0.9, cw - 2.8, 3.6, g[i][0], { pt: 6.6, align: 1, dir: 2, textColor: INK_DIM }));
      it.push(F(bx + 1.4, y + 4.4, cw - 2.8, 6.2, g[i][1], { pt: 9, bold: true, align: 1, dir: 2 }));
    }
    c.y = y + 15;
    return it;
  }

  /* ---- برگهٔ بیمه (نسخهٔ دوم رسید، با بارکد قابل اسکن) ------------------ */
  function T_INSPAGE(c, style) {
    var x = c.x, w = c.w, y = c.y, it = [];
    it.push(HL(x, y, w, { bw: 0.8 }));
    y += 2.5;
    it.push(T(x, y, w, 7, "نسخهٔ سازمان بیمه‌گر", { pt: 11 }));
    y += 9;
    it.push(R(x, y, w, 30, { bw: 0.4 }));
    it.push(PV(x + w * 0.52, y + 1.8, w * 0.46, "بیمار:", "{full}", { pt: 9.2, bold: true }));
    it.push(PV(x + 2, y + 1.8, w * 0.46, "کد ملی:", "{nid}", { pt: 9.2 }));
    it.push(PV(x + w * 0.52, y + 7.4, w * 0.46, "بیمهٔ پایه:", "{ins_full}", { pt: 8.8 }));
    it.push(PV(x + 2, y + 7.4, w * 0.46, "بیمهٔ مکمل:", "{supp_full}", { pt: 8.8 }));
    it.push(PV(x + w * 0.52, y + 12.8, w * 0.46, "سهم پایه:", "{basepay}", { pt: 8.8 }));
    it.push(PV(x + 2, y + 12.8, w * 0.46, "سهم مکمل:", "{supppay}", { pt: 8.8 }));
    it.push(PV(x + w * 0.52, y + 18.2, w * 0.46, "ش. معرفی‌نامه:", "{referralno}", { pt: 8.8, visibility: 1 }));
    it.push(PV(x + 2, y + 18.2, w * 0.46, "کد رهگیری:", "{eprescription}", { pt: 8.8, visibility: 1 }));
    it.push(PV(x + w * 0.52, y + 23.6, w * 0.46, "پزشک:", "{doctor}", { pt: 8.8 }));
    it.push(PV(x + 2, y + 23.6, w * 0.46, "نوع خدمت:", "{servicetype}", { pt: 8.8 }));
    // بارکد قابل اسکن + نمایش عدد آن (الزام برگهٔ بیمه)
    var bw2 = style === 1 ? 70 : 58;
    it.push(BARCODE(x + (w - bw2) / 2, y + 32, bw2, 17, { sym: "code128" }));
    c.y = y + 53;
    return it;
  }

  /* ======================================================================== *
   *  THE 30 READY-MADE DESIGNS
   *  همه در گروه reception، همه A4 عمودی، همه تک‌رنگ، همه با جدول خدمات
   *  «نام خدمت | شرح خدمت | تعداد»، هیچ‌کدام «نام پدر» یا «آدرس» ندارند.
   *  هر ردیف جدول زیر یک ترکیب یکتا از استایل بلوک‌ها است، بنابراین ۳۰ طرح
   *  ظاهر کاملاً متفاوتی دارند و هیچ دو تایی شبیه هم نیستند.
   * ======================================================================== */
  var W = 210, H = 297, M = 10;              // A4 + حاشیه

  //  فیلدهای هر ردیف:
  //  name, header, title, appt, patient, ins, doctor, svc, fin, foot,
  //  extras: {frame, notes, vitals, inspage, svcH}
  var SPEC = [
    ["۰۱) رسید پذیرش ثامن‌الائمه — استاندارد",      0, 0, 0, 0, 0, 0, 0, 0, 0, { frame: 1, svcH: 62 }],
    ["۰۲) رسید پذیرش — سربرگ نواری",                1, 1, 1, 1, 1, 1, 0, 1, 4, { svcH: 58 }],
    ["۰۳) رسید پذیرش — خط‌کشی خالص",                2, 2, 0, 1, 2, 2, 1, 0, 0, { frame: 1, svcH: 66 }],
    ["۰۴) رسید پذیرش — شبکه‌ای فشرده",              0, 1, 1, 2, 1, 0, 0, 1, 1, { svcH: 55 }],
    ["۰۵) رسید پذیرش — با عکس بیمار",               3, 0, 0, 3, 0, 0, 0, 0, 0, { frame: 1, svcH: 54 }],
    ["۰۶) رسید پذیرش — جعبهٔ نوبت بزرگ",            1, 2, 2, 0, 1, 1, 2, 0, 2, { svcH: 56 }],
    ["۰۷) رسید پذیرش + برگهٔ بیمه",                 0, 0, 1, 1, 2, 2, 0, 3, 3, { inspage: 1, svcH: 40 }],
    ["۰۸) رسید پذیرش — کامل با علائم حیاتی",        2, 0, 0, 0, 0, 0, 0, 0, 0, { vitals: 1, svcH: 46 }],
    ["۰۹) رسید پذیرش — با توضیحات و تشخیص",         1, 1, 1, 1, 1, 1, 0, 0, 1, { notes: 1, svcH: 44 }],
    ["۱۰) رسید پذیرش — دو ستونهٔ مالی",             0, 2, 0, 0, 0, 2, 1, 2, 0, { frame: 1, svcH: 60 }],
    ["۱۱) رسید پذیرش — مینیمال بی‌لوگو",            4, 0, 1, 1, 2, 2, 1, 3, 1, { svcH: 70 }],
    ["۱۲) رسید پذیرش — کادر کامل رسمی",             3, 1, 0, 0, 0, 0, 2, 0, 0, { frame: 1, svcH: 58 }],
    ["۱۳) رسید پذیرش — نواری تیره",                 1, 1, 1, 2, 1, 1, 2, 1, 4, { svcH: 52 }],
    ["۱۴) رسید پذیرش — سطری کلاسیک",                2, 0, 0, 1, 2, 2, 0, 0, 0, { svcH: 64 }],
    ["۱۵) رسید پذیرش — QR و بارکد",                 0, 2, 1, 0, 0, 0, 1, 0, 2, { frame: 1, svcH: 56 }],
    ["۱۶) رسید پذیرش — جدول خدمات بلند",            1, 0, 1, 1, 1, 2, 0, 3, 1, { svcH: 92 }],
    ["۱۷) رسید پذیرش — تخصصی (تخصص برجسته)",        0, 1, 0, 0, 2, 1, 0, 0, 0, { svcH: 58 }],
    ["۱۸) رسید پذیرش — بیمه‌محور",                  2, 1, 1, 1, 0, 2, 2, 1, 0, { inspage: 1, svcH: 38 }],
    ["۱۹) رسید پذیرش — فشردهٔ یک‌صفحه‌ای",          4, 2, 1, 2, 2, 2, 1, 3, 1, { svcH: 76 }],
    ["۲۰) رسید پذیرش — رسمی با امضا",               3, 0, 0, 0, 0, 0, 0, 0, 3, { frame: 1, svcH: 62 }],
    ["۲۱) رسید پذیرش — علائم حیاتی + توضیحات",      1, 1, 1, 1, 1, 1, 0, 3, 1, { vitals: 1, notes: 1, svcH: 40 }],
    ["۲۲) رسید پذیرش — شبکهٔ سه‌ستونه",             0, 0, 0, 2, 0, 0, 2, 0, 0, { frame: 1, svcH: 50 }],
    ["۲۳) رسید پذیرش — با عکس و بارکد",             1, 2, 1, 3, 1, 1, 0, 1, 0, { svcH: 48 }],
    ["۲۴) رسید پذیرش — خط‌چین سبک",                 2, 2, 2, 1, 2, 2, 1, 2, 1, { svcH: 58 }],
    ["۲۵) رسید پذیرش — مالی برجسته",                0, 1, 0, 0, 1, 0, 0, 0, 4, { svcH: 54 }],
    ["۲۶) رسید پذیرش — دوبرگی (بیمار/بیمه)",        1, 0, 0, 1, 0, 1, 0, 3, 3, { inspage: 1, svcH: 42 }],
    ["۲۷) رسید پذیرش — جدول تیرهٔ خدمات",           3, 1, 1, 0, 1, 2, 2, 1, 0, { svcH: 60 }],
    ["۲۸) رسید پذیرش — بی‌قاب و سبک",               4, 0, 1, 1, 2, 0, 1, 0, 1, { svcH: 68 }],
    ["۲۹) رسید پذیرش — کامل‌ترین (همه بخش‌ها)",     0, 1, 1, 2, 1, 1, 0, 1, 0, { vitals: 1, notes: 1, svcH: 34 }],
    ["۳۰) رسید پذیرش — نسخهٔ شبانه‌روزی",           2, 2, 2, 0, 0, 1, 0, 0, 2, { frame: 1, svcH: 52 }]
  ];

  // ⚠️ طبق خواستهٔ کارفرما: تمام طرح‌های آمادهٔ قبلی حذف می‌شوند و فقط ۳۰
  // طرح جدید زیر باقی می‌ماند.
  ALL.length = 0;

  // یک بار طرح را با ارتفاع دلخواهِ جدول خدمات می‌سازد و آخرین y را برمی‌گرداند.
  function buildOne(s, svcH) {
    var ex = s[10] || {};
    var it = [];
    var c = { x: M, y: M, w: W - 2 * M };

    if (ex.frame) it.push(FRAME(W, H, 5));

    it.push(T_HEADER(c, s[1]));
    it.push(T_TITLE(c, "رسید پذیرش و صورتحساب", s[2]));
    it.push(T_APPT(c, s[3]));
    it.push(T_PATIENT(c, s[4], { headBand: s[2] === 1 }));
    it.push(T_INS(c, s[5]));
    it.push(T_DOCTOR(c, s[6]));
    if (ex.vitals) it.push(T_VITALS(c, s[5] === 1 ? 1 : 0));
    it.push(T_SERVICES(c, s[7], { h: svcH }));
    if (ex.notes) it.push(T_NOTES(c, s[5] === 1 ? 1 : 0));
    it.push(T_FINANCE(c, s[8]));
    it.push(T_FOOTER(c, s[9], W, H));
    if (ex.inspage) it.push(T_INSPAGE(c, s[9] === 3 ? 1 : 0));

    return { items: it, bottom: c.y };
  }

  // ---- AUTO-FIT به یک برگ A4 --------------------------------------------
  // جدول خدمات تنها بلوک «کشسان» طرح است، پس ارتفاع آن را طوری می‌گیریم که
  // مجموع بلوک‌ها دقیقاً داخل کاغذ جا شود. حداقل ارتفاع ۲۲ میلی‌متر است
  // (سرستون + دو سطر خدمت) و در صورت لزوم ابتدا از بلوک‌های اختیاری
  // (علائم حیاتی / توضیحات / برگهٔ بیمه) عبور نمی‌کنیم — آن‌ها بخشی از
  // هویت طرح‌اند — بلکه فقط جدول خدمات فشرده می‌شود.
  var LIMIT = H - M;                 // پایین‌ترین مرز مجاز (۲۸۷ میلی‌متر)
  var SVC_MIN = 22;

  SPEC.forEach(function (s) {
    var name = s[0], ex = s[10] || {};
    var want = ex.svcH || 58;

    // سرباری که مستقل از جدول خدمات است را با یک ساخت آزمایشی می‌سنجیم.
    var probe = buildOne(s, SVC_MIN);
    var overhead = probe.bottom - SVC_MIN;
    var room = LIMIT - overhead;
    var svcH = Math.min(want, room);
    if (svcH < SVC_MIN) svcH = SVC_MIN;
    // به نیم‌میلی‌متر گرد می‌کنیم تا هندسه تمیز بماند.
    svcH = Math.round(svcH * 2) / 2;

    var built = (svcH === SVC_MIN) ? probe : buildOne(s, svcH);

    // اگر طرح بلوک‌های اختیاری زیادی دارد و با کوچک‌ترین جدول خدمات هم از برگ
    // بیرون می‌زند، یک فشردگی عمودی یکنواخت (حداکثر ~۱۵٪) اعمال می‌شود تا کل
    // سند در یک برگ A4 جا شود. اندازهٔ قلم دست‌نخورده می‌ماند، فقط فاصله‌ها و
    // ارتفاع کادرها به یک نسبت کوچک می‌شوند، بنابراین چیدمان و هم‌ترازی حفظ
    // می‌شود. این کار قطعی است و در هر بار بارگذاری عیناً یکسان تکرار می‌شود.
    if (built.bottom > LIMIT) {
      var k = (LIMIT - M) / (built.bottom - M);
      if (k < 0.85) k = 0.85;
      // آرایه‌ها تودرتو هستند؛ فشردگی را با پیمایش بازگشتی اعمال می‌کنیم.
      (function walk(arr) {
        for (var i = 0; i < arr.length; i++) {
          if (Array.isArray(arr[i])) { walk(arr[i]); continue; }
          var o = arr[i]; if (!o || o.isFrame) continue;
          o.y = Math.round((M + (o.y - M) * k) * 10) / 10;
          o.h = Math.round(Math.max(o.h * k, o.type === "hline" ? o.h : 3.2) * 10) / 10;
          if (o.type === "services") {
            o.rowH = Math.round(Math.max(o.rowH * k, 4.5) * 10) / 10;
            o.headerH = Math.round(Math.max(o.headerH * k, 5.5) * 10) / 10;
          }
        }
      })(built.items);
    }

    push("reception", name, "A4", 0, built.items);
  });

  window.AZ_TEMPLATES = ALL;
})();
