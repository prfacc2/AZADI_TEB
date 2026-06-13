// ============================================================================
//  manage.inc — management panel implementation (#included into admin.cpp)
//  Owner-drawn, theme-aware, RTL. Two views:
//    VIEW_CATS  — department category grid + add-category bar + filters
//    VIEW_EMPS  — employees of the selected category in a table
//  Plus a centered EMPLOYEE-DETAILS modal with an A4/A5 print preview.
// ============================================================================
#define MG_CLASS L"AzManage"

enum { VIEW_CATS=0, VIEW_EMPS=1 };

// control ids
enum {
    MG_ADD_CAT=2001, MG_E_NAME, MG_E_MGR, MG_E_ID, MG_CB_AUTOID,
    MG_FILTER_ABC, MG_FILTER_NEW, MG_BACK,
    MG_ADD_EMP, MG_DESIGN,
    MG_CARD_BASE=2200,     // clicking a category card
    MG_DETAIL_BASE=2400,   // clicking an employee "details" button
};

struct EmpRow {
    User user;
    EmpProfile prof;
    bool online;
};

struct ManageState {
    int view;
    int hot;
    std::wstring curCat;        // selected category id
    std::wstring curCatName;
    int filter;                 // 0 = alphabetical, 1 = newest
    // controls
    HWND eName, eMgr, eId, bAdd, bAutoId, bAbc, bNew, bBack, bAddEmp, bDesign;
    bool autoId;
    std::vector<DeptCat> cats;
    std::vector<EmpRow>  emps;
    std::wstring msg; COLORREF msgCol;
};
static ManageState* s_mg=NULL;

// ---- data refresh ----------------------------------------------------------
static void mgLoadCats(){
    if(!s_mg) return;
    s_mg->cats=loadDepts();
    if(s_mg->filter==0){
        std::sort(s_mg->cats.begin(),s_mg->cats.end(),
            [](const DeptCat&a,const DeptCat&b){ return a.name<b.name; });
    } else {
        std::reverse(s_mg->cats.begin(),s_mg->cats.end());  // newest first
    }
}
static void mgLoadEmps(){
    if(!s_mg) return;
    s_mg->emps.clear();
    auto users=loadUsers();
    for(auto& u:users){
        EmpProfile p=loadEmpProfile(u.username);
        // a user belongs to the selected category by deptId OR by legacy dept name
        bool match = (!s_mg->curCat.empty() && p.deptId==s_mg->curCat)
                  || (u.dept==s_mg->curCatName && (p.deptId.empty()));
        if(s_mg->curCat.empty()) match=true;   // "all"
        if(!match) continue;
        EmpRow r; r.user=u; r.prof=p; r.online=isUserOnline(u.username);
        s_mg->emps.push_back(r);
    }
}

