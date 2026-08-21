/* ============================================================================
   crm_dialogs.js — CRM themed modal / confirm / alert dialogs (v1.77 module,
   split out of crm.js).

   ES5-ONLY (no const/let/arrow/template literals/class) so it parses and runs
   on BOTH WebView2 (Chromium) and the MSHTML/Trident (IE11) fallback.

   The MSHTML/WebView shell blocks native confirm()/alert() in some configs and
   they never match the panel theme, so every delete/confirm flow routes through
   these. This module attaches Crm.modal / Crm.confirm / Crm.alert onto the
   shared window.Crm object. It must load AFTER crm.js (which creates window.Crm
   and exposes Crm.el / Crm.esc) and BEFORE any page renders — both guaranteed by
   the <script> order in index.html, since these are only ever called from a
   render-time user action, never at load time. No behaviour changed from the
   original inline implementation; the code was moved verbatim.
   ============================================================================ */
(function (global) {
  'use strict';
  var Crm = global.Crm;
  /* local aliases to the helpers crm.js exposes, so the moved body stays verbatim */
  var el = Crm.el, esc = Crm.esc;

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
     confirm(msg, onYes, opts) calls onYes() only when the operator picks
     «بله»; alert(msg, opts) is a single-button notice. Both return the modal
     handle (with .close) so callers can dismiss programmatically. */
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
})(window);
