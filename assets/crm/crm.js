/* ============================================================================
   crm.js — CRM Management panel controller (v1.70.0)

   ES5-ONLY (no const/let/arrow/template literals/class) so it parses and runs
   on BOTH WebView2 (Chromium) and the MSHTML/Trident (IE11) fallback that ships
   with every Windows. Uses the shared shell runtime (common.js) as its single
   C++<->JS transport: AzBridge.call(verb, payload) -> window.external.azCall /
   chrome.webview.postMessage. Page modules register on window.Crm.pages and are
   routed by Crm.nav(). RTL is handled with direction:rtl (no layout flipping).
   ============================================================================ */
(function (global) {
  'use strict';

  var Crm = {
    pages: {},          /* pageId -> {title, render(host)} */
    state: { page: 'dashboard', data: {} }
  };

  /* ---- tiny helpers ----------------------------------------------------- */
  function esc(s) {
    s = (s == null) ? '' : ('' + s);
    return s.replace(/&/g, '&amp;').replace(/</g, '&lt;')
            .replace(/>/g, '&gt;').replace(/"/g, '&quot;');
  }
  function faDigits(s) {
    s = '' + (s == null ? '' : s);
    var map = ['۰','۱','۲','۳','۴','۵','۶','۷','۸','۹'];
    var out = '';
    for (var i = 0; i < s.length; i++) {
      var c = s.charAt(i);
      out += (c >= '0' && c <= '9') ? map[+c] : c;
    }
    return out;
  }
  function enDigits(s) {
    s = '' + (s == null ? '' : s);
    var out = '';
    for (var i = 0; i < s.length; i++) {
      var c = s.charCodeAt(i);
      if (c >= 0x06F0 && c <= 0x06F9) out += String.fromCharCode(c - 0x06F0 + 48);      /* ۰-۹ */
      else if (c >= 0x0660 && c <= 0x0669) out += String.fromCharCode(c - 0x0660 + 48);  /* ٠-٩ */
      else out += s.charAt(i);
    }
    return out;
  }
  function fmtMoney(n) {
    n = +n || 0;
    var neg = n < 0; if (neg) n = -n;
    var s = '' + Math.floor(n);
    var grp = '';
    while (s.length > 3) { grp = ',' + s.substr(s.length - 3) + grp; s = s.substr(0, s.length - 3); }
    return (neg ? '-' : '') + s + grp;
  }
  function el(tag, cls, html) {
    var e = document.createElement(tag);
    if (cls) e.className = cls;
    if (html != null) e.innerHTML = html;
    return e;
  }
  function $(id) { return document.getElementById(id); }

  Crm.esc = esc;
  Crm.faDigits = faDigits;
  Crm.enDigits = enDigits;
  Crm.fmtMoney = fmtMoney;
  Crm.el = el;
  Crm.$ = $;

  /* ---- bridge call ------------------------------------------------------ */
  Crm.call = function (verb, payload) {
    return global.AzBridge.call(verb, payload || {});
  };
  Crm.toast = function (text, kind, ms) {
    if (global.AzUi) global.AzUi.toast(text, kind, ms);
  };

  /* ---- navigation ------------------------------------------------------- */
  Crm.nav = function (pageId) {
    var p = Crm.pages[pageId];
    if (!p) { Crm.toast('صفحه یافت نشد: ' + pageId, 'err'); return; }
    Crm.state.page = pageId;
    /* Active state is toggled on every nav target that carries data-page —
       both the (retired) sidebar items and the new quick-access tiles, so the
       redesign needs no special-casing here. */
    var items = document.querySelectorAll('[data-page]');
    for (var i = 0; i < items.length; i++) {
      var on = items[i].getAttribute('data-page') === pageId;
      var base = items[i].className.replace(/\s+active/g, '');
      items[i].className = base + (on ? ' active' : '');
    }
    var host = $('crmPage');
    if (!host) return;
    host.innerHTML = '';
    try { p.render(host); }
    catch (e) { if (global.console) console.error(e); host.innerHTML = '<div class="crm-banner err">خطا در بارگذاری صفحه.</div>'; }
    host.scrollTop = 0;
    /* keep the top-bar search in sync with whatever page we land on */
    var s = $('navSearch');
    if (s && pageId !== 'dashboard') { s.value = ''; }
  };

  /* jump back to the categorized home (dashboard) — used by the top-bar logo */
  Crm.home = function () { Crm.nav('dashboard'); };

  /* ---- shared page header builder --------------------------------------- */
  Crm.head = function (host, title, sub) {
    var h = el('div', 'crm-page-head');
    h.appendChild(el('h2', 'crm-page-title', esc(title)));
    if (sub) h.appendChild(el('span', 'crm-page-sub', esc(sub)));
    host.appendChild(h);
    return h;
  };

  /* ---- simple modal helper (returns the card + a close fn) -------------- */
  Crm.modal = function (title, bodyHtml) {
    var bg = el('div', 'crm-modal-bg');
    var card = el('div', 'crm-modal');
    var head = el('div', 'crm-modal-head');
    head.appendChild(el('div', 'crm-modal-title', esc(title)));
    var closeBtn = el('button', 'crm-modal-close', '×');
    head.appendChild(closeBtn);
    card.appendChild(head);
    var body = el('div', 'crm-modal-body');
    if (bodyHtml != null) body.innerHTML = bodyHtml;
    card.appendChild(body);
    bg.appendChild(card);
    document.body.appendChild(bg);
    function close() { if (bg.parentNode) bg.parentNode.removeChild(bg); }
    closeBtn.onclick = close;
    bg.onclick = function (ev) { if (ev.target === bg) close(); };
    Crm._lastModalBody = body;
    return { card: card, body: body, close: close };
  };

  /* ---- themed confirm/alert dialogs (replace browser confirm()/alert()) --
     The MSHTML/WebView shell blocks native dialogs in some configs and they
     never match the panel theme, so every delete/confirm flow routes through
     these. confirm(msg, onYes, opts) calls onYes() only when the operator
     picks «بله»; alert(msg, opts) is a single-button notice. Both return the
     modal handle (with .close) so callers can dismiss programmatically. */
  Crm.confirm = function (msg, onYes, opts) {
    opts = opts || {};
    var m = Crm.modal(opts.title || 'تأیید عملیات', null);
    var body = m.body;
    var ic = opts.danger
      ? '<svg viewBox="0 0 24 24" width="34" height="34"><path fill="#dc2626" d="M12 2L1 21h22L12 2zm0 6l6.5 11h-13L12 8zm-1 4v4h2v-4h-2zm0 5v2h2v-2h-2z"/></svg>'
      : '<svg viewBox="0 0 24 24" width="34" height="34"><path fill="#2f6fe4" d="M12 2a10 10 0 100 20 10 10 0 000-20zm1 15h-2v-2h2v2zm0-4h-2V7h2v6z"/></svg>';
    body.innerHTML =
      '<div class="crm-confirm">' +
        '<div class="crm-confirm-ic">' + ic + '</div>' +
        '<div class="crm-confirm-msg">' + esc(msg) + '</div>' +
      '</div>';
    var foot = el('div', 'crm-modal-foot');
    var noBtn = el('button', 'crm-btn ghost', opts.noLabel || 'خیر');
    var yesBtn = el('button', 'crm-btn ' + (opts.danger ? 'danger' : 'primary'), opts.yesLabel || 'بله، تأیید می‌کنم');
    foot.appendChild(noBtn);
    foot.appendChild(yesBtn);
    m.card.appendChild(foot);
    function done(ok) { m.close(); if (ok && typeof onYes === 'function') { try { onYes(); } catch (e) { if (global.console) console.error(e); } } }
    noBtn.onclick = function () { done(false); };
    yesBtn.onclick = function () { done(true); };
    return m;
  };

  Crm.alert = function (msg, opts) {
    opts = opts || {};
    var m = Crm.modal(opts.title || 'اعلان', null);
    var body = m.body;
    body.innerHTML = '<div class="crm-confirm"><div class="crm-confirm-msg">' + esc(msg) + '</div></div>';
    var foot = el('div', 'crm-modal-foot');
    var okBtn = el('button', 'crm-btn primary', opts.okLabel || 'تأیید');
    foot.appendChild(okBtn);
    m.card.appendChild(foot);
    okBtn.onclick = m.close;
    return m;
  };

  /* ---- table helper ----------------------------------------------------- */
  Crm.table = function (columns, rows) {
    /* columns: [{key,label,cls,render(row)}]; rows: array */
    var wrap = el('div', 'crm-table-wrap');
    var tbl = el('table', 'crm-tbl');
    var thead = el('thead'); var tr = el('tr');
    for (var c = 0; c < columns.length; c++) {
      var th = el('th', columns[c].cls || '', esc(columns[c].label));
      tr.appendChild(th);
    }
    thead.appendChild(tr); tbl.appendChild(thead);
    var tbody = el('tbody');
    if (!rows || !rows.length) {
      var empty = el('tr'); var td = el('td', 'empty', 'موردی یافت نشد');
      td.setAttribute('colspan', '' + columns.length);
      empty.appendChild(td); tbody.appendChild(empty);
    } else {
      for (var r = 0; r < rows.length; r++) {
        var row = rows[r]; var trow = el('tr');
        for (var k = 0; k < columns.length; k++) {
          var col = columns[k];
          var cell = el('td', col.cls || '');
          if (typeof col.render === 'function') {
            var html = col.render(row, r);
            if (html && html.nodeType) cell.appendChild(html);
            else cell.innerHTML = html == null ? '' : html;
          } else {
            cell.innerHTML = esc(row[col.key]);
          }
          trow.appendChild(cell);
        }
        tbody.appendChild(trow);
      }
    }
    tbl.appendChild(tbody); wrap.appendChild(tbl);
    return wrap;
  };

  /* ---- a pill span ------------------------------------------------------ */
  Crm.pill = function (text, kind) {
    return '<span class="crm-pill ' + (kind || 'info') + '">' + esc(text) + '</span>';
  };

  /* ---- init / boot ------------------------------------------------------ */
  function applyInit(d) {
    /* The CRM top bar (clock, user label, theme button) was removed — date/time
       and appearance are owned by the C++ shell settings, so showing them here
       was redundant. We still honour the persisted theme C++ sends on crm.init
       so the page matches the operator's chosen appearance. */
    if (typeof d.theme === 'string') {
      Crm._dark = d.theme === 'dark';
      if (global.AzBoot) global.AzBoot.applyTheme(Crm._dark);
    }
    var badge = $('navMsgBadge');
    if (badge) {
      var n = +d.messages || 0;
      if (n > 0) { badge.style.display = ''; badge.innerHTML = faDigits('' + n); }
      else badge.style.display = 'none';
    }
    Crm.state.data = d;
  }

  function hideLoader() {
    var ldr = $('loader');
    var app = $('app');
    if (app) app.setAttribute('aria-hidden', 'false');
    if (app) app.className = 'crm-app ready';
    if (ldr) {
      ldr.className = 'crm-loader fade';
      setTimeout(function () { if (ldr.parentNode) ldr.parentNode.removeChild(ldr); }, 280);
    }
  }

  function boot() {
    Crm.call('crm.init', {}).then(function (d) {
      applyInit(d || {});
      hideLoader();
      Crm.nav('dashboard');
    }, function (e) {
      if (global.console) console.error(e);
      var lt = $('loaderText');
      if (lt) lt.innerHTML = 'اتصال به پنل مدیریت ناموفق بود.';
      /* still try to show the shell so the operator sees the structure */
      hideLoader();
      Crm.nav('dashboard');
    });
  }

  /* ---- wire static controls --------------------------------------------- */
  function wireNav() {
    /* Every element that carries data-page navigates — covers the new
       quick-access tiles AND any legacy sidebar items. */
    var items = document.querySelectorAll('[data-page]');
    for (var i = 0; i < items.length; i++) {
      (function (btn) {
        btn.onclick = function () { Crm.nav(btn.getAttribute('data-page')); };
      })(items[i]);
    }
    var home = $('navHome');
    if (home) home.onclick = function () { Crm.home(); };
    var backup = $('navBackup');
    if (backup) backup.onclick = function () {
      Crm.toast('در حال باز کردن پشتیبان‌گیری…', 'info');
      Crm.call('crm.backup', {}).then(function () {}, function () { Crm.toast('پشتیبان‌گیری ناموفق بود.', 'err'); });
    };
    /* top-bar global search: jump home and filter the tile grid live */
    var search = $('navSearch');
    if (search) {
      search.onkeyup = function () {
        if (Crm.state.page !== 'dashboard') {
          Crm.nav('dashboard');           /* nav renders the (filtered) home */
        } else {
          var host = $('crmPage');
          if (host) { host.innerHTML = ''; Crm.pages.dashboard.render(host); }
        }
        var s2 = $('navSearch');
        if (s2) s2.focus();
      };
    }
  }

  /* ---- listen for C++ push events --------------------------------------- */
  if (global.AzBridge && global.AzBridge.on) {
    global.AzBridge.on('crm.refresh', function () {
      var p = Crm.pages[Crm.state.page];
      var host = $('crmPage');
      if (p && host && typeof p.render === 'function') {
        host.innerHTML = ''; p.render(host);
      }
    });
  }

  /* ---- start once the shell bridge transport is up ---------------------- */
  if (global.AzBoot) {
    global.AzBoot.ready(function () {
      wireNav();
      boot();
    });
  } else {
    /* standalone dev harness (no native bridge) — still render the shell */
    wireNav();
    hideLoader();
    Crm.nav('dashboard');
  }

  global.Crm = Crm;
})(window);