// ---- layout ----------------------------------------------------------------
static int mgTopH(){ return S(120); }
static void mgLayout(HWND h){
    if(!s_mg) return;
    RECT rc; GetClientRect(h,&rc);
    int W=rc.right, pad=S(24);
    if(s_mg->view==VIEW_CATS){
        // add-category bar across the top
        int y=S(70), x=W-pad, fh=S(36);
        int wName=S(220), wMgr=S(180), wId=S(120), wAuto=S(150), wBtn=S(120), g=S(8);
        MoveWindow(s_mg->eName, x-wName,                 y, wName, fh, TRUE); x-=wName+g;
        MoveWindow(s_mg->eMgr,  x-wMgr,                  y, wMgr,  fh, TRUE); x-=wMgr+g;
        MoveWindow(s_mg->bAutoId,x-wAuto,                y, wAuto, fh, TRUE); x-=wAuto+g;
        MoveWindow(s_mg->eId,   x-wId,                   y, wId,   fh, TRUE); x-=wId+g;
        MoveWindow(s_mg->bAdd,  x-wBtn,                  y, wBtn,  fh, TRUE);
        ShowWindow(s_mg->eName,SW_SHOW); ShowWindow(s_mg->eMgr,SW_SHOW);
        ShowWindow(s_mg->bAutoId,SW_SHOW); ShowWindow(s_mg->bAdd,SW_SHOW);
        ShowWindow(s_mg->eId,s_mg->autoId?SW_HIDE:SW_SHOW);
        // filter buttons (left)
        MoveWindow(s_mg->bAbc, pad,         S(70), S(120), fh, TRUE);
        MoveWindow(s_mg->bNew, pad+S(128),  S(70), S(120), fh, TRUE);
        ShowWindow(s_mg->bAbc,SW_SHOW); ShowWindow(s_mg->bNew,SW_SHOW);
        ShowWindow(s_mg->bBack,SW_HIDE); ShowWindow(s_mg->bAddEmp,SW_HIDE);
        ShowWindow(s_mg->bDesign,SW_HIDE);
    } else {
        ShowWindow(s_mg->eName,SW_HIDE); ShowWindow(s_mg->eMgr,SW_HIDE);
        ShowWindow(s_mg->bAutoId,SW_HIDE); ShowWindow(s_mg->bAdd,SW_HIDE);
        ShowWindow(s_mg->eId,SW_HIDE);
        ShowWindow(s_mg->bAbc,SW_HIDE); ShowWindow(s_mg->bNew,SW_HIDE);
        int fh=S(38);
        MoveWindow(s_mg->bBack,  W-pad-S(120), S(70), S(120), fh, TRUE);
        MoveWindow(s_mg->bAddEmp,W-pad-S(120)-S(8)-S(170), S(70), S(170), fh, TRUE);
        MoveWindow(s_mg->bDesign,pad, S(70), S(220), fh, TRUE);
        ShowWindow(s_mg->bBack,SW_SHOW); ShowWindow(s_mg->bAddEmp,SW_SHOW);
        ShowWindow(s_mg->bDesign,SW_SHOW);
    }
}

// ---- category card geometry ------------------------------------------------
static int mgCols(int W){ int c=(W-S(48))/S(240); return c<1?1:c; }
static RECT mgCardRect(HWND h, int idx){
    RECT rc; GetClientRect(h,&rc);
    int cols=mgCols(rc.right);
    int cw=S(224), ch=S(120), gap=S(16);
    int row=idx/cols, col=idx%cols;
    // RTL: first card on the right
    int x=rc.right-S(24)-(col+1)*cw-col*gap;
    int y=mgTopH()+S(8)+row*(ch+gap);
    RECT r={x,y,x+cw,y+ch}; return r;
}
static int mgEmpRowH(){ return S(56); }
static RECT mgEmpRowRect(HWND h, int idx){
    RECT rc; GetClientRect(h,&rc);
    int y=mgTopH()+S(8)+idx*(mgEmpRowH()+S(6));
    RECT r={S(24),y,rc.right-S(24),y+mgEmpRowH()}; return r;
}

// ====================== employee details modal ==============================
#define MGD_CLASS L"AzEmpDetail"
struct DetailState { EmpRow row; int hot; int paper; };
static HWND s_mgd=NULL; static DetailState* s_dt=NULL;

static int mgdCardW(){ return S(720); }
static int mgdCardH(){ return S(560); }
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
    line(L"کد ملی:",            toFaDigits(r.prof.nationalId), false);
    line(L"نام پدر:",           r.prof.fatherName, false);
    line(L"تلفن ثابت:",         toFaDigits(r.prof.landline), false);
    line(L"بخش:",               s_mg?s_mg->curCatName:r.user.dept, false);
    line(L"شیفت کاری:",         (r.prof.shiftFrom.empty()?L"—":(r.prof.shiftFrom+L" تا "+r.prof.shiftTo)), false);
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

