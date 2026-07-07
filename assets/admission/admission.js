/* ============================================================================
   admission.js — Patient Admission UI logic (AZADI_TEB).
   Talks to C++ ONLY through Bridge (structured messages). No DOM scraping is
   used as the data mechanism; C++ is the single source of truth.
   ============================================================================ */
(function () {
  'use strict';

  const $ = (id) => document.getElementById(id);
  const state = {
    services: [],          // current admission service rows
    insurances: [],        // {name,pct} base list
    supp: [],              // supplementary list
    patient: null,
    catalog: []            // service catalog (from management)
  };

  // ---- Persian digit helpers ----
  const FA = '۰۱۲۳۴۵۶۷۸۹';
  const toFa = (n) => String(n).replace(/[0-9]/g, (d) => FA[+d]);
  const toEn = (s) => String(s || '').replace(/[۰-۹]/g, (d) => FA.indexOf(d));
  const money = (n) => toFa(Number(n || 0).toLocaleString('en-US'));

  function toast(msg, kind) {
    const t = $('toast');
    t.textContent = msg;
    t.className = 'toast show ' + (kind || '');
    clearTimeout(toast._t);
    toast._t = setTimeout(() => { t.className = 'toast'; }, 2600);
  }

  function setSync(kind, text) {
    const b = $('syncBadge');
    b.className = 'sync-badge ' + (kind || '');
    $('syncText').textContent = text;
  }

  // ======================================================= billing compute
  function recompute() {
    let total = 0, ins = 0, disc = 0;
    state.services.forEach((s) => {
      const line = (s.price || 0) * (s.qty || 1);
      total += line;
      ins += s.insShare || 0;
      disc += s.discount || 0;
    });
    const payable = total - ins - disc;
    $('invTotal').textContent = money(total);
    $('invIns').textContent = money(ins);
    $('invDisc').textContent = money(disc);
    $('invPayable').textContent = money(payable < 0 ? 0 : payable);
  }

  // ======================================================= service rows
  function insPct() {
    const idx = $('insMain').selectedIndex;
    return (state.insurances[idx] && state.insurances[idx].pct) || 0;
  }

  function addServiceRow(svc) {
    const pct = insPct();
    const price = svc.price || 0;
    const insShare = $('hasIns').checked ? Math.round(price * pct / 100) : 0;
    state.services.push({
      code: svc.code || '', name: svc.name || '', qty: 1,
      price: price, discount: 0,
      insShare: insShare, patShare: price - insShare
    });
    renderServices();
    recompute();
  }

  function renderServices() {
    const body = $('svcBody');
    body.innerHTML = '';
    if (!state.services.length) {
      body.innerHTML = '<tr><td colspan="7" class="empty">خدمتی اضافه نشده است</td></tr>';
      return;
    }
    state.services.forEach((s, i) => {
      const tr = document.createElement('tr');
      const line = (s.price || 0) * (s.qty || 1);
      const insLine = (s.insShare || 0) * (s.qty || 1);
      tr.innerHTML =
        '<td>' + toFa(s.code || '—') + '</td>' +
        '<td>' + (s.name || '') + '</td>' +
        '<td><input class="inp qty-inp" type="text" value="' + toFa(s.qty) + '" data-i="' + i + '"/></td>' +
        '<td>' + money(s.price) + '</td>' +
        '<td>' + money(insLine) + '</td>' +
        '<td>' + money(line - insLine) + '</td>' +
        '<td><button class="del-btn" data-del="' + i + '" title="حذف">✕</button></td>';
      body.appendChild(tr);
    });
    body.querySelectorAll('.qty-inp').forEach((el) => {
      el.addEventListener('change', (e) => {
        const i = +e.target.dataset.i;
        const q = Math.max(1, parseInt(toEn(e.target.value), 10) || 1);
        state.services[i].qty = q;
        renderServices(); recompute();
      });
    });
    body.querySelectorAll('.del-btn').forEach((el) => {
      el.addEventListener('click', (e) => {
        state.services.splice(+e.target.dataset.del, 1);
        renderServices(); recompute();
      });
    });
  }

  // ======================================================= populate patient
  function fillPatient(p) {
    if (!p) return;
    state.patient = p;
    $('nid').value = toFa(p.nid || '');
    $('first').value = p.first || '';
    $('last').value = p.last || '';
    $('father').value = p.father || '';
    $('birth').value = toFa(p.birth || '');
    $('mobile').value = toFa(p.mobile || '');
    if (p.gender) $('gender').value = p.gender;
    // profile card
    const full = ((p.first || '') + ' ' + (p.last || '')).trim();
    $('pfName').textContent = full || '— نامشخص —';
    $('pfNid').textContent = toFa(p.nid || '—');
    $('pfGender').textContent = p.gender || '—';
    $('pfBirth').textContent = toFa(p.birth || '—');
    $('pfMobile').textContent = toFa(p.mobile || '—');
    $('pfAvatar').textContent = (p.first || '؟').charAt(0);
    // insurances
    if (Array.isArray(p.insurances) && p.insurances.length) {
      $('insMain').selectedIndex = p.insurances[0];
    }
  }

  // ======================================================= lists rendering
  function renderPatientResults(rows) {
    const box = $('patResults');
    if (!rows || !rows.length) { box.innerHTML = '<div class="empty">نتیجه‌ای نیست</div>'; return; }
    box.innerHTML = '';
    rows.slice(0, 20).forEach((p) => {
      const d = document.createElement('div');
      d.className = 'row';
      d.innerHTML = '<span class="r-name">' + ((p.first || '') + ' ' + (p.last || '')).trim() +
        '</span><span class="r-sub">' + toFa(p.nid || '') + '</span>';
      d.addEventListener('click', () => fillPatient(p));
      box.appendChild(d);
    });
  }

  function renderDocResults(rows) {
    const box = $('docResults');
    if (!rows || !rows.length) { box.innerHTML = '<div class="empty">نتیجه‌ای نیست</div>'; return; }
    box.innerHTML = '';
    rows.slice(0, 20).forEach((doc) => {
      const d = document.createElement('div');
      d.className = 'row';
      d.innerHTML = '<span class="r-name">' + (doc.name || '') + '</span><span class="r-sub">' + (doc.specialty || '') + '</span>';
      d.addEventListener('click', () => {
        $('doc2name').value = doc.name || '';
        $('doc2code').value = toFa(doc.code || '');
      });
      box.appendChild(d);
    });
  }

  function renderSvcSuggest(rows) {
    const box = $('svcSuggest');
    if (!rows || !rows.length) { box.className = 'svc-suggest'; box.innerHTML = ''; return; }
    box.innerHTML = '';
    rows.slice(0, 30).forEach((s) => {
      const d = document.createElement('div');
      d.className = 's-row';
      d.innerHTML = '<span><b>' + (s.name || '') + '</b> <span class="s-code">' + toFa(s.code || '') + '</span></span>' +
        '<span class="s-price">' + money(s.price) + '</span>';
      d.addEventListener('click', () => {
        addServiceRow(s);
        box.className = 'svc-suggest'; $('svcSearch').value = '';
        toast('خدمت «' + s.name + '» افزوده شد', 'ok');
      });
      box.appendChild(d);
    });
    box.className = 'svc-suggest open';
  }

  function renderQueue(rows) {
    const body = $('queueBody');
    if (!rows || !rows.length) { body.innerHTML = '<tr><td colspan="4" class="empty">موردی در صف نیست</td></tr>'; return; }
    body.innerHTML = '';
    rows.slice(0, 50).forEach((q) => {
      const tr = document.createElement('tr');
      tr.innerHTML = '<td>' + (q.name || '') + '</td><td>' + toFa(q.nid || '') + '</td>' +
        '<td>' + (q.status || '') + '</td><td>' + money(q.amount) + '</td>';
      body.appendChild(tr);
    });
  }

  // ======================================================= wire events
  function wire() {
    // National ID Enter -> C++ lookup
    $('nid').addEventListener('keydown', (e) => {
      if (e.key === 'Enter') {
        e.preventDefault();
        const nid = toEn($('nid').value).trim();
        if (!nid) return;
        setSync('', 'در حال استعلام…');
        Bridge.call('patient.lookup', { nid: nid }).then((r) => {
          if (r && r.found) { fillPatient(r.patient || r); toast('اطلاعات بیمار بارگذاری شد', 'ok'); }
          else toast('بیماری با این کد ملی یافت نشد', 'err');
          setSync('ok', 'همگام');
        }).catch((err) => { toast('خطا: ' + err.message, 'err'); setSync('err', 'خطا'); });
      }
    });

    // local patient search
    const doPatSearch = () => {
      const q = $('patSearch').value.trim();
      Bridge.call('patient.search', { q: q }).then((r) => renderPatientResults(r.rows || r.patients));
    };
    $('patSearchBtn').addEventListener('click', doPatSearch);
    $('patSearch').addEventListener('keydown', (e) => { if (e.key === 'Enter') doPatSearch(); });

    // doctor search
    const doDocSearch = () => {
      Bridge.call('doctor.search', { q: $('docSearch').value.trim() }).then((r) => renderDocResults(r.rows || r.doctors));
    };
    $('docSearchBtn').addEventListener('click', doDocSearch);
    $('docSearch').addEventListener('keydown', (e) => { if (e.key === 'Enter') doDocSearch(); });

    // service search (live)
    let svcTimer = null;
    $('svcSearch').addEventListener('input', () => {
      clearTimeout(svcTimer);
      const q = $('svcSearch').value.trim();
      if (!q) { renderSvcSuggest([]); return; }
      svcTimer = setTimeout(() => {
        Bridge.call('service.search', { q: q }).then((r) => renderSvcSuggest(r.rows || r.services));
      }, 180);
    });
    $('svcAddBtn').addEventListener('click', () => {
      const q = $('svcSearch').value.trim();
      Bridge.call('service.search', { q: q }).then((r) => renderSvcSuggest(r.rows || r.services));
    });

    // recompute when insurance changes
    $('insMain').addEventListener('change', () => {
      // reapply insurance share to existing rows
      const pct = insPct();
      state.services.forEach((s) => {
        s.insShare = $('hasIns').checked ? Math.round(s.price * pct / 100) : 0;
        s.patShare = s.price - s.insShare;
      });
      renderServices(); recompute();
    });
    $('hasIns').addEventListener('change', () => $('insMain').dispatchEvent(new Event('change')));

    // collapsible lists
    $('svcToggle').addEventListener('click', () => {
      $('svcTblWrap').classList.toggle('expanded');
      Bridge.call('ui.toggleServices', {});
    });
    $('queueToggle').addEventListener('click', () => {
      $('queueWrap').classList.toggle('expanded');
      Bridge.call('ui.toggleQueue', {});
    });

    // save admission
    $('btnSave').addEventListener('click', () => {
      const rec = collectRecord();
      setSync('', 'در حال ثبت…');
      Bridge.call('admission.save', rec).then((r) => {
        if (r && r.ok) { toast('پذیرش با موفقیت ثبت شد', 'ok'); if (r.ps) updatePS(r.ps); }
        else toast('ثبت ناموفق: ' + ((r && r.err) || 'نامشخص'), 'err');
        setSync('ok', 'همگام');
      }).catch((err) => { toast('خطا: ' + err.message, 'err'); setSync('err', 'خطا'); });
    });

    // clear
    $('btnClear').addEventListener('click', () => { clearForm(); Bridge.call('admission.clear', {}); });
    $('btnNew').addEventListener('click', () => { clearForm(); Bridge.call('admission.new', {}); });

    // print buttons
    $('prtReceipt').addEventListener('click', () => Bridge.call('print.receipt', collectRecord()));
    $('prtIns').addEventListener('click', () => Bridge.call('print.insurance', collectRecord()));
    $('prtRx').addEventListener('click', () => Bridge.call('print.rx', collectRecord()));
    $('prtLast').addEventListener('click', () => Bridge.call('print.last', {}));
  }

  function collectRecord() {
    return {
      patient: {
        nid: toEn($('nid').value).trim(),
        first: $('first').value.trim(), last: $('last').value.trim(),
        father: $('father').value.trim(), gender: $('gender').value,
        birth: toEn($('birth').value).trim(), mobile: toEn($('mobile').value).trim(),
        phone: toEn($('phone').value).trim(), addr: $('addr').value.trim()
      },
      hasIns: $('hasIns').checked,
      insMain: $('insMain').selectedIndex,
      insSupp: $('insSupp').selectedIndex,
      insSuppPct: parseInt(toEn($('insSuppPct').value), 10) || 0,
      ptype: $('ptype').value, ntype: $('ntype').value,
      doc2code: toEn($('doc2code').value).trim(), doc2name: $('doc2name').value.trim(),
      perfcode: toEn($('perfcode').value).trim(), perfname: $('perfname').value.trim(),
      apptDate: toEn($('apptDate').value).trim(), apptShift: $('apptShift').value,
      rxDate: toEn($('rxDate').value).trim(),
      noPay: $('noPay').checked,
      services: state.services
    };
  }

  function clearForm() {
    ['nid', 'first', 'last', 'father', 'birth', 'mobile', 'phone', 'addr',
     'doc2code', 'doc2name', 'perfcode', 'perfname', 'apptDate', 'rxDate'].forEach((id) => { $(id).value = ''; });
    state.services = []; state.patient = null;
    $('pfName').textContent = '— بیماری انتخاب نشده —';
    ['pfNid', 'pfGender', 'pfBirth', 'pfMobile'].forEach((id) => $(id).textContent = '—');
    $('pfAvatar').textContent = '؟';
    renderServices(); recompute();
  }

  function updatePS(ps) {
    if (ps.P != null) $('psPVal').textContent = money(ps.P);
    if (ps.S != null) $('psSVal').textContent = money(ps.S);
  }

  // ======================================================= C++ -> JS events
  function subscribeEvents() {
    Bridge.on('patient.load', (d) => fillPatient(d));
    Bridge.on('services.update', (d) => { if (d.rows) { state.services = d.rows; renderServices(); recompute(); } });
    Bridge.on('queue.update', (d) => renderQueue(d.rows));
    Bridge.on('ps.update', (d) => updatePS(d));
    Bridge.on('insurance.update', (d) => {
      if (d.main) fillSelect($('insMain'), d.main);
      if (d.supp) fillSelect($('insSupp'), d.supp);
    });
  }

  function fillSelect(sel, arr) {
    const cur = sel.selectedIndex;
    sel.innerHTML = '';
    arr.forEach((o) => {
      const opt = document.createElement('option');
      opt.textContent = o.name; sel.appendChild(opt);
    });
    if (cur >= 0 && cur < arr.length) sel.selectedIndex = cur;
  }

  // ======================================================= boot
  function boot() {
    Bridge.call('init', {}).then((r) => {
      if (r.insurances) { state.insurances = r.insurances; fillSelect($('insMain'), r.insurances); }
      if (r.supp) { state.supp = r.supp; fillSelect($('insSupp'), r.supp); }
      if (r.patient) fillPatient(r.patient);
      if (r.services) { state.services = r.services; renderServices(); }
      renderServices(); recompute();
      setSync('ok', 'همگام با برنامه');
      toast('پذیرش بیمار آماده است', 'ok');
    }).catch((err) => {
      setSync('err', 'قطع ارتباط');
      // still allow the page to be usable with empty lists
      renderServices(); recompute();
      console.error('init failed', err);
    });
  }

  document.addEventListener('DOMContentLoaded', () => {
    wire();
    subscribeEvents();
    Bridge.ready(boot);
  });
})();
