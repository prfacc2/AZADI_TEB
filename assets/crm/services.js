/* ============================================================================
   services.js — Services (خدمات) management. ES5-only.
   List / search / add / edit / delete clinic services via crm.services.* verbs.
   The on-disk store (data\services.dat) is owned by C++ (addService /
   updateService / removeService); the UI sends fields + originalCode for edits.
   ============================================================================ */
(function (global) {
  'use strict';
  var Crm = global.Crm;

  /* Live-format the price input with thousand separators (ریال). Strips every
     non-digit (commas, spaces, …) and re-groups in 3s, shown in Persian digits
     to match the rest of the panel. On save the commas are stripped again. */
  function fmtPriceInput(inp) {
    var digits = Crm.enDigits(inp.value || '').replace(/[^0-9]/g, '');
    inp.value = digits ? Crm.faDigits(Crm.fmtMoney(+digits)) : '';
  }

  function load(host, q) {
    Crm.call('crm.services.list', { q: q || '' }).then(function (d) {
      render(host, d.rows || [], q);
    }, function () {
      host.innerHTML = '';
      Crm.head(host, 'خدمات', 'مدیریت فهرست خدمات و تعرفه‌ها');
      host.appendChild(Crm.el('div', 'crm-banner err', 'بارگذاری خدمات ناموفق بود.'));
    });
  }

  function render(host, rows, q) {
    host.innerHTML = '';
    Crm.head(host, 'خدمات', 'مدیریت فهرست خدمات و تعرفه‌ها');

    var tb = Crm.el('div', 'crm-toolbar');
    var search = Crm.el('div', 'crm-search');
    search.innerHTML =
      '<input class="crm-input" id="svQ" placeholder="جستجوی نام، کد یا دسته خدمت…" value="' + Crm.esc(q || '') + '" />' +
      '<span class="crm-search-ic"><svg viewBox="0 0 24 24" width="16" height="16"><path fill="currentColor" d="M10 2a8 8 0 105.3 14L20 20.7 21.7 19l-4.7-4.7A8 8 0 0010 2zm0 2a6 6 0 110 12 6 6 0 010-12z"/></svg></span>';
    tb.appendChild(search);
    tb.appendChild(Crm.el('div', 'spacer', ''));
    var addBtn = Crm.el('button', 'crm-btn primary', '+ افزودن خدمت');
    tb.appendChild(addBtn);
    host.appendChild(tb);

    host.appendChild(Crm.table([
      { key: 'i', label: 'ردیف', render: function (r, i) { return Crm.faDigits('' + (i + 1)); } },
      { key: 'code', label: 'کد', cls: 'c-mono', render: function (r) { return Crm.esc(r.code); } },
      { key: 'name', label: 'نام خدمت', render: function (r) { return '<b>' + Crm.esc(r.name) + '</b>'; } },
      { key: 'category', label: 'دسته', render: function (r) { return Crm.esc(r.category || '—'); } },
      { key: 'price', label: 'مبلغ (ریال)', cls: 'c-num', render: function (r) { return Crm.faDigits(Crm.fmtMoney(r.price)); } },
      { key: 'status', label: 'وضعیت', render: function (r) { return Crm.pill(r.status ? 'فعال' : 'غیرفعال', r.status ? 'on' : 'off'); } },
      { key: 'ops', label: 'عملیات', render: function (r) {
          var b = Crm.el('span');
          b.innerHTML = '<button class="crm-row-btn" data-act="edit">ویرایش</button>' +
                        '<button class="crm-row-btn danger" data-act="del">حذف</button>';
          b.childNodes[0].onclick = function () { openModal(host, r); };
          b.childNodes[1].onclick = function () { del(host, r); };
          return b;
        } }
    ], rows));

    var qEl = Crm.$('svQ');
    if (qEl) {
      qEl.onkeyup = function () {
        Crm.call('crm.services.list', { q: qEl.value }).then(function (d) { render(host, d.rows || [], qEl.value); qEl.focus(); });
      };
      if (q) { var v = qEl.value; qEl.value = ''; qEl.value = v; }
    }
    addBtn.onclick = function () { openModal(host, null); };
  }

  function openModal(host, s) {
    var adding = !s;
    if (!s) s = { status: 1 };
    var m = Crm.modal(adding ? 'افزودن خدمت' : 'ویرایش خدمت', null);
    var body = m.body;
    body.innerHTML =
      '<div class="crm-form">' +
      '<input type="hidden" id="sOrig" value="' + Crm.esc(adding ? '' : s.code) + '" />' +
      '<div class="crm-field"><label class="crm-label">کد خدمت</label>' +
        '<input class="crm-input" id="sCode" value="' + Crm.esc(s.code || '') + '" placeholder="خالی = خودکار" /></div>' +
      '<div class="crm-field"><label class="crm-label">نام خدمت</label>' +
        '<input class="crm-input" id="sName" value="' + Crm.esc(s.name || '') + '" /></div>' +
      '<div class="crm-field"><label class="crm-label">دسته</label>' +
        '<input class="crm-input" id="sCat" value="' + Crm.esc(s.category || '') + '" /></div>' +
      '<div class="crm-field"><label class="crm-label">بخش</label>' +
        '<input class="crm-input" id="sDept" value="' + Crm.esc(s.dept || '') + '" /></div>' +
      '<div class="crm-field"><label class="crm-label">مبلغ پایه (ریال)</label>' +
        '<input class="crm-input" id="sPrice" value="' + Crm.faDigits(Crm.fmtMoney(s.price || 0)) + '" /></div>' +
      '<div class="crm-field"><label class="crm-label">نوع بیمه</label>' +
        '<input class="crm-input" id="sIns" value="' + Crm.esc(s.insType || '') + '" /></div>' +
      '<div class="crm-field full"><label class="crm-label">شرح / توضیحات</label>' +
        '<textarea class="crm-textarea" id="sDesc">' + Crm.esc(s.desc || '') + '</textarea></div>' +
      '<div class="crm-field full"><label class="crm-check"><input type="checkbox" id="sActive" ' + (s.status ? 'checked' : '') + ' />فعال</label></div>' +
      '</div>';
    var foot = Crm.el('div', 'crm-modal-foot');
    foot.innerHTML = '<button class="crm-btn ghost" id="mCancel">انصراف</button><button class="crm-btn primary" id="mSave">ذخیره</button>';
    m.card.appendChild(foot);
    Crm.$('mCancel').onclick = m.close;
    var priceEl = Crm.$('sPrice');
    if (priceEl) {
      priceEl.onkeyup = function () { fmtPriceInput(priceEl); };
      priceEl.onblur = function () { fmtPriceInput(priceEl); };
    }
    Crm.$('mSave').onclick = function () {
      var payload = {
        originalCode: Crm.$('sOrig').value,
        code: Crm.$('sCode').value,
        name: Crm.$('sName').value,
        category: Crm.$('sCat').value,
        dept: Crm.$('sDept').value,
        price: +Crm.enDigits(Crm.$('sPrice').value.replace(/,/g, '')) || 0,
        insType: Crm.$('sIns').value,
        desc: Crm.$('sDesc').value,
        active: Crm.$('sActive').checked
      };
      if (!payload.name) { Crm.toast('نام خدمت الزامی است.', 'err'); return; }
      Crm.call('crm.services.save', payload).then(function () {
        Crm.toast(adding ? 'خدمت اضافه شد.' : 'خدمت ویرایش شد.', 'ok');
        m.close(); load(host, '');
      }, function () { Crm.toast('ذخیره ناموفق بود.', 'err'); });
    };
  }

  function del(host, s) {
    if (!confirm('حذف خدمت «' + s.name + '» (' + s.code + ')؟')) return;
    Crm.call('crm.services.delete', { code: s.code }).then(function () {
      Crm.toast('خدمت حذف شد.', 'ok'); load(host, '');
    }, function () { Crm.toast('حذف ناموفق بود.', 'err'); });
  }

  Crm.pages.services = {
    title: 'خدمات',
    render: function (host) { load(host, ''); }
  };
})(window);
