/* ============================================================================
   dashboard.js — CRM Dashboard page (KPIs + quick stats). ES5-only.
   Registers on window.Crm.pages.dashboard. Calls crm.dashboard.
   ============================================================================ */
(function (global) {
  'use strict';
  var Crm = global.Crm;

  function kpi(label, value, foot, kind) {
    var card = Crm.el('div', 'crm-kpi' + (kind ? ' ' + kind : ''));
    card.innerHTML =
      '<div class="crm-kpi-label">' + Crm.esc(label) + '</div>' +
      '<div class="crm-kpi-value">' + Crm.faDigits(value) + '</div>' +
      (foot ? '<div class="crm-kpi-foot">' + Crm.esc(foot) + '</div>' : '');
    return card;
  }

  Crm.pages.dashboard = {
    title: 'داشبورد',
    render: function (host) {
      Crm.head(host, 'داشبورد مدیریت', 'نمای کلی آمار درمانگاه');
      var kpis = Crm.el('div', 'crm-kpis');
      host.appendChild(kpis);
      /* placeholder while loading */
      kpis.appendChild(kpi('بیماران', '…', '', ''));
      kpis.appendChild(kpi('پزشکان و پرستاران', '…', '', 'k-green'));
      kpis.appendChild(kpi('خدمات', '…', '', 'k-amber'));
      kpis.appendChild(kpi('بخش‌ها', '…', '', 'k-violet'));

      Crm.call('crm.dashboard', {}).then(function (d) {
        kpis.innerHTML = '';
        kpis.appendChild(kpi('بیماران ثبت‌شده', d.patients || 0, 'مجموع پرونده‌ها', ''));
        kpis.appendChild(kpi('پزشکان و پرستاران', d.doctors || 0, 'نیروی درمانی', 'k-green'));
        kpis.appendChild(kpi('خدمات فعال', d.services || 0, 'فهرست خدمات درمانگاه', 'k-amber'));
        kpis.appendChild(kpi('بخش‌ها', d.sections || 0, 'بخش‌های تعریف‌شده', 'k-violet'));
        kpis.appendChild(kpi('کاربران سیستم', d.employees || 0, 'پذیرش و مدیریت', 'k-rose'));
        kpis.appendChild(kpi('پذیرش امروز', d.today || 0, 'تعداد پذیرش روز جاری', ''));
        kpis.appendChild(kpi('پیام‌های جدید', d.messages || 0, 'کارتابل خوانده‌نشده', 'k-green'));
        kpis.appendChild(kpi('درخواست‌های در انتظار', d.pendingReqs || 0, 'تنظیمات و پروفایل', 'k-amber'));

        var card = Crm.el('div', 'crm-card');
        card.innerHTML =
          '<div class="crm-card-title"><span class="dot"></span>وضعیت امروز</div>' +
          '<div class="crm-banner info">' +
          'تاریخ: ' + Crm.faDigits(d.date || '----/--/--') + ' &nbsp;•&nbsp; ' +
          'ساعت: ' + Crm.faDigits(d.time || '--:--:--') +
          '</div>' +
          '<div style="font-size:13px;color:#475569;line-height:2">' +
          'از منوی کناری برای مدیریت بخش‌ها، بیماران، پزشکان، خدمات، کاربران، ' +
          'کارتابل پیام‌ها و تنظیمات سیستم استفاده کنید. تغییرات بلافاصله در ' +
          'سیستم ذخیره و در صفحه پذیرش اعمال می‌شود.' +
          '</div>';
        host.appendChild(card);
      }, function () {
        kpis.innerHTML = '';
        kpis.appendChild(Crm.el('div', 'crm-banner err', 'بارگذاری آمار ناموفق بود.'));
      });
    }
  };
})(window);
