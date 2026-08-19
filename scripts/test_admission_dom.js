/* ============================================================================
   test_admission_dom.js — headless regression test for the admission UI.

   Reproduces the exact "click a service suggestion → add it" flow that froze
   the WHOLE app under MSHTML, plus the other DOM-teardown hazards (qty typing,
   delete, queue, patient/doctor pickers). Run with jsdom:

     cd /tmp/jstest && NODE_PATH=/tmp/jstest/node_modules \
         node /home/user/webapp/scripts/test_admission_dom.js

   Every method prints a single line. The harness exits 0 only if ALL pass.
   ============================================================================ */
'use strict';
var fs = require('fs');
var path = require('path');
var { JSDOM } = require('jsdom');

var ROOT = path.resolve(__dirname, '..');
var HTML = fs.readFileSync(path.join(ROOT, 'assets/admission/index.html'), 'utf8')
  .replace(/<link[^>]*>/g, '')
  .replace(/<script[^>]*src=[^>]*><\/script>/g, '')
  .replace(/<img[^>]*>/g, '');
var JS = fs.readFileSync(path.join(ROOT, 'assets/admission/admission.js'), 'utf8');
var BRIDGE = fs.readFileSync(path.join(ROOT, 'assets/admission/bridge.js'), 'utf8');

function freshDOM() {
  return new JSDOM(HTML, { runScripts: 'outside-only', url: 'http://127.0.0.1/' });
}

/* mock Bridge — evals bridge.js first (with a stubbed chrome.webview so the HTTP
   polling path never starts), then OVERWRITES global.Bridge with our mock. */
function makeBridge(w, catalog, patient, initOverrides) {
  function thenable(r) {
    return { then: function (ok) { setTimeout(function () { ok(r); }, 1); return this; },
             'catch': function () { return this; } };
  }
  w.chrome = { webview: { addEventListener: function () {}, postMessage: function () {} } };
  try { w.eval(BRIDGE); } catch (e) {}
  w.Bridge = {
    transport: function () { return 'mock'; },
    ready: function (cb) { setTimeout(cb, 1); },
    on: function () {},
    call: function (verb, payload) {
      if (verb === 'init') {
        var init = { ok: true,
          insurances: [{ name: 'آزاد', pct: 0 }, { name: 'تأمین', pct: 70 }],
          supp: [{ name: 'ندارد', pct: 0 }], ps: { P: 0, S: 0 }, date: '1404/04/24' };
        var ik;
        for (ik in (initOverrides || {})) if (Object.prototype.hasOwnProperty.call(initOverrides, ik)) init[ik] = initOverrides[ik];
        return thenable(init);
      }
      if (verb === 'service.search') return thenable({ rows: catalog });
      if (verb === 'service.resolve') {
        var c = (payload && payload.code) || '', hit = null, i;
        for (i = 0; i < catalog.length; i++)
          if (String(catalog[i].code) === String(c)) { hit = catalog[i]; break; }
        return thenable(hit ? { found: true, service: hit } : { found: false });
      }
      if (verb === 'patient.lookup') return thenable(patient ? { found: true, patient: patient } : { found: false });
      if (verb === 'patient.search') return thenable({ rows: patient ? [patient] : [] });
      if (verb === 'doctor.search') return thenable({ rows: [] });
      if (verb === 'queue.list') return thenable({ rows: [] });
      if (verb === 'bill.compute') {
        var rows = (payload && payload.services) || [], gross = 0, disc = 0;
        var org = 0, supp = 0, pat = 0, bp = payload && payload.hasIns && payload.insMain === 1 ? 70 : 0;
        var sp = Number(payload && payload.insSuppPct) || 0, bi, row, unit, g, ds, net, os, rem, ss;
        if (bp < 0) bp = 0; else if (bp > 100) bp = 100;
        if (sp < 0) sp = 0; else if (sp > 100) sp = 100;
        for (bi = 0; bi < rows.length; bi++) {
          row = rows[bi]; unit = row.freeRate ? Number(row.freePrice || 0) : Number(row.price || 0);
          g = unit * (Number(row.qty) || 1); ds = Math.min(g, Math.max(0, Number(row.discount) || 0));
          net = g - ds; os = Math.round(net * bp / 100); rem = net - os; ss = Math.round(rem * sp / 100);
          gross += g; disc += ds; org += os; supp += ss; pat += rem - ss;
        }
        return thenable({ gross: gross, disc: disc, org: org, supp: supp, pat: pat,
          paid: payload && payload.noPay ? 0 : pat });
      }
      return thenable({ ok: true });
    }
  };
}

