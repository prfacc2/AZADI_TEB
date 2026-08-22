/* ============================================================================
   accounts.js — «تعریف حساب کاربری» (v1.79.0, replaces the old «کاربران»).
   ES5-only.
   ----------------------------------------------------------------------------
   The account workshop, driven by the personnel registry («تعریف پرسنل»):
     1) pick a department (or «همه» / «در حالت تعلیق») → its persons list
        renders LIVE;
     2) search them by کد پرسنلی / نام / کد ملی (one box, server matches all);
     3) pick a person → assign username + password + ACCESS TICKS (دسترسی‌ها);
     4) the account lands in the accounts table — username visible, password
        NEVER shown; clicking a name opens the person's sheet, clicking a
        department opens the department sheet.
   The organisational-department manager (بخش‌های سازمانی) that used to live on
   the old «کاربران» page is preserved below (nothing is lost).
   ============================================================================ */
(function (global) {
  'use strict';
  var Crm = global.Crm;

  /* the access ticks available today (keys must match userHasPerm in C++) */
  var PERMS = [
    { key: 'admission', label: 'پذیرش بیمار',  hint: 'ثبت و صدور قبض پذیرش' },
    { key: 'worklist',  label: 'کارتابل',       hint: 'مشاهده پیام‌های مدیریت' },
    { key: 'cashier',   label: 'گزارش و صندوق', hint: 'صندوق نرفته‌ها و صف پذیرش' },
    { key: 'settings',  label: 'تنظیمات',       hint: 'تنظیمات پذیرش' }
  ];
  function permsLabel(p) {
    if (!p) return 'دسترسی کامل';
    if (p === '-') return 'بدون دسترسی';
    var map = { admission: 'پذیرش', worklist: 'کارتابل', cashier: 'صندوق', settings: 'تنظیمات' };
    var parts = ('' + p).split(','), out = [], i;
    for (i = 0; i < parts.length; i++) if (map[parts[i]]) out.push(map[parts[i]]);
    return out.length ? out.join('، ') : 'دسترسی کامل';
  }

  /* ---- dept info sheet (shared) ------------------------------------------ */
  Crm.viewDeptInfo = function (deptId) {
    Crm.call('crm.depts.info', { id: deptId }).then(function (d) {
      if (!d || !d.ok) { Crm.toast('اطلاعات بخش پیدا نشد.', 'err'); return; }
      var dep = d.dept || {}, i;
      var m = Crm.modal('اطلاعات بخش — ' + (dep.name || ''), null);
      var h = '<div class="crm-printable" id="deptSheet">' +
        '<div class="crm-sheet-head"><span class="crm-sheet-id">' +
          '<b>' + Crm.esc(dep.name || '') + '</b>' +
          '<span>مدیر بخش: ' + Crm.esc(dep.manager || '—') + '</span>' +
          '<span class="crm-sheet-code">' + Crm.esc(dep.id || '') + '</span>' +
        '</span></div>' +
        '<div class="crm-sheet-sub">پرسنل این بخش (' + Crm.faDigits('' + ((d.persons || []).length)) + ' نفر)</div>' +
        '<table class="crm-sheet-tbl"><tr><td>کد پرسنلی</td><td>نام</td><td>نقش</td><td>مقام/سمت</td><td>حساب کاربری</td></tr>';
      var ps = d.persons || [];
      for (i = 0; i < ps.length; i++) {
        h += '<tr><td>' + Crm.esc(ps[i].code) + '</td><td><b>' + Crm.esc((ps[i].firstName || '') + ' ' + (ps[i].lastName || '')) + '</b></td>' +
             '<td>' + Crm.esc(ps[i].roleLabel || '') + '</td><td>' + Crm.esc(ps[i].position || '—') + '</td>' +
             '<td>' + Crm.esc(ps[i].username || '—') + '</td></tr>';
      }
      if (!ps.length) h += '<tr><td colspan="5">پرسنلی در این بخش ثبت نشده است.</td></tr>';
      h += '</table><div class="crm-sheet-sub">حساب‌های کاربری بخش</div>' +
           '<table class="crm-sheet-tbl"><tr><td>نام کاربری</td><td>نام کامل</td><td>وضعیت</td></tr>';
      var us = d.users || [];
      for (i = 0; i < us.length; i++) {
        h += '<tr><td>' + Crm.esc(us[i].username) + '</td><td><b>' + Crm.esc(us[i].fullname) + '</b></td>' +
             '<td>' + (us[i].online ? 'آنلاین' : '—') + '</td></tr>';
      }
      if (!us.length) h += '<tr><td colspan="3">حسابی برای این بخش نیست.</td></tr>';
      h += '</table></div>';
      m.body.innerHTML = h;
      var foot = Crm.el('div', 'crm-modal-foot');
      foot.innerHTML = '<button class="crm-btn ghost" id="shClose">بستن</button>' +
                       '<button class="crm-btn outline" id="shPrint">چاپ اطلاعات بخش</button>';
      m.card.appendChild(foot);
      Crm.$('shClose').onclick = m.close;
      Crm.$('shPrint').onclick = function () { Crm.printNode('deptSheet', 'اطلاعات بخش — ' + (dep.name || '')); };
    }, function () { Crm.toast('بارگذاری اطلاعات بخش ناموفق بود.', 'err'); });
  };

  /* ---- page state ---------------------------------------------------------- */
  var st = { dept: '', q: '', picked: null,
             fDept: '', fPerm: '', fQ: '' };

  function load(host) {
    Crm.call('crm.employees.list', {}).then(function (d) {
      render(host, d.rows || [], d.depts || []);
    }, function () {
      host.innerHTML = '';
      Crm.head(host, 'تعریف حساب کاربری', 'ساخت و مدیریت حساب‌های پرسنل');
      host.appendChild(Crm.el('div', 'crm-banner err', 'بارگذاری حساب‌ها ناموفق بود.'));
    });
  }

  /* the personnel pick-list (driven by dept + search) */
  function loadPersonPick(host) {
    var params = { q: st.q };
    if (st.dept && st.dept !== '__none__') params.deptId = st.dept;
    Crm.call('crm.persons.list', params).then(function (d) {
      var rows = d.rows || [];
      if (st.dept === '__none__') {
        var f = [];
        for (var i = 0; i < rows.length; i++) if (!rows[i].deptId) f.push(rows[i]);
        rows = f;
      }
      var box = Crm.$('accPersonList');
      if (!box) return;
      if (!rows.length) {
        box.innerHTML = '<div class="crm-empty-line">پرسنلی یافت نشد — ابتدا در «تعریف پرسنل» معرفی کنید.</div>';
        return;
      }
      var h = '';
      for (var i = 0; i < rows.length; i++) {
        var p = rows[i];
        var sel = st.picked && st.picked.code === p.code;
        h += '<button type="button" class="crm-pick' + (sel ? ' sel' : '') + (p.username ? ' has-acc' : '') + '" data-code="' + Crm.esc(p.code) + '">' +
          '<span class="crm-codechip">' + Crm.esc(p.code) + '</span>' +
          '<span class="crm-pick-name"><b>' + Crm.esc((p.firstName || '') + ' ' + (p.lastName || '')) + '</b>' +
          '<small>' + Crm.esc(p.roleLabel || '') + (p.deptName ? ' — ' + Crm.esc(p.deptName) : ' — در حالت تعلیق') + '</small></span>' +
          (p.username ? '<span class="crm-pill on">' + Crm.esc(p.username) + '</span>' : '<span class="crm-pill off">بدون حساب</span>') +
        '</button>';
      }
      box.innerHTML = h;
      var items = box.querySelectorAll('.crm-pick');
      for (var k = 0; k < items.length; k++) {
        (function (btn) {
          btn.onclick = function () {
            var code = btn.getAttribute('data-code');
            for (var j = 0; j < rows.length; j++) if (rows[j].code === code) st.picked = rows[j];
            if (st.picked && st.picked.username)
              Crm.toast('این پرسنل قبلاً حساب کاربری دارد: «' + st.picked.username + '»', 'err');
            renderPick(host, rows);
          };
        })(items[k]);
      }
    }, function () {});
  }
  function renderPick(host, rows) {
    loadPersonPick(host);   /* re-render list with the new selection */
    var info = Crm.$('accPickedInfo');
    if (info) {
      if (st.picked) {
        info.innerHTML = 'پرسنل انتخاب‌شده: <b>' + Crm.esc((st.picked.firstName || '') + ' ' + (st.picked.lastName || '')) +
          '</b> <span class="crm-codechip">' + Crm.esc(st.picked.code) + '</span>' +
          (st.picked.username ? ' <span class="crm-pill on">دارای حساب: ' + Crm.esc(st.picked.username) + '</span>' : '');
      } else info.innerHTML = 'هنوز پرسنلی انتخاب نشده است.';
    }
    var dis = !st.picked || !!st.picked.username;
    var u = Crm.$('accUser'), ps = Crm.$('accPass'), sv = Crm.$('accSave');
    if (u) u.disabled = dis; if (ps) ps.disabled = dis; if (sv) sv.disabled = dis;
  }

  function render(host, users, depts) {
    host.innerHTML = '';
    Crm.head(host, 'تعریف حساب کاربری', 'ساخت حساب برای پرسنل + مدیریت دسترسی‌ها');

    /* ================= Card 1: ساخت حساب ================================== */
    var c1 = Crm.el('div', 'crm-card');
    c1.innerHTML = '<div class="crm-card-title"><span class="dot"></span>ساخت حساب کاربری</div>' +
      '<div class="crm-accmode"><button type="button" class="crm-accmode-btn on" id="accModeP">حساب پرسنل</button>' +
      '<button type="button" class="crm-accmode-btn" id="accModeM">حساب مدیریت</button></div>';
    var form = Crm.el('div', 'crm-form');
    form.innerHTML =
      /* personnel mode */
      '<div class="crm-field accmode-p"><label class="crm-label">بخش</label>' +
        '<select class="crm-select" id="accDept">' + deptFilterOpts(depts) + '</select></div>' +
      '<div class="crm-field accmode-p"><label class="crm-label">جستجو (کد پرسنلی / نام / کد ملی)</label>' +
        '<input class="crm-input" id="accQ" placeholder="تایپ کنید — لیست زنده به‌روز می‌شود…" /></div>' +
      '<div class="crm-field full accmode-p"><div class="crm-picklist" id="accPersonList"></div></div>' +
      '<div class="crm-field full accmode-p"><div class="crm-banner" id="accPickedInfo">هنوز پرسنلی انتخاب نشده است.</div></div>' +
      /* management mode (kept from the old «کاربران» page — nothing is lost) */
      '<div class="crm-field accmode-m" style="display:none"><label class="crm-label">نام کامل</label>' +
        '<input class="crm-input" id="accFullM" placeholder="مثلاً: مدیر مالی" /></div>' +
      /* shared */
      '<div class="crm-field"><label class="crm-label">نام کاربری</label><input class="crm-input c-mono" id="accUser" disabled /></div>' +
      '<div class="crm-field"><label class="crm-label">رمز عبور</label><input class="crm-input" id="accPass" type="text" disabled /></div>' +
      '<div class="crm-field full accmode-p"><label class="crm-label">دسترسی‌ها (تیک ندارد = آن قسمت برای این حساب نمایش داده نمی‌شود)</label>' +
        '<div class="crm-permrow" id="accPerms">' + permChecks(null) + '</div></div>';
    c1.appendChild(form);
    var f1 = Crm.el('div', 'crm-modal-foot');
    f1.style.padding = '0';
    f1.innerHTML = '<span class="spacer"></span><button class="crm-btn primary" id="accSave" disabled>ساخت حساب کاربری</button>';
    c1.appendChild(f1);
    host.appendChild(c1);

    /* ================= Card 2: حساب‌های موجود ============================== */
    var c2 = Crm.el('div', 'crm-card');
    c2.innerHTML = '<div class="crm-card-title"><span class="dot"></span>حساب‌های کاربری</div>';
    var tb2 = Crm.el('div', 'crm-toolbar');
    var fDept = Crm.el('div', 'crm-search'); fDept.style.maxWidth = '190px';
    fDept.innerHTML = '<select class="crm-select" id="afDept">' + deptFilterOpts(depts) + '</select>';
    var fPerm = Crm.el('div', 'crm-search'); fPerm.style.maxWidth = '170px';
    fPerm.innerHTML = '<select class="crm-select" id="afPerm">' +
      '<option value="">همه دسترسی‌ها</option>' +
      '<option value="full">دسترسی کامل</option>' +
      '<option value="admission">پذیرش بیمار</option><option value="worklist">کارتابل</option>' +
      '<option value="cashier">گزارش و صندوق</option><option value="settings">تنظیمات</option>' +
      '<option value="none">بدون دسترسی</option></select>';
    var fQ = Crm.el('div', 'crm-search');
    fQ.innerHTML = '<input class="crm-input" id="afQ" placeholder="جستجوی نام کاربری / نام…" />' +
      '<span class="crm-search-ic"><svg viewBox="0 0 24 24" width="16" height="16"><path fill="currentColor" d="M10 2a8 8 0 105.3 14L20 20.7 21.7 19l-4.7-4.7A8 8 0 0010 2zm0 2a6 6 0 110 12 6 6 0 010-12z"/></svg></span>';
    tb2.appendChild(fDept); tb2.appendChild(fPerm); tb2.appendChild(fQ);
    c2.appendChild(tb2);
    c2.appendChild(Crm.table([
      { key: 'i', label: 'ردیف', render: function (r, i) { return Crm.faDigits('' + (i + 1)); } },
      { key: 'username', label: 'نام کاربری', cls: 'c-mono', render: function (r) { return '<b class="crm-codechip">' + Crm.esc(r.username) + '</b>'; } },
      { key: 'fullname', label: 'نام و نام خانوادگی', render: function (r) {
          return '<button class="crm-link" data-person="' + Crm.esc(r.username) + '"><b>' + Crm.esc(r.fullname) + '</b></button>';
        } },
      { key: 'dept', label: 'بخش', render: function (r) {
          return Crm.esc(r.dept || '—');   /* deptId not on the user row; see sheet */
        } },
      { key: 'position', label: 'مقام/سمت', render: function (r) { return Crm.esc(r.position || '—'); } },
      { key: 'perms', label: 'دسترسی‌ها', render: function (r) {
          var lbl = permsLabel(r.perms);
          return Crm.pill(lbl, (!r.perms) ? 'on' : (r.perms === '-' ? 'off' : 'info'));
        } },
      { key: 'online', label: 'وضعیت', render: function (r) { return Crm.pill(r.online ? 'آنلاین' : '—', r.online ? 'on' : 'off'); } },
      { key: 'ops', label: 'عملیات', render: function (r) {
          var b = Crm.el('span');
          b.innerHTML = '<button class="crm-row-btn" data-act="edit">دسترسی/رمز</button>' +
                        '<button class="crm-row-btn danger" data-act="del">حذف</button>';
          b.childNodes[0].onclick = function () { openPermsModal(host, r); };
          b.childNodes[1].onclick = function () { del(host, r); };
          return b;
        } }
    ], filterUsers(users, depts)));
    host.appendChild(c2);

    /* ================= Card 3: بخش‌های سازمانی (kept from the old page) ===== */
    var c3 = Crm.el('div', 'crm-card');
    c3.innerHTML = '<div class="crm-card-title"><span class="dot"></span>بخش‌های سازمانی</div>';
    var dtb = Crm.el('div', 'crm-toolbar');
    var dsearch = Crm.el('div', 'crm-search');
    dsearch.style.maxWidth = '240px';
    dsearch.innerHTML = '<input class="crm-input" id="deptName" placeholder="نام بخش جدید (مثلاً: آزمایشگاه)" />';
    dtb.appendChild(dsearch);
    dtb.appendChild(Crm.el('div', 'spacer', ''));
    var dAdd = Crm.el('button', 'crm-btn outline', 'افزودن بخش');
    dtb.appendChild(dAdd);
    c3.appendChild(dtb);
    c3.appendChild(Crm.table([
      { key: 'i', label: 'ردیف', render: function (r, i) { return Crm.faDigits('' + (i + 1)); } },
      { key: 'name', label: 'نام بخش', render: function (r) { return '<b>' + Crm.esc(r.name) + '</b>'; } },
      { key: 'manager', label: 'مدیر بخش', render: function (r) { return Crm.esc(r.manager || '—'); } },
      { key: 'ops', label: 'عملیات', render: function (r) {
          var b = Crm.el('span');
          b.innerHTML = '<button class="crm-row-btn" data-act="info">اطلاعات بخش</button>' +
                        '<button class="crm-row-btn danger" data-act="del">حذف</button>';
          b.childNodes[0].onclick = function () { Crm.viewDeptInfo(r.id); };
          b.childNodes[1].onclick = function () {
            Crm.confirm('حذف بخش «' + r.name + '»؟', function () {
              Crm.call('crm.depts.delete', { id: r.id }).then(function () { Crm.toast('بخش حذف شد.', 'ok'); load(host); },
                function () { Crm.toast('حذف ناموفق بود.', 'err'); });
            }, { danger: true });
          };
          return b;
        } }
    ], depts));
    host.appendChild(c3);

    /* wire card 1 */
    Crm.$('accDept').onchange = function () { st.dept = this.value; st.picked = null; loadPersonPick(host); renderPickInfoOnly(host); };
    var qT = null;
    Crm.$('accQ').oninput = function () {
      var v = this.value;
      if (qT) clearTimeout(qT);
      qT = setTimeout(function () { st.q = v; loadPersonPick(host); }, 240);
    };
    Crm.$('accSave').onclick = function () { createAccount(host); };
    /* mode toggle: personnel-linked (default) vs direct management account */
    var mgmtMode = false;
    function setMode(mgmt) {
      mgmtMode = mgmt;
      st.picked = null;
      Crm.$('accModeP').className = 'crm-accmode-btn' + (mgmt ? '' : ' on');
      Crm.$('accModeM').className = 'crm-accmode-btn' + (mgmt ? ' on' : '');
      var ps = host.querySelectorAll('.accmode-p'), ms = host.querySelectorAll('.accmode-m'), i;
      for (i = 0; i < ps.length; i++) ps[i].style.display = mgmt ? 'none' : '';
      for (i = 0; i < ms.length; i++) ms[i].style.display = mgmt ? '' : 'none';
      var u = Crm.$('accUser'), ps2 = Crm.$('accPass'), sv = Crm.$('accSave');
      if (u) u.disabled = false; if (ps2) ps2.disabled = false; if (sv) sv.disabled = false;
      if (!mgmt) renderPick(host);
      var info = Crm.$('accPickedInfo');
      if (info && mgmt) info.innerHTML = 'حساب مدیریت مستقیماً با نام کامل ساخته می‌شود (بدون پرسنل).';
    }
    Crm.$('accModeP').onclick = function () { setMode(false); };
    Crm.$('accModeM').onclick = function () { setMode(true); };
    c1._setMgmt = function () { return mgmtMode; };
    /* wire card 2 filters */
    Crm.$('afDept').onchange = function () { st.fDept = this.value; load(host); };
    Crm.$('afPerm').onchange = function () { st.fPerm = this.value; load(host); };
    var fT = null;
    Crm.$('afQ').oninput = function () { var v = this.value; if (fT) clearTimeout(fT); fT = setTimeout(function () { st.fQ = v; load(host); }, 240); };
    /* wire card 3 */
    dAdd.onclick = function () {
      var nm = Crm.$('deptName').value;
      if (!nm) { Crm.toast('نام بخش الزامی است.', 'err'); return; }
      Crm.call('crm.depts.save', { name: nm }).then(function () { Crm.toast('بخش اضافه شد.', 'ok'); load(host); },
        function () { Crm.toast('افزودن بخش ناموفق بود.', 'err'); });
    };
    /* person links in the accounts table */
    var pl = host.querySelectorAll('[data-person]');
    for (var i = 0; i < pl.length; i++) {
      (function (a) {
        a.onclick = function () {
          var un = a.getAttribute('data-person');
          /* find the linked personnel record by username, then open its sheet */
          Crm.call('crm.persons.list', { q: '' }).then(function (d) {
            var rows = d.rows || [], j, hit = null;
            for (j = 0; j < rows.length; j++) if (rows[j].username === un) { hit = rows[j]; break; }
            if (hit && Crm.viewPerson) Crm.viewPerson(hit.code);
            else Crm.alert('برای این حساب پرسنلی ثبت نشده است (حساب قدیمی).');
          });
        };
      })(pl[i]);
    }
    /* first pick-list fill */
    loadPersonPick(host);
  }
  function renderPickInfoOnly(host) {
    var info = Crm.$('accPickedInfo');
    if (info && !st.picked) info.innerHTML = 'هنوز پرسنلی انتخاب نشده است.';
  }

  function deptFilterOpts(depts) {
    var o = '<option value="">همه بخش‌ها</option><option value="__none__">در حالت تعلیق</option>';
    for (var i = 0; i < depts.length; i++)
      o += '<option value="' + Crm.esc(depts[i].id) + '">' + Crm.esc(depts[i].name) + '</option>';
    return o;
  }
  function permChecks(perms) {
    /* empty/absent perms = full access → all boxes ticked; "-" = none ticked */
    var all = (perms == null || perms === '');
    var h = '', i;
    for (i = 0; i < PERMS.length; i++) {
      var on = all || (perms !== '-' && (',' + (perms || '') + ',').indexOf(',' + PERMS[i].key + ',') >= 0);
      h += '<label class="crm-check crm-perm"><input type="checkbox" data-perm="' + PERMS[i].key + '"' + (on ? ' checked' : '') + ' />' +
           '<b>' + PERMS[i].label + '</b><small>' + PERMS[i].hint + '</small></label>';
    }
    return h;
  }
  function readPermChecks(containerId) {
    /* v1.79.0 bugfix: read ONLY the given container — the page form (#accPerms)
       and the edit modal (#pmPerms) can coexist in the DOM, and mixing their
       ticks corrupts both. */
    var root = document.getElementById(containerId);
    if (!root) return '';
    var boxes = root.querySelectorAll('input[type=checkbox]');
    var keys = [], i, total = 0;
    for (i = 0; i < boxes.length; i++) { total++; if (boxes[i].checked) keys.push(boxes[i].getAttribute('data-perm')); }
    if (keys.length === total) return '';      /* all ticked = full access */
    if (!keys.length) return '-';              /* none ticked = NO access    */
    return keys.join(',');
  }

  function filterUsers(users, depts) {
    var out = [], i;
    var fq = (st.fQ || '').toLowerCase();
    for (i = 0; i < users.length; i++) {
      var u = users[i];
      if (st.fDept) {
        var dName = '';
        for (var k = 0; k < depts.length; k++) if (depts[k].id === st.fDept) dName = depts[k].name;
        if (st.fDept === '__none__') { if (u.dept) continue; }
        else if (u.dept !== dName) continue;
      }
      if (st.fPerm === 'full' && u.perms) continue;
      if (st.fPerm === 'none' && u.perms !== '-') continue;
      if (st.fPerm && st.fPerm !== 'full' && st.fPerm !== 'none') {
        if (!u.perms || (',' + u.perms + ',').indexOf(',' + st.fPerm + ',') < 0) continue;
      }
      if (fq && (('' + u.username).toLowerCase().indexOf(fq) < 0) &&
          (('' + u.fullname).toLowerCase().indexOf(fq) < 0)) continue;
      out.push(u);
    }
    return out;
  }

  function createAccount(host) {
    var un = Crm.$('accUser').value, pw = Crm.$('accPass').value;
    /* management mode: direct account (role=1), no personnel link — the same
       capability the old «کاربران» page had, so nothing is lost. */
    var modeBtn = Crm.$('accModeM');
    var mgmt = modeBtn && (' ' + modeBtn.className + ' ').indexOf(' on ') >= 0;
    if (mgmt) {
      var full = Crm.$('accFullM').value;
      if (!full) { Crm.toast('نام کامل الزامی است.', 'err'); return; }
      if (!un || !pw) { Crm.toast('نام کاربری و رمز عبور الزامی است.', 'err'); return; }
      Crm.call('crm.employees.save', {
        username: un, fullname: full, dept: '', role: 1, password: pw
      }).then(function (d) {
        if (d && d.ok === false) { Crm.toast(d.err || 'ساخت حساب ناموفق بود.', 'err'); return; }
        Crm.toast('حساب مدیریت «' + un + '» ساخته شد.', 'ok');
        load(host);
      }, function () { Crm.toast('ساخت حساب ناموفق بود.', 'err'); });
      return;
    }
    if (!st.picked) { Crm.toast('ابتدا پرسنل را از لیست انتخاب کنید.', 'err'); return; }
    if (!un || !pw) { Crm.toast('نام کاربری و رمز عبور الزامی است.', 'err'); return; }
    Crm.call('crm.accounts.create', {
      personCode: st.picked.code, username: un, password: pw, perms: readPermChecks('accPerms')
    }).then(function (d) {
      if (d && d.ok === false) { Crm.toast(d.err || 'ساخت حساب ناموفق بود.', 'err'); return; }
      Crm.toast('حساب «' + un + '» ساخته شد.', 'ok');
      st.picked = null; st.q = '';
      load(host);
    }, function () { Crm.toast('ساخت حساب ناموفق بود.', 'err'); });
  }

  /* edit access ticks + optional password reset (username immutable) */
  function openPermsModal(host, u) {
    var m = Crm.modal('دسترسی و رمز — ' + u.username, null);
    m.body.innerHTML =
      '<div class="crm-form">' +
      '<div class="crm-field full"><label class="crm-label">دسترسی‌ها</label>' +
        '<div class="crm-permrow" id="pmPerms">' + permChecks(u.perms) + '</div></div>' +
      '<div class="crm-field full"><label class="crm-label">رمز عبور جدید (خالی = بدون تغییر)</label>' +
        '<input class="crm-input" id="pmPass" type="text" /></div>' +
      '<div class="crm-field full"><small class="crm-hint">نام کاربری «' + Crm.esc(u.username) +
        '» ثابت است؛ رمز هرگز نمایش داده نمی‌شود.</small></div>' +
      '</div>';
    var foot = Crm.el('div', 'crm-modal-foot');
    foot.innerHTML = '<button class="crm-btn ghost" id="mCancel">انصراف</button><button class="crm-btn primary" id="mSave">ذخیره</button>';
    m.card.appendChild(foot);
    Crm.$('mCancel').onclick = m.close;
    Crm.$('mSave').onclick = function () {
      Crm.call('crm.accounts.update', {
        username: u.username, password: Crm.$('pmPass').value, perms: readPermChecks('pmPerms')
      }).then(function (d) {
        if (d && d.ok === false) { Crm.toast(d.err || 'ناموفق', 'err'); return; }
        Crm.toast('حساب به‌روزرسانی شد.', 'ok'); m.close(); load(host);
      }, function () { Crm.toast('به‌روزرسانی ناموفق بود.', 'err'); });
    };
  }

  function del(host, u) {
    Crm.confirm('حذف حساب «' + u.username + '»؟ (پرسنل باقی می‌ماند و بعداً می‌تواند حساب بگیرد)', function () {
      Crm.call('crm.employees.delete', { username: u.username }).then(function () {
        Crm.toast('حساب حذف شد.', 'ok'); load(host);
      }, function () { Crm.toast('حذف ناموفق بود.', 'err'); });
    }, { danger: true });
  }

  Crm.pages.employees = {
    title: 'تعریف حساب کاربری',
    render: function (host) { st.picked = null; load(host); }
  };
})(window);
