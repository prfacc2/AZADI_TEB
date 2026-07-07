/* ============================================================================
   admission.js — Patient Admission UI logic (AZADI_TEB).

   ES5-only syntax (no arrow functions / const / let / template literals / Map /
   spread / destructuring) so it runs on BOTH the WebView2 (Chromium) engine and
   the universal MSHTML/Trident fallback without any "Syntax error".

   Talks to C++ ONLY through Bridge (structured messages). No DOM scraping is
   used as the data mechanism; C++ is the single source of truth.
   ============================================================================ */
(function () {
  'use strict';

  function $(id) { return document.getElementById(id); }

  var state = {
    services: [],          /* current admission service rows */
    insurances: [],        /* {name,pct} base list */
    supp: [],              /* supplementary list */
    patient: null,
    catalog: []            /* service catalog (from management) */
  };

  /* ---- Persian digit helpers ---- */
  var FA = '۰۱۲۳۴۵۶۷۸۹';
  function toFa(n) {
    return String(n).replace(/[0-9]/g, function (d) { return FA.charAt(+d); });
  }
  function toEn(s) {
    return String(s == null ? '' : s).replace(/[۰-۹]/g, function (d) {
      return String(FA.indexOf(d));
    });
  }
  function money(n) {
    var v = Number(n || 0);
    /* group thousands (ES5-safe, avoid locale quirks on MSHTML) */
    var str = String(Math.round(v));
    var neg = str.charAt(0) === '-';
    if (neg) str = str.substr(1);
    var out = '', c = 0, i;
    for (i = str.length - 1; i >= 0; i--) {
      out = str.charAt(i) + out;
      if (++c % 3 === 0 && i > 0) out = ',' + out;
    }
    return toFa((neg ? '-' : '') + out);
  }

  function toast(msg, kind) {
    var t = $('toast');
    if (!t) return;
    t.innerHTML = '';
    t.appendChild(document.createTextNode(msg));
    t.className = 'toast show ' + (kind || '');
    if (toast._t) clearTimeout(toast._t);
    toast._t = setTimeout(function () { t.className = 'toast'; }, 2600);
  }

  function setSync(kind, text) {
    var b = $('syncBadge');
    if (b) b.className = 'sync-badge ' + (kind || '');
    var st = $('syncText');
    if (st) { st.innerHTML = ''; st.appendChild(document.createTextNode(text)); }
  }

  function setText(el, text) {
    if (!el) return;
    el.innerHTML = '';
    el.appendChild(document.createTextNode(text == null ? '' : String(text)));
  }

  function esc(s) {
    s = (s == null ? '' : String(s));
    return s.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;')
            .replace(/"/g, '&quot;');
  }

  /* ======================================================= billing compute */
  function recompute() {
    var total = 0, ins = 0, disc = 0, i, s, line;
    for (i = 0; i < state.services.length; i++) {
      s = state.services[i];
      line = (s.price || 0) * (s.qty || 1);
      total += line;
      ins += (s.insShare || 0) * (s.qty || 1);
      disc += s.discount || 0;
    }
    var payable = total - ins - disc;
    setText($('invTotal'), money(total));
    setText($('invIns'), money(ins));
    setText($('invDisc'), money(disc));
    setText($('invPayable'), money(payable < 0 ? 0 : payable));
  }

  /* ======================================================= service rows */
  function insPct() {
    var sel = $('insMain');
    var idx = sel ? sel.selectedIndex : -1;
    return (state.insurances[idx] && state.insurances[idx].pct) || 0;
  }

  function addServiceRow(svc) {
    var pct = insPct();
    var price = svc.price || 0;
    var hasIns = $('hasIns') && $('hasIns').checked;
    var insShare = hasIns ? Math.round(price * pct / 100) : 0;
    state.services.push({
      code: svc.code || '', name: svc.name || '', qty: 1,
      price: price, discount: 0,
      insShare: insShare, patShare: price - insShare
    });
    renderServices();
    recompute();
  }

  function renderServices() {
    var body = $('svcBody');
    if (!body) return;
    if (!state.services.length) {
      body.innerHTML = '<tr><td colspan="7" class="empty">خدمتی اضافه نشده است</td></tr>';
      return;
    }
    var html = '', i, s, line, insLine;
    for (i = 0; i < state.services.length; i++) {
      s = state.services[i];
      line = (s.price || 0) * (s.qty || 1);
      insLine = (s.insShare || 0) * (s.qty || 1);
      html +=
        '<tr>' +
        '<td>' + toFa(s.code || '—') + '</td>' +
        '<td>' + esc(s.name || '') + '</td>' +
        '<td><input class="inp qty-inp" type="text" value="' + toFa(s.qty) + '" data-i="' + i + '"/></td>' +
        '<td>' + money(s.price) + '</td>' +
        '<td>' + money(insLine) + '</td>' +
        '<td>' + money(line - insLine) + '</td>' +
        '<td><button class="del-btn" data-del="' + i + '" title="حذف">✕</button></td>' +
        '</tr>';
    }
    body.innerHTML = html;
    var qtys = body.getElementsByClassName('qty-inp');
    for (i = 0; i < qtys.length; i++) {
      qtys[i].onchange = function (e) {
        e = e || window.event;
        var tgt = e.target || e.srcElement;
        var idx = +tgt.getAttribute('data-i');
        var q = Math.max(1, parseInt(toEn(tgt.value), 10) || 1);
        state.services[idx].qty = q;
        renderServices(); recompute();
      };
    }
    var dels = body.getElementsByClassName('del-btn');
    for (i = 0; i < dels.length; i++) {
      dels[i].onclick = function (e) {
        e = e || window.event;
        var tgt = e.target || e.srcElement;
        state.services.splice(+tgt.getAttribute('data-del'), 1);
        renderServices(); recompute();
      };
    }
  }

  /* ======================================================= populate patient */
  function fillPatient(p) {
    if (!p) return;
    state.patient = p;
    if ($('nid')) $('nid').value = toFa(p.nid || '');
    if ($('first')) $('first').value = p.first || '';
    if ($('last')) $('last').value = p.last || '';
    if ($('father')) $('father').value = p.father || '';
    if ($('birth')) $('birth').value = toFa(p.birth || '');
    if ($('mobile')) $('mobile').value = toFa(p.mobile || '');
    if ($('phone') && p.phone) $('phone').value = toFa(p.phone);
    if ($('addr') && p.addr) $('addr').value = p.addr;
    if (p.gender && $('gender')) $('gender').value = p.gender;
    var full = ((p.first || '') + ' ' + (p.last || '')).replace(/^\s+|\s+$/g, '');
    setText($('pfName'), full || '— نامشخص —');
    setText($('pfNid'), toFa(p.nid || '—'));
    setText($('pfGender'), p.gender || '—');
    setText($('pfBirth'), toFa(p.birth || '—'));
    setText($('pfMobile'), toFa(p.mobile || '—'));
    setText($('pfAvatar'), (p.first || '؟').charAt(0));
    if (p.insurances && p.insurances.length && $('insMain')) {
      $('insMain').selectedIndex = p.insurances[0];
    }
  }

  /* ======================================================= lists rendering */
  function renderPatientResults(rows) {
    var box = $('patResults');
    if (!box) return;
    if (!rows || !rows.length) { box.innerHTML = '<div class="empty">نتیجه‌ای نیست</div>'; return; }
    box.innerHTML = '';
    var i, lim = Math.min(rows.length, 20);
    for (i = 0; i < lim; i++) {
      (function (p) {
        var d = document.createElement('div');
        d.className = 'row';
        d.innerHTML = '<span class="r-name">' +
          esc(((p.first || '') + ' ' + (p.last || '')).replace(/^\s+|\s+$/g, '')) +
          '</span><span class="r-sub">' + toFa(p.nid || '') + '</span>';
        d.onclick = function () { fillPatient(p); };
        box.appendChild(d);
      })(rows[i]);
    }
  }

  function renderDocResults(rows) {
    var box = $('docResults');
    if (!box) return;
    if (!rows || !rows.length) { box.innerHTML = '<div class="empty">نتیجه‌ای نیست</div>'; return; }
    box.innerHTML = '';
    var i, lim = Math.min(rows.length, 20);
    for (i = 0; i < lim; i++) {
      (function (doc) {
        var d = document.createElement('div');
        d.className = 'row';
        d.innerHTML = '<span class="r-name">' + esc(doc.name || '') +
          '</span><span class="r-sub">' + esc(doc.specialty || '') + '</span>';
        d.onclick = function () {
          if ($('doc2name')) $('doc2name').value = doc.name || '';
          if ($('doc2code')) $('doc2code').value = toFa(doc.code || '');
        };
        box.appendChild(d);
      })(rows[i]);
    }
  }

  function renderSvcSuggest(rows) {
    var box = $('svcSuggest');
    if (!box) return;
    if (!rows || !rows.length) { box.className = 'svc-suggest'; box.innerHTML = ''; return; }
    box.innerHTML = '';
    var i, lim = Math.min(rows.length, 30);
    for (i = 0; i < lim; i++) {
      (function (s) {
        var d = document.createElement('div');
        d.className = 's-row';
        d.innerHTML = '<span><b>' + esc(s.name || '') + '</b> <span class="s-code">' +
          toFa(s.code || '') + '</span></span>' +
          '<span class="s-price">' + money(s.price) + '</span>';
        d.onclick = function () {
          addServiceRow(s);
          box.className = 'svc-suggest';
          if ($('svcSearch')) $('svcSearch').value = '';
          toast('خدمت «' + s.name + '» افزوده شد', 'ok');
        };
        box.appendChild(d);
      })(rows[i]);
    }
    box.className = 'svc-suggest open';
  }

  function renderQueue(rows) {
    var body = $('queueBody');
    if (!body) return;
    if (!rows || !rows.length) {
      body.innerHTML = '<tr><td colspan="4" class="empty">موردی در صف نیست</td></tr>';
      return;
    }
    var html = '', i, q, lim = Math.min(rows.length, 50);
    for (i = 0; i < lim; i++) {
      q = rows[i];
      html += '<tr><td>' + esc(q.name || '') + '</td><td>' + toFa(q.nid || '') + '</td>' +
        '<td>' + esc(q.status || '') + '</td><td>' + money(q.amount) + '</td></tr>';
    }
    body.innerHTML = html;
  }

  /* ======================================================= wire events */
  function on(el, ev, fn) { if (el) { if (el.addEventListener) el.addEventListener(ev, fn, false); else el.attachEvent('on' + ev, fn); } }

  function wire() {
    /* National ID Enter -> C++ lookup */
    on($('nid'), 'keydown', function (e) {
      e = e || window.event;
      if (e.keyCode === 13 || e.key === 'Enter') {
        if (e.preventDefault) e.preventDefault();
        var nid = toEn($('nid').value).replace(/^\s+|\s+$/g, '');
        if (!nid) return;
        setSync('', 'در حال استعلام…');
        Bridge.call('patient.lookup', { nid: nid }).then(function (r) {
          if (r && r.found) { fillPatient(r.patient || r); toast('اطلاعات بیمار بارگذاری شد', 'ok'); }
          else toast('بیماری با این کد ملی یافت نشد', 'err');
          setSync('ok', 'همگام');
        }).catch(function (err) { toast('خطا: ' + err.message, 'err'); setSync('err', 'خطا'); });
      }
    });

    /* local patient search */
    function doPatSearch() {
      var q = $('patSearch') ? $('patSearch').value.replace(/^\s+|\s+$/g, '') : '';
      Bridge.call('patient.search', { q: q }).then(function (r) {
        renderPatientResults(r.rows || r.patients);
      });
    }
    on($('patSearchBtn'), 'click', doPatSearch);
    on($('patSearch'), 'keydown', function (e) { e = e || window.event; if (e.keyCode === 13) doPatSearch(); });

    /* doctor search */
    function doDocSearch() {
      var q = $('docSearch') ? $('docSearch').value.replace(/^\s+|\s+$/g, '') : '';
      Bridge.call('doctor.search', { q: q }).then(function (r) {
        renderDocResults(r.rows || r.doctors);
      });
    }
    on($('docSearchBtn'), 'click', doDocSearch);
    on($('docSearch'), 'keydown', function (e) { e = e || window.event; if (e.keyCode === 13) doDocSearch(); });

    /* service search (live) */
    var svcTimer = null;
    on($('svcSearch'), 'input', function () {
      if (svcTimer) clearTimeout(svcTimer);
      var q = $('svcSearch').value.replace(/^\s+|\s+$/g, '');
      if (!q) { renderSvcSuggest([]); return; }
      svcTimer = setTimeout(function () {
        Bridge.call('service.search', { q: q }).then(function (r) {
          renderSvcSuggest(r.rows || r.services);
        });
      }, 200);
    });
    /* MSHTML: 'input' may not fire — also react to keyup */
    on($('svcSearch'), 'keyup', function () {
      if (svcTimer) clearTimeout(svcTimer);
      var q = $('svcSearch').value.replace(/^\s+|\s+$/g, '');
      if (!q) { renderSvcSuggest([]); return; }
      svcTimer = setTimeout(function () {
        Bridge.call('service.search', { q: q }).then(function (r) {
          renderSvcSuggest(r.rows || r.services);
        });
      }, 220);
    });
    on($('svcAddBtn'), 'click', function () {
      var q = $('svcSearch') ? $('svcSearch').value.replace(/^\s+|\s+$/g, '') : '';
      Bridge.call('service.search', { q: q }).then(function (r) {
        renderSvcSuggest(r.rows || r.services);
      });
    });

    /* recompute when insurance changes */
    on($('insMain'), 'change', function () {
      var pct = insPct();
      var hasIns = $('hasIns') && $('hasIns').checked;
      var i, s;
      for (i = 0; i < state.services.length; i++) {
        s = state.services[i];
        s.insShare = hasIns ? Math.round(s.price * pct / 100) : 0;
        s.patShare = s.price - s.insShare;
      }
      renderServices(); recompute();
    });
    on($('hasIns'), 'change', function () {
      var sel = $('insMain');
      if (!sel) return;
      if (sel.fireEvent) sel.fireEvent('onchange');
      else { var ev = document.createEvent('HTMLEvents'); ev.initEvent('change', true, false); sel.dispatchEvent(ev); }
    });

    /* collapsible lists */
    on($('svcToggle'), 'click', function () {
      var w = $('svcTblWrap');
      if (w) { if (/expanded/.test(w.className)) w.className = w.className.replace(/\s*expanded/, ''); else w.className += ' expanded'; }
      Bridge.call('ui.toggleServices', {});
    });
    on($('queueToggle'), 'click', function () {
      var w = $('queueWrap');
      if (w) { if (/expanded/.test(w.className)) w.className = w.className.replace(/\s*expanded/, ''); else w.className += ' expanded'; }
      Bridge.call('ui.toggleQueue', {});
    });

    /* save admission */
    on($('btnSave'), 'click', function () {
      var rec = collectRecord();
      setSync('', 'در حال ثبت…');
      Bridge.call('admission.save', rec).then(function (r) {
        if (r && r.ok) { toast('پذیرش با موفقیت ثبت شد', 'ok'); if (r.ps) updatePS(r.ps); }
        else toast('ثبت ناموفق: ' + ((r && r.err) || 'نامشخص'), 'err');
        setSync('ok', 'همگام');
      }).catch(function (err) { toast('خطا: ' + err.message, 'err'); setSync('err', 'خطا'); });
    });

    on($('btnClear'), 'click', function () { clearForm(); Bridge.call('admission.clear', {}); });
    on($('btnNew'), 'click', function () { clearForm(); Bridge.call('admission.new', {}); });

    on($('prtReceipt'), 'click', function () { Bridge.call('print.receipt', collectRecord()); });
    on($('prtIns'), 'click', function () { Bridge.call('print.insurance', collectRecord()); });
    on($('prtRx'), 'click', function () { Bridge.call('print.rx', collectRecord()); });
    on($('prtLast'), 'click', function () { Bridge.call('print.last', {}); });
  }

  function val(id) { var e = $(id); return e ? e.value : ''; }
  function trimEn(id) { return toEn(val(id)).replace(/^\s+|\s+$/g, ''); }
  function trim(id) { return String(val(id)).replace(/^\s+|\s+$/g, ''); }

  function collectRecord() {
    return {
      patient: {
        nid: trimEn('nid'),
        first: trim('first'), last: trim('last'),
        father: trim('father'), gender: val('gender'),
        birth: trimEn('birth'), mobile: trimEn('mobile'),
        phone: trimEn('phone'), addr: trim('addr')
      },
      hasIns: $('hasIns') ? $('hasIns').checked : false,
      insMain: $('insMain') ? $('insMain').selectedIndex : -1,
      insSupp: $('insSupp') ? $('insSupp').selectedIndex : -1,
      insSuppPct: parseInt(trimEn('insSuppPct'), 10) || 0,
      ptype: val('ptype'), ntype: val('ntype'),
      doc2code: trimEn('doc2code'), doc2name: trim('doc2name'),
      perfcode: trimEn('perfcode'), perfname: trim('perfname'),
      apptDate: trimEn('apptDate'), apptShift: val('apptShift'),
      rxDate: trimEn('rxDate'),
      noPay: $('noPay') ? $('noPay').checked : false,
      services: state.services
    };
  }

  function clearForm() {
    var ids = ['nid', 'first', 'last', 'father', 'birth', 'mobile', 'phone', 'addr',
      'doc2code', 'doc2name', 'perfcode', 'perfname', 'apptDate', 'rxDate'];
    var i;
    for (i = 0; i < ids.length; i++) { if ($(ids[i])) $(ids[i]).value = ''; }
    state.services = []; state.patient = null;
    setText($('pfName'), '— بیماری انتخاب نشده —');
    var pf = ['pfNid', 'pfGender', 'pfBirth', 'pfMobile'];
    for (i = 0; i < pf.length; i++) setText($(pf[i]), '—');
    setText($('pfAvatar'), '؟');
    renderServices(); recompute();
  }

  function updatePS(ps) {
    if (ps.P != null) setText($('psPVal'), money(ps.P));
    if (ps.S != null) setText($('psSVal'), money(ps.S));
  }

  /* ======================================================= C++ -> JS events */
  function subscribeEvents() {
    Bridge.on('patient.load', function (d) { fillPatient(d); });
    Bridge.on('services.update', function (d) {
      if (d.rows) { state.services = d.rows; renderServices(); recompute(); }
    });
    Bridge.on('queue.update', function (d) { renderQueue(d.rows); });
    Bridge.on('ps.update', function (d) { updatePS(d); });
    Bridge.on('insurance.update', function (d) {
      if (d.main) fillSelect($('insMain'), d.main);
      if (d.supp) fillSelect($('insSupp'), d.supp);
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
    if (cur >= 0 && cur < arr.length) sel.selectedIndex = cur;
  }

  /* ======================================================= boot */
  function boot() {
    Bridge.call('init', {}).then(function (r) {
      if (r.insurances) { state.insurances = r.insurances; fillSelect($('insMain'), r.insurances); }
      if (r.supp) { state.supp = r.supp; fillSelect($('insSupp'), r.supp); }
      if (r.patient) fillPatient(r.patient);
      if (r.services) { state.services = r.services; }
      renderServices(); recompute();
      setSync('ok', 'همگام با برنامه');
      toast('پذیرش بیمار آماده است', 'ok');
    }).catch(function (err) {
      setSync('err', 'قطع ارتباط');
      renderServices(); recompute();
      if (window.console) console.error('init failed', err);
    });
  }

  function domReady(fn) {
    if (document.readyState === 'complete' || document.readyState === 'interactive') {
      setTimeout(fn, 1);
    } else if (document.addEventListener) {
      document.addEventListener('DOMContentLoaded', fn, false);
    } else {
      document.attachEvent('onreadystatechange', function () {
        if (document.readyState === 'complete') fn();
      });
    }
  }

  domReady(function () {
    wire();
    subscribeEvents();
    Bridge.ready(boot);
  });
})();