function boot(w) {
  try { w.eval(JS); } catch (e) { return e; }
  w.document.dispatchEvent(new w.Event('DOMContentLoaded', { bubbles: true }));
  return null;
}
function $(w, id) { return w.document.getElementById(id); }
function click(w, el) { el.dispatchEvent(new w.MouseEvent('click', { bubbles: true, cancelable: true })); }
function typeInput(w, el, val) { el.value = val; el.dispatchEvent(new w.Event('input', { bubbles: true })); }
function drain(ms) { return new Promise(function (r) { setTimeout(r, ms || 60); }); }
function parseMoneyText(el) {
  var text = (el || {}).textContent || '';
  var en = text.replace(/[۰-۹]/g, function (d) { return '۰۱۲۳۴۵۶۷۸۹'.indexOf(d); }).replace(/,/g, '');
  return parseInt(en, 10) || 0;
}
function enterKey(w, el) {
  var ke = new w.KeyboardEvent('keydown', { bubbles: true, keyCode: 13, which: 13 });
  try { Object.defineProperty(ke, 'keyCode', { value: 13 }); } catch (e) {}
  el.dispatchEvent(ke);
}

var results = [];
function ok(name, d) { results.push({ name: name, pass: true, detail: d }); }
function bad(name, d) { results.push({ name: name, pass: false, detail: d }); }

/* open a fresh booted window, search a code, wait for the suggestion picker */
function setupWithPicker(catalog, patient, code) {
  var dom = freshDOM(); var w = dom.window;
  makeBridge(w, catalog, patient);
  var e = boot(w); if (e) return Promise.reject(new Error('boot: ' + e.message));
  return drain(80).then(function () {
    var s = $(w, 'svcSearch');
    typeInput(w, s, code);
    return drain(240).then(function () { return w; });
  });
}

var CATALOG = [
  { code: '1', name: 'تزریق', price: 1200000, status: 1 },
  { code: '2', name: 'ویزیت', price: 800000, status: 1 }
];
var PATIENT = { nid: '1234567890', first: 'علی', last: 'احمدی', gender: '1', insurances: [1] };

function M1() {
  return setupWithPicker(CATALOG, PATIENT, '1').then(function (w) {
    var box = $(w, 'svcSuggest');
    var row = box.querySelector('[data-s]');
    if (!row) { bad('M1 add-on-suggest-click', 'no suggestion row'); return; }
    click(w, row);                       /* THE action that used to freeze */
    return drain(80).then(function () {
      var trs = $(w, 'svcBody').getElementsByTagName('tr');
      if (trs.length === 1) ok('M1 add-on-suggest-click', '1 service row, no wedge');
      else bad('M1 add-on-suggest-click', 'rows=' + trs.length);
    });
  });
}

function M2() {
  var dom = freshDOM(); var w = dom.window;
  makeBridge(w, CATALOG, PATIENT);
  var e = boot(w); if (e) return Promise.reject(new Error('boot: ' + e.message));
  return drain(80).then(function () {
    var s = $(w, 'svcSearch');
    typeInput(w, s, '2');
    enterKey(w, s);
    return drain(120).then(function () {
      var trs = $(w, 'svcBody').getElementsByTagName('tr');
      if (trs.length === 1) ok('M2 resolve-on-Enter', '1 service row');
      else bad('M2 resolve-on-Enter', 'rows=' + trs.length);
    });
  });
}

function M3() {
  return setupWithPicker(CATALOG, PATIENT, '1').then(function (w) {
    click(w, $(w, 'svcSuggest').querySelector('[data-s]'));
    return drain(80).then(function () {
      var inp = $(w, 'svcBody').querySelector('.qty-inp');
      if (!inp) { bad('M3 qty-typing no-rebuild', 'no qty input'); return; }
      typeInput(w, inp, '3');
      inp.dispatchEvent(new w.Event('keyup', { bubbles: true }));
      return drain(60).then(function () {
        var inp2 = $(w, 'svcBody').querySelector('.qty-inp');
        if (inp2 === inp) ok('M3 qty-typing no-rebuild', 'input survived typing');
        else bad('M3 qty-typing no-rebuild', 'input replaced (table rebuilt under caret)');
      });
    });
  });
}

function M4() {
  return setupWithPicker(CATALOG, PATIENT, '1').then(function (w) {
    click(w, $(w, 'svcSuggest').querySelector('[data-s]'));
    return drain(80).then(function () {
      var del = $(w, 'svcBody').querySelector('[data-del]');
      if (!del) { bad('M4 delete-row', 'no delete btn'); return; }
      click(w, del);
      return drain(80).then(function () {
        var trs = $(w, 'svcBody').getElementsByTagName('tr');
        var empty = trs.length === 1 && /empty/.test(trs[0].innerHTML);
        if (empty) ok('M4 delete-row', 'row removed');
        else bad('M4 delete-row', 'rows=' + trs.length);
      });
    });
  });
}