// ====================== add-employee modal (simplified) =====================
//  Collects a full identity + document path and creates the login user plus a
//  pending employee profile attached to the current category.
static void mgAddEmployee(HWND h){
    // gather identity via a sequence of small prompts is clunky; instead we
    // reuse the existing user-create flow then attach profile. For a single
    // self-contained EXE we present an input via the admin's create form is not
    // available here, so create a minimal record the manager can flesh out.
    // We open a simple modal using the standard EDIT controls inline.
    // To keep it robust and non-blocking, we create a basic user and profile:
    wchar_t un[64]; SYSTEMTIME st=iranNow();
    swprintf(un,64,L"emp%04d%02d%02d%02d",st.wYear%100,st.wMonth,st.wDay,st.wMinute);
    User u; u.username=un; u.fullname=L"کارمند جدید";
    u.dept=s_mg->curCatName; u.role=0; u.hash=hashPassword(L"1234");
    std::wstring err;
    if(addUser(u,err)){
        EmpProfile p; p.username=u.username; p.deptId=s_mg->curCat;
        saveEmpProfile(p);
        s_mg->msg=L"کارمند پایه ساخته شد (نام کاربری: "+std::wstring(un)+
            L"، رمز: 1234). اطلاعات کامل را از پنل کاربران ویرایش کنید.";
        s_mg->msgCol=g_theme.success;
        mgLoadEmps();
    } else { s_mg->msg=err; s_mg->msgCol=g_theme.danger; }
    InvalidateRect(h,NULL,FALSE);
}

// ---- add category ----------------------------------------------------------
static void mgDoAddCat(HWND h){
    wchar_t nm[128]={0},mg[128]={0},id[64]={0};
    GetWindowTextW(s_mg->eName,nm,128);
    GetWindowTextW(s_mg->eMgr,mg,128);
    GetWindowTextW(s_mg->eId,id,64);
    DeptCat c; c.name=trim(nm); c.manager=trim(mg);
    c.id = s_mg->autoId? L"" : trim(id);
    c.icon=L"shield";
    std::wstring err;
    if(addDept(c,err)){
        s_mg->msg=L"بخش «"+c.name+L"» اضافه شد."; s_mg->msgCol=g_theme.success;
        SetWindowTextW(s_mg->eName,L""); SetWindowTextW(s_mg->eMgr,L"");
        SetWindowTextW(s_mg->eId,L"");
        mgLoadCats();
    } else { s_mg->msg=err; s_mg->msgCol=g_theme.danger; }
    InvalidateRect(h,NULL,FALSE);
}

// ---- hit testing -----------------------------------------------------------
static int mgHit(HWND h, POINT pt){
    if(!s_mg) return 0;
    if(s_mg->view==VIEW_CATS){
        for(size_t i=0;i<s_mg->cats.size();i++){
            RECT r=mgCardRect(h,(int)i);
            if(PtInRect(&r,pt)) return MG_CARD_BASE+(int)i;
        }
    } else {
        for(size_t i=0;i<s_mg->emps.size();i++){
            RECT r=mgEmpRowRect(h,(int)i);
            RECT db={r.left+S(12),r.top+S(12),r.left+S(120),r.bottom-S(12)};
            if(PtInRect(&db,pt)) return MG_DETAIL_BASE+(int)i;
        }
    }
    return 0;
}

