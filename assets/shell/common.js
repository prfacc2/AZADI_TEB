/* ============================================================================
   common.js — SHARED multi-page embedded-web SHELL runtime (v1.40.0)

   Loaded by EVERY embedded page BEFORE its page-specific script (and, on the
   admission page, before bridge.js so AzBridge is the single transport). It
   exposes five namespaces on `window`:

     AzBoot   — page identity + theme + one-shot ready() gate
     AzBridge — the ONE C++<->JS IPC layer (WebView2 postMessage OR XHR /api),
                verb namespacing, X-Az-Page request header, in-flight dedup,
                and a client.log / client.metrics uplink
     AzUi     — shared UI helpers (toast host, $-by-id, escape)
     AzNav    — reusable Enter/Tab keyboard navigation over an explicit id order
                (NAV_ORDER), with focusNext / focusPrev, no wrap-around and
                MSHTML-safe <select> handling
     AzPerf   — lightweight page performance counters flushed to client.metrics

   IMPORTANT: ES5-ONLY. No arrow functions, no const/let, no template literals,
   no Map/Set, no fetch, no spread/destructuring — MUST parse on MSHTML/Trident
   (IE11) as well as WebView2 (Chromium). Keep in lock-step with common.css.
   ============================================================================ */
(function (global) {
  'use strict';

  var doc = global.document;

  /* ======================================================================= */
  /*  AzBoot — page identity, theme, ready gate                              */
  /* ======================================================================= */
  var _pageId = '';
  function readPageId() {
    /* <meta name="az-page" content="admission"> declares the page id. */
    var m = doc.getElementsByTagName('meta');
    var i;
    for (i = 0; i < m.length; i++) {
      if (m[i].getAttribute && m[i].getAttribute('name') === 'az-page') {
        var c = m[i].getAttribute('content');
        if (c) return ('' + c);
      }
    }
    return '';
  }

  var _bootCbs = [];
  var _booted = false;
  function fireBoot() {
    if (_booted) return;
    _booted = true;
    var i;
    for (i = 0; i < _bootCbs.length; i++) {
      try { _bootCbs[i](); } catch (e) { if (global.console) console.error(e); }
    }
    _bootCbs = [];
  }

  var AzBoot = {
    page: function () { return _pageId; },
    /* run cb once the bridge transport is up. */
    ready: function (cb) {
      if (typeof cb !== 'function') return;
      if (_booted) { try { cb(); } catch (e) {} return; }
      _bootCbs.push(cb);
    },
    /* toggle the shared dark/light surfaces (class on <html>). */
    applyTheme: function (dark) {
      var h = doc.documentElement;
      if (!h) return;
      if (dark) {
        if (h.className.indexOf('theme-dark') < 0) {
          h.className = (h.className ? h.className + ' ' : '') + 'theme-dark';
        }
      } else {
        h.className = ('' + h.className).replace(/(^|\s)theme-dark(\s|$)/g, ' ');
      }
    }
  };

  /* ======================================================================= */
  /*  AzUi — toast host + tiny DOM helpers                                   */
  /* ======================================================================= */
  function $(id) { return doc.getElementById(id); }

  function ensureToastHost() {
    var host = $('azToastHost');
    if (!host) {
      host = doc.createElement('div');
      host.id = 'azToastHost';
      (doc.body || doc.documentElement).appendChild(host);
    }
    return host;
  }

  var AzUi = {
    $: $,
    esc: function (s) {
      s = (s == null) ? '' : ('' + s);
      return s.replace(/&/g, '&amp;').replace(/</g, '&lt;')
              .replace(/>/g, '&gt;').replace(/"/g, '&quot;');
    },
    /* kind: 'ok' | 'err' | 'warn' | 'info' (default 'info'); ms default 2600 */
    toast: function (text, kind, ms) {
      var host = ensureToastHost();
      var t = doc.createElement('div');
      t.className = 'toast ' + (kind || 'info');
      t.innerHTML = AzUi.esc(text);
      host.appendChild(t);
      /* force reflow so the opacity transition runs on MSHTML too */
      var _f = t.offsetHeight; if (_f) { /* touch */ }
      t.className += ' show';
      var life = (typeof ms === 'number' && ms > 0) ? ms : 2600;
      setTimeout(function () {
        t.className = t.className.replace(/\sshow/, '');
        setTimeout(function () {
          if (t.parentNode) t.parentNode.removeChild(t);
        }, 260);
      }, life);
    }
  };

  /* ======================================================================= */
  /*  AzBridge — the ONE C++<->JS transport                                  */
  /*    · WebView2 postMessage OR XHR POST /api/<verb>                        */
  /*    · X-Az-Page header on every request (verb namespacing on the server) */
  /*    · in-flight dedup: identical (verb,payload) requests share one call   */
  /*    · client.log / client.metrics uplink helpers                          */
  /* ======================================================================= */
  function Deferred() { this._st = 0; this._val = null; this._ok = []; this._err = []; }
  Deferred.prototype.resolve = function (v) {
    if (this._st !== 0) return;
    this._st = 1; this._val = v;
    var i, a = this._ok;
    for (i = 0; i < a.length; i++) this._safe(a[i], v);
    this._ok = []; this._err = [];
  };
  Deferred.prototype.reject = function (e) {
    if (this._st !== 0) return;
    this._st = 2; this._val = e;
    var i, a = this._err;
    for (i = 0; i < a.length; i++) this._safe(a[i], e);
    this._ok = []; this._err = [];
  };
  Deferred.prototype._safe = function (fn, v) {
    try { fn(v); } catch (ex) { if (global.console) console.error(ex); }
  };
  Deferred.prototype.promise = function () {
    var self = this;
    var api = {
      then: function (onOk, onErr) {
        if (typeof onOk === 'function') {
          if (self._st === 1) self._safe(onOk, self._val);
          else if (self._st === 0) self._ok.push(onOk);
        }
        if (typeof onErr === 'function') {
          if (self._st === 2) self._safe(onErr, self._val);
          else if (self._st === 0) self._err.push(onErr);
        }
        return api;
      },
      'catch': function (onErr) { return api.then(null, onErr); }
    };
    return api;
  };

  var pending = {};          /* id -> Deferred (webview transport)  */
  var listeners = {};        /* event -> [handlers]                 */
  var inflight = {};         /* dedup key -> thenable (shared)      */
  var seq = 1;
  var transport = null;      /* 'webview' | 'http' */
  var readyCbs = [];
  var isReady = false;

  function bridgeReadyFire() {
    if (isReady) return;
    isReady = true;
    var i;
    for (i = 0; i < readyCbs.length; i++) {
      try { readyCbs[i](); } catch (e) { if (global.console) console.error(e); }
    }
    readyCbs = [];
    /* the bridge being up is the final gate for AzBoot.ready() */
    fireBoot();
  }

  function dispatchEvent(name, data) {
    var hs = listeners[name];
    if (!hs) return;
    var i;
    for (i = 0; i < hs.length; i++) {
      try { hs[i](data); } catch (e) { if (global.console) console.error(e); }
    }
  }

  function parseJson(s) {
    if (typeof s !== 'string') return s;
    try { return JSON.parse(s); } catch (e) { return null; }
  }

  function handleInbound(msg) {
    if (!msg || typeof msg !== 'object') return;
    if (msg.id && pending[msg.id]) {
      var d = pending[msg.id];
      delete pending[msg.id];
      if (msg.error) d.reject(new Error(msg.error));
      else d.resolve(msg.result != null ? msg.result : {});
      return;
    }
    if (msg.event) dispatchEvent(msg.event, msg.data || {});
  }

  /* ---- WebView2 transport ---- */
  function initWebView() {
    var wv = global.chrome && global.chrome.webview;
    if (!wv) return false;
    transport = 'webview';
    wv.addEventListener('message', function (ev) {
      var d = ev.data;
      if (typeof d === 'string') { d = parseJson(d); if (!d) return; }
      handleInbound(d);
    });
    try { wv.postMessage(JSON.stringify({ verb: 'ready', id: 'ready', page: _pageId })); } catch (e) {}
    bridgeReadyFire();
    return true;
  }
  function callWebView(verb, payload) {
    var id = 'r' + (seq++);
    var d = new Deferred();
    pending[id] = d;
    try {
      global.chrome.webview.postMessage(
        JSON.stringify({ verb: verb, id: id, page: _pageId, payload: payload || {} }));
    } catch (e) { delete pending[id]; d.reject(e); return d.promise(); }
    setTimeout(function () {
      if (pending[id]) { delete pending[id]; d.reject(new Error('timeout: ' + verb)); }
    }, 15000);
    return d.promise();
  }

  /* ---- HTTP transport (MSHTML + dev) ---- */
  function xhrPost(url, body, onOk, onErr) {
    var x;
    try { x = new XMLHttpRequest(); }
    catch (e) {
      try { x = new global.ActiveXObject('Microsoft.XMLHTTP'); }
      catch (e2) { if (onErr) onErr(new Error('no XHR')); return; }
    }
    try {
      x.open('POST', url, true);
      try { x.setRequestHeader('Content-Type', 'application/json'); } catch (e3) {}
      /* verb namespacing / diagnostics: tell the server which page called. */
      try { if (_pageId) x.setRequestHeader('X-Az-Page', _pageId); } catch (e4) {}
      x.onreadystatechange = function () {
        if (x.readyState !== 4) return;
        if (x.status >= 200 && x.status < 300) { if (onOk) onOk(x.responseText); }
        else { if (onErr) onErr(new Error('HTTP ' + x.status)); }
      };
      x.send(body || '{}');
    } catch (e5) { if (onErr) onErr(e5); }
  }
  function initHttp() {
    transport = 'http';
    bridgeReadyFire();
    function poll() {
      xhrPost('/api/poll', '{}',
        function (txt) {
          var j = parseJson(txt);
          if (j && j.events) { var i; for (i = 0; i < j.events.length; i++) handleInbound(j.events[i]); }
          setTimeout(poll, 900);
        },
        function () { setTimeout(poll, 1500); });
    }
    poll();
    return true;
  }
  function callHttp(verb, payload) {
    var d = new Deferred();
    xhrPost('/api/' + verb, JSON.stringify(payload || {}),
      function (txt) {
        var j = parseJson(txt);
        if (j && j.error) { d.reject(new Error(j.error)); return; }
        d.resolve(j != null ? j : {});
      },
      function (err) { d.reject(err); });
    return d.promise();
  }

  function rawCall(verb, payload) {
    if (transport === 'webview') return callWebView(verb, payload);
    return callHttp(verb, payload);
  }

  /* dedup key — identical concurrent (verb,payload) requests share one call. */
  function dedupKey(verb, payload) {
    var p = '';
    try { p = JSON.stringify(payload || {}); } catch (e) { p = ''; }
    return verb + '\u0001' + p;
  }

  var AzBridge = {
    transport: function () { return transport; },
    ready: function (cb) { if (isReady) cb(); else readyCbs.push(cb); },
    on: function (event, handler) {
      if (!listeners[event]) listeners[event] = [];
      listeners[event].push(handler);
    },
    /* de-duplicated request. Returns a thenable. */
    call: function (verb, payload) {
      var key = dedupKey(verb, payload);
      if (inflight[key]) return inflight[key];
      var p = rawCall(verb, payload);
      inflight[key] = p;
      p.then(function () { delete inflight[key]; },
             function () { delete inflight[key]; });
      return p;
    },
    /* structured client log uplink (server persists to client.log). */
    log: function (level, msg, extra) {
      try {
        rawCall('client.log', {
          page: _pageId, level: ('' + (level || 'info')),
          msg: ('' + (msg || '')), extra: extra || {},
          t: (new Date()).getTime()
        });
      } catch (e) {}
    },
    /* structured metrics uplink (server persists to client.metrics). */
    metrics: function (name, value, extra) {
      try {
        rawCall('client.metrics', {
          page: _pageId, name: ('' + (name || '')),
          value: (typeof value === 'number') ? value : 0,
          extra: extra || {}, t: (new Date()).getTime()
        });
      } catch (e) {}
    }
  };

  /* ======================================================================= */
  /*  AzNav — reusable Enter/Tab navigation over an explicit id order         */
  /*    Pages call AzNav.bind(NAV_ORDER, opts). No wrap-around. Ctrl+A selects */
  /*    all text in an <input>. <select> is handled MSHTML-safe (keypress +    */
  /*    a guarded keyup so the dropdown does not swallow the advance).         */
  /* ======================================================================= */
  function isVisible(el) {
    if (!el) return false;
    if (el.disabled) return false;
    /* offsetParent is null for display:none on both engines. */
    if (el.offsetParent === null && el.type !== 'hidden') {
      if (!(el.offsetWidth || el.offsetHeight)) return false;
    }
    return true;
  }

  var AzNav = {
    isVisible: isVisible,
    /* order: array of element ids; opts.onEnter(id,el,key) may return true to
       stop the default "advance to next" behaviour (e.g. run a lookup). */
    bind: function (order, opts) {
      opts = opts || {};
      function elFor(id) { return $(id); }
      function idxOf(el) {
        if (!el || !el.id) return -1;
        var i;
        for (i = 0; i < order.length; i++) if (order[i] === el.id) return i;
        return -1;
      }
      function focusEl(el) {
        if (!el) return;
        try { el.focus(); } catch (e) {}
        if (el.tagName === 'INPUT' && el.select) { try { el.select(); } catch (e2) {} }
      }
      function focusNext(cur) {
        var oi = idxOf(cur), i;
        if (oi < 0) return;
        for (i = oi + 1; i < order.length; i++) {
          var el = elFor(order[i]); if (el && isVisible(el)) { focusEl(el); return; }
        }
        /* no wrap-around: stay put at the last field. */
      }
      function focusPrev(cur) {
        var oi = idxOf(cur), i;
        if (oi < 0) return;
        for (i = oi - 1; i >= 0; i--) {
          var el = elFor(order[i]); if (el && isVisible(el)) { focusEl(el); return; }
        }
      }
      function keydown(ev) {
        ev = ev || global.event;
        var el = ev.target || ev.srcElement;
        if (!el || idxOf(el) < 0) return;
        var key = ev.keyCode || ev.which;
        /* Ctrl+A -> select all text inside an INPUT (not a browser select-all) */
        if ((ev.ctrlKey || ev.metaKey) && (key === 65)) {
          if (el.tagName === 'INPUT' && el.select) {
            try { el.select(); } catch (e) {}
            if (ev.preventDefault) ev.preventDefault(); ev.returnValue = false;
            return false;
          }
        }
        /* Shift+Tab -> previous (never wrap) */
        if (key === 9 && ev.shiftKey) {
          focusPrev(el);
          if (ev.preventDefault) ev.preventDefault(); ev.returnValue = false;
          return false;
        }
        /* Tab / Enter -> advance (or the page's custom onEnter handler) */
        if (key === 9 || key === 13) {
          if (el.tagName === 'SELECT' && key === 13) { el.__navHandled = true; }
          var stop = false;
          if (typeof opts.onEnter === 'function') {
            try { stop = !!opts.onEnter(el.id, el, key); } catch (e) {}
          }
          if (!stop) focusNext(el);
          if (ev.preventDefault) ev.preventDefault(); ev.returnValue = false;
          return false;
        }
        return true;
      }
      function keyup(ev) {
        ev = ev || global.event;
        var el = ev.target || ev.srcElement;
        if (!el || el.tagName !== 'SELECT') return;
        if (el.__navHandled) { el.__navHandled = false; }
      }
      var i;
      for (i = 0; i < order.length; i++) {
        var e = elFor(order[i]);
        if (!e) continue;
        if (e.addEventListener) {
          e.addEventListener('keydown', keydown, false);
          if (e.tagName === 'SELECT') e.addEventListener('keyup', keyup, false);
        } else if (e.attachEvent) {  /* MSHTML */
          e.attachEvent('onkeydown', keydown);
          if (e.tagName === 'SELECT') e.attachEvent('onkeyup', keyup);
        }
      }
      return { focusNext: focusNext, focusPrev: focusPrev,
               focusEl: focusEl, indexOf: idxOf };
    }
  };

  /* ======================================================================= */
  /*  AzPerf — lightweight page perf counters, flushed to client.metrics      */
  /* ======================================================================= */
  var _t0 = (new Date()).getTime();
  var AzPerf = {
    since: function () { return (new Date()).getTime() - _t0; },
    mark: function (name, ms) {
      var v = (typeof ms === 'number') ? ms : AzPerf.since();
      AzBridge.metrics(name, v);
    }
  };

  /* ======================================================================= */
  /*  Boot                                                                    */
  /* ======================================================================= */
  _pageId = readPageId();

  /* auto-select transport: prefer WebView2, else HTTP (MSHTML uses this). */
  if (!initWebView()) initHttp();

  /* apply the persisted theme early (best-effort). */
  (function () {
    var h = doc.documentElement;
    if (h && h.getAttribute && h.getAttribute('data-theme') === 'dark') {
      AzBoot.applyTheme(true);
    }
  })();

  /* v1.44.0: intentionally NO "page loaded" log/metric here. Per the logging
     policy, client.log / client.metrics are written ONLY on abnormal events
     (hang / crash / timeout / exception) — never on happy-path boot noise. */

  /* publish. AzBridge is ALSO exposed as `Bridge` so pages written against the
     original admission bridge (Bridge.call/ready/on) keep working unchanged. */
  global.AzBoot = AzBoot;
  global.AzBridge = AzBridge;
  global.AzUi = AzUi;
  global.AzNav = AzNav;
  global.AzPerf = AzPerf;
  if (!global.Bridge) global.Bridge = AzBridge;

})(window);
