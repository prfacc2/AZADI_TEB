/* ============================================================================
   admission.js — Patient Admission UI logic (AZADI_TEB).

   ES5-only syntax (no arrow functions / const / let / template literals / Map /
   spread / destructuring / fetch) so it runs identically on the WebView2
   (Chromium) engine AND the universal MSHTML / Trident (IE11) fallback.

   Talks to C++ ONLY through Bridge (structured JSON messages). C++ is the
   single source of truth — the UI never fabricates prices or identities.

   Behaviours implemented per the Management spec:
     · National-ID + Enter  → C++ lookup, auto-fills every patient field and
       auto-selects the verified insurance, WITHOUT resetting other fields.
     · Enter moves focus to the NEXT logical field (data-nav order). Tab too.
     · Birth-date field auto-inserts "/", Ctrl+A selects the whole value and
       typing replaces cleanly from the start.
     · Invoice is ZERO on open; numbers appear only after a real service (with
       a Management-defined name + price) is added.
     · Billing: آزاد / no insurance → full net base price (no reduction);
       insured → reduced per the base + supplementary insurance percentages.
     · Services searched by code/name; price ALWAYS from the catalog; quantity
       editable (pencil / double-click). List updates live, no refresh.
     · Queue is «صندوق نرفته‌ها»; receptionist can add the current admission to
       it; search + recent-minutes filter + collapse/expand.
     · ثبت پذیرش و صدور قبض → C++ saves + prints per the Management print design.
   ============================================================================ */