// ---- painting --------------------------------------------------------------
static void mgPaintCats(HDC dc, HWND h){
    RECT rc; GetClientRect(h,&rc);
    // labels above the add bar
    SelectObject(dc,g_fSmall); SetTextColor(dc,g_theme.textDim);
    RECT lr={rc.right-S(244),S(50),rc.right-S(24),S(68)};
    DrawTextW(dc,L"افزودن بخش جدید: نام • مدیر • شناسه (خودکار/دستی)",-1,&lr,
        DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
    if(s_mg->cats.empty()){
        SelectObject(dc,g_fUI); SetTextColor(dc,g_theme.textDim);
        RECT er={S(24),mgTopH()+S(20),rc.right-S(24),mgTopH()+S(60)};
        DrawTextW(dc,L"هنوز بخشی تعریف نشده است. از نوار بالا یک بخش اضافه کنید.",-1,&er,
            DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
        return;
    }
    for(size_t i=0;i<s_mg->cats.size();i++){
        DeptCat& c=s_mg->cats[i];
        RECT r=mgCardRect(h,(int)i);
        bool hov=(s_mg->hot==MG_CARD_BASE+(int)i);
        gpShadow(dc,r,S(14),S(14),40);
        gpGradRoundRect(dc,r,S(14),
            hov?g_theme.surfaceTop:g_theme.surface, g_theme.surface,
            hov?g_theme.accent:g_theme.border);
        // icon badge
        int br=S(22), bx=r.right-S(32), by=r.top+S(30);
        RECT badge={bx-br,by-br,bx+br,by+br};
        gpGradRoundRect(dc,badge,br,g_theme.accent2,g_theme.accent,CLR_INVALID);
        RECT bi={badge.left+S(10),badge.top+S(10),badge.right-S(10),badge.bottom-S(10)};
        drawIcon(dc,ICO_SHIELD,bi,RGB(255,255,255),S(2));
        SetBkMode(dc,TRANSPARENT);
        SelectObject(dc,g_fUIB); SetTextColor(dc,g_theme.text);
        RECT nr={r.left+S(14),r.top+S(14),r.right-S(60),r.top+S(44)};
        DrawTextW(dc,c.name.c_str(),-1,&nr,
            DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX|DT_END_ELLIPSIS);
        SelectObject(dc,g_fSmall); SetTextColor(dc,g_theme.textDim);
        RECT mr={r.left+S(14),r.top+S(48),r.right-S(14),r.top+S(72)};
        DrawTextW(dc,(L"مدیر: "+(c.manager.empty()?std::wstring(L"—"):c.manager)).c_str(),-1,&mr,
            DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
        RECT ir={r.left+S(14),r.bottom-S(28),r.right-S(14),r.bottom-S(8)};
        DrawTextW(dc,(L"شناسه: "+toFaDigits(c.id)).c_str(),-1,&ir,
            DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
    }
}
static void mgPaintEmps(HDC dc, HWND h){
    RECT rc; GetClientRect(h,&rc);
    SelectObject(dc,g_fUIB); SetTextColor(dc,g_theme.text);
    RECT tr={S(24),S(116),rc.right-S(24),S(140)};
    DrawTextW(dc,(L"کارکنان بخش: "+s_mg->curCatName).c_str(),-1,&tr,
        DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
    if(s_mg->emps.empty()){
        SelectObject(dc,g_fUI); SetTextColor(dc,g_theme.textDim);
        RECT er={S(24),mgTopH()+S(30),rc.right-S(24),mgTopH()+S(70)};
        DrawTextW(dc,L"هیچ کارمندی در این بخش ثبت نشده است.",-1,&er,
            DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
        return;
    }
    for(size_t i=0;i<s_mg->emps.size();i++){
        EmpRow& e=s_mg->emps[i];
        RECT r=mgEmpRowRect(h,(int)i);
        bool hov=(s_mg->hot==MG_DETAIL_BASE+(int)i);
        gpRoundRect(dc,r,S(11),hov?g_theme.hover:g_theme.surface,g_theme.border,255);
        // photo placeholder (right)
        int av=S(40); RECT ph={r.right-S(14)-av,r.top+S(8),r.right-S(14),r.top+S(8)+av};
        gpGradRoundRect(dc,ph,av/2,g_theme.accent2,g_theme.accent,CLR_INVALID);
        std::wstring ini=e.user.fullname.empty()?L"؟":e.user.fullname.substr(0,1);
        SetBkMode(dc,TRANSPARENT); SelectObject(dc,g_fUIB); SetTextColor(dc,RGB(255,255,255));
        DrawTextW(dc,ini.c_str(),-1,&ph,DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_NOPREFIX);
        // name + username
        SetTextColor(dc,g_theme.text); SelectObject(dc,g_fUIB);
        RECT nr={r.left+S(150),r.top+S(6),r.right-S(64),r.top+S(28)};
        DrawTextW(dc,e.user.fullname.c_str(),-1,&nr,
            DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
        SelectObject(dc,g_fSmall); SetTextColor(dc,g_theme.textDim);
        RECT ur={r.left+S(150),r.top+S(28),r.right-S(64),r.bottom-S(6)};
        std::wstring sub=L"@"+e.user.username+L"   •   شیفت: "+
            (e.prof.shiftFrom.empty()?L"—":e.prof.shiftFrom);
        DrawTextW(dc,sub.c_str(),-1,&ur,
            DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
        // online status chip (middle)
        const wchar_t* stt = e.online?L"برخط":L"غیرفعال";
        COLORREF sc = e.online?RGB(40,170,90):RGB(150,150,150);
        RECT chip={r.left+S(140),r.top+S(16),r.left+S(220),r.bottom-S(16)};
        gpRoundRect(dc,chip,S(12),sc,CLR_INVALID,e.online?255:120);
        SetTextColor(dc,RGB(255,255,255)); SelectObject(dc,g_fSmall);
        DrawTextW(dc,stt,-1,&chip,DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
        // details button (left)
        RECT db={r.left+S(12),r.top+S(12),r.left+S(120),r.bottom-S(12)};
        gpRoundRect(dc,db,S(9),hov?g_theme.accent:g_theme.surface2,
            hov?g_theme.accent:g_theme.border,255);
        SetTextColor(dc,hov?g_theme.accentText:g_theme.text); SelectObject(dc,g_fUIB);
        DrawTextW(dc,L"جزئیات",-1,&db,DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
    }
}

static LRESULT CALLBACK manageProc(HWND h, UINT m, WPARAM w, LPARAM l){
    switch(m){
    case WM_CREATE: {
        s_mg=new ManageState();
        s_mg->view=VIEW_CATS; s_mg->hot=0; s_mg->filter=0; s_mg->autoId=true;
        s_mg->msgCol=g_theme.textDim;
        DWORD es=WS_CHILD|WS_TABSTOP|ES_AUTOHSCROLL;
        s_mg->eName=CreateWindowExW(0,L"EDIT",L"",es,0,0,10,10,h,(HMENU)MG_E_NAME,g_hInst,0);
        s_mg->eMgr =CreateWindowExW(0,L"EDIT",L"",es,0,0,10,10,h,(HMENU)MG_E_MGR,g_hInst,0);
        s_mg->eId  =CreateWindowExW(0,L"EDIT",L"",es,0,0,10,10,h,(HMENU)MG_E_ID,g_hInst,0);
        s_mg->bAdd =createFlatButton(h,MG_ADD_CAT,L"افزودن بخش",ICO_PLUS,BS_PRIMARY,0,0,10,10);
        s_mg->bAutoId=createFlatButton(h,MG_CB_AUTOID,L"\u2611 شناسه خودکار",0,BS_OUTLINE,0,0,10,10);
        s_mg->bAbc =createFlatButton(h,MG_FILTER_ABC,L"الفبایی",0,BS_OUTLINE,0,0,10,10);
        s_mg->bNew =createFlatButton(h,MG_FILTER_NEW,L"جدیدترین",0,BS_OUTLINE,0,0,10,10);
        s_mg->bBack=createFlatButton(h,MG_BACK,L"بازگشت",ICO_BACK,BS_OUTLINE,0,0,10,10);
        s_mg->bAddEmp=createFlatButton(h,MG_ADD_EMP,L"افزودن کارمند",ICO_USER,BS_PRIMARY,0,0,10,10);
        s_mg->bDesign=createFlatButton(h,MG_DESIGN,L"طراحی چاپ این بخش",ICO_PRINT,BS_OUTLINE,0,0,10,10);
        HWND eds[3]={s_mg->eName,s_mg->eMgr,s_mg->eId};
        for(HWND e:eds){ SendMessageW(e,WM_SETFONT,(WPARAM)g_fUI,TRUE); enableEnterNavigation(e); }
        mgLoadCats();
        return 0; }
    case WM_NCDESTROY: if(s_mg){ delete s_mg; s_mg=NULL; } break;
    case WM_SIZE: if(s_mg) mgLayout(h); return 0;
    case WM_APP_THEME: InvalidateRect(h,NULL,TRUE); return 0;
    case WM_CTLCOLOREDIT:{ HDC dc=(HDC)w; SetTextColor(dc,g_theme.inputText);
        SetBkColor(dc,g_theme.inputBg); return (LRESULT)g_brInput; }
    case WM_COMMAND:{
        if(!s_mg) return 0;
        int id=LOWORD(w);
        switch(id){
        case MG_ADD_CAT: mgDoAddCat(h); break;
        case MG_CB_AUTOID: s_mg->autoId=!s_mg->autoId;
            SetWindowTextW(s_mg->bAutoId,s_mg->autoId?L"\u2611 شناسه خودکار":L"\u2610 شناسه دستی");
            mgLayout(h); InvalidateRect(h,NULL,FALSE); break;
        case MG_FILTER_ABC: s_mg->filter=0; mgLoadCats(); InvalidateRect(h,NULL,FALSE); break;
        case MG_FILTER_NEW: s_mg->filter=1; mgLoadCats(); InvalidateRect(h,NULL,FALSE); break;
        case MG_BACK: s_mg->view=VIEW_CATS; mgLoadCats(); mgLayout(h); InvalidateRect(h,NULL,TRUE); break;
        case MG_ADD_EMP: mgAddEmployee(h); break;
        case MG_DESIGN: openPrintDesigner(g_hFrame,0); break;
        }
        return 0; }
    case WM_MOUSEMOVE:{ POINT pt={GET_X_LPARAM(l),GET_Y_LPARAM(l)};
        int hr=mgHit(h,pt);
        if(s_mg&&s_mg->hot!=hr){ s_mg->hot=hr; InvalidateRect(h,NULL,FALSE); }
        return 0; }
    case WM_LBUTTONDOWN:{ POINT pt={GET_X_LPARAM(l),GET_Y_LPARAM(l)};
        int id=mgHit(h,pt);
        if(id>=MG_DETAIL_BASE){ int i=id-MG_DETAIL_BASE;
            if(i<(int)s_mg->emps.size()) openEmpDetails(h,s_mg->emps[i]); }
        else if(id>=MG_CARD_BASE){ int i=id-MG_CARD_BASE;
            if(i<(int)s_mg->cats.size()){
                s_mg->curCat=s_mg->cats[i].id; s_mg->curCatName=s_mg->cats[i].name;
                s_mg->view=VIEW_EMPS; mgLoadEmps(); mgLayout(h);
                InvalidateRect(h,NULL,TRUE);
            }
        }
        return 0; }
    case WM_ERASEBKGND: return 1;
    case WM_PAINT:{ PAINTSTRUCT ps; HDC dc0=BeginPaint(h,&ps);
        RECT rc; GetClientRect(h,&rc);
        HDC dc=CreateCompatibleDC(dc0);
        HBITMAP bmp=CreateCompatibleBitmap(dc0,rc.right,rc.bottom);
        HGDIOBJ obm=SelectObject(dc,bmp);
        FillRect(dc,&rc,g_brBg); SetBkMode(dc,TRANSPARENT);
        SetTextColor(dc,g_theme.text); SelectObject(dc,g_fTitle);
        RECT tr={S(24),S(16),rc.right-S(24),S(50)};
        DrawTextW(dc,L"پنل مدیریت — بخش‌ها و کارکنان",-1,&tr,
            DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
        if(s_mg){
            if(s_mg->view==VIEW_CATS) mgPaintCats(dc,h); else mgPaintEmps(dc,h);
            if(!s_mg->msg.empty()){
                SetTextColor(dc,s_mg->msgCol); SelectObject(dc,g_fSmall);
                RECT mr={S(24),rc.bottom-S(30),rc.right-S(24),rc.bottom-S(8)};
                DrawTextW(dc,s_mg->msg.c_str(),-1,&mr,
                    DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
            }
        }
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