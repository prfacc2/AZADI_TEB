/* ============================================================================
   patients.js — Patients (بیمار) management. ES5-only.
   Search / list / add / edit / delete patients via crm.patients.* verbs. The
   on-disk store (data\patients.dat) is owned by C++ (rememberPatient /
   deletePatient); the UI sends fields. Insurance lists come from crm.init.
   ============================================================================ */
(function (global) {
  'use strict';
  var Crm = global.Crm;

  function insOptions(selIdx, includeNone) {
    var list = (Crm.state.data && Crm.state.data.insurances) || [];
    var o = includeNone ? '<option value="-1">— بدون بیمه —</option>' : '';
    for (var i = 0; i < list.length; i++) {
      o += '<option value="' + list[i].idx + '"' + (list[i].idx === selIdx ? ' selected' : '') + '>' +
           Crm.esc(list[i].name) + '</option>';
    }
    return o;
  }
  function suppOptions(selIdx) {
    var list = (Crm.state.data && Crm.state.data.supp) || [];
    var o = '<option value="-1">— ندارد —</option>';
    for (var i = 0; i < list.length; i++) {
      o += '<option value="' + list[i].idx + '"' + (list[i].idx === selIdx ? ' selected' : '') + '>' +
           Crm.esc(list[i].name) + '</option>';
    }
    return o;
  }
  function insName(idx) {
    var list = (Crm.state.data && Crm.state.data.insurances) || [];
    for (var i = 0; i < list.length; i++) if (list[i].idx === idx) return list[i].name;
    return '—';
  }
  function suppName(idx) {
    var list = (Crm.state.data && Crm.state.data.supp) || [];
    for (var i = 0; i < list.length; i++) if (list[i].idx === idx) return list[i].name;
    return '—';
  }

  function load(host, q) {
    Crm.call('crm.patients.list', { q: q || '' }).then(function (d) {
      render(host, d.rows || [], q);
    }, function () {
      host.innerHTML = '';
      Crm.head(host, 'بیماران', 'جستجو و مدیریت پرونده بیماران');
      host.appendChild(Crm.el('div', 'crm-banner err', 'بارگذاری بیماران ناموفق بود.'));
    });
  }

  function render(host, rows, q) {
    host.innerHTML = '';
    Crm.head(host, 'بیماران', 'جستجو و مدیریت پرونده بیماران');

    var tb = Crm.el('div', 'crm-toolbar');
    var search = Crm.el('div', 'crm-search');
    search.innerHTML =
      '<input class="crm-input" id="ptQ" placeholder="جستجو با نام، کد ملی یا موبایل…" value="' + Crm.esc(q || '') + '" />' +
      '<span class="crm-search-ic"><svg viewBox="0 0 24 24" width="16" height="16"><path fill="currentColor" d="M10 2a8 8 0 105.3 14L20 20.7 21.7 19l-4.7-4.7A8 8 0 0010 2zm0 2a6 6 0 110 12 6 6 0 010-12z"/></svg></span>';
    tb.appendChild(search);
    tb.appendChild(Crm.el('div', 'spacer', ''));
    var addBtn = Crm.el('button', 'crm-btn primary', '+ افزودن بیمار');
    tb.appendChild(addBtn);
    host.appendChild(tb);

    host.appendChild(Crm.table([
      { key: 'i', label: 'ردیف', render: function (r, i) { return Crm.faDigits('' + (i + 1)); } },
      { key: 'nid', label: 'کد ملی', cls: 'c-mono', render: function (r) { return Crm.faDigits(Crm.esc(r.nid)); } },
      { key: 'name', label: 'نام و نام خانوادگی', render: function (r) { return '<b>' + Crm.esc(r.first + ' ' + r.last) + '</b>'; } },
      { key: 'father', label: 'نام پدر', render: function (r) { return Crm.esc(r.father || '—'); } },
      { key: 'mobile', label: 'موبایل', cls: 'c-num', render: function (r) { return Crm.faDigits(Crm.esc(r.mobile || '—')); } },
      { key: 'ins', label: 'بیمه', render: function (r) {
          var names = [];
          if (r.insurances && r.insurances.length) for (var i = 0; i < r.insurances.length; i++) names.push(insName(r.insurances[i]));
          return names.length ? Crm.esc(names.join('، ')) : '—';
        } },
      { key: 'ops', label: 'عملیات', render: function (r) {
          var b = Crm.el('span');
          b.innerHTML = '<button class="crm-row-btn" data-act="edit">ویرایش</button>' +
                        '<button class="crm-row-btn danger" data-act="del">حذف</button>';
          b.childNodes[0].onclick = function () { openModal(host, r); };
          b.childNodes[1].onclick = function () { del(host, r); };
          return b;
        } }
    ], rows));

    var qEl = Crm.$('ptQ');
    if (qEl) {
      qEl.onkeyup = function () {
        Crm.call('crm.patients.list', { q: qEl.value }).then(function (d) { render(host, d.rows || [], qEl.value); qEl.focus(); });
      };
      if (q) { var v = qEl.value; qEl.value = ''; qEl.value = v; }
    }
    addBtn.onclick = function () { openModal(host, null); };
  }

  function openModal(host, p) {
    var adding = !p;
    var m = Crm.modal(adding ? 'افزودن بیمار' : 'ویرایش بیمار', null);
    var body = m.body;
    var ins = (p && p.insurances && p.insurances.length) ? p.insurances[0] : -1;
    body.innerHTML =
      '<div class="crm-form">' +
      '<div class="crm-field"><label class="crm-label">کد ملی</label>' +
        '<input class="crm-input" id="pNid" value="' + Crm.faDigits(Crm.esc(p ? p.nid : '')) + '" placeholder="کد ملی ۱۰ رقمی" /></div>' +
      '<div class="crm-field"><label class="crm-label">نام</label>' +
        '<input class="crm-input" id="pFirst" value="' + Crm.esc(p ? p.first : '') + '" /></div>' +
      '<div class="crm-field"><label class="crm-label">نام خانوادگی</label>' +
        '<input class="crm-input" id="pLast" value="' + Crm.esc(p ? p.last : '') + '" /></div>' +
      '<div class="crm-field"><label class="crm-label">نام پدر</label>' +
        '<input class="crm-input" id="pFather" value="' + Crm.esc(p ? p.father : '') + '" /></div>' +
      '<div class="crm-field"><label class="crm-label">جنسیت</label>' +
        '<select class="crm-select" id="pGender"><option value="مرد"' + (p && p.gender === 'مرد' ? ' selected' : '') + '>مرد</option>' +
        '<option value="زن"' + (p && p.gender === 'زن' ? ' selected' : '') + '>زن</option></select></div>' +
      '<div class="crm-field"><label class="crm-label">تاریخ تولد</label>' +
        '<input class="crm-input" id="pBirth" value="' + Crm.esc(p ? p.birth : '') + '" placeholder="مثال: 1370/05/12" /></div>' +
      '<div class="crm-field"><label class="crm-label">موبایل</label>' +
        '<input class="crm-input" id="pMobile" value="' + Crm.faDigits(Crm.esc(p ? p.mobile : '')) + '" /></div>' +
      '<div class="crm-field"><label class="crm-label">تلفن ثابت</label>' +
        '<input class="crm-input" id="pPhone" value="' + Crm.faDigits(Crm.esc(p ? p.phone : '')) + '" /></div>' +
      '<div class="crm-field"><label class="crm-label">بیمه پایه</label>' +
        '<select class="crm-select" id="pIns">' + insOptions(ins, true) + '</select></div>' +
      '<div class="crm-field"><label class="crm-label">بیمه تکمیلی</label>' +
        '<select class="crm-select" id="pSupp">' + suppOptions(p ? p.suppIdx : -1) + '</select></div>' +
      '<div class="crm-field full"><label class="crm-label">آدرس</label>' +
        '<textarea class="crm-textarea" id="pAddr">' + Crm.esc(p ? p.addr : '') + '</textarea></div>' +
      '</div>';
    var foot = Crm.el('div', 'crm-modal-foot');
    foot.innerHTML = '<button class="crm-btn ghost" id="mCancel">انصراف</button><button class="crm-btn primary" id="mSave">ذخیره</button>';
    m.card.appendChild(foot);
    Crm.$('mCancel').onclick = m.close;
    Crm.$('mSave').onclick = function () {
      var insIdx = +Crm.enDigits(Crm.$('pIns').value);
      var payload = {
        nid: Crm.enDigits(Crm.$('pNid').value),
        first: Crm.$('pFirst').value,
        last: Crm.$('pLast').value,
        father: Crm.$('pFather').value,
        gender: Crm.$('pGender').value,
        birth: Crm.$('pBirth').value,
        mobile: Crm.enDigits(Crm.$('pMobile').value),
        phone: Crm.enDigits(Crm.$('pPhone').value),
        addr: Crm.$('pAddr').value,
        insurances: (insIdx >= 0) ? [insIdx] : [],
        suppIdx: +Crm.enDigits(Crm.$('pSupp').value)
      };
      if (!payload.nid) { Crm.toast('کد ملی الزامی است.', 'err'); return; }
      Crm.call('crm.patients.save', payload).then(function () {
        Crm.toast(adding ? 'بیمار اضافه شد.' : 'بیمار ویرایش شد.', 'ok');
        m.close(); load(host, '');
      }, function () { Crm.toast('ذخیره ناموفق بود.', 'err'); });
    };
  }

  function del(host, p) {
    Crm.confirm('حذف پرونده بیمار «' + p.first + ' ' + p.last + '» (' + p.nid + ')؟', function () {
      Crm.call('crm.patients.delete', { nid: p.nid }).then(function () {
        Crm.toast('بیمار حذف شد.', 'ok'); load(host, '');
      }, function () { Crm.toast('حذف ناموفق بود.', 'err'); });
    }, { danger: true });
  }

  Crm.pages.patients = {
    title: 'بیماران',
    render: function (host) { load(host, ''); }
  };
})(window);
