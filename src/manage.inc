// ============================================================================
//  manage.inc — management panel (v1.8.0 dashboard redesign)
//  #included into admin.cpp.  Owner-drawn, theme-aware, RTL.
//
//  The panel is now a structured DASHBOARD with a left navigation rail and
//  separate content pages instead of one flat, crowded screen:
//
//    PG_DASH   داشبورد           — summary cards (sections / employees / online
//                                  / pending requests) + quick actions
//    PG_SECTS  بخش‌ها             — existing sections grid + «ایجاد بخش» page
//    PG_EMPS   کارمندان           — employee list with combinable filters +
//                                  «افزودن کارمند» (full new-employee form)
//    PG_MSG    پیام به کارمندان   — compose with live search, dept filters,
//                                  message routing and media attachment
//    PG_REQS   درخواست‌ها         — settings + profile change requests
//                                  (distinct violet "info" category, never red)
//    PG_SAVED  پیام‌های ذخیره‌شده  — archived messages (when enabled in settings)
//
//  All heavy lifting (employee details / print, request approval modals) is
//  preserved.  Every rounded surface uses the v1.8.0 *Bg corner-patch helpers
//  so corners always match the theme background.
// ============================================================================
#define MG_CLASS L"AzManage"

// navigation pages
// v1.3.0: the «بیماران» (patients) page now lives in the clinic-management
// panel — index 1, right after the dashboard (RTL: visually first after it).
// It was previously only reachable from the hidden-admin panel, which was the
// wrong surface. The hidden-admin patients tab is removed (see admin.cpp).
enum { PG_DASH=0, PG_PATIENTS=1, PG_SECTS=2, PG_EMPS=3, PG_MSG=4, PG_REQS=5,
       PG_SAVED=6, PG_BACKUP=7, PG_COUNT=8 };

// control ids
enum {
    MG_ADD_CAT=2001, MG_E_NAME, MG_E_MGR, MG_E_ID, MG_CB_AUTOID,
    MG_FILTER_ABC, MG_FILTER_NEW, MG_FILTER_CREATED, MG_BACK,
    MG_ADD_EMP, MG_DESIGN,
    MG_MSG, MG_REQS, MG_PROFREQS, MG_BACKUP,
    MG_NOTE_ADD, MG_E_NOTE, MG_NOTE_ATTACH, MG_NOTE_SEND,   // saved messages
    MG_CB_DEPT,            // department filter combo (employees page)
    MG_DESIGN_SEC,         // print-design for the selected section
    MG_PT_SEARCH=2050,     // patients page: search box
    MG_PT_LIST,            // patients page: virtualized ListView
    MG_NAV_BASE=2100,      // sidebar nav buttons (MG_NAV_BASE+page)
    MG_CARD_BASE=2200,     // clicking a section card
    MG_DETAIL_BASE=2400,   // clicking an employee row
    MG_QA_BASE=2600,       // dashboard quick-action tiles
};

struct EmpRow {
    User user;
    EmpProfile prof;
    bool online;
    std::wstring deptName;
};

struct ManageState {
    int page;                   // PG_*
    int hot;                    // hot-tracked element id
    int navHot;                 // hot nav item (-1 none)
    // sections
    std::vector<DeptCat> cats;
    bool secAddMode;            // showing the «ایجاد بخش» add-form?
    HWND eName, eMgr, eId, bAdd, bAutoId, bSecNew, bSecBack;
    bool autoId;
    std::wstring autoIdPreview;
    // employees
    std::vector<EmpRow> emps;
    int  empSort;               // 0=abc, 1=newest, 2=created(oldest-first)
    std::wstring empDeptFilter; // dept id filter ("" = all)
    HWND bAbc, bNew, bCreated, bAddEmp, cDept;
    // messages / requests buttons (page bodies use owner-draw)
    HWND bMsg, bReqs, bProfReqs;
    // saved messages (local notes) page
    HWND eNote, bNoteAdd, bNoteAttach, bNoteSend;
    std::wstring noteAttach;    // pending attachment path for the next note
    int  noteHot;               // hot-tracked local-note row (-1 none)
    std::wstring curCatName;    // for personnel card "section" line
    std::wstring msg; COLORREF msgCol;
    // ---- patients page (v1.3.0): moved here from the hidden-admin panel ----
    HWND ptSearch=nullptr;      // debounced search box
    HWND ptList=nullptr;        // LVS_OWNERDATA virtualized list
    std::vector<PatientRow>* ptAll=nullptr;   // owned full set (heap)
    std::vector<int> ptView;    // indices into *ptAll after filtering
    std::wstring ptQuery;       // last applied search text
    bool ptLoading=false, ptLoaded=false;
};
// background patient loader → UI thread marshal (no HWND access off-thread)
#define WM_MG_PATIENTS_READY (WM_APP+74)
static ManageState* s_mg=NULL;
static HWND g_hManage=NULL;
static void mgRefreshReqBadge();
static void openProfReqs(HWND owner);
static void mgGoPage(HWND h, int page);

// ====================================================== PATIENTS PAGE =======
//  v1.3.0 — a virtualized (LVS_OWNERDATA) patient list in the clinic-management
//  panel. Reuses the SAME data layer as the (now-removed) hidden-admin tab:
//  loadAllPatients() + uikit::NormalizeFa search. Loaded ONCE on a worker
//  thread so the UI never blocks; only visible rows are materialized so it
//  stays smooth with very large datasets.
static const wchar_t* mgGenderText(const std::wstring& g){
    if(g==L"1"||g==L"مرد"||g==L"M"||g==L"m") return L"مرد";
    if(g==L"0"||g==L"زن"||g==L"F"||g==L"f")  return L"زن";
    return L"—";
}
static void mgPatientsFilter(){
    if(!s_mg) return;
    s_mg->ptView.clear();
    if(!s_mg->ptAll) return;
    std::wstring q=uikit::NormalizeFa(s_mg->ptQuery);
    const auto& all=*s_mg->ptAll;
    s_mg->ptView.reserve(all.size());
    if(q.empty()){
        for(int i=0;i<(int)all.size();++i) s_mg->ptView.push_back(i);
    } else {
        for(int i=0;i<(int)all.size();++i){
            const PatientRow& p=all[i];
            std::wstring name=uikit::NormalizeFa(p.first+L" "+p.last);
            std::wstring nid =uikit::NormalizeFa(p.nid);
            std::wstring mob =uikit::NormalizeFa(p.mobile);
            if(name.find(q)!=std::wstring::npos ||
               nid.find(q)!=std::wstring::npos  ||
               mob.find(q)!=std::wstring::npos)
                s_mg->ptView.push_back(i);
        }
    }
    if(s_mg->ptList){
        ListView_SetItemCountEx(s_mg->ptList,(int)s_mg->ptView.size(),
            LVSICF_NOINVALIDATEALL|LVSICF_NOSCROLL);
        InvalidateRect(s_mg->ptList,NULL,FALSE);
    }
}
static void mgPatientsLoadAsync(HWND h){
    if(!s_mg || s_mg->ptLoaded || s_mg->ptLoading) return;
    s_mg->ptLoading=true;
    HWND target=h;
    std::thread([target](){
        auto* vec=new std::vector<PatientRow>(loadAllPatients());
        if(IsWindow(target)) PostMessageW(target,WM_MG_PATIENTS_READY,0,(LPARAM)vec);
        else delete vec;
    }).detach();
}

// ---- data refresh ----------------------------------------------------------
static std::wstring deptNameById(const std::wstring& id){
    if(!s_mg) return L"";
    for(auto& c:s_mg->cats) if(c.id==id) return c.name;
    return L"";
}
static void mgLoadCats(){
    if(!s_mg) return;
    s_mg->cats=loadDepts();
}
static void mgLoadEmps(){
    if(!s_mg) return;
    s_mg->emps.clear();
    auto users=loadUsers();
    int order=0;
    for(auto& u:users){
        EmpProfile p=loadEmpProfile(u.username);
        // department filter (combines with the sort below)
        if(!s_mg->empDeptFilter.empty()){
            std::wstring did = p.deptId.empty()? L"" : p.deptId;
            // match by deptId OR by legacy dept-name fallback
            bool match = (did==s_mg->empDeptFilter);
            if(!match && did.empty() && u.dept==deptNameById(s_mg->empDeptFilter)) match=true;
            if(!match) continue;
        }
        EmpRow r; r.user=u; r.prof=p; r.online=isUserOnline(u.username);
        r.deptName = p.deptId.empty()? u.dept : deptNameById(p.deptId);
        if(r.deptName.empty()) r.deptName=u.dept;
        s_mg->emps.push_back(r);
        (void)order;
    }
    // sort (combines with the dept filter above)
    if(s_mg->empSort==0){            // alphabetical (by full name)
        std::sort(s_mg->emps.begin(),s_mg->emps.end(),
            [](const EmpRow&a,const EmpRow&b){ return a.user.fullname<b.user.fullname; });
    } else if(s_mg->empSort==1){     // newest first (file order is oldest→newest)
        std::reverse(s_mg->emps.begin(),s_mg->emps.end());
    }
    // empSort==2 → created order (oldest first) = natural load order, no change
}
// ====================== employee details modal ==============================
#define MGD_CLASS L"AzEmpDetail"
struct DetailState { EmpRow row; int hot; int paper; };
static HWND s_mgd=NULL; static DetailState* s_dt=NULL;

static int mgdCardW(){ return S(740); }
static int mgdCardH(){ return S(660); }
static RECT mgdCard(HWND h){
    RECT rc; GetClientRect(h,&rc);
    int w=mgdCardW(),hh=mgdCardH();
    RECT c={(rc.right-w)/2,(rc.bottom-hh)/2,(rc.right+w)/2,(rc.bottom+hh)/2};
    return c;
}

