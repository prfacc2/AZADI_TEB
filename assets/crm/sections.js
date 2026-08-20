/* ============================================================================
   sections.js — Sections (بخش) management. ES5-only.
   List / search / add / edit / delete clinic sections via crm.sections.* verbs.
   File format (data\sections.dat) is owned by the C++ layer (Sections_Upsert /
   Sections_Delete); the UI only sends fields.
   ============================================================================ */
(function (global) {
  'use strict';
  var Crm = global.Crm;

  var KINDS = [
    { v: 'reception',  l: 'پذیرش' },
    { v: 'injection',  l: 'تزریق' },
    { v: 'lab',        l: 'آزمایشگاه' },
    { v: 'radiology',  l: 'رادیولوژی' },
    { v: 'physio',     l: 'فیزیوتراپی' },
    { v: 'other',      l: 'سایر' }
  ];

  function load(host, q) {
    Crm.call('crm.sections.list', {}).then(function (d) {
      render(host, d.rows || [], q);
    }, function () {
      host.innerHTML = '';
      Crm.head(host, 'بخش‌ها', 'تعریف و مدیریت بخش‌های درمانگاه');
      host.appendChild(Crm.el('div', 'crm-banner err', 'بارگذاری بخش‌ها ناموفق بود.'));
    });
  }

  function render(host, rows, q) {
    host.innerHTML = '';
    Crm.head(host, 'بخش‌ها', 'تعریف و مدیریت بخش‌های درمانگاه');

    /* toolbar */
    var tb = Crm.el('div', 'crm-toolbar');
    var search = Crm.el('div', 'crm-search');
    search.innerHTML =
      '<input class="crm-input" id="secQ" placeholder="جستجوی نام یا کد بخش…" value="' + Crm.esc(q || '') + '" />' +
      '<span class="crm-search-ic"><svg viewBox="0 0 24 24" width="16" height="16"><path fill="currentColor" d="M10 2a8 8 0 105.3 14L20 20.7 21.7 19l-4.7-4.7A8 8 0 0010 2zm0 2a6 6 0 110 12 6 6 0 010-12z"/></svg></span>';
    tb.appendChild(search);
    tb.appendChild(Crm.el('div', 'spacer', ''));
    var addBtn = Crm.el('button', 'crm-btn primary', '+ افزودن بخش');
    tb.appendChild(addBtn);
    host.appendChild(tb);

    /* filter */
    var fq = (q || '').toLowerCase();
    var view = [];
    for (var i = 0; i < rows.length; i++) {
      var r = rows[i];
      if (!fq || ('' + r.name).toLowerCase().indexOf(fq) >= 0 ||
          ('' + r.code).toLowerCase().indexOf(fq) >= 0) view.push(r);
    }

    host.appendChild(Crm.table([
      { key: 'i', label: 'ردیف', cls: 'c-row', render: function (r, i) { return Crm.faDigits('' + (i + 1)); } },
      { key: 'code', label: 'کد', cls: 'c-mono', render: function (r) { return Crm.esc(r.code || '—'); } },
      { key: 'name', label: 'نام بخش', render: function (r) { return '<b>' + Crm.esc(r.name) + '</b>'; } },
      { key: 'kind', label: 'نوع', render: function (r) { return Crm.esc(r.kindLabel || r.kind); } },
      { key: 'active', label: 'وضعیت', render: function (r) { return Crm.pill(r.active ? 'فعال' : 'غیرفعال', r.active ? 'on' : 'off'); } },
      { key: 'ops', label: 'عملیات', render: function (r) {
          var b = Crm.el('span');
          b.innerHTML = '<button class="crm-row-btn" data-act="edit">ویرایش</button>' +
                        '<button class="crm-row-btn danger" data-act="del">حذف</button>';
          b.childNodes[0].onclick = function () { openModal(host, r); };
          b.childNodes[1].onclick = function () { del(host, r); };
          return b;
        } }
    ], view));

    /* wire search */
    var qEl = Crm.$('secQ');
    if (qEl) {
      qEl.onkeyup = function () { render(host, rows, qEl.value); qEl.focus(); };
      if (q) { var v = qEl.value; qEl.value = ''; qEl.value = v; }
    }
    addBtn.onclick = function () { openModal(host, null); };
  }

  function openModal(host, sec) {
    var adding = !sec;
    var m = Crm.modal(adding ? 'افزودن بخش' : 'ویرایش بخش', null);
    var body = m.body;
    body.innerHTML =
      '<div class="crm-form">' +
      '<input type="hidden" id="fId" value="' + (sec ? sec.id : 0) + '" />' +
      '<div class="crm-field"><label class="crm-label">کد بخش</label>' +
        '<input class="crm-input" id="fCode" value="' + Crm.esc(sec ? sec.code : '') + '" placeholder="مثال: REC01" /></div>' +
      '<div class="crm-field"><label class="crm-label">نام بخش</label>' +
        '<input class="crm-input" id="fName" value="' + Crm.esc(sec ? sec.name : '') + '" placeholder="نام فارسی بخش" /></div>' +
      '<div class="crm-field"><label class="crm-label">نوع بخش</label>' +
        '<select class="crm-select" id="fKind">' + kindOptions(sec ? sec.kind : 'reception') + '</select></div>' +
      '<div class="crm-field full"><label class="crm-check"><input type="checkbox" id="fActive" ' + (!sec || sec.active ? 'checked' : '') + ' />فعال</label></div>' +
      '</div>';
    var foot = Crm.el('div', 'crm-modal-foot');
    foot.innerHTML = '<button class="crm-btn ghost" id="mCancel">انصراف</button><button class="crm-btn primary" id="mSave">ذخیره</button>';
    m.card.appendChild(foot);
    Crm.$('mCancel').onclick = m.close;
    Crm.$('mSave').onclick = function () {
      var payload = {
        id: +Crm.enDigits(Crm.$('fId').value) || 0,
        code: Crm.$('fCode').value,
        name: Crm.$('fName').value,
        kind: Crm.$('fKind').value,
        active: Crm.$('fActive').checked
      };
      if (!payload.name) { Crm.toast('نام بخش الزامی است.', 'err'); return; }
      Crm.call('crm.sections.save', payload).then(function () {
        Crm.toast(adding ? 'بخش اضافه شد.' : 'بخش ویرایش شد.', 'ok');
        m.close(); load(host, '');
      }, function (e) { Crm.toast('ذخیره ناموفق بود.', 'err'); });
    };
  }

  function kindOptions(sel) {
    var o = '';
    for (var i = 0; i < KINDS.length; i++) {
      o += '<option value="' + KINDS[i].v + '"' + (KINDS[i].v === sel ? ' selected' : '') + '>' + Crm.esc(KINDS[i].l) + '</option>';
    }
    return o;
  }

  function del(host, sec) {
    if (!confirm('حذف بخش «' + sec.name + '»؟')) return;
    Crm.call('crm.sections.delete', { id: sec.id }).then(function () {
      Crm.toast('بخش حذف شد.', 'ok'); load(host, '');
    }, function () { Crm.toast('حذف ناموفق بود.', 'err'); });
  }

  Crm.pages.sections = {
    title: 'بخش‌ها',
    render: function (host) { load(host, ''); }
  };
})(window);
