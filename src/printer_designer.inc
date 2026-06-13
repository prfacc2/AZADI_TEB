// ============================================================================
//  printer_designer.inc — visual WYSIWYG print designer (v1.4.0)
//  #included into printer.cpp (it relies on Design/DItem/FIELDS/templates and
//  the save/load helpers defined there).
//
//  Layout:
//    ┌───────────────────────────────────────────────────────────┐
//    │  toolbar (close, template ▾, save, backup, restore, zoom)   │
//    ├──────────────────────────────┬────────────────────────────┤
//    │                              │  RIGHT RAIL                  │
//    │   LEFT CANVAS                │   • add: label/text/line/    │
//    │   A4 / A5 page, draggable    │     border/logo              │
//    │   items, snapping guides     │   • field bindings (blue)    │
//    │                              │   • selected-item properties │
//    └──────────────────────────────┴────────────────────────────┘
// ============================================================================

#define PD_CLASS L"AzPrintDesigner"

// ---- right-rail action ids -------------------------------------------------
enum {
    PD_CLOSE=1, PD_SAVE, PD_BACKUP, PD_RESTORE, PD_ZOOMIN, PD_ZOOMOUT,
    PD_TPL_PREV, PD_TPL_NEXT, PD_PAPER,
    PD_ADD_LABEL, PD_ADD_TEXT, PD_ADD_LINEH, PD_ADD_LINEV, PD_ADD_BORDER, PD_ADD_LOGO,
    PD_DEL_ITEM,
    // property toggles for the selected item
    PD_P_BOLD, PD_P_ITALIC, PD_P_UNDER, PD_P_STRIKE,
    PD_P_FSMINUS, PD_P_FSPLUS, PD_P_COLOR, PD_P_LINECOLOR,
    PD_P_LSTYLE, PD_P_LOGOPICK,
    PD_P_BGCOLOR, PD_P_BGCLEAR, PD_P_ALIGN, PD_RESETVIEW,
    // field bind chips start here
    PD_FIELD_BASE=400,
    // template list (restore) start here
    PD_TPLITEM_BASE=600,
};

struct DesignerState {
    HWND owner;
    int  section;
    Design d;
    int  sel;            // selected item index, -1 none
    int  tplIdx;         // current template index in dropdown
    double zoom;         // canvas zoom (px per mm)
    int  hot;
    double panX, panY;   // canvas pan offset (px) applied to the page origin
    // drag
    bool dragging; double dragDX, dragDY;
    bool panning;  int panStartX, panStartY; double panStartPX, panStartPY;
    HWND eX, eY, eW, eH, eText, eFont, eFontSize; // property editors
    HWND eName;          // backup name
    bool showTplList;    // restore list open
    std::vector<std::wstring> backups;
};
static HWND s_des=NULL;
static DesignerState* s_ds=NULL;

// -------- geometry ----------------------------------------------------------
static int dsToolbarH(){ return S(52); }
static int dsRailW(){ return S(320); }
static RECT dsCanvasRect(HWND h){
    RECT rc; GetClientRect(h,&rc);
    RECT r={ rc.left+S(12), rc.top+dsToolbarH()+S(8),
             rc.right-dsRailW()-S(12), rc.bottom-S(12) };
    return r;
}
static RECT dsRailRect(HWND h){
    RECT rc; GetClientRect(h,&rc);
    RECT r={ rc.right-dsRailW()-S(4), rc.top+dsToolbarH()+S(8),
             rc.right-S(12), rc.bottom-S(12) };
    return r;
}
//  page placement inside the canvas (centered, zoomed)
static void dsPageMetrics(HWND h, RECT& page, double& zoom){
    RECT cv=dsCanvasRect(h);
    double pw,ph; paperMM(s_ds->d.paper,pw,ph);
    zoom=s_ds->zoom;
    int Wpx=(int)(pw*zoom), Hpx=(int)(ph*zoom);
    int x=cv.left+((cv.right-cv.left)-Wpx)/2 + (int)s_ds->panX;
    int y=cv.top +((cv.bottom-cv.top)-Hpx)/2 + (int)s_ds->panY;
    page.left=x; page.top=y; page.right=x+Wpx; page.bottom=y+Hpx;
}
//  mm → canvas pixel
static int dsX(const RECT& page, double zoom, double mm){ return page.left+(int)(mm*zoom); }
static int dsY(const RECT& page, double zoom, double mm){ return page.top +(int)(mm*zoom); }
//  canvas pixel → mm
static double dsMMx(const RECT& page, double zoom, int px){ return (px-page.left)/zoom; }
static double dsMMy(const RECT& page, double zoom, int px){ return (px-page.top)/zoom; }

// item bounding rect in canvas px (approximate for labels)
static RECT dsItemRect(const RECT& page, double zoom, const DItem& it){
    int x=dsX(page,zoom,it.x), y=dsY(page,zoom,it.y);
    if(it.kind==IT_LINE_H){ RECT r={x,y-S(4),dsX(page,zoom,it.x+it.w),y+S(4)}; return r; }
    if(it.kind==IT_LINE_V){ RECT r={x-S(4),y,x+S(4),dsY(page,zoom,it.y+it.h)}; return r; }
    if(it.kind==IT_BORDER||it.kind==IT_LOGO){
        RECT r={x,y,dsX(page,zoom,it.x+it.w),dsY(page,zoom,it.y+it.h)}; return r; }
    // label: width by mm width, height by font
    int wpx=(int)(it.w*zoom); if(wpx<S(40)) wpx=S(40);
    int hpx=(int)(it.fontSize*zoom*0.50)+S(6); if(hpx<S(14)) hpx=S(14);
    RECT r={x,y,x+wpx,y+hpx}; return r;
}

// -------- editors -----------------------------------------------------------
static void dsSyncEditors(HWND h);
static void dsLayoutEditors(HWND h);

static std::wstring fieldLabelFromText(const DItem& it){
    std::wstring s=it.text;
    if(!it.field.empty()){
        for(int i=0;i<N_FIELDS;i++)
            if(it.field==FIELDS[i].token){ s+=L"["; s+=FIELDS[i].label; s+=L"]"; break; }
    }
    if(s.empty()) s=L"(خالی)";
    return s;
}

// -------- list backups ------------------------------------------------------
static std::wstring backupsIndexPath(int sec){
    wchar_t b[40]; swprintf(b,40,L"\\design_%d_list.txt",sec); return dataDir()+b;
}
static std::vector<std::wstring> loadBackupNames(int sec){
    std::vector<std::wstring> out;
    std::wstring all=readFileUtf8(backupsIndexPath(sec));
    size_t pos=0;
    while(pos<all.size()){
        size_t e=all.find(L'\n',pos); if(e==std::wstring::npos) e=all.size();
        std::wstring s=trim(all.substr(pos,e-pos)); pos=e+1;
        if(!s.empty()) out.push_back(s);
    }
    return out;
}
static std::wstring backupSlotPath(int sec, const std::wstring& name){
    std::wstring safe=name; for(auto&c:safe) if(c==L'\\'||c==L'/'||c==L':'||c==L'\t') c=L'_';
    return dataDir()+L"\\design_"+std::to_wstring(sec)+L"_bk_"+safe+L".txt";
}
static void doBackup(HWND h){
    wchar_t nm[128]={0};
    if(s_ds->eName) GetWindowTextW(s_ds->eName,nm,128);
    std::wstring name=trim(nm);
    if(name.empty()){
        SYSTEMTIME st=iranNow(); wchar_t b[64];
        swprintf(b,64,L"طرح %s %02d:%02d",jalaliDateShort(st).c_str(),st.wHour,st.wMinute);
        name=b;
    }
    s_ds->d.name=name;
    writeFileUtf8(backupSlotPath(s_ds->section,name),serializeDesign(s_ds->d),false);
    // add to index if new
    auto names=loadBackupNames(s_ds->section);
    bool found=false; for(auto&n:names) if(n==name) found=true;
    if(!found){ std::wstring idx; for(auto&n:names) idx+=n+L"\r\n"; idx+=name+L"\r\n";
        writeFileUtf8(backupsIndexPath(s_ds->section),idx,false); }
    s_ds->backups=loadBackupNames(s_ds->section);
    MessageBoxW(h,(L"طرح با نام «"+name+L"» ذخیره شد.").c_str(),
        L"پشتیبان طرح",MB_OK|MB_ICONINFORMATION);
    InvalidateRect(h,NULL,FALSE);
}
static void doRestore(HWND h, const std::wstring& name){
    Design d; std::wstring all=readFileUtf8(backupSlotPath(s_ds->section,name));
    if(all.empty() || !parseDesign(all,d)){
        MessageBoxW(h,L"بازیابی طرح ناموفق بود.",L"بازیابی",MB_OK|MB_ICONWARNING); return;
    }
    s_ds->d=d; s_ds->sel=-1; s_ds->showTplList=false;
    dsSyncEditors(h); InvalidateRect(h,NULL,FALSE);
}