(function () {
  'use strict';

  function $(id) { return document.getElementById(id); }

  var state = {
    services: [],          /* current admission service rows */
    insurances: [],        /* {name,pct} base list */
    supp: [],              /* supplementary list */
    patient: null,         /* current loaded patient (or null) */
    catalog: [],           /* last service search results */
    queue: [],             /* صندوق نرفته‌ها rows */
    doctors: [],
    ps: { P: 0, S: 0 },
    ready: false
  };

  /* ---- Persian digit helpers ---- */
  var FA = '۰۱۲۳۴۵۶۷۸۹';
  function toFa(n) {
    return String(n == null ? '' : n).replace(/[0-9]/g, function (d) { return FA.charAt(+d); });
  }
  function toEn(s) {
    return String(s == null ? '' : s)
      .replace(/[۰-۹]/g, function (d) { return String(FA.indexOf(d)); })
      .replace(/[٠-٩]/g, function (d) { return String('٠١٢٣٤٥٦٧٨٩'.indexOf(d)); });
  }
  function money(n) {
    var v = Number(n || 0);
    var str = String(Math.round(Math.abs(v)));
    var out = '', c = 0, i;
    for (i = str.length - 1; i >= 0; i--) {
      out = str.charAt(i) + out;
      if (++c % 3 === 0 && i > 0) out = ',' + out;
    }
    return toFa((v < 0 ? '-' : '') + out);
  }
  function trimStr(s) { return String(s == null ? '' : s).replace(/^\s+|\s+$/g, ''); }

  function setText(el, text) {
    if (!el) return;
    el.innerHTML = '';
    el.appendChild(document.createTextNode(text == null ? '' : String(text)));
  }
  function esc(s) {
    s = (s == null ? '' : String(s));
    return s.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g, '&quot;');
  }
  function on(el, ev, fn) {
    if (!el) return;
    if (el.addEventListener) el.addEventListener(ev, fn, false);
    else el.attachEvent('on' + ev, fn);
  }

  function toast(msg, kind) {
    var t = $('toast');
    if (!t) return;
    setText(t, msg);
    t.className = 'toast show ' + (kind || '');
    if (toast._t) clearTimeout(toast._t);
    toast._t = setTimeout(function () { t.className = 'toast'; }, 2600);
  }

  function setSync(kind, text) {
    var b = $('syncBadge');
    if (b) b.className = 'sync-badge ' + (kind || '');
    setText($('syncText'), text);
  }

  /* ==========================================================================
     THEME (v1.43.0) — keep the embedded page in perfect sync with the native
     shell. C++ sends the current theme in init and pushes a "theme" event on
     every toggle. We set data-theme on <html> + a .theme-dark class on <body>
     so CSS can restyle everything (text stays light on dark, no clash), and
     switching back and forth never leaves stale colors behind.
     ========================================================================== */
  var _curTheme = 'light';
  function applyTheme(name) {
    name = (name === 'dark') ? 'dark' : 'light';
    _curTheme = name;
    var html = document.documentElement;
    var body = document.body;
    if (html) {
      html.setAttribute('data-theme', name);
      html.className = (html.className || '').replace(/\btheme-(dark|light)\b/g, '');
      html.className = trimStr(html.className + ' theme-' + name);
    }
    if (body) {
      body.className = (body.className || '').replace(/\btheme-(dark|light)\b/g, '');
      body.className = trimStr(body.className + ' theme-' + name);
    }
  }

  /* ==========================================================================
     BILLING — computed entirely here from Management-defined prices.
     Base insurance pct is the organisation share of the covered amount;
     supplementary insurance pct then covers a share of the patient remainder.
     آزاد (pct 0) → patient pays the full net base price (no reduction).
     ========================================================================== */
  function hasIns() { return $('hasIns') && $('hasIns').checked; }

  function baseInsPct() {
    if (!hasIns()) return 0;
    var sel = $('insMain');
    var idx = sel ? sel.selectedIndex : -1;
    return (state.insurances[idx] && state.insurances[idx].pct) || 0;
  }
  function suppInsPct() {
    /* explicit percentage box wins; else the selected supplementary plan pct */
    var box = $('insSuppPct');
    if (box) {
      var v = parseInt(toEn(box.value), 10);
      if (!isNaN(v) && v > 0) return v;
    }
    var sel = $('insSupp');
    var idx = sel ? sel.selectedIndex : -1;
    return (state.supp[idx] && state.supp[idx].pct) || 0;
  }

  /* per-row computation using current insurance selection */
  function computeRow(s) {
    var qty = s.qty || 1;
    var gross = (s.price || 0) * qty;
    var disc = s.discount || 0;
    var net = gross - disc; if (net < 0) net = 0;
    var bPct = baseInsPct();
    var orgShare = Math.round(net * bPct / 100);      /* base insurer share */
    var afterBase = net - orgShare;                    /* patient remainder  */
    var sPct = suppInsPct();
    var suppShare = Math.round(afterBase * sPct / 100);/* supplementary share */
    var patShare = afterBase - suppShare;
    s._gross = gross; s._disc = disc; s._net = net;
    s._org = orgShare; s._supp = suppShare; s._pat = patShare;
    return s;
  }

  function recompute() {
    var i, s;
    var sumGross = 0, sumDisc = 0, sumOrg = 0, sumSupp = 0, sumPat = 0;
    for (i = 0; i < state.services.length; i++) {
      s = computeRow(state.services[i]);
      sumGross += s._gross; sumDisc += s._disc;
      sumOrg += s._org; sumSupp += s._supp; sumPat += s._pat;
    }
    var netTotal = sumGross - sumDisc; if (netTotal < 0) netTotal = 0;
    var afterBase = netTotal - sumOrg;

    /* service footer strip */
    setText($('sfTotal'), money(sumGross));
    setText($('sfDisc'), money(sumDisc));
    setText($('sfIns'), money(sumOrg + sumSupp));
    setText($('sfPat'), money(sumPat));

    /* invoice — بیمه اصلی */
    setText($('invMainTotal'), money(sumGross));
    setText($('invMainPat'), money(afterBase));
    setText($('invMainOrg'), money(sumOrg));
    /* invoice — بیمه مکمل */
    setText($('invSuppTotal'), money(afterBase));
    setText($('invSuppShare'), money(sumSupp));
    setText($('invSuppPat'), money(sumPat));
    /* invoice — مبلغ نهایی */
    setText($('invFinTotal'), money(sumGross));
    setText($('invFinDisc'), money(sumDisc));
    var paid = $('noPay') && $('noPay').checked ? 0 : sumPat;
    setText($('invFinPaid'), money(paid));
    setText($('invRemain'), money(sumPat - paid));

    /* total card */
    setText($('tcVal'), money(sumPat));

    /* v1.43.0: C++ is the authoritative calculator. We show the instant local
       result above (so the UI is snappy on weak machines), then ask C++ to
       recompute and reconcile the display if it differs. This is debounced and
       fully async — it NEVER blocks the UI (fixes both the "wrong price" and any
       risk of a compute-related freeze). */
    scheduleServerBill(paid);

    return { gross: sumGross, disc: sumDisc, org: sumOrg, supp: sumSupp, pat: sumPat, paid: paid };
  }

  /* Debounced authoritative recompute on the C++ side. */
  var _billTimer = null, _billBusy = false;
  function scheduleServerBill(localPaid) {
    if (!state.ready) return;                 /* skip during boot */
    if (!state.services.length) return;       /* nothing to reconcile */
    if (_billTimer) clearTimeout(_billTimer);
    _billTimer = setTimeout(function () {
      if (_billBusy) return;
      _billBusy = true;
      var payload = {
        hasIns: hasIns(),
        insMain: $('insMain') ? $('insMain').selectedIndex : -1,
        insSupp: $('insSupp') ? $('insSupp').selectedIndex : -1,
        insSuppPct: parseInt(toEn($('insSuppPct') ? $('insSuppPct').value : '0'), 10) || 0,
        noPay: $('noPay') ? $('noPay').checked : false,
        services: state.services
      };
      Bridge.call('bill.compute', payload).then(function (b) {
        _billBusy = false;
        if (!b) return;
        /* reconcile only the money read-outs with the C++ authoritative values */
        setText($('sfTotal'), money(b.gross));
        setText($('sfDisc'), money(b.disc));
        setText($('sfIns'), money((b.org || 0) + (b.supp || 0)));
        setText($('sfPat'), money(b.pat));
        setText($('invMainTotal'), money(b.gross));
        setText($('invMainOrg'), money(b.org));
        setText($('invMainPat'), money((b.gross || 0) - (b.disc || 0) - (b.org || 0)));
        setText($('invSuppTotal'), money((b.gross || 0) - (b.disc || 0) - (b.org || 0)));
        setText($('invSuppShare'), money(b.supp));
        setText($('invSuppPat'), money(b.pat));
        setText($('invFinTotal'), money(b.gross));
        setText($('invFinDisc'), money(b.disc));
        var paid = b.paid != null ? b.paid : b.pat;
        setText($('invFinPaid'), money(paid));
        setText($('invRemain'), money((b.pat || 0) - paid));
        setText($('tcVal'), money(b.pat));
      })['catch'](function () { _billBusy = false; });
    }, 120);
  }

  /* ==========================================================================
     SERVICE ROWS
     ========================================================================== */
  function addServiceRow(svc) {
    /* price ALWAYS from the catalog — never typed by the operator */
    state.services.push({
      code: svc.code || '', name: svc.name || '',
      qty: 1, price: Number(svc.price) || 0, discount: 0
    });
    renderServices();
    recompute();
  }

  function renderServices() {
    var body = $('svcBody');
    if (!body) return;
    if (!state.services.length) {
      body.innerHTML = '<tr><td colspan="9" class="empty">خدمتی افزوده نشده است</td></tr>';
      return;
    }
    var html = '', i, s;
    for (i = 0; i < state.services.length; i++) {
      s = computeRow(state.services[i]);
      html +=
        '<tr>' +
        '<td>' + toFa(i + 1) + '</td>' +
        '<td class="td-name">' + esc(s.name || '') + '</td>' +
        '<td>' + toFa(s.code || '—') + '</td>' +
        '<td><input class="inp qty-inp" type="text" value="' + toFa(s.qty) + '" data-i="' + i + '"/></td>' +
        '<td>' + money(s.price) + '</td>' +
        '<td>' + money(s._disc) + '</td>' +
        '<td>' + money(s._org + s._supp) + '</td>' +
        '<td>' + money(s._pat) + '</td>' +
        '<td>' +
          '<button class="act-btn act-edit" data-edit="' + i + '" title="ویرایش تعداد">✎</button>' +
          '<button class="act-btn act-del" data-del="' + i + '" title="حذف">✕</button>' +
        '</td>' +
        '</tr>';
    }
    body.innerHTML = html;

    var qtys = body.getElementsByClassName('qty-inp');
    for (i = 0; i < qtys.length; i++) {
      qtys[i].onchange = onQtyChange;
      /* B4: reflect qty edits instantly (not only on blur/change) */
      qtys[i].oninput = onQtyChange;
      qtys[i].onkeyup = onQtyChange;
      qtys[i].onkeydown = function (e) {
        e = e || window.event;
        var k = e.keyCode || e.which;
        if (k === 13) {                       /* Enter in qty → back to search */
          if (e.preventDefault) e.preventDefault(); else e.returnValue = false;
          var s = $('svcSearch');
          if (s) { s.focus(); if (s.select) try { s.select(); } catch (er) {} }
          return false;
        }
      };
      qtys[i].ondblclick = function () { this.focus(); this.select(); };
    }
    var dels = body.getElementsByClassName('act-del');
    for (i = 0; i < dels.length; i++) {
      dels[i].onclick = function (e) {
        e = e || window.event; var tgt = e.target || e.srcElement;
        state.services.splice(+tgt.getAttribute('data-del'), 1);
        renderServices(); recompute();
      };
    }
    var eds = body.getElementsByClassName('act-edit');
    for (i = 0; i < eds.length; i++) {
      eds[i].onclick = function (e) {
        e = e || window.event; var tgt = e.target || e.srcElement;
        var idx = +tgt.getAttribute('data-edit');
        var inp = body.getElementsByClassName('qty-inp')[idx];
        if (inp) { inp.focus(); inp.select(); }
      };
    }
  }
  function onQtyChange(e) {
    e = e || window.event; var tgt = e.target || e.srcElement;
    var idx = +tgt.getAttribute('data-i');
    var q = Math.max(1, parseInt(toEn(tgt.value), 10) || 1);
    state.services[idx].qty = q;
    renderServices(); recompute();
  }

  /* ==========================================================================
     PATIENT — fill without disturbing anything the operator is typing.
     ========================================================================== */
  /* B1 FIX: normalise any gender representation → the exact <select> option
     TEXT ("مرد" / "زن"), then select that option by index. Returns true when a
     match was applied. */
  function setGender(raw) {
    var sel = $('gender');
    if (!sel || raw == null) return false;
    var g = trimStr(String(raw))
      .replace(/[\u200c\u200e\u200f\u202a-\u202e]/g, '') /* strip ZWNJ/LRM/RLM */
      .toLowerCase();
    if (!g) return false;
    var female = (g === 'زن' || g === 'مونث' || g === 'مؤنث' || g === 'خانم' ||
      g === 'female' || g === 'f' || g === 'w' || g === '2' || g === '۲');
    var male = (g === 'مرد' || g === 'مذکر' || g === 'آقا' || g === 'اقا' ||
      g === 'male' || g === 'm' || g === '1' || g === '۱');
    var target = female ? 'زن' : (male ? 'مرد' : null);
    if (!target) return false;
    var i;
    for (i = 0; i < sel.options.length; i++) {
      var t = trimStr(sel.options[i].text || sel.options[i].value || '');
      if (t === target) { sel.selectedIndex = i; return true; }
    }
    /* fallback: index 0 = مرد, 1 = زن (matches the static HTML order) */
    sel.selectedIndex = female ? 1 : 0;
    return true;
  }

  function fillPatient(p, opts) {
    if (!p) return;
    opts = opts || {};
    state.patient = p;
    function setIf(id, v) {
      var el = $(id); if (!el) return;
      /* never blank a field we have no value for (avoids "jumping") */
      if (v == null || v === '') return;
      el.value = v;
    }
    setIf('nid', toFa(p.nid || ''));
    setIf('first', p.first || '');
    setIf('last', p.last || '');
    setIf('father', p.father || '');
    setIf('birth', toFa(p.birth || ''));
    /* B1 FIX: mobile/phone can arrive as English/Persian/Arabic digits — always
       normalise to English first, then re-render as Persian, so the value is
       clean and never blank because of a stray direction mark. */
    setIf('mobile', toFa(trimStr(toEn(p.mobile || ''))));
    setIf('phone', toFa(trimStr(toEn(p.phone || p.landline || ''))));
    setIf('addr', p.addr || p.address || '');
    /* B1 FIX: robust gender auto-select. The stored value may be "مرد"/"زن",
       "male"/"female", "m"/"f", "1"/"2", or carry a whitespace / RLM. Match it
       against the <select> option TEXT (options have no explicit value=) so the
       right option is chosen every time. Never touches other fields. */
    setGender(p.gender);

    var full = trimStr((p.first || '') + ' ' + (p.last || ''));
    setText($('pfName'), full || 'بیمار جدید');
    setText($('pfFile'), toFa(p.file || p.nid || '----'));

    /* B2: auto-select the supplementary insurance when we recall one */
    if (p.suppIdx != null && p.suppIdx >= 0 && $('insSupp') &&
        p.suppIdx < state.supp.length) {
      $('insSupp').selectedIndex = p.suppIdx;
    }

    /* auto-select the verified base insurance (index into INSURANCES[]) */
    if (p.insurances && p.insurances.length && $('insMain')) {
      var idx = p.insurances[0];
      if (idx >= 0 && idx < state.insurances.length) {
        $('insMain').selectedIndex = idx;
      }
    }
    /* B3: only ever turn hasIns ON when we have POSITIVE insurance data;
       NEVER auto-uncheck it — the operator drives that. */
    if (p.insurances && p.insurances.length > 0 && $('hasIns')) {
      if (p.insurances[0] > 0) $('hasIns').checked = true;
    }
    recompute();
  }

  /* ==========================================================================
     RESULT LISTS
     ========================================================================== */
  function renderPatientResults(rows) {
    var box = $('patResults');
    if (!box) return;
    if (!rows || !rows.length) { box.innerHTML = '<div class="empty">نتیجه‌ای نیست</div>'; return; }
    box.innerHTML = '';
    var i, lim = Math.min(rows.length, 25);
    for (i = 0; i < lim; i++) {
      (function (p) {
        var d = document.createElement('div');
        d.className = 'row';
        d.innerHTML = '<span class="r-name">' +
          esc(trimStr((p.first || '') + ' ' + (p.last || ''))) +
          '</span><span class="r-sub">' + toFa(p.nid || '') + '</span>';
        d.onclick = function () { fillPatient(p); box.innerHTML = '<div class="empty">نتیجه‌ای نیست</div>'; toast('اطلاعات بیمار بارگذاری شد', 'ok'); };
        box.appendChild(d);
      })(rows[i]);
    }
  }

  function renderDocResults(rows) {
    var box = $('docResults');
    if (!box) return;
    state.doctors = rows || [];
    if (!rows || !rows.length) { box.innerHTML = '<div class="empty">نتیجه‌ای نیست</div>'; return; }
    box.innerHTML = '';
    var i, lim = Math.min(rows.length, 25);
    for (i = 0; i < lim; i++) {
      (function (doc, ix) {
        var d = document.createElement('div');
        d.className = 'row';
        d.innerHTML = '<span class="r-name">' + esc(doc.name || '') +
          '</span><span class="r-sub">' + esc(doc.specialty || '') + '</span>';
        d.onclick = function () { selectDoctor(doc, ix); };
        box.appendChild(d);
      })(rows[i], i);
    }
  }
  function selectDoctor(doc, ix) {
    /* populate the doctor <select> options and select this one */
    var sel = $('doc2name');
    if (sel) {
      sel.innerHTML = '';
      var opt = document.createElement('option');
      opt.value = doc.name || '';
      opt.appendChild(document.createTextNode((doc.name || '') + (doc.specialty ? ' — ' + doc.specialty : '')));
      sel.appendChild(opt);
      sel.selectedIndex = 0;
    }
    if ($('doc2code') && doc.code) $('doc2code').value = toFa(doc.code);
  }

  function renderSvcSuggest(rows) {
    var box = $('svcSuggest');
    if (!box) return;
    state.catalog = rows || [];
    if (!rows || !rows.length) { box.className = 'svc-suggest'; box.innerHTML = ''; return; }
    box.innerHTML = '';
    var i, lim = Math.min(rows.length, 40);
    for (i = 0; i < lim; i++) {
      (function (s) {
        var d = document.createElement('div');
        d.className = 's-row';
        d.innerHTML = '<span><b>' + esc(s.name || '') + '</b> <span class="s-code">' +
          toFa(s.code || '') + '</span></span>' +
          '<span class="s-price">' + money(s.price) + ' ریال</span>';
        d.onclick = function () {
          addServiceRow(s);
          box.className = 'svc-suggest'; box.innerHTML = '';
          if ($('svcSearch')) $('svcSearch').value = '';
          toast('خدمت «' + (s.name || '') + '» افزوده شد', 'ok');
        };
        box.appendChild(d);
      })(rows[i]);
    }
    box.className = 'svc-suggest open';
  }

  /* ==========================================================================
     SERVICE PICKER — mini popup (modal). v1.42.0
     A polished code+name picker. Debounced live search over the Management
     catalog (uses the C++ cached loadServices, so thousands of services stay
     fast). Add by clicking a row, by pressing Enter, or by typing a code.
     ========================================================================== */
  var _svcMdlTimer = null;
  var _svcMdlRows = [];
  var _svcMdlSel = -1;      /* keyboard-highlighted row index */
  var _svcMdlBusy = false;

  function openSvcModal(seed) {
    var m = $('svcModal'); if (!m) return;
    m.className = 'svc-modal open';
    m.setAttribute('aria-hidden', 'false');
    var q = $('svcMdlQuery'), c = $('svcMdlCode');
    if (c) c.value = '';
    if (q) { q.value = seed || ''; }
    _svcMdlSel = -1;
    renderSvcModal([], seed ? 'در حال جستجو…' : 'برای جستجو تایپ کنید…');
    /* focus the query box; if a code-like seed, focus code box instead */
    setTimeout(function () {
      var t = q;
      if (seed && /^[0-9۰-۹A-Za-z]+$/.test(seed) && c) { c.value = seed; t = c; }
      if (t) { t.focus(); if (t.select) try { t.select(); } catch (e) {} }
      if (seed) svcModalSearch(seed);
    }, 30);
  }
  function closeSvcModal() {
    var m = $('svcModal'); if (!m) return;
    m.className = 'svc-modal';
    m.setAttribute('aria-hidden', 'true');
    if (_svcMdlTimer) { clearTimeout(_svcMdlTimer); _svcMdlTimer = null; }
    var s = $('svcSearch'); if (s) { s.value = ''; try { s.focus(); } catch (e) {} }
  }
  function svcModalOpen() { return $('svcModal') && /open/.test($('svcModal').className); }

  function renderSvcModal(rows, emptyMsg) {
    _svcMdlRows = rows || [];
    var body = $('svcMdlBody');
    var hint = $('svcMdlHint');
    if (!body) return;
    if (!_svcMdlRows.length) {
      body.innerHTML = '<tr><td colspan="5" class="empty">' +
        esc(emptyMsg || 'نتیجه‌ای نیست') + '</td></tr>';
      if (hint) setText(hint, emptyMsg || 'نتیجه‌ای نیست');
      return;
    }
    var html = '', i, s, lim = Math.min(_svcMdlRows.length, 200);
    for (i = 0; i < lim; i++) {
      s = _svcMdlRows[i];
      var cat = trimStr((s.category || '') + (s.dept ? ' / ' + s.dept : ''));
      html +=
        '<tr data-i="' + i + '"' + (i === _svcMdlSel ? ' class="sel"' : '') + '>' +
        '<td class="c-code">' + toFa(s.code || '—') + '</td>' +
        '<td class="c-name">' + esc(s.name || '') + '</td>' +
        '<td class="c-cat">' + esc(cat || '—') + '</td>' +
        '<td class="c-price">' + money(s.price) + '</td>' +
        '<td class="c-act"><button class="svc-mdl-add" data-add="' + i + '">افزودن</button></td>' +
        '</tr>';
    }
    body.innerHTML = html;
    if (hint) setText(hint, 'تعداد نتایج: ' + toFa(_svcMdlRows.length));

    /* row click (anywhere) selects+highlights; the button adds */
    var trs = body.getElementsByTagName('tr'), k;
    for (k = 0; k < trs.length; k++) {
      trs[k].onclick = function (e) {
        e = e || window.event; var tgt = e.target || e.srcElement;
        var idx;
        if (tgt.getAttribute && tgt.getAttribute('data-add') != null) {
          idx = +tgt.getAttribute('data-add');
          svcModalAdd(idx);
          return;
        }
        var tr = tgt; while (tr && tr.getAttribute && tr.getAttribute('data-i') == null) tr = tr.parentNode;
        if (tr && tr.getAttribute) { _svcMdlSel = +tr.getAttribute('data-i'); highlightSvcModal(); }
      };
      trs[k].ondblclick = function (e) {
        e = e || window.event; var tgt = e.target || e.srcElement;
        var tr = tgt; while (tr && tr.getAttribute && tr.getAttribute('data-i') == null) tr = tr.parentNode;
        if (tr && tr.getAttribute) svcModalAdd(+tr.getAttribute('data-i'));
      };
    }
  }
  function highlightSvcModal() {
    var body = $('svcMdlBody'); if (!body) return;
    var trs = body.getElementsByTagName('tr'), k;
    for (k = 0; k < trs.length; k++) {
      var idx = trs[k].getAttribute && trs[k].getAttribute('data-i');
      trs[k].className = (idx != null && +idx === _svcMdlSel) ? 'sel' : '';
    }
    /* keep the highlighted row in view */
    if (_svcMdlSel >= 0 && trs[_svcMdlSel] && trs[_svcMdlSel].scrollIntoView) {
      try { trs[_svcMdlSel].scrollIntoView({ block: 'nearest' }); } catch (e) {}
    }
  }
  function svcModalAdd(idx) {
    var s = _svcMdlRows[idx];
    if (!s) return;
    addServiceRow(s);
    toast('خدمت «' + (s.name || '') + '» افزوده شد', 'ok');
    /* keep the modal open for rapid multi-add; clear the query for the next */
    var q = $('svcMdlQuery');
    if (q) { q.value = ''; try { q.focus(); } catch (e) {} }
    renderSvcModal([], 'برای افزودن خدمتِ بعدی جستجو کنید…');
  }
  function svcModalSearch(q) {
    q = trimStr(q == null ? ($('svcMdlQuery') ? $('svcMdlQuery').value : '') : q);
    if (_svcMdlTimer) clearTimeout(_svcMdlTimer);
    if (!q) { renderSvcModal([], 'برای جستجو تایپ کنید…'); return; }
    renderSvcModal(_svcMdlRows, 'در حال جستجو…');
    _svcMdlTimer = setTimeout(function () {
      if (_svcMdlBusy) return;
      _svcMdlBusy = true;
      Bridge.call('service.search', { q: q }).then(function (r) {
        _svcMdlBusy = false;
        var rows = r.rows || r.services || [];
        _svcMdlSel = rows.length ? 0 : -1;
        renderSvcModal(rows, 'نتیجه‌ای یافت نشد');
        highlightSvcModal();
      })['catch'](function () { _svcMdlBusy = false; });
    }, 160);
  }
  function svcModalAddByCode() {
    var c = $('svcMdlCode'); if (!c) return;
    var code = trimStr(toEn(c.value));
    if (!code) { toast('کد خدمت را وارد کنید', 'err'); return; }
    Bridge.call('service.resolve', { code: code }).then(function (r) {
      if (r && r.found && r.service) {
        addServiceRow(r.service);
        toast('خدمت «' + (r.service.name || '') + '» افزوده شد', 'ok');
        c.value = ''; try { c.focus(); } catch (e) {}
      } else {
        /* fall back to a search so a partial code still helps */
        var q = $('svcMdlQuery'); if (q) { q.value = code; }
        svcModalSearch(code);
        toast('خدمتی با این کد یافت نشد؛ نتایج جستجو نمایش داده شد', 'err');
      }
    });
  }

  /* ==========================================================================
     QUEUE — صندوق نرفته‌ها
     ========================================================================== */
  function renderQueue(rows) {
    state.queue = rows || state.queue || [];
    var body = $('queueBody');
    if (!body) return;
    var filter = filterQueue(state.queue);
    setText($('qCount'), toFa(state.queue.length));
    if (!filter.length) {
      body.innerHTML = '<tr><td colspan="7" class="empty">موردی در صندوق نیست</td></tr>';
      return;
    }
    var html = '', i, q, lim = Math.min(filter.length, 60);
    for (i = 0; i < lim; i++) {
      q = filter[i];
      html += '<tr>' +
        '<td>' + toFa(i + 1) + '</td>' +
        '<td class="td-name">' + esc(q.name || '') + '</td>' +
        '<td>' + toFa(q.barcode || q.nid || '') + '</td>' +
        '<td>' + toFa(q.date || '') + '</td>' +
        '<td>' + toFa(q.time || '') + '</td>' +
        '<td>' + toFa(q.minsAgo != null ? (q.minsAgo + ' دقیقه') : '') + '</td>' +
        '<td>' +
          '<button class="act-btn act-repeat" title="بازخوانی" data-q="' + i + '">⟳</button>' +
          '<button class="act-btn act-del" title="حذف" data-qdel="' + i + '">✕</button>' +
        '</td>' +
      '</tr>';
    }
    body.innerHTML = html;
    var reps = body.getElementsByClassName('act-repeat');
    for (i = 0; i < reps.length; i++) {
      reps[i].onclick = function (e) {
        e = e || window.event; var tgt = e.target || e.srcElement;
        var q = filter[+tgt.getAttribute('data-q')];
        if (q) { fillPatient(q); toast('بیمار از صندوق بازخوانی شد', 'ok'); }
      };
    }
    var qdels = body.getElementsByClassName('act-del');
    for (i = 0; i < qdels.length; i++) {
      qdels[i].onclick = function (e) {
        e = e || window.event; var tgt = e.target || e.srcElement;
        var q = filter[+tgt.getAttribute('data-qdel')];
        if (q) Bridge.call('queue.remove', { id: q.id }).then(function () { refreshQueue(); });
      };
    }
  }
  function filterQueue(rows) {
    var q = $('qSearch') ? trimStr($('qSearch').value) : '';
    var qd = toEn(q);
    var mins = $('qMinutes') ? parseInt($('qMinutes').value, 10) || 0 : 0;
    var out = [], i, r;
    for (i = 0; i < rows.length; i++) {
      r = rows[i];
      if (q) {
        var name = String(r.name || ''), nid = toEn(String(r.nid || '')), bc = toEn(String(r.barcode || ''));
        if (name.indexOf(q) < 0 && nid.indexOf(qd) < 0 && bc.indexOf(qd) < 0) continue;
      }
      if (mins > 0 && r.minsAgo != null && r.minsAgo > mins) continue;
      out.push(r);
    }
    return out;
  }
  function refreshQueue() {
    Bridge.call('queue.list', {}).then(function (r) { renderQueue(r.rows || []); });
  }

  /* ==========================================================================
     NAVIGATION (B1) — Enter / Tab advance through an EXPLICIT hard-coded id
     order that matches the secretary's real visual/logical workflow, NOT the
     raw DOM order (wrappers rearrange cells under RTL, so DOM order is wrong).
     Auxiliary search boxes and the table qty cells are deliberately NOT here —
     they keep their own dedicated Enter handlers.
     ========================================================================== */
  var NAV_ORDER = [
    'nid', 'first', 'last', 'father',
    'birth', 'gender', 'mobile', 'phone', 'addr',
    'insMain', 'ptype', 'ntype', 'insSuppPct',
    'apptDate', 'apptShift',
    'doc2code', 'doc2name', 'perfcode', 'perfname',
    'insBooklet', 'insValid', 'rxDate', 'insSupp'
  ];

  function isVisible(el) {
    if (!el) return false;
    if (el.disabled) return false;
    if (el.type === 'hidden') return false;
    /* offsetParent is null for display:none elements (fast IE-safe check) */
    if (el.offsetParent === null && el.offsetWidth === 0 && el.offsetHeight === 0) return false;
    return true;
  }

  /* Map NAV_ORDER ids to live, visible + enabled DOM elements (in order). */
  function navElements() {
    var arr = [], i, el;
    for (i = 0; i < NAV_ORDER.length; i++) {
      el = $(NAV_ORDER[i]);
      if (!el) continue;
      if (!isVisible(el)) continue;
      arr.push(el);
    }
    return arr;
  }

  /* Index of `cur` inside NAV_ORDER (not DOM order). -1 when not a nav field. */
  function navIndexOf(cur) {
    if (!cur || !cur.id) return -1;
    var i;
    for (i = 0; i < NAV_ORDER.length; i++) if (NAV_ORDER[i] === cur.id) return i;
    return -1;
  }

  function focusEl(n) {
    if (!n) return;
    try { n.focus(); } catch (e) {}
    if (n.tagName === 'INPUT') {
      /* n.select() works on Chromium/WebView2; on some MSHTML builds it fails
         silently, so fall back to an explicit selection range. */
      var selected = false;
      if (n.select) { try { n.select(); selected = true; } catch (e2) { selected = false; } }
      if (n.setSelectionRange) {
        try { n.setSelectionRange(0, (n.value || '').length); } catch (e3) {}
      }
    }
  }

  /* Advance to the next live nav element AFTER `cur` (no wrap-around). */
  function focusNext(cur) {
    var arr = navElements();
    var oi = navIndexOf(cur), i, ci = -1;
    for (i = 0; i < arr.length; i++) if (arr[i] === cur) { ci = i; break; }
    if (ci >= 0) { if (ci + 1 < arr.length) focusEl(arr[ci + 1]); return; }
    /* cur not currently visible in the live list — fall back to NAV_ORDER pos */
    if (oi >= 0) {
      for (i = oi + 1; i < NAV_ORDER.length; i++) {
        var el = $(NAV_ORDER[i]); if (el && isVisible(el)) { focusEl(el); return; }
      }
      return;
    }
    if (arr.length) focusEl(arr[0]);
  }

  /* Go back to the previous live nav element BEFORE `cur` (no wrap-around). */
  function focusPrev(cur) {
    var arr = navElements();
    var oi = navIndexOf(cur), i, ci = -1;
    for (i = 0; i < arr.length; i++) if (arr[i] === cur) { ci = i; break; }
    if (ci >= 0) { if (ci - 1 >= 0) focusEl(arr[ci - 1]); return; }
    if (oi >= 0) {
      for (i = oi - 1; i >= 0; i--) {
        var el = $(NAV_ORDER[i]); if (el && isVisible(el)) { focusEl(el); return; }
      }
    }
  }

  /* ==========================================================================
     BIRTH-DATE (and appt-date) auto-slash + Ctrl+A + clean replace
     ========================================================================== */
  function wireDateField(el) {
    if (!el) return;
    on(el, 'keydown', function (e) {
      e = e || window.event;
      var key = e.keyCode || e.which;
      /* Ctrl+A → select whole value (default anyway, but ensure it) */
      if ((e.ctrlKey || e.metaKey) && (key === 65)) {
        try { el.select(); } catch (er) {}
      }
    });
    on(el, 'input', function () { formatDate(el); });
    on(el, 'keyup', function (e) {
      e = e || window.event; var key = e.keyCode || e.which;
      if (key === 8 || key === 46) return; /* don't re-add slash on delete */
      formatDate(el);
    });
  }
  function formatDate(el) {
    var raw = toEn(el.value).replace(/[^0-9]/g, '');
    if (raw.length > 8) raw = raw.substr(0, 8);
    var out = raw;
    if (raw.length > 4) out = raw.substr(0, 4) + '/' + raw.substr(4, 2) + (raw.length > 6 ? '/' + raw.substr(6) : '');
    else if (raw.length > 0) out = raw;
    /* keep caret comfortable: only rewrite if changed */
    var fa = toFa(out);
    if (el.value !== fa) el.value = fa;
  }

  /* ==========================================================================
     CLOCK (top bar)
     ========================================================================== */
  var FA_DAYS = ['یکشنبه', 'دوشنبه', 'سه‌شنبه', 'چهارشنبه', 'پنجشنبه', 'جمعه', 'شنبه'];
  function tickClock() {
    var d = new Date();
    function p(n) { return (n < 10 ? '0' : '') + n; }
    setText($('tbClock'), toFa(p(d.getHours()) + ':' + p(d.getMinutes()) + ':' + p(d.getSeconds())));
    /* date label is refreshed from C++ (Jalali) on init; keep placeholder here */
  }

  /* ==========================================================================
     WIRING
     ========================================================================== */
  function wire() {
    /* --- B1: canonical Enter/Tab/Shift+Tab/Ctrl+A handler wired by id over
       NAV_ORDER (NOT a CSS selector). Direct per-element binding is the
       engine-reliable path (event delegation is unreliable on WebView2/MSHTML).
       On <select>, Enter must advance focus and NEVER open the dropdown. --- */
    function fieldKeydown(el) {
      return function (e) {
        e = e || window.event;
        var key = e.keyCode || e.which;

        /* Ctrl+A / Cmd+A → select the entire value inside an INPUT */
        if ((e.ctrlKey || e.metaKey) && (key === 65 || key === 97)) {
          if (el.tagName === 'INPUT') {
            try { el.select(); } catch (er) {}
            if (e.preventDefault) e.preventDefault(); else e.returnValue = false;
            if (e.stopPropagation) e.stopPropagation();
            return false;
          }
        }

        /* Enter (13) → advance; nid triggers lookup FIRST then advances.
           Tab (9) with no Shift → advance (normalise across engines). */
        if (key === 13 || (key === 9 && !e.shiftKey)) {
          if (e.preventDefault) e.preventDefault(); else e.returnValue = false;
          if (e.stopPropagation) e.stopPropagation();
          if (el.id === 'nid' && key === 13) {
            lookupNid(el);  /* lookupNid calls focusNext(el) on completion */
            return false;
          }
          focusNext(el);
          return false;
        }

        /* Shift+Tab → go back */
        if (key === 9 && e.shiftKey) {
          if (e.preventDefault) e.preventDefault(); else e.returnValue = false;
          if (e.stopPropagation) e.stopPropagation();
          focusPrev(el);
          return false;
        }
      };
    }
    function selectOnFocus(el) {
      return function () { if (el.tagName === 'INPUT') { try { el.select(); } catch (e) {} } };
    }
    /* Bind by id iteration over NAV_ORDER (once, after DOM is ready). */
    var _ni, _el;
    for (_ni = 0; _ni < NAV_ORDER.length; _ni++) {
      _el = $(NAV_ORDER[_ni]);
      if (!_el) continue;
      (function (el) {
        on(el, 'keydown', fieldKeydown(el));
        if (el.tagName === 'INPUT') on(el, 'focus', selectOnFocus(el));
        /* <select>: also attach keypress + a guarded keyup so Enter advances
           reliably on MSHTML (where preventDefault on keydown is unreliable and
           the dropdown may open before the handler runs). Space still opens. */
        if (el.tagName === 'SELECT') {
          on(el, 'keypress', fieldKeydown(el));
          on(el, 'keyup', function (e) {
            e = e || window.event;
            var k = e.keyCode || e.which;
            if (k === 13 && el.__navHandled !== true) {
              el.__navHandled = true;
              focusNext(el);
              setTimeout(function () { el.__navHandled = false; }, 100);
            }
          });
        }
      })(_el);
    }

    /* national id via the quick-search box too */
    on($('qsNid'), 'keydown', function (e) {
      e = e || window.event; var key = e.keyCode || e.which;
      if (key === 13) { if (e.preventDefault) e.preventDefault(); doQuickNid(); }
    });
    on($('qsNidBtn'), 'click', doQuickNid);
    on($('qsFileBtn'), 'click', doPatFileSearch);
    on($('qsFile'), 'keydown', function (e) { e = e || window.event; if ((e.keyCode || e.which) === 13) { if (e.preventDefault) e.preventDefault(); doPatFileSearch(); } });

    /* birth-date + appt-date behaviours */
    var dates = document.querySelectorAll('[data-date]'), di;
    for (di = 0; di < dates.length; di++) wireDateField(dates[di]);

    /* doctor search */
    on($('docSearchBtn'), 'click', doDocSearch);
    on($('docCodeBtn'), 'click', doDocByCode);
    on($('docSearch'), 'keydown', function (e) { e = e || window.event; if ((e.keyCode || e.which) === 13) { if (e.preventDefault) e.preventDefault(); doDocSearch(); } });
    on($('docCode'), 'keydown', function (e) { e = e || window.event; if ((e.keyCode || e.which) === 13) { if (e.preventDefault) e.preventDefault(); doDocByCode(); } });

    /* service live search */
    var svcTimer = null;
    function svcSearch() {
      if (svcTimer) clearTimeout(svcTimer);
      var q = $('svcSearch') ? trimStr($('svcSearch').value) : '';
      if (!q) { renderSvcSuggest([]); return; }
      svcTimer = setTimeout(function () {
        Bridge.call('service.search', { q: q }).then(function (r) { renderSvcSuggest(r.rows || r.services || []); });
      }, 180);
    }
    on($('svcSearch'), 'input', svcSearch);
    on($('svcSearch'), 'keyup', svcSearch);
    /* Enter confirms the current service selection. To avoid ever adding a
       STALE match (the 180ms debounce may not have fired yet), we always ask
       C++ for the freshest match for the exact query and add the first hit —
       so Enter reflects the live Management catalog, not a cached suggestion. */
    var _svcSearchInFlight = false;   /* B4: debounce Enter re-entry */
    on($('svcSearch'), 'keydown', function (e) {
      e = e || window.event;
      if ((e.keyCode || e.which) !== 13) return;
      if (e.preventDefault) e.preventDefault();
      if (_svcSearchInFlight) return;   /* a service.search is already running */
      var q = $('svcSearch') ? trimStr($('svcSearch').value) : '';
      if (!q) return;
      _svcSearchInFlight = true;
      Bridge.call('service.search', { q: q }).then(function (r) {
        _svcSearchInFlight = false;
        var rows = r.rows || r.services || [];
        state.catalog = rows;
        if (rows.length) {
          addServiceRow(rows[0]);
          renderSvcSuggest([]); $('svcSearch').value = '';
          toast('خدمت «' + (rows[0].name || '') + '» افزوده شد', 'ok');
        } else {
          renderSvcSuggest([]);
          toast('خدمتی با این عبارت یافت نشد', 'err');
        }
      })['catch'](function () { _svcSearchInFlight = false; });
    });
    /* افزودن خدمت button now opens the polished mini-picker modal (v1.42.0),
       seeded with whatever is already in the inline search box. */
    on($('svcAddBtn'), 'click', function () {
      var q = $('svcSearch') ? trimStr($('svcSearch').value) : '';
      renderSvcSuggest([]);
      openSvcModal(q);
    });

    /* ---- service-picker modal wiring (v1.42.0) ---- */
    on($('svcModalClose'), 'click', closeSvcModal);
    on($('svcModalBackdrop'), 'click', closeSvcModal);
    on($('svcMdlDone'), 'click', closeSvcModal);
    on($('svcMdlCodeAdd'), 'click', svcModalAddByCode);
    on($('svcMdlQuery'), 'input', function () { svcModalSearch(); });
    on($('svcMdlQuery'), 'keyup', function () { svcModalSearch(); });
    on($('svcMdlCode'), 'keydown', function (e) {
      e = e || window.event; if ((e.keyCode || e.which) === 13) {
        if (e.preventDefault) e.preventDefault(); svcModalAddByCode();
      }
    });
    on($('svcMdlQuery'), 'keydown', function (e) {
      e = e || window.event; var k = e.keyCode || e.which;
      if (k === 40) {            /* ArrowDown */
        if (e.preventDefault) e.preventDefault();
        if (_svcMdlSel < _svcMdlRows.length - 1) _svcMdlSel++; highlightSvcModal();
      } else if (k === 38) {     /* ArrowUp */
        if (e.preventDefault) e.preventDefault();
        if (_svcMdlSel > 0) _svcMdlSel--; highlightSvcModal();
      } else if (k === 13) {     /* Enter → add highlighted */
        if (e.preventDefault) e.preventDefault();
        if (_svcMdlSel >= 0) svcModalAdd(_svcMdlSel);
      }
    });
    /* Esc closes the modal from anywhere inside it */
    on($('svcModal'), 'keydown', function (e) {
      e = e || window.event; if ((e.keyCode || e.which) === 27) { closeSvcModal(); }
    });

    /* insurance changes → recompute (never blanks patient fields) */
    on($('insMain'), 'change', function () { renderServices(); recompute(); });
    on($('insSupp'), 'change', function () { renderServices(); recompute(); });
    on($('insSuppPct'), 'input', function () { renderServices(); recompute(); });
    on($('insSuppPct'), 'keyup', function () { renderServices(); recompute(); });
    on($('hasIns'), 'change', function () { renderServices(); recompute(); });
    on($('noPay'), 'change', function () { recompute(); });

    /* collapse / expand lists */
    on($('svcToggle'), 'click', function () { toggleWrap('svcTblWrap', this); });
    on($('queueToggle'), 'click', function () { toggleWrap('queueWrap', this); });

    /* queue search / filter / add */
    on($('qSearch'), 'input', function () { renderQueue(state.queue); });
    on($('qSearch'), 'keyup', function () { renderQueue(state.queue); });
    on($('qMinutes'), 'change', function () { renderQueue(state.queue); });
    on($('addToQueue'), 'click', addCurrentToQueue);
    on($('tabQueue'), 'click', function () { setActiveTab('tabQueue'); refreshQueue(); });
    on($('tabAdmQ'), 'click', function () { setActiveTab('tabAdmQ'); refreshQueue(); });

    /* save / clear / new */
    on($('btnSave'), 'click', saveAdmission);
    on($('btnNew'), 'click', function () { clearForm(); Bridge.call('admission.new', {}); toast('پذیرش جدید', 'ok'); });
    on($('hdrNewAdm'), 'click', function () { clearForm(); toast('پذیرش جدید', 'ok'); });
    on($('hdrNew'), 'click', function () { clearForm(); toast('پذیرش جدید', 'ok'); });
    on($('btnClear'), 'click', function () { clearForm(); Bridge.call('admission.clear', {}); });
    on($('btnCancel'), 'click', function () { clearForm(); });

    /* print buttons */
    on($('prtIns'), 'click', function () { Bridge.call('print.insurance', collectRecord()); toast('در حال چاپ رسید بیمه…'); });
    on($('prtRx'), 'click', function () { Bridge.call('print.rx', collectRecord()); toast('در حال چاپ نسخه…'); });
    on($('prtLast'), 'click', function () { Bridge.call('print.last', {}); toast('چاپ آخرین قبض…'); });
    on($('hdrPrint'), 'click', function () { Bridge.call('print.last', {}); });
    on($('hdrSettings'), 'click', function () { Bridge.call('ui.settings', {}); });
    on($('btnErx'), 'click', function () { Bridge.call('rx.electronic', collectRecord()); });

    /* F8 = print last */
    on(document, 'keydown', function (e) {
      e = e || window.event; var key = e.keyCode || e.which;
      if (key === 119) { Bridge.call('print.last', {}); }        /* F8 */
    });
  }

  function toggleWrap(id, btn) {
    var w = $(id);
    if (!w) return;
    if (/expanded/.test(w.className)) {
      w.className = w.className.replace(/\s*expanded/, '');
      if (btn) btn.innerHTML = '&#9662;';
    } else {
      w.className += ' expanded';
      if (btn) btn.innerHTML = '&#9652;';
    }
  }
  function setActiveTab(id) {
    var tabs = document.getElementsByClassName('tab'), i;
    for (i = 0; i < tabs.length; i++) tabs[i].className = 'tab';
    if ($(id)) $(id).className = 'tab active';
  }

  /* --- lookups --- */
  function lookupNid(el) {
    var nid = toEn(el.value).replace(/\s+/g, '');
    if (!nid) { focusNext(el); return; }
    setSync('', 'در حال استعلام…');
    Bridge.call('patient.lookup', { nid: nid }).then(function (r) {
      if (r && r.found) { fillPatient(r.patient || r); toast('اطلاعات بیمار بارگذاری شد', 'ok'); }
      else toast('بیماری با این کد ملی یافت نشد — لطفاً دستی وارد کنید', 'err');
      setSync('ok', 'همگام');
      focusNext(el);
    })['catch'](function (err) { toast('خطا در استعلام', 'err'); setSync('err', 'خطا'); focusNext(el); });
  }
  function doQuickNid() {
    var nid = toEn($('qsNid') ? $('qsNid').value : '').replace(/\s+/g, '');
    if (!nid) return;
    Bridge.call('patient.lookup', { nid: nid }).then(function (r) {
      if (r && r.found) { fillPatient(r.patient || r); toast('اطلاعات بیمار بارگذاری شد', 'ok'); }
      else toast('یافت نشد', 'err');
    });
  }
  function doPatFileSearch() {
    var q = $('qsFile') ? trimStr($('qsFile').value) : '';
    Bridge.call('patient.search', { q: q }).then(function (r) { renderPatientResults(r.rows || r.patients || []); });
  }
  function doDocSearch() {
    var q = $('docSearch') ? trimStr($('docSearch').value) : '';
    Bridge.call('doctor.search', { q: q }).then(function (r) { renderDocResults(r.rows || r.doctors || []); });
  }
  function doDocByCode() {
    var code = toEn($('docCode') ? $('docCode').value : '').replace(/\s+/g, '');
    Bridge.call('doctor.search', { q: code, code: code }).then(function (r) {
      var rows = r.rows || r.doctors || [];
      renderDocResults(rows);
      if (rows.length) selectDoctor(rows[0], 0);
    });
  }

  /* --- add current admission to queue (صندوق نرفته‌ها) --- */
  function addCurrentToQueue() {
    var rec = collectRecord();
    if (!rec.patient.nid) { toast('ابتدا کد ملی بیمار را وارد کنید', 'err'); return; }
    Bridge.call('queue.add', rec).then(function (r) {
      if (r && r.ok) { toast('به صندوق نرفته‌ها افزوده شد', 'ok'); refreshQueue(); }
      else toast('افزودن ناموفق بود', 'err');
    })['catch'](function () { toast('خطا در افزودن به صندوق', 'err'); });
  }

  /* --- save admission + print per Management design --- */
  function saveAdmission() {
    var rec = collectRecord();
    if (!rec.patient.nid) { toast('کد ملی بیمار الزامی است', 'err'); if ($('nid')) $('nid').focus(); return; }
    if (!rec.patient.first || !rec.patient.last) { toast('نام و نام خانوادگی الزامی است', 'err'); return; }
    if (!state.services.length) { toast('حداقل یک خدمت باید افزوده شود', 'err'); return; }
    setSync('', 'در حال ثبت…');
    Bridge.call('admission.save', rec).then(function (r) {
      if (r && r.ok) {
        toast('پذیرش ثبت و قبض چاپ شد' + (r.queueNo ? ' — نوبت ' + toFa(r.queueNo) : ''), 'ok');
        /* B5: warn when the classic-GDI fallback template was used because no
           print-design is bound to the operator's section. */
        if (r.printMode && String(r.printMode).indexOf('classic-') === 0) {
          setTimeout(function () {
            toast('هیچ دیزاین چاپی به بخش شما متصل نیست — قالب پیش‌فرض استفاده شد', 'warn');
          }, 700);
        }
        if (r.ps) updatePS(r.ps);
        refreshQueue();
      } else {
        toast('ثبت ناموفق: ' + ((r && r.err) || 'نامشخص'), 'err');
      }
      setSync('ok', 'همگام');
    })['catch'](function (err) { toast('خطا در ثبت پذیرش', 'err'); setSync('err', 'خطا'); });
  }

  /* --- collect the full record for C++ --- */
  function val(id) { var e = $(id); return e ? e.value : ''; }
  function trimEn(id) { return toEn(val(id)).replace(/^\s+|\s+$/g, ''); }
  function trimFa(id) { return trimStr(val(id)); }

  function collectRecord() {
    var totals = recompute();
    return {
      patient: {
        nid: trimEn('nid'), first: trimFa('first'), last: trimFa('last'),
        father: trimFa('father'), gender: val('gender'),
        birth: trimEn('birth'), mobile: trimEn('mobile'),
        phone: trimEn('phone'), addr: trimFa('addr')
      },
      hasIns: hasIns(),
      insMain: $('insMain') ? $('insMain').selectedIndex : -1,
      insSupp: $('insSupp') ? $('insSupp').selectedIndex : -1,
      insSuppPct: suppInsPct(),
      insBooklet: trimEn('insBooklet'),
      insValid: trimEn('insValid'),
      ptype: val('ptype'), ntype: val('ntype'),
      doc2code: trimEn('doc2code'), doc2name: trimFa('doc2name'),
      perfcode: trimEn('perfcode'), perfname: trimFa('perfname'),
      apptDate: trimEn('apptDate'), apptShift: val('apptShift'),
      rxDate: trimEn('rxDate'),
      noPay: $('noPay') ? $('noPay').checked : false,
      services: state.services,
      totals: totals
    };
  }

  function clearForm() {
    var ids = ['nid', 'first', 'last', 'father', 'birth', 'mobile', 'phone', 'addr',
      'doc2code', 'perfcode', 'perfname', 'insBooklet', 'insValid', 'rxDate', 'apptDate',
      'svcSearch', 'qsNid', 'qsFile', 'docCode', 'docSearch'];
    var i;
    for (i = 0; i < ids.length; i++) { if ($(ids[i])) $(ids[i]).value = ''; }
    if ($('doc2name')) $('doc2name').innerHTML = '<option value="">— انتخاب —</option>';
    if ($('perfname')) $('perfname').innerHTML = '<option value="">— انتخاب —</option>';
    if ($('insSuppPct')) $('insSuppPct').value = toFa('0');
    if ($('insMain')) $('insMain').selectedIndex = 0;
    if ($('hasIns')) $('hasIns').checked = false;
    if ($('noPay')) $('noPay').checked = false;
    state.services = []; state.patient = null; state.catalog = [];
    setText($('pfName'), 'بیمار جدید');
    setText($('pfFile'), '----');
    if ($('patResults')) $('patResults').innerHTML = '<div class="empty">نتیجه‌ای نیست</div>';
    if ($('docResults')) $('docResults').innerHTML = '<div class="empty">نتیجه‌ای نیست</div>';
    renderSvcSuggest([]);
    renderServices(); recompute();
    if ($('first')) $('first').focus();
  }

  function updatePS(ps) {
    if (ps.P != null) { state.ps.P = ps.P; setText($('psPVal'), toFa(ps.P)); }
    if (ps.S != null) { state.ps.S = ps.S; setText($('psSVal'), toFa(ps.S)); }
  }

  /* ==========================================================================
     C++ → JS events
     ========================================================================== */
  function subscribeEvents() {
    /* v1.43.0: live theme sync (dark ⇄ light) pushed from the native shell */
    Bridge.on('theme', function (d) { applyTheme(d && d.theme); });
    Bridge.on('patient.load', function (d) { fillPatient(d); });
    Bridge.on('services.update', function (d) {
      if (d.rows) { state.services = d.rows; renderServices(); recompute(); }
    });
    Bridge.on('queue.update', function (d) { renderQueue(d.rows || []); });
    Bridge.on('ps.update', function (d) { updatePS(d); });
    Bridge.on('clock.update', function (d) {
      if (d.time) setText($('tbClock'), toFa(d.time));
      if (d.date) setText($('tbDate'), toFa(d.date));
    });
    Bridge.on('insurance.update', function (d) {
      if (d.main) { state.insurances = d.main; fillSelect($('insMain'), d.main); }
      if (d.supp) { state.supp = d.supp; fillSelect($('insSupp'), d.supp); }
      recompute();
    });
    /* Debug-only: lets the headless --smoke-admission-keys test place a value in
       #nid and focus it, so the synthesized Enter keystroke exercises the real
       keydown → lookupNid path end-to-end. No effect in normal operation. */
    Bridge.on('debug.focusNid', function (d) {
      var el = $('nid');
      if (!el) return;
      if (d && d.nid != null) el.value = '' + d.nid;
      try { el.focus(); } catch (e) {}
      if (el.setSelectionRange) { try { el.setSelectionRange(0, (el.value || '').length); } catch (e2) {} }
    });
    /* LIVE service catalog sync from Management (add / edit / delete a service).
       We keep the freshest catalog and, if the suggestion dropdown is currently
       open, re-run the active search so the operator sees the change instantly —
       with NO page reload. Prices of already-added rows are refreshed too, so an
       admission in progress reflects a Management price edit immediately. */
    Bridge.on('catalog.update', function (d) {
      var rows = (d && (d.services || d.rows)) || [];
      state.fullCatalog = rows;
      /* refresh prices of rows already in the current admission */
      if (state.services.length) {
        var i, j, changed = false;
        for (i = 0; i < state.services.length; i++) {
          for (j = 0; j < rows.length; j++) {
            if (rows[j].code && rows[j].code === state.services[i].code) {
              var np = Number(rows[j].price) || 0;
              if (state.services[i].price !== np) { state.services[i].price = np; changed = true; }
              if (rows[j].name) state.services[i].name = rows[j].name;
              break;
            }
          }
        }
        if (changed) { renderServices(); recompute(); }
      }
      /* if the suggestion list is open, re-run the current query live */
      var sug = $('svcSuggest');
      if (sug && /open/.test(sug.className)) {
        var q = $('svcSearch') ? trimStr($('svcSearch').value) : '';
        if (q) Bridge.call('service.search', { q: q }).then(function (r) { renderSvcSuggest(r.rows || r.services || []); });
      }
      toast('فهرست خدمات به‌روزرسانی شد', 'ok');
    });
  }

  function fillSelect(sel, arr) {
    if (!sel) return;
    var cur = sel.selectedIndex, i;
    sel.innerHTML = '';
    for (i = 0; i < arr.length; i++) {
      var opt = document.createElement('option');
      opt.appendChild(document.createTextNode(arr[i].name));
      sel.appendChild(opt);
    }
    if (cur >= 0 && cur < arr.length) sel.selectedIndex = cur; else sel.selectedIndex = 0;
  }

  /* ==========================================================================
     LOADER + BOOT
     ========================================================================== */
  var _loaderHidden = false;
  function hideLoader() {
    if (_loaderHidden) return; _loaderHidden = true;
    var el = $('loader'); if (!el) return;
    if (el.className.indexOf('hide') < 0) el.className += ' hide';
    setTimeout(function () { if (el) el.style.display = 'none'; }, 420);
  }
  function loaderText(t) { setText($('loaderText'), t); }

  function boot() {
    loaderText('در حال همگام‌سازی با برنامه…');
    Bridge.call('init', {}).then(function (r) {
      if (r.insurances) { state.insurances = r.insurances; fillSelect($('insMain'), r.insurances); }
      if (r.supp) { state.supp = r.supp; fillSelect($('insSupp'), r.supp); }
      if (r.date) setText($('tbDate'), toFa(r.date));
      if (r.time) setText($('tbClock'), toFa(r.time));
      if (r.theme) applyTheme(r.theme);   /* v1.43.0: open already in the right theme */
      if (r.patient) fillPatient(r.patient);
      if (r.services) { state.services = r.services; }
      if (r.ps) updatePS(r.ps);
      /* invoice starts at ZERO until a service is added */
      renderServices(); recompute();
      refreshQueue();
      setSync('ok', 'همگام با برنامه');
      state.ready = true;
      hideLoader();
      toast('پذیرش بیمار آماده است', 'ok');
    })['catch'](function (err) {
      setSync('err', 'قطع ارتباط با برنامه');
      renderServices(); recompute();
      hideLoader();
      if (window.console) console.error('init failed', err);
    });
  }

  function domReady(fn) {
    if (document.readyState === 'complete' || document.readyState === 'interactive') setTimeout(fn, 1);
    else if (document.addEventListener) document.addEventListener('DOMContentLoaded', fn, false);
    else document.attachEvent('onreadystatechange', function () { if (document.readyState === 'complete') fn(); });
  }

  domReady(function () {
    wire();
    subscribeEvents();
    setActiveTab('tabQueue');
    renderServices();
    recompute();               /* zero invoice on open */
    tickClock();
    setInterval(tickClock, 1000);
    Bridge.ready(boot);
    setTimeout(hideLoader, 8000);   /* safety net */
  });
})();
