/* ============================================================================
   bridge.js — bidirectional C++ <-> JS IPC layer for the Admission UI.

   Two transports, auto-selected at runtime:
     (A) WebView2  — window.chrome.webview.postMessage / addEventListener
                     (native, zero-latency, preferred).
     (B) HTTP      — POST /api/<verb> to the loopback host (fallback / when the
                     page is opened in a plain browser during development).

   Public API:
     Bridge.ready(cb)               -> called once the transport is up
     Bridge.call(verb, payload)     -> returns a Promise<result-object>
     Bridge.on(event, handler)      -> subscribe to C++ -> JS push events
   Messages are structured JSON with request/response IDs (script requirement).
   ============================================================================ */
(function (global) {
  'use strict';

  const pending = new Map();   // id -> {resolve, reject}
  const listeners = new Map(); // event -> [handlers]
  let seq = 1;
  let transport = null;        // 'webview' | 'http'
  const readyCbs = [];
  let isReady = false;

  function fireReady() {
    if (isReady) return;
    isReady = true;
    readyCbs.splice(0).forEach((cb) => { try { cb(); } catch (e) { console.error(e); } });
  }

  function dispatchEvent(name, data) {
    const hs = listeners.get(name);
    if (hs) hs.forEach((h) => { try { h(data); } catch (e) { console.error(e); } });
  }

  // ---- handle an inbound message object (from either transport) ----
  function handleInbound(msg) {
    if (!msg || typeof msg !== 'object') return;
    // response to a call
    if (msg.id && pending.has(msg.id)) {
      const p = pending.get(msg.id);
      pending.delete(msg.id);
      if (msg.error) p.reject(new Error(msg.error));
      else p.resolve(msg.result != null ? msg.result : {});
      return;
    }
    // C++ -> JS push event
    if (msg.event) dispatchEvent(msg.event, msg.data || {});
  }

  // ---------------------------------------------------------------- WebView2
  function initWebView() {
    const wv = global.chrome && global.chrome.webview;
    if (!wv) return false;
    transport = 'webview';
    wv.addEventListener('message', (ev) => {
      let d = ev.data;
      if (typeof d === 'string') { try { d = JSON.parse(d); } catch (_) { return; } }
      handleInbound(d);
    });
    // tell the host we are ready to receive state
    wv.postMessage(JSON.stringify({ verb: 'ready', id: 'ready' }));
    fireReady();
    return true;
  }

  function callWebView(verb, payload) {
    const id = 'r' + (seq++);
    return new Promise((resolve, reject) => {
      pending.set(id, { resolve, reject });
      global.chrome.webview.postMessage(JSON.stringify({ verb, id, payload: payload || {} }));
      setTimeout(() => {
        if (pending.has(id)) { pending.delete(id); reject(new Error('timeout: ' + verb)); }
      }, 15000);
    });
  }

  // -------------------------------------------------------------------- HTTP
  function initHttp() {
    transport = 'http';
    // poll for pushed events (C++ -> JS) via /api/poll
    fireReady();
    (function poll() {
      fetch('/api/poll', { method: 'POST', body: '{}' })
        .then((r) => r.ok ? r.json() : null)
        .then((j) => { if (j && j.events) j.events.forEach((e) => handleInbound(e)); })
        .catch(() => {})
        .finally(() => setTimeout(poll, 1000));
    })();
    return true;
  }

  function callHttp(verb, payload) {
    const id = 'r' + (seq++);
    return fetch('/api/' + verb, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(payload || {})
    })
      .then((r) => r.ok ? r.json() : Promise.reject(new Error('HTTP ' + r.status)))
      .then((j) => { if (j && j.error) throw new Error(j.error); return j; });
  }

  // ------------------------------------------------------------- public API
  const Bridge = {
    transport: () => transport,
    ready(cb) { if (isReady) cb(); else readyCbs.push(cb); },
    on(event, handler) {
      if (!listeners.has(event)) listeners.set(event, []);
      listeners.get(event).push(handler);
    },
    call(verb, payload) {
      if (transport === 'webview') return callWebView(verb, payload);
      return callHttp(verb, payload);
    }
  };

  // auto-init: prefer WebView2, fall back to HTTP
  if (!initWebView()) initHttp();

  global.Bridge = Bridge;
})(window);