// -------- painting ----------------------------------------------------------
static void dsToolBtn(HDC dc, RECT r, int id, const wchar_t* t, int icon, bool primary){
    bool hov=(s_ds->hot==id);
    COLORREF bg = primary? (hov?g_theme.accentHover:g_theme.accent)
                         : (hov?g_theme.hover:g_theme.surface2);
    gpRoundRect(dc,r,S(8),bg,primary?bg:g_theme.border,255);
    COLORREF tc=primary?g_theme.accentText:g_theme.text;
    SetTextColor(dc,tc);
    if(icon){ RECT ir={r.left+S(6),r.top+S(6),r.left+S(26),r.bottom-S(6)};
        drawIcon(dc,icon,ir,tc,S(2)); }
    SelectObject(dc,g_fSmall);
    RECT nr={r.left+(icon?S(28):S(6)),r.top,r.right-S(6),r.bottom};
    DrawTextW(dc,t,-1,&nr,DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
}

static void dsPaintToolbar(HDC dc, HWND h){
    RECT rc; GetClientRect(h,&rc);
    RECT tb={0,0,rc.right,dsToolbarH()};
    gpGradRoundRect(dc,tb,0,g_theme.headerTop,g_theme.headerBot,CLR_INVALID);
    SetBkMode(dc,TRANSPARENT);
    // close
    RECT cb={S(10),S(12),S(40),S(42)};
    if(s_ds->hot==PD_CLOSE) gpRoundRect(dc,cb,S(8),g_theme.hover,CLR_INVALID,255);
    RECT ci={cb.left+S(6),cb.top+S(6),cb.right-S(6),cb.bottom-S(6)};
    drawIcon(dc,ICO_X,ci,RGB(255,255,255),S(2));
    // title (section)
    SelectObject(dc,g_fUIB); SetTextColor(dc,RGB(255,255,255));
    RECT tr={rc.right-S(280),S(8),rc.right-S(12),dsToolbarH()-S(8)};
    std::wstring tt=std::wstring(L"طراح چاپ — ")+
        PRINT_SECTIONS[s_ds->section<N_PRINT_SECTIONS?s_ds->section:0];
    DrawTextW(dc,tt.c_str(),-1,&tr,DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
    // buttons row
    int bx=S(50), by=S(10), bh=S(32);
    auto put=[&](int id,const wchar_t* t,int icon,int w,bool primary){
        RECT r={bx,by,bx+w,by+bh}; dsToolBtn(dc,r,id,t,icon,primary); bx+=w+S(6); };
    put(PD_SAVE,L"ذخیره",ICO_SAVE,S(70),true);
    put(PD_BACKUP,L"پشتیبان",ICO_PLUS,S(78),false);
    put(PD_RESTORE,L"بازیابی",ICO_UPDATE,S(78),false);
    // template prev/next
    { RECT rp={bx,by,bx+S(26),by+bh}; dsToolBtn(dc,rp,PD_TPL_PREV,L"‹",0,false); bx+=S(28);
      RECT rt={bx,by,bx+S(140),by+bh};
      gpRoundRect(dc,rt,S(8),g_theme.surface,g_theme.border,255);
      SetTextColor(dc,g_theme.text); SelectObject(dc,g_fSmall);
      DrawTextW(dc,TEMPLATE_NAMES[s_ds->tplIdx%N_TEMPLATES],-1,&rt,
        DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX); bx+=S(142);
      RECT rn={bx,by,bx+S(26),by+bh}; dsToolBtn(dc,rn,PD_TPL_NEXT,L"›",0,false); bx+=S(28); }
    put(PD_PAPER,s_ds->d.paper?L"A5":L"A4",0,S(50),false);
    put(PD_ZOOMOUT,L"−",0,S(34),false);
    put(PD_ZOOMIN,L"+",0,S(34),false);
    put(PD_RESETVIEW,L"نمای اصلی",0,S(80),false);
}

static void dsPaintCanvas(HDC dc, HWND h){
    RECT cv=dsCanvasRect(h);
    gpRoundRect(dc,cv,S(10),g_dark?RGB(24,28,34):RGB(225,230,238),g_theme.border,255);
    RECT page; double zoom; dsPageMetrics(h,page,zoom);
    // page shadow + white sheet
    gpShadow(dc,page,S(2),S(14),70);
    { HBRUSH wb=CreateSolidBrush(RGB(255,255,255)); FillRect(dc,&page,wb); DeleteObject(wb); }
    { HPEN p=CreatePen(PS_SOLID,1,RGB(160,160,160)); HGDIOBJ o=SelectObject(dc,p);
      HGDIOBJ ob=SelectObject(dc,GetStockObject(NULL_BRUSH));
      Rectangle(dc,page.left,page.top,page.right,page.bottom);
      SelectObject(dc,ob); SelectObject(dc,o); DeleteObject(p); }

    // center guide when dragging near center
    double pw,ph; paperMM(s_ds->d.paper,pw,ph);
    if(s_ds->dragging && s_ds->sel>=0){
        DItem& it=s_ds->d.items[s_ds->sel];
        double cxmm=it.x+it.w/2;
        if(fabs(cxmm-pw/2)<2.0){
            int gx=dsX(page,zoom,pw/2);
            HPEN p=CreatePen(PS_DOT,1,RGB(0,150,255)); HGDIOBJ o=SelectObject(dc,p);
            MoveToEx(dc,gx,page.top,0); LineTo(dc,gx,page.bottom);
            SelectObject(dc,o); DeleteObject(p);
        }
    }

    SetBkMode(dc,TRANSPARENT);
    for(size_t i=0;i<s_ds->d.items.size();i++){
        DItem& it=s_ds->d.items[i];
        RECT ir=dsItemRect(page,zoom,it);
        if(it.kind==IT_LABEL){
            std::wstring s=it.text;
            if(!it.field.empty()){
                for(int k=0;k<N_FIELDS;k++)
                    if(it.field==FIELDS[k].token){ s+=FIELDS[k].label; break; }
            }
            if(s.empty()) s=L"…";
            // background fill
            if(it.bgColor!=CLR_INVALID){
                RECT bg={ir.left,ir.top,dsX(page,zoom,it.x+it.w),
                         ir.top+(int)(it.fontSize*zoom*0.55)+S(4)};
                HBRUSH bb=CreateSolidBrush(it.bgColor); FillRect(dc,&bg,bb); DeleteObject(bb);
            }
            int lf=-(int)(it.fontSize*zoom*0.42); if(lf>-8) lf=-8;
            HFONT f=CreateFontW(lf,0,0,0,it.bold?FW_BOLD:FW_NORMAL,
                it.italic?1:0,it.underline?1:0,it.strike?1:0,DEFAULT_CHARSET,
                0,0,CLEARTYPE_QUALITY,0,
                it.fontName.empty()?L"Vazirmatn":it.fontName.c_str());
            HGDIOBJ of=SelectObject(dc,f);
            // bound text drawn in BLUE so user knows it is a field
            SetTextColor(dc, it.field.empty()? it.color : RGB(20,90,200));
            RECT tr={ir.left,ir.top,dsX(page,zoom,it.x+it.w),ir.bottom+S(40)};
            UINT al=(it.align==1)?DT_CENTER:(it.align==2)?DT_LEFT:DT_RIGHT;
            DrawTextW(dc,s.c_str(),-1,&tr,
                al|DT_TOP|DT_WORDBREAK|DT_RTLREADING|DT_NOPREFIX);
            SelectObject(dc,of); DeleteObject(f);
        } else if(it.kind==IT_LINE_H){
            int st=it.lineStyle==1?PS_DASH:it.lineStyle==2?PS_DOT:PS_SOLID;
            HPEN p=CreatePen(st,(int)(it.lineW*zoom)<1?1:(int)(it.lineW*zoom),it.color);
            HGDIOBJ o=SelectObject(dc,p);
            int y=dsY(page,zoom,it.y);
            MoveToEx(dc,dsX(page,zoom,it.x),y,0); LineTo(dc,dsX(page,zoom,it.x+it.w),y);
            SelectObject(dc,o); DeleteObject(p);
        } else if(it.kind==IT_LINE_V){
            int st=it.lineStyle==1?PS_DASH:it.lineStyle==2?PS_DOT:PS_SOLID;
            HPEN p=CreatePen(st,(int)(it.lineW*zoom)<1?1:(int)(it.lineW*zoom),it.color);
            HGDIOBJ o=SelectObject(dc,p);
            int x=dsX(page,zoom,it.x);
            MoveToEx(dc,x,dsY(page,zoom,it.y),0); LineTo(dc,x,dsY(page,zoom,it.y+it.h));
            SelectObject(dc,o); DeleteObject(p);
        } else if(it.kind==IT_BORDER){
            int st=it.lineStyle==1?PS_DASH:it.lineStyle==2?PS_DOT:PS_SOLID;
            HPEN p=CreatePen(st,(int)(it.lineW*zoom)<1?1:(int)(it.lineW*zoom),it.borderColor);
            HGDIOBJ o=SelectObject(dc,p);
            HGDIOBJ ob=SelectObject(dc,GetStockObject(NULL_BRUSH));
            Rectangle(dc,ir.left,ir.top,ir.right,ir.bottom);
            SelectObject(dc,ob); SelectObject(dc,o); DeleteObject(p);
        } else if(it.kind==IT_LOGO){
            HBRUSH lb=CreateSolidBrush(RGB(238,242,248)); FillRect(dc,&ir,lb); DeleteObject(lb);
            HPEN p=CreatePen(PS_DOT,1,RGB(120,120,120)); HGDIOBJ o=SelectObject(dc,p);
            HGDIOBJ ob=SelectObject(dc,GetStockObject(NULL_BRUSH));
            Rectangle(dc,ir.left,ir.top,ir.right,ir.bottom);
            SelectObject(dc,ob); SelectObject(dc,o); DeleteObject(p);
            SetTextColor(dc,RGB(120,120,120)); SelectObject(dc,g_fSmall);
            DrawTextW(dc,L"لوگو",-1,&ir,DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_NOPREFIX);
        }
        // selection handles
        if((int)i==s_ds->sel){
            HPEN p=CreatePen(PS_SOLID,S(2),g_theme.accent); HGDIOBJ o=SelectObject(dc,p);
            HGDIOBJ ob=SelectObject(dc,GetStockObject(NULL_BRUSH));
            RECT sr=ir; InflateRect(&sr,S(3),S(3));
            Rectangle(dc,sr.left,sr.top,sr.right,sr.bottom);
            SelectObject(dc,ob); SelectObject(dc,o); DeleteObject(p);
        }
    }
    // ruler hint
    SetTextColor(dc,g_theme.textDim); SelectObject(dc,g_fSmall);
    wchar_t zb[48]; swprintf(zb,48,L"بزرگنمایی: %d%%   کاغذ: %s",
        (int)(zoom/((s_ds->d.paper?2.0:1.4))*100),s_ds->d.paper?L"A5":L"A4");
    RECT zr={cv.left+S(10),cv.bottom-S(24),cv.left+S(260),cv.bottom-S(4)};
    DrawTextW(dc,zb,-1,&zr,DT_LEFT|DT_SINGLELINE|DT_VCENTER|DT_NOPREFIX);
}

static void dsRailLabel(HDC dc, RECT r, const wchar_t* t){
    SetTextColor(dc,g_theme.textDim); SelectObject(dc,g_fSmall);
    DrawTextW(dc,t,-1,&r,DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
}
static void dsPaintRail(HDC dc, HWND h){
    RECT rail=dsRailRect(h);
    gpRoundRect(dc,rail,S(12),g_theme.surface,g_theme.border,255);
    SetBkMode(dc,TRANSPARENT);
    int x0=rail.left+S(12), x1=rail.right-S(12), y=rail.top+S(10);

    if(s_ds->showTplList){
        // restore list
        SelectObject(dc,g_fUIB); SetTextColor(dc,g_theme.text);
        RECT tr={x0,y,x1,y+S(24)};
        DrawTextW(dc,L"بازیابی طرح ذخیره‌شده",-1,&tr,
            DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
        y+=S(30);
        if(s_ds->backups.empty()){
            RECT er={x0,y,x1,y+S(30)}; SetTextColor(dc,g_theme.textDim);
            SelectObject(dc,g_fSmall);
            DrawTextW(dc,L"هنوز طرحی ذخیره نشده است.",-1,&er,
                DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
        }
        for(size_t i=0;i<s_ds->backups.size();i++){
            RECT r={x0,y,x1,y+S(34)};
            bool hov=(s_ds->hot==PD_TPLITEM_BASE+(int)i);
            gpRoundRect(dc,r,S(8),hov?g_theme.hover:g_theme.surface2,g_theme.border,255);
            SetTextColor(dc,g_theme.text); SelectObject(dc,g_fSmall);
            RECT nr={r.left+S(10),r.top,r.right-S(10),r.bottom};
            DrawTextW(dc,s_ds->backups[i].c_str(),-1,&nr,
                DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
            y+=S(38);
        }
        return;
    }

    // ---- add items ----
    SelectObject(dc,g_fUIB); SetTextColor(dc,g_theme.text);
    RECT hr={x0,y,x1,y+S(22)};
    DrawTextW(dc,L"افزودن عنصر",-1,&hr,DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
    y+=S(28);
    int bw=(x1-x0-S(8))/2;
    auto add=[&](int id,const wchar_t* t,int icon,int col,int row){
        RECT r={x0+col*(bw+S(8)),y+row*S(40),x0+col*(bw+S(8))+bw,y+row*S(40)+S(34)};
        dsToolBtn(dc,r,id,t,icon,false); };
    add(PD_ADD_LABEL,L"فیلد/برچسب",ICO_ID,1,0);
    add(PD_ADD_TEXT, L"متن آزاد",  ICO_RECEIPT,0,0);
    add(PD_ADD_LINEH,L"خط افقی",   0,1,1);
    add(PD_ADD_LINEV,L"خط عمودی",  0,0,1);
    add(PD_ADD_BORDER,L"کادر",     0,1,2);
    add(PD_ADD_LOGO, L"لوگو",      ICO_PLUS,0,2);
    y+=3*S(40)+S(6);

    // ---- field bindings ----
    SelectObject(dc,g_fUIB); SetTextColor(dc,g_theme.text);
    RECT fr={x0,y,x1,y+S(22)};
    DrawTextW(dc,L"اتصال فیلد (روی برچسب انتخاب‌شده):",-1,&fr,
        DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
    y+=S(26);
    int fx=x1, fy=y, chH=S(24);
    SelectObject(dc,g_fSmall);
    for(int i=0;i<N_FIELDS;i++){
        SIZE sz; GetTextExtentPoint32W(dc,FIELDS[i].label,
            (int)wcslen(FIELDS[i].label),&sz);
        int cwid=sz.cx+S(16);
        if(fx-cwid < x0){ fx=x1; fy+=chH+S(4); }
        RECT r={fx-cwid,fy,fx,fy+chH};
        bool hov=(s_ds->hot==PD_FIELD_BASE+i);
        bool boundHere=(s_ds->sel>=0 && s_ds->d.items[s_ds->sel].field==FIELDS[i].token);
        gpRoundRect(dc,r,S(11),boundHere?g_theme.accent:(hov?g_theme.hover:RGB(225,238,255)),
            boundHere?g_theme.accent:RGB(150,190,240),255);
        SetTextColor(dc,boundHere?g_theme.accentText:RGB(20,90,200));
        DrawTextW(dc,FIELDS[i].label,-1,&r,
            DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
        fx-=cwid+S(4);
    }
    y=fy+chH+S(10);

    // ---- properties of selected ----
    SelectObject(dc,g_fUIB); SetTextColor(dc,g_theme.text);
    RECT pr={x0,y,x1,y+S(22)};
    DrawTextW(dc,L"ویژگی‌های عنصر انتخاب‌شده",-1,&pr,
        DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
    y+=S(28);
    if(s_ds->sel<0){
        SetTextColor(dc,g_theme.textDim); SelectObject(dc,g_fSmall);
        RECT nr={x0,y,x1,y+S(30)};
        DrawTextW(dc,L"یک عنصر را روی صفحه انتخاب کنید.",-1,&nr,
            DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
        return;
    }
    DItem& it=s_ds->d.items[s_ds->sel];
    // X/Y/W/H labels (the edit boxes overlay)
    dsRailLabel(dc,{x1-S(70),y,x1,y+S(24)},L"X (mm)");
    dsRailLabel(dc,{x0,y,x0+S(80),y+S(24)},L"Y (mm)");
    y+=S(30);
    dsRailLabel(dc,{x1-S(70),y,x1,y+S(24)},L"عرض");
    dsRailLabel(dc,{x0,y,x0+S(80),y+S(24)},L"ارتفاع/متن");
    y+=S(30);
    // text + font labels handled by editors; show style toggles
    y+=S(30);  // text editor row
    y+=S(30);  // font editor row
    // style toggle chips
    auto styBtn=[&](int id,const wchar_t* t,bool on,int idx){
        int w=S(40); RECT r={x1-(idx+1)*(w+S(4))+S(4),y,x1-idx*(w+S(4)),y+S(28)};
        bool hov=(s_ds->hot==id);
        gpRoundRect(dc,r,S(7),on?g_theme.accent:(hov?g_theme.hover:g_theme.surface2),
            on?g_theme.accent:g_theme.border,255);
        SetTextColor(dc,on?g_theme.accentText:g_theme.text); SelectObject(dc,g_fUIB);
        DrawTextW(dc,t,-1,&r,DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_NOPREFIX);
    };
    bool isText=(it.kind==IT_LABEL);
    if(isText){
        styBtn(PD_P_BOLD,L"B",it.bold,0);
        styBtn(PD_P_ITALIC,L"I",it.italic,1);
        styBtn(PD_P_UNDER,L"U",it.underline,2);
        styBtn(PD_P_STRIKE,L"S",it.strike,3);
        // font size +/-
        RECT fm={x0,y,x0+S(28),y+S(28)}; dsToolBtn(dc,fm,PD_P_FSMINUS,L"−",0,false);
        RECT fp={x0+S(32),y,x0+S(60),y+S(28)}; dsToolBtn(dc,fp,PD_P_FSPLUS,L"+",0,false);
        wchar_t fsb[16]; swprintf(fsb,16,L"%d",it.fontSize);
        SetTextColor(dc,g_theme.text); SelectObject(dc,g_fSmall);
        RECT fsr={x0+S(64),y,x0+S(100),y+S(28)};
        DrawTextW(dc,fsb,-1,&fsr,DT_LEFT|DT_SINGLELINE|DT_VCENTER|DT_NOPREFIX);
        y+=S(34);
        // color
        RECT cr={x1-S(120),y,x1,y+S(28)}; dsToolBtn(dc,cr,PD_P_COLOR,L"رنگ متن",0,false);
        { RECT sw={cr.left-S(34),y+S(4),cr.left-S(6),y+S(24)};
          HBRUSH b=CreateSolidBrush(it.color); FillRect(dc,&sw,b); DeleteObject(b);
          FrameRect(dc,&sw,(HBRUSH)GetStockObject(GRAY_BRUSH)); }
        y+=S(34);
        // background fill color + clear
        RECT bgr={x1-S(120),y,x1,y+S(28)}; dsToolBtn(dc,bgr,PD_P_BGCOLOR,L"رنگ زمینه",0,false);
        { RECT sw={bgr.left-S(34),y+S(4),bgr.left-S(6),y+S(24)};
          if(it.bgColor==CLR_INVALID){ FillRect(dc,&sw,(HBRUSH)GetStockObject(WHITE_BRUSH));
            SetTextColor(dc,RGB(150,150,150)); SelectObject(dc,g_fSmall);
            DrawTextW(dc,L"بدون",-1,&sw,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX); }
          else { HBRUSH b=CreateSolidBrush(it.bgColor); FillRect(dc,&sw,b); DeleteObject(b); }
          FrameRect(dc,&sw,(HBRUSH)GetStockObject(GRAY_BRUSH)); }
        RECT bgc={x0,y,x0+S(60),y+S(28)}; dsToolBtn(dc,bgc,PD_P_BGCLEAR,L"حذف زمینه",0,false);
        y+=S(34);
        // alignment
        const wchar_t* al[3]={L"چپ‌چین",L"وسط‌چین",L"راست‌چین"};
        // align stored 0=right,1=center,2=left → show label matching value
        const wchar_t* alLbl = (it.align==0)?L"راست‌چین":(it.align==1)?L"وسط‌چین":L"چپ‌چین";
        RECT ar={x0,y,x1,y+S(28)}; dsToolBtn(dc,ar,PD_P_ALIGN,alLbl,0,false);
        (void)al;
        y+=S(34);
    } else {
        // line/border: color + thickness + style
        RECT cr={x1-S(120),y,x1,y+S(28)}; dsToolBtn(dc,cr,PD_P_LINECOLOR,L"رنگ خط",0,false);
        { RECT sw={cr.left-S(34),y+S(4),cr.left-S(6),y+S(24)};
          HBRUSH b=CreateSolidBrush(it.kind==IT_BORDER?it.borderColor:it.color);
          FillRect(dc,&sw,b); DeleteObject(b);
          FrameRect(dc,&sw,(HBRUSH)GetStockObject(GRAY_BRUSH)); }
        RECT lm={x0,y,x0+S(28),y+S(28)}; dsToolBtn(dc,lm,PD_P_FSMINUS,L"−",0,false);
        RECT lp={x0+S(32),y,x0+S(60),y+S(28)}; dsToolBtn(dc,lp,PD_P_FSPLUS,L"+",0,false);
        y+=S(34);
        const wchar_t* ls[3]={L"خط ممتد",L"خط‌چین",L"نقطه‌چین"};
        RECT lsr={x0,y,x1,y+S(28)}; dsToolBtn(dc,lsr,PD_P_LSTYLE,ls[it.lineStyle%3],0,false);
        y+=S(34);
    }
    if(it.kind==IT_LOGO){
        RECT lr={x0,y,x1,y+S(30)}; dsToolBtn(dc,lr,PD_P_LOGOPICK,L"انتخاب تصویر لوگو (PNG)",ICO_PLUS,false);
        y+=S(36);
    }
    // delete
    RECT dr={x0,y,x1,y+S(32)};
    bool hovd=(s_ds->hot==PD_DEL_ITEM);
    gpRoundRect(dc,dr,S(8),hovd?g_theme.dangerHover:g_theme.danger,g_theme.danger,255);
    SetTextColor(dc,RGB(255,255,255)); SelectObject(dc,g_fUIB);
    DrawTextW(dc,L"حذف این عنصر",-1,&dr,DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
}

static void dsPaint(HWND h, HDC dc0){
    RECT rc; GetClientRect(h,&rc);
    HDC dc=CreateCompatibleDC(dc0);
    HBITMAP bmp=CreateCompatibleBitmap(dc0,rc.right,rc.bottom);
    HGDIOBJ obm=SelectObject(dc,bmp);
    { HBRUSH b=CreateSolidBrush(g_theme.bg); FillRect(dc,&rc,b); DeleteObject(b); }
    dsPaintCanvas(dc,h);
    dsPaintRail(dc,h);
    dsPaintToolbar(dc,h);
    BitBlt(dc0,0,0,rc.right,rc.bottom,dc,0,0,SRCCOPY);
    SelectObject(dc,obm); DeleteObject(bmp); DeleteDC(dc);
}

// -------- editor layout & sync ----------------------------------------------
static void dsLayoutEditors(HWND h){
    RECT rail=dsRailRect(h);
    int x0=rail.left+S(12), x1=rail.right-S(12);
    // these positions mirror the property block in dsPaintRail
    // We recompute the y where properties begin by replicating logic roughly:
    // Simpler: hide editors when no selection or on tpl list.
    if(s_ds->sel<0 || s_ds->showTplList){
        ShowWindow(s_ds->eX,SW_HIDE); ShowWindow(s_ds->eY,SW_HIDE);
        ShowWindow(s_ds->eW,SW_HIDE); ShowWindow(s_ds->eH,SW_HIDE);
        ShowWindow(s_ds->eText,SW_HIDE); ShowWindow(s_ds->eFont,SW_HIDE);
        ShowWindow(s_ds->eName,s_ds->showTplList?SW_HIDE:SW_HIDE);
        return;
    }
    // The properties header sits after: add(3 rows) + fields(variable). Because
    // field chips wrap, we approximate using a stored start computed in paint.
    // To keep it robust we position editors relative to rail bottom area.
    int by = rail.bottom - S(250);
    int half=(x1-x0-S(8))/2;
    MoveWindow(s_ds->eX, x1-half, by, half, S(24), TRUE);
    MoveWindow(s_ds->eY, x0,      by, half, S(24), TRUE);
    by+=S(30);
    MoveWindow(s_ds->eW, x1-half, by, half, S(24), TRUE);
    MoveWindow(s_ds->eH, x0,      by, half, S(24), TRUE);
    by+=S(30);
    MoveWindow(s_ds->eText, x0, by, x1-x0, S(24), TRUE);
    by+=S(30);
    MoveWindow(s_ds->eFont, x0, by, x1-x0, S(24), TRUE);
    ShowWindow(s_ds->eX,SW_SHOW); ShowWindow(s_ds->eY,SW_SHOW);
    ShowWindow(s_ds->eW,SW_SHOW); ShowWindow(s_ds->eH,SW_SHOW);
    DItem& it=s_ds->d.items[s_ds->sel];
    ShowWindow(s_ds->eText, it.kind==IT_LABEL?SW_SHOW:SW_HIDE);
    ShowWindow(s_ds->eFont, it.kind==IT_LABEL?SW_SHOW:SW_HIDE);
    ShowWindow(s_ds->eName,SW_HIDE);
}
static void dsSyncEditors(HWND h){
    if(s_ds->sel<0){ dsLayoutEditors(h); return; }
    DItem& it=s_ds->d.items[s_ds->sel];
    wchar_t b[64];
    swprintf(b,64,L"%.1f",it.x); SetWindowTextW(s_ds->eX,b);
    swprintf(b,64,L"%.1f",it.y); SetWindowTextW(s_ds->eY,b);
    swprintf(b,64,L"%.1f",it.w); SetWindowTextW(s_ds->eW,b);
    swprintf(b,64,L"%.1f",it.h); SetWindowTextW(s_ds->eH,b);
    SetWindowTextW(s_ds->eText,it.text.c_str());
    SetWindowTextW(s_ds->eFont,it.fontName.c_str());
    dsLayoutEditors(h);
}
static void dsReadEditors(){
    if(!s_ds || s_ds->sel<0) return;
    DItem& it=s_ds->d.items[s_ds->sel];
    wchar_t b[128];
    GetWindowTextW(s_ds->eX,b,128); it.x=_wtof(b);
    GetWindowTextW(s_ds->eY,b,128); it.y=_wtof(b);
    GetWindowTextW(s_ds->eW,b,128); it.w=_wtof(b);
    GetWindowTextW(s_ds->eH,b,128); it.h=_wtof(b);
    GetWindowTextW(s_ds->eText,b,128); it.text=b;
    GetWindowTextW(s_ds->eFont,b,128); { std::wstring f=trim(b); it.fontName=f.empty()?L"Vazirmatn":f; }
}

// -------- helpers: add item, pick color/logo --------------------------------
static void dsAdd(HWND h, int kind, bool bindable){
    double pw,ph; paperMM(s_ds->d.paper,pw,ph);
    DItem it; it.kind=kind; it.x=pw/2-25; it.y=20; it.w=50; it.h=8;
    if(kind==IT_LINE_H){ it.x=10; it.w=pw-20; it.h=0; it.lineW=0.3; it.color=RGB(60,60,60); }
    else if(kind==IT_LINE_V){ it.y=10; it.h=40; it.w=0; it.lineW=0.3; it.color=RGB(60,60,60); }
    else if(kind==IT_BORDER){ it.x=8; it.y=8; it.w=pw-16; it.h=40; it.lineW=0.4; }
    else if(kind==IT_LOGO){ it.x=10; it.y=10; it.w=25; it.h=25; }
    else if(kind==IT_LABEL){ it.text= bindable?L"":L"متن نمونه"; it.fontSize=11; }
    s_ds->d.items.push_back(it);
    s_ds->sel=(int)s_ds->d.items.size()-1;
    dsSyncEditors(h); InvalidateRect(h,NULL,FALSE);
}
static bool dsPickColor(HWND h, COLORREF& io){
    static COLORREF custom[16]={0};
    CHOOSECOLORW cc={0}; cc.lStructSize=sizeof(cc); cc.hwndOwner=h;
    cc.lpCustColors=custom; cc.rgbResult=io; cc.Flags=CC_FULLOPEN|CC_RGBINIT;
    if(ChooseColorW(&cc)){ io=cc.rgbResult; return true; }
    return false;
}

// -------- hit testing on rail/toolbar ---------------------------------------
static int dsHitToolbar(HWND h, POINT pt){
    RECT cb={S(10),S(12),S(40),S(42)}; if(PtInRect(&cb,pt)) return PD_CLOSE;
    if(pt.y>dsToolbarH()) return 0;
    int bx=S(50), by=S(10), bh=S(32);
    auto chk=[&](int w)->bool{ RECT r={bx,by,bx+w,by+bh}; bool in=PtInRect(&r,pt); bx+=w+S(6); return in; };
    if(chk(S(70))) return PD_SAVE;
    if(chk(S(78))) return PD_BACKUP;
    if(chk(S(78))) return PD_RESTORE;
    { RECT rp={bx,by,bx+S(26),by+bh}; if(PtInRect(&rp,pt)) return PD_TPL_PREV; bx+=S(28); }
    bx+=S(142);
    { RECT rn={bx,by,bx+S(26),by+bh}; if(PtInRect(&rn,pt)) return PD_TPL_NEXT; bx+=S(28); }
    if(chk(S(50))) return PD_PAPER;
    if(chk(S(34))) return PD_ZOOMOUT;
    if(chk(S(34))) return PD_ZOOMIN;
    if(chk(S(80))) return PD_RESETVIEW;
    return 0;
}
static int dsHitRail(HWND h, POINT pt){
    RECT rail=dsRailRect(h);
    if(!PtInRect(&rail,pt)) return 0;
    int x0=rail.left+S(12), x1=rail.right-S(12), y=rail.top+S(10);

    if(s_ds->showTplList){
        y+=S(30);
        for(size_t i=0;i<s_ds->backups.size();i++){
            RECT r={x0,y,x1,y+S(34)}; if(PtInRect(&r,pt)) return PD_TPLITEM_BASE+(int)i;
            y+=S(38);
        }
        return 0;
    }
    y+=S(28);
    int bw=(x1-x0-S(8))/2;
    struct AB{int id,col,row;} abs[]={
        {PD_ADD_LABEL,1,0},{PD_ADD_TEXT,0,0},{PD_ADD_LINEH,1,1},
        {PD_ADD_LINEV,0,1},{PD_ADD_BORDER,1,2},{PD_ADD_LOGO,0,2}};
    for(auto&a:abs){
        RECT r={x0+a.col*(bw+S(8)),y+a.row*S(40),
                x0+a.col*(bw+S(8))+bw,y+a.row*S(40)+S(34)};
        if(PtInRect(&r,pt)) return a.id;
    }
    y+=3*S(40)+S(6);
    y+=S(26);
    // field chips
    HDC dc=GetDC(h); HGDIOBJ of=SelectObject(dc,g_fSmall);
    int fx=x1, fy=y, chH=S(24);
    for(int i=0;i<N_FIELDS;i++){
        SIZE sz; GetTextExtentPoint32W(dc,FIELDS[i].label,(int)wcslen(FIELDS[i].label),&sz);
        int cwid=sz.cx+S(16);
        if(fx-cwid<x0){ fx=x1; fy+=chH+S(4); }
        RECT r={fx-cwid,fy,fx,fy+chH};
        if(PtInRect(&r,pt)){ SelectObject(dc,of); ReleaseDC(h,dc); return PD_FIELD_BASE+i; }
        fx-=cwid+S(4);
    }
    SelectObject(dc,of); ReleaseDC(h,dc);
    y=fy+chH+S(10);
    // properties
    if(s_ds->sel<0) return 0;
    DItem& it=s_ds->d.items[s_ds->sel];
    y+=S(28);            // header
    y+=S(30)+S(30);      // X/Y + W/H labels
    y+=S(30)+S(30);      // text + font editor rows
    bool isText=(it.kind==IT_LABEL);
    if(isText){
        for(int idx=0;idx<4;idx++){
            int w=S(40); RECT r={x1-(idx+1)*(w+S(4))+S(4),y,x1-idx*(w+S(4)),y+S(28)};
            if(PtInRect(&r,pt)) return PD_P_BOLD+idx;
        }
        RECT fm={x0,y,x0+S(28),y+S(28)}; if(PtInRect(&fm,pt)) return PD_P_FSMINUS;
        RECT fp={x0+S(32),y,x0+S(60),y+S(28)}; if(PtInRect(&fp,pt)) return PD_P_FSPLUS;
        y+=S(34);
        RECT cr={x1-S(120),y,x1,y+S(28)}; if(PtInRect(&cr,pt)) return PD_P_COLOR;
        y+=S(34);
        RECT bgr={x1-S(120),y,x1,y+S(28)}; if(PtInRect(&bgr,pt)) return PD_P_BGCOLOR;
        RECT bgc={x0,y,x0+S(60),y+S(28)}; if(PtInRect(&bgc,pt)) return PD_P_BGCLEAR;
        y+=S(34);
        RECT ar={x0,y,x1,y+S(28)}; if(PtInRect(&ar,pt)) return PD_P_ALIGN;
        y+=S(34);
    } else {
        RECT cr={x1-S(120),y,x1,y+S(28)}; if(PtInRect(&cr,pt)) return PD_P_LINECOLOR;
        RECT lm={x0,y,x0+S(28),y+S(28)}; if(PtInRect(&lm,pt)) return PD_P_FSMINUS;
        RECT lp={x0+S(32),y,x0+S(60),y+S(28)}; if(PtInRect(&lp,pt)) return PD_P_FSPLUS;
        y+=S(34);
        RECT lsr={x0,y,x1,y+S(28)}; if(PtInRect(&lsr,pt)) return PD_P_LSTYLE;
        y+=S(34);
    }
    if(it.kind==IT_LOGO){ RECT lr={x0,y,x1,y+S(30)}; if(PtInRect(&lr,pt)) return PD_P_LOGOPICK; y+=S(36); }
    RECT dr={x0,y,x1,y+S(32)}; if(PtInRect(&dr,pt)) return PD_DEL_ITEM;
    return 0;
}
//  canvas hit → item index, -1 none
static int dsHitCanvas(HWND h, POINT pt){
    RECT cv=dsCanvasRect(h); if(!PtInRect(&cv,pt)) return -2;  // not canvas
    RECT page; double zoom; dsPageMetrics(h,page,zoom);
    for(int i=(int)s_ds->d.items.size()-1;i>=0;i--){
        RECT ir=dsItemRect(page,zoom,s_ds->d.items[i]);
        InflateRect(&ir,S(4),S(4));
        if(PtInRect(&ir,pt)) return i;
    }
    return -1;
}

static void dsClose(){
    if(s_des && IsWindow(s_des)){ HWND v=s_des; s_des=NULL; DestroyWindow(v); }
}

static LRESULT CALLBACK desProc(HWND h, UINT m, WPARAM w, LPARAM l){
    switch(m){
    case WM_ERASEBKGND: return 1;
    case WM_PAINT:{ PAINTSTRUCT ps; HDC dc=BeginPaint(h,&ps); dsPaint(h,dc); EndPaint(h,&ps); return 0; }
    case WM_APP_THEME: InvalidateRect(h,NULL,FALSE); return 0;
    case WM_SIZE: if(s_ds){ dsLayoutEditors(h);} InvalidateRect(h,NULL,FALSE); return 0;
    case WM_MOUSEMOVE:{
        POINT pt={GET_X_LPARAM(l),GET_Y_LPARAM(l)};
        if(s_ds->panning){
            s_ds->panX = s_ds->panStartPX + (pt.x - s_ds->panStartX);
            s_ds->panY = s_ds->panStartPY + (pt.y - s_ds->panStartY);
            InvalidateRect(h,NULL,FALSE);
            return 0;
        }
        if(s_ds->dragging && s_ds->sel>=0){
            RECT page; double zoom; dsPageMetrics(h,page,zoom);
            double mx=dsMMx(page,zoom,pt.x)-s_ds->dragDX;
            double my=dsMMy(page,zoom,pt.y)-s_ds->dragDY;
            // snapping to center & to 5mm grid
            double pw,ph; paperMM(s_ds->d.paper,pw,ph);
            DItem& it=s_ds->d.items[s_ds->sel];
            double cx=mx+it.w/2;
            if(fabs(cx-pw/2)<2.0) mx=pw/2-it.w/2;
            mx=floor(mx*2+0.5)/2; my=floor(my*2+0.5)/2;
            if(mx<0)mx=0; if(my<0)my=0;
            it.x=mx; it.y=my;
            dsSyncEditors(h); InvalidateRect(h,NULL,FALSE);
            return 0;
        }
        int hr=dsHitToolbar(h,pt); if(!hr) hr=dsHitRail(h,pt);
        if(s_ds->hot!=hr){ s_ds->hot=hr; InvalidateRect(h,NULL,FALSE); }
        return 0; }
    case WM_LBUTTONDOWN:{
        POINT pt={GET_X_LPARAM(l),GET_Y_LPARAM(l)};
        SetFocus(h);
        int t=dsHitToolbar(h,pt);
        if(t){
            switch(t){
            case PD_CLOSE: dsClose(); return 0;
            case PD_SAVE: dsReadEditors(); saveDesignFile(s_ds->section,s_ds->d);
                logSettingsChange(std::wstring(L"تغییر طرح چاپ بخش «")+
                    ((s_ds->section>=0&&s_ds->section<N_PRINT_SECTIONS)?
                        PRINT_SECTIONS[s_ds->section]:L"؟")+L"»");
                MessageBoxW(h,L"طرح این بخش ذخیره شد و در چاپ استفاده می‌شود.",
                    L"ذخیره طرح",MB_OK|MB_ICONINFORMATION); break;
            case PD_BACKUP: dsReadEditors(); doBackup(h); break;
            case PD_RESTORE: s_ds->backups=loadBackupNames(s_ds->section);
                s_ds->showTplList=!s_ds->showTplList; dsLayoutEditors(h);
                InvalidateRect(h,NULL,FALSE); break;
            case PD_TPL_PREV: s_ds->tplIdx=(s_ds->tplIdx+N_TEMPLATES-1)%N_TEMPLATES;
                s_ds->d=templateByIndex(s_ds->section,s_ds->tplIdx); s_ds->sel=-1;
                dsSyncEditors(h); InvalidateRect(h,NULL,FALSE); break;
            case PD_TPL_NEXT: s_ds->tplIdx=(s_ds->tplIdx+1)%N_TEMPLATES;
                s_ds->d=templateByIndex(s_ds->section,s_ds->tplIdx); s_ds->sel=-1;
                dsSyncEditors(h); InvalidateRect(h,NULL,FALSE); break;
            case PD_PAPER: s_ds->d.paper=!s_ds->d.paper; InvalidateRect(h,NULL,FALSE); break;
            case PD_ZOOMIN: s_ds->zoom*=1.15; if(s_ds->zoom>8)s_ds->zoom=8;
                dsLayoutEditors(h); InvalidateRect(h,NULL,FALSE); break;
            case PD_ZOOMOUT:s_ds->zoom/=1.15; if(s_ds->zoom<0.8)s_ds->zoom=0.8;
                dsLayoutEditors(h); InvalidateRect(h,NULL,FALSE); break;
            }
            return 0;
        }
        int r=dsHitRail(h,pt);
        if(r){
            if(r>=PD_TPLITEM_BASE){ int i=r-PD_TPLITEM_BASE;
                if(i<(int)s_ds->backups.size()) doRestore(h,s_ds->backups[i]); return 0; }
            if(r>=PD_FIELD_BASE && r<PD_FIELD_BASE+N_FIELDS){
                if(s_ds->sel>=0 && s_ds->d.items[s_ds->sel].kind==IT_LABEL){
                    s_ds->d.items[s_ds->sel].field=FIELDS[r-PD_FIELD_BASE].token;
                    InvalidateRect(h,NULL,FALSE);
                }
                return 0;
            }
            switch(r){
            case PD_ADD_LABEL: dsAdd(h,IT_LABEL,true); break;
            case PD_ADD_TEXT:  dsAdd(h,IT_LABEL,false); break;
            case PD_ADD_LINEH: dsAdd(h,IT_LINE_H,false); break;
            case PD_ADD_LINEV: dsAdd(h,IT_LINE_V,false); break;
            case PD_ADD_BORDER:dsAdd(h,IT_BORDER,false); break;
            case PD_ADD_LOGO:  dsAdd(h,IT_LOGO,false); break;
            case PD_DEL_ITEM:
                if(s_ds->sel>=0){ s_ds->d.items.erase(s_ds->d.items.begin()+s_ds->sel);
                    s_ds->sel=-1; dsSyncEditors(h); InvalidateRect(h,NULL,FALSE);} break;
            case PD_P_BOLD: if(s_ds->sel>=0){ DItem&it=s_ds->d.items[s_ds->sel]; it.bold=!it.bold; InvalidateRect(h,NULL,FALSE);} break;
            case PD_P_ITALIC: if(s_ds->sel>=0){ DItem&it=s_ds->d.items[s_ds->sel]; it.italic=!it.italic; InvalidateRect(h,NULL,FALSE);} break;
            case PD_P_UNDER: if(s_ds->sel>=0){ DItem&it=s_ds->d.items[s_ds->sel]; it.underline=!it.underline; InvalidateRect(h,NULL,FALSE);} break;
            case PD_P_STRIKE: if(s_ds->sel>=0){ DItem&it=s_ds->d.items[s_ds->sel]; it.strike=!it.strike; InvalidateRect(h,NULL,FALSE);} break;
            case PD_P_FSMINUS: if(s_ds->sel>=0){ DItem&it=s_ds->d.items[s_ds->sel];
                if(it.kind==IT_LABEL){ if(it.fontSize>5) it.fontSize--; }
                else { it.lineW-=0.1; if(it.lineW<0.1)it.lineW=0.1; }
                InvalidateRect(h,NULL,FALSE);} break;
            case PD_P_FSPLUS: if(s_ds->sel>=0){ DItem&it=s_ds->d.items[s_ds->sel];
                if(it.kind==IT_LABEL){ if(it.fontSize<72) it.fontSize++; }
                else { it.lineW+=0.1; if(it.lineW>5)it.lineW=5; }
                InvalidateRect(h,NULL,FALSE);} break;
            case PD_P_COLOR: if(s_ds->sel>=0){ DItem&it=s_ds->d.items[s_ds->sel];
                COLORREF c=it.color; if(dsPickColor(h,c)){ it.color=c; InvalidateRect(h,NULL,FALSE);} } break;
            case PD_P_BGCOLOR: if(s_ds->sel>=0){ DItem&it=s_ds->d.items[s_ds->sel];
                COLORREF c=(it.bgColor==CLR_INVALID)?RGB(255,255,255):it.bgColor;
                if(dsPickColor(h,c)){ it.bgColor=c; InvalidateRect(h,NULL,FALSE);} } break;
            case PD_P_BGCLEAR: if(s_ds->sel>=0){ s_ds->d.items[s_ds->sel].bgColor=CLR_INVALID;
                InvalidateRect(h,NULL,FALSE);} break;
            case PD_P_ALIGN: if(s_ds->sel>=0){ DItem&it=s_ds->d.items[s_ds->sel];
                it.align=(it.align+1)%3; InvalidateRect(h,NULL,FALSE);} break;
            case PD_RESETVIEW: s_ds->panX=0; s_ds->panY=0;
                s_ds->zoom=s_ds->d.paper?2.0:1.4; dsLayoutEditors(h);
                InvalidateRect(h,NULL,FALSE); break;
            case PD_P_LINECOLOR: if(s_ds->sel>=0){ DItem&it=s_ds->d.items[s_ds->sel];
                COLORREF c=(it.kind==IT_BORDER)?it.borderColor:it.color;
                if(dsPickColor(h,c)){ if(it.kind==IT_BORDER) it.borderColor=c; else it.color=c;
                    InvalidateRect(h,NULL,FALSE);} } break;
            case PD_P_LSTYLE: if(s_ds->sel>=0){ DItem&it=s_ds->d.items[s_ds->sel];
                it.lineStyle=(it.lineStyle+1)%3; InvalidateRect(h,NULL,FALSE);} break;
            case PD_P_LOGOPICK: if(s_ds->sel>=0){ DItem&it=s_ds->d.items[s_ds->sel];
                wchar_t file[MAX_PATH]={0}; OPENFILENAMEW ofn={0};
                ofn.lStructSize=sizeof(ofn); ofn.hwndOwner=h;
                ofn.lpstrFilter=L"تصاویر PNG\0*.png\0همه فایل‌ها\0*.*\0";
                ofn.lpstrFile=file; ofn.nMaxFile=MAX_PATH;
                ofn.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST;
                if(GetOpenFileNameW(&ofn)){ it.logoPath=file; InvalidateRect(h,NULL,FALSE);} } break;
            }
            return 0;
        }
        // canvas: select / begin drag
        int hit=dsHitCanvas(h,pt);
        if(hit==-2) return 0;
        if(hit>=0){
            s_ds->sel=hit; dsSyncEditors(h);
            RECT page; double zoom; dsPageMetrics(h,page,zoom);
            DItem& it=s_ds->d.items[hit];
            s_ds->dragDX=dsMMx(page,zoom,pt.x)-it.x;
            s_ds->dragDY=dsMMy(page,zoom,pt.y)-it.y;
            s_ds->dragging=true; SetCapture(h);
        } else {
            // empty canvas → start panning (grab-drag the page)
            s_ds->sel=-1; dsSyncEditors(h);
            RECT cv=dsCanvasRect(h);
            if(PtInRect(&cv,pt)){
                s_ds->panning=true; SetCapture(h);
                s_ds->panStartX=pt.x; s_ds->panStartY=pt.y;
                s_ds->panStartPX=s_ds->panX; s_ds->panStartPY=s_ds->panY;
                SetCursor(LoadCursor(NULL,IDC_SIZEALL));
            }
        }
        InvalidateRect(h,NULL,FALSE);
        return 0; }
    case WM_MBUTTONDOWN:{
        POINT pt={GET_X_LPARAM(l),GET_Y_LPARAM(l)};
        RECT cv=dsCanvasRect(h);
        if(PtInRect(&cv,pt)){
            s_ds->panning=true; SetCapture(h);
            s_ds->panStartX=pt.x; s_ds->panStartY=pt.y;
            s_ds->panStartPX=s_ds->panX; s_ds->panStartPY=s_ds->panY;
        }
        return 0; }
    case WM_MBUTTONUP:
        if(s_ds->panning){ s_ds->panning=false; ReleaseCapture(); }
        return 0;
    case WM_MOUSEWHEEL:{
        // zoom-to-cursor: keep the mm point under the cursor fixed while zooming
        POINT pt={GET_X_LPARAM(l),GET_Y_LPARAM(l)};
        ScreenToClient(h,&pt);
        RECT cv=dsCanvasRect(h);
        if(!PtInRect(&cv,pt)) return 0;
        RECT page; double zoom; dsPageMetrics(h,page,zoom);
        double mmx=dsMMx(page,zoom,pt.x), mmy=dsMMy(page,zoom,pt.y);
        int delta=GET_WHEEL_DELTA_WPARAM(w);
        double factor = (delta>0)? 1.12 : (1.0/1.12);
        double nz=s_ds->zoom*factor;
        if(nz<0.5) nz=0.5; if(nz>10) nz=10;
        s_ds->zoom=nz;
        // recompute page origin (without pan) then adjust pan so (mmx,mmy)
        // stays under the cursor.
        double pw,ph; paperMM(s_ds->d.paper,pw,ph);
        int Wpx=(int)(pw*nz), Hpx=(int)(ph*nz);
        int baseX=cv.left+((cv.right-cv.left)-Wpx)/2;
        int baseY=cv.top +((cv.bottom-cv.top)-Hpx)/2;
        // want: baseX + panX + mmx*nz == pt.x  → panX = pt.x - baseX - mmx*nz
        s_ds->panX = pt.x - baseX - mmx*nz;
        s_ds->panY = pt.y - baseY - mmy*nz;
        dsLayoutEditors(h); InvalidateRect(h,NULL,FALSE);
        return 0; }
    case WM_LBUTTONUP:
        if(s_ds->panning){ s_ds->panning=false; ReleaseCapture(); }
        if(s_ds->dragging){ s_ds->dragging=false; ReleaseCapture(); dsSyncEditors(h); }
        return 0;
    case WM_COMMAND:
        if(HIWORD(w)==EN_KILLFOCUS){ dsReadEditors(); InvalidateRect(h,NULL,FALSE); }
        return 0;
    case WM_CTLCOLOREDIT:{ HDC dc=(HDC)w; SetTextColor(dc,g_theme.inputText);
        SetBkColor(dc,g_theme.inputBg); return (LRESULT)g_brInput; }
    case WM_KEYDOWN:
        if(w==VK_ESCAPE){ dsClose(); return 0; }
        if(w==VK_DELETE && s_ds->sel>=0){
            s_ds->d.items.erase(s_ds->d.items.begin()+s_ds->sel);
            s_ds->sel=-1; dsSyncEditors(h); InvalidateRect(h,NULL,FALSE); return 0; }
        break;
    case WM_DESTROY:
        if(s_ds){ delete s_ds; s_ds=NULL; } s_des=NULL;
        if(g_hFrame) InvalidateRect(g_hFrame,NULL,TRUE);
        return 0;
    }
    return DefWindowProcW(h,m,w,l);
}

void openPrintDesigner(HWND owner, int sectionIdx){
    if(s_des && IsWindow(s_des)){ dsClose(); }
    static bool reg=false;
    if(!reg){ WNDCLASSW wc={0}; wc.lpfnWndProc=desProc; wc.hInstance=g_hInst;
        wc.hCursor=LoadCursor(NULL,IDC_ARROW); wc.lpszClassName=PD_CLASS;
        RegisterClassW(&wc); reg=true; }
    RECT rc; GetClientRect(owner,&rc);
    POINT org={0,0}; ClientToScreen(owner,&org);
    s_ds=new DesignerState();
    s_ds->owner=owner; s_ds->section=sectionIdx; s_ds->sel=-1;
    s_ds->tplIdx=0; s_ds->hot=0; s_ds->dragging=false; s_ds->showTplList=false;
    s_ds->panX=0; s_ds->panY=0; s_ds->panning=false;
    s_ds->zoom = (sectionIdx>=0)? (1.4) : 1.4;
    if(!loadDesignFile(sectionIdx,s_ds->d)) s_ds->d=defaultDesign(sectionIdx);
    s_ds->zoom = s_ds->d.paper? 2.0 : 1.4;   // fit A4/A5 to screen-ish
    s_ds->backups=loadBackupNames(sectionIdx);

    s_des=CreateWindowExW(WS_EX_TOPMOST,PD_CLASS,L"",
        WS_POPUP|WS_VISIBLE|WS_CLIPCHILDREN,
        org.x,org.y,rc.right,rc.bottom,owner,NULL,g_hInst,NULL);

    // property editors (overlay the rail)
    DWORD es=WS_CHILD|ES_AUTOHSCROLL;
    s_ds->eX=CreateWindowExW(0,L"EDIT",L"",es,0,0,10,10,s_des,(HMENU)1001,g_hInst,NULL);
    s_ds->eY=CreateWindowExW(0,L"EDIT",L"",es,0,0,10,10,s_des,(HMENU)1002,g_hInst,NULL);
    s_ds->eW=CreateWindowExW(0,L"EDIT",L"",es,0,0,10,10,s_des,(HMENU)1003,g_hInst,NULL);
    s_ds->eH=CreateWindowExW(0,L"EDIT",L"",es,0,0,10,10,s_des,(HMENU)1004,g_hInst,NULL);
    s_ds->eText=CreateWindowExW(0,L"EDIT",L"",es,0,0,10,10,s_des,(HMENU)1005,g_hInst,NULL);
    s_ds->eFont=CreateWindowExW(0,L"EDIT",L"",es,0,0,10,10,s_des,(HMENU)1006,g_hInst,NULL);
    s_ds->eName=CreateWindowExW(0,L"EDIT",L"",es,0,0,10,10,s_des,(HMENU)1007,g_hInst,NULL);
    HWND eds[]={s_ds->eX,s_ds->eY,s_ds->eW,s_ds->eH,s_ds->eText,s_ds->eFont,s_ds->eName};
    for(HWND e:eds) SendMessageW(e,WM_SETFONT,(WPARAM)g_fSmall,TRUE);
    dsLayoutEditors(s_des);

    BringWindowToTop(s_des); SetFocus(s_des);
}
