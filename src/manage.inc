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
    MG_MSG,                // «پیام به کارکنان» (open compose modal)
    MG_REQS,               // «درخواست تغییرات تنظیمات» (open requests modal)
    MG_PROFREQS,           // «درخواست‌های تغییر پروفایل» (open profile-requests modal)
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
    HWND bMsg, bReqs;           // «پیام به کارکنان» / «درخواست تغییرات تنظیمات»
    HWND bProfReqs;             // «درخواست‌های تغییر پروفایل»
    bool autoId;
    std::vector<DeptCat> cats;
    std::vector<EmpRow>  emps;
    std::wstring msg; COLORREF msgCol;
};
static ManageState* s_mg=NULL;
static HWND g_hManage=NULL;          // the live management screen window
static void mgRefreshReqBadge();     // forward decl (defined below)
static void openProfReqs(HWND owner); // forward decl (defined below)

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
static int mgTopH(){ return S(166); }   // room for add-bar + feature toolbar row
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
        // second toolbar row: «پیام به کارکنان» + «درخواست‌های تغییر تنظیمات»
        int ty=S(116);
        MoveWindow(s_mg->bMsg,  W-pad-S(200),            ty, S(200), fh, TRUE);
        MoveWindow(s_mg->bReqs, W-pad-S(200)-S(8)-S(280),ty, S(280), fh, TRUE);
        MoveWindow(s_mg->bProfReqs, W-pad-S(200)-S(8)-S(280)-S(8)-S(280), ty, S(280), fh, TRUE);
        ShowWindow(s_mg->bMsg,SW_SHOW); ShowWindow(s_mg->bReqs,SW_SHOW);
        ShowWindow(s_mg->bProfReqs,SW_SHOW);
    } else {
        ShowWindow(s_mg->eName,SW_HIDE); ShowWindow(s_mg->eMgr,SW_HIDE);
        ShowWindow(s_mg->bAutoId,SW_HIDE); ShowWindow(s_mg->bAdd,SW_HIDE);
        ShowWindow(s_mg->eId,SW_HIDE);
        ShowWindow(s_mg->bAbc,SW_HIDE); ShowWindow(s_mg->bNew,SW_HIDE);
        ShowWindow(s_mg->bMsg,SW_HIDE); ShowWindow(s_mg->bReqs,SW_HIDE);
        ShowWindow(s_mg->bProfReqs,SW_HIDE);
        int fh2=S(38);
        MoveWindow(s_mg->bBack,  W-pad-S(120), S(70), S(120), fh2, TRUE);
        MoveWindow(s_mg->bAddEmp,W-pad-S(120)-S(8)-S(170), S(70), S(170), fh2, TRUE);
        MoveWindow(s_mg->bDesign,pad, S(70), S(220), fh2, TRUE);
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

// ====================== «پیام به کارکنان» compose modal =====================
//  Pick a recipient (همه / a specific user), a severity (عادی/فوری/بحرانی) and
//  a message; pushes a typed cartable message that syncs to the recipients.
#define MGMSG_CLASS L"AzMgMsg"
enum { MM_CB_TO=3101, MM_E_TEXT, MM_R_NORMAL, MM_R_URGENT, MM_R_CRIT,
       MM_B_SEND, MM_B_CANCEL };
struct MsgState { HWND cTo, eText, rNormal, rUrgent, rCrit, bSend, bCancel;
                  std::vector<User> users; };
static HWND s_mm=NULL; static MsgState* s_ms=NULL;
static void mmClose(){ if(s_mm&&IsWindow(s_mm)){ HWND v=s_mm; s_mm=NULL; DestroyWindow(v);} }
static RECT mmCard(HWND h){ RECT rc; GetClientRect(h,&rc);
    int w=S(560),hh=S(420);
    RECT c={(rc.right-w)/2,(rc.bottom-hh)/2,(rc.right+w)/2,(rc.bottom+hh)/2}; return c; }
static void mmLayout(HWND h){
    if(!s_ms) return; RECT c=mmCard(h);
    int x=c.left+S(28), r=c.right-S(28), y=c.top+S(70), fh=S(36);
    MoveWindow(s_ms->cTo,   x, y, r-x, S(200), TRUE); y+=fh+S(16);
    MoveWindow(s_ms->rNormal, r-S(120), y, S(120), fh, TRUE);
    MoveWindow(s_ms->rUrgent, r-S(120)-S(130), y, S(130), fh, TRUE);
    MoveWindow(s_ms->rCrit,   r-S(120)-S(260), y, S(130), fh, TRUE); y+=fh+S(16);
    MoveWindow(s_ms->eText, x, y, r-x, c.bottom-S(80)-y, TRUE);
    int by=c.bottom-S(56), bw=S(150);
    MoveWindow(s_ms->bSend,   r-bw,        by, bw, S(40), TRUE);
    MoveWindow(s_ms->bCancel, r-bw-S(8)-bw,by, bw, S(40), TRUE);
}
static void mmDoSend(HWND h){
    if(!s_ms) return;
    int sel=(int)SendMessageW(s_ms->cTo,CB_GETCURSEL,0,0);
    std::wstring to=L"*";
    if(sel>0 && sel-1<(int)s_ms->users.size()) to=s_ms->users[sel-1].username;
    wchar_t buf[2048]={0}; GetWindowTextW(s_ms->eText,buf,2048);
    std::wstring text=trim(buf);
    if(text.empty()){
        MessageBoxW(h,L"متن پیام را وارد کنید.",L"پیام",MB_OK|MB_ICONINFORMATION); return; }
    int type=KMSG_NORMAL;
    if(SendMessageW(s_ms->rUrgent,BM_GETCHECK,0,0)==BST_CHECKED) type=KMSG_URGENT;
    else if(SendMessageW(s_ms->rCrit,BM_GETCHECK,0,0)==BST_CHECKED) type=KMSG_CRITICAL;
    pushMessageT(g_session.user.fullname.empty()?g_session.user.username:g_session.user.fullname,
                 to, text, type);
    MessageBoxW(h,L"پیام برای کارتابل کارکنان ارسال شد.",L"ارسال موفق",
                MB_OK|MB_ICONINFORMATION);
    mmClose();
}
static LRESULT CALLBACK mmProc(HWND h, UINT m, WPARAM w, LPARAM l){
    switch(m){
    case WM_CREATE:{
        s_ms=new MsgState();
        DWORD cbs=WS_CHILD|WS_VISIBLE|WS_TABSTOP|CBS_DROPDOWNLIST;
        s_ms->cTo=CreateWindowExW(0,L"COMBOBOX",L"",cbs,0,0,10,10,h,(HMENU)MM_CB_TO,g_hInst,0);
        s_ms->eText=CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",L"",
            WS_CHILD|WS_VISIBLE|WS_TABSTOP|ES_MULTILINE|ES_AUTOVSCROLL|WS_VSCROLL,
            0,0,10,10,h,(HMENU)MM_E_TEXT,g_hInst,0);
        DWORD rb=WS_CHILD|WS_VISIBLE|BS_AUTORADIOBUTTON|BS_RIGHTBUTTON|BS_RIGHT;
        s_ms->rNormal=CreateWindowExW(0,L"BUTTON",L"عادی",rb|WS_GROUP,0,0,10,10,h,(HMENU)MM_R_NORMAL,g_hInst,0);
        s_ms->rUrgent=CreateWindowExW(0,L"BUTTON",L"فوری",rb,0,0,10,10,h,(HMENU)MM_R_URGENT,g_hInst,0);
        s_ms->rCrit  =CreateWindowExW(0,L"BUTTON",L"بحرانی",rb,0,0,10,10,h,(HMENU)MM_R_CRIT,g_hInst,0);
        s_ms->bSend  =createFlatButton(h,MM_B_SEND,L"ارسال پیام",ICO_BELL,BS_PRIMARY,0,0,10,10);
        s_ms->bCancel=createFlatButton(h,MM_B_CANCEL,L"انصراف",ICO_X,BS_OUTLINE,0,0,10,10);
        SendMessageW(s_ms->cTo,CB_ADDSTRING,0,(LPARAM)L"همهٔ کارکنان");
        s_ms->users=loadUsers();
        for(auto&u:s_ms->users){
            std::wstring it=(u.fullname.empty()?u.username:u.fullname)+L" (@"+u.username+L")";
            SendMessageW(s_ms->cTo,CB_ADDSTRING,0,(LPARAM)it.c_str());
        }
        SendMessageW(s_ms->cTo,CB_SETCURSEL,0,0);
        SendMessageW(s_ms->rNormal,BM_SETCHECK,BST_CHECKED,0);
        HWND fc[6]={s_ms->cTo,s_ms->eText,s_ms->rNormal,s_ms->rUrgent,s_ms->rCrit,0};
        for(int i=0;fc[i];i++) SendMessageW(fc[i],WM_SETFONT,(WPARAM)g_fUI,TRUE);
        mmLayout(h);
        return 0; }
    case WM_SIZE: mmLayout(h); return 0;
    case WM_NCDESTROY: if(s_ms){ delete s_ms; s_ms=NULL; } s_mm=NULL;
        if(g_hFrame) InvalidateRect(g_hFrame,NULL,TRUE); return 0;
    case WM_ERASEBKGND: return 1;
    case WM_CTLCOLOREDIT: case WM_CTLCOLORLISTBOX:{ HDC dc=(HDC)w;
        SetTextColor(dc,g_theme.inputText); SetBkColor(dc,g_theme.inputBg);
        return (LRESULT)g_brInput; }
    case WM_CTLCOLORSTATIC:{ HDC dc=(HDC)w; HWND ctl=(HWND)l;
        if(s_ms && (ctl==s_ms->rNormal||ctl==s_ms->rUrgent||ctl==s_ms->rCrit)){
            SetTextColor(dc,g_theme.text); SetBkColor(dc,g_theme.surface);
            return (LRESULT)g_brSurface; }
        SetTextColor(dc,g_theme.inputText); SetBkColor(dc,g_theme.inputBg);
        return (LRESULT)g_brInput; }
    case WM_COMMAND:{ int id=LOWORD(w);
        if(id==MM_B_SEND) mmDoSend(h);
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
        { HBRUSH pb=CreateSolidBrush(g_theme.surface); FillRect(dc,&c,pb); DeleteObject(pb); }
        gpGradRoundRect(dc,c,S(18),g_theme.surfaceTop,g_theme.surface,g_theme.border);
        SetBkMode(dc,TRANSPARENT);
        SelectObject(dc,g_fTitle); SetTextColor(dc,g_theme.accent);
        RECT tr={c.left+S(28),c.top+S(16),c.right-S(28),c.top+S(50)};
        DrawTextW(dc,L"پیام به کارکنان",-1,&tr,
            DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
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

// ============== «درخواست‌های تغییر تنظیمات» (settings-change requests) =======
#define MGREQ_CLASS L"AzMgReq"
struct ReqState { std::vector<SetReq> reqs; int hot; };
static HWND s_mr=NULL; static ReqState* s_rs=NULL;
static void mrClose(){ if(s_mr&&IsWindow(s_mr)){ HWND v=s_mr; s_mr=NULL; DestroyWindow(v);} }
static RECT mrCard(HWND h){ RECT rc; GetClientRect(h,&rc);
    int w=S(760),hh=S(560);
    RECT c={(rc.right-w)/2,(rc.bottom-hh)/2,(rc.right+w)/2,(rc.bottom+hh)/2}; return c; }
static LRESULT CALLBACK mrProc(HWND h, UINT m, WPARAM w, LPARAM l){
    switch(m){
    case WM_CREATE: s_rs=new ReqState(); s_rs->reqs=loadSetReqs(); s_rs->hot=0; return 0;
    case WM_NCDESTROY: if(s_rs){ delete s_rs; s_rs=NULL; } s_mr=NULL;
        if(g_hFrame) InvalidateRect(g_hFrame,NULL,TRUE); return 0;
    case WM_ERASEBKGND: return 1;
    case WM_LBUTTONDOWN:{ POINT pt={GET_X_LPARAM(l),GET_Y_LPARAM(l)};
        RECT c=mrCard(h);
        RECT cb={c.left+S(14),c.top+S(14),c.left+S(40),c.top+S(40)};
        if(!PtInRect(&c,pt)||PtInRect(&cb,pt)){ mrClose(); return 0; }
        // «علامت‌گذاری همه به‌عنوان دیده‌شده» button
        int by=c.bottom-S(54), bw=S(220);
        RECT mb={c.left+S(24),by,c.left+S(24)+bw,by+S(40)};
        if(PtInRect(&mb,pt)){ markSetReqsSeen(); if(s_rs) s_rs->reqs=loadSetReqs();
            mgRefreshReqBadge(); InvalidateRect(h,NULL,FALSE);
            if(g_hManage) InvalidateRect(g_hManage,NULL,FALSE); }
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
        SelectObject(dc,g_fTitle); SetTextColor(dc,g_theme.danger);
        RECT tr={c.left+S(24),c.top+S(16),c.right-S(54),c.top+S(50)};
        DrawTextW(dc,L"درخواست‌های تغییر تنظیمات کارکنان",-1,&tr,
            DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
        // list
        int y=c.top+S(64);
        if(!s_rs || s_rs->reqs.empty()){
            SelectObject(dc,g_fUI); SetTextColor(dc,g_theme.textDim);
            RECT er={c.left+S(24),y,c.right-S(24),y+S(40)};
            DrawTextW(dc,L"درخواستی ثبت نشده است.",-1,&er,
                DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
        } else {
            for(size_t i=0;i<s_rs->reqs.size() && y<c.bottom-S(80);i++){
                SetReq& q=s_rs->reqs[i];
                int ch=S(76);
                RECT row={c.left+S(20),y,c.right-S(20),y+ch};
                gpRoundRect(dc,row,S(10),q.seen?g_theme.surface:g_theme.surface2,
                    q.seen?g_theme.border:g_theme.danger,255);
                if(!q.seen){ RECT dot={row.right-S(24),row.top+S(10),row.right-S(12),row.top+S(22)};
                    gpRoundRect(dc,dot,S(6),g_theme.danger,CLR_INVALID,255); }
                SelectObject(dc,g_fUIB); SetTextColor(dc,g_theme.text);
                RECT l1={row.left+S(14),row.top+S(8),row.right-S(30),row.top+S(30)};
                std::wstring head=L"کاربر: "+(q.user.empty()?std::wstring(L"—"):q.user)+
                    L"   •   سیستم: "+(q.system.empty()?std::wstring(L"—"):q.system)+
                    L"   •   "+q.time;
                DrawTextW(dc,head.c_str(),-1,&l1,
                    DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX|DT_END_ELLIPSIS);
                SelectObject(dc,g_fUI); SetTextColor(dc,g_theme.textDim);
                RECT l2={row.left+S(14),row.top+S(32),row.right-S(14),row.top+S(54)};
                DrawTextW(dc,(L"تغییر: "+q.change).c_str(),-1,&l2,
                    DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX|DT_END_ELLIPSIS);
                RECT l3={row.left+S(14),row.top+S(52),row.right-S(14),row.bottom-S(4)};
                DrawTextW(dc,(L"پروفایل: "+(q.profile.empty()?std::wstring(L"—"):q.profile)).c_str(),
                    -1,&l3,DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX|DT_END_ELLIPSIS);
                y+=ch+S(8);
            }
        }
        // mark-all button
        int by=c.bottom-S(54), bw=S(220);
        RECT mb={c.left+S(24),by,c.left+S(24)+bw,by+S(40)};
        gpRoundRect(dc,mb,S(9),g_theme.accent,g_theme.accent,255);
        SetTextColor(dc,g_theme.accentText); SelectObject(dc,g_fUIB);
        DrawTextW(dc,L"علامت‌گذاری همه به‌عنوان دیده‌شده",-1,&mb,
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
        SelectObject(dc,g_fTitle); SetTextColor(dc,g_theme.danger);
        RECT tr={c.left+S(24),c.top+S(16),c.right-S(54),c.top+S(50)};
        DrawTextW(dc,L"درخواست‌های تغییر پروفایل کارکنان",-1,&tr,
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
                gpRoundRect(dc,row,S(10),selected?g_theme.surface2:g_theme.surface,
                    selected?g_theme.accent:g_theme.danger,255);
                // text (RIGHT side)
                SelectObject(dc,g_fUIB); SetTextColor(dc,g_theme.text);
                RECT l1={row.left+S(230),row.top+S(8),row.right-S(14),row.top+S(32)};
                std::wstring head=L"کاربر: "+(q.user.empty()?std::wstring(L"—"):q.user)+
                    L"   •   "+q.time;
                DrawTextW(dc,head.c_str(),-1,&l1,
                    DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX|DT_END_ELLIPSIS);
                SelectObject(dc,g_fUI); SetTextColor(dc,g_theme.textDim);
                RECT l2={row.left+S(230),row.top+S(32),row.right-S(14),row.top+S(56)};
                std::wstring nm=L"نام: "+q.oldName+L"  ←  "+q.newName;
                DrawTextW(dc,nm.c_str(),-1,&l2,
                    DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX|DT_END_ELLIPSIS);
                RECT l3={row.left+S(230),row.top+S(54),row.right-S(14),row.bottom-S(6)};
                std::wstring ph = q.newPhoto.empty()? std::wstring(L"عکس: بدون تغییر")
                                                    : std::wstring(L"عکس: جدید پیوست شد");
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

// ---- update the «درخواست‌ها» button label with an unseen-count badge -------
static void mgRefreshReqBadge(){
    if(!s_mg || !s_mg->bReqs) return;
    int n=unseenSetReqCount();
    std::wstring lbl=L"درخواست‌های تغییر تنظیمات";
    if(n>0) lbl=L"● ("+toFaDigits(std::to_wstring(n))+L") "+lbl;
    SetWindowTextW(s_mg->bReqs,lbl.c_str());
    InvalidateRect(s_mg->bReqs,NULL,TRUE);
    // profile-change requests badge
    if(s_mg->bProfReqs){
        int pn=unseenProfReqCount();
        std::wstring pl=L"درخواست‌های تغییر پروفایل";
        if(pn>0) pl=L"● ("+toFaDigits(std::to_wstring(pn))+L") "+pl;
        SetWindowTextW(s_mg->bProfReqs,pl.c_str());
        InvalidateRect(s_mg->bProfReqs,NULL,TRUE);
    }
}

// ---- painting --------------------------------------------------------------
static void mgPaintCats(HDC dc, HWND h){
    RECT rc; GetClientRect(h,&rc);
    // labels above the add bar
    SelectObject(dc,g_fSmall); SetTextColor(dc,g_theme.textDim);
    RECT lr={rc.right-S(244),S(50),rc.right-S(24),S(68)};
    DrawTextW(dc,L"افزودن بخش جدید: نام • مدیر • شناسه (خودکار/دستی)",-1,&lr,
        DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
    // small red notification badge above the «درخواست‌ها» button when there are
    // unseen settings-change requests (the count also shows in the button text).
    {
        int n=unseenSetReqCount();
        if(n>0){
            int pad=S(24), W=rc.right, ty=S(116), fh=S(36);
            // bReqs button: x = W-pad-200-8-280 .. width 280 ; badge at its TOP-RIGHT
            int bx=W-pad-S(200)-S(8)-S(280);
            int bcx=bx+S(280)-S(6), bcy=ty-S(2);
            int r=S(11);
            RECT badge={bcx-r,bcy-r,bcx+r,bcy+r};
            gpRoundRect(dc,badge,r,g_theme.danger,CLR_INVALID,255);
            SetBkMode(dc,TRANSPARENT); SelectObject(dc,g_fSmall);
            SetTextColor(dc,RGB(255,255,255));
            DrawTextW(dc,toFaDigits(std::to_wstring(n>99?99:n)).c_str(),-1,&badge,
                DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);
        }
    }
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
        s_mg->bMsg =createFlatButton(h,MG_MSG,L"پیام به کارکنان",ICO_BELL,BS_PRIMARY,0,0,10,10);
        s_mg->bReqs=createFlatButton(h,MG_REQS,L"درخواست‌های تغییر تنظیمات",ICO_GEAR,BS_DANGER,0,0,10,10);
        s_mg->bProfReqs=createFlatButton(h,MG_PROFREQS,L"درخواست‌های تغییر پروفایل",ICO_USER,BS_DANGER,0,0,10,10);
        HWND eds[3]={s_mg->eName,s_mg->eMgr,s_mg->eId};
        for(HWND e:eds){ SendMessageW(e,WM_SETFONT,(WPARAM)g_fUI,TRUE); enableEnterNavigation(e); }
        g_hManage=h;
        mgLoadCats();
        mgRefreshReqBadge();
        return 0; }
    case WM_NCDESTROY: if(g_hManage==h) g_hManage=NULL;
        if(s_mg){ delete s_mg; s_mg=NULL; } break;
    case WM_SIZE: if(s_mg) mgLayout(h); return 0;
    case WM_APP_THEME: InvalidateRect(h,NULL,TRUE); return 0;
    case WM_CTLCOLOREDIT:{ HDC dc=(HDC)w; SetTextColor(dc,g_theme.inputText);
        SetBkColor(dc,g_theme.inputBg); return (LRESULT)g_brInput; }
    case WM_CTLCOLORLISTBOX:{ HDC dc=(HDC)w; SetTextColor(dc,g_theme.inputText);
        SetBkColor(dc,g_theme.inputBg); return (LRESULT)g_brInput; }
    case WM_CTLCOLORSTATIC:{ HDC dc=(HDC)w; SetTextColor(dc,g_theme.inputText);
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
        case MG_MSG:  openMsgCompose(h); break;
        case MG_REQS: openSetReqs(h); break;
        case MG_PROFREQS: openProfReqs(h); break;
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