// render the personnel card onto a DC (preview AND printer share this)
static void renderPersonnelCard(HDC dc, RECT area, const EmpRow& r, bool forPrint){
    SetBkMode(dc,TRANSPARENT);
    COLORREF ink = forPrint?RGB(0,0,0):g_theme.text;
    COLORREF dim = forPrint?RGB(90,90,90):g_theme.textDim;
    COLORREF acc = forPrint?RGB(20,50,100):g_theme.accent;
    // border
    { HPEN p=CreatePen(PS_SOLID,forPrint?2:1,acc); HGDIOBJ o=SelectObject(dc,p);
      HGDIOBJ ob=SelectObject(dc,GetStockObject(NULL_BRUSH));
      Rectangle(dc,area.left,area.top,area.right,area.bottom);
      SelectObject(dc,ob); SelectObject(dc,o); DeleteObject(p); }
    int pad=S(20);
    // title
    SelectObject(dc,g_fTitle); SetTextColor(dc,acc);
    RECT tr={area.left+pad,area.top+pad,area.right-pad,area.top+pad+S(40)};
    DrawTextW(dc,(std::wstring(L"شناسنامهٔ پرسنلی — درمانگاه ")+APP_NAME_W).c_str(),-1,&tr,
        DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
    // photo box top-left
    RECT ph={area.left+pad, area.top+pad+S(50), area.left+pad+S(120), area.top+pad+S(200)};
    { HPEN p=CreatePen(PS_DOT,1,dim); HGDIOBJ o=SelectObject(dc,p);
      HGDIOBJ ob=SelectObject(dc,GetStockObject(NULL_BRUSH));
      Rectangle(dc,ph.left,ph.top,ph.right,ph.bottom);
      SelectObject(dc,ob); SelectObject(dc,o); DeleteObject(p); }
    SelectObject(dc,g_fSmall); SetTextColor(dc,dim);
    DrawTextW(dc,L"عکس پرسنلی",-1,&ph,DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
    // info lines (right side)
    int rx=area.right-pad, infoTop=area.top+pad+S(56);
    int lh=S(30); int y=infoTop;
    auto line=[&](const wchar_t* lbl, const std::wstring& val, bool bold){
        SelectObject(dc, bold?g_fUIB:g_fUI); SetTextColor(dc,ink);
        std::wstring s=std::wstring(lbl)+L"  "+ (val.empty()?L"—":val);
        RECT lr={area.left+S(150),y,rx,y+lh};
        DrawTextW(dc,s.c_str(),-1,&lr,DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
        y+=lh;
    };
    line(L"نام و نام خانوادگی:", r.user.fullname, true);
    line(L"نام کاربری:",        r.user.username, false);
    line(L"کد پرسنلی:",         (r.prof.empId.empty()?L"—":toFaDigits(r.prof.empId)), false);
    line(L"شناسه یکتا:",        (r.prof.uniqueId.empty()?L"—":toFaDigits(r.prof.uniqueId)), false);
    line(L"کد ملی:",            toFaDigits(r.prof.nationalId), false);
    line(L"نام پدر:",           r.prof.fatherName, false);
    line(L"سمت:",               (r.prof.position.empty()?L"—":r.prof.position), false);
    line(L"موبایل:",            toFaDigits(r.prof.mobile), false);
    line(L"تلفن ثابت:",         toFaDigits(r.prof.landline), false);
    line(L"بخش:",               (r.deptName.empty()?r.user.dept:r.deptName), false);
    line(L"شیفت کاری:",         (r.prof.shiftFrom.empty()?L"—":(r.prof.shiftFrom+L" تا "+r.prof.shiftTo)), false);
    line(L"ساعات کاری هفتگی:",  (r.prof.workHours.empty()?L"—":toFaDigits(r.prof.workHours)), false);
    line(L"تاریخ استخدام:",     (r.prof.hireDate.empty()?L"—":toFaDigits(r.prof.hireDate)), false);
    line(L"نوع دسترسی:",        (r.user.role==0?L"پذیرش":r.user.role==1?L"مدیریت":L"مدیر سامانه"), false);
    line(L"وضعیت:",             (r.online?L"برخط (آنلاین)":L"غیرفعال"), false);
    // address (wrapped)
    SelectObject(dc,g_fUI); SetTextColor(dc,ink);
    RECT ar={area.left+pad,y+S(6),rx,y+S(70)};
    DrawTextW(dc,(L"آدرس:  "+(r.prof.address.empty()?std::wstring(L"—"):r.prof.address)).c_str(),
        -1,&ar,DT_RIGHT|DT_WORDBREAK|DT_RTLREADING|DT_NOPREFIX);
    // national-card photo box bottom
    RECT nc={area.left+pad, area.bottom-pad-S(110), area.left+pad+S(220), area.bottom-pad};
    { HPEN p=CreatePen(PS_DOT,1,dim); HGDIOBJ o=SelectObject(dc,p);
      HGDIOBJ ob=SelectObject(dc,GetStockObject(NULL_BRUSH));
      Rectangle(dc,nc.left,nc.top,nc.right,nc.bottom);
      SelectObject(dc,ob); SelectObject(dc,o); DeleteObject(p); }
    SelectObject(dc,g_fSmall); SetTextColor(dc,dim);
    DrawTextW(dc,L"تصویر کارت ملی",-1,&nc,DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
}

static bool printPersonnelCard(HWND owner, const EmpRow& r, int paper){
    std::wstring prn=getSetting(L"printer_name",L"");
    if(prn.empty()){ wchar_t def[256]={0}; DWORD sz=256;
        if(GetDefaultPrinterW(def,&sz)) prn=def; }
    HDC dc = prn.empty()?NULL:CreateDCW(L"WINSPOOL",prn.c_str(),NULL,NULL);
    if(!dc){
        PRINTDLGW pd={0}; pd.lStructSize=sizeof(pd); pd.hwndOwner=owner;
        pd.Flags=PD_RETURNDC|PD_NOPAGENUMS|PD_NOSELECTION;
        if(!PrintDlgW(&pd)) return false; dc=pd.hDC;
    }
    if(!dc) return false;
    DOCINFOW di={sizeof(di)}; di.lpszDocName=L"آزادی طب — شناسنامهٔ پرسنلی";
    if(StartDocW(dc,&di)<=0){ DeleteDC(dc); return false; }
    StartPage(dc);
    int horz=GetDeviceCaps(dc,HORZRES), vert=GetDeviceCaps(dc,VERTRES);
    int dpiY=GetDeviceCaps(dc,LOGPIXELSY);
    int marg=dpiY/2;
    RECT area={marg,marg,horz-marg, (paper==1)? (vert/2) : (vert-marg)};
    renderPersonnelCard(dc,area,r,true);
    EndPage(dc); EndDoc(dc); DeleteDC(dc);
    return true;
}

static void mgdClose(){ if(s_mgd&&IsWindow(s_mgd)){ HWND v=s_mgd; s_mgd=NULL; DestroyWindow(v);} }
static LRESULT CALLBACK mgdProc(HWND h, UINT m, WPARAM w, LPARAM l){
    switch(m){
    case WM_ERASEBKGND: return 1;
    case WM_PAINT:{ PAINTSTRUCT ps; HDC dc0=BeginPaint(h,&ps);
        RECT rc; GetClientRect(h,&rc);
        HDC dc=CreateCompatibleDC(dc0);
        HBITMAP bmp=CreateCompatibleBitmap(dc0,rc.right,rc.bottom);
        HGDIOBJ obm=SelectObject(dc,bmp);
        { HBRUSH sb=CreateSolidBrush(g_dark?RGB(6,9,14):RGB(28,36,48));
          FillRect(dc,&rc,sb); DeleteObject(sb); }
        gpFillAlpha(dc,rc,0,g_dark?RGB(0,0,0):RGB(20,28,40),120);
        RECT c=mgdCard(h);
        gpShadow(dc,c,S(20),S(22),80);
        { HBRUSH pb=CreateSolidBrush(g_theme.surface); FillRect(dc,&c,pb); DeleteObject(pb); }
        gpGradRoundRect(dc,c,S(18),g_theme.surfaceTop,g_theme.surface,g_theme.border);
        SetBkMode(dc,TRANSPARENT);
        // close
        { RECT cb={c.left+S(14),c.top+S(14),c.left+S(40),c.top+S(40)};
          if(s_dt&&s_dt->hot==1) gpRoundRect(dc,cb,S(8),g_theme.hover,CLR_INVALID,255);
          RECT ci={cb.left+S(5),cb.top+S(5),cb.right-S(5),cb.bottom-S(5)};
          drawIcon(dc,ICO_X,ci,g_theme.text,S(2)); }
        // preview area
        RECT pv={c.left+S(24),c.top+S(54),c.right-S(24),c.bottom-S(70)};
        { HBRUSH wb=CreateSolidBrush(RGB(255,255,255)); FillRect(dc,&pv,wb); DeleteObject(wb); }
        if(s_dt) renderPersonnelCard(dc,pv,s_dt->row,false);
        // buttons
        auto btn=[&](int id,const wchar_t* t,RECT r,bool primary){
            bool hov=(s_dt&&s_dt->hot==id);
            COLORREF bg=primary?(hov?g_theme.accentHover:g_theme.accent):(hov?g_theme.hover:g_theme.surface2);
            gpRoundRect(dc,r,S(9),bg,primary?bg:g_theme.border,255);
            SetTextColor(dc,primary?g_theme.accentText:g_theme.text); SelectObject(dc,g_fUIB);
            DrawTextW(dc,t,-1,&r,DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
        };
        int by=c.bottom-S(58), bw=S(150);
        RECT r4={c.right-S(24)-bw,by,c.right-S(24),by+S(40)};       btn(2,L"چاپ A4",r4,true);
        RECT r5={r4.left-S(8)-bw,by,r4.left-S(8),by+S(40)};         btn(3,L"چاپ A5",r5,false);
        BitBlt(dc0,0,0,rc.right,rc.bottom,dc,0,0,SRCCOPY);
        SelectObject(dc,obm); DeleteObject(bmp); DeleteDC(dc);
        EndPaint(h,&ps); return 0; }
    case WM_MOUSEMOVE:{ POINT pt={GET_X_LPARAM(l),GET_Y_LPARAM(l)};
        RECT c=mgdCard(h); int hr=0;
        RECT cb={c.left+S(14),c.top+S(14),c.left+S(40),c.top+S(40)};
        int by=c.bottom-S(58), bw=S(150);
        RECT r4={c.right-S(24)-bw,by,c.right-S(24),by+S(40)};
        RECT r5={r4.left-S(8)-bw,by,r4.left-S(8),by+S(40)};
        if(PtInRect(&cb,pt))hr=1; else if(PtInRect(&r4,pt))hr=2; else if(PtInRect(&r5,pt))hr=3;
        if(s_dt&&s_dt->hot!=hr){ s_dt->hot=hr; InvalidateRect(h,NULL,FALSE); } return 0; }
    case WM_LBUTTONDOWN:{ POINT pt={GET_X_LPARAM(l),GET_Y_LPARAM(l)};
        RECT c=mgdCard(h);
        if(!PtInRect(&c,pt)){ mgdClose(); return 0; }
        RECT cb={c.left+S(14),c.top+S(14),c.left+S(40),c.top+S(40)};
        if(PtInRect(&cb,pt)){ mgdClose(); return 0; }
        int by=c.bottom-S(58), bw=S(150);
        RECT r4={c.right-S(24)-bw,by,c.right-S(24),by+S(40)};
        RECT r5={r4.left-S(8)-bw,by,r4.left-S(8),by+S(40)};
        if(PtInRect(&r4,pt)){ printPersonnelCard(h,s_dt->row,0); }
        else if(PtInRect(&r5,pt)){ printPersonnelCard(h,s_dt->row,1); }
        return 0; }
    case WM_KEYDOWN: if(w==VK_ESCAPE){ mgdClose(); return 0; } break;
    case WM_DESTROY: if(s_dt){ delete s_dt; s_dt=NULL; } s_mgd=NULL;
        if(g_hFrame) InvalidateRect(g_hFrame,NULL,TRUE); return 0;
    }
    return DefWindowProcW(h,m,w,l);
}
static void openEmpDetails(HWND owner, const EmpRow& r){
    if(s_mgd&&IsWindow(s_mgd)) mgdClose();
    static bool reg=false;
    if(!reg){ WNDCLASSW wc={0}; wc.lpfnWndProc=mgdProc; wc.hInstance=g_hInst;
        wc.hCursor=LoadCursor(NULL,IDC_ARROW); wc.lpszClassName=MGD_CLASS;
        RegisterClassW(&wc); reg=true; }
    RECT rc; GetClientRect(g_hFrame,&rc);
    POINT org={0,0}; ClientToScreen(g_hFrame,&org);
    s_dt=new DetailState(); s_dt->row=r; s_dt->hot=0; s_dt->paper=0;
    s_mgd=CreateWindowExW(WS_EX_TOPMOST,MGD_CLASS,L"",
        WS_POPUP|WS_VISIBLE,org.x,org.y,rc.right,rc.bottom,g_hFrame,NULL,g_hInst,NULL);
    BringWindowToTop(s_mgd); SetFocus(s_mgd);
}
// ============== «درخواست‌های تغییر تنظیمات» (settings-change requests) =======
//  v1.9.0 redesign — a clean, interactive request centre:
//    • each card shows title, detailed action, employee, network/system source,
//      time, and (if present) a preview string.
//    • clicking a card SELECTS it → a VERY THIN GREEN border (the whole card,
//      including its «جزئیات» detail area, shares this selected state).
//    • a selected request is automatically marked READ → it turns GRAY.
//    • per-card actions: تأیید (approve+apply) / رد (reject) / حذف (delete with
//      confirmation).
//    • «علامت‌گذاری همه به‌عنوان خوانده‌شده» works; the unread counter and the
//      nav badge update live and immediately.
#define MGREQ_CLASS L"AzMgReq"
//  v1.9.0: a thin green selection border colour.
static COLORREF mrGreen(){ return RGB(0x2E,0xB8,0x6B); }
struct ReqState { std::vector<SetReq> reqs; int hot; int sel; int scroll; };
static HWND s_mr=NULL; static ReqState* s_rs=NULL;
static void mrClose(){ if(s_mr&&IsWindow(s_mr)){ HWND v=s_mr; s_mr=NULL; DestroyWindow(v);} }
static RECT mrCard(HWND h){ RECT rc; GetClientRect(h,&rc);
    int w=S(780),hh=S(600);
    RECT c={(rc.right-w)/2,(rc.bottom-hh)/2,(rc.right+w)/2,(rc.bottom+hh)/2}; return c; }
static int  mrRowH(){ return S(104); }
static int  mrListTop(HWND h){ RECT c=mrCard(h); return c.top+S(62); }
static int  mrListBot(HWND h){ RECT c=mrCard(h); return c.bottom-S(60); }
static RECT mrRowRect(HWND h,int i){
    RECT c=mrCard(h); int y=mrListTop(h)+i*(mrRowH()+S(8));
    RECT r={c.left+S(20),y,c.right-S(20),y+mrRowH()}; return r; }
// per-row action buttons (LEFT side)
static RECT mrApprove(RECT row){ RECT b={row.left+S(12),row.bottom-S(40),row.left+S(98),row.bottom-S(10)}; return b; }
static RECT mrReject (RECT row){ RECT b={row.left+S(106),row.bottom-S(40),row.left+S(176),row.bottom-S(10)}; return b; }
static RECT mrDelete (RECT row){ RECT b={row.left+S(184),row.bottom-S(40),row.left+S(254),row.bottom-S(10)}; return b; }
static void mrRefreshLive(HWND h){
    if(s_rs) s_rs->reqs=loadSetReqs();
    mgRefreshReqBadge();
    InvalidateRect(h,NULL,FALSE);
    if(g_hManage) InvalidateRect(g_hManage,NULL,FALSE);
}
static LRESULT CALLBACK mrProc(HWND h, UINT m, WPARAM w, LPARAM l){
    switch(m){
    case WM_CREATE: s_rs=new ReqState(); s_rs->reqs=loadSetReqs();
        s_rs->hot=-1; s_rs->sel=-1; s_rs->scroll=0; return 0;
    case WM_NCDESTROY: if(s_rs){ delete s_rs; s_rs=NULL; } s_mr=NULL;
        if(g_hFrame) InvalidateRect(g_hFrame,NULL,TRUE); return 0;
    case WM_ERASEBKGND: return 1;
    case WM_MOUSEMOVE:{ if(!s_rs) break; POINT pt={GET_X_LPARAM(l),GET_Y_LPARAM(l)};
        int oh=s_rs->hot; s_rs->hot=-1;
        for(int i=0;i<(int)s_rs->reqs.size();i++){ RECT r=mrRowRect(h,i);
            if(r.top>=mrListBot(h)) break;
            if(PtInRect(&r,pt)){ s_rs->hot=i; break; } }
        if(oh!=s_rs->hot) InvalidateRect(h,NULL,FALSE);
        return 0; }
    case WM_LBUTTONDOWN:{ if(!s_rs) break; POINT pt={GET_X_LPARAM(l),GET_Y_LPARAM(l)};
        RECT c=mrCard(h);
        RECT cb={c.left+S(14),c.top+S(14),c.left+S(40),c.top+S(40)};
        if(!PtInRect(&c,pt)){ mrClose(); return 0; }
        if(PtInRect(&cb,pt)){ mrClose(); return 0; }
        // «علامت‌گذاری همه به‌عنوان خوانده‌شده»
        int by=c.bottom-S(50), bw=S(250);
        RECT mb={c.left+S(24),by,c.left+S(24)+bw,by+S(36)};
        if(PtInRect(&mb,pt)){ markSetReqsSeen(); mrRefreshLive(h); return 0; }
        // per-row hit testing
        for(int i=0;i<(int)s_rs->reqs.size();i++){
            RECT r=mrRowRect(h,i); if(r.top>=mrListBot(h)) break;
            if(!PtInRect(&r,pt)) continue;
            SetReq& q=s_rs->reqs[i];
            RECT ap=mrApprove(r), rj=mrReject(r), dl=mrDelete(r);
            if(q.status==0 && PtInRect(&ap,pt)){ setSetReqStatus(i,1,L""); mrRefreshLive(h); return 0; }
            if(q.status==0 && PtInRect(&rj,pt)){ setSetReqStatus(i,2,L""); mrRefreshLive(h); return 0; }
            if(PtInRect(&dl,pt)){
                if(MessageBoxW(h,L"این درخواست حذف شود؟",L"حذف درخواست",
                    MB_ICONQUESTION|MB_YESNO|MB_DEFBUTTON2)==IDYES){
                    deleteSetReq(i); if(s_rs->sel==i) s_rs->sel=-1; mrRefreshLive(h); }
                return 0; }
            // clicking the card body (incl. details area) selects + marks read
            s_rs->sel=i;
            if(!q.seen){ markOneSetReqSeen(i); }
            mrRefreshLive(h);
            return 0;
        }
        return 0; }
    case WM_RBUTTONDOWN:{ if(!s_rs) break; POINT pt={GET_X_LPARAM(l),GET_Y_LPARAM(l)};
        for(int i=0;i<(int)s_rs->reqs.size();i++){
            RECT r=mrRowRect(h,i); if(r.top>=mrListBot(h)) break;
            if(!PtInRect(&r,pt)) continue;
            s_rs->sel=i; InvalidateRect(h,NULL,FALSE);
            HMENU pm=CreatePopupMenu();
            AppendMenuW(pm,MF_STRING,1,L"ارسال به پیام‌های ذخیره‌شده");
            POINT sp=pt; ClientToScreen(h,&sp);
            int cmd=TrackPopupMenu(pm,TPM_RIGHTBUTTON|TPM_RETURNCMD,sp.x,sp.y,0,h,NULL);
            DestroyMenu(pm);
            if(cmd==1){
                SetReq& q=s_rs->reqs[i];
                std::wstring title=q.title.empty()?q.change:q.title;
                std::wstring body=title+L"\r\nاقدام: "+q.change+
                    L"\r\nکارمند: "+(q.user.empty()?std::wstring(L"—"):q.user)+
                    L"   سامانه: "+(q.system.empty()?std::wstring(L"—"):q.system)+
                    L"   زمان: "+q.time;
                if(!q.preview.empty()) body+=L"\r\nپیش‌نمایش: "+q.preview;
                pushLocalNote(g_session.user.username,body,L"");
                MessageBoxW(h,L"درخواست به «پیام‌های ذخیره‌شده» شما اضافه شد.",
                    L"پیام‌های ذخیره‌شده",MB_ICONINFORMATION|MB_OK);
                if(g_hManage) InvalidateRect(g_hManage,NULL,FALSE);
            }
            return 0;
        }
        return 0; }
    case WM_KEYDOWN: if(w==VK_ESCAPE){ mrClose(); return 0; } break;
    case WM_PAINT:{ PAINTSTRUCT ps; HDC dc0=BeginPaint(h,&ps);
        RECT rc; GetClientRect(h,&rc);
        HDC dc=CreateCompatibleDC(dc0);
        HBITMAP bmp=CreateCompatibleBitmap(dc0,rc.right,rc.bottom);
        HGDIOBJ obm=SelectObject(dc,bmp);
        { HBRUSH sb=CreateSolidBrush(g_dark?RGB(6,9,14):RGB(28,36,48));
          FillRect(dc,&rc,sb); DeleteObject(sb); }
        gpFillAlpha(dc,rc,0,g_dark?RGB(0,0,0):RGB(20,28,40),120);
        RECT c=mrCard(h);
        gpShadow(dc,c,S(20),S(22),80);
        { HBRUSH pb=CreateSolidBrush(g_theme.surface); FillRect(dc,&c,pb); DeleteObject(pb); }
        gpGradRoundRect(dc,c,S(18),g_theme.surfaceTop,g_theme.surface,g_theme.border);
        SetBkMode(dc,TRANSPARENT);
        // close
        { RECT cb={c.left+S(14),c.top+S(14),c.left+S(40),c.top+S(40)};
          RECT ci={cb.left+S(5),cb.top+S(5),cb.right-S(5),cb.bottom-S(5)};
          drawIcon(dc,ICO_X,ci,g_theme.text,S(2)); }
        SelectObject(dc,g_fTitle); SetTextColor(dc,g_infoAccent);
        RECT tr={c.left+S(24),c.top+S(14),c.right-S(150),c.top+S(48)};
        DrawTextW(dc,L"درخواست‌های تغییر تنظیمات کارمندان",-1,&tr,
            DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
        // live unread counter chip (top-left)
        { int unread=0; for(auto&q:s_rs->reqs) if(!q.seen) unread++;
          RECT cc={c.left+S(50),c.top+S(18),c.left+S(140),c.top+S(44)};
          gpRoundRect(dc,cc,(cc.bottom-cc.top)/2, unread>0?g_infoAccent:g_theme.surface2,
              unread>0?g_infoAccent:g_theme.border,255);
          SelectObject(dc,g_fSmall); SetTextColor(dc, unread>0?RGB(255,255,255):g_theme.textDim);
          std::wstring ct=L"خوانده‌نشده: "+toFaDigits(std::to_wstring(unread));
          DrawTextW(dc,ct.c_str(),-1,&cc,DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX); }
        // list
        if(!s_rs || s_rs->reqs.empty()){
            SelectObject(dc,g_fUI); SetTextColor(dc,g_theme.textDim);
            RECT er={c.left+S(24),mrListTop(h)+S(20),c.right-S(24),mrListTop(h)+S(60)};
            DrawTextW(dc,L"درخواستی ثبت نشده است.",-1,&er,
                DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
        } else {
            for(int i=0;i<(int)s_rs->reqs.size();i++){
                RECT row=mrRowRect(h,i); if(row.top>=mrListBot(h)) break;
                SetReq& q=s_rs->reqs[i];
                bool seld=(s_rs->sel==i), hot=(s_rs->hot==i);
                // body: GRAY when read; subtle attention tint when unread.
                COLORREF body = q.seen ? blendColor(g_theme.surface,g_theme.textDim,8)
                                       : g_theme.surface2;
                COLORREF bord = seld ? mrGreen()
                              : hot  ? g_infoAccent : g_theme.border;
                gpRoundRectBg(dc,row,S(11),body,bord,g_theme.surface,255);
                // VERY THIN green selection border: draw a single hairline inset
                if(seld){ RECT in={row.left+1,row.top+1,row.right-1,row.bottom-1};
                    gpRoundRect(dc,in,S(10),CLR_INVALID,mrGreen(),255); }
                // unread dot
                if(!q.seen){ RECT dot={row.right-S(26),row.top+S(12),row.right-S(14),row.top+S(24)};
                    gpRoundRect(dc,dot,S(6),g_infoAccent,CLR_INVALID,255); }
                // status badge (approved/rejected) top-left of card
                if(q.status!=0){
                    RECT sb2={row.left+S(12),row.top+S(10),row.left+S(110),row.top+S(34)};
                    bool ok=(q.status==1);
                    gpRoundRect(dc,sb2,(sb2.bottom-sb2.top)/2, ok?mrGreen():g_theme.danger,
                        ok?mrGreen():g_theme.danger,255);
                    SelectObject(dc,g_fSmall); SetTextColor(dc,RGB(255,255,255));
                    DrawTextW(dc, ok?L"✓ تأیید شد":L"✕ رد شد",-1,&sb2,
                        DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
                }
                // title (bold)
                SelectObject(dc,g_fUIB); SetTextColor(dc,g_theme.text);
                RECT l1={row.left+S(14),row.top+S(8),row.right-S(34),row.top+S(30)};
                std::wstring title=q.title.empty()?q.change:q.title;
                DrawTextW(dc,title.c_str(),-1,&l1,
                    DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX|DT_END_ELLIPSIS);
                // detailed action
                SelectObject(dc,g_fUI); SetTextColor(dc,g_theme.textDim);
                RECT l2={row.left+S(14),row.top+S(30),row.right-S(14),row.top+S(50)};
                DrawTextW(dc,(L"اقدام: "+q.change).c_str(),-1,&l2,
                    DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX|DT_END_ELLIPSIS);
                // employee • system • time
                SelectObject(dc,g_fSmall); SetTextColor(dc,g_theme.textDim);
                RECT l3={row.left+S(14),row.top+S(50),row.right-S(14),row.top+S(68)};
                std::wstring meta=L"کارمند: "+(q.user.empty()?std::wstring(L"—"):q.user)+
                    L"   •   سامانه: "+(q.system.empty()?std::wstring(L"—"):q.system)+
                    L"   •   "+q.time;
                DrawTextW(dc,meta.c_str(),-1,&l3,
                    DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX|DT_END_ELLIPSIS);
                // preview (if any) — right side above the action buttons
                if(!q.preview.empty()){
                    SelectObject(dc,g_fSmall); SetTextColor(dc,g_infoAccent2);
                    RECT pv={row.left+S(270),row.bottom-S(38),row.right-S(14),row.bottom-S(12)};
                    DrawTextW(dc,(L"پیش‌نمایش: "+q.preview).c_str(),-1,&pv,
                        DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX|DT_END_ELLIPSIS);
                }
                // action buttons (only when pending)
                if(q.status==0){
                    RECT ap=mrApprove(row), rj=mrReject(row);
                    gpRoundRect(dc,ap,S(8),mrGreen(),mrGreen(),255);
                    SelectObject(dc,g_fSmall); SetTextColor(dc,RGB(255,255,255));
                    DrawTextW(dc,L"تأیید",-1,&ap,DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
                    gpRoundRect(dc,rj,S(8),g_theme.danger,g_theme.danger,255);
                    DrawTextW(dc,L"رد",-1,&rj,DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
                }
                // delete (always)
                RECT dl=mrDelete(row);
                gpRoundRect(dc,dl,S(8),g_theme.surface2,g_theme.border,255);
                SelectObject(dc,g_fSmall); SetTextColor(dc,g_theme.danger);
                DrawTextW(dc,L"حذف",-1,&dl,DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
            }
        }
        // mark-all button
        int by=c.bottom-S(50), bw=S(250);
        RECT mb={c.left+S(24),by,c.left+S(24)+bw,by+S(36)};
        gpRoundRect(dc,mb,S(9),g_theme.accent,g_theme.accent,255);
        SetTextColor(dc,g_theme.accentText); SelectObject(dc,g_fUIB);
        DrawTextW(dc,L"علامت‌گذاری همه به‌عنوان خوانده‌شده",-1,&mb,
            DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
        BitBlt(dc0,0,0,rc.right,rc.bottom,dc,0,0,SRCCOPY);
        SelectObject(dc,obm); DeleteObject(bmp); DeleteDC(dc);
        EndPaint(h,&ps); return 0; }
    }
    return DefWindowProcW(h,m,w,l);
}
static void openSetReqs(HWND owner){
    if(s_mr&&IsWindow(s_mr)) mrClose();
    static bool reg=false;
    if(!reg){ WNDCLASSW wc={0}; wc.lpfnWndProc=mrProc; wc.hInstance=g_hInst;
        wc.hCursor=LoadCursor(NULL,IDC_ARROW); wc.lpszClassName=MGREQ_CLASS;
        RegisterClassW(&wc); reg=true; }
    RECT rc; GetClientRect(g_hFrame,&rc);
    POINT org={0,0}; ClientToScreen(g_hFrame,&org);
    s_mr=CreateWindowExW(WS_EX_TOPMOST,MGREQ_CLASS,L"",WS_POPUP|WS_VISIBLE,
        org.x,org.y,rc.right,rc.bottom,g_hFrame,NULL,g_hInst,NULL);
    BringWindowToTop(s_mr); SetFocus(s_mr);
}

// ============== «درخواست‌های تغییر پروفایل» (profile-change requests) ========
//  Each pending request shows old→new name and whether a photo was attached.
//  Manager can «تأیید» (approve → applies override + green cartable) or
//  «رد» (reject → red cartable with the optional reason from the textbox).
#define MGPRF_CLASS L"AzMgProf"
#define ID_PR_REASON 3101
struct ProfReqState {
    std::vector<ProfReq> reqs;
    int sel;                 // selected row (for the reason box), -1 none
    HWND eReason;
};
static HWND s_pr=NULL; static ProfReqState* s_prs=NULL;
static void prClose(){ if(s_pr&&IsWindow(s_pr)){ HWND v=s_pr; s_pr=NULL; DestroyWindow(v);} }
static RECT prCard(HWND h){ RECT rc; GetClientRect(h,&rc);
    int w=S(780),hh=S(600);
    RECT c={(rc.right-w)/2,(rc.bottom-hh)/2,(rc.right+w)/2,(rc.bottom+hh)/2}; return c; }
static int  prRowH(){ return S(84); }
static int  prListTop(HWND h){ RECT c=prCard(h); return c.top+S(64); }
static RECT prRowRect(HWND h, int i){
    RECT c=prCard(h); int y=prListTop(h)+i*(prRowH()+S(8));
    RECT r={c.left+S(20),y,c.right-S(20),y+prRowH()}; return r; }
// the two action buttons live on the LEFT side of each row
static RECT prApproveRect(RECT row){ RECT b={row.left+S(12),row.top+S(20),row.left+S(120),row.bottom-S(20)}; return b; }
static RECT prRejectRect (RECT row){ RECT b={row.left+S(128),row.top+S(20),row.left+S(220),row.bottom-S(20)}; return b; }
//  v1.9.0: a square photo thumbnail on the RIGHT edge of each profile request
//  row — clicking it enlarges the photo (system viewer, where it can be saved).
static RECT prPhotoRect(RECT row){ int s2=S(64); RECT b={row.right-S(14)-s2,row.top+(row.bottom-row.top-s2)/2,row.right-S(14),row.top+(row.bottom-row.top+s2)/2}; return b; }
//  Look up an employee's department from the user store (by full name or login).
static std::wstring prEmpDept(const std::wstring& who){
    if(who.empty()) return L"";
    for(auto& u:loadUsers())
        if(u.fullname==who || u.username==who) return u.dept;
    return L"";
}
static void prLayoutReason(HWND h){
    if(!s_prs||!s_prs->eReason) return;
    RECT c=prCard(h);
    int ry=c.bottom-S(52), rw=c.right-c.left-S(48)-S(150);
    MoveWindow(s_prs->eReason, c.left+S(24), ry, rw, S(34), TRUE);
    ShowWindow(s_prs->eReason, SW_SHOW);
}
static void prDoApprove(HWND h, int i){
    if(!s_prs || i<0 || i>=(int)s_prs->reqs.size()) return;
    if(s_prs->reqs[i].status!=0) return;
    setProfReqStatus(i, 1, L"");
    s_prs->reqs=loadProfReqs(); s_prs->sel=-1;
    SetWindowTextW(s_prs->eReason,L"");
    mgRefreshReqBadge();
    if(g_hManage) InvalidateRect(g_hManage,NULL,FALSE);
    InvalidateRect(h,NULL,FALSE);
}
static void prDoReject(HWND h, int i){
    if(!s_prs || i<0 || i>=(int)s_prs->reqs.size()) return;
    if(s_prs->reqs[i].status!=0) return;
    wchar_t rb[512]={0}; if(s_prs->eReason) GetWindowTextW(s_prs->eReason,rb,512);
    setProfReqStatus(i, 2, trim(rb));   // reason may be empty
    s_prs->reqs=loadProfReqs(); s_prs->sel=-1;
    SetWindowTextW(s_prs->eReason,L"");
    mgRefreshReqBadge();
    if(g_hManage) InvalidateRect(g_hManage,NULL,FALSE);
    InvalidateRect(h,NULL,FALSE);
}
static LRESULT CALLBACK prProc(HWND h, UINT m, WPARAM w, LPARAM l){
    switch(m){
    case WM_CREATE: {
        s_prs=new ProfReqState(); s_prs->reqs=loadProfReqs(); s_prs->sel=-1;
        DWORD es=WS_CHILD|WS_VISIBLE|ES_AUTOHSCROLL;
        s_prs->eReason=CreateWindowExW(0,L"EDIT",L"",es,0,0,10,10,h,(HMENU)ID_PR_REASON,g_hInst,0);
        SendMessageW(s_prs->eReason,WM_SETFONT,(WPARAM)g_fUI,TRUE);
        SendMessageW(s_prs->eReason,EM_SETCUEBANNER,TRUE,(LPARAM)L"دلیل رد (اختیاری)");
        prLayoutReason(h);
        return 0; }
    case WM_NCDESTROY: if(s_prs){ delete s_prs; s_prs=NULL; } s_pr=NULL;
        if(g_hFrame) InvalidateRect(g_hFrame,NULL,TRUE); return 0;
    case WM_ERASEBKGND: return 1;
    case WM_CTLCOLOREDIT:{ HDC dc=(HDC)w; SetTextColor(dc,g_theme.inputText);
        SetBkColor(dc,g_theme.inputBg); return (LRESULT)g_brInput; }
    case WM_KEYDOWN: if(w==VK_ESCAPE){ prClose(); return 0; } break;
    case WM_LBUTTONDOWN:{ POINT pt={GET_X_LPARAM(l),GET_Y_LPARAM(l)};
        RECT c=prCard(h);
        RECT cb={c.left+S(14),c.top+S(14),c.left+S(40),c.top+S(40)};
        if(!PtInRect(&c,pt)){ prClose(); return 0; }
        if(PtInRect(&cb,pt)){ prClose(); return 0; }
        if(s_prs){
            for(int i=0;i<(int)s_prs->reqs.size();i++){
                if(s_prs->reqs[i].status!=0) continue;
                RECT row=prRowRect(h,i);
                if(row.top>c.bottom-S(80)) break;
                RECT ab=prApproveRect(row), rb=prRejectRect(row);
                if(PtInRect(&ab,pt)){ prDoApprove(h,i); return 0; }
                if(PtInRect(&rb,pt)){ s_prs->sel=i; prDoReject(h,i); return 0; }
                // photo thumbnail → enlarge / download in the system viewer
                RECT prr=prPhotoRect(row);
                std::wstring photo = !s_prs->reqs[i].newPhoto.empty()
                                   ? s_prs->reqs[i].newPhoto : s_prs->reqs[i].oldPhoto;
                if(!photo.empty() && PtInRect(&prr,pt)){
                    ShellExecuteW(h,L"open",photo.c_str(),NULL,NULL,SW_SHOWNORMAL);
                    s_prs->sel=i; InvalidateRect(h,NULL,FALSE); return 0;
                }
                if(PtInRect(&row,pt)){ s_prs->sel=i; SetFocus(s_prs->eReason);
                    InvalidateRect(h,NULL,FALSE); return 0; }
            }
        }
        return 0; }
    case WM_PAINT:{ PAINTSTRUCT ps; HDC dc0=BeginPaint(h,&ps);
        RECT rc; GetClientRect(h,&rc);
        HDC dc=CreateCompatibleDC(dc0);
        HBITMAP bmp=CreateCompatibleBitmap(dc0,rc.right,rc.bottom);
        HGDIOBJ obm=SelectObject(dc,bmp);
        { HBRUSH sb=CreateSolidBrush(g_dark?RGB(6,9,14):RGB(28,36,48));
          FillRect(dc,&rc,sb); DeleteObject(sb); }
        gpFillAlpha(dc,rc,0,g_dark?RGB(0,0,0):RGB(20,28,40),120);
        RECT c=prCard(h);
        gpShadow(dc,c,S(20),S(22),80);
        { HBRUSH pb=CreateSolidBrush(g_theme.surface); FillRect(dc,&c,pb); DeleteObject(pb); }
        gpGradRoundRect(dc,c,S(18),g_theme.surfaceTop,g_theme.surface,g_theme.border);
        SetBkMode(dc,TRANSPARENT);
        { RECT cb={c.left+S(14),c.top+S(14),c.left+S(40),c.top+S(40)};
          RECT ci={cb.left+S(5),cb.top+S(5),cb.right-S(5),cb.bottom-S(5)};
          drawIcon(dc,ICO_X,ci,g_theme.text,S(2)); }
        SelectObject(dc,g_fTitle); SetTextColor(dc,g_infoAccent);
        RECT tr={c.left+S(24),c.top+S(16),c.right-S(54),c.top+S(50)};
        DrawTextW(dc,L"درخواست‌های تغییر پروفایل کارمندان",-1,&tr,
            DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
        // build pending list count
        int pending=0; if(s_prs) for(auto&q:s_prs->reqs) if(q.status==0) pending++;
        if(!s_prs || pending==0){
            SelectObject(dc,g_fUI); SetTextColor(dc,g_theme.textDim);
            RECT er={c.left+S(24),prListTop(h),c.right-S(24),prListTop(h)+S(40)};
            DrawTextW(dc,L"درخواست در انتظار تأییدی وجود ندارد.",-1,&er,
                DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
            // still show recently processed (history) below, dimmed
        }
        if(s_prs){
            for(int i=0;i<(int)s_prs->reqs.size();i++){
                ProfReq& q=s_prs->reqs[i];
                if(q.status!=0) continue;   // only pending in the actionable list
                RECT row=prRowRect(h,i);
                if(row.top>c.bottom-S(80)) break;
                bool selected=(s_prs->sel==i);
                gpRoundRectBg(dc,row,S(10),selected?g_theme.surface2:g_theme.surface,
                    selected?g_theme.accent:g_infoAccent, g_theme.surface,255);
                // photo thumbnail (RIGHT edge) — prefer the NEW photo, else old.
                std::wstring photo = !q.newPhoto.empty()? q.newPhoto : q.oldPhoto;
                int txtR = row.right-S(14);
                if(!photo.empty()){
                    RECT pr=prPhotoRect(row);
                    gpRoundRectBg(dc,pr,S(8),g_theme.surface2,g_infoAccent,g_theme.surface,255);
                    if(!gpDrawImageFileCircle(dc,photo,pr)){
                        RECT ii={pr.left+S(16),pr.top+S(16),pr.right-S(16),pr.bottom-S(16)};
                        drawIcon(dc,ICO_USER,ii,g_theme.textDim,S(2));
                    }
                    // tiny "enlarge" hint glyph
                    SelectObject(dc,g_fSmall); SetTextColor(dc,g_infoAccent2);
                    RECT eh={pr.left,pr.bottom+S(1),pr.right,pr.bottom+S(15)};
                    DrawTextW(dc,L"بزرگ‌نمایی",-1,&eh,DT_CENTER|DT_SINGLELINE|DT_NOPREFIX);
                    txtR = pr.left-S(10);
                }
                int txtL = row.left+S(230);
                // text (RIGHT side)
                SelectObject(dc,g_fUIB); SetTextColor(dc,g_theme.text);
                RECT l1={txtL,row.top+S(6),txtR,row.top+S(28)};
                std::wstring head=L"کارمند: "+(q.user.empty()?std::wstring(L"—"):q.user);
                std::wstring dep=prEmpDept(q.user);
                if(!dep.empty()) head+=L"   •   بخش: "+dep;
                DrawTextW(dc,head.c_str(),-1,&l1,
                    DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX|DT_END_ELLIPSIS);
                SelectObject(dc,g_fSmall); SetTextColor(dc,g_theme.textDim);
                RECT lm={txtL,row.top+S(28),txtR,row.top+S(46)};
                std::wstring meta=L"سامانه: "+systemSourceName()+L"   •   "+q.time;
                DrawTextW(dc,meta.c_str(),-1,&lm,
                    DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX|DT_END_ELLIPSIS);
                SelectObject(dc,g_fUI); SetTextColor(dc,g_theme.textDim);
                RECT l2={txtL,row.top+S(46),txtR,row.top+S(68)};
                std::wstring nm=L"نام: "+q.oldName+L"  ←  "+q.newName;
                DrawTextW(dc,nm.c_str(),-1,&l2,
                    DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX|DT_END_ELLIPSIS);
                RECT l3={txtL,row.top+S(66),txtR,row.bottom-S(6)};
                std::wstring ph = q.newPhoto.empty()? std::wstring(L"عکس: بدون تغییر")
                                                    : std::wstring(L"عکس: جدید پیوست شد (برای بزرگ‌نمایی/دانلود کلیک کنید)");
                DrawTextW(dc,ph.c_str(),-1,&l3,
                    DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX|DT_END_ELLIPSIS);
                // action buttons (LEFT)
                RECT ab=prApproveRect(row);
                gpRoundRect(dc,ab,S(8),g_theme.success,g_theme.success,255);
                SetTextColor(dc,RGB(255,255,255)); SelectObject(dc,g_fUIB);
                DrawTextW(dc,L"تأیید",-1,&ab,DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
                RECT rb=prRejectRect(row);
                gpRoundRect(dc,rb,S(8),g_theme.danger,g_theme.danger,255);
                SetTextColor(dc,RGB(255,255,255));
                DrawTextW(dc,L"رد",-1,&rb,DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
            }
        }
        // reason hint label (above the textbox, LEFT-aligned in RTL = right edge)
        SelectObject(dc,g_fSmall); SetTextColor(dc,g_theme.textDim);
        RECT hr={c.left+S(24),c.bottom-S(80),c.right-S(24),c.bottom-S(58)};
        DrawTextW(dc,L"برای «رد»، ابتدا ردیف را انتخاب کنید و در صورت تمایل دلیل را بنویسید (اختیاری).",-1,&hr,
            DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
        BitBlt(dc0,0,0,rc.right,rc.bottom,dc,0,0,SRCCOPY);
        SelectObject(dc,obm); DeleteObject(bmp); DeleteDC(dc);
        EndPaint(h,&ps); return 0; }
    }
    return DefWindowProcW(h,m,w,l);
}
static void openProfReqs(HWND owner){
    if(s_pr&&IsWindow(s_pr)) prClose();
    static bool reg=false;
    if(!reg){ WNDCLASSW wc={0}; wc.lpfnWndProc=prProc; wc.hInstance=g_hInst;
        wc.hCursor=LoadCursor(NULL,IDC_ARROW); wc.lpszClassName=MGPRF_CLASS;
        RegisterClassW(&wc); reg=true; }
    RECT rc; GetClientRect(g_hFrame,&rc);
    POINT org={0,0}; ClientToScreen(g_hFrame,&org);
    s_pr=CreateWindowExW(WS_EX_TOPMOST,MGPRF_CLASS,L"",WS_POPUP|WS_VISIBLE,
        org.x,org.y,rc.right,rc.bottom,g_hFrame,NULL,g_hInst,NULL);
    BringWindowToTop(s_pr); SetFocus(s_pr);
}

// ====================== «پیام به کارمندان» compose modal =====================
//  v1.8.0 full redesign:
//    • live SEARCH box (by id / unique-id / name) with suggestion list
//    • department filter chips (multi-select, combinable) for message routing
//    • severity (عادی/فوری/بحرانی)
//    • MEDIA attachment (image / video / gif / jpg / png / word / any) — copied
//      into the local attachments store and sent with the message so the
//      recipient can download it; content is preserved permanently.
#define MGMSG_CLASS L"AzMgMsg"
#define MM_E_SEARCH 3201
#define MM_E_TEXT   3202
#define MM_R_NORMAL 3203
#define MM_R_URGENT 3204
#define MM_R_CRIT   3205
#define MM_B_SEND   3206
#define MM_B_CANCEL 3207
#define MM_B_ATTACH 3208
#define MM_LB_SUG   3209
#define MM_B_SAVE   3210
#define MM_B_VIEWEMP 3211   // «مشاهدهٔ کارمندان» toggle for the right-side panel
#define MM_E_EMPSRCH 3212   // employee-panel search box
struct MsgState {
    HWND eSearch, eText, rNormal, rUrgent, rCrit, bSend, bCancel, bAttach, lbSug, bSave;
    HWND bViewEmp, eEmpSrch;          // employee panel toggle + its search box
    std::vector<User> users;          // all users (recipient pool)
    std::vector<EmpProfile> profs;    // parallel profiles (for id/uid match)
    std::vector<int> matchIdx;        // current search matches → users index
    std::vector<DeptCat> depts;       // dept filter chips
    std::vector<bool> deptOn;         // which dept chips are active
    int selUser;                      // chosen single recipient (-1 = use depts/all)
    std::wstring attachPath;          // local copied attachment path
    std::wstring attachName;          // display name
    bool sendToSaved;                 // also archive to saved messages?
    // right-side employee panel (CLOSED by default)
    bool empPanelOpen;
    std::wstring empQuery;            // filter text for the panel
    int empHot;                       // hot-tracked panel row (-1 none)
    int empScroll;                    // vertical scroll offset (px)
    // §E: PRow carries a code too — for section headers it is the section code,
    // for employees it is the personnel code (empId). kind 0=dept header,1=emp.
    struct PRow { int kind; int idx; std::wstring name; std::wstring code; };
    std::vector<PRow> empRows;
};
static HWND s_mm=NULL; static MsgState* s_ms=NULL;
static void mmClose(){ if(s_mm&&IsWindow(s_mm)){ HWND v=s_mm; s_mm=NULL; DestroyWindow(v);} }
static int  mmPanelW(){ return S(280); }
static RECT mmCardOuter(HWND h){ RECT rc; GetClientRect(h,&rc);
    bool pan = s_ms && s_ms->empPanelOpen;
    int w=S(640)+(pan?mmPanelW()+S(12):0), hh=S(620);
    RECT c={(rc.right-w)/2,(rc.bottom-hh)/2,(rc.right+w)/2,(rc.bottom+hh)/2}; return c; }
// the compose card (right part) — fixed width; the panel sits to its LEFT.
static RECT mmCard(HWND h){ RECT o=mmCardOuter(h);
    RECT c={o.right-S(640),o.top,o.right,o.bottom}; return c; }
static RECT mmPanelRect(HWND h){ RECT o=mmCardOuter(h);
    RECT p={o.left,o.top,o.left+mmPanelW(),o.bottom}; return p; }
static int  mmPanelListTop(HWND h){ RECT p=mmPanelRect(h); return p.top+S(96); }
static int  mmPanelRowH(){ return S(40); }
// dept chip geometry (left→right wrap inside the card)
static int  mmChipsTop(HWND h){ RECT c=mmCard(h); return c.top+S(150); }
static RECT mmChipRect(HWND h, int i){
    RECT c=mmCard(h); int x=c.right-S(28), y=mmChipsTop(h);
    int cw=S(150), ch=S(30), gap=S(8), perRow=(c.right-c.left-S(56))/(cw+gap);
    if(perRow<1) perRow=1;
    int row=i/perRow, col=i%perRow;
    int rx=x-(col+1)*cw-col*gap;
    RECT r={rx,y+row*(ch+gap),rx+cw,y+row*(ch+gap)+ch}; return r;
}
static int mmChipRows(HWND h){
    if(!s_ms||s_ms->depts.empty()) return 0;
    RECT c=mmCard(h); int cw=S(150),gap=S(8);
    int perRow=(c.right-c.left-S(56))/(cw+gap); if(perRow<1) perRow=1;
    return ((int)s_ms->depts.size()+perRow-1)/perRow;
}
static void mmRunSearch(HWND h){
    if(!s_ms) return;
    wchar_t buf[128]={0}; GetWindowTextW(s_ms->eSearch,buf,128);
    std::wstring q=trim(buf);
    s_ms->matchIdx.clear();
    SendMessageW(s_ms->lbSug,LB_RESETCONTENT,0,0);
    if(q.empty()){ ShowWindow(s_ms->lbSug,SW_HIDE); return; }
    std::wstring ql=q; for(auto&ch:ql) ch=towlower(ch);
    for(size_t i=0;i<s_ms->users.size() && s_ms->matchIdx.size()<8;i++){
        std::wstring nm=s_ms->users[i].fullname; for(auto&ch:nm) ch=towlower(ch);
        std::wstring un=s_ms->users[i].username; for(auto&ch:un) ch=towlower(ch);
        const EmpProfile& p=s_ms->profs[i];
        bool hit = nm.find(ql)!=std::wstring::npos || un.find(ql)!=std::wstring::npos
                || p.empId.find(q)!=std::wstring::npos
                || p.uniqueId.find(q)!=std::wstring::npos
                || p.nationalId.find(q)!=std::wstring::npos;
        if(hit){
            s_ms->matchIdx.push_back((int)i);
            std::wstring it=(s_ms->users[i].fullname.empty()?s_ms->users[i].username
                            :s_ms->users[i].fullname)+L"  (@"+s_ms->users[i].username+L")";
            if(!p.empId.empty()) it+=L"  ["+p.empId+L"]";
            SendMessageW(s_ms->lbSug,LB_ADDSTRING,0,(LPARAM)it.c_str());
        }
    }
    ShowWindow(s_ms->lbSug, s_ms->matchIdx.empty()?SW_HIDE:SW_SHOW);
}
// Rebuild the grouped employee list for the right-side panel:
//   • grouped by department; ONLINE first (green dot) then OFFLINE (gray dot)
//   • honours the panel search box (name / username / dept).
//  §E (v1.10.0): grouped by SECTION, ONLINE first; each section header shows
//  the section CODE, each employee row shows the personnel CODE. The list is
//  CAPPED at 20 sections and 20 employees per section (lazy virtualization:
//  rows beyond the cap are summarised with a «… و N مورد دیگر» line so the
//  panel stays light on weak hardware and never balloons). Online presence is
//  preserved; sorting keeps online employees at the top of each section.
#define MM_MAX_SECTIONS 20
#define MM_MAX_EMPS_PER_SECTION 20
static void mmBuildEmpRows(HWND h){
    if(!s_ms) return;
    s_ms->empRows.clear();
    std::wstring q=s_ms->empQuery; for(auto&ch:q) ch=towlower(ch);
    auto deptNameOf=[&](int i)->std::wstring{
        std::wstring did=s_ms->profs[i].deptId;
        for(auto&d:s_ms->depts) if(d.id==did) return d.name;
        return s_ms->users[i].dept.empty()?L"بدون بخش":s_ms->users[i].dept;
    };
    auto deptCodeOf=[&](const std::wstring& name)->std::wstring{
        for(auto&d:s_ms->depts) if(d.name==name) return d.id;
        return L"\u2014";   // —
    };
    // build the section ordering, capped at MM_MAX_SECTIONS
    std::vector<std::wstring> groups;
    for(auto&d:s_ms->depts){ if((int)groups.size()>=MM_MAX_SECTIONS) break;
        groups.push_back(d.name); }
    groups.push_back(L"بدون بخش");
    auto matches=[&](int i)->bool{
        if(q.empty()) return true;
        std::wstring nm=s_ms->users[i].fullname; for(auto&ch:nm) ch=towlower(ch);
        std::wstring un=s_ms->users[i].username; for(auto&ch:un) ch=towlower(ch);
        std::wstring dn=deptNameOf(i);            for(auto&ch:dn) ch=towlower(ch);
        std::wstring cd=s_ms->profs[i].empId;     for(auto&ch:cd) ch=towlower(ch);
        return nm.find(q)!=std::wstring::npos||un.find(q)!=std::wstring::npos
             ||dn.find(q)!=std::wstring::npos||cd.find(q)!=std::wstring::npos;
    };
    int sectionsShown=0;
    for(auto&g:groups){
        if(sectionsShown>=MM_MAX_SECTIONS) break;
        std::vector<int> on, off;
        for(int i=0;i<(int)s_ms->users.size();i++){
            if(deptNameOf(i)!=g) continue;
            if(!matches(i)) continue;
            if(isUserOnline(s_ms->users[i].username)) on.push_back(i);
            else off.push_back(i);
        }
        if(on.empty()&&off.empty()) continue;
        std::wstring code = (g==L"بدون بخش") ? std::wstring(L"\u2014") : deptCodeOf(g);
        s_ms->empRows.push_back({0,-1,g,code});
        sectionsShown++;
        // ONLINE first, then OFFLINE — capped per section.
        int emitted=0; int totalInSection=(int)on.size()+(int)off.size();
        auto emit=[&](const std::vector<int>& v){
            for(int i:v){ if(emitted>=MM_MAX_EMPS_PER_SECTION) return;
                s_ms->empRows.push_back({1,i,L"",s_ms->profs[i].empId}); emitted++; }
        };
        emit(on); emit(off);
        if(totalInSection>emitted){
            wchar_t mb[64]; swprintf(mb,64,L"\u2026 \u0648 %d \u0645\u0648\u0631\u062f \u062f\u06cc\u06af\u0631",
                totalInSection-emitted);
            s_ms->empRows.push_back({2,-1,mb,L""});   // kind 2 = "more" summary
        }
    }
}
static void mmLayout(HWND h){
    if(!s_ms) return; RECT c=mmCard(h);
    int x=c.left+S(28), r=c.right-S(28), y=c.top+S(64), fh=S(34);
    // search box (full width)
    MoveWindow(s_ms->eSearch, x, y, r-x, fh, TRUE);
    // suggestion list floats just under the search box
    MoveWindow(s_ms->lbSug, x, y+fh+S(2), r-x, S(140), TRUE);
    // dept chips occupy mmChipsTop region (owner-drawn) — push the rest below
    int rows=mmChipRows(h); int chipsH = rows>0 ? rows*(S(30)+S(8))+S(8) : 0;
    int yy = mmChipsTop(h)+chipsH+S(28);
    // severity radios
    MoveWindow(s_ms->rNormal, r-S(110), yy, S(110), fh, TRUE);
    MoveWindow(s_ms->rUrgent, r-S(110)-S(120), yy, S(120), fh, TRUE);
    MoveWindow(s_ms->rCrit,   r-S(110)-S(240), yy, S(120), fh, TRUE);
    // attach button (left of radios)
    MoveWindow(s_ms->bAttach, x, yy, S(170), fh, TRUE);
    yy+=fh+S(12);
    // message body
    int bodyBottom=c.bottom-S(96);
    MoveWindow(s_ms->eText, x, yy, r-x, bodyBottom-yy, TRUE);
    // "send to saved" toggle (shown only when saved messages enabled)
    MoveWindow(s_ms->bSave, x, c.bottom-S(86), S(260), S(30), TRUE);
    ShowWindow(s_ms->bSave, savedMsgsEnabled()?SW_SHOW:SW_HIDE);
    // action buttons
    int by=c.bottom-S(50), bw=S(150);
    MoveWindow(s_ms->bSend,   r-bw,         by, bw, S(38), TRUE);
    MoveWindow(s_ms->bCancel, r-bw-S(8)-bw, by, bw, S(38), TRUE);
}
static void mmPickAttachment(HWND h){
    if(!s_ms) return;
    wchar_t file[1024]={0};
    OPENFILENAMEW ofn={0}; ofn.lStructSize=sizeof(ofn); ofn.hwndOwner=h;
    ofn.lpstrFilter=L"همهٔ رسانه‌ها\0*.jpg;*.jpeg;*.png;*.gif;*.bmp;*.mp4;*.avi;*.mov;*.mkv;*.doc;*.docx;*.pdf\0"
                    L"تصاویر (JPG/PNG/GIF)\0*.jpg;*.jpeg;*.png;*.gif;*.bmp\0"
                    L"ویدئو\0*.mp4;*.avi;*.mov;*.mkv\0"
                    L"اسناد (Word/PDF)\0*.doc;*.docx;*.pdf\0"
                    L"همهٔ فایل‌ها\0*.*\0\0";
    ofn.lpstrFile=file; ofn.nMaxFile=1024;
    ofn.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST;
    if(GetOpenFileNameW(&ofn)){
        std::wstring stored=copyAttachmentLocal(file);
        if(!stored.empty()){
            s_ms->attachPath=stored;
            size_t sl=stored.find_last_of(L"\\/");
            s_ms->attachName=(sl==std::wstring::npos)?stored:stored.substr(sl+1);
            SetWindowTextW(s_ms->bAttach,(L"📎 "+s_ms->attachName).c_str());
        } else {
            MessageBoxW(h,L"کپی پیوست ناموفق بود.",L"پیام",MB_OK|MB_ICONWARNING);
        }
        InvalidateRect(h,NULL,FALSE);
    }
}
static void mmDoSend(HWND h){
    if(!s_ms) return;
    wchar_t buf[4096]={0}; GetWindowTextW(s_ms->eText,buf,4096);
    std::wstring text=trim(buf);
    if(text.empty()){
        MessageBoxW(h,L"متن پیام را وارد کنید.",L"پیام",MB_OK|MB_ICONINFORMATION); return; }
    int type=KMSG_NORMAL;
    if(SendMessageW(s_ms->rUrgent,BM_GETCHECK,0,0)==BST_CHECKED) type=KMSG_URGENT;
    else if(SendMessageW(s_ms->rCrit,BM_GETCHECK,0,0)==BST_CHECKED) type=KMSG_CRITICAL;
    std::wstring body=text;
    if(!s_ms->attachPath.empty())
        body += L"\n\n📎 پیوست: "+s_ms->attachName+L"\n["+s_ms->attachPath+L"]";
    std::wstring from=g_session.user.fullname.empty()?g_session.user.username
                                                     :g_session.user.fullname;
    // routing: a single chosen recipient overrides; else union of selected
    // departments; else broadcast to everyone (*).
    int sent=0;
    if(s_ms->selUser>=0 && s_ms->selUser<(int)s_ms->users.size()){
        pushMessageT(from,s_ms->users[s_ms->selUser].username,body,type); sent=1;
    } else {
        bool anyDept=false;
        for(size_t d=0;d<s_ms->depts.size();d++) if(s_ms->deptOn[d]) anyDept=true;
        if(anyDept){
            for(size_t i=0;i<s_ms->users.size();i++){
                std::wstring did=s_ms->profs[i].deptId;
                for(size_t d=0;d<s_ms->depts.size();d++){
                    if(s_ms->deptOn[d] && did==s_ms->depts[d].id){
                        pushMessageT(from,s_ms->users[i].username,body,type); sent++;
                        break;
                    }
                }
            }
            if(sent==0){ MessageBoxW(h,L"در بخش‌های انتخاب‌شده کارمندی یافت نشد.",
                L"پیام",MB_OK|MB_ICONINFORMATION); return; }
        } else {
            pushMessageT(from,L"*",body,type); sent=1;   // broadcast
        }
    }
    // optionally archive to saved messages
    if(savedMsgsEnabled() &&
       SendMessageW(s_ms->bSave,BM_GETCHECK,0,0)==BST_CHECKED){
        std::wstring to = s_ms->selUser>=0 ? s_ms->users[s_ms->selUser].username : L"*";
        pushSavedMsg(from,to,text,type,s_ms->attachPath);
    }
    MessageBoxW(h,(L"پیام برای "+toFaDigits(std::to_wstring(sent))+
                  L" گیرنده ارسال شد.").c_str(),
                L"ارسال موفق",MB_OK|MB_ICONINFORMATION);
    mmClose();
    if(g_hManage) InvalidateRect(g_hManage,NULL,TRUE);
}
static LRESULT CALLBACK mmProc(HWND h, UINT m, WPARAM w, LPARAM l){
    switch(m){
    case WM_CREATE:{
        s_ms=new MsgState(); s_ms->selUser=-1; s_ms->sendToSaved=false;
        s_ms->empPanelOpen=false; s_ms->empHot=-1; s_ms->empScroll=0;
        DWORD es=WS_CHILD|WS_VISIBLE|WS_TABSTOP|ES_AUTOHSCROLL;
        s_ms->eSearch=CreateWindowExW(0,L"EDIT",L"",es,0,0,10,10,h,(HMENU)MM_E_SEARCH,g_hInst,0);
        SendMessageW(s_ms->eSearch,EM_SETCUEBANNER,TRUE,
            (LPARAM)L"جستجوی گیرنده: نام / نام‌کاربری / کد پرسنلی / شناسه یکتا");
        s_ms->lbSug=CreateWindowExW(0,L"LISTBOX",L"",
            WS_CHILD|WS_BORDER|WS_VSCROLL|LBS_NOTIFY,0,0,10,10,h,(HMENU)MM_LB_SUG,g_hInst,0);
        s_ms->eText=CreateWindowExW(0,L"EDIT",L"",
            WS_CHILD|WS_VISIBLE|WS_TABSTOP|ES_MULTILINE|ES_AUTOVSCROLL|WS_VSCROLL,
            0,0,10,10,h,(HMENU)MM_E_TEXT,g_hInst,0);
        DWORD rb=WS_CHILD|WS_VISIBLE|BS_AUTORADIOBUTTON|BS_RIGHTBUTTON|BS_RIGHT;
        s_ms->rNormal=CreateWindowExW(0,L"BUTTON",L"عادی",rb|WS_GROUP,0,0,10,10,h,(HMENU)MM_R_NORMAL,g_hInst,0);
        s_ms->rUrgent=CreateWindowExW(0,L"BUTTON",L"فوری",rb,0,0,10,10,h,(HMENU)MM_R_URGENT,g_hInst,0);
        s_ms->rCrit  =CreateWindowExW(0,L"BUTTON",L"بحرانی",rb,0,0,10,10,h,(HMENU)MM_R_CRIT,g_hInst,0);
        s_ms->bAttach=createFlatButton(h,MM_B_ATTACH,L"📎 افزودن رسانه",ICO_NONE,BS_OUTLINE,0,0,10,10);
        s_ms->bSave  =CreateWindowExW(0,L"BUTTON",L"ذخیره در پیام‌های ذخیره‌شده",
            WS_CHILD|BS_AUTOCHECKBOX|BS_RIGHTBUTTON|BS_RIGHT,0,0,10,10,h,(HMENU)MM_B_SAVE,g_hInst,0);
        s_ms->bSend  =createFlatButton(h,MM_B_SEND,L"ارسال پیام",ICO_BELL,BS_PRIMARY,0,0,10,10);
        s_ms->bCancel=createFlatButton(h,MM_B_CANCEL,L"انصراف",ICO_X,BS_OUTLINE,0,0,10,10);
        s_ms->bViewEmp=createFlatButton(h,MM_B_VIEWEMP,L"مشاهدهٔ کارمندان",ICO_USER,BS_OUTLINE,0,0,10,10);
        s_ms->eEmpSrch=CreateWindowExW(0,L"EDIT",L"",
            WS_CHILD|WS_TABSTOP|ES_AUTOHSCROLL,0,0,10,10,h,(HMENU)MM_E_EMPSRCH,g_hInst,0);
        SendMessageW(s_ms->eEmpSrch,EM_SETCUEBANNER,TRUE,(LPARAM)L"جستجوی کارمند…");
        SendMessageW(s_ms->eEmpSrch,WM_SETFONT,(WPARAM)g_fUI,TRUE);
        s_ms->users=loadUsers();
        for(auto&u:s_ms->users) s_ms->profs.push_back(loadEmpProfile(u.username));
        s_ms->depts=loadDepts();
        s_ms->deptOn.assign(s_ms->depts.size(),false);
        SendMessageW(s_ms->rNormal,BM_SETCHECK,BST_CHECKED,0);
        HWND fc[7]={s_ms->eSearch,s_ms->eText,s_ms->rNormal,s_ms->rUrgent,s_ms->rCrit,s_ms->lbSug,0};
        for(int i=0;fc[i];i++) SendMessageW(fc[i],WM_SETFONT,(WPARAM)g_fUI,TRUE);
        SendMessageW(s_ms->bSave,WM_SETFONT,(WPARAM)g_fUI,TRUE);
        mmLayout(h);
        return 0; }
    case WM_SIZE: mmLayout(h); return 0;
    case WM_NCDESTROY: if(s_ms){ delete s_ms; s_ms=NULL; } s_mm=NULL;
        if(g_hFrame) InvalidateRect(g_hFrame,NULL,TRUE); return 0;
    case WM_ERASEBKGND: return 1;
    case WM_CTLCOLOREDIT: case WM_CTLCOLORLISTBOX:{ HDC dc=(HDC)w;
        SetTextColor(dc,g_theme.inputText); SetBkColor(dc,g_theme.inputBg);
        return (LRESULT)g_brInput; }
    case WM_CTLCOLORSTATIC:{ HDC dc=(HDC)w;
        SetTextColor(dc,g_theme.text); SetBkColor(dc,g_theme.surface);
        return (LRESULT)g_brSurface; }
    case WM_LBUTTONDOWN:{ POINT pt={GET_X_LPARAM(l),GET_Y_LPARAM(l)};
        RECT c=mmCard(h);
        RECT cb={c.left+S(14),c.top+S(14),c.left+S(40),c.top+S(40)};
        // employee panel: pick a recipient by clicking a row
        if(s_ms && s_ms->empPanelOpen){
            RECT p=mmPanelRect(h);
            if(PtInRect(&p,pt)){
                int top=mmPanelListTop(h)-s_ms->empScroll, rh=mmPanelRowH();
                for(int i=0;i<(int)s_ms->empRows.size();i++){
                    if(s_ms->empRows[i].kind!=1) continue;
                    int ry=top+i*rh; RECT rr={p.left+S(8),ry,p.right-S(8),ry+rh};
                    if(PtInRect(&rr,pt)){
                        int u=s_ms->empRows[i].idx;
                        s_ms->selUser=u;
                        std::wstring nm=s_ms->users[u].fullname.empty()
                            ?s_ms->users[u].username:s_ms->users[u].fullname;
                        SetWindowTextW(s_ms->eSearch,nm.c_str());
                        for(size_t d=0;d<s_ms->deptOn.size();d++) s_ms->deptOn[d]=false;
                        ShowWindow(s_ms->lbSug,SW_HIDE);
                        InvalidateRect(h,NULL,FALSE);
                        return 0;
                    }
                }
                return 0;   // clicks inside the panel never close the modal
            }
        }
        if(!PtInRect(&c,pt)){ mmClose(); return 0; }
        if(PtInRect(&cb,pt)){ mmClose(); return 0; }
        // dept chip toggles
        if(s_ms) for(int i=0;i<(int)s_ms->depts.size();i++){
            RECT r=mmChipRect(h,i);
            if(PtInRect(&r,pt)){ s_ms->deptOn[i]=!s_ms->deptOn[i];
                s_ms->selUser=-1;   // dept routing clears single-recipient
                InvalidateRect(h,NULL,FALSE); return 0; }
        }
        return 0; }
    case WM_MOUSEMOVE:{ if(!s_ms||!s_ms->empPanelOpen) break;
        POINT pt={GET_X_LPARAM(l),GET_Y_LPARAM(l)};
        int oh=s_ms->empHot; s_ms->empHot=-1;
        RECT p=mmPanelRect(h);
        int top=mmPanelListTop(h)-s_ms->empScroll, rh=mmPanelRowH();
        for(int i=0;i<(int)s_ms->empRows.size();i++){
            if(s_ms->empRows[i].kind!=1) continue;
            int ry=top+i*rh; RECT rr={p.left+S(8),ry,p.right-S(8),ry+rh};
            if(PtInRect(&rr,pt)){ s_ms->empHot=i; break; }
        }
        if(oh!=s_ms->empHot) InvalidateRect(h,NULL,FALSE);
        return 0; }
    case WM_MOUSEWHEEL:{ if(!s_ms||!s_ms->empPanelOpen) break;
        int d=GET_WHEEL_DELTA_WPARAM(w);
        s_ms->empScroll-=d/2; if(s_ms->empScroll<0) s_ms->empScroll=0;
        int contentH=(int)s_ms->empRows.size()*mmPanelRowH();
        RECT p=mmPanelRect(h); int viewH=p.bottom-mmPanelListTop(h)-S(12);
        int maxS=contentH-viewH; if(maxS<0) maxS=0;
        if(s_ms->empScroll>maxS) s_ms->empScroll=maxS;
        InvalidateRect(h,NULL,FALSE); return 0; }
    case WM_COMMAND:{ int id=LOWORD(w), code=HIWORD(w);
        if(id==MM_E_SEARCH && code==EN_CHANGE){ mmRunSearch(h); }
        else if(id==MM_E_EMPSRCH && code==EN_CHANGE){
            wchar_t b[128]={0}; GetWindowTextW(s_ms->eEmpSrch,b,128);
            s_ms->empQuery=trim(b); s_ms->empScroll=0;
            mmBuildEmpRows(h); InvalidateRect(h,NULL,FALSE);
        }
        else if(id==MM_B_VIEWEMP){
            s_ms->empPanelOpen=!s_ms->empPanelOpen;
            SetWindowTextW(s_ms->bViewEmp, s_ms->empPanelOpen?L"بستن کارمندان":L"مشاهدهٔ کارمندان");
            if(s_ms->empPanelOpen){ s_ms->empScroll=0; mmBuildEmpRows(h); }
            mmLayout(h); InvalidateRect(h,NULL,TRUE);
        }
        else if(id==MM_LB_SUG && code==LBN_SELCHANGE){
            int sel=(int)SendMessageW(s_ms->lbSug,LB_GETCURSEL,0,0);
            if(sel>=0 && sel<(int)s_ms->matchIdx.size()){
                s_ms->selUser=s_ms->matchIdx[sel];
                std::wstring nm=s_ms->users[s_ms->selUser].fullname.empty()
                    ?s_ms->users[s_ms->selUser].username:s_ms->users[s_ms->selUser].fullname;
                SetWindowTextW(s_ms->eSearch,nm.c_str());
                ShowWindow(s_ms->lbSug,SW_HIDE);
                for(size_t d=0;d<s_ms->deptOn.size();d++) s_ms->deptOn[d]=false;
                InvalidateRect(h,NULL,FALSE);
            }
        }
        else if(id==MM_B_ATTACH) mmPickAttachment(h);
        else if(id==MM_B_SEND) mmDoSend(h);
        else if(id==MM_B_CANCEL) mmClose();
        return 0; }
    case WM_KEYDOWN: if(w==VK_ESCAPE){ mmClose(); return 0; } break;
    case WM_PAINT:{ PAINTSTRUCT ps; HDC dc0=BeginPaint(h,&ps);
        RECT rc; GetClientRect(h,&rc);
        HDC dc=CreateCompatibleDC(dc0);
        HBITMAP bmp=CreateCompatibleBitmap(dc0,rc.right,rc.bottom);
        HGDIOBJ obm=SelectObject(dc,bmp);
        { HBRUSH sb=CreateSolidBrush(g_dark?RGB(6,9,14):RGB(28,36,48));
          FillRect(dc,&rc,sb); DeleteObject(sb); }
        gpFillAlpha(dc,rc,0,g_dark?RGB(0,0,0):RGB(20,28,40),120);
        RECT c=mmCard(h);
        gpShadow(dc,c,S(20),S(22),80);
        gpGradRoundRectBg(dc,c,S(18),g_theme.surfaceTop,g_theme.surface,g_theme.border,
            g_dark?RGB(6,9,14):RGB(28,36,48));
        SetBkMode(dc,TRANSPARENT);
        { RECT cb={c.left+S(14),c.top+S(14),c.left+S(40),c.top+S(40)};
          RECT ci={cb.left+S(5),cb.top+S(5),cb.right-S(5),cb.bottom-S(5)};
          drawIcon(dc,ICO_X,ci,g_theme.text,S(2)); }
        SelectObject(dc,g_fTitle); SetTextColor(dc,g_theme.accent);
        RECT tr={c.left+S(28),c.top+S(16),c.right-S(54),c.top+S(46)};
        DrawTextW(dc,L"پیام به کارمندان",-1,&tr,
            DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
        // dept-filter label + chips
        SelectObject(dc,g_fSmall); SetTextColor(dc,g_theme.textDim);
        RECT dl={c.left+S(28),mmChipsTop(h)-S(22),c.right-S(28),mmChipsTop(h)-S(4)};
        DrawTextW(dc,L"مسیردهی بر اساس بخش (چندانتخابی) — یا یک گیرنده از جستجو:",-1,&dl,
            DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
        if(s_ms) for(int i=0;i<(int)s_ms->depts.size();i++){
            RECT r=mmChipRect(h,i); bool on=s_ms->deptOn[i];
            gpRoundRectBg(dc,r,S(14),on?g_theme.accent:g_theme.surface2,
                on?g_theme.accent:g_theme.border, g_theme.surface,255);
            SetTextColor(dc,on?g_theme.accentText:g_theme.text); SelectObject(dc,g_fSmall);
            DrawTextW(dc,s_ms->depts[i].name.c_str(),-1,&r,
                DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX|DT_END_ELLIPSIS);
        }
        // selected single recipient indicator (top, under title)
        if(s_ms && s_ms->selUser>=0){
            SelectObject(dc,g_fSmall); SetTextColor(dc,g_theme.accent);
            RECT sr={c.left+S(190),c.top+S(20),c.right-S(54),c.top+S(44)};
            std::wstring nm=s_ms->users[s_ms->selUser].fullname.empty()
                ?s_ms->users[s_ms->selUser].username:s_ms->users[s_ms->selUser].fullname;
            DrawTextW(dc,(L"گیرنده: "+nm).c_str(),-1,&sr,
                DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX|DT_END_ELLIPSIS);
        }
        // ---- right-side employee panel (when open) ----
        if(s_ms && s_ms->empPanelOpen){
            RECT p=mmPanelRect(h);
            gpGradRoundRectBg(dc,p,S(16),g_theme.surfaceTop,g_theme.surface,g_theme.border,
                g_dark?RGB(6,9,14):RGB(28,36,48));
            SelectObject(dc,g_fUIB); SetTextColor(dc,g_theme.text);
            RECT ph={p.left+S(14),p.top+S(14),p.right-S(14),p.top+S(40)};
            DrawTextW(dc,L"کارمندان",-1,&ph,DT_RIGHT|DT_VCENTER|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);
            RECT clip={p.left+S(2),mmPanelListTop(h),p.right-S(2),p.bottom-S(8)};
            HRGN rgn=CreateRectRgn(clip.left,clip.top,clip.right,clip.bottom);
            SelectClipRgn(dc,rgn);
            int top=mmPanelListTop(h)-s_ms->empScroll, rh=mmPanelRowH();
            for(int i=0;i<(int)s_ms->empRows.size();i++){
                int ry=top+i*rh;
                if(ry+rh<clip.top||ry>clip.bottom) continue;
                auto& row=s_ms->empRows[i];
                if(row.kind==0){
                    // §E: section header — name on the right, section code badge on the left.
                    SelectObject(dc,g_fSmall); SetTextColor(dc,g_infoAccent2);
                    RECT hr={p.left+S(14),ry+S(6),p.right-S(14),ry+rh-S(4)};
                    DrawTextW(dc,row.name.c_str(),-1,&hr,
                        DT_RIGHT|DT_VCENTER|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX|DT_END_ELLIPSIS);
                    if(!row.code.empty()){
                        // §G: section code in monospace so digits align in a column.
                        SelectObject(dc,g_fCode);
                        SetTextColor(dc,blendColor(g_theme.textDim,g_theme.surface,20));
                        RECT cr={p.left+S(14),ry+S(6),p.left+S(120),ry+rh-S(4)};
                        DrawTextW(dc,row.code.c_str(),-1,&cr,
                            DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX|DT_END_ELLIPSIS);
                        SelectObject(dc,g_fSmall);
                    }
                    gpLine(dc,p.left+S(14),ry+rh-S(2),p.right-S(14),ry+rh-S(2),g_theme.border,1);
                } else if(row.kind==2){
                    // §E: "… و N مورد دیگر" lazy-overflow summary row (caps reached).
                    SelectObject(dc,g_fSmall); SetTextColor(dc,g_theme.textDim);
                    RECT mr={p.left+S(16),ry+S(2),p.right-S(16),ry+rh-S(2)};
                    DrawTextW(dc,row.name.c_str(),-1,&mr,
                        DT_RIGHT|DT_VCENTER|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX|DT_END_ELLIPSIS);
                } else {
                    int u=row.idx; bool online=isUserOnline(s_ms->users[u].username);
                    bool sel=(s_ms->selUser==u), hot=(s_ms->empHot==i);
                    RECT rr={p.left+S(8),ry+S(2),p.right-S(8),ry+rh-S(2)};
                    if(sel) gpRoundRectBg(dc,rr,S(9),blendColor(g_theme.surface,g_theme.accent,18),g_theme.accent,g_theme.surface);
                    else if(hot) gpRoundRectBg(dc,rr,S(9),g_theme.surface2,g_theme.border,g_theme.surface);
                    RECT dot={rr.right-S(22),rr.top+S(13),rr.right-S(10),rr.top+S(25)};
                    gpRoundRect(dc,dot,S(6), online?RGB(0x2E,0xB8,0x6B):blendColor(g_theme.textDim,g_theme.surface,30),
                        CLR_INVALID,255);
                    SelectObject(dc,g_fUI); SetTextColor(dc, sel?g_theme.accent:(online?g_theme.text:g_theme.textDim));
                    // §E: employee code shown on the left of each row; name on the right.
                    if(!row.code.empty()){
                        // §G: personnel code in monospace (Consolas → Courier New).
                        SelectObject(dc,g_fCode);
                        SetTextColor(dc, sel?g_theme.accent:blendColor(g_theme.textDim,g_theme.surface,15));
                        RECT cr={rr.left+S(10),rr.top,rr.left+S(96),rr.bottom};
                        DrawTextW(dc,row.code.c_str(),-1,&cr,
                            DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX|DT_END_ELLIPSIS);
                        SelectObject(dc,g_fUI);
                        SetTextColor(dc, sel?g_theme.accent:(online?g_theme.text:g_theme.textDim));
                    }
                    RECT nr={rr.left+S(100),rr.top,rr.right-S(30),rr.bottom};
                    std::wstring nm=s_ms->users[u].fullname.empty()
                        ?s_ms->users[u].username:s_ms->users[u].fullname;
                    DrawTextW(dc,nm.c_str(),-1,&nr,
                        DT_RIGHT|DT_VCENTER|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX|DT_END_ELLIPSIS);
                }
            }
            SelectClipRgn(dc,NULL); DeleteObject(rgn);
        }
        BitBlt(dc0,0,0,rc.right,rc.bottom,dc,0,0,SRCCOPY);
        SelectObject(dc,obm); DeleteObject(bmp); DeleteDC(dc);
        EndPaint(h,&ps); return 0; }
    }
    return DefWindowProcW(h,m,w,l);
}
static void openMsgCompose(HWND owner){
    if(s_mm&&IsWindow(s_mm)) mmClose();
    static bool reg=false;
    if(!reg){ WNDCLASSW wc={0}; wc.lpfnWndProc=mmProc; wc.hInstance=g_hInst;
        wc.hCursor=LoadCursor(NULL,IDC_ARROW); wc.lpszClassName=MGMSG_CLASS;
        RegisterClassW(&wc); reg=true; }
    RECT rc; GetClientRect(g_hFrame,&rc);
    POINT org={0,0}; ClientToScreen(g_hFrame,&org);
    s_mm=CreateWindowExW(WS_EX_TOPMOST,MGMSG_CLASS,L"",WS_POPUP|WS_VISIBLE,
        org.x,org.y,rc.right,rc.bottom,g_hFrame,NULL,g_hInst,NULL);
    BringWindowToTop(s_mm); SetFocus(s_mm);
}

// ====================== new-employee form modal =============================
//  v1.8.0 full, polished, production-ready form. Auto-generates the personnel
//  code (empId) and a system-unique identifier (uniqueId) — both editable.
//  Collects: full name, username, password, national-id, father, position,
//  department, mobile, landline, email, shift from/to, weekly work hours,
//  hire-date, address. Creates the login User + EmpProfile in one step.
#define MGNEW_CLASS L"AzNewEmp"
enum {
    NE_E_FULL=3401, NE_E_USER, NE_E_PASS, NE_E_NID, NE_E_FATHER, NE_E_POS,
    NE_E_MOBILE, NE_E_LAND, NE_E_EMAIL, NE_E_SFROM, NE_E_STO, NE_E_HOURS,
    NE_E_HIRE, NE_E_ADDR, NE_E_EMPID, NE_E_UID, NE_CB_DEPT, NE_CB_ROLE,
    NE_B_OK, NE_B_CANCEL, NE_CB_AUTOID, NE_B_PHOTO, NE_B_IDCARD
};
struct NewEmpState {
    HWND eFull,eUser,ePass,eNid,eFather,ePos,eMobile,eLand,eEmail,
         eSfrom,eSto,eHours,eHire,eAddr,eEmpId,eUid,cDept,cRole,bOk,bCancel,bAutoId,
         bPhoto,bIdCard;
    std::vector<DeptCat> depts;
    bool autoId;
    std::wstring photoPath, idCardPath;   // chosen image files (copied on save)
};
static HWND s_ne=NULL; static NewEmpState* s_nes=NULL;
static void neClose(){ if(s_ne&&IsWindow(s_ne)){ HWND v=s_ne; s_ne=NULL; DestroyWindow(v);} }
static RECT neCard(HWND h){ RECT rc; GetClientRect(h,&rc);
    int w=S(760),hh=S(688);
    if(hh>rc.bottom-S(20)) hh=rc.bottom-S(20);
    RECT c={(rc.right-w)/2,(rc.bottom-hh)/2,(rc.right+w)/2,(rc.bottom+hh)/2}; return c; }
static std::wstring neGenEmpId(){
    auto users=loadUsers(); int n=(int)users.size()+1;
    wchar_t b[24]; swprintf(b,24,L"PRS%04d",n); return b;
}
static std::wstring neGenUid(){
    SYSTEMTIME st=iranNow();
    wchar_t b[40]; swprintf(b,40,L"UID%02d%02d%02d%02d%02d%02d",
        st.wYear%100,st.wMonth,st.wDay,st.wHour,st.wMinute,st.wSecond);
    return b;
}
//  Two-column form layout. Returns y after the last row.
static void neLayout(HWND h){
    if(!s_nes) return; RECT c=neCard(h);
    int top=c.top+S(64), fh=S(30), lh=S(24), gap=S(10);
    int colGap=S(24);
    int colW=(c.right-c.left-S(48)-colGap)/2;
    int rCol=c.right-S(24);                 // right column right edge
    int rColL=rCol-colW;
    int lColR=rColL-colGap;                  // left column right edge
    int lColL=lColR-colW;
    int yR=top, yL=top;
    auto row=[&](HWND ctl, int& y, int colRight, int colLeft, int fieldH){
        MoveWindow(ctl, colLeft, y+lh, colRight-colLeft, fieldH, TRUE);
        ShowWindow(ctl,SW_SHOW);
        y += lh + fieldH + gap;
    };
    // RIGHT column
    row(s_nes->eFull,  yR, rCol, rColL, fh);
    row(s_nes->eUser,  yR, rCol, rColL, fh);
    row(s_nes->ePass,  yR, rCol, rColL, fh);
    row(s_nes->eNid,   yR, rCol, rColL, fh);
    row(s_nes->eFather,yR, rCol, rColL, fh);
    row(s_nes->cDept,  yR, rCol, rColL, S(200));
    row(s_nes->cRole,  yR, rCol, rColL, S(160));
    row(s_nes->eMobile,yR, rCol, rColL, fh);
    // LEFT column
    row(s_nes->eEmpId, yL, lColR, lColL, fh);
    row(s_nes->eUid,   yL, lColR, lColL, fh);
    // auto-id checkbox sits right under the two id fields
    MoveWindow(s_nes->bAutoId, lColL, yL+S(2), lColR-lColL, S(26), TRUE);
    ShowWindow(s_nes->bAutoId,SW_SHOW); yL+=S(34);
    row(s_nes->ePos,   yL, lColR, lColL, fh);
    row(s_nes->eLand,  yL, lColR, lColL, fh);
    row(s_nes->eEmail, yL, lColR, lColL, fh);
    // shift from/to share a row (two halves)
    { int half=(lColR-lColL-S(8))/2;
      MoveWindow(s_nes->eSfrom, lColL+half+S(8), yL+lh, half, fh, TRUE);
      MoveWindow(s_nes->eSto,   lColL,           yL+lh, half, fh, TRUE);
      ShowWindow(s_nes->eSfrom,SW_SHOW); ShowWindow(s_nes->eSto,SW_SHOW);
      yL += lh+fh+gap; }
    row(s_nes->eHours, yL, lColR, lColL, fh);
    row(s_nes->eHire,  yL, lColR, lColL, fh);
    // image-upload buttons (national-id card + personal photo) share one full
    // width row just above the address field. They never overlap any label.
    int upY=c.bottom-S(156);
    { int half=(rCol-lColL-S(8))/2;
      MoveWindow(s_nes->bIdCard, lColL,            upY, half, S(34), TRUE);
      MoveWindow(s_nes->bPhoto,  lColL+half+S(8),  upY, half, S(34), TRUE);
      ShowWindow(s_nes->bIdCard,SW_SHOW); ShowWindow(s_nes->bPhoto,SW_SHOW); }
    // address spans full width near the bottom
    int addrY=c.bottom-S(108);
    MoveWindow(s_nes->eAddr, lColL, addrY, rCol-lColL, S(40), TRUE);
    ShowWindow(s_nes->eAddr,SW_SHOW);
    // buttons
    int by=c.bottom-S(52), bw=S(150);
    MoveWindow(s_nes->bOk,     rCol-bw,         by, bw, S(38), TRUE);
    MoveWindow(s_nes->bCancel, rCol-bw-S(8)-bw, by, bw, S(38), TRUE);
    // id fields visible only when manual
    ShowWindow(s_nes->eEmpId, s_nes->autoId?SW_HIDE:SW_SHOW);
    ShowWindow(s_nes->eUid,   s_nes->autoId?SW_HIDE:SW_SHOW);
}
static void neDoCreate(HWND h){
    if(!s_nes) return;
    wchar_t full[128]={0},user[64]={0},pass[64]={0},nid[32]={0},father[64]={0},
            pos[64]={0},mobile[32]={0},land[32]={0},email[96]={0},
            sfrom[16]={0},sto[16]={0},hours[16]={0},hire[24]={0},addr[256]={0},
            empId[32]={0},uid[48]={0};
    GetWindowTextW(s_nes->eFull,full,128);
    GetWindowTextW(s_nes->eUser,user,64);
    GetWindowTextW(s_nes->ePass,pass,64);
    GetWindowTextW(s_nes->eNid,nid,32);
    GetWindowTextW(s_nes->eFather,father,64);
    GetWindowTextW(s_nes->ePos,pos,64);
    GetWindowTextW(s_nes->eMobile,mobile,32);
    GetWindowTextW(s_nes->eLand,land,32);
    GetWindowTextW(s_nes->eEmail,email,96);
    GetWindowTextW(s_nes->eSfrom,sfrom,16);
    GetWindowTextW(s_nes->eSto,sto,16);
    GetWindowTextW(s_nes->eHours,hours,16);
    GetWindowTextW(s_nes->eHire,hire,24);
    GetWindowTextW(s_nes->eAddr,addr,256);
    GetWindowTextW(s_nes->eEmpId,empId,32);
    GetWindowTextW(s_nes->eUid,uid,48);
    std::wstring un=trim(user), fn=trim(full), pw=trim(pass);
    if(fn.empty()){ MessageBoxW(h,L"نام و نام خانوادگی الزامی است.",L"افزودن کارمند",MB_OK|MB_ICONWARNING); return; }
    if(un.empty()){ MessageBoxW(h,L"نام کاربری الزامی است.",L"افزودن کارمند",MB_OK|MB_ICONWARNING); return; }
    if(pw.empty()) pw=L"1234";
    User u; u.username=un; u.fullname=fn; u.hash=hashPassword(pw);
    int deptSel=(int)SendMessageW(s_nes->cDept,CB_GETCURSEL,0,0);
    std::wstring deptId, deptName;
    if(deptSel>=0 && deptSel<(int)s_nes->depts.size()){
        deptId=s_nes->depts[deptSel].id; deptName=s_nes->depts[deptSel].name; }
    u.dept=deptName;
    int roleSel=(int)SendMessageW(s_nes->cRole,CB_GETCURSEL,0,0);
    u.role=(roleSel==1)?1:0;
    std::wstring err;
    if(!addUser(u,err)){ MessageBoxW(h,err.c_str(),L"افزودن کارمند",MB_OK|MB_ICONERROR); return; }
    EmpProfile p; p.username=un; p.deptId=deptId;
    p.nationalId=trim(nid); p.fatherName=trim(father); p.position=trim(pos);
    p.mobile=trim(mobile); p.landline=trim(land); p.email=trim(email);
    p.shiftFrom=trim(sfrom); p.shiftTo=trim(sto); p.workHours=trim(hours);
    p.hireDate=trim(hire); p.address=trim(addr);
    p.empId   = s_nes->autoId? neGenEmpId() : trim(empId);
    p.uniqueId= s_nes->autoId? neGenUid()   : trim(uid);
    p.photoPath=s_nes->photoPath; p.idCardPath=s_nes->idCardPath;
    saveEmpProfile(p);
    if(s_mg){ s_mg->msg=L"کارمند «"+fn+L"» با کد پرسنلی "+toFaDigits(p.empId)+L" ساخته شد.";
        s_mg->msgCol=g_theme.success; mgLoadEmps(); }
    neClose();
    if(g_hManage) InvalidateRect(g_hManage,NULL,TRUE);
}
static LRESULT CALLBACK neProc(HWND h, UINT m, WPARAM w, LPARAM l){
    switch(m){
    case WM_CREATE:{
        s_nes=new NewEmpState(); s_nes->autoId=true;
        DWORD es=WS_CHILD|WS_TABSTOP|ES_AUTOHSCROLL;
        auto mk=[&](int id,DWORD extra)->HWND{
            HWND e=CreateWindowExW(0,L"EDIT",L"",es|extra,0,0,10,10,h,(HMENU)(UINT_PTR)id,g_hInst,0);
            SendMessageW(e,WM_SETFONT,(WPARAM)g_fUI,TRUE); return e; };
        s_nes->eFull=mk(NE_E_FULL,0); s_nes->eUser=mk(NE_E_USER,0);
        s_nes->ePass=mk(NE_E_PASS,ES_PASSWORD); s_nes->eNid=mk(NE_E_NID,0);
        s_nes->eFather=mk(NE_E_FATHER,0); s_nes->ePos=mk(NE_E_POS,0);
        s_nes->eMobile=mk(NE_E_MOBILE,0); s_nes->eLand=mk(NE_E_LAND,0);
        s_nes->eEmail=mk(NE_E_EMAIL,0); s_nes->eSfrom=mk(NE_E_SFROM,0);
        s_nes->eSto=mk(NE_E_STO,0); s_nes->eHours=mk(NE_E_HOURS,0);
        s_nes->eHire=mk(NE_E_HIRE,0); s_nes->eAddr=mk(NE_E_ADDR,0);
        s_nes->eEmpId=mk(NE_E_EMPID,0); s_nes->eUid=mk(NE_E_UID,0);
        SendMessageW(s_nes->eSfrom,EM_SETCUEBANNER,TRUE,(LPARAM)L"از (۰۸:۰۰)");
        SendMessageW(s_nes->eSto,EM_SETCUEBANNER,TRUE,(LPARAM)L"تا (۱۶:۰۰)");
        SendMessageW(s_nes->eHours,EM_SETCUEBANNER,TRUE,(LPARAM)L"ساعت/هفته");
        SendMessageW(s_nes->eHire,EM_SETCUEBANNER,TRUE,(LPARAM)L"۱۴۰۴/۰۳/۲۸");
        s_nes->cDept=CreateWindowExW(0,L"COMBOBOX",L"",
            WS_CHILD|WS_VISIBLE|WS_TABSTOP|CBS_DROPDOWNLIST|WS_VSCROLL,
            0,0,10,10,h,(HMENU)NE_CB_DEPT,g_hInst,0);
        s_nes->cRole=CreateWindowExW(0,L"COMBOBOX",L"",
            WS_CHILD|WS_VISIBLE|WS_TABSTOP|CBS_DROPDOWNLIST,
            0,0,10,10,h,(HMENU)NE_CB_ROLE,g_hInst,0);
        SendMessageW(s_nes->cDept,WM_SETFONT,(WPARAM)g_fUI,TRUE);
        SendMessageW(s_nes->cRole,WM_SETFONT,(WPARAM)g_fUI,TRUE);
        s_nes->depts=loadDepts();
        for(auto&d:s_nes->depts) SendMessageW(s_nes->cDept,CB_ADDSTRING,0,(LPARAM)d.name.c_str());
        // preselect current section if any
        if(s_mg && !s_mg->curCatName.empty()){
            for(int i=0;i<(int)s_nes->depts.size();i++)
                if(s_nes->depts[i].name==s_mg->curCatName){ SendMessageW(s_nes->cDept,CB_SETCURSEL,i,0); break; }
        }
        else if(!s_nes->depts.empty()) SendMessageW(s_nes->cDept,CB_SETCURSEL,0,0);
        SendMessageW(s_nes->cRole,CB_ADDSTRING,0,(LPARAM)L"پذیرش");
        SendMessageW(s_nes->cRole,CB_ADDSTRING,0,(LPARAM)L"مدیریت");
        SendMessageW(s_nes->cRole,CB_SETCURSEL,0,0);
        s_nes->bAutoId=CreateWindowExW(0,L"BUTTON",L"شناسه‌ها به‌صورت خودکار",
            WS_CHILD|BS_AUTOCHECKBOX|BS_RIGHTBUTTON|BS_RIGHT,0,0,10,10,h,(HMENU)NE_CB_AUTOID,g_hInst,0);
        SendMessageW(s_nes->bAutoId,WM_SETFONT,(WPARAM)g_fUI,TRUE);
        SendMessageW(s_nes->bAutoId,BM_SETCHECK,BST_CHECKED,0);
        s_nes->bIdCard=createFlatButton(h,NE_B_IDCARD,L"بارگذاری تصویر کارت ملی",ICO_ID,BS_OUTLINE,0,0,10,10);
        s_nes->bPhoto =createFlatButton(h,NE_B_PHOTO, L"بارگذاری عکس پرسنلی",ICO_USER,BS_OUTLINE,0,0,10,10);
        s_nes->bOk=createFlatButton(h,NE_B_OK,L"ثبت کارمند",ICO_PLUS,BS_PRIMARY,0,0,10,10);
        s_nes->bCancel=createFlatButton(h,NE_B_CANCEL,L"انصراف",ICO_X,BS_OUTLINE,0,0,10,10);
        neLayout(h);
        return 0; }
    case WM_SIZE: neLayout(h); return 0;
    case WM_NCDESTROY: if(s_nes){ delete s_nes; s_nes=NULL; } s_ne=NULL;
        if(g_hFrame) InvalidateRect(g_hFrame,NULL,TRUE); return 0;
    case WM_ERASEBKGND: return 1;
    case WM_CTLCOLOREDIT: case WM_CTLCOLORLISTBOX:{ HDC dc=(HDC)w;
        SetTextColor(dc,g_theme.inputText); SetBkColor(dc,g_theme.inputBg);
        return (LRESULT)g_brInput; }
    case WM_CTLCOLORSTATIC:{ HDC dc=(HDC)w;
        SetTextColor(dc,g_theme.text); SetBkColor(dc,g_theme.surface);
        return (LRESULT)g_brSurface; }
    case WM_COMMAND:{ int id=LOWORD(w);
        if(id==NE_B_OK) neDoCreate(h);
        else if(id==NE_B_CANCEL) neClose();
        else if(id==NE_CB_AUTOID){ s_nes->autoId=!s_nes->autoId; neLayout(h);
            InvalidateRect(h,NULL,FALSE); }
        else if(id==NE_B_PHOTO || id==NE_B_IDCARD){
            wchar_t fn[MAX_PATH]={0};
            OPENFILENAMEW ofn={sizeof(ofn)}; ofn.hwndOwner=h;
            ofn.lpstrFilter=L"تصویر\0*.png;*.jpg;*.jpeg;*.bmp;*.gif\0همه\0*.*\0";
            ofn.lpstrFile=fn; ofn.nMaxFile=MAX_PATH;
            ofn.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST;
            if(GetOpenFileNameW(&ofn)){
                std::wstring stored=copyAttachmentLocal(fn);
                if(stored.empty()) stored=fn;
                if(id==NE_B_PHOTO){ s_nes->photoPath=stored;
                    SetWindowTextW(s_nes->bPhoto,L"عکس پرسنلی ✓"); }
                else { s_nes->idCardPath=stored;
                    SetWindowTextW(s_nes->bIdCard,L"تصویر کارت ملی ✓"); }
            }
        }
        return 0; }
    case WM_KEYDOWN: if(w==VK_ESCAPE){ neClose(); return 0; } break;
    case WM_PAINT:{ PAINTSTRUCT ps; HDC dc0=BeginPaint(h,&ps);
        RECT rc; GetClientRect(h,&rc);
        HDC dc=CreateCompatibleDC(dc0);
        HBITMAP bmp=CreateCompatibleBitmap(dc0,rc.right,rc.bottom);
        HGDIOBJ obm=SelectObject(dc,bmp);
        { HBRUSH sb=CreateSolidBrush(g_dark?RGB(6,9,14):RGB(28,36,48));
          FillRect(dc,&rc,sb); DeleteObject(sb); }
        gpFillAlpha(dc,rc,0,g_dark?RGB(0,0,0):RGB(20,28,40),120);
        RECT c=neCard(h);
        gpShadow(dc,c,S(20),S(22),80);
        gpGradRoundRectBg(dc,c,S(18),g_theme.surfaceTop,g_theme.surface,g_theme.border,
            g_dark?RGB(6,9,14):RGB(28,36,48));
        SetBkMode(dc,TRANSPARENT);
        { RECT cb={c.left+S(14),c.top+S(14),c.left+S(40),c.top+S(40)};
          RECT ci={cb.left+S(5),cb.top+S(5),cb.right-S(5),cb.bottom-S(5)};
          drawIcon(dc,ICO_X,ci,g_theme.text,S(2)); }
        SelectObject(dc,g_fTitle); SetTextColor(dc,g_theme.accent);
        RECT tr={c.left+S(28),c.top+S(16),c.right-S(54),c.top+S(46)};
        DrawTextW(dc,L"افزودن کارمند جدید",-1,&tr,
            DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
        // field labels (drawn just above each control)
        SelectObject(dc,g_fSmall); SetTextColor(dc,g_theme.textDim);
        auto lbl=[&](HWND ctl,const wchar_t* t){
            RECT wr; GetWindowRect(ctl,&wr);
            POINT p1={wr.left,wr.top}; ScreenToClient(h,&p1);
            int wcw=wr.right-wr.left;
            RECT lr={p1.x,p1.y-S(20),p1.x+wcw,p1.y-S(2)};
            DrawTextW(dc,t,-1,&lr,DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
        };
        if(s_nes){
            lbl(s_nes->eFull,L"نام و نام خانوادگی *");
            lbl(s_nes->eUser,L"نام کاربری *");
            lbl(s_nes->ePass,L"رمز عبور (پیش‌فرض ۱۲۳۴)");
            lbl(s_nes->eNid,L"کد ملی");
            lbl(s_nes->eFather,L"نام پدر");
            lbl(s_nes->cDept,L"بخش / دپارتمان");
            lbl(s_nes->cRole,L"نوع دسترسی");
            lbl(s_nes->eMobile,L"موبایل");
            if(!s_nes->autoId){ lbl(s_nes->eEmpId,L"کد پرسنلی"); lbl(s_nes->eUid,L"شناسه یکتا"); }
            lbl(s_nes->ePos,L"سمت / عنوان شغلی");
            lbl(s_nes->eLand,L"تلفن ثابت");
            lbl(s_nes->eEmail,L"ایمیل");
            lbl(s_nes->eSto,L"شیفت — از / تا");
            lbl(s_nes->eHours,L"ساعات کاری هفتگی");
            lbl(s_nes->eHire,L"تاریخ استخدام");
            lbl(s_nes->eAddr,L"آدرس");
        }
        BitBlt(dc0,0,0,rc.right,rc.bottom,dc,0,0,SRCCOPY);
        SelectObject(dc,obm); DeleteObject(bmp); DeleteDC(dc);
        EndPaint(h,&ps); return 0; }
    }
    return DefWindowProcW(h,m,w,l);
}
static void openNewEmployee(HWND owner){
    if(s_ne&&IsWindow(s_ne)) neClose();
    static bool reg=false;
    if(!reg){ WNDCLASSW wc={0}; wc.lpfnWndProc=neProc; wc.hInstance=g_hInst;
        wc.hCursor=LoadCursor(NULL,IDC_ARROW); wc.lpszClassName=MGNEW_CLASS;
        RegisterClassW(&wc); reg=true; }
    RECT rc; GetClientRect(g_hFrame,&rc);
    POINT org={0,0}; ClientToScreen(g_hFrame,&org);
    s_ne=CreateWindowExW(WS_EX_TOPMOST,MGNEW_CLASS,L"",WS_POPUP|WS_VISIBLE,
        org.x,org.y,rc.right,rc.bottom,g_hFrame,NULL,g_hInst,NULL);
    BringWindowToTop(s_ne); SetFocus(s_ne);
}

// ============================================================================
//  DASHBOARD shell — left nav rail + content pages
// ============================================================================
static int  mgNavW(){ return S(220); }     // sidebar width
static int  mgHeadH(){ return S(64); }      // content header height
static const wchar_t* MG_NAV_LABELS[PG_COUNT] = {
    L"داشبورد", L"بیماران", L"بخش‌ها", L"کارمندان", L"پیام به کارمندان",
    L"درخواست‌ها", L"پیام‌های ذخیره‌شده", L"پشتیبان‌گیری"
};
static int MG_NAV_ICONS[PG_COUNT] = {
    ICO_TAB, ICO_ID, ICO_SHIELD, ICO_USER, ICO_BELL, ICO_GEAR, ICO_SAVE, ICO_REFRESH
};
//  content area to the LEFT of the RTL nav rail (rail is on the RIGHT)
static RECT mgContentRect(HWND h){
    RECT rc; GetClientRect(h,&rc);
    RECT r={rc.left+S(8), rc.top+S(8), rc.right-mgNavW()-S(8), rc.bottom-S(8)};
    return r;
}
// The compose block at the top of the saved-messages page:
//   caption line  +  editor (multi-line)  +  hint line.
// A single source of truth so the editor, its buttons, the hint and the note
// list below them never overlap. Declared early so mgLayout() can use them.
static int mgSavedComposeTop(HWND h){ RECT a=mgContentRect(h); return a.top+mgHeadH()+S(12); }
static int mgSavedCaptionH(){ return S(24); }
static int mgSavedEditorH(){ return S(74); }
static int mgSavedHintH(){ return S(22); }
static int mgSavedComposeBottom(HWND h){
    return mgSavedComposeTop(h)+mgSavedCaptionH()+mgSavedEditorH()+S(8)+mgSavedHintH();
}
static RECT mgNavItemRect(HWND h, int i){
    RECT rc; GetClientRect(h,&rc);
    int x=rc.right-mgNavW()+S(12), w=mgNavW()-S(24);
    int y=rc.top+S(96)+i*(S(48)+S(6));
    RECT r={x,y,x+w,y+S(48)}; return r;
}

// content geometry within mgContentRect ------------------------------------
static int mgCols(int W){ int c=(W-S(24))/S(248); return c<1?1:c; }
static RECT mgSecCardRect(HWND h, int idx){
    RECT a=mgContentRect(h);
    int innerW=a.right-a.left-S(24);
    int cols=innerW/S(248); if(cols<1) cols=1;
    int cw=S(232), ch=S(124), gap=S(16);
    int row=idx/cols, col=idx%cols;
    int x=a.right-S(12)-(col+1)*cw-col*gap;     // RTL: first on the right
    int y=a.top+mgHeadH()+S(58)+row*(ch+gap);
    RECT r={x,y,x+cw,y+ch}; return r;
}
static int mgEmpRowH(){ return S(58); }
static RECT mgEmpRowRect(HWND h, int idx){
    RECT a=mgContentRect(h);
    int y=a.top+mgHeadH()+S(58)+idx*(mgEmpRowH()+S(8));
    RECT r={a.left+S(12),y,a.right-S(12),y+mgEmpRowH()}; return r;
}
// dashboard quick-action tiles + summary cards
static RECT mgSummaryRect(HWND h, int i){      // i=0..3 across the top
    RECT a=mgContentRect(h);
    int n=4, gap=S(14), x0=a.left+S(12);
    int totW=a.right-a.left-S(24);
    int cw=(totW-gap*(n-1))/n;
    int x=a.right-S(12)-(i+1)*cw-i*gap;        // RTL
    int y=a.top+mgHeadH()+S(16);
    RECT r={x,y,x+cw,y+S(96)}; return r;
}
static RECT mgQuickRect(HWND h, int i){        // i=0..3 quick-action buttons
    RECT a=mgContentRect(h);
    int n=4, gap=S(14);
    int totW=a.right-a.left-S(24);
    int cw=(totW-gap*(n-1))/n;
    int x=a.right-S(12)-(i+1)*cw-i*gap;
    int y=a.top+mgHeadH()+S(150);
    RECT r={x,y,x+cw,y+S(110)}; return r;
}

// ---- layout: show/hide controls per page -----------------------------------
static void mgLayout(HWND h){
    if(!s_mg) return;
    RECT a=mgContentRect(h);
    int top=a.top+mgHeadH()+S(12);
    int rEdge=a.right-S(12), fh=S(34);
    // hide everything first
    HWND all[]={s_mg->eName,s_mg->eMgr,s_mg->eId,s_mg->bAdd,s_mg->bAutoId,
        s_mg->bSecNew,s_mg->bSecBack,s_mg->bAbc,s_mg->bNew,s_mg->bCreated,
        s_mg->bAddEmp,s_mg->cDept,s_mg->bMsg,s_mg->bReqs,s_mg->bProfReqs,
        s_mg->eNote,s_mg->bNoteAttach,s_mg->bNoteSend,
        s_mg->ptSearch,s_mg->ptList};
    for(HWND w:all) if(w) ShowWindow(w,SW_HIDE);

    if(s_mg->page==PG_PATIENTS){
        // search box (full width, right-aligned RTL) then the list fills the rest
        int y=top;
        MoveWindow(s_mg->ptSearch, a.left+S(12), y, (a.right-a.left)-S(24), fh, TRUE);
        int ly=y+fh+S(10);
        MoveWindow(s_mg->ptList, a.left+S(12), ly,
                   (a.right-a.left)-S(24), a.bottom-ly-S(8), TRUE);
        ShowWindow(s_mg->ptSearch,SW_SHOW);
        ShowWindow(s_mg->ptList,SW_SHOW);
    } else if(s_mg->page==PG_SECTS){
        if(s_mg->secAddMode){
            // «ایجاد بخش» add form: name • manager • id • auto-checkbox • add
            int y=top, lh=S(22);
            int w1=S(240);
            MoveWindow(s_mg->eName, rEdge-w1, y+lh, w1, fh, TRUE);
            MoveWindow(s_mg->eMgr,  rEdge-w1, y+lh+fh+S(20)+lh, w1, fh, TRUE);
            int y3=y+lh+fh+S(20)+lh+fh+S(20);
            MoveWindow(s_mg->bAutoId, rEdge-w1, y3, w1, S(28), TRUE);
            MoveWindow(s_mg->eId,    rEdge-w1, y3+S(34)+lh, w1, fh, TRUE);
            MoveWindow(s_mg->bAdd,   rEdge-w1, y3+S(34)+lh+fh+S(20), S(160), S(40), TRUE);
            MoveWindow(s_mg->bSecBack, a.left+S(12), top, S(120), S(36), TRUE);
            ShowWindow(s_mg->eName,SW_SHOW); ShowWindow(s_mg->eMgr,SW_SHOW);
            ShowWindow(s_mg->bAutoId,SW_SHOW); ShowWindow(s_mg->bAdd,SW_SHOW);
            ShowWindow(s_mg->bSecBack,SW_SHOW);
            ShowWindow(s_mg->eId, s_mg->autoId?SW_HIDE:SW_SHOW);
        } else {
            // sections grid + «ایجاد بخش» button (top-left)
            MoveWindow(s_mg->bSecNew, a.left+S(12), top, S(160), S(38), TRUE);
            ShowWindow(s_mg->bSecNew,SW_SHOW);
        }
    } else if(s_mg->page==PG_EMPS){
        // filter row: sort buttons (right) + dept combo (right of them) + add (left)
        int y=top;
        int x=rEdge;
        MoveWindow(s_mg->bAbc,     x-S(110), y, S(110), fh, TRUE); x-=S(110)+S(8);
        MoveWindow(s_mg->bNew,     x-S(120), y, S(120), fh, TRUE); x-=S(120)+S(8);
        MoveWindow(s_mg->bCreated, x-S(150), y, S(150), fh, TRUE); x-=S(150)+S(14);
        MoveWindow(s_mg->cDept,    x-S(190), y, S(190), S(220), TRUE);
        MoveWindow(s_mg->bAddEmp,  a.left+S(12), y, S(170), fh, TRUE);
        ShowWindow(s_mg->bAbc,SW_SHOW); ShowWindow(s_mg->bNew,SW_SHOW);
        ShowWindow(s_mg->bCreated,SW_SHOW); ShowWindow(s_mg->cDept,SW_SHOW);
        ShowWindow(s_mg->bAddEmp,SW_SHOW);
    } else if(s_mg->page==PG_MSG){
        // a big «نوشتن پیام» button centered (the compose modal does the work)
        MoveWindow(s_mg->bMsg, a.left+(a.right-a.left)/2-S(110), top+S(30), S(220), S(44), TRUE);
        ShowWindow(s_mg->bMsg,SW_SHOW);
    } else if(s_mg->page==PG_REQS){
        // notification banner occupies [top .. top+60], separator at top+76 —
        // the two action buttons sit BELOW that, never over the banner/title.
        int y=top+S(60)+S(16)+S(16);   // banner + gap + separator gap
        int bw=S(300);
        MoveWindow(s_mg->bReqs,     rEdge-bw, y,        bw, S(46), TRUE);
        MoveWindow(s_mg->bProfReqs, rEdge-bw, y+S(58),  bw, S(46), TRUE);
        ShowWindow(s_mg->bReqs,SW_SHOW); ShowWindow(s_mg->bProfReqs,SW_SHOW);
    } else if(s_mg->page==PG_SAVED){
        // compose block at the top:  caption (painted)  ▸  multiline editor on
        // the right, the two action buttons stacked at its left, all sharing one
        // height so nothing overlaps and the note list starts cleanly below.
        int edTop = mgSavedComposeTop(h)+mgSavedCaptionH();
        int edH   = mgSavedEditorH();
        int bw=S(110), gap=S(10);
        int editR=rEdge, editL=a.left+S(12)+bw+gap;
        MoveWindow(s_mg->eNote, editL, edTop, editR-editL, edH, TRUE);
        // two buttons stacked to the LEFT of the editor, vertically centred
        int bh=S(34), bgap=S(6);
        int btnsTop = edTop + (edH-(bh*2+bgap))/2;
        MoveWindow(s_mg->bNoteSend,   a.left+S(12), btnsTop,            bw, bh, TRUE);
        MoveWindow(s_mg->bNoteAttach, a.left+S(12), btnsTop+bh+bgap,    bw, bh, TRUE);
        ShowWindow(s_mg->eNote,SW_SHOW);
        ShowWindow(s_mg->bNoteAttach,SW_SHOW);
        ShowWindow(s_mg->bNoteSend,SW_SHOW);
    }
}

// ============================================================================
//  helpers: auto-id preview, add-section, card titles
// ============================================================================
static void mgUpdateAutoIdPreview(){
    if(!s_mg) return;
    wchar_t b[24]; swprintf(b,24,L"DEP%03d",(int)s_mg->cats.size()+1);
    s_mg->autoIdPreview=b;
}
static void mgDoAddCat(HWND h){
    if(!s_mg) return;
    wchar_t nm[128]={0},mg[128]={0},id[64]={0};
    GetWindowTextW(s_mg->eName,nm,128);
    GetWindowTextW(s_mg->eMgr,mg,128);
    GetWindowTextW(s_mg->eId,id,64);
    DeptCat c; c.name=trim(nm); c.manager=trim(mg);
    c.id = s_mg->autoId? L"" : trim(id);
    c.icon=L"shield";
    std::wstring err;
    if(addDept(c,err)){
        s_mg->msg=L"بخش «"+c.name+L"» با موفقیت اضافه شد."; s_mg->msgCol=g_theme.success;
        SetWindowTextW(s_mg->eName,L""); SetWindowTextW(s_mg->eMgr,L"");
        SetWindowTextW(s_mg->eId,L"");
        mgLoadCats(); mgUpdateAutoIdPreview();
        s_mg->secAddMode=false; mgLayout(h);
    } else { s_mg->msg=err; s_mg->msgCol=g_theme.danger; }
    InvalidateRect(h,NULL,FALSE);
}
static const wchar_t* mgCardTitle(int page){
    switch(page){
        case PG_DASH:  return L"داشبورد مدیریت";
        case PG_PATIENTS: return L"مدیریت بیماران";
        case PG_SECTS: return L"مدیریت بخش‌ها";
        case PG_EMPS:  return L"فهرست کارمندان";
        case PG_MSG:   return L"پیام به کارمندان";
        case PG_REQS:  return L"درخواست‌های تغییر";
        case PG_SAVED: return L"پیام‌های ذخیره‌شده";
        case PG_BACKUP:return L"پشتیبان‌گیری و بازیابی";
    }
    return L"";
}

// ---- small drawing util: pill chip -----------------------------------------
static void mgChip(HDC dc, RECT r, const wchar_t* txt, COLORREF fill, COLORREF tx){
    gpRoundRectBg(dc,r,(r.bottom-r.top)/2,fill,fill,g_theme.surface);
    SetTextColor(dc,tx); SelectObject(dc,g_fSmall);
    DrawTextW(dc,txt,-1,&r,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);
}

// ============================================================================
//  PAGE PAINTERS
// ============================================================================
static void mgPaintDash(HDC dc, HWND h){
    RECT a=mgContentRect(h);
    int nSec=(int)loadDepts().size();
    auto users=loadUsers();
    int nEmp=(int)users.size(), nOn=0;
    for(auto&u:users) if(isUserOnline(u.username)) nOn++;
    int nReq=unseenSetReqCount()+unseenProfReqCount();
    struct SumCard { const wchar_t* lbl; std::wstring val; COLORREF col; int ico; };
    SumCard cards[4]={
        { L"بخش‌ها",          toFaDigits(std::to_wstring(nSec)), g_infoAccent,    ICO_SHIELD },
        { L"کارمندان",         toFaDigits(std::to_wstring(nEmp)), g_theme.accent,  ICO_USER },
        { L"آنلاین",          toFaDigits(std::to_wstring(nOn)),  g_theme.success, ICO_BELL },
        { L"درخواست‌های نو",  toFaDigits(std::to_wstring(nReq)), nReq>0?g_infoAccent2:g_theme.textDim, ICO_GEAR },
    };
    for(int i=0;i<4;i++){
        RECT r=mgSummaryRect(h,i);
        gpRoundRectBg(dc,r,S(14),g_theme.surface,g_theme.border,g_theme.bg);
        RECT ir={r.right-S(56),r.top+S(16),r.right-S(20),r.top+S(52)};
        drawIcon(dc,cards[i].ico,ir,cards[i].col,S(2));
        SetTextColor(dc,g_theme.text); SelectObject(dc,g_fHuge);
        RECT vr={r.left+S(16),r.top+S(12),r.right-S(60),r.top+S(58)};
        DrawTextW(dc,cards[i].val.c_str(),-1,&vr,DT_RIGHT|DT_VCENTER|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);
        SetTextColor(dc,g_theme.textDim); SelectObject(dc,g_fSmall);
        RECT lr={r.left+S(16),r.bottom-S(30),r.right-S(16),r.bottom-S(8)};
        DrawTextW(dc,cards[i].lbl,-1,&lr,DT_RIGHT|DT_VCENTER|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);
    }
    // quick actions title
    SetTextColor(dc,g_theme.text); SelectObject(dc,g_fUIB);
    { RECT qr={a.left+S(12),a.top+mgHeadH()+S(124),a.right-S(12),a.top+mgHeadH()+S(148)};
      DrawTextW(dc,L"دسترسی سریع",-1,&qr,DT_RIGHT|DT_VCENTER|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX); }
    struct QA { const wchar_t* lbl; const wchar_t* sub; int ico; int page; };
    QA qa[4]={
        { L"ایجاد بخش",       L"افزودن بخش جدید",        ICO_PLUS,  PG_SECTS },
        { L"افزودن کارمند",   L"ثبت کارمند جدید",        ICO_USER,  PG_EMPS  },
        { L"پیام به کارمندان", L"ارسال پیام و رسانه",     ICO_BELL,  PG_MSG   },
        { L"درخواست‌ها",      L"بررسی درخواست‌ها",       ICO_GEAR,  PG_REQS  },
    };
    for(int i=0;i<4;i++){
        RECT r=mgQuickRect(h,i);
        bool hot=(s_mg&&s_mg->hot==MG_QA_BASE+i);
        gpRoundRectBg(dc,r,S(14), hot?g_theme.surface2:g_theme.surface,
                      hot?g_infoAccent:g_theme.border, g_theme.bg);
        RECT ir={r.left+(r.right-r.left)/2-S(18),r.top+S(18),r.left+(r.right-r.left)/2+S(18),r.top+S(54)};
        drawIcon(dc,qa[i].ico,ir,hot?g_infoAccent:g_theme.accent,S(2));
        SetTextColor(dc,g_theme.text); SelectObject(dc,g_fUIB);
        RECT lr={r.left+S(8),r.top+S(60),r.right-S(8),r.top+S(84)};
        DrawTextW(dc,qa[i].lbl,-1,&lr,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);
        SetTextColor(dc,g_theme.textDim); SelectObject(dc,g_fSmall);
        RECT sr={r.left+S(8),r.top+S(84),r.right-S(8),r.bottom-S(8)};
        DrawTextW(dc,qa[i].sub,-1,&sr,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);
    }

    // ------------------------------------------------------------------------
    // v1.12.0 (§5): CRM-style "at-a-glance" activity panel. A read-only,
    // additive surface — it never changes data, only summarises it: total
    // patient roster, appointments registered today, and a tiny workload bar
    // (today vs. the day's busiest historical count). All sourced from the
    // existing file-backed stores so nothing else changes.
    {
        int panelTop = a.top+mgHeadH()+S(150)+S(110)+S(20);   // below quick tiles
        int panelBot = a.bottom-S(12);
        if(panelBot - panelTop >= S(120)){
            RECT pr={a.left+S(12),panelTop,a.right-S(12),panelBot};
            gpRoundRectBg(dc,pr,S(14),g_theme.surface,g_theme.border,g_theme.bg);
            // title
            SetTextColor(dc,g_theme.text); SelectObject(dc,g_fUIB);
            RECT tr={pr.left+S(16),pr.top+S(12),pr.right-S(16),pr.top+S(38)};
            DrawTextW(dc,L"یک نگاه به فعالیت کلینیک",-1,&tr,
                DT_RIGHT|DT_VCENTER|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);
            // data
            int nPat=(int)loadAllPatients().size();
            auto appts=loadAppointments(false);
            SYSTEMTIME nowst=iranNow();
            std::wstring today=jalaliDateShort(nowst);
            int nToday=0; int busiest=0;
            { // tally per-day counts without <map>: collect distinct dates +
              // counts in parallel vectors (appointment volume is small).
              std::vector<std::wstring> days; std::vector<int> cnt;
              for(auto&ap:appts){
                  if(ap.cancelled) continue;
                  size_t k=0; for(;k<days.size();k++) if(days[k]==ap.apptDate) break;
                  if(k==days.size()){ days.push_back(ap.apptDate); cnt.push_back(0); }
                  cnt[k]++;
              }
              for(size_t k=0;k<days.size();k++){
                  if(cnt[k]>busiest) busiest=cnt[k];
                  if(days[k]==today) nToday=cnt[k];
              }
            }
            // two read-only metric chips (patients / today) — RTL, right→left
            struct AC{ const wchar_t* lbl; std::wstring val; COLORREF col; int ico; };
            AC ac[2]={
                { L"کل بیماران ثبت‌شده", toFaDigits(std::to_wstring(nPat)),   g_theme.accent, ICO_USER },
                { L"نوبت‌های امروز",     toFaDigits(std::to_wstring(nToday)), g_infoAccent,   ICO_BELL },
            };
            int cy=pr.top+S(48);
            int cellW=(pr.right-pr.left-S(32))/2;
            for(int i=0;i<2;i++){
                int cx=pr.right-S(16)-(i+1)*cellW;          // RTL
                RECT cr={cx,cy,cx+cellW-S(8),cy+S(56)};
                RECT ico={cr.right-S(44),cr.top+S(8),cr.right-S(14),cr.top+S(38)};
                drawIcon(dc,ac[i].ico,ico,ac[i].col,S(2));
                SetTextColor(dc,g_theme.text); SelectObject(dc,g_fTitle);
                RECT vr={cr.left+S(6),cr.top+S(4),cr.right-S(48),cr.top+S(36)};
                DrawTextW(dc,ac[i].val.c_str(),-1,&vr,DT_RIGHT|DT_VCENTER|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);
                SetTextColor(dc,g_theme.textDim); SelectObject(dc,g_fSmall);
                RECT lr={cr.left+S(6),cr.bottom-S(20),cr.right-S(6),cr.bottom};
                DrawTextW(dc,ac[i].lbl,-1,&lr,DT_RIGHT|DT_VCENTER|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);
            }
            // workload bar: today vs busiest day on record
            int barTop=cy+S(64);
            if(barTop+S(24) <= pr.bottom-S(10)){
                SetTextColor(dc,g_theme.textDim); SelectObject(dc,g_fSmall);
                RECT wl={pr.left+S(16),barTop,pr.right-S(16),barTop+S(18)};
                DrawTextW(dc,L"حجم کاری امروز نسبت به شلوغ‌ترین روز ثبت‌شده",-1,&wl,
                    DT_RIGHT|DT_VCENTER|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);
                RECT track={pr.left+S(16),barTop+S(20),pr.right-S(16),barTop+S(30)};
                gpRoundRectBg(dc,track,S(5),g_theme.surface2,g_theme.border,g_theme.surface);
                int den=busiest>0?busiest:1;
                double frac=(double)nToday/(double)den; if(frac>1.0)frac=1.0; if(frac<0)frac=0;
                int fillW=(int)((track.right-track.left)*frac);
                if(fillW>0){
                    RECT fillr={track.right-fillW,track.top,track.right,track.bottom}; // RTL fill
                    gpRoundRectBg(dc,fillr,S(5),g_infoAccent,g_infoAccent,g_theme.surface2);
                }
            }
        }
    }
}

static void mgPaintSects(HDC dc, HWND h){
    RECT a=mgContentRect(h);
    if(s_mg->secAddMode){
        int top=a.top+mgHeadH()+S(12), rEdge=a.right-S(12), lh=S(22), fh=S(34), w1=S(240);
        SetTextColor(dc,g_theme.text); SelectObject(dc,g_fUIB);
        RECT t1={rEdge-w1,top,rEdge,top+lh};
        DrawTextW(dc,L"نام بخش",-1,&t1,DT_RIGHT|DT_VCENTER|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);
        RECT t2={rEdge-w1,top+lh+fh+S(20),rEdge,top+lh+fh+S(20)+lh};
        DrawTextW(dc,L"مدیر بخش (اختیاری)",-1,&t2,DT_RIGHT|DT_VCENTER|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);
        int y3=top+lh+fh+S(20)+lh+fh+S(20);
        RECT t3={rEdge-w1,y3+S(34),rEdge,y3+S(34)+lh};
        std::wstring idlbl=L"شناسهٔ بخش";
        if(s_mg->autoId) idlbl=L"شناسهٔ بخش (خودکار: "+s_mg->autoIdPreview+L")";
        SetTextColor(dc,g_theme.textDim);
        DrawTextW(dc,idlbl.c_str(),-1,&t3,DT_RIGHT|DT_VCENTER|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);
        return;
    }
    // existing sections grid
    if(s_mg->cats.empty()){
        SetTextColor(dc,g_theme.textDim); SelectObject(dc,g_fUI);
        RECT er={a.left,a.top+mgHeadH()+S(80),a.right,a.top+mgHeadH()+S(120)};
        DrawTextW(dc,L"هنوز بخشی ثبت نشده است.",-1,&er,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);
        return;
    }
    for(int i=0;i<(int)s_mg->cats.size();i++){
        RECT r=mgSecCardRect(h,i);
        bool hot=(s_mg->hot==MG_CARD_BASE+i);
        gpRoundRectBg(dc,r,S(14), hot?g_theme.surface2:g_theme.surface,
                      hot?g_infoAccent:g_theme.border, g_theme.bg);
        RECT ir={r.right-S(50),r.top+S(14),r.right-S(18),r.top+S(46)};
        drawIcon(dc,ICO_SHIELD,ir,g_infoAccent,S(2));
        SetTextColor(dc,g_theme.text); SelectObject(dc,g_fUIB);
        RECT nr={r.left+S(14),r.top+S(16),r.right-S(56),r.top+S(46)};
        DrawTextW(dc,s_mg->cats[i].name.c_str(),-1,&nr,DT_RIGHT|DT_VCENTER|DT_SINGLELINE|DT_RTLREADING|DT_WORD_ELLIPSIS|DT_NOPREFIX);
        SetTextColor(dc,g_theme.textDim); SelectObject(dc,g_fSmall);
        RECT idr={r.left+S(14),r.top+S(52),r.right-S(14),r.top+S(74)};
        DrawTextW(dc,(L"شناسه: "+toFaDigits(s_mg->cats[i].id)).c_str(),-1,&idr,DT_RIGHT|DT_VCENTER|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);
        std::wstring mgr=s_mg->cats[i].manager.empty()?L"بدون مدیر":(L"مدیر: "+s_mg->cats[i].manager);
        RECT mr={r.left+S(14),r.top+S(76),r.right-S(14),r.top+S(98)};
        DrawTextW(dc,mgr.c_str(),-1,&mr,DT_RIGHT|DT_VCENTER|DT_SINGLELINE|DT_RTLREADING|DT_WORD_ELLIPSIS|DT_NOPREFIX);
    }
}

static void mgPaintEmps(HDC dc, HWND h){
    RECT a=mgContentRect(h);
    if(s_mg->emps.empty()){
        SetTextColor(dc,g_theme.textDim); SelectObject(dc,g_fUI);
        RECT er={a.left,a.top+mgHeadH()+S(90),a.right,a.top+mgHeadH()+S(130)};
        DrawTextW(dc,L"کارمندی با این فیلتر یافت نشد.",-1,&er,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);
        return;
    }
    for(int i=0;i<(int)s_mg->emps.size();i++){
        RECT r=mgEmpRowRect(h,i);
        if(r.top>a.bottom) break;
        bool hot=(s_mg->hot==MG_DETAIL_BASE+i);
        gpRoundRectBg(dc,r,S(12), hot?g_theme.surface2:g_theme.surface,
                      hot?g_infoAccent:g_theme.border, g_theme.bg);
        const EmpRow& e=s_mg->emps[i];
        // online dot
        { RECT dot={r.right-S(34),r.top+(r.bottom-r.top)/2-S(6),r.right-S(22),r.top+(r.bottom-r.top)/2+S(6)};
          gpRoundRectBg(dc,dot,S(6), e.online?g_theme.success:g_theme.textDim,
                        e.online?g_theme.success:g_theme.textDim, g_theme.surface); }
        SetTextColor(dc,g_theme.text); SelectObject(dc,g_fUIB);
        RECT nr={r.left+S(14),r.top+S(8),r.right-S(48),r.top+S(32)};
        DrawTextW(dc,e.user.fullname.c_str(),-1,&nr,DT_RIGHT|DT_VCENTER|DT_SINGLELINE|DT_RTLREADING|DT_WORD_ELLIPSIS|DT_NOPREFIX);
        SetTextColor(dc,g_theme.textDim); SelectObject(dc,g_fSmall);
        std::wstring sub=e.deptName;
        if(!e.prof.empId.empty()) sub+=L"  •  کد: "+toFaDigits(e.prof.empId);
        if(!e.prof.position.empty()) sub+=L"  •  "+e.prof.position;
        RECT sr={r.left+S(14),r.top+S(32),r.right-S(48),r.bottom-S(8)};
        DrawTextW(dc,sub.c_str(),-1,&sr,DT_RIGHT|DT_VCENTER|DT_SINGLELINE|DT_RTLREADING|DT_WORD_ELLIPSIS|DT_NOPREFIX);
    }
}

static void mgPaintMsg(HDC dc, HWND h){
    RECT a=mgContentRect(h);
    SetTextColor(dc,g_theme.textDim); SelectObject(dc,g_fUI);
    RECT tr={a.left+S(24),a.top+mgHeadH()+S(96),a.right-S(24),a.top+mgHeadH()+S(140)};
    DrawTextW(dc,L"برای ارسال پیام، رسانه و هدف‌گذاری، دکمهٔ «نوشتن پیام» را بزنید.",-1,&tr,
        DT_CENTER|DT_WORDBREAK|DT_RTLREADING|DT_NOPREFIX);
}

static void mgPaintReqs(HDC dc, HWND h){
    RECT a=mgContentRect(h);
    int top=a.top+mgHeadH()+S(12);
    // ---- notifications ABOVE the buttons (Task 11) ----
    int nSet=unseenSetReqCount(), nProf=unseenProfReqCount();
    RECT nb={a.left+S(12),top,a.right-S(12),top+S(60)};
    COLORREF nc=(nSet+nProf)>0?g_infoAccent:g_theme.border;
    gpRoundRectBg(dc,nb,S(12), g_theme.surface, nc, g_theme.bg);
    RECT ic={nb.right-S(48),nb.top+S(16),nb.right-S(18),nb.top+S(46)};
    drawIcon(dc,ICO_BELL,ic,(nSet+nProf)>0?g_infoAccent:g_theme.textDim,S(2));
    SetTextColor(dc,g_theme.text); SelectObject(dc,g_fUIB);
    std::wstring nt;
    if(nSet+nProf>0) nt=L"شما "+toFaDigits(std::to_wstring(nSet+nProf))+L" درخواست بررسی‌نشده دارید.";
    else nt=L"درخواست بررسی‌نشده‌ای وجود ندارد.";
    RECT ntr={nb.left+S(16),nb.top,nb.right-S(54),nb.bottom};
    DrawTextW(dc,nt.c_str(),-1,&ntr,DT_RIGHT|DT_VCENTER|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);
    // ---- separator line ----
    int sy=nb.bottom+S(16);
    gpLine(dc,a.left+S(12),sy,a.right-S(12),sy,g_theme.border,1);
    // the two big buttons sit just below (placed by mgLayout); their position
    // starts at top so we push them with the layout — here we just label hints.
}

// ---- saved messages (LOCAL notes) page -------------------------------------
//  Stored locally per machine/user (data\local_notes_<user>.dat) — NEVER sent
//  over the network. Both staff and management have their own private area.
static int mgNoteListTop(HWND h){ return mgSavedComposeBottom(h)+S(12); }
static int mgNoteRowH(){ return S(78); }
static RECT mgNoteRowRect(HWND h,int i){
    RECT a=mgContentRect(h);
    int y=mgNoteListTop(h)+i*(mgNoteRowH()+S(8));
    RECT r={a.left+S(12),y,a.right-S(12),y+mgNoteRowH()}; return r;
}
// delete button + thumbnail rects within a note row
static RECT mgNoteDelRect(RECT row){ RECT b={row.left+S(12),row.bottom-S(34),row.left+S(82),row.bottom-S(10)}; return b; }
static RECT mgNoteImgRect(RECT row){ int s=S(54); RECT b={row.right-S(12)-s,row.top+S(12),row.right-S(12),row.top+S(12)+s}; return b; }
static void mgPaintSaved(HDC dc, HWND h){
    RECT a=mgContentRect(h);
    int cTop=mgSavedComposeTop(h);
    // section caption ABOVE the editor (icon flush-right, title to its left)
    { SelectObject(dc,g_fUIB); SetTextColor(dc,g_theme.accent);
      int icoW=S(18), gap=S(8);
      RECT ci={a.right-S(12)-icoW,cTop+S(1),a.right-S(12),cTop+S(19)};
      drawIcon(dc,ICO_SAVE,ci,g_theme.accent,S(2));
      RECT cr={a.left+S(12),cTop,a.right-S(12)-icoW-gap,cTop+mgSavedCaptionH()};
      DrawTextW(dc,L"یادداشت جدید",-1,&cr,
          DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX); }
    // editor input-well behind eNote (consistent flat field look)
    { int edTop=cTop+mgSavedCaptionH(), edH=mgSavedEditorH();
      int bw=S(110), gap=S(10);
      RECT well={a.left+S(12)+bw+gap-S(6),edTop-S(4),a.right-S(12)+S(0),edTop+edH+S(4)};
      bool foc=(GetFocus()==s_mg->eNote);
      fillRoundRect(dc,well,S(8),g_theme.inputBg,foc?g_theme.accent:g_theme.border); }
    // hint line BELOW the compose block
    SetTextColor(dc,g_theme.textDim); SelectObject(dc,g_fSmall);
    int hintY=mgSavedComposeBottom(h)-mgSavedHintH();
    RECT hint={a.left+S(12),hintY,a.right-S(12),hintY+mgSavedHintH()};
    std::wstring at = s_mg->noteAttach.empty()? std::wstring() : L"   •   پیوست: ✓";
    DrawTextW(dc,(L"یادداشت‌ها فقط روی همین رایانه ذخیره می‌شوند و در شبکه منتقل نمی‌گردند."+at).c_str(),
        -1,&hint,DT_RIGHT|DT_VCENTER|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);
    auto notes=loadLocalNotes(g_session.user.username);
    if(notes.empty()){
        SetTextColor(dc,g_theme.textDim); SelectObject(dc,g_fUI);
        RECT er={a.left,mgNoteListTop(h)+S(30),a.right,mgNoteListTop(h)+S(70)};
        DrawTextW(dc,L"یادداشت ذخیره‌شده‌ای وجود ندارد.",-1,&er,
            DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);
        return;
    }
    for(int i=0;i<(int)notes.size();i++){
        RECT r=mgNoteRowRect(h,i); if(r.top>=a.bottom-S(16)) break;
        bool hot=(s_mg->noteHot==i);
        gpRoundRectBg(dc,r,S(12),g_theme.surface, hot?g_infoAccent:g_theme.border, g_theme.bg);
        // time header
        SetTextColor(dc,g_infoAccent2); SelectObject(dc,g_fSmall);
        RECT hr={r.left+S(14),r.top+S(8),r.right-S(80),r.top+S(28)};
        DrawTextW(dc,notes[i].time.c_str(),-1,&hr,
            DT_RIGHT|DT_VCENTER|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);
        // body text
        SetTextColor(dc,g_theme.text); SelectObject(dc,g_fUI);
        bool hasImg=!notes[i].attachPath.empty();
        int bodyR = hasImg ? r.right-S(80) : r.right-S(14);
        RECT br={r.left+S(90),r.top+S(28),bodyR,r.bottom-S(8)};
        std::wstring body=notes[i].text;
        DrawTextW(dc,body.c_str(),-1,&br,
            DT_RIGHT|DT_WORDBREAK|DT_RTLREADING|DT_NOPREFIX|DT_END_ELLIPSIS|DT_EDITCONTROL);
        // image thumbnail (click → open/enlarge)
        if(hasImg){
            RECT ir=mgNoteImgRect(r);
            if(!gpDrawImageFileCircle(dc,notes[i].attachPath,ir)){
                gpRoundRectBg(dc,ir,S(8),g_theme.surface2,g_theme.border,g_theme.surface);
                RECT ic={ir.left+S(12),ir.top+S(12),ir.right-S(12),ir.bottom-S(12)};
                drawIcon(dc,ICO_ID,ic,g_theme.textDim,S(2));
            }
        }
        // delete button
        RECT dl=mgNoteDelRect(r);
        gpRoundRect(dc,dl,S(8),g_theme.surface2,g_theme.border,255);
        SelectObject(dc,g_fSmall); SetTextColor(dc,g_theme.danger);
        DrawTextW(dc,L"حذف",-1,&dl,DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
    }
}

// ---- backup page: an intro + a big «باز کردن مدیریت پشتیبان» action --------
static RECT mgBackupBtnRect(HWND h){
    RECT a=mgContentRect(h);
    int top=a.top+mgHeadH()+S(120);
    int w=S(380);
    RECT r={ (a.left+a.right)/2-w/2, top, (a.left+a.right)/2+w/2, top+S(56) };
    return r;
}
static void mgPaintBackup(HDC dc, HWND h){
    RECT a=mgContentRect(h);
    int top=a.top+mgHeadH()+S(16);
    // intro card
    RECT ib={a.left+S(16),top,a.right-S(16),top+S(84)};
    gpRoundRectBg(dc,ib,S(14),g_theme.surface,g_theme.border,g_theme.bg);
    RECT ic={ib.right-S(60),ib.top+S(24),ib.right-S(20),ib.top+S(64)};
    drawIcon(dc,ICO_REFRESH,ic,g_theme.accent,S(2));
    SetTextColor(dc,g_theme.text); SelectObject(dc,g_fUIB);
    RECT t1={ib.left+S(16),ib.top+S(12),ib.right-S(70),ib.top+S(40)};
    DrawTextW(dc,L"پشتیبان‌گیری و بازیابی اطلاعات بیماران",-1,&t1,
        DT_RIGHT|DT_VCENTER|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);
    SetTextColor(dc,g_theme.textDim); SelectObject(dc,g_fSmall);
    RECT t2={ib.left+S(16),ib.top+S(40),ib.right-S(70),ib.bottom-S(8)};
    DrawTextW(dc,L"تهیهٔ نسخهٔ پشتیبان کامل، یا بازیابی کامل/انتخابی از فایل‌های پشتیبان (شامل پشتیبان‌های حجیم متین‌طب). عملیات سنگین در پس‌زمینه اجرا می‌شود.",
        -1,&t2,DT_RIGHT|DT_WORDBREAK|DT_RTLREADING|DT_NOPREFIX);
    // big open button — icon flush-right (RTL leading edge), the label centred
    // in the area to its LEFT so the two never overlap or get clipped.
    RECT r=mgBackupBtnRect(h);
    bool hot=(s_mg&&s_mg->hot==MG_BACKUP);
    gpRoundRect(dc,r,S(10), hot?g_theme.accentHover:g_theme.accent, g_theme.accent,255);
    int icoW=S(28), igap=S(10);
    RECT bi={r.right-S(16)-icoW,r.top+S(14),r.right-S(16),r.bottom-S(14)};
    drawIcon(dc,ICO_SAVE,bi,g_theme.accentText,S(2));
    SetTextColor(dc,g_theme.accentText); SelectObject(dc,g_fBig);
    RECT bl={r.left+S(12),r.top,r.right-S(16)-icoW-igap,r.bottom};
    DrawTextW(dc,L"باز کردن مدیریت پشتیبان",-1,&bl,
        DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);
}

// ---- patients page: a small status line under the (real-HWND) list ----------
static void mgPaintPatients(HDC dc, HWND h){
    if(!s_mg) return;
    RECT a=mgContentRect(h);
    SetTextColor(dc,g_theme.textDim); SelectObject(dc,g_fSmall);
    RECT sr={a.left+S(24),a.bottom-S(28),a.right-S(24),a.bottom-S(6)};
    std::wstring s;
    if(s_mg->ptLoading)      s=L"در حال بارگذاری بیماران…";
    else if(!s_mg->ptLoaded) s=L"—";
    else {
        size_t tot = s_mg->ptAll? s_mg->ptAll->size():0;
        size_t shown = s_mg->ptView.size();
        s=L"نمایش "+toFaDigits(std::to_wstring(shown))+L" از "+
          toFaDigits(std::to_wstring(tot))+L" بیمار";
    }
    DrawTextW(dc,s.c_str(),-1,&sr,DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
}

// ---- the dashboard shell: nav rail (right) + content card (left) -----------
static void mgPaintShell(HDC dc, HWND h){
    RECT rc; GetClientRect(h,&rc);
    FillRect(dc,&rc,g_brBg); SetBkMode(dc,TRANSPARENT);
    // ---- nav rail on the RIGHT ----
    RECT nav={rc.right-mgNavW(), rc.top, rc.right, rc.bottom};
    gpRoundRectBg(dc,(RECT){nav.left+S(4),nav.top+S(8),nav.right-S(8),nav.bottom-S(8)},
                  S(16),g_theme.surface,g_theme.border,g_theme.bg);
    // app/section title in the rail header
    SetTextColor(dc,g_theme.text); SelectObject(dc,g_fTitle);
    RECT th={nav.left+S(12),nav.top+S(20),nav.right-S(16),nav.top+S(60)};
    DrawTextW(dc,L"پنل مدیریت",-1,&th,DT_RIGHT|DT_VCENTER|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);
    for(int i=0;i<PG_COUNT;i++){
        RECT r=mgNavItemRect(h,i);
        bool sel=(s_mg && s_mg->page==i);
        bool hot=(s_mg && s_mg->navHot==i);
        if(sel) gpRoundRectBg(dc,r,S(12),g_infoAccent,g_infoAccent,g_theme.surface);
        else if(hot) gpRoundRectBg(dc,r,S(12),g_theme.surface2,g_theme.border,g_theme.surface);
        RECT ir={r.right-S(44),r.top+S(12),r.right-S(16),r.top+S(40)};
        drawIcon(dc,MG_NAV_ICONS[i],ir, sel?RGB(255,255,255):g_theme.textDim, S(2));
        SetTextColor(dc, sel?RGB(255,255,255):g_theme.text); SelectObject(dc,g_fUI);
        RECT lr={r.left+S(12),r.top,r.right-S(50),r.bottom};
        DrawTextW(dc,MG_NAV_LABELS[i],-1,&lr,DT_RIGHT|DT_VCENTER|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);
        // request badge on the REQS nav item
        if(i==PG_REQS){
            int n=unseenSetReqCount()+unseenProfReqCount();
            if(n>0){
                RECT bd={r.left+S(12),r.top+S(14),r.left+S(34),r.top+S(36)};
                gpRoundRectBg(dc,bd,S(11),g_infoAccent2,g_infoAccent2,
                              sel?g_infoAccent:(hot?g_theme.surface2:g_theme.surface));
                SetTextColor(dc,RGB(255,255,255)); SelectObject(dc,g_fSmall);
                DrawTextW(dc,toFaDigits(std::to_wstring(n)).c_str(),-1,&bd,
                    DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);
            }
        }
    }
    // ---- content card on the LEFT ----
    RECT a=mgContentRect(h);
    gpRoundRectBg(dc,a,S(18),g_theme.surface,g_theme.border,g_theme.bg);
    // content header
    SetTextColor(dc,g_theme.text); SelectObject(dc,g_fBig);
    RECT hr={a.left+S(24),a.top+S(16),a.right-S(24),a.top+mgHeadH()-S(8)};
    DrawTextW(dc,mgCardTitle(s_mg?s_mg->page:0),-1,&hr,DT_RIGHT|DT_VCENTER|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);
    gpLine(dc,a.left+S(16),a.top+mgHeadH(),a.right-S(16),a.top+mgHeadH(),g_theme.border,1);
    // page body
    if(s_mg){
        switch(s_mg->page){
            case PG_DASH:  mgPaintDash(dc,h);  break;
            case PG_PATIENTS: mgPaintPatients(dc,h); break;
            case PG_SECTS: mgPaintSects(dc,h); break;
            case PG_EMPS:  mgPaintEmps(dc,h);  break;
            case PG_MSG:   mgPaintMsg(dc,h);   break;
            case PG_REQS:  mgPaintReqs(dc,h);  break;
            case PG_SAVED: mgPaintSaved(dc,h); break;
            case PG_BACKUP:mgPaintBackup(dc,h);break;
        }
        if(!s_mg->msg.empty()){
            SetTextColor(dc,s_mg->msgCol); SelectObject(dc,g_fSmall);
            RECT mr={a.left+S(24),a.bottom-S(30),a.right-S(24),a.bottom-S(8)};
            DrawTextW(dc,s_mg->msg.c_str(),-1,&mr,DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
        }
    }
}

// ============================================================================
//  hit-testing
// ============================================================================
static int mgHit(HWND h, POINT pt){
    if(!s_mg) return 0;
    // nav rail
    for(int i=0;i<PG_COUNT;i++){
        RECT r=mgNavItemRect(h,i);
        if(PtInRect(&r,pt)) return MG_NAV_BASE+i;
    }
    if(s_mg->page==PG_DASH){
        for(int i=0;i<4;i++){ RECT r=mgQuickRect(h,i); if(PtInRect(&r,pt)) return MG_QA_BASE+i; }
    } else if(s_mg->page==PG_SECTS && !s_mg->secAddMode){
        for(int i=0;i<(int)s_mg->cats.size();i++){ RECT r=mgSecCardRect(h,i); if(PtInRect(&r,pt)) return MG_CARD_BASE+i; }
    } else if(s_mg->page==PG_EMPS){
        for(int i=0;i<(int)s_mg->emps.size();i++){ RECT r=mgEmpRowRect(h,i); if(PtInRect(&r,pt)) return MG_DETAIL_BASE+i; }
    } else if(s_mg->page==PG_BACKUP){
        RECT r=mgBackupBtnRect(h); if(PtInRect(&r,pt)) return MG_BACKUP;
    }
    return 0;
}

static void mgRefreshReqBadge(){
    // badge is drawn on the nav rail; just repaint
    if(g_hManage) InvalidateRect(g_hManage,NULL,FALSE);
}

static void mgFillDeptCombo(){
    if(!s_mg || !s_mg->cDept) return;
    SendMessageW(s_mg->cDept,CB_RESETCONTENT,0,0);
    SendMessageW(s_mg->cDept,CB_ADDSTRING,0,(LPARAM)L"همهٔ بخش‌ها");
    auto cats=loadDepts();
    for(auto& c:cats) SendMessageW(s_mg->cDept,CB_ADDSTRING,0,(LPARAM)c.name.c_str());
    // restore selection
    int sel=0;
    if(!s_mg->empDeptFilter.empty()){
        for(int i=0;i<(int)cats.size();i++) if(cats[i].id==s_mg->empDeptFilter){ sel=i+1; break; }
    }
    SendMessageW(s_mg->cDept,CB_SETCURSEL,sel,0);
}

static void mgGoPage(HWND h, int page){
    if(!s_mg) return;
    s_mg->page=page; s_mg->secAddMode=false; s_mg->msg.clear();
    if(page==PG_SECTS){ mgLoadCats(); mgUpdateAutoIdPreview(); }
    else if(page==PG_EMPS){ mgLoadEmps(); mgFillDeptCombo(); }
    else if(page==PG_PATIENTS){ mgPatientsLoadAsync(h); }
    mgLayout(h);
    InvalidateRect(h,NULL,TRUE);
}

// ---- sort-button label refresh (active highlighting via text marker) -------
static void mgRefreshSortBtns(){
    if(!s_mg) return;
    SetWindowTextW(s_mg->bAbc,     s_mg->empSort==0?L"● الفبایی":L"الفبایی");
    SetWindowTextW(s_mg->bNew,     s_mg->empSort==1?L"● جدیدترین":L"جدیدترین");
    SetWindowTextW(s_mg->bCreated, s_mg->empSort==2?L"● زمان ساخت":L"زمان ساخت");
}

// ============================================================================
//  window procedure
// ============================================================================
static LRESULT CALLBACK manageProc(HWND h, UINT m, WPARAM w, LPARAM l){
    switch(m){
    case WM_CREATE: {
        s_mg=new ManageState();
        s_mg->page=PG_DASH; s_mg->hot=0; s_mg->navHot=-1;
        s_mg->secAddMode=false; s_mg->autoId=true;
        s_mg->empSort=1; s_mg->msgCol=g_theme.textDim;
        DWORD es=WS_CHILD|WS_TABSTOP|ES_AUTOHSCROLL;
        s_mg->eName=CreateWindowExW(0,L"EDIT",L"",es,0,0,10,10,h,(HMENU)MG_E_NAME,g_hInst,0);
        s_mg->eMgr =CreateWindowExW(0,L"EDIT",L"",es,0,0,10,10,h,(HMENU)MG_E_MGR,g_hInst,0);
        s_mg->eId  =CreateWindowExW(0,L"EDIT",L"",es,0,0,10,10,h,(HMENU)MG_E_ID,g_hInst,0);
        s_mg->bAdd =createFlatButton(h,MG_ADD_CAT,L"افزودن بخش",ICO_PLUS,BS_PRIMARY,0,0,10,10);
        s_mg->bAutoId=createFlatButton(h,MG_CB_AUTOID,L"\u2611 شناسه خودکار",0,BS_OUTLINE,0,0,10,10);
        s_mg->bSecNew=createFlatButton(h,MG_DESIGN_SEC,L"ایجاد بخش",ICO_PLUS,BS_PRIMARY,0,0,10,10);
        s_mg->bSecBack=createFlatButton(h,MG_BACK,L"بازگشت",ICO_BACK,BS_OUTLINE,0,0,10,10);
        s_mg->bAbc =createFlatButton(h,MG_FILTER_ABC,L"الفبایی",0,BS_OUTLINE,0,0,10,10);
        s_mg->bNew =createFlatButton(h,MG_FILTER_NEW,L"● جدیدترین",0,BS_OUTLINE,0,0,10,10);
        s_mg->bCreated=createFlatButton(h,MG_FILTER_CREATED,L"زمان ساخت",0,BS_OUTLINE,0,0,10,10);
        s_mg->bAddEmp=createFlatButton(h,MG_ADD_EMP,L"افزودن کارمند",ICO_USER,BS_PRIMARY,0,0,10,10);
        s_mg->cDept=createThemedCombo(h,MG_CB_DEPT);
        s_mg->bMsg =createFlatButton(h,MG_MSG,L"نوشتن پیام",ICO_BELL,BS_PRIMARY,0,0,10,10);
        s_mg->bReqs=createFlatButton(h,MG_REQS,L"درخواست‌های تغییر تنظیمات",ICO_GEAR,BS_INFO,0,0,10,10);
        s_mg->bProfReqs=createFlatButton(h,MG_PROFREQS,L"درخواست‌های تغییر پروفایل",ICO_USER,BS_INFO,0,0,10,10);
        // saved messages (local notes) controls
        s_mg->eNote=CreateWindowExW(0,L"EDIT",L"",
            WS_CHILD|WS_TABSTOP|ES_AUTOVSCROLL|ES_MULTILINE|WS_VSCROLL,
            0,0,10,10,h,(HMENU)MG_E_NOTE,g_hInst,0);
        s_mg->bNoteAttach=createFlatButton(h,MG_NOTE_ATTACH,L"تصویر",ICO_ID,BS_OUTLINE,0,0,10,10);
        s_mg->bNoteSend=createFlatButton(h,MG_NOTE_SEND,L"ذخیره",ICO_SAVE,BS_PRIMARY,0,0,10,10);
        s_mg->noteHot=-1;
        // ---- patients page controls (v1.3.0) ----
        s_mg->ptSearch=CreateWindowExW(0,L"EDIT",L"",
            WS_CHILD|WS_TABSTOP|ES_AUTOHSCROLL,0,0,10,10,h,(HMENU)MG_PT_SEARCH,g_hInst,0);
        SendMessageW(s_mg->ptSearch,WM_SETFONT,(WPARAM)g_fUI,TRUE);
        s_mg->ptList=CreateWindowExW(WS_EX_CLIENTEDGE,WC_LISTVIEWW,L"",
            WS_CHILD|WS_TABSTOP|LVS_OWNERDATA|LVS_REPORT|LVS_SHOWSELALWAYS|LVS_SINGLESEL,
            0,0,10,10,h,(HMENU)MG_PT_LIST,g_hInst,0);
        ListView_SetExtendedListViewStyle(s_mg->ptList,
            LVS_EX_FULLROWSELECT|LVS_EX_DOUBLEBUFFER|LVS_EX_GRIDLINES);
        SendMessageW(s_mg->ptList,WM_SETFONT,(WPARAM)g_fUI,TRUE);
        {   // RTL report columns (rightmost first): #, name, nid, mobile, gender, ...
            struct { const wchar_t* t; int w; } cols[]={
                {L"#",S(44)},{L"نام و نام خانوادگی",S(220)},{L"کد ملی",S(130)},
                {L"تلفن همراه",S(140)},{L"جنسیت",S(80)},{L"تاریخ تولد",S(120)} };
            LVCOLUMNW lc={0}; lc.mask=LVCF_TEXT|LVCF_WIDTH|LVCF_FMT; lc.fmt=LVCFMT_RIGHT;
            for(int i=0;i<6;i++){ lc.pszText=(LPWSTR)cols[i].t; lc.cx=cols[i].w;
                ListView_InsertColumn(s_mg->ptList,i,&lc); }
        }
        SendMessageW(s_mg->eNote,WM_SETFONT,(WPARAM)g_fUI,TRUE);
        HWND eds[3]={s_mg->eName,s_mg->eMgr,s_mg->eId};
        for(HWND e:eds){ SendMessageW(e,WM_SETFONT,(WPARAM)g_fUI,TRUE); enableEnterNavigation(e); }
        g_hManage=h;
        mgLoadCats(); mgUpdateAutoIdPreview();
        mgGoPage(h,PG_DASH);
#ifdef AZ_DEBUG_BUILD
        {   // headless screenshot helper: jump straight to a named sub-page
            wchar_t dbgp[24]={0};
            GetEnvironmentVariableW(L"AZ_DEBUG_PAGE", dbgp, 24);
            if(dbgp[0]){
                int pg=-1;
                if(!wcscmp(dbgp,L"saved"))   pg=PG_SAVED;
                else if(!wcscmp(dbgp,L"reqs"))   pg=PG_REQS;
                else if(!wcscmp(dbgp,L"backup")) pg=PG_BACKUP;
                else if(!wcscmp(dbgp,L"msg"))    pg=PG_MSG;
                else if(!wcscmp(dbgp,L"emps"))   pg=PG_EMPS;
                else if(!wcscmp(dbgp,L"sects"))  pg=PG_SECTS;
                if(pg>=0){ mgGoPage(h,pg); InvalidateRect(h,NULL,TRUE); }
            }
        }
#endif
        return 0; }
    case WM_NCDESTROY: if(g_hManage==h) g_hManage=NULL;
        if(s_mg){ delete s_mg->ptAll; s_mg->ptAll=nullptr;
                  delete s_mg; s_mg=NULL; } break;
    case WM_SIZE: if(s_mg) mgLayout(h); return 0;
    case WM_APP_THEME: InvalidateRect(h,NULL,TRUE); return 0;
    case WM_CTLCOLOREDIT:{ HDC dc=(HDC)w; SetTextColor(dc,g_theme.inputText);
        SetBkColor(dc,g_theme.inputBg); return (LRESULT)g_brInput; }
    case WM_CTLCOLORLISTBOX:{ HDC dc=(HDC)w; SetTextColor(dc,g_theme.inputText);
        SetBkColor(dc,g_theme.inputBg); return (LRESULT)g_brInput; }
    case WM_CTLCOLORSTATIC:{ HDC dc=(HDC)w; SetTextColor(dc,g_theme.inputText);
        SetBkColor(dc,g_theme.inputBg); return (LRESULT)g_brInput; }
    case WM_MG_PATIENTS_READY:{
        auto* vec=(std::vector<PatientRow>*)l;
        if(!s_mg){ delete vec; return 0; }
        delete s_mg->ptAll; s_mg->ptAll=vec;
        s_mg->ptLoaded=true; s_mg->ptLoading=false;
        mgPatientsFilter();
        if(s_mg->page==PG_PATIENTS) InvalidateRect(h,NULL,TRUE);
        return 0; }
    case WM_TIMER:
        if(s_mg && w==0xB1 /*patients search debounce*/){
            KillTimer(h,0xB1);
            wchar_t b[256]={0}; if(s_mg->ptSearch) GetWindowTextW(s_mg->ptSearch,b,256);
            s_mg->ptQuery=b; mgPatientsFilter();
            return 0;
        }
        break;
    case WM_NOTIFY:{
        if(!s_mg) break;
        NMHDR* nh=(NMHDR*)l;
        if(nh && nh->hwndFrom==s_mg->ptList){
            if(nh->code==LVN_GETDISPINFOW){
                NMLVDISPINFOW* di=(NMLVDISPINFOW*)l;
                int vi=di->item.iItem;
                if(s_mg->ptAll && vi>=0 && vi<(int)s_mg->ptView.size() &&
                   (di->item.mask & LVIF_TEXT)){
                    const PatientRow& p=(*s_mg->ptAll)[s_mg->ptView[vi]];
                    static wchar_t cell[256];
                    switch(di->item.iSubItem){
                        case 0: wcsncpy(cell,toFaDigits(std::to_wstring(vi+1)).c_str(),255); break;
                        case 1: wcsncpy(cell,(p.first+L" "+p.last).c_str(),255); break;
                        case 2: wcsncpy(cell,toFaDigits(p.nid).c_str(),255); break;
                        case 3: wcsncpy(cell,toFaDigits(p.mobile).c_str(),255); break;
                        case 4: wcsncpy(cell,mgGenderText(p.gender),255); break;
                        case 5: wcsncpy(cell,toFaDigits(p.birth).c_str(),255); break;
                        default: cell[0]=0; break;
                    }
                    cell[255]=0; di->item.pszText=cell;
                }
                return 0;
            }
        }
        break; }
    case WM_COMMAND:{
        if(!s_mg) return 0;
        int id=LOWORD(w);
        if(id==MG_PT_SEARCH && HIWORD(w)==EN_CHANGE){
            SetTimer(h,0xB1,250,NULL);  // debounce 250 ms
            return 0;
        }
        if(id==MG_CB_DEPT && HIWORD(w)==CBN_SELCHANGE){
            int sel=(int)SendMessageW(s_mg->cDept,CB_GETCURSEL,0,0);
            auto cats=loadDepts();
            if(sel<=0) s_mg->empDeptFilter.clear();
            else if(sel-1<(int)cats.size()) s_mg->empDeptFilter=cats[sel-1].id;
            mgLoadEmps(); InvalidateRect(h,NULL,TRUE);
            return 0;
        }
        switch(id){
        case MG_ADD_CAT: mgDoAddCat(h); break;
        case MG_CB_AUTOID: s_mg->autoId=!s_mg->autoId;
            SetWindowTextW(s_mg->bAutoId,s_mg->autoId?L"\u2611 شناسه خودکار":L"\u2610 شناسه دستی");
            mgLayout(h); InvalidateRect(h,NULL,FALSE); break;
        case MG_DESIGN_SEC: s_mg->secAddMode=true; s_mg->msg.clear();
            mgUpdateAutoIdPreview(); mgLayout(h); InvalidateRect(h,NULL,TRUE); break;
        case MG_BACK: s_mg->secAddMode=false; mgLayout(h); InvalidateRect(h,NULL,TRUE); break;
        case MG_FILTER_ABC: s_mg->empSort=0; mgRefreshSortBtns(); mgLoadEmps(); InvalidateRect(h,NULL,FALSE); break;
        case MG_FILTER_NEW: s_mg->empSort=1; mgRefreshSortBtns(); mgLoadEmps(); InvalidateRect(h,NULL,FALSE); break;
        case MG_FILTER_CREATED: s_mg->empSort=2; mgRefreshSortBtns(); mgLoadEmps(); InvalidateRect(h,NULL,FALSE); break;
        case MG_ADD_EMP: openNewEmployee(h); break;
        case MG_MSG:  openMsgCompose(h); break;
        case MG_REQS: openSetReqs(h); break;
        case MG_PROFREQS: openProfReqs(h); break;
        case MG_NOTE_ATTACH:{
            wchar_t file[1024]={0};
            OPENFILENAMEW ofn={0}; ofn.lStructSize=sizeof(ofn); ofn.hwndOwner=h;
            ofn.lpstrFilter=L"تصاویر\0*.jpg;*.jpeg;*.png;*.gif;*.bmp\0همهٔ فایل‌ها\0*.*\0\0";
            ofn.lpstrFile=file; ofn.nMaxFile=1024;
            ofn.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST;
            if(GetOpenFileNameW(&ofn)){
                std::wstring stored=copyAttachmentLocal(file);
                s_mg->noteAttach = stored.empty()? std::wstring(file) : stored;
                SetWindowTextW(s_mg->bNoteAttach,L"تصویر ✓");
                InvalidateRect(h,NULL,FALSE);
            }
            break; }
        case MG_NOTE_SEND:{
            wchar_t buf[4096]={0}; GetWindowTextW(s_mg->eNote,buf,4096);
            std::wstring txt=trim(buf);
            if(txt.empty() && s_mg->noteAttach.empty()){
                MessageBoxW(h,L"متن یادداشت یا تصویری وارد کنید.",L"پیام‌های ذخیره‌شده",
                    MB_ICONINFORMATION|MB_OK);
                break;
            }
            pushLocalNote(g_session.user.username,txt,s_mg->noteAttach);
            s_mg->noteAttach.clear();
            SetWindowTextW(s_mg->eNote,L"");
            SetWindowTextW(s_mg->bNoteAttach,L"تصویر");
            InvalidateRect(h,NULL,TRUE);
            break; }
        }
        return 0; }
    case WM_MOUSEMOVE:{ POINT pt={GET_X_LPARAM(l),GET_Y_LPARAM(l)};
        int hr=mgHit(h,pt);
        int nh=-1;
        if(hr>=MG_NAV_BASE && hr<MG_NAV_BASE+PG_COUNT) nh=hr-MG_NAV_BASE;
        bool ch=false;
        if(s_mg){ if(s_mg->hot!=hr){ s_mg->hot=hr; ch=true; }
                  if(s_mg->navHot!=nh){ s_mg->navHot=nh; ch=true; } }
        if(s_mg && s_mg->page==PG_SAVED){
            int onh=s_mg->noteHot; s_mg->noteHot=-1;
            int cnt=localNoteCount(g_session.user.username);
            for(int i=0;i<cnt;i++){ RECT r=mgNoteRowRect(h,i);
                if(PtInRect(&r,pt)){ s_mg->noteHot=i; break; } }
            if(onh!=s_mg->noteHot) ch=true;
        }
        if(ch) InvalidateRect(h,NULL,FALSE);
        return 0; }
    case WM_LBUTTONDOWN:{ POINT pt={GET_X_LPARAM(l),GET_Y_LPARAM(l)};
        // saved-messages page: note delete / image-open hit-tests first
        if(s_mg && s_mg->page==PG_SAVED){
            auto notes=loadLocalNotes(g_session.user.username);
            for(int i=0;i<(int)notes.size();i++){
                RECT r=mgNoteRowRect(h,i);
                RECT dl=mgNoteDelRect(r), im=mgNoteImgRect(r);
                if(PtInRect(&dl,pt)){
                    if(MessageBoxW(h,L"این یادداشت حذف شود؟",L"حذف یادداشت",
                        MB_ICONQUESTION|MB_YESNO|MB_DEFBUTTON2)==IDYES){
                        deleteLocalNote(g_session.user.username,i);
                        InvalidateRect(h,NULL,TRUE);
                    }
                    return 0;
                }
                if(!notes[i].attachPath.empty() && PtInRect(&im,pt)){
                    ShellExecuteW(h,L"open",notes[i].attachPath.c_str(),NULL,NULL,SW_SHOWNORMAL);
                    return 0;
                }
            }
        }
        int id=mgHit(h,pt);
        if(id==MG_BACKUP){ openBackupManager(h); }
        else if(id>=MG_NAV_BASE && id<MG_NAV_BASE+PG_COUNT){ mgGoPage(h,id-MG_NAV_BASE); }
        else if(id>=MG_QA_BASE && id<MG_QA_BASE+4){
            int i=id-MG_QA_BASE;
            if(i==0){ mgGoPage(h,PG_SECTS); s_mg->secAddMode=true; mgUpdateAutoIdPreview(); mgLayout(h); InvalidateRect(h,NULL,TRUE); }
            else if(i==1){ mgGoPage(h,PG_EMPS); openNewEmployee(h); }
            else if(i==2){ mgGoPage(h,PG_MSG); openMsgCompose(h); }
            else if(i==3){ mgGoPage(h,PG_REQS); }
        }
        else if(id>=MG_DETAIL_BASE){ int i=id-MG_DETAIL_BASE;
            if(i<(int)s_mg->emps.size()) openEmpDetails(h,s_mg->emps[i]); }
        else if(id>=MG_CARD_BASE){ int i=id-MG_CARD_BASE;
            if(i<(int)s_mg->cats.size()){
                // clicking a section card jumps to its employees, filtered
                s_mg->empDeptFilter=s_mg->cats[i].id;
                mgGoPage(h,PG_EMPS);
            }
        }
        return 0; }
    case WM_ERASEBKGND: return 1;
    case WM_PAINT:{ PAINTSTRUCT ps; HDC dc0=BeginPaint(h,&ps);
        RECT rc; GetClientRect(h,&rc);
        HDC dc=CreateCompatibleDC(dc0);
        HBITMAP bmp=CreateCompatibleBitmap(dc0,rc.right,rc.bottom);
        HGDIOBJ obm=SelectObject(dc,bmp);
        mgPaintShell(dc,h);
        BitBlt(dc0,0,0,rc.right,rc.bottom,dc,0,0,SRCCOPY);
        SelectObject(dc,obm); DeleteObject(bmp); DeleteDC(dc);
        EndPaint(h,&ps); return 0; }
    }
    return DefWindowProcW(h,m,w,l);
}

HWND createManageScreen(HWND frame){
    static bool reg=false;
    if(!reg){
        WNDCLASSW wc={0};
        wc.lpfnWndProc=manageProc; wc.hInstance=g_hInst;
        wc.hCursor=LoadCursor(NULL,IDC_ARROW);
        wc.lpszClassName=MG_CLASS;
        RegisterClassW(&wc); reg=true;
    }
    RECT rc=frameContentRect();
    HWND h=CreateWindowExW(0,MG_CLASS,L"",WS_CHILD|WS_VISIBLE|WS_CLIPCHILDREN,
        rc.left,rc.top,rc.right-rc.left,rc.bottom-rc.top,frame,NULL,g_hInst,NULL);
    if(s_mg) mgLayout(h);
    return h;
}
