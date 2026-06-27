/* fields.js — bindable data fields, grouped by category.
   The `key` values mirror the C++ printer tokens so a saved design prints
   real data. New keys here must also be handled in printer.cpp fieldValue(). */
window.AZ_FIELD_CATS = [
  { title:"تاریخ و ساعت", items:[
    { key:"{date}",  label:"تاریخ", sample:"۱۴۰۵/۰۴/۰۶" },
    { key:"{time}",  label:"ساعت", sample:"۱۰:۳۰" },
    { key:"{shift}", label:"شیفت", sample:"صبح" },
    { key:"{datetime}", label:"تاریخ و ساعت", sample:"۱۴۰۵/۰۴/۰۶ - ۱۰:۳۰" },
  ]},
  { title:"اطلاعات بیمار", items:[
    { key:"{first}",    label:"نام", sample:"علی" },
    { key:"{last}",     label:"نام خانوادگی", sample:"رضایی" },
    { key:"{full}",     label:"نام و نام خانوادگی", sample:"علی رضایی" },
    { key:"{father}",   label:"نام پدر", sample:"حسن" },
    { key:"{nid}",      label:"کد ملی", sample:"۰۰۱۲۳۴۵۶۷۸" },
    { key:"{birth}",    label:"تاریخ تولد", sample:"۱۳۷۰/۰۵/۱۲" },
    { key:"{gender}",   label:"جنسیت", sample:"مرد" },
    { key:"{mobile}",   label:"تلفن همراه", sample:"۰۹۱۲۰۰۰۰۰۰۰" },
    { key:"{landline}", label:"تلفن ثابت", sample:"۰۲۱۰۰۰۰۰۰۰" },
    { key:"{address}",  label:"آدرس", sample:"تهران، خیابان…" },
    { key:"{ptype}",    label:"نوع بیمار", sample:"سرپایی" },
  ]},
  { title:"بیمه", items:[
    { key:"{ins}",      label:"بیمه اصلی", sample:"تأمین اجتماعی" },
    { key:"{supp}",     label:"بیمه مکمل", sample:"دانا" },
    { key:"{insno}",    label:"شماره دفترچه", sample:"۱۲۳۴۵۶" },
    { key:"{insexp}",   label:"اعتبار بیمه", sample:"۱۴۰۵/۱۲/۲۹" },
  ]},
  { title:"پزشک و نوبت", items:[
    { key:"{doctor}",   label:"پزشک معالج", sample:"دکتر احمدی" },
    { key:"{dept}",     label:"بخش / دپارتمان", sample:"دندانپزشکی" },
    { key:"{queue}",    label:"شماره نوبت", sample:"۱۲" },
    { key:"{apptdate}", label:"تاریخ نوبت", sample:"۱۴۰۵/۰۴/۰۷" },
    { key:"{appttime}", label:"ساعت نوبت", sample:"۱۱:۰۰" },
    { key:"{appttype}", label:"نوع نوبت", sample:"عادی" },
  ]},
  { title:"مالی و صورتحساب", items:[
    { key:"{total}",    label:"جمع کل", sample:"۲٬۵۰۰٬۰۰۰ ریال" },
    { key:"{insshare}", label:"سهم بیمه", sample:"۱٬۰۰۰٬۰۰۰ ریال" },
    { key:"{discount}", label:"مبلغ تخفیف", sample:"۲۰۰٬۰۰۰ ریال" },
    { key:"{paid}",     label:"مبلغ نهایی / پرداختی", sample:"۱٬۳۰۰٬۰۰۰ ریال" },
    { key:"{service}",  label:"شرح خدمت", sample:"ویزیت" },
  ]},
  { title:"درمانگاه / سامانه", items:[
    { key:"{clinic}",   label:"نام درمانگاه", sample:"درمانگاه آزادی طب" },
    { key:"{receiptNo}",label:"شماره قبض", sample:"۱۰۰۲۳" },
    { key:"{user}",     label:"کاربر پذیرش", sample:"پذیرش ۱" },
    { key:"{issued}",   label:"چاپ توسط پذیرش", sample:"چاپ توسط: پذیرش ۱" },
  ]},
];

/* flat lookup by key -> {label,sample} */
window.AZ_FIELDS = (function(){
  var m = {};
  window.AZ_FIELD_CATS.forEach(function(c){ c.items.forEach(function(it){ m[it.key]=it; }); });
  return m;
})();
