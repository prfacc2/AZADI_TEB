/* ============================================================================
   messages.js — Cartable / inbox (کارتابل). ES5-only.
   List / send / mark-seen / delete / pin messages via crm.messages.* verbs.
   The on-disk store is owned by C++ (loadMessages / pushMessageT /
   pinMessage / seenOneMessage / deleteOneMessage); the list is newest-first and
   idx matches the C++ indexNewestFirst convention.
   ============================================================================ */
(function (global) {
  'use strict';
  var Crm = global.Crm;

  function load(host) {
    Crm.call('crm.messages.list', {}).then(function (d) {
      render(host, d.rows || [], d.unseen || 0);
      /* mark all seen shortly after viewing */
      Crm.call('crm.messages.seen', {}).then(function () {
        var badge = Crm.$('navMsgBadge');
        if (badge) badge.style.display = 'none';
      });
    }, function () {
      host.innerHTML = '';
      Crm.head(host, 'کارتابل', 'صندوق پیام‌های دریافتی');
      host.appendChild(Crm.el('div', 'crm-banner err', 'بارگذاری پیام‌ها ناموفق بود.'));
    });
  }

  function typeLabel(t) {
    if (t === 2) return 'بحرانی';
    if (t === 1) return 'فوری';
    return 'عادی';
  }
  function typeKind(t) {
    if (t === 2) return 'off';
    if (t === 1) return 'info';
    return 'on';
  }

  function render(host, rows, unseen) {
    host.innerHTML = '';
    Crm.head(host, 'کارتابل', 'صندوق پیام‌های دریافتی' + (unseen ? ' (' + Crm.faDigits('' + unseen) + ' جدید)' : ''));

    /* compose card */
    var cc = Crm.el('div', 'crm-card');
    cc.innerHTML = '<div class="crm-card-title"><span class="dot"></span>ارسال پیام جدید</div>';
    var form = Crm.el('div', 'crm-form');
    form.innerHTML =
      '<div class="crm-field"><label class="crm-label">گیرنده</label>' +
        '<select class="crm-select" id="mTo"><option value="*">همه (پخش)</option></select></div>' +
      '<div class="crm-field"><label class="crm-label">اولویت</label>' +
        '<select class="crm-select" id="mType"><option value="0">عادی</option><option value="1">فوری</option><option value="2">بحرانی</option></select></div>' +
      '<div class="crm-field full"><label class="crm-label">متن پیام</label>' +
        '<textarea class="crm-textarea" id="mText"></textarea></div>';
    cc.appendChild(form);
    var foot = Crm.el('div', 'crm-modal-foot');
    foot.innerHTML = '<button class="crm-btn primary" id="mSend">ارسال</button>';
    cc.appendChild(foot);
    host.appendChild(cc);

    /* recipient select: known users would need a verb; keep broadcast + free text */
    var toSel = Crm.$('mTo');
    var toWrap = toSel.parentNode;
    var free = Crm.el('input', 'crm-input');
    free.id = 'mToFree';
    free.setAttribute('placeholder', 'یا نام کاربری گیرنده…');
    free.style.marginTop = '6px';
    toWrap.appendChild(free);

    Crm.$('mSend').onclick = function () {
      var to = Crm.$('mToFree').value || Crm.$('mTo').value || '*';
      var text = Crm.$('mText').value;
      if (!text) { Crm.toast('متن پیام خالی است.', 'err'); return; }
      Crm.call('crm.messages.send', { to: to, text: text, type: +Crm.$('mType').value }).then(function () {
        Crm.toast('پیام ارسال شد.', 'ok'); load(host);
      }, function () { Crm.toast('ارسال ناموفق بود.', 'err'); });
    };

    /* list card */
    var lc = Crm.el('div', 'crm-card');
    lc.innerHTML = '<div class="crm-card-title"><span class="dot"></span>پیام‌های دریافتی</div>';
    if (!rows.length) {
      lc.appendChild(Crm.el('div', 'crm-banner info', 'پیامی در کارتابل نیست.'));
    } else {
      for (var i = 0; i < rows.length; i++) {
        lc.appendChild(msgRow(host, rows[i]));
      }
    }
    host.appendChild(lc);
  }

  function msgRow(host, m) {
    var cls = 'crm-msg';
    if (!m.seen) cls += ' unseen';
    if (m.pinned) cls += ' pinned';
    var row = Crm.el('div', cls);
    var body = Crm.el('div', 'crm-msg-body');
    body.innerHTML =
      '<div><span class="crm-msg-from">' + Crm.esc(m.from) + '</span>' +
      '<span class="crm-msg-time">' + Crm.esc(m.time || '') + '</span> ' +
      Crm.pill(typeLabel(m.type), typeKind(m.type)) + '</div>' +
      '<div class="crm-msg-text">' + Crm.esc(m.text) + '</div>';
    row.appendChild(body);
    var acts = Crm.el('div', 'crm-msg-actions');
    acts.innerHTML =
      '<button class="crm-row-btn" data-act="pin">' + (m.pinned ? 'برداشتن پین' : 'پین') + '</button>' +
      '<button class="crm-row-btn danger" data-act="del">حذف</button>';
    acts.childNodes[0].onclick = function () {
      Crm.call('crm.messages.pin', { idx: m.idx, pin: !m.pinned }).then(function () { load(host); },
        function () { Crm.toast('عملیات ناموفق بود.', 'err'); });
    };
    acts.childNodes[1].onclick = function () {
      Crm.confirm('حذف این پیام؟', function () {
        Crm.call('crm.messages.delete', { idx: m.idx }).then(function () { Crm.toast('پیام حذف شد.', 'ok'); load(host); },
          function () { Crm.toast('حذف ناموفق بود.', 'err'); });
      }, { danger: true });
    };
    row.appendChild(acts);
    return row;
  }

  Crm.pages.messages = {
    title: 'کارتابل',
    render: function (host) { load(host); }
  };
})(window);
