/* templates.js — 20 built-in print templates, generated programmatically.
   Coordinates are in millimetres (paper space). The shape matches the JSON the
   C++ side stores/renders. */
(function(){
  var _id = 1;
  function nid(){ return _id++; }

  function item(o){
    return Object.assign({
      id:nid(), type:"label", x:10,y:10,w:50,h:8, rot:0, z:0, locked:false,
      text:"", field:"", prefix:"", suffix:"",
      font:"Vazirmatn", pt:11, bold:false, italic:false, align:0, lineSpacing:1.2,
      textColor:"#000000", fillColor:"#ffffff", fillTransparent:true,
      borderColor:"#000000", borderWidth:0, borderStyle:0, corner:0,
      padding:1, opacity:1, visibility:0,
      startValue:1, step:1
    }, o);
  }
  function L(x,y,w,h,text,pt,bold,align){ return item({type:"label",x:x,y:y,w:w,h:h,text:text,pt:pt,bold:!!bold,align:align||0}); }
  function F(x,y,w,h,field,prefix,pt,align){ return item({type:"field",x:x,y:y,w:w,h:h,field:field,prefix:prefix||"",pt:pt||11,align:align||0}); }
  function HL(x,y,w,bw){ return item({type:"hline",x:x,y:y,w:w,h:0.4,borderWidth:bw||0.4}); }
  function VL(x,y,h,bw){ return item({type:"vline",x:x,y:y,w:0.4,h:h,borderWidth:bw||0.4}); }
  function RECT(x,y,w,h,bw,corner){ return item({type:"rect",x:x,y:y,w:w,h:h,borderWidth:bw||0.4,corner:corner||0,fillTransparent:true}); }
  function FRAME(pw,ph,m,bw){ return item({type:"frame",x:m,y:m,w:pw-2*m,h:ph-2*m,borderWidth:bw||0.6,fillTransparent:true,isFrame:true}); }
  function LOGO(x,y,w,h){ return item({type:"logo",x:x,y:y,w:w,h:h}); }
  function QR(x,y,s){ return item({type:"qr",x:x,y:y,w:s,h:s,field:"{receiptNo}"}); }
  function APPT(x,y,w,h,pt){ return item({type:"apptno",x:x,y:y,w:w,h:h,pt:pt||22,bold:true,align:1,startValue:1,step:1}); }

  var PAPER = {A4:[210,297],A5:[148,210],A6:[105,148],B5:[176,250],Letter:[215.9,279.4],R80:[80,200],R58:[58,200]};
  function dims(p,orient){ var d=PAPER[p]||[148,210]; var w=d[0],h=d[1]; if(orient===1){var t=w;w=h;h=t;} return [w,h]; }

  var CLINIC="درمانگاه آزادی طب";

  function body(items, m, top, cw, pt){
    var y=top, rh=7.2;
    items.push(F(m,y,cw,rh,"{full}","نام و نام خانوادگی: ",pt,0)); y+=rh;
    items.push(F(m,y,cw,rh,"{nid}","کد ملی: ",pt,0)); y+=rh;
    items.push(F(m,y,cw/2,rh,"{ins}","بیمه: ",pt,0)); items.push(F(m+cw/2,y,cw/2,rh,"{insno}","دفترچه: ",pt,0)); y+=rh;
    items.push(F(m,y,cw,rh,"{doctor}","پزشک: ",pt,0)); y+=rh;
    items.push(HL(m,y+1,cw,0.3)); y+=rh;
    items.push(F(m,y,cw,rh,"{total}","جمع کل: ",pt,0)); y+=rh;
    items.push(F(m,y,cw,rh,"{paid}","پرداختی: ",pt,0)); y+=rh;
    return y;
  }

  function build(idx){
    _id = 1;
    var d = { id:0, kind:"builtin", name:"", paper:"A5", orientation:0, items:[] };
    function P(p,o){ d.paper=p; d.orientation=o||0; var w=dims(p,d.orientation); d.paperW=w[0]; d.paperH=w[1]; }
    var it=d.items;
    switch(idx){
      case 0:{ P("A5",0); d.name="پذیرش استاندارد A5 عمودی"; var m=10,cw=d.paperW-2*m;
        it.push(L(m,m,cw,10,CLINIC,16,true,1)); it.push(HL(m,m+12,cw,0.5));
        it.push(F(m,m+15,cw/2,7,"{receiptNo}","شماره قبض: ",10,2));
        it.push(F(m+cw/2,m+15,cw/2,7,"{date}","تاریخ: ",10,0));
        body(it,m,m+26,cw,11); it.push(APPT(m,d.paperH-26,cw,12,22)); break; }
      case 1:{ P("A4",0); d.name="پذیرش رسمی A4 با کادر"; var m=14,cw=d.paperW-2*m;
        it.push(FRAME(d.paperW,d.paperH,8,0.7));
        it.push(LOGO(m,m,22,22)); it.push(L(m+26,m+2,cw-26,9,CLINIC,18,true,0));
        it.push(L(m+26,m+12,cw-26,6,"رسید پذیرش بیمار",11,false,0));
        it.push(HL(m,m+24,cw,0.6));
        it.push(F(m,m+28,cw/2,7,"{date}","تاریخ: ",11,0)); it.push(F(m+cw/2,m+28,cw/2,7,"{time}","ساعت: ",11,2));
        body(it,m,m+40,cw,12); it.push(QR(d.paperW-m-26,d.paperH-m-26,26)); break; }
      case 2:{ P("A5",1); d.name="پذیرش A5 افقی دو ستونه"; var m=10,cw=d.paperW-2*m;
        it.push(L(m,m,cw,9,CLINIC,15,true,1)); it.push(HL(m,m+11,cw,0.4));
        var col=cw/2-4;
        it.push(F(m,m+15,col,7,"{full}","نام: ",11,0)); it.push(F(m,m+22,col,7,"{nid}","کد ملی: ",11,0));
        it.push(F(m,m+29,col,7,"{mobile}","موبایل: ",11,0));
        it.push(F(m+col+8,m+15,col,7,"{ins}","بیمه: ",11,0)); it.push(F(m+col+8,m+22,col,7,"{doctor}","پزشک: ",11,0));
        it.push(F(m+col+8,m+29,col,7,"{queue}","نوبت: ",11,0)); break; }
      case 3:{ P("R80",0); d.name="فیش حرارتی ۸ سانت"; var m=4,cw=d.paperW-2*m;
        it.push(L(m,m,cw,7,CLINIC,12,true,1)); it.push(HL(m,m+9,cw,0.4));
        it.push(APPT(m,m+11,cw,14,26)); it.push(L(m,m+26,cw,5,"شماره نوبت شما",9,false,1));
        it.push(HL(m,m+32,cw,0.3));
        it.push(F(m,m+34,cw,6,"{full}","نام: ",10,0)); it.push(F(m,m+40,cw,6,"{doctor}","پزشک: ",10,0));
        it.push(F(m,m+46,cw,6,"{date}","تاریخ: ",10,0)); it.push(F(m,m+52,cw,6,"{time}","ساعت: ",10,0)); break; }
      case 4:{ P("A6",0); d.name="کارت نوبت A6"; var m=7,cw=d.paperW-2*m;
        it.push(FRAME(d.paperW,d.paperH,4,0.6)); it.push(L(m,m,cw,8,CLINIC,13,true,1));
        it.push(APPT(m,m+14,cw,18,30)); it.push(L(m,m+34,cw,6,"نوبت شما",10,false,1));
        it.push(F(m,m+44,cw,6,"{full}","",10,1)); it.push(F(m,m+52,cw,6,"{doctor}","پزشک: ",9,1)); break; }
      case 5:{ P("A5",0); d.name="نسخه پزشک A5"; var m=10,cw=d.paperW-2*m;
        it.push(L(m,m,cw,9,CLINIC,15,true,1)); it.push(F(m,m+11,cw,6,"{doctor}","پزشک معالج: ",11,1));
        it.push(HL(m,m+19,cw,0.4));
        it.push(F(m,m+22,cw,7,"{full}","نام بیمار: ",11,0)); it.push(F(m,m+29,cw,7,"{birth}","تاریخ تولد: ",11,0));
        it.push(F(m,m+36,cw,7,"{date}","تاریخ: ",11,0)); it.push(RECT(m,m+46,cw,d.paperH-m-46-10,0.4,2)); break; }
      case 6:{ P("A4",0); d.name="صورتحساب مالی A4"; var m=14,cw=d.paperW-2*m;
        it.push(L(m,m,cw,9,CLINIC,18,true,1)); it.push(L(m,m+11,cw,6,"صورتحساب",12,false,1));
        it.push(HL(m,m+20,cw,0.6));
        it.push(F(m,m+24,cw,7,"{full}","بیمار: ",12,0));
        it.push(F(m,m+34,cw,7,"{service}","خدمت: ",12,0)); it.push(F(m,m+41,cw,7,"{total}","جمع کل: ",12,0));
        it.push(F(m,m+48,cw,7,"{insshare}","سهم بیمه: ",12,0)); it.push(F(m,m+55,cw,7,"{discount}","تخفیف: ",12,0));
        it.push(HL(m,m+64,cw,0.4)); it.push(F(m,m+66,cw,9,"{paid}","قابل پرداخت: ",14,0)); break; }
      case 7:{ P("A5",0); d.name="پذیرش ساده بدون کادر"; var m=12,cw=d.paperW-2*m;
        it.push(L(m,m,cw,8,CLINIC,14,true,1)); body(it,m,m+16,cw,11); break; }
      case 8:{ P("A5",0); d.name="پذیرش با لوگو و QR"; var m=10,cw=d.paperW-2*m;
        it.push(LOGO(m,m,18,18)); it.push(L(m+22,m+2,cw-22,8,CLINIC,15,true,0));
        it.push(HL(m,m+22,cw,0.4)); body(it,m,m+26,cw,11);
        it.push(QR(d.paperW-m-22,d.paperH-m-22,22)); break; }
      case 9:{ P("R58",0); d.name="فیش حرارتی ۵.۸ سانت"; var m=3,cw=d.paperW-2*m;
        it.push(L(m,m,cw,6,CLINIC,10,true,1)); it.push(APPT(m,m+8,cw,12,22));
        it.push(F(m,m+22,cw,5,"{full}","",9,1)); it.push(F(m,m+28,cw,5,"{date}","",9,1)); break; }
      case 10:{ P("A4",1); d.name="پذیرش A4 افقی"; var m=14,cw=d.paperW-2*m;
        it.push(L(m,m,cw,9,CLINIC,18,true,1)); it.push(HL(m,m+12,cw,0.5));
        var col=cw/2-6;
        body(it,m,m+18,col,12); it.push(VL(m+col+6,m+18,d.paperH-m-18-14,0.3));
        it.push(F(m+col+12,m+18,col,7,"{queue}","نوبت: ",12,0));
        it.push(F(m+col+12,m+26,col,7,"{appttime}","ساعت نوبت: ",12,0)); break; }
      case 11:{ P("A5",0); d.name="پذیرش دندانپزشکی"; var m=10,cw=d.paperW-2*m;
        it.push(L(m,m,cw,9,CLINIC,15,true,1)); it.push(L(m,m+11,cw,6,"بخش دندانپزشکی",11,false,1));
        it.push(HL(m,m+19,cw,0.4)); body(it,m,m+22,cw,11); break; }
      case 12:{ P("A5",0); d.name="پذیرش آزمایشگاه"; var m=10,cw=d.paperW-2*m;
        it.push(L(m,m,cw,9,CLINIC,15,true,1)); it.push(L(m,m+11,cw,6,"آزمایشگاه تشخیص طبی",11,false,1));
        it.push(HL(m,m+19,cw,0.4)); body(it,m,m+22,cw,11); it.push(APPT(m,d.paperH-24,cw,12,20)); break; }
      case 13:{ P("A4",0); d.name="پذیرش رادیولوژی A4"; var m=14,cw=d.paperW-2*m;
        it.push(FRAME(d.paperW,d.paperH,8,0.6)); it.push(L(m,m,cw,9,CLINIC,17,true,1));
        it.push(L(m,m+11,cw,6,"بخش تصویربرداری",11,false,1)); it.push(HL(m,m+20,cw,0.5));
        body(it,m,m+26,cw,12); break; }
      case 14:{ P("A5",0); d.name="پذیرش با عکس بیمار"; var m=10,cw=d.paperW-2*m;
        it.push(L(m,m,cw-28,9,CLINIC,14,true,0)); it.push(item({type:"photo",x:d.paperW-m-24,y:m,w:24,h:30,borderWidth:0.3}));
        it.push(HL(m,m+34,cw,0.4)); body(it,m,m+38,cw,11); break; }
      case 15:{ P("A5",0); d.name="رسید بیمه A5"; var m=10,cw=d.paperW-2*m;
        it.push(L(m,m,cw,9,CLINIC,15,true,1)); it.push(L(m,m+11,cw,6,"رسید بیمه",11,false,1));
        it.push(HL(m,m+19,cw,0.4));
        it.push(F(m,m+22,cw,7,"{full}","بیمار: ",11,0)); it.push(F(m,m+29,cw,7,"{ins}","بیمه: ",11,0));
        it.push(F(m,m+36,cw,7,"{insno}","شماره دفترچه: ",11,0)); it.push(F(m,m+43,cw,7,"{insshare}","سهم بیمه: ",11,0)); break; }
      case 16:{ P("A4",0); d.name="پذیرش دو نسخه‌ای A4"; var m=12,cw=d.paperW-2*m,half=(d.paperH-2*m)/2;
        for(var k=0;k<2;k++){ var yy=m+k*half;
          it.push(L(m,yy,cw,8,CLINIC,14,true,1)); it.push(HL(m,yy+10,cw,0.4));
          it.push(F(m,yy+13,cw,7,"{full}","نام: ",11,0)); it.push(F(m,yy+20,cw,7,"{doctor}","پزشک: ",11,0));
          it.push(F(m,yy+27,cw,7,"{queue}","نوبت: ",11,0)); }
        it.push(HL(m,m+half,cw,0.5)); break; }
      case 17:{ P("A6",1); d.name="کارت نوبت افقی A6"; var m=6,cw=d.paperW-2*m;
        it.push(FRAME(d.paperW,d.paperH,3,0.5)); it.push(L(m,m,cw,7,CLINIC,12,true,1));
        it.push(APPT(m,m+10,cw/2,16,30)); it.push(F(m+cw/2,m+12,cw/2,6,"{full}","",11,0));
        it.push(F(m+cw/2,m+20,cw/2,6,"{doctor}","پزشک: ",9,0)); break; }
      case 18:{ P("A5",0); d.name="پذیرش VIP طلایی"; var m=10,cw=d.paperW-2*m;
        var fr=FRAME(d.paperW,d.paperH,6,1.0); fr.borderColor="#b8860b"; it.push(fr);
        var t=L(m,m,cw,10,CLINIC,17,true,1); t.textColor="#9a6b00"; it.push(t);
        it.push(L(m,m+12,cw,6,"پذیرش ویژه",11,false,1)); it.push(HL(m,m+20,cw,0.5));
        body(it,m,m+24,cw,11); break; }
      case 19:{ P("A4",0); d.name="پذیرش کامل اداری A4"; var m=14,cw=d.paperW-2*m;
        it.push(FRAME(d.paperW,d.paperH,8,0.7)); it.push(LOGO(m,m,20,20));
        it.push(L(m+24,m,cw-24,8,CLINIC,17,true,0)); it.push(L(m+24,m+9,cw-24,6,"فرم پذیرش بیمار",10,false,0));
        it.push(F(m,m+18,cw/2,6,"{date}","تاریخ: ",10,0)); it.push(F(m+cw/2,m+18,cw/2,6,"{receiptNo}","شماره: ",10,2));
        it.push(HL(m,m+26,cw,0.5)); body(it,m,m+30,cw,12);
        it.push(QR(d.paperW-m-24,d.paperH-m-24,24)); it.push(APPT(m,d.paperH-m-16,40,12,20)); break; }
      default:{ P("A5",0); d.name="خالی"; }
    }
    return d;
  }

  var arr=[];
  for(var i=0;i<20;i++) arr.push(build(i));
  window.AZ_TEMPLATES = arr;
})();
