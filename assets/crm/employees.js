/* ============================================================================
   employees.js — Employees/Users (کاربر) management. ES5-only.
   List / add / edit / delete system users + manage department categories. The
   on-disk stores (data\users.dat, data\depts.dat) are owned by C++ (addUser /
   updateUserAccount / removeUser / addDept / removeDept); the UI sends fields.
   ============================================================================ */
(function (global) {
  'use strict';
  var Crm = global.Crm;

  function load(host) {
    Crm.call('crm.employees.list', {}).then(function (d) {
      render(host, d.rows || [], d.depts || []);
    }, function () {
      host.innerHTML = '';
      Crm.head(host, 'کاربران', 'مدیریت کاربران سیستم و بخش‌ها');
      host.appendChild(Crm.el('div', 'crm-banner err', 'بارگذاری کاربران ناموفق بود.'));
    });
  }

  function deptOptions(depts, sel) {
    var o = '<option value="">— بدون بخش —</option>';
    for (var i = 0; i < depts.length; i++) {
      o += '<option value="' + Crm.esc(depts[i].name) + '"' + (depts[i].name === sel ? ' selected' : '') + '>' +
           Crm.esc(depts[i].name) + '</option>';
    }
    return o;
  }

  function render(host, users, depts) {
    host.innerHTML = '';
    Crm.head(host, 'کاربران', 'مدیریت کاربران سیستم و بخش‌ها');

    /* users card */
    var uc = Crm.el('div', 'crm-card');
    uc.innerHTML = '<div class="crm-card-title"><span class="dot"></span>کاربران سیستم</div>';
    var tb = Crm.el('div', 'crm-toolbar');
    tb.appendChild(Crm.el('div', 'spacer', ''));
    var addBtn = Crm.el('button', 'crm-btn primary', '+ افزودن کاربر');
    tb.appendChild(addBtn);
    uc.appendChild(tb);
    uc.appendChild(Crm.table([
      { key: 'i', label: 'ردیف', render: function (r, i) { return Crm.faDigits('' + (i + 1)); } },
      { key: 'username', label: 'نام کاربری', cls: 'c-mono', render: function (r) { return Crm.esc(r.username); } },
      { key: 'fullname', label: 'نام کامل', render: function (r) { return '<b>' + Crm.esc(r.fullname) + '</b>'; } },
      { key: 'dept', label: 'بخش', render: function (r) { return Crm.esc(r.dept || '—'); } },
      { key: 'role', label: 'نقش', render: function (r) { return Crm.esc(r.role === 1 ? 'مدیریت' : 'پذیرش'); } },
      { key: 'online', label: 'آنلاین', render: function (r) { return Crm.pill(r.online ? 'آنلاین' : '—', r.online ? 'on' : 'off'); } },
      { key: 'ops', label: 'عملیات', render: function (r) {
          var b = Crm.el('span');
          b.innerHTML = '<button class="crm-row-btn" data-act="edit">ویرایش</button>' +
                        '<button class="crm-row-btn danger" data-act="del">حذف</button>';
          b.childNodes[0].onclick = function () { openModal(host, r, depts); };
          b.childNodes[1].onclick = function () { del(host, r); };
          return b;
        } }
    ], users));
    host.appendChild(uc);

    /* departments card */
    var dc = Crm.el('div', 'crm-card');
    dc.innerHTML = '<div class="crm-card-title"><span class="dot"></span>بخش‌های سازمانی</div>';
    var dtb = Crm.el('div', 'crm-toolbar');
    var dsearch = Crm.el('div', 'crm-search');
    dsearch.style.maxWidth = '240px';
    dsearch.innerHTML = '<input class="crm-input" id="deptName" placeholder="نام بخش جدید" />';
    dtb.appendChild(dsearch);
    dtb.appendChild(Crm.el('div', 'spacer', ''));
    var dAdd = Crm.el('button', 'crm-btn outline', 'افزودن بخش');
    dtb.appendChild(dAdd);
    dc.appendChild(dtb);
    dc.appendChild(Crm.table([
      { key: 'i', label: 'ردیف', render: function (r, i) { return Crm.faDigits('' + (i + 1)); } },
      { key: 'name', label: 'نام بخش', render: function (r) { return '<b>' + Crm.esc(r.name) + '</b>'; } },
      { key: 'manager', label: 'مدیر بخش', render: function (r) { return Crm.esc(r.manager || '—'); } },
      { key: 'ops', label: 'عملیات', render: function (r) {
          var b = Crm.el('span');
          b.innerHTML = '<button class="crm-row-btn danger" data-act="del">حذف</button>';
          b.childNodes[0].onclick = function () {
            Crm.confirm('حذف بخش «' + r.name + '»؟', function () {
              Crm.call('crm.depts.delete', { id: r.id }).then(function () { Crm.toast('بخش حذف شد.', 'ok'); load(host); },
                function () { Crm.toast('حذف ناموفق بود.', 'err'); });
            }, { danger: true });
          };
          return b;
        } }
    ], depts));
    host.appendChild(dc);

    addBtn.onclick = function () { openModal(host, null, depts); };
    dAdd.onclick = function () {
      var nm = Crm.$('deptName').value;
      if (!nm) { Crm.toast('نام بخش الزامی است.', 'err'); return; }
      Crm.call('crm.depts.save', { name: nm }).then(function () { Crm.toast('بخش اضافه شد.', 'ok'); load(host); },
        function () { Crm.toast('افزودن بخش ناموفق بود.', 'err'); });
    };
  }

  function openModal(host, u, depts) {
    var adding = !u;
    if (!u) u = { role: 0 };
    var m = Crm.modal(adding ? 'افزودن کاربر' : 'ویرایش کاربر', null);
    var body = m.body;
    body.innerHTML =
      '<div class="crm-form">' +
      '<div class="crm-field"><label class="crm-label">نام کاربری</label>' +
        '<input class="crm-input" id="uUser" value="' + Crm.esc(u.username || '') + '" ' + (adding ? '' : 'readonly') + ' /></div>' +
      '<div class="crm-field"><label class="crm-label">نام کامل</label>' +
        '<input class="crm-input" id="uFull" value="' + Crm.esc(u.fullname || '') + '" /></div>' +
      '<div class="crm-field"><label class="crm-label">بخش</label>' +
        '<select class="crm-select" id="uDept">' + deptOptions(depts || [], u.dept) + '</select></div>' +
      '<div class="crm-field"><label class="crm-label">نقش</label>' +
        '<select class="crm-select" id="uRole"><option value="0"' + (u.role === 0 ? ' selected' : '') + '>پذیرش</option>' +
        '<option value="1"' + (u.role === 1 ? ' selected' : '') + '>مدیریت</option></select></div>' +
      '<div class="crm-field full"><label class="crm-label">' + (adding ? 'رمز عبور' : 'رمز عبور جدید (خالی = بدون تغییر)') + '</label>' +
        '<input class="crm-input" id="uPass" type="text" value="" /></div>' +
      '</div>';
    var foot = Crm.el('div', 'crm-modal-foot');
    foot.innerHTML = '<button class="crm-btn ghost" id="mCancel">انصراف</button><button class="crm-btn primary" id="mSave">ذخیره</button>';
    m.card.appendChild(foot);
    Crm.$('mCancel').onclick = m.close;
    Crm.$('mSave').onclick = function () {
      var payload = {
        username: Crm.$('uUser').value,
        fullname: Crm.$('uFull').value,
        dept: Crm.$('uDept').value,
        role: +Crm.$('uRole').value,
        password: Crm.$('uPass').value
      };
      if (!payload.username) { Crm.toast('نام کاربری الزامی است.', 'err'); return; }
      if (!payload.fullname) { Crm.toast('نام کامل الزامی است.', 'err'); return; }
      Crm.call('crm.employees.save', payload).then(function () {
        Crm.toast(adding ? 'کاربر اضافه شد.' : 'کاربر ویرایش شد.', 'ok');
        m.close(); load(host);
      }, function () { Crm.toast('ذخیره ناموفق بود.', 'err'); });
    };
  }

  function del(host, u) {
    Crm.confirm('حذف کاربر «' + u.username + '»؟', function () {
      Crm.call('crm.employees.delete', { username: u.username }).then(function () {
        Crm.toast('کاربر حذف شد.', 'ok'); load(host);
      }, function () { Crm.toast('حذف ناموفق بود.', 'err'); });
    }, { danger: true });
  }

  Crm.pages.employees = {
    title: 'کاربران',
    render: function (host) { load(host); }
  };
})(window);
