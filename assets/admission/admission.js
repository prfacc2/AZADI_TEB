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

  /* ==========================================================================
     v1.50.0 — FREEZE-PROOF HELPERS.
     Root cause of the whole-app freeze: the old per-row onclick handlers
     SYNCHRONOUSLY tore down (innerHTML='') the very DOM node that was still
     dispatching the click event. The in-process MSHTML/Trident engine shares
     the app's Win32 message pump; destroying the dispatching node wedges it and
     freezes the ENTIRE program (close button, tab switch, everything).
     Solution: every handler that rebuilds a list it lives inside MUST route the
     mutation through defer() so the DOM is only touched after the event has
     fully unwound. ========================================================================== */
  function defer(fn) {
    setTimeout(function () {
      try { fn(); } catch (e) { if (window.console) console.error(e); }
    }, 0);
  }

  /* nearest ancestor (or self) of `node` carrying attribute `attr`, stopping at
     `stop`. ES5/MSHTML-safe (no Element.closest). */
  function findUp(node, attr, stop) {
    while (node && node !== stop) {
      if (node.getAttribute && node.getAttribute(attr) != null) return node;
      node = node.parentNode;
    }
    return null;
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
    return { gross: sumGross, disc: sumDisc, org: sumOrg, supp: sumSupp, pat: sumPat, paid: paid };
  }

  /* ==========================================================================
     SERVICE ROWS — v1.50.0 FREEZE FIX.
     The old code rebuilt the whole table (innerHTML) and re-bound per-node
     handlers SYNCHRONOUSLY inside the very event dispatch whose source node it
     was destroying. On the in-process MSHTML/Trident engine (which shares the
     app's UI thread + Win32 message pump), tearing down the node that is still
     dispatching the current event wedges the engine — and the WHOLE app froze.
     New design: addServiceRow only mutates state; ALL re-rendering goes through
     scheduleRender() (setTimeout 0) so the DOM is never torn down while an
     event is still being dispatched. Row actions use ONE delegated listener
     bound once in wire() — zero per-render handler rebinding.
     ========================================================================== */
  var _renderTimer = null;
  function scheduleRender() {
    if (_renderTimer) return;             /* coalesce bursts into one render */
    _renderTimer = setTimeout(function () {
      _renderTimer = null;
      try { renderServices(); } catch (e) { if (window.console) console.error(e); }
      try { recompute(); } catch (e2) { if (window.console) console.error(e2); }
    }, 0);
  }

  function addServiceRow(svc) {
    /* price ALWAYS from the catalog — never typed by the operator */
    state.services.push({
      code: svc.code || '', name: svc.name || '',
      qty: 1, price: Number(svc.price) || 0, discount: 0
    });
    scheduleRender();
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
        '<tr data-row="' + i + '">' +
        '<td>' + toFa(i + 1) + '</td>' +
        '<td class="td-name">' + esc(s.name || '') + '</td>' +
        '<td>' + toFa(s.code || '—') + '</td>' +
        '<td><input class="inp qty-inp" type="text" value="' + toFa(s.qty) + '" data-i="' + i + '"/></td>' +
        '<td>' + money(s.price) + '</td>' +
        '<td>' + money(s._disc) + '</td>' +
        '<td>' + money(s._org + s._supp) + '</td>' +
        '<td>' + money(s._pat) + '</td>' +
        '<td>' +
          '<button type="button" class="act-btn act-edit" data-edit="' + i + '" title="ویرایش تعداد">✎</button>' +
          '<button type="button" class="act-btn act-del" data-del="' + i + '" title="حذف">✕</button>' +
        '</td>' +
        '</tr>';
    }
    body.innerHTML = html;
    /* NO handler binding here — everything is delegated (wired once). */
  }

  /* live-refresh the computed cells of the row that owns `inputEl` WITHOUT
     rebuilding the table (rebuilding would destroy the input mid-keystroke). */
  function refreshRowCells(inputEl, idx) {
    if (!(idx >= 0 && idx < state.services.length)) return;
    var s = computeRow(state.services[idx]);
    var tr = inputEl;
    while (tr && String(tr.tagName || '').toLowerCase() !== 'tr') tr = tr.parentNode;
    if (!tr || !tr.cells || tr.cells.length < 9) return;
    setText(tr.cells[5], money(s._disc));
    setText(tr.cells[6], money(s._org + s._supp));
    setText(tr.cells[7], money(s._pat));
  }

  /* ==========================================================================
     PATIENT — fill without disturbing anything the operator is typing.
     ========================================================================== */
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
    setIf('mobile', toFa(p.mobile || ''));
    setIf('phone', toFa(p.phone || ''));   /* B2: تلفن ثابت now really arrives */
    setIf('addr', p.addr || '');           /* B2: آدرس now really arrives */
    if (p.gender && $('gender')) $('gender').value = p.gender;

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
    state.patResults = rows || [];
    if (!rows || !rows.length) { box.innerHTML = '<div class="empty">نتیجه‌ای نیست</div>'; return; }
    var html = '', i, p, lim = Math.min(rows.length, 25);
    for (i = 0; i < lim; i++) {
      p = rows[i];
      html += '<div class="row" data-p="' + i + '"><span class="r-name">' +
        esc(trimStr((p.first || '') + ' ' + (p.last || ''))) +
        '</span><span class="r-sub">' + toFa(p.nid || '') + '</span></div>';
    }
    box.innerHTML = html;   /* delegated click — wired once in wire() */
  }

  function renderDocResults(rows) {
    var box = $('docResults');
    if (!box) return;
    state.doctors = rows || [];
    if (!rows || !rows.length) { box.innerHTML = '<div class="empty">نتیجه‌ای نیست</div>'; return; }
    var html = '', i, doc, lim = Math.min(rows.length, 25);
    for (i = 0; i < lim; i++) {
      doc = rows[i];
      html += '<div class="row" data-d="' + i + '"><span class="r-name">' + esc(doc.name || '') +
        '</span><span class="r-sub">' + esc(doc.specialty || '') + '</span></div>';
    }
    box.innerHTML = html;   /* delegated click — wired once in wire() */
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
    var html = '', i, s, lim = Math.min(rows.length, 40);
    for (i = 0; i < lim; i++) {
      s = rows[i];
      html += '<div class="s-row" data-s="' + i + '">' +
        '<span><b>' + esc(s.name || '') + '</b> <span class="s-code">' +
        toFa(s.code || '') + '</span></span>' +
        '<span class="s-price">' + money(s.price) + ' ریال</span></div>';
    }
    box.innerHTML = html;   /* delegated click — wired once in wire() */
    box.className = 'svc-suggest open';
  }

  /* ==========================================================================
     QUEUE — صندوق نرفته‌ها
     ========================================================================== */
  function renderQueue(rows) {
    state.queue = rows || state.queue || [];
    var body = $('queueBody');
    if (!body) return;
    var filter = filterQueue(state.queue);
    state.queueView = filter;             /* v1.50.0: delegated handlers read this */
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
          '<button type="button" class="act-btn act-repeat" title="بازخوانی" data-q="' + i + '">⟳</button>' +
          '<button type="button" class="act-btn act-del" title="حذف" data-qdel="' + i + '">✕</button>' +
        '</td>' +
      '</tr>';
    }
    body.innerHTML = html;
    /* NO handler binding here — everything is delegated (wired once). */
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
    /* --------------------------------------------------------------------
       v1.49.0 — ROBUST "enter a service code → pick the service" handler.
       When the operator types a value and confirms it (Enter or the
       «افزودن خدمت» button) we resolve it against the LIVE Management catalog
       in two stages, so an exact SERVICE CODE always wins over a fuzzy name
       match, and multiple services can be added one after another:

         1) service.resolve  — exact code lookup (SRV0001, 1001, …). This is
            the fast, unambiguous path when the operator knows the code.
         2) service.search   — free-text fallback (name / partial code). If it
            returns exactly one hit we add it; if several, we open the picker.

       Every path is guarded by _svcSearchInFlight so a burst of Enter presses
       can never stack overlapping bridge calls (which is what used to make the
       surface feel "stuck"). The bridge itself has a 15s timeout, so even a
       lost response can never wedge the box — the flag is always cleared in
       both then() and catch().
       -------------------------------------------------------------------- */
    var _svcSearchInFlight = false;   /* B4: debounce Enter/click re-entry */

    function clearSvcBox() {
      renderSvcSuggest([]);
      if ($('svcSearch')) $('svcSearch').value = '';
    }

    /* Resolve `q` and either add the matched service or show the picker.
       `preferPicker` (used by the button) shows the list when the query is
       ambiguous instead of silently adding the first hit. */
    function resolveAndAddService(q, preferPicker) {
      q = trimStr(q || '');
      if (!q) return;
      if (_svcSearchInFlight) return;
      _svcSearchInFlight = true;

      function done() { _svcSearchInFlight = false; }

      /* stage 1: exact service-code lookup */
      Bridge.call('service.resolve', { code: q }).then(function (r) {
        if (r && r.found && r.service) {
          addServiceRow(r.service);          /* price comes from the catalog */
          clearSvcBox();
          toast('خدمت «' + (r.service.name || '') + '» افزوده شد', 'ok');
          done();
          return;
        }
        /* stage 2: free-text search fallback */
        Bridge.call('service.search', { q: q }).then(function (sr) {
          done();
          var rows = (sr && (sr.rows || sr.services)) || [];
          state.catalog = rows;
          if (!rows.length) {
            renderSvcSuggest([]);
            toast('خدمتی با این عبارت یافت نشد', 'err');
            return;
          }
          if (rows.length === 1 || !preferPicker) {
            addServiceRow(rows[0]);
            clearSvcBox();
            toast('خدمت «' + (rows[0].name || '') + '» افزوده شد', 'ok');
          } else {
            renderSvcSuggest(rows);          /* several matches → let user pick */
          }
        })['catch'](function () { done(); });
      })['catch'](function () {
        /* resolve verb unavailable/failed → degrade to pure search */
        Bridge.call('service.search', { q: q }).then(function (sr) {
          done();
          var rows = (sr && (sr.rows || sr.services)) || [];
          state.catalog = rows;
          if (rows.length) {
            if (rows.length === 1 || !preferPicker) {
              addServiceRow(rows[0]); clearSvcBox();
              toast('خدمت «' + (rows[0].name || '') + '» افزوده شد', 'ok');
            } else { renderSvcSuggest(rows); }
          } else {
            renderSvcSuggest([]);
            toast('خدمتی با این عبارت یافت نشد', 'err');
          }
        })['catch'](function () { done(); });
      });
    }

    /* Enter confirms the current service selection. */
    on($('svcSearch'), 'keydown', function (e) {
      e = e || window.event;
      if ((e.keyCode || e.which) !== 13) return;
      if (e.preventDefault) e.preventDefault(); else e.returnValue = false;
      resolveAndAddService($('svcSearch') ? $('svcSearch').value : '', false);
      return false;
    });
    /* «افزودن خدمت» button: prefer showing the picker when ambiguous. */
    on($('svcAddBtn'), 'click', function () {
      resolveAndAddService($('svcSearch') ? $('svcSearch').value : '', true);
    });

    /* ====================================================================
       v1.50.0 — DELEGATED list/table handlers (bound ONCE, never rebound).
       Root cause of the whole-app freeze: the old per-node onclick handlers
       synchronously rebuilt (innerHTML) the very subtree that was still
       dispatching the click. In-process MSHTML wedges on that, and since it
       shares the app's UI thread, the ENTIRE program froze. Delegation +
       defer() guarantees the DOM is only mutated after dispatch unwinds.
       ==================================================================== */

    /* service suggestion dropdown → pick a service (deferred) */
    on($('svcSuggest'), 'click', function (e) {
      e = e || window.event;
      var box = $('svcSuggest');
      var row = findUp(e.target || e.srcElement, 'data-s', box);
      if (!row) return;
      var s = state.catalog[+row.getAttribute('data-s')];
      if (!s) return;
      if (e.preventDefault) e.preventDefault(); else e.returnValue = false;
      defer(function () {
        addServiceRow(s);                  /* schedules its own render */
        box.className = 'svc-suggest'; box.innerHTML = '';
        if ($('svcSearch')) $('svcSearch').value = '';
        toast('خدمت «' + (s.name || '') + '» افزوده شد', 'ok');
      });
    });

    /* services table → delete / edit buttons (delegated) */
    on($('svcBody'), 'click', function (e) {
      e = e || window.event;
      var body = $('svcBody');
      var tgt = e.target || e.srcElement;
      var del = findUp(tgt, 'data-del', body);
      if (del) {
        var di = +del.getAttribute('data-del');
        defer(function () {
          if (di >= 0 && di < state.services.length) state.services.splice(di, 1);
          scheduleRender();
        });
        return;
      }
      var ed = findUp(tgt, 'data-edit', body);
      if (ed) {
        var ei = +ed.getAttribute('data-edit');
        defer(function () {
          var inp = body.getElementsByClassName('qty-inp')[ei];
          if (inp) { try { inp.focus(); inp.select(); } catch (er) {} }
        });
      }
    });

    /* services table → qty edits (delegated). While typing we only refresh the
       computed cells of THAT row (never rebuild the table under the caret);
       the full re-render happens on blur/change. */
    function onQtyEvent(e) {
      e = e || window.event;
      var tgt = e.target || e.srcElement;
      if (!tgt || !/(^|\s)qty-inp(\s|$)/.test(tgt.className || '')) return;
      var idx = +tgt.getAttribute('data-i');
      if (!(idx >= 0 && idx < state.services.length)) return;
      var q = Math.max(1, parseInt(toEn(tgt.value), 10) || 1);
      state.services[idx].qty = q;
      refreshRowCells(tgt, idx);
      recompute();                          /* totals only — no DOM teardown */
    }
    on($('svcBody'), 'input', onQtyEvent);
    on($('svcBody'), 'keyup', onQtyEvent);
    on($('svcBody'), 'change', function (e) {
      e = e || window.event;
      var tgt = e.target || e.srcElement;
      if (!tgt || !/(^|\s)qty-inp(\s|$)/.test(tgt.className || '')) return;
      onQtyEvent(e);
      scheduleRender();                     /* safe: deferred out of dispatch */
    });
    on($('svcBody'), 'keydown', function (e) {
      e = e || window.event;
      var tgt = e.target || e.srcElement;
      if (!tgt || !/(^|\s)qty-inp(\s|$)/.test(tgt.className || '')) return;
      var k = e.keyCode || e.which;
      if (k === 13) {                       /* Enter in qty → back to search */
        if (e.preventDefault) e.preventDefault(); else e.returnValue = false;
        defer(function () {
          scheduleRender();
          var s = $('svcSearch');
          if (s) { try { s.focus(); if (s.select) s.select(); } catch (er) {} }
        });
        return false;
      }
    });
    on($('svcBody'), 'dblclick', function (e) {
      e = e || window.event;
      var tgt = e.target || e.srcElement;
      if (!tgt || !/(^|\s)qty-inp(\s|$)/.test(tgt.className || '')) return;
      try { tgt.focus(); tgt.select(); } catch (er) {}
    });

    /* patient results list → load patient (delegated, deferred) */
    on($('patResults'), 'click', function (e) {
      e = e || window.event;
      var box = $('patResults');
      var row = findUp(e.target || e.srcElement, 'data-p', box);
      if (!row) return;
      var p = (state.patResults || [])[+row.getAttribute('data-p')];
      if (!p) return;
      defer(function () {
        fillPatient(p);
        box.innerHTML = '<div class="empty">نتیجه‌ای نیست</div>';
        toast('اطلاعات بیمار بارگذاری شد', 'ok');
      });
    });

    /* doctor results list → select doctor (delegated, deferred) */
    on($('docResults'), 'click', function (e) {
      e = e || window.event;
      var box = $('docResults');
      var row = findUp(e.target || e.srcElement, 'data-d', box);
      if (!row) return;
      var ix = +row.getAttribute('data-d');
      var doc = (state.doctors || [])[ix];
      if (!doc) return;
      defer(function () { selectDoctor(doc, ix); });
    });

    /* queue table → recall / remove (delegated, deferred) */
    on($('queueBody'), 'click', function (e) {
      e = e || window.event;
      var body = $('queueBody');
      var tgt = e.target || e.srcElement;
      var rep = findUp(tgt, 'data-q', body);
      if (rep) {
        var q = (state.queueView || [])[+rep.getAttribute('data-q')];
        if (q) defer(function () { fillPatient(q); toast('بیمار از صندوق بازخوانی شد', 'ok'); });
        return;
      }
      var del = findUp(tgt, 'data-qdel', body);
      if (del) {
        var qd = (state.queueView || [])[+del.getAttribute('data-qdel')];
        if (qd) defer(function () {
          Bridge.call('queue.remove', { id: qd.id }).then(function () { refreshQueue(); });
        });
      }
    });

    /* insurance changes → recompute (never blanks patient fields) */
    on($('insMain'), 'change', function () { scheduleRender(); });
    on($('insSupp'), 'change', function () { scheduleRender(); });
    on($('insSuppPct'), 'input', function () { scheduleRender(); });
    on($('insSuppPct'), 'keyup', function () { scheduleRender(); });
    on($('hasIns'), 'change', function () { scheduleRender(); });
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

  /* v1.50.0: idempotence guard — if any engine fires the ready event more than
     once (MSHTML readystatechange quirks), we must NEVER wire the delegated
     handlers twice (that would double-add services on every click). */
  var _wired = false;
  domReady(function () {
    if (_wired) return;
    _wired = true;
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
