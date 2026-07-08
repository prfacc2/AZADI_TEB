/* ============================================================================
   ping.js — page logic for the «آزمون اتصال» demo page (v1.40.0).

   Proves the multi-page embedded-web shell end-to-end:
     · the page was registered in the C++ WebPages_* registry and its assets
       were served through the worker thread pool,
     · the SHARED shell runtime (common.js) is present and AzBridge is wired,
     · a dedicated /api verb (ping) round-trips to src/web_ping_api.cpp.

   ES5-ONLY (must parse on MSHTML/Trident as well as WebView2/Chromium).
   ============================================================================ */
(function (global) {
  'use strict';

  var $ = AzUi.$;
  var _dark = false;

  function setText(id, v) { var e = $(id); if (e) e.innerHTML = AzUi.esc(v); }

  function doPing() {
    var t0 = (new Date()).getTime();
    setText('serverReply', 'در حال ارسال…');
    AzBridge.call('ping', { echo: 'سلام از پوستهٔ چندصفحه‌ای', t: t0 })
      .then(function (res) {
        res = res || {};
        var rtt = (new Date()).getTime() - t0;
        setText('serverReply', res.pong ? ('' + res.pong) : 'بدون پاسخ');
        setText('appVer', res.version ? ('' + res.version) : '—');
        setText('workers', (res.workers != null) ? ('' + res.workers) : '—');
        setText('rtt', rtt + ' میلی‌ثانیه');
        AzUi.toast('اتصال برقرار است ✓', 'ok');
        AzPerf.mark('ping.rtt', rtt);
      })
      .catch(function (err) {
        setText('serverReply', 'خطا: ' + (err && err.message ? err.message : err));
        AzUi.toast('اتصال ناموفق بود', 'err');
        AzBridge.log('error', 'ping failed', { err: '' + (err && err.message) });
      });
  }

  function toggleTheme() {
    _dark = !_dark;
    AzBoot.applyTheme(_dark);
    AzUi.toast(_dark ? 'پوستهٔ تیره' : 'پوستهٔ روشن', 'info', 1400);
  }

  AzBoot.ready(function () {
    setText('bridgeState', 'متصل شد ✓');
    setText('transport', AzBridge.transport() === 'webview' ? 'WebView2' : 'HTTP (MSHTML)');
    var b = $('btnPing'); if (b) b.onclick = doPing;
    var tb = $('btnTheme'); if (tb) tb.onclick = toggleTheme;
    /* fire an initial ping so the page is populated on open. */
    doPing();
  });

})(window);