function M5() {
  var dom = freshDOM(); var w = dom.window;
  makeBridge(w, CATALOG, PATIENT);
  var e = boot(w); if (e) return Promise.reject(new Error('boot: ' + e.message));
  return drain(80).then(function () {
    var s = $(w, 'svcSearch');
    typeInput(w, s, '1');
    return drain(240).then(function () {
      var i;
      for (i = 0; i < 25; i++) {
        var row = $(w, 'svcSuggest').querySelector('[data-s]');
        if (row) click(w, row);
      }
      return drain(200).then(function () {
        var trs = $(w, 'svcBody').getElementsByTagName('tr');
        if (trs.length >= 1) ok('M5 rapid-25-adds', trs.length + ' rows, no wedge');
        else bad('M5 rapid-25-adds', 'rows=' + trs.length);
      });
    });
  });
}

function M6() {
  var dom = freshDOM(); var w = dom.window;
  makeBridge(w, CATALOG, PATIENT);
  var e = boot(w); if (e) return Promise.reject(new Error('boot: ' + e.message));
  return drain(80).then(function () {
    $(w, 'hasIns').checked = true;
    $(w, 'hasIns').dispatchEvent(new w.Event('change', { bubbles: true }));
    $(w, 'insMain').selectedIndex = 1;
    $(w, 'insMain').dispatchEvent(new w.Event('change', { bubbles: true }));
    var s = $(w, 'svcSearch');
    typeInput(w, s, '1');
    return drain(240).then(function () {
      click(w, $(w, 'svcSuggest').querySelector('[data-s]'));
      return drain(120).then(function () {
        var patText = ($(w, 'sfPat') || {}).textContent || '';
        var en = patText.replace(/[۰-۹]/g, function (d) { return '۰۱۲۳۴۵۶۷۸۹'.indexOf(d); }).replace(/,/g, '');
        var pat = parseInt(en, 10);
        if (pat === 360000) ok('M6 insurance-billing', 'pat=360000 (30% of 1.2M)');
        else bad('M6 insurance-billing', 'pat=' + pat + ' (expected 360000)');
      });
    });
  });
}

function M7() {
  var dom = freshDOM(); var w = dom.window;
  makeBridge(w, CATALOG, PATIENT);
  try { w.eval(JS); } catch (e) { return Promise.reject(new Error('boot: ' + e.message)); }
  /* fire ready TWICE (MSHTML readystatechange quirk) */
  w.document.dispatchEvent(new w.Event('DOMContentLoaded', { bubbles: true }));
  w.document.dispatchEvent(new w.Event('DOMContentLoaded', { bubbles: true }));
  return drain(80).then(function () {
    var s = $(w, 'svcSearch');
    typeInput(w, s, '1');
    return drain(240).then(function () {
      click(w, $(w, 'svcSuggest').querySelector('[data-s]'));
      return drain(80).then(function () {
        var trs = $(w, 'svcBody').getElementsByTagName('tr');
        if (trs.length === 1) ok('M7 double-ready idempotent', '1 row, not 2');
        else bad('M7 double-ready idempotent', 'rows=' + trs.length + ' (double-wired!)');
      });
    });
  });
}

function M8() {
  return setupWithPicker(CATALOG, PATIENT, '1').then(function (w) {
    click(w, $(w, 'svcSuggest').querySelector('[data-s]'));
    return drain(80).then(function () {
      var inp = $(w, 'svcBody').querySelector('.disc-inp');
      if (!inp) { bad('M8 live-service-discount', 'no discount input'); return; }
      typeInput(w, inp, '200000');
      return drain(180).then(function () {
        var shown = ($(w, 'sfDisc') || {}).textContent || '';
        if (parseMoneyText($(w, 'sfDisc')) === 200000 && $(w, 'svcBody').querySelector('.disc-inp') === inp)
          ok('M8 live-service-discount', 'discount=200000, input survived typing');
        else bad('M8 live-service-discount', 'shown=' + shown);
      });
    });
  });
}

function M9() {
  var dom = freshDOM(); var w = dom.window;
  makeBridge(w, CATALOG, PATIENT);
  var e = boot(w); if (e) return Promise.reject(new Error('boot: ' + e.message));
  return drain(80).then(function () {
    var right = $(w, 'colRight'), left = $(w, 'colLeft'), center = $(w, 'colCenter');
    right.dispatchEvent(new w.MouseEvent('mousedown', { bubbles: true }));
    var rightOnly = /is-active/.test(right.className) && !/is-active/.test(center.className);
    left.dispatchEvent(new w.Event('focusin', { bubbles: true }));
    if (rightOnly && /is-active/.test(left.className) && !/is-active/.test(right.className))
      ok('M9 active-zone-focus', 'mouse and focus move a single active zone');
    else bad('M9 active-zone-focus', 'active class did not move exclusively');
  });
}

