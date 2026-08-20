/* ============================================================================
   doctors.js — Doctors/Nurses (پزشک/پرستار) management. ES5-only.
   List / search / add / edit / delete with ALL DoctorDef fields (medical ID,
   specialty, contract, accounting, …). Uses index-based identity (idx) since
   the legacy doctors store has no stable id. The on-disk store (data\doctors.dat)
   is owned by C++ (saveDoctors); the UI sends one record + idx.
   ============================================================================ */
(function (global) {
  'use strict';
  var Crm = global.Crm;

  var CONTRACTS = [
    { v: 0, l: 'ساعتی' },
    { v: 1, l: 'قراردادی' },
    { v: 2, l: 'پورسانت' },
    { v: 3, l: 'سایر' }
  ];

  function load(host, q) {
    Crm.call('crm.doctors.list', { q: q || '' }).then(function (d) {
      render(host, d.rows || [], q);
    }, function () {
      host.innerHTML = '';
      Crm.head(host, 'پزشکان و پرستاران', 'مدیریت نیروی درمانی و قراردادها');
      host.appendChild(Crm.el('div', 'crm-banner err', 'بارگذاری پزشکان ناموفق بود.'));
    });
  }

  function render(host, rows, q) {
    host.innerHTML = '';
    Crm.head(host, 'پزشکان و پرستاران', 'مدیریت نیروی درمانی و قراردادها');

    var tb = Crm.el('div', 'crm-toolbar');
    var search = Crm.el('div', 'crm-search');
    search.innerHTML =
      '<input class="crm-input" id="dcQ" placeholder="جستجو با نام، تخصص یا کد نظام پزشکی…" value="' + Crm.esc(q || '') + '" />' +
      '<span class="crm-search-ic"><svg viewBox="0 0 24 24" width="16" height="16"><path fill="currentColor" d="M10 2a8 8 0 105.3 14L20 20.7 21.7 19l-4.7-4.7A8 8 0 0010 2zm0 2a6 6 0 110 12 6 6 0 010-12z"/></svg></span>';
    tb.appendChild(search);
    tb.appendChild(Crm.el('div', 'spacer', ''));
    var addBtn = Crm.el('button', 'crm-btn primary', '+ افزودن پزشک/پرستار');
    tb.appendChild(addBtn);
    host.appendChild(tb);

    host.appendChild(Crm.table([
      { key: 'i', label: 'ردیف', render: function (r) { return Crm.faDigits('' + (r.idx + 1)); } },
      { key: 'name', label: 'نام', render: function (r) { return '<b>' + Crm.esc(r.name) + '</b>'; } },
      { key: 'specialty', label: 'تخصص', render: function (r) { return Crm.esc(r.specialty || '—'); } },
      { key: 'docType', label: 'نوع', render: function (r) { return Crm.esc(r.docType === 1 ? 'پرستار' : 'پزشک'); } },
      { key: 'medicalId', label: 'کد نظام پزشکی', cls: 'c-mono', render: function (r) { return Crm.faDigits(Crm.esc(r.medicalId || '—')); } },
      { key: 'active', label: 'وضعیت', render: function (r) { return Crm.pill(r.active ? 'فعال' : 'غیرفعال', r.active ? 'on' : 'off'); } },
      { key: 'ops', label: 'عملیات', render: function (r) {
          var b = Crm.el('span');
          b.innerHTML = '<button class="crm-row-btn" data-act="edit">ویرایش</button>' +
                        '<button class="crm-row-btn danger" data-act="del">حذف</button>';
          b.childNodes[0].onclick = function () { openModal(host, r); };
          b.childNodes[1].onclick = function () { del(host, r); };
          return b;
        } }
    ], rows));

    var qEl = Crm.$('dcQ');
    if (qEl) {
      qEl.onkeyup = function () {
        Crm.call('crm.doctors.list', { q: qEl.value }).then(function (d) { render(host, d.rows || [], qEl.value); qEl.focus(); });
      };
      if (q) { var v = qEl.value; qEl.value = ''; qEl.value = v; }
    }
    addBtn.onclick = function () { openModal(host, null); };
  }

  function contractOptions(sel) {
    var o = '';
    for (var i = 0; i < CONTRACTS.length; i++) {
      o += '<option value="' + CONTRACTS[i].v + '"' + (CONTRACTS[i].v === sel ? ' selected' : '') + '>' + Crm.esc(CONTRACTS[i].l) + '</option>';
    }
    return o;
  }

  function field(id, label, val, placeholder, full) {
    return '<div class="crm-field' + (full ? ' full' : '') + '"><label class="crm-label">' + Crm.esc(label) + '</label>' +
           '<input class="crm-input" id="' + id + '" value="' + Crm.esc(val || '') + '" placeholder="' + Crm.esc(placeholder || '') + '" /></div>';
  }

  function openModal(host, d) {
    var adding = !d;
    if (!d) d = { docType: 0, active: true, printOnReceipt: true, contractType: 0, services: [] };
    var m = Crm.modal(adding ? 'افزودن پزشک/پرستار' : 'ویرایش پزشک/پرستار', null);
    var body = m.body;
    var svc = (d.services || []).join('، ');
    body.innerHTML =
      '<div class="crm-form">' +
      '<input type="hidden" id="dIdx" value="' + (d.idx != null ? d.idx : -1) + '" />' +
      field('dName', 'نام نمایشی', d.name, 'مثال: دکتر علی رضایی') +
      '<div class="crm-field"><label class="crm-label">نوع نیرو</label>' +
        '<select class="crm-select" id="dDocType"><option value="0"' + (d.docType === 0 ? ' selected' : '') + '>پزشک</option>' +
        '<option value="1"' + (d.docType === 1 ? ' selected' : '') + '>پرستار</option></select></div>' +
      field('dPrefix', 'پیشوند نام', d.namePrefix, 'دکتر / پزشک') +
      field('dFirst', 'نام', d.firstName, '') +
      field('dLast', 'نام خانوادگی', d.lastName, '') +
      field('dSpec', 'تخصص', d.specialty, 'متخصص داخلی') +
      field('dInsSpec', 'تخصص بیمه', d.insSpecialty, '') +
      field('dMedId', 'کد نظام پزشکی', d.medicalId, '') +
      field('dDocCode', 'کد پزشک', d.docCode, '') +
      field('dDegree', 'مدرک تحصیلی', d.degree, '') +
      field('dNid', 'کد ملی', d.nationalId, '') +
      field('dMobile', 'موبایل', d.mobile, '') +
      field('dEmail', 'ایمیل', d.email, '') +
      field('dDept', 'بخش', d.deptId, 'کد یا نام بخش') +
      '<div class="crm-field"><label class="crm-label">نوع قرارداد</label>' +
        '<select class="crm-select" id="dContract">' + contractOptions(d.contractType || 0) + '</select></div>' +
      field('dFranchise', 'فرانشیز', d.franchise, '') +
      field('dEmerg', 'قرار پزشک اورژانس', d.emergencyContract, '', true) +
      field('dAccounting', 'حسابداری', d.accounting, '', true) +
      field('dServices', 'خدمات (با ویرگول جدا کنید)', svc, 'ویزیت، نوار، ...', true) +
      field('dAddress', 'آدرس', d.address, '', true) +
      '<div class="crm-field"><label class="crm-check"><input type="checkbox" id="dActive" ' + (d.active ? 'checked' : '') + ' />فعال</label></div>' +
      '<div class="crm-field"><label class="crm-check"><input type="checkbox" id="dPrint" ' + (d.printOnReceipt ? 'checked' : '') + ' />چاپ در قبض</label></div>' +
      '</div>';
    var foot = Crm.el('div', 'crm-modal-foot');
    foot.innerHTML = '<button class="crm-btn ghost" id="mCancel">انصراف</button><button class="crm-btn primary" id="mSave">ذخیره</button>';
    m.card.appendChild(foot);
    Crm.$('mCancel').onclick = m.close;
    Crm.$('mSave').onclick = function () {
      var services = [];
      var raw = Crm.$('dServices').value;
      if (raw) {
        var parts = raw.split(/[،,؛;]+/);
        for (var i = 0; i < parts.length; i++) { var s = parts[i].replace(/^\s+|\s+$/g, ''); if (s) services.push(s); }
      }
      var payload = {
        idx: +Crm.enDigits(Crm.$('dIdx').value),
        name: Crm.$('dName').value,
        docType: +Crm.$('dDocType').value,
        namePrefix: Crm.$('dPrefix').value,
        firstName: Crm.$('dFirst').value,
        lastName: Crm.$('dLast').value,
        specialty: Crm.$('dSpec').value,
        insSpecialty: Crm.$('dInsSpec').value,
        medicalId: Crm.enDigits(Crm.$('dMedId').value),
        docCode: Crm.enDigits(Crm.$('dDocCode').value),
        degree: Crm.$('dDegree').value,
        nationalId: Crm.enDigits(Crm.$('dNid').value),
        mobile: Crm.enDigits(Crm.$('dMobile').value),
        email: Crm.$('dEmail').value,
        deptId: Crm.$('dDept').value,
        contractType: +Crm.$('dContract').value,
        franchise: Crm.$('dFranchise').value,
        emergencyContract: Crm.$('dEmerg').value,
        accounting: Crm.$('dAccounting').value,
        address: Crm.$('dAddress').value,
        services: services,
        active: Crm.$('dActive').checked,
        printOnReceipt: Crm.$('dPrint').checked
      };
      if (!payload.name) { Crm.toast('نام نمایشی الزامی است.', 'err'); return; }
      Crm.call('crm.doctors.save', payload).then(function () {
        Crm.toast(adding ? 'پزشک اضافه شد.' : 'پزشک ویرایش شد.', 'ok');
        m.close(); load(host, '');
      }, function () { Crm.toast('ذخیره ناموفق بود.', 'err'); });
    };
  }

  function del(host, d) {
    Crm.confirm('حذف «' + d.name + '»؟', function () {
      Crm.call('crm.doctors.delete', { idx: d.idx }).then(function () {
        Crm.toast('پزشک حذف شد.', 'ok'); load(host, '');
      }, function () { Crm.toast('حذف ناموفق بود.', 'err'); });
    }, { danger: true });
  }

  Crm.pages.doctors = {
    title: 'پزشکان و پرستاران',
    render: function (host) { load(host, ''); }
  };
})(window);
