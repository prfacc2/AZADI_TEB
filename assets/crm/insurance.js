/* ============================================================================
   insurance.js — Insurance DEFINITION page (تعریف بیمه). ES5-only.
   v1.74: a registry editor for base (InsDef) and supplementary (SuppDef)
   insurances, managed through crm.insurance.* / crm.supp.* verbs against the
   C++ file-backed stores (data\insdefs.dat / data\suppdefs.dat). Every field the
   operator enters round-trips through C++, the org share flows to Ins_Percent /
   Supp_Percent (so printed + admission percentages honour it), and the merged
   insurance lists surface in the patient form + web admission. All totals /
   shares are computed live in the browser from the real inputs.
   ============================================================================ */
(function (global) {
  'use strict';
  var Crm = global.Crm;

  function fmtPriceInput(inp) {
    var digits = Crm.enDigits(inp.value || '').replace(/[^0-9]/g, '');
    inp.value = digits ? Crm.faDigits(Crm.fmtMoney(+digits)) : '';
  }
  function numVal(id) { return +Crm.enDigits((Crm.$(id) || { value: '' }).value.replace(/,/g, '')) || 0; }
  function val(id) { return (Crm.$(id) || { value: '' }).value; }
  function checked(id) { var e = Crm.$(id); return e ? e.checked : false; }

  var secCache = [];
  function fetchSections(onOk) {
    Crm.call('crm.sections.list', {}).then(function (d) {
      secCache = d.rows || [];
      var top = [];
      for (var i = 0; i < secCache.length; i++) if (!secCache[i].parentId) top.push(secCache[i]);
      onOk(top);
    }, function () { onOk([]); });
  }
  function sectionOptions(sel) {
    var o = '<option value="">— بدون بخش —</option>';
    for (var i = 0; i < secCache.length; i++) {
      if (secCache[i].parentId) continue;
      o += '<option value="' + Crm.esc(secCache[i].name) + '"' +
           (secCache[i].name === sel ? ' selected' : '') + '>' + Crm.esc(secCache[i].name) + '</option>';
    }
    return o;
  }

  function insTypeName(t) { return t === 'private' ? 'خصوصی' : (t === 'government' ? 'دولتی' : (t || '—')); }

  /* ----------------------------- base insurance tab ------------------------ */
  function baseTab(host) {
    var card = Crm.el('div', 'crm-card');
    card.innerHTML = '<div class="crm-card-title"><span class="dot"></span>تعریف بیمه پایه</div>';
    var form = Crm.el('div', 'crm-form');
    form.innerHTML =
      '<input type="hidden" id="bIdx" value="-1" />' +
      '<div class="crm-field"><label class="crm-label">بخش</label>' +
        '<select class="crm-select" id="bSection">' + sectionOptions('') + '</select></div>' +
      '<div class="crm-field"><label class="crm-label">کد بیمه</label>' +
        '<input class="crm-input" id="bCode" value="" /></div>' +
      '<div class="crm-field"><label class="crm-label">نام بیمه / گروه</label>' +
        '<input class="crm-input" id="bGroup" value="" placeholder="مثال: تأمین اجتماعی" /></div>' +
      '<div class="crm-field"><label class="crm-label">سهم سازمان (٪)</label>' +
        '<input class="crm-input" id="bOrg" value="" placeholder="خالی = استفاده از مقدار پیش‌فرض" /></div>' +
      '<div class="crm-field"><label class="crm-label">نام وارونه</label>' +
        '<input class="crm-input" id="bFlip" value="" /></div>' +
      '<div class="crm-field"><label class="crm-label">کد قرارداد</label>' +
        '<input class="crm-input" id="bContract" value="" /></div>' +
      '<div class="crm-field"><label class="crm-label">نوع بیمه</label>' +
        '<select class="crm-select" id="bType"><option value="government">دولتی</option>' +
        '<option value="private">خصوصی</option></select></div>' +
      '<div class="crm-field"><label class="crm-label">مبلغ فنی (ریال)</label>' +
        '<input class="crm-input" id="bTech" value="" /></div>' +
      '<div class="crm-field"><label class="crm-label">مبلغ حرفه‌ای (ریال)</label>' +
        '<input class="crm-input" id="bProf" value="" /></div>' +
      '<div class="crm-field"><label class="crm-label">مبلغ مصرفی (ریال)</label>' +
        '<input class="crm-input" id="bCons" value="" /></div>' +
      '<div class="crm-field full"><span class="crm-banner info" id="bTotal" style="margin:0">مجموع مبالغ: ۰</span></div>' +
      '<div class="crm-field full"><label class="crm-check"><input type="checkbox" id="bActive" checked />فعال</label></div>';
    card.appendChild(form);
    var foot = Crm.el('div', 'crm-modal-foot');
    foot.innerHTML = '<button class="crm-btn ghost" id="bReset">پاک‌کردن</button>' +
                     '<button class="crm-btn success" id="bActivate">ذخیره و فعال‌سازی</button>' +
                     '<button class="crm-btn primary" id="bSave">ذخیره</button>';
    card.appendChild(foot);
    host.appendChild(card);

    wirePrices(['bTech', 'bProf', 'bCons'], updateBaseTotal);
    Crm.$('bReset').onclick = function () { loadBaseDef(null); };
    Crm.$('bSave').onclick = function () { saveBase(host, false); };
    Crm.$('bActivate').onclick = function () { saveBase(host, true); };

    /* table */
    var tc = Crm.el('div', 'crm-card');
    tc.innerHTML = '<div class="crm-card-title"><span class="dot"></span>بیمه‌های پایه تعریف‌شده</div>';
    host.appendChild(tc);
    renderBaseTable(host);
  }

  function updateBaseTotal() {
    var t = numVal('bTech') + numVal('bProf') + numVal('bCons');
    var el = Crm.$('bTotal');
    if (el) el.innerHTML = 'مجموع مبالغ: ' + Crm.faDigits(Crm.fmtMoney(t)) + ' ریال';
  }

  function loadBaseDef(d) {
    Crm.$('bIdx').value = d ? d.idx : -1;
    Crm.$('bSection').innerHTML = sectionOptions(d ? d.sectionCode : '');
    Crm.$('bCode').value = d ? (d.insCode || '') : '';
    Crm.$('bGroup').value = d ? (d.groupName || '') : '';
    Crm.$('bOrg').value = (d && d.orgShare >= 0) ? Crm.faDigits('' + d.orgShare) : '';
    Crm.$('bFlip').value = d ? (d.flipName || '') : '';
    Crm.$('bContract').value = d ? (d.contractCode || '') : '';
    Crm.$('bType').value = d ? (d.insType || 'government') : 'government';
    Crm.$('bTech').value = d ? Crm.faDigits(Crm.fmtMoney(d.tech)) : '';
    Crm.$('bProf').value = d ? Crm.faDigits(Crm.fmtMoney(d.prof)) : '';
    Crm.$('bCons').value = d ? Crm.faDigits(Crm.fmtMoney(d.cons)) : '';
    Crm.$('bActive').checked = d ? d.active : true;
    updateBaseTotal();
  }

  function saveBase(host, activate) {
    var org = val('bOrg');
    var payload = {
      idx: +Crm.enDigits(val('bIdx')),
      sectionCode: val('bSection'),
      insCode: val('bCode'),
      orgShare: org ? +Crm.enDigits(org) : -1,
      groupName: val('bGroup'),
      flipName: val('bFlip'),
      contractCode: val('bContract'),
      insType: val('bType'),
      tech: numVal('bTech'), prof: numVal('bProf'), cons: numVal('bCons'),
      active: activate ? true : checked('bActive')
    };
    if (!payload.groupName && !payload.insCode) { Crm.toast('نام یا کد بیمه الزامی است.', 'err'); return; }
    Crm.call('crm.insurance.save', payload).then(function (d) {
      Crm.toast(activate ? 'بیمه ذخیره و فعال شد.' : 'بیمه ذخیره شد.', 'ok');
      loadBaseDef(null);
      renderBaseTable(host);
      refreshState();
    }, function () { Crm.toast('ذخیره ناموفق بود.', 'err'); });
  }

  function renderBaseTable(host) {
    Crm.call('crm.insurance.list', {}).then(function (d) {
      var rows = d.rows || [];
      if (Crm.state.data) Crm.state.data.insDefs = rows;
      var tc = host.childNodes[host.childNodes.length - 1];
      if (!tc) return;
      var old = tc.getElementsByTagName('table');
      for (var i = old.length - 1; i >= 0; i--) old[i].parentNode.removeChild(old[i]);
      tc.appendChild(Crm.table([
        { key: 'i', label: 'ردیف', render: function (r, i) { return Crm.faDigits('' + (i + 1)); } },
        { key: 'group', label: 'نام بیمه', render: function (r) { return '<b>' + Crm.esc(r.groupName || r.insCode || '—') + '</b>'; } },
        { key: 'code', label: 'کد بیمه', cls: 'c-mono', render: function (r) { return Crm.esc(r.insCode || '—'); } },
        { key: 'section', label: 'بخش', render: function (r) { return Crm.esc(r.sectionCode || '—'); } },
        { key: 'org', label: 'سهم سازمان', cls: 'c-num', render: function (r) { return r.orgShare >= 0 ? Crm.faDigits('' + r.orgShare) + '٪' : 'پیش‌فرض'; } },
        { key: 'type', label: 'نوع', render: function (r) { return Crm.esc(insTypeName(r.insType)); } },
        { key: 'total', label: 'مجموع مبالغ', cls: 'c-num', render: function (r) { return Crm.faDigits(Crm.fmtMoney(r.total || (r.tech + r.prof + r.cons))); } },
        { key: 'active', label: 'وضعیت', render: function (r) { return Crm.pill(r.active ? 'فعال' : 'غیرفعال', r.active ? 'on' : 'off'); } },
        { key: 'ops', label: 'عملیات', render: function (r) {
            var b = Crm.el('span');
            b.innerHTML = '<button class="crm-row-btn" data-act="edit">ویرایش</button>' +
                          '<button class="crm-row-btn danger" data-act="del">حذف</button>';
            b.childNodes[0].onclick = function () { loadBaseDef(r); };
            b.childNodes[1].onclick = function () {
              Crm.confirm('حذف تعریف بیمه «' + (r.groupName || r.insCode) + '»؟', function () {
                Crm.call('crm.insurance.delete', { idx: r.idx }).then(function () {
                  Crm.toast('بیمه حذف شد.', 'ok'); renderBaseTable(host); refreshState();
                }, function () { Crm.toast('حذف ناموفق بود.', 'err'); });
              }, { danger: true });
            };
            return b;
          } }
      ], rows));
    });
  }

  /* ------------------------- supplementary insurance tab ------------------- */
  function suppTab(host) {
    var card = Crm.el('div', 'crm-card');
    card.innerHTML = '<div class="crm-card-title"><span class="dot"></span>تعریف بیمه تکمیلی</div>';
    var form = Crm.el('div', 'crm-form');
    form.innerHTML =
      '<input type="hidden" id="cIdx" value="-1" />' +
      '<div class="crm-field"><label class="crm-label">بخش</label>' +
        '<select class="crm-select" id="cSection">' + sectionOptions('') + '</select></div>' +
      '<div class="crm-field"><label class="crm-label">مشخصات بیمه</label>' +
        '<input class="crm-input" id="cSpec" value="" /></div>' +
      '<div class="crm-field"><label class="crm-label">نام بیمه تکمیلی</label>' +
        '<input class="crm-input" id="cName" value="" placeholder="مثال: بیمه ایران" /></div>' +
      '<div class="crm-field"><label class="crm-label">نوع تعرفه</label>' +
        '<input class="crm-input" id="cTariff" value="" /></div>' +
      '<div class="crm-field"><label class="crm-label">فرانشیز</label>' +
        '<input class="crm-input" id="cFranchise" value="دستی" /></div>' +
      '<div class="crm-field"><label class="crm-label">مبلغ سقف (ریال)</label>' +
        '<input class="crm-input" id="cCeiling" value="" /></div>' +
      '<div class="crm-field"><label class="crm-label">کد نوع بیمه</label>' +
        '<input class="crm-input" id="cTypeCode" value="" /></div>' +
      '<div class="crm-field"><label class="crm-label">کد قرارداد</label>' +
        '<input class="crm-input" id="cContract" value="" /></div>' +
      '<div class="crm-field"><label class="crm-label">درصد سهم سازمان در فرانشیز</label>' +
        '<input class="crm-input" id="cOrgPct" value="" /></div>' +
      '<div class="crm-field"><label class="crm-label">نوع محاسبه قیمت</label>' +
        '<select class="crm-select" id="cPriceType"><option value="private">خصوصی</option>' +
        '<option value="government">دولتی</option></select></div>' +
      '<div class="crm-field"><label class="crm-label">رنگ</label>' +
        '<input type="color" class="crm-color" id="cColor" value="#2f6fe4" /></div>' +
      '<div class="crm-field"><label class="crm-label">مبلغ فنی (ریال)</label>' +
        '<input class="crm-input" id="cTech" value="" /></div>' +
      '<div class="crm-field"><label class="crm-label">مبلغ حرفه‌ای (ریال)</label>' +
        '<input class="crm-input" id="cProf" value="" /></div>' +
      '<div class="crm-field"><label class="crm-label">مبلغ مصرفی (ریال)</label>' +
        '<input class="crm-input" id="cCons" value="" /></div>' +
      '<div class="crm-field"><label class="crm-label">تاریخ اعتبار</label>' +
        '<input class="crm-input" id="cValidity" value="" placeholder="مثال: 1404/12/29" /></div>' +
      '<div class="crm-field"><label class="crm-label">نام کاربری</label>' +
        '<input class="crm-input" id="cUser" value="" /></div>' +
      '<div class="crm-field"><label class="crm-label">کد ملی (الزامی)</label>' +
        '<input class="crm-input" id="cNid" value="" /></div>' +
      '<div class="crm-field"><label class="crm-label">نام فایل</label>' +
        '<input class="crm-input" id="cFile" value="" /></div>' +
      '<div class="crm-field"><label class="crm-check"><input type="checkbox" id="cBooklet" />دفترچه</label></div>' +
      '<div class="crm-field"><label class="crm-check"><input type="checkbox" id="cFranchiseDefault" checked />پیش‌فرض دستی در پذیرش</label></div>' +
      '<div class="crm-field"><label class="crm-check"><input type="checkbox" id="cByLaw" />محاسبه بر اساس قانون بیمه</label></div>' +
      '<div class="crm-field"><label class="crm-check"><input type="checkbox" id="cDefaultOff" />بدون فرانشیز اگر بیمه پایه پذیرفته شد</label></div>' +
      '<div class="crm-field"><label class="crm-check"><input type="checkbox" id="cDifference" />اختلاف</label></div>' +
      '<div class="crm-field full"><span class="crm-banner info" id="cTotal" style="margin:0">مجموع مبالغ: ۰ — سهم سازمان: ۰</span></div>' +
      '<div class="crm-field full"><label class="crm-check"><input type="checkbox" id="cActive" checked />فعال</label></div>';
    card.appendChild(form);
    var foot = Crm.el('div', 'crm-modal-foot');
    foot.innerHTML = '<button class="crm-btn ghost" id="cReset">پاک‌کردن</button>' +
                     '<button class="crm-btn success" id="cActivate">ذخیره و فعال‌سازی</button>' +
                     '<button class="crm-btn primary" id="cSave">ذخیره</button>';
    card.appendChild(foot);
    host.appendChild(card);

    wirePrices(['cCeiling', 'cTech', 'cProf', 'cCons'], updateSuppTotal);
    var op = Crm.$('cOrgPct'); if (op) op.onkeyup = updateSuppTotal;
    Crm.$('cReset').onclick = function () { loadSuppDef(null); };
    Crm.$('cSave').onclick = function () { saveSupp(host, false); };
    Crm.$('cActivate').onclick = function () { saveSupp(host, true); };
    if (Crm.state.data && Crm.state.data.user) { var u = Crm.$('cUser'); if (u && !u.value) u.value = Crm.state.data.user; }

    var tc = Crm.el('div', 'crm-card');
    tc.innerHTML = '<div class="crm-card-title"><span class="dot"></span>بیمه‌های تکمیلی تعریف‌شده</div>';
    host.appendChild(tc);
    renderSuppTable(host);
  }

  function updateSuppTotal() {
    var t = numVal('cTech') + numVal('cProf') + numVal('cCons');
    var pct = +Crm.enDigits(val('cOrgPct')) || 0;
    var org = Math.round(t * pct / 100);
    var el = Crm.$('cTotal');
    if (el) el.innerHTML = 'مجموع مبالغ: ' + Crm.faDigits(Crm.fmtMoney(t)) +
      ' ریال — سهم سازمان (' + Crm.faDigits('' + pct) + '٪): ' + Crm.faDigits(Crm.fmtMoney(org)) + ' ریال';
  }

  function loadSuppDef(d) {
    Crm.$('cIdx').value = d ? d.idx : -1;
    Crm.$('cSection').innerHTML = sectionOptions(d ? d.sectionCode : '');
    Crm.$('cSpec').value = d ? (d.insSpec || '') : '';
    Crm.$('cName').value = d ? (d.name || '') : '';
    Crm.$('cTariff').value = d ? (d.tariffType || '') : '';
    Crm.$('cFranchise').value = d ? (d.franchise || 'دستی') : 'دستی';
    Crm.$('cCeiling').value = d ? Crm.faDigits(Crm.fmtMoney(d.ceiling)) : '';
    Crm.$('cTypeCode').value = d ? (d.insTypeCode || '') : '';
    Crm.$('cContract').value = d ? (d.contractCode || '') : '';
    Crm.$('cOrgPct').value = (d && d.franchiseOrgPct) ? Crm.faDigits('' + d.franchiseOrgPct) : '';
    Crm.$('cPriceType').value = d ? (d.priceCalcType || 'private') : 'private';
    Crm.$('cColor').value = d ? (d.color || '#2f6fe4') : '#2f6fe4';
    Crm.$('cTech').value = d ? Crm.faDigits(Crm.fmtMoney(d.tech)) : '';
    Crm.$('cProf').value = d ? Crm.faDigits(Crm.fmtMoney(d.prof)) : '';
    Crm.$('cCons').value = d ? Crm.faDigits(Crm.fmtMoney(d.cons)) : '';
    Crm.$('cValidity').value = d ? (d.validityDate || '') : '';
    Crm.$('cUser').value = d ? (d.username || (Crm.state.data && Crm.state.data.user) || '') : (Crm.state.data && Crm.state.data.user) || '';
    Crm.$('cNid').value = d ? (d.nationalId || '') : '';
    Crm.$('cFile').value = d ? (d.fileName || '') : '';
    Crm.$('cBooklet').checked = d ? !!d.booklet : false;
    Crm.$('cFranchiseDefault').checked = d ? !!d.franchiseDefault : true;
    Crm.$('cByLaw').checked = d ? !!d.byLaw : false;
    Crm.$('cDefaultOff').checked = d ? !!d.defaultOff : false;
    Crm.$('cDifference').checked = d ? !!d.difference : false;
    Crm.$('cActive').checked = d ? d.active : true;
    updateSuppTotal();
  }

  function saveSupp(host, activate) {
    var payload = {
      idx: +Crm.enDigits(val('cIdx')),
      sectionCode: val('cSection'),
      insSpec: val('cSpec'),
      name: val('cName'),
      tariffType: val('cTariff'),
      franchise: val('cFranchise'),
      franchiseDefault: checked('cFranchiseDefault') ? 1 : 0,
      byLaw: checked('cByLaw') ? 1 : 0,
      ceiling: numVal('cCeiling'),
      insTypeCode: val('cTypeCode'),
      contractCode: val('cContract'),
      tech: numVal('cTech'), prof: numVal('cProf'), cons: numVal('cCons'),
      franchiseOrgPct: +Crm.enDigits(val('cOrgPct')) || 0,
      defaultOff: checked('cDefaultOff') ? 1 : 0,
      priceCalcType: val('cPriceType'),
      difference: checked('cDifference') ? 1 : 0,
      color: val('cColor'),
      validityDate: val('cValidity'),
      username: val('cUser'),
      nationalId: val('cNid'),
      booklet: checked('cBooklet') ? 1 : 0,
      fileName: val('cFile'),
      active: activate ? true : checked('cActive')
    };
    if (!payload.name) { Crm.toast('نام بیمه تکمیلی الزامی است.', 'err'); return; }
    if (!payload.nationalId) { Crm.toast('کد ملی الزامی است.', 'err'); return; }
    Crm.call('crm.supp.save', payload).then(function () {
      Crm.toast(activate ? 'بیمه تکمیلی ذخیره و فعال شد.' : 'بیمه تکمیلی ذخیره شد.', 'ok');
      loadSuppDef(null);
      renderSuppTable(host);
      refreshState();
    }, function (e) { Crm.toast('ذخیره ناموفق بود.', 'err'); });
  }

  function renderSuppTable(host) {
    Crm.call('crm.supp.list', {}).then(function (d) {
      var rows = d.rows || [];
      if (Crm.state.data) Crm.state.data.suppDefs = rows;
      var tc = host.childNodes[host.childNodes.length - 1];
      if (!tc) return;
      var old = tc.getElementsByTagName('table');
      for (var i = old.length - 1; i >= 0; i--) old[i].parentNode.removeChild(old[i]);
      tc.appendChild(Crm.table([
        { key: 'validity', label: 'تاریخ اعتبار', render: function (r) { return Crm.esc(r.validityDate || '—'); } },
        { key: 'active', label: 'وضعیت', render: function (r) { return Crm.pill(r.active ? 'فعال' : 'غیرفعال', r.active ? 'on' : 'off'); } },
        { key: 'idx', label: 'شناسه', cls: 'c-mono', render: function (r) { return Crm.faDigits('' + r.idx); } },
        { key: 'user', label: 'نام کاربری', render: function (r) { return Crm.esc(r.username || '—'); } },
        { key: 'nid', label: 'کد ملی', cls: 'c-mono', render: function (r) { return Crm.faDigits(Crm.esc(r.nationalId || '—')); } },
        { key: 'booklet', label: 'دفترچه', render: function (r) { return r.booklet ? '✔' : '—'; } },
        { key: 'file', label: 'نام فایل', render: function (r) { return Crm.esc(r.fileName || '—'); } },
        { key: 'org', label: 'سهم سازمان', cls: 'c-num', render: function (r) { return Crm.faDigits('' + (r.franchiseOrgPct || 0)) + '٪'; } },
        { key: 'type', label: 'نوع بیمه', render: function (r) { return Crm.esc(insTypeName(r.priceCalcType)); } },
        { key: 'name', label: 'نام بیمه', render: function (r) { return '<b>' + Crm.esc(r.name) + '</b>'; } },
        { key: 'code', label: 'کد بیمه', cls: 'c-mono', render: function (r) { return Crm.esc(r.insTypeCode || '—'); } },
        { key: 'ops', label: 'عملیات', render: function (r) {
            var b = Crm.el('span');
            b.innerHTML = '<button class="crm-row-btn" data-act="edit">ویرایش</button>' +
                          '<button class="crm-row-btn danger" data-act="del">حذف</button>';
            b.childNodes[0].onclick = function () { loadSuppDef(r); };
            b.childNodes[1].onclick = function () {
              Crm.confirm('حذف بیمه تکمیلی «' + r.name + '»؟', function () {
                Crm.call('crm.supp.delete', { idx: r.idx }).then(function () {
                  Crm.toast('بیمه تکمیلی حذف شد.', 'ok'); renderSuppTable(host); refreshState();
                }, function () { Crm.toast('حذف ناموفق بود.', 'err'); });
              }, { danger: true });
            };
            return b;
          } }
      ], rows));
    });
  }

  /* ----------------------------- shared helpers ---------------------------- */
  function wirePrices(ids, onCh) {
    for (var i = 0; i < ids.length; i++) {
      var e = Crm.$(ids[i]);
      if (e) { (function (elx) { elx.onkeyup = function () { fmtPriceInput(elx); if (onCh) onCh(); }; elx.onblur = function () { fmtPriceInput(elx); if (onCh) onCh(); }; })(e); }
    }
  }
  /* re-fetch crm.init so the merged insurance lists (patient form + admission)
     pick up the just-saved definitions without a full app restart. */
  function refreshState() {
    Crm.call('crm.init', {}).then(function (d) { if (d) Crm.state.data = d; });
  }

  Crm.pages.insurance = {
    title: 'تعریف بیمه',
    render: function (host) {
      fetchSections(function () {
        host.innerHTML = '';
        Crm.head(host, 'تعریف بیمه', 'مدیریت بیمه‌های پایه و تکمیلی');
        var tabs = Crm.el('div', 'crm-tabs');
        var tBase = Crm.el('button', 'crm-tab', 'بیمه پایه');
        var tSupp = Crm.el('button', 'crm-tab', 'بیمه تکمیلی');
        tabs.appendChild(tBase); tabs.appendChild(tSupp);
        host.appendChild(tabs);
        var body = Crm.el('div', 'crm-tab-body');
        host.appendChild(body);
        function sel(which) {
          Crm._insTab = which;
          tBase.className = 'crm-tab' + (which === 'base' ? ' active' : '');
          tSupp.className = 'crm-tab' + (which === 'supp' ? ' active' : '');
          body.innerHTML = '';
          if (which === 'supp') suppTab(body); else baseTab(body);
        }
        tBase.onclick = function () { sel('base'); };
        tSupp.onclick = function () { sel('supp'); };
        sel(Crm._insTab || 'base');
      });
    }
  };
})(window);