function bootWithInit(init) {
  var dom = freshDOM(); var w = dom.window;
  makeBridge(w, CATALOG, PATIENT, init);
  var e = boot(w); if (e) return Promise.reject(new Error('boot: ' + e.message));
  return drain(80).then(function () { return w; });
}

function M10() {
  return bootWithInit({ theme: 'light', palette: 'calm' }).then(function (w) {
    if (/theme-calm/.test(w.document.body.className) && !/theme-dark/.test(w.document.body.className))
      ok('M10 light-palette', 'calm palette applied');
    else bad('M10 light-palette', w.document.body.className);
  });
}

function M11() {
  return bootWithInit({ theme: 'dark', palette: 'warm' }).then(function (w) {
    if (/theme-dark/.test(w.document.body.className) && !/theme-warm/.test(w.document.body.className))
      ok('M11 dark-palette-isolation', 'dark excludes warm/calm classes');
    else bad('M11 dark-palette-isolation', w.document.body.className);
  });
}

function M12() {
  return bootWithInit({
    supp: [{ name: 'ندارد', pct: 0 }, { name: 'نامعتبر', pct: 150 }]
  }).then(function (w) {
    $(w, 'hasIns').checked = true;
    $(w, 'insMain').selectedIndex = 1;
    $(w, 'insSupp').selectedIndex = 1;
    $(w, 'insSuppPct').selectedIndex = 0;
    typeInput(w, $(w, 'svcSearch'), '1');
    return drain(240).then(function () {
      click(w, $(w, 'svcSuggest').querySelector('[data-s]'));
      return drain(180).then(function () {
        var pat = parseMoneyText($(w, 'sfPat'));
        var insured = parseMoneyText($(w, 'sfIns'));
        if (pat === 0 && insured === 1200000) ok('M12 percentage-clamp', '150% supplementary clamped to remaining 100%');
        else bad('M12 percentage-clamp', 'pat=' + pat + ', insured=' + insured);
      });
    });
  });
}

function M13() {
  return setupWithPicker(CATALOG, PATIENT, '1').then(function (w) {
    click(w, $(w, 'svcSuggest').querySelector('[data-s]'));
    return drain(80).then(function () {
      var inp = $(w, 'svcBody').querySelector('.disc-inp');
      typeInput(w, inp, '9999999');
      return drain(180).then(function () {
        var disc = parseMoneyText($(w, 'sfDisc'));
        var paid = parseMoneyText($(w, 'invFinPaid'));
        var pat = parseMoneyText($(w, 'sfPat'));
        if (disc === 1200000 && paid === 0 && pat === 0)
          ok('M13 discount-bounds', 'discount capped at gross; all totals remain nonnegative');
        else bad('M13 discount-bounds', 'disc=' + disc + ', paid=' + paid + ', pat=' + pat);
      });
    });
  });
}

function report() {
  var pass = 0, fail = 0, i;
  for (i = 0; i < results.length; i++) {
    if (results[i].pass) { pass++; console.log('  \u2714 ' + results[i].name + ': OK' + (results[i].detail ? ' (' + results[i].detail + ')' : '')); }
    else { fail++; console.log('  \u2718 ' + results[i].name + ': FAIL (' + results[i].detail + ')'); }
  }
  console.log('\n' + pass + ' passed, ' + fail + ' failed');
  if (fail === 0) { console.log('ALL METHODS PASS \u2014 no freeze signature detected'); process.exit(0); }
  console.log('FREEZE REGRESSION DETECTED'); process.exit(1);
}

M1()
  .catch(function (e) { bad('M1', e.message); })
  .then(M2).catch(function (e) { bad('M2', e.message); })
  .then(M3).catch(function (e) { bad('M3', e.message); })
  .then(M4).catch(function (e) { bad('M4', e.message); })
  .then(M5).catch(function (e) { bad('M5', e.message); })
  .then(M6).catch(function (e) { bad('M6', e.message); })
  .then(M7).catch(function (e) { bad('M7', e.message); })
  .then(M8).catch(function (e) { bad('M8', e.message); })
  .then(M9).catch(function (e) { bad('M9', e.message); })
  .then(M10).catch(function (e) { bad('M10', e.message); })
  .then(M11).catch(function (e) { bad('M11', e.message); })
  .then(M12).catch(function (e) { bad('M12', e.message); })
  .then(M13).catch(function (e) { bad('M13', e.message); })
  .then(report);
