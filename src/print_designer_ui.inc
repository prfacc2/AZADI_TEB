// ============================================================================
//  print_designer_ui.inc — UI for the print designer (release 1.4.0, §3/§4)
//  Included into print_designer.cpp. Contains:
//    • the section picker dialog (§3.1)
//    • the designer window: canvas + inspector + template strip (§3.3+)
//    • the restore-design window (§4)
//  Real WYSIWYG GDI+ canvas; drag/resize/rotate; undo/redo; .aztpl I/O.
// ============================================================================

using namespace Gdiplus;

// ---------------------------------------------------------------- ids --------
namespace {

#define IDC_DZ_SEARCH   2001
#define IDC_DZ_LIST     2002
#define IDC_DZ_OK       2003
#define IDC_DZ_CANCEL   2004

#define IDC_DSG_SAVE    3001
#define IDC_DSG_SAVEAS  3002
#define IDC_DSG_DOWNLOAD 3003
#define IDC_DSG_EXIT    3004
#define IDC_DSG_PAPER   3005
#define IDC_DSG_ORIENT  3006
#define IDC_DSG_ZOOM    3007

#define IDC_RST_DROP    4001
#define IDC_RST_PICK    4002
#define IDC_RST_APPLY   4003
#define IDC_RST_CANCEL  4004
#define IDC_RST_LIST    4005

// ---- selected sections passed from picker -> designer ----------------------
struct DesignerLaunch {
    std::vector<int> sectionIds;
};

// =====================================================================
//  SECTION PICKER (§3.1)
// =====================================================================
struct PickState {
    std::vector<Section> all, shown;
    std::vector<int>     selected;   // section ids
    HWND hSearch, hList, hOk, hCount;
    bool result;
};

static void picker_reload(PickState* st){
    std::wstring q;
    wchar_t buf[128]={0}; GetWindowTextW(st->hSearch,buf,127); q=buf;
    Sections_Find(q, st->shown);
    ListView_DeleteAllItems(st->hList);
    for(size_t i=0;i<st->shown.size();++i){
        const Section& s=st->shown[i];
        LVITEMW it; ZeroMemory(&it,sizeof(it));
        it.mask=LVIF_TEXT|LVIF_PARAM; it.iItem=(int)i; it.iSubItem=0;
        it.lParam=s.id; it.pszText=(LPWSTR)s.code.c_str();
        ListView_InsertItem(st->hList,&it);
        ListView_SetItemText(st->hList,(int)i,1,(LPWSTR)s.name_fa.c_str());
        ListView_SetItemText(st->hList,(int)i,2,(LPWSTR)Sections_KindLabel(s.kind));
        ListView_SetItemText(st->hList,(int)i,3,
            (LPWSTR)(s.is_active?L"\u0641\u0639\u0627\u0644":L"\u063a\u06cc\u0631\u0641\u0639\u0627\u0644"));
        bool chk=std::find(st->selected.begin(),st->selected.end(),s.id)!=st->selected.end();
        ListView_SetCheckState(st->hList,(int)i,chk);
    }
}
static void picker_syncSel(PickState* st){
    st->selected.clear();
    int n=ListView_GetItemCount(st->hList);
    for(int i=0;i<n;i++){
        if(ListView_GetCheckState(st->hList,i)){
            LVITEMW it; ZeroMemory(&it,sizeof(it)); it.mask=LVIF_PARAM; it.iItem=i;
            ListView_GetItem(st->hList,&it);
            st->selected.push_back((int)it.lParam);
        }
    }
    wchar_t b[64]; swprintf(b,64,
        L"\u0628\u062e\u0634\u200c\u0647\u0627\u06cc \u0627\u0646\u062a\u062e\u0627\u0628 \u0634\u062f\u0647: %d",
        (int)st->selected.size());
    SetWindowTextW(st->hCount,b);
    EnableWindow(st->hOk, !st->selected.empty());
}

static LRESULT CALLBACK PickerProc(HWND h,UINT m,WPARAM w,LPARAM l){
    PickState* st=(PickState*)GetWindowLongPtrW(h,GWLP_USERDATA);
    switch(m){
    case WM_CREATE: return 0;
    case WM_COMMAND:
        if(LOWORD(w)==IDC_DZ_SEARCH && HIWORD(w)==EN_CHANGE){ picker_reload(st); picker_syncSel(st); }
        else if(LOWORD(w)==IDC_DZ_OK){ st->result=true; DestroyWindow(h); }
        else if(LOWORD(w)==IDC_DZ_CANCEL){ st->result=false; DestroyWindow(h); }
        return 0;
    case WM_NOTIFY:{
        LPNMHDR nh=(LPNMHDR)l;
        if(nh->idFrom==IDC_DZ_LIST && nh->code==LVN_ITEMCHANGED){
            NMLISTVIEW* nv=(NMLISTVIEW*)l;
            if(nv->uChanged & LVIF_STATE) picker_syncSel(st);
        }
        return 0;
    }
    case WM_CTLCOLORSTATIC: {
        HDC dc=(HDC)w; SetBkColor(dc,g_theme.surface); SetTextColor(dc,g_theme.text);
        return (LRESULT)g_brSurface;
    }
    case WM_ERASEBKGND:{ RECT rc; GetClientRect(h,&rc); FillRect((HDC)w,&rc,g_brSurface); return 1; }
    case WM_CLOSE: st->result=false; DestroyWindow(h); return 0;
    case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(h,m,w,l);
}

// Returns true and fills out.sectionIds if user pressed «ادامه».
static bool RunSectionPicker(HWND owner, DesignerLaunch& out){
    static bool reg=false;
    const wchar_t* CLS=L"AzSectionPicker";
    if(!reg){ WNDCLASSW wc={0}; wc.lpfnWndProc=PickerProc; wc.hInstance=g_hInst;
        wc.hCursor=LoadCursor(NULL,IDC_ARROW); wc.lpszClassName=CLS;
        wc.hbrBackground=g_brSurface; RegisterClassW(&wc); reg=true; }
    int W=S(640),H=S(520);
    RECT pr; GetWindowRect(owner,&pr);
    int x=pr.left+((pr.right-pr.left)-W)/2, y=pr.top+((pr.bottom-pr.top)-H)/2;
    PickState st; st.result=false;
    HWND h=CreateWindowExW(WS_EX_DLGMODALFRAME,CLS,
        L"\u0627\u0646\u062a\u062e\u0627\u0628 \u0628\u062e\u0634\u200c\u0647\u0627 \u0628\u0631\u0627\u06cc \u062f\u06cc\u0632\u0627\u06cc\u0646 \u0686\u0627\u067e",
        WS_POPUP|WS_CAPTION|WS_SYSMENU,x,y,W,H,owner,NULL,g_hInst,NULL);
    SetWindowLongPtrW(h,GWLP_USERDATA,(LONG_PTR)&st);

    int mgn=S(12), cw=W-2*mgn-S(16);
    HWND cap=CreateWindowExW(0,L"STATIC",
        L"\u062c\u0633\u062a\u062c\u0648 \u0628\u0627 \u0646\u0627\u0645 \u0628\u062e\u0634 \u06cc\u0627 \u06a9\u062f \u0628\u062e\u0634",
        WS_CHILD|WS_VISIBLE|SS_RIGHT,mgn,mgn,cw,S(18),h,NULL,g_hInst,NULL);
    SendMessageW(cap,WM_SETFONT,(WPARAM)g_fSmall,TRUE);
    st.hSearch=CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",L"",
        WS_CHILD|WS_VISIBLE|ES_AUTOHSCROLL,mgn,mgn+S(20),cw,S(26),h,
        (HMENU)IDC_DZ_SEARCH,g_hInst,NULL);
    SendMessageW(st.hSearch,WM_SETFONT,(WPARAM)g_fUI,TRUE);
    st.hList=CreateWindowExW(WS_EX_CLIENTEDGE,WC_LISTVIEWW,L"",
        WS_CHILD|WS_VISIBLE|LVS_REPORT|LVS_SINGLESEL,
        mgn,mgn+S(54),cw,H-S(54)-S(96),h,(HMENU)IDC_DZ_LIST,g_hInst,NULL);
    ListView_SetExtendedListViewStyle(st.hList,
        LVS_EX_CHECKBOXES|LVS_EX_FULLROWSELECT|LVS_EX_GRIDLINES);
    SendMessageW(st.hList,WM_SETFONT,(WPARAM)g_fUI,TRUE);
    LVCOLUMNW col; ZeroMemory(&col,sizeof(col)); col.mask=LVCF_TEXT|LVCF_WIDTH;
    struct C{const wchar_t* t;int w;} cols[]={
        {L"\u06a9\u062f",S(70)},
        {L"\u0646\u0627\u0645 \u0628\u062e\u0634",S(240)},
        {L"\u0646\u0648\u0639",S(110)},
        {L"\u0641\u0639\u0627\u0644",S(80)} };
    for(int i=0;i<4;i++){ col.pszText=(LPWSTR)cols[i].t; col.cx=cols[i].w;
        ListView_InsertColumn(st.hList,i,&col); }
    st.hCount=CreateWindowExW(0,L"STATIC",L"",WS_CHILD|WS_VISIBLE|SS_RIGHT,
        mgn,H-S(82),cw,S(20),h,NULL,g_hInst,NULL);
    SendMessageW(st.hCount,WM_SETFONT,(WPARAM)g_fSmall,TRUE);
    st.hOk=createFlatButton(h,IDC_DZ_OK,
        L"\u0627\u062f\u0627\u0645\u0647",ICO_CHECK,BS_PRIMARY,
        W-mgn-S(120),H-S(56),S(120),S(34));
    EnableWindow(st.hOk,FALSE);
    createFlatButton(h,IDC_DZ_CANCEL,
        L"\u0627\u0646\u0635\u0631\u0627\u0641",ICO_X,BS_OUTLINE,
        mgn,H-S(56),S(110),S(34));

    Sections_Init();
    picker_reload(&st); picker_syncSel(&st);
    ShowWindow(h,SW_SHOW);
    EnableWindow(owner,FALSE);
    MSG msg;
    while(GetMessageW(&msg,NULL,0,0)){
        if(!IsWindow(h)) break;
        if(IsDialogMessageW(h,&msg)) continue;
        TranslateMessage(&msg); DispatchMessageW(&msg);
    }
    EnableWindow(owner,TRUE); SetForegroundWindow(owner);
    out.sectionIds=st.selected;
    return st.result && !st.selected.empty();
}

// =====================================================================
//  DESIGNER WINDOW
// =====================================================================
struct DesignerState {
    PrintDesign design;
    std::vector<int> targetSections;     // from picker / for save
    int  selId;                          // selected item id (-1 none)
    std::vector<int> multiSel;
    double zoom;                         // 0.25 .. 4.0
    double panX, panY;                   // px offset of paper origin in canvas
    bool dirty;
    bool showGrid;
    bool dragging; POINT dragStart; double dragItemX0, dragItemY0;
    bool panning; POINT panStart; double pan0x, pan0y;
    UndoStack<PrintDesign> undo;
    HWND hPaperCombo, hOrientCombo, hZoom;
    HWND hMain;
    DesignerState():selId(-1),zoom(1.0),panX(40),panY(70),dirty(false),
        showGrid(true),dragging(false),panning(false),undo(100){}
};

static double pxPerMm(DesignerState* st){ return 96.0/25.4 * st->zoom; }

static PrintItem* findItem(DesignerState* st,int id){
    for(auto& it:st->design.items) if(it.id==id) return &it;
    return nullptr;
}
static int nextItemId(DesignerState* st){
    int mx=0; for(auto& it:st->design.items) if(it.id>mx) mx=it.id; return mx+1;
}
static void commit(DesignerState* st){ st->dirty=true; st->undo.push(st->design); }

// map paper-mm -> canvas px
static POINT mmToPx(DesignerState* st,double mx,double my){
    double k=pxPerMm(st);
    POINT p; p.x=(LONG)(st->panX + mx*k); p.y=(LONG)(st->panY + my*k); return p;
}
static void pxToMm(DesignerState* st,int px,int py,double& mx,double& my){
    double k=pxPerMm(st);
    mx=(px-st->panX)/k; my=(py-st->panY)/k;
}

// hit-test honoring frame special rule (§3.10)
static int hitTest(DesignerState* st,int px,int py){
    double mx,my; pxToMm(st,px,py,mx,my);
    // descending z, frames last
    std::vector<PrintItem*> order;
    for(auto& it:st->design.items) if(!it.is_frame) order.push_back(&it);
    std::sort(order.begin(),order.end(),
        [](PrintItem*a,PrintItem*b){return a->z>b->z;});
    for(auto* it:order){
        if(mx>=it->x && mx<=it->x+it->w && my>=it->y && my<=it->y+it->h)
            return it->id;
    }
    // frames: hollow stroke only
    double k=pxPerMm(st);
    for(auto& it:st->design.items) if(it.is_frame){
        double sw=(it.borderWidth+2)/k;     // stroke thickness in mm
        bool nearL=fabs(mx-it.x)<=sw, nearR=fabs(mx-(it.x+it.w))<=sw;
        bool nearT=fabs(my-it.y)<=sw, nearB=fabs(my-(it.y+it.h))<=sw;
        bool inside=mx>=it.x-sw && mx<=it.x+it.w+sw && my>=it.y-sw && my<=it.y+it.h+sw;
        if(inside && (nearL||nearR||nearT||nearB)) return it.id;
    }
    return -1;
}

// forward
static void designer_layout(HWND h);
static void designer_buildInspector(HWND h);
extern "C" {}

// ---- GDI+ rendering of one item into canvas px coords ----------------------
static std::wstring sampleValue(const std::wstring& field){
    if(field==L"firstName") return L"\u0639\u0644\u06cc";
    if(field==L"lastName")  return L"\u0631\u0636\u0627\u06cc\u06cc";
    if(field==L"nationalCode") return L"\u06f0\u06f0\u06f1\u06f2\u06f3\u06f4\u06f5\u06f6\u06f7\u06f8";
    if(field==L"insurance") return L"\u062a\u0623\u0645\u06cc\u0646 \u0627\u062c\u062a\u0645\u0627\u0639\u06cc";
    if(field==L"suppInsurance") return L"\u062f\u0627\u0646\u0627";
    if(field==L"doctor")    return L"\u062f\u06a9\u062a\u0631 \u0645\u062d\u0645\u062f\u06cc";
    if(field==L"total")     return L"\u06f5\u06f0\u06f0\u060c\u06f0\u06f0\u06f0";
    if(field==L"paid")      return L"\u06f4\u06f5\u06f0\u060c\u06f0\u06f0\u06f0";
    if(field==L"receiptNo") return L"\u06f1\u06f2\u06f3\u06f4";
    if(field==L"date")      return FormatJalaliPersian(0);
    if(field==L"time")      return L"\u06f1\u06f0:\u06f3\u06f0";
    return L"\u2014";
}

static void drawItemGdip(Graphics& g,DesignerState* st,const PrintItem& it,bool selected){
    double k=pxPerMm(st);
    REAL x=(REAL)(st->panX+it.x*k), y=(REAL)(st->panY+it.y*k);
    REAL w=(REAL)(it.w*k), hgt=(REAL)(it.h*k);
    GraphicsState gs=g.Save();
    if(it.rot!=0){
        g.TranslateTransform(x+w/2,y+hgt/2);
        g.RotateTransform((REAL)it.rot);
        g.TranslateTransform(-(x+w/2),-(y+hgt/2));
    }
    Color tc(255,(it.textColor>>16)&0xFF,(it.textColor>>8)&0xFF,it.textColor&0xFF);
    Color bc(255,(it.borderColor>>16)&0xFF,(it.borderColor>>8)&0xFF,it.borderColor&0xFF);
    // fill
    if(!it.fillTransparent && it.type!=PIT_HLINE && it.type!=PIT_VLINE){
        Color fc(255,(it.fillColor>>16)&0xFF,(it.fillColor>>8)&0xFF,it.fillColor&0xFF);
        SolidBrush fb(fc); g.FillRectangle(&fb,x,y,w,hgt);
    }
    switch(it.type){
    case PIT_HLINE:{ Pen p(bc,(REAL)std::max(1.0,it.borderWidth*st->zoom));
        g.DrawLine(&p,x,y+hgt/2,x+w,y+hgt/2); break; }
    case PIT_VLINE:{ Pen p(bc,(REAL)std::max(1.0,it.borderWidth*st->zoom));
        g.DrawLine(&p,x+w/2,y,x+w/2,y+hgt); break; }
    case PIT_RECT: case PIT_FRAME:{
        Pen p(bc,(REAL)std::max(1.0,it.borderWidth*st->zoom));
        g.DrawRectangle(&p,x,y,w,hgt); break; }
    case PIT_QR:{
        Pen p(bc,1.0f); g.DrawRectangle(&p,x,y,w,hgt);
        // a crude QR finder-pattern preview
        SolidBrush b(Color(255,0,0,0));
        REAL cell=w/7;
        for(int i=0;i<7;i++)for(int j=0;j<7;j++)
            if(((i+j)%2)==0 || i==0||j==0||i==6||j==6)
                g.FillRectangle(&b,x+i*cell,y+j*cell,cell,cell);
        break; }
    case PIT_LOGO: case PIT_IMAGE: case PIT_PHOTO:{
        Pen p(bc,1.0f); g.DrawRectangle(&p,x,y,w,hgt);
        FontFamily ff(L"Vazirmatn"); Font f(&ff,(REAL)(std::min(w,hgt)*0.5),
            FontStyleRegular,UnitPixel);
        SolidBrush tb(Color(255,150,150,150));
        StringFormat sf; sf.SetAlignment(StringAlignmentCenter);
        sf.SetLineAlignment(StringAlignmentCenter);
        const wchar_t* gl=(it.type==PIT_PHOTO)?L"\u0639":(it.type==PIT_LOGO)?L"\u0644":L"\u062a";
        RectF rf(x,y,w,hgt); g.DrawString(gl,-1,&f,rf,&sf,&tb);
        break; }
    case PIT_APPTNO: case PIT_LABEL: case PIT_FIELD: default:{
        std::wstring text;
        if(it.type==PIT_APPTNO) text=L"###";
        else if(it.type==PIT_FIELD) text=it.prefix+sampleValue(it.field)+it.suffix;
        else text=it.text;
        FontFamily ff(it.fontName.c_str());
        int style=FontStyleRegular;
        if(it.bold) style|=FontStyleBold;
        if(it.italic) style|=FontStyleItalic;
        Font f(&ff,(REAL)(it.fontPt*1.333*st->zoom),(FontStyle)style,UnitPixel);
        SolidBrush tb(tc);
        StringFormat sf;
        sf.SetFormatFlags(StringFormatFlagsDirectionRightToLeft);
        if(it.align==1) sf.SetAlignment(StringAlignmentCenter);
        else if(it.align==2) sf.SetAlignment(StringAlignmentFar);  // visual left in RTL
        else sf.SetAlignment(StringAlignmentNear);                 // right
        sf.SetLineAlignment(StringAlignmentCenter);
        RectF rf(x,y,w,hgt);
        g.DrawString(text.c_str(),-1,&f,rf,&sf,&tb);
        break; }
    }
    g.Restore(gs);
    if(selected){
        Pen sp(Color(255,0,120,255),1.5f);
        g.DrawRectangle(&sp,x-1,y-1,w+2,hgt+2);
    }
}

static void designer_paint(HWND h){
    DesignerState* st=(DesignerState*)GetWindowLongPtrW(h,GWLP_USERDATA);
    if(!st) return;
    RECT rc; GetClientRect(h,&rc);
    PAINTSTRUCT ps; HDC dc0=BeginPaint(h,&ps);
    uikit::MemDC mem(dc0,rc.right,rc.bottom);
    HBRUSH bg=CreateSolidBrush(g_theme.bg); FillRect(mem.dc,&rc,bg); DeleteObject(bg);

    int topBar=S(44), strip=S(60);
    int canvasTop=topBar+strip;
    int inspW=(int)((rc.right)*0.30);
    RECT canvas={0,canvasTop,rc.right-inspW,rc.bottom};

    // canvas background
    HBRUSH cb=CreateSolidBrush(blendColor(g_theme.bg,g_theme.surface,40));
    FillRect(mem.dc,&canvas,cb); DeleteObject(cb);

    // GDI+ on a clipped region
    {
        Graphics g(mem.dc);
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        g.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);
        Region clip(Rect(canvas.left,canvas.top,canvas.right-canvas.left,
                         canvas.bottom-canvas.top));
        g.SetClip(&clip);
        double k=pxPerMm(st);
        REAL px=(REAL)st->panX, py=(REAL)st->panY;
        REAL pw=(REAL)(st->design.paperW*k), ph=(REAL)(st->design.paperH*k);
        // page shadow + white page
        SolidBrush sh(Color(40,0,0,0)); g.FillRectangle(&sh,px+4,py+4,pw,ph);
        SolidBrush wp(Color(255,255,255,255)); g.FillRectangle(&wp,px,py,pw,ph);
        Pen pe(Color(255,180,180,180),1.0f); g.DrawRectangle(&pe,px,py,pw,ph);
        // grid
        if(st->showGrid){
            Pen gp(Color(255,235,235,235),1.0f);
            for(double mx=0;mx<=st->design.paperW;mx+=5){
                REAL X=(REAL)(px+mx*k); g.DrawLine(&gp,X,py,X,py+ph);
            }
            for(double my=0;my<=st->design.paperH;my+=5){
                REAL Y=(REAL)(py+my*k); g.DrawLine(&gp,px,Y,px+pw,Y);
            }
        }
        // items in ascending z
        std::vector<const PrintItem*> order;
        for(auto& it:st->design.items) order.push_back(&it);
        std::sort(order.begin(),order.end(),
            [](const PrintItem*a,const PrintItem*b){return a->z<b->z;});
        for(auto* it:order){
            bool sel=(it->id==st->selId) ||
                std::find(st->multiSel.begin(),st->multiSel.end(),it->id)!=st->multiSel.end();
            drawItemGdip(g,st,*it,sel);
        }
    }
    mem.blitTo(dc0);
    EndPaint(h,&ps);
}

// Build the inspector child controls for the current selection.
static HWND g_inspectorHwnds[64]; static int g_inspectorCount=0;
static void clearInspector(){
    for(int i=0;i<g_inspectorCount;i++) if(g_inspectorHwnds[i]&&IsWindow(g_inspectorHwnds[i]))
        DestroyWindow(g_inspectorHwnds[i]);
    g_inspectorCount=0;
}

static void designer_buildInspector(HWND h){
    DesignerState* st=(DesignerState*)GetWindowLongPtrW(h,GWLP_USERDATA);
    if(!st) return;
    clearInspector();
    RECT rc; GetClientRect(h,&rc);
    int inspW=(int)(rc.right*0.30);
    int x0=rc.right-inspW+S(8), y=S(44)+S(60)+S(10), w=inspW-S(16);
    auto add=[&](HWND c){ if(g_inspectorCount<64) g_inspectorHwnds[g_inspectorCount++]=c; };
    auto label=[&](const wchar_t* t,int ww=0){
        HWND c=CreateWindowExW(0,L"STATIC",t,WS_CHILD|WS_VISIBLE|SS_RIGHT,
            x0,y,ww?ww:w,S(18),h,NULL,g_hInst,NULL);
        SendMessageW(c,WM_SETFONT,(WPARAM)g_fSmall,TRUE); add(c); };

    PrintItem* it = (st->selId>=0)? findItem(st,st->selId):nullptr;
    if(!it){
        // palette header (when nothing selected)
        HWND c=CreateWindowExW(0,L"STATIC",
            L"\u0622\u06cc\u062a\u0645\u200c\u0647\u0627 \u2014 \u0628\u0631\u0627\u06cc \u0627\u0641\u0632\u0648\u062f\u0646 \u062f\u0648\u0628\u0627\u0631 \u06a9\u0644\u06cc\u06a9 \u06a9\u0646\u06cc\u062f",
            WS_CHILD|WS_VISIBLE|SS_RIGHT,x0,y,w,S(20),h,NULL,g_hInst,NULL);
        SendMessageW(c,WM_SETFONT,(WPARAM)g_fUIB,TRUE); add(c); y+=S(26);
        struct PI{const wchar_t* t;int type;};
        static const PI pal[]={
            {L"\u0646\u0627\u0645 (\u0641\u06cc\u0644\u062f)",PIT_FIELD},
            {L"\u0644\u06cc\u0628\u0644 \u0645\u062a\u0646",PIT_LABEL},
            {L"\u062e\u0637 \u0627\u0641\u0642\u06cc",PIT_HLINE},
            {L"\u062e\u0637 \u0639\u0645\u0648\u062f\u06cc",PIT_VLINE},
            {L"\u0645\u0633\u062a\u0637\u06cc\u0644",PIT_RECT},
            {L"\u06a9\u0627\u062f\u0631 \u062f\u0648\u0631 \u0635\u0641\u062d\u0647",PIT_FRAME},
            {L"\u062a\u0635\u0648\u06cc\u0631 \u0633\u0641\u0627\u0631\u0634\u06cc",PIT_IMAGE},
            {L"\u0644\u0648\u06af\u0648 \u062f\u0631\u0645\u0627\u0646\u06af\u0627\u0647",PIT_LOGO},
            {L"QR / \u0628\u0627\u0631\u06a9\u062f",PIT_QR},
            {L"\u0639\u06a9\u0633 \u067e\u0631\u0633\u0646\u0644\u06cc \u0628\u06cc\u0645\u0627\u0631",PIT_PHOTO},
            {L"\u0634\u0645\u0627\u0631\u0647 \u0646\u0648\u0628\u062a",PIT_APPTNO},
        };
        for(const auto& p:pal){
            HWND b=createFlatButton(h,5000+p.type,p.t,ICO_PLUS,BS_OUTLINE,
                x0,y,w,S(26)); add(b);
            SetWindowLongPtrW(b,GWLP_USERDATA,(LONG_PTR)p.type);
            y+=S(30);
        }
        return;
    }
    // single-selection properties
    label(L"\u0645\u0648\u0642\u0639\u06cc\u062a (X, Y mm)"); y+=S(20);
    add(uikit::AzNumberSpinner_Create(h,6001,it->x,0,500,1,x0+w/2+S(2),y,w/2-S(2),S(24)));
    add(uikit::AzNumberSpinner_Create(h,6002,it->y,0,500,1,x0,y,w/2-S(2),S(24))); y+=S(28);
    label(L"\u0627\u0628\u0639\u0627\u062f (\u0639\u0631\u0636\u060c \u0627\u0631\u062a\u0641\u0627\u0639 mm)"); y+=S(20);
    add(uikit::AzNumberSpinner_Create(h,6003,it->w,1,500,1,x0+w/2+S(2),y,w/2-S(2),S(24)));
    add(uikit::AzNumberSpinner_Create(h,6004,it->h,1,500,1,x0,y,w/2-S(2),S(24))); y+=S(28);
    label(L"\u0686\u0631\u062e\u0634 (\u062f\u0631\u062c\u0647)"); y+=S(20);
    add(uikit::AzNumberSpinner_Create(h,6005,it->rot,-180,180,1,x0,y,w,S(24))); y+=S(28);
    if(it->type==PIT_LABEL || it->type==PIT_FIELD || it->type==PIT_APPTNO){
        label(L"\u0627\u0646\u062f\u0627\u0632\u0647 \u0641\u0648\u0646\u062a (pt)"); y+=S(20);
        add(uikit::AzNumberSpinner_Create(h,6006,it->fontPt,5,72,1,x0,y,w,S(24))); y+=S(28);
        label(L"\u0631\u0646\u06af \u0645\u062a\u0646"); y+=S(20);
        add(uikit::AzColorPicker_Create(h,6007,
            RGB((it->textColor>>16)&0xFF,(it->textColor>>8)&0xFF,it->textColor&0xFF),
            x0,y,w,S(24))); y+=S(28);
        if(it->type==PIT_LABEL){
            label(L"\u0645\u062a\u0646"); y+=S(20);
            HWND e=CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",it->text.c_str(),
                WS_CHILD|WS_VISIBLE|ES_AUTOHSCROLL,x0,y,w,S(24),h,
                (HMENU)6008,g_hInst,NULL);
            SendMessageW(e,WM_SETFONT,(WPARAM)g_fUI,TRUE); add(e); y+=S(28);
        }
    }
    label(L"\u0631\u0646\u06af \u06a9\u0627\u062f\u0631"); y+=S(20);
    add(uikit::AzColorPicker_Create(h,6009,
        RGB((it->borderColor>>16)&0xFF,(it->borderColor>>8)&0xFF,it->borderColor&0xFF),
        x0,y,w,S(24))); y+=S(28);
    label(L"\u0642\u0641\u0644 \u0645\u0648\u0642\u0639\u06cc\u062a"); y+=S(20);
    add(uikit::AzSwitch_Create(h,6010,it->locked,x0+w-S(48),y,S(48),S(24))); y+=S(30);
    add(createFlatButton(h,6011,L"\u062d\u0630\u0641 \u0622\u06cc\u062a\u0645",ICO_TRASH,BS_DANGER,x0,y,w/2-S(2),S(28)));
    add(createFlatButton(h,6012,L"\u06a9\u067e\u06cc \u0622\u06cc\u062a\u0645",ICO_SAVE,BS_OUTLINE,x0+w/2+S(2),y,w/2-S(2),S(28)));
    y+=S(32);
}

static void designer_layout(HWND h){
    DesignerState* st=(DesignerState*)GetWindowLongPtrW(h,GWLP_USERDATA);
    if(!st) return;
    RECT rc; GetClientRect(h,&rc);
    int mgn=S(8), bh=S(30), by=S(7);
    int x=mgn;
    auto place=[&](HWND c,int wdt){ if(c){ MoveWindow(c,x,by,wdt,bh,TRUE); x+=wdt+S(6);} };
    place(GetDlgItem(h,IDC_DSG_SAVE),S(90));
    place(GetDlgItem(h,IDC_DSG_SAVEAS),S(140));
    place(GetDlgItem(h,IDC_DSG_DOWNLOAD),S(120));
    place(GetDlgItem(h,IDC_DSG_EXIT),S(80));
    if(st->hPaperCombo) MoveWindow(st->hPaperCombo,rc.right/2-S(110),by,S(110),S(200),TRUE);
    if(st->hOrientCombo) MoveWindow(st->hOrientCombo,rc.right/2+S(4),by,S(100),S(200),TRUE);
    if(st->hZoom) MoveWindow(st->hZoom,rc.right-S(180),by,S(170),bh,TRUE);
    designer_buildInspector(h);
    uikit::AzLayoutGuard_Verify(h);
}

// add a palette item at canvas center
static void addPaletteItem(DesignerState* st,int type){
    PrintItem it;
    it.id=nextItemId(st); it.type=type;
    it.x=st->design.paperW/2-15; it.y=st->design.paperH/2-5; it.w=30; it.h=8;
    it.z = (int)st->design.items.size();
    switch(type){
    case PIT_FRAME: it.is_frame=true; it.x=8;it.y=8;
        it.w=st->design.paperW-16; it.h=st->design.paperH-16;
        it.borderWidth=1; it.borderColor=0x1F6FEB; break;
    case PIT_HLINE: it.w=40; it.h=0.2; it.borderWidth=0.3; break;
    case PIT_VLINE: it.w=0.2; it.h=40; it.borderWidth=0.3; break;
    case PIT_RECT:  it.w=40; it.h=20; it.borderWidth=0.3; break;
    case PIT_QR:    it.w=20; it.h=20; it.field=L"receiptNo"; break;
    case PIT_LOGO:  it.w=24; it.h=24; break;
    case PIT_IMAGE: it.w=30; it.h=20; break;
    case PIT_PHOTO: it.w=22; it.h=28; it.borderWidth=0.3; break;
    case PIT_APPTNO: it.w=40; it.h=14; it.fontPt=22; it.bold=true; it.align=1; break;
    case PIT_LABEL: it.text=L"\u0645\u062a\u0646 \u062f\u0644\u062e\u0648\u0627\u0647"; break;
    case PIT_FIELD: it.field=L"firstName"; it.prefix=L"\u0646\u0627\u0645: "; break;
    }
    st->design.items.push_back(it);
    st->selId=it.id; st->multiSel.clear();
    commit(st);
}

static void designer_save(HWND h,bool asNew);

static LRESULT CALLBACK DesignerProc(HWND h,UINT m,WPARAM w,LPARAM l){
    DesignerState* st=(DesignerState*)GetWindowLongPtrW(h,GWLP_USERDATA);
    switch(m){
    case WM_SIZE: if(st) designer_layout(h); return 0;
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: designer_paint(h); return 0;
    case WM_LBUTTONDOWN:{
        if(!st) break;
        int mx=GET_X_LPARAM(l),my=GET_Y_LPARAM(l);
        RECT rc; GetClientRect(h,&rc);
        int inspW=(int)(rc.right*0.30);
        if(mx>rc.right-inspW) break;          // inspector area
        if(my<S(44)+S(60)) break;             // top bar/strip
        int id=hitTest(st,mx,my);
        bool ctrl=(GetKeyState(VK_CONTROL)&0x8000)!=0;
        if(id<0){
            st->selId=-1; st->multiSel.clear();
            st->panning=true; st->panStart.x=mx; st->panStart.y=my;
            st->pan0x=st->panX; st->pan0y=st->panY;
            SetCapture(h);
        } else {
            if(ctrl){
                st->multiSel.push_back(id);
            } else {
                st->selId=id; st->multiSel.clear();
                PrintItem* it=findItem(st,id);
                if(it && !it->locked){
                    st->dragging=true; st->dragStart.x=mx; st->dragStart.y=my;
                    st->dragItemX0=it->x; st->dragItemY0=it->y;
                    SetCapture(h);
                }
            }
            designer_buildInspector(h);
        }
        InvalidateRect(h,NULL,FALSE);
        return 0;
    }
    case WM_MOUSEMOVE:{
        if(!st) break;
        int mx=GET_X_LPARAM(l),my=GET_Y_LPARAM(l);
        if(st->dragging){
            PrintItem* it=findItem(st,st->selId);
            if(it){ double k=pxPerMm(st);
                it->x=st->dragItemX0+(mx-st->dragStart.x)/k;
                it->y=st->dragItemY0+(my-st->dragStart.y)/k;
                st->dirty=true; InvalidateRect(h,NULL,FALSE); }
        } else if(st->panning){
            st->panX=st->pan0x+(mx-st->panStart.x);
            st->panY=st->pan0y+(my-st->panStart.y);
            InvalidateRect(h,NULL,FALSE);
        }
        return 0;
    }
    case WM_LBUTTONUP:{
        if(!st) break;
        if(st->dragging){ st->dragging=false; commit(st); designer_buildInspector(h); }
        if(st->panning) st->panning=false;
        ReleaseCapture();
        return 0;
    }
    case WM_MBUTTONDOWN:{
        if(!st) break;
        st->panning=true; st->panStart.x=GET_X_LPARAM(l); st->panStart.y=GET_Y_LPARAM(l);
        st->pan0x=st->panX; st->pan0y=st->panY; SetCapture(h); return 0;
    }
    case WM_MBUTTONUP: if(st){ st->panning=false; ReleaseCapture(); } return 0;
    case WM_MOUSEWHEEL:{
        if(!st) break;
        int delta=GET_WHEEL_DELTA_WPARAM(w);
        bool ctrl=(LOWORD(w)&MK_CONTROL)!=0;
        bool shift=(LOWORD(w)&MK_SHIFT)!=0;
        if(ctrl){
            POINT pt={GET_X_LPARAM(l),GET_Y_LPARAM(l)}; ScreenToClient(h,&pt);
            double mxBefore,myBefore; pxToMm(st,pt.x,pt.y,mxBefore,myBefore);
            double f=(delta>0)?1.1:1/1.1;
            st->zoom*=f; if(st->zoom<0.25)st->zoom=0.25; if(st->zoom>4.0)st->zoom=4.0;
            // keep cursor anchored
            double k=pxPerMm(st);
            st->panX=pt.x-mxBefore*k; st->panY=pt.y-myBefore*k;
            if(st->hZoom) SendMessageW(st->hZoom,TBM_SETPOS,TRUE,(LPARAM)(int)(st->zoom*100));
        } else if(shift){
            st->panX += (delta>0)? S(40):-S(40);
        } else {
            st->panY += (delta>0)? S(40):-S(40);
        }
        InvalidateRect(h,NULL,FALSE);
        return 0;
    }
    case WM_HSCROLL:{
        if(st && (HWND)l==st->hZoom){
            int pos=(int)SendMessageW(st->hZoom,TBM_GETPOS,0,0);
            st->zoom=pos/100.0; if(st->zoom<0.25)st->zoom=0.25;
            InvalidateRect(h,NULL,FALSE);
        }
        return 0;
    }
    case WM_KEYDOWN:{
        if(!st) break;
        bool ctrl=(GetKeyState(VK_CONTROL)&0x8000)!=0;
        PrintItem* it=findItem(st,st->selId);
        switch(w){
        case 'S': if(ctrl){ designer_save(h,(GetKeyState(VK_SHIFT)&0x8000)!=0); } return 0;
        case 'Z': if(ctrl){ PrintDesign p; if(st->undo.undo(p)){ st->design=p; designer_buildInspector(h); InvalidateRect(h,NULL,FALSE);} } return 0;
        case 'Y': if(ctrl){ PrintDesign p; if(st->undo.redo(p)){ st->design=p; designer_buildInspector(h); InvalidateRect(h,NULL,FALSE);} } return 0;
        case 'D': if(ctrl && it){ PrintItem n=*it; n.id=nextItemId(st); n.x+=10; n.y+=10;
                    st->design.items.push_back(n); st->selId=n.id; commit(st);
                    designer_buildInspector(h); InvalidateRect(h,NULL,FALSE);} return 0;
        case VK_DELETE: case VK_BACK:
            if(it){ st->design.items.erase(std::remove_if(st->design.items.begin(),
                st->design.items.end(),[&](const PrintItem&x){return x.id==st->selId;}),
                st->design.items.end()); st->selId=-1; commit(st);
                designer_buildInspector(h); InvalidateRect(h,NULL,FALSE);} return 0;
        case VK_LEFT: case VK_RIGHT: case VK_UP: case VK_DOWN:
            if(it && !it->locked){ double d=(GetKeyState(VK_SHIFT)&0x8000)?5:1;
                if(w==VK_LEFT)it->x-=d; if(w==VK_RIGHT)it->x+=d;
                if(w==VK_UP)it->y-=d; if(w==VK_DOWN)it->y+=d;
                st->dirty=true; InvalidateRect(h,NULL,FALSE);} return 0;
        case VK_ESCAPE: st->selId=-1; st->multiSel.clear();
            designer_buildInspector(h); InvalidateRect(h,NULL,FALSE); return 0;
        }
        return 0;
    }
    case WM_COMMAND:{
        if(!st) break;
        int id=LOWORD(w);
        if(id>=5000 && id<5100){ addPaletteItem(st,id-5000);
            designer_buildInspector(h); InvalidateRect(h,NULL,FALSE); return 0; }
        if(id>=6001 && id<6100){
            PrintItem* it=findItem(st,st->selId);
            if(it){
                if(id==6001) it->x=uikit::AzNumberSpinner_Get((HWND)l);
                else if(id==6002) it->y=uikit::AzNumberSpinner_Get((HWND)l);
                else if(id==6003) it->w=uikit::AzNumberSpinner_Get((HWND)l);
                else if(id==6004) it->h=uikit::AzNumberSpinner_Get((HWND)l);
                else if(id==6005) it->rot=uikit::AzNumberSpinner_Get((HWND)l);
                else if(id==6006) it->fontPt=uikit::AzNumberSpinner_Get((HWND)l);
                else if(id==6007){ COLORREF c=uikit::AzColorPicker_Get((HWND)l);
                    it->textColor=(GetRValue(c)<<16)|(GetGValue(c)<<8)|GetBValue(c); }
                else if(id==6008 && HIWORD(w)==EN_CHANGE){ wchar_t b[256]={0};
                    GetWindowTextW((HWND)l,b,255); it->text=b; }
                else if(id==6009){ COLORREF c=uikit::AzColorPicker_Get((HWND)l);
                    it->borderColor=(GetRValue(c)<<16)|(GetGValue(c)<<8)|GetBValue(c);
                    if(it->borderWidth<0.2) it->borderWidth=0.3; }
                else if(id==6010){ it->locked=uikit::AzSwitch_Get((HWND)l); }
                else if(id==6011){ st->design.items.erase(std::remove_if(
                    st->design.items.begin(),st->design.items.end(),
                    [&](const PrintItem&x){return x.id==st->selId;}),st->design.items.end());
                    st->selId=-1; commit(st); designer_buildInspector(h); }
                else if(id==6012){ PrintItem n=*it; n.id=nextItemId(st); n.x+=10;n.y+=10;
                    st->design.items.push_back(n); st->selId=n.id; commit(st);
                    designer_buildInspector(h); }
                st->dirty=true; InvalidateRect(h,NULL,FALSE);
            }
            return 0;
        }
        switch(id){
        case IDC_DSG_SAVE:    designer_save(h,false); return 0;
        case IDC_DSG_SAVEAS:  designer_save(h,true);  return 0;
        case IDC_DSG_DOWNLOAD:{
            wchar_t file[MAX_PATH]=L"design.aztpl";
            OPENFILENAMEW ofn; ZeroMemory(&ofn,sizeof(ofn)); ofn.lStructSize=sizeof(ofn);
            ofn.hwndOwner=h; ofn.lpstrFilter=L"AzadiTeb Template\0*.aztpl\0";
            ofn.lpstrDefExt=L"aztpl"; ofn.lpstrFile=file; ofn.nMaxFile=MAX_PATH;
            ofn.Flags=OFN_OVERWRITEPROMPT;
            if(GetSaveFileNameW(&ofn)){
                std::string j=Design_ToJson(st->design);
                HANDLE fh=CreateFileW(file,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,
                    FILE_ATTRIBUTE_NORMAL,NULL);
                if(fh!=INVALID_HANDLE_VALUE){ DWORD wr; WriteFile(fh,j.data(),
                    (DWORD)j.size(),&wr,NULL); CloseHandle(fh); }
            }
            return 0;
        }
        case IDC_DSG_EXIT: SendMessageW(h,WM_CLOSE,0,0); return 0;
        case IDC_DSG_PAPER:
            if(HIWORD(w)==CBN_SELCHANGE){
                int sel=(int)SendMessageW(st->hPaperCombo,CB_GETCURSEL,0,0);
                wchar_t b[32]={0}; SendMessageW(st->hPaperCombo,CB_GETLBTEXT,sel,(LPARAM)b);
                double pw,ph; if(Paper_Dims(b,pw,ph)){
                    st->design.paper=b;
                    st->design.paperW=(st->design.orientation==1)?ph:pw;
                    st->design.paperH=(st->design.orientation==1)?pw:ph;
                    commit(st); InvalidateRect(h,NULL,FALSE);
                }
            }
            return 0;
        case IDC_DSG_ORIENT:
            if(HIWORD(w)==CBN_SELCHANGE){
                int sel=(int)SendMessageW(st->hOrientCombo,CB_GETCURSEL,0,0);
                if(sel!=st->design.orientation){
                    st->design.orientation=sel;
                    std::swap(st->design.paperW,st->design.paperH);
                    commit(st); InvalidateRect(h,NULL,FALSE);
                }
            }
            return 0;
        }
        return 0;
    }
    case WM_CTLCOLORSTATIC:{ HDC dc=(HDC)w; SetBkColor(dc,g_theme.bg);
        SetTextColor(dc,g_theme.text); return (LRESULT)g_brBg; }
    case WM_CLOSE:
        if(st && st->dirty){
            int r=MessageBoxW(h,
                L"\u062a\u063a\u06cc\u06cc\u0631\u0627\u062a \u0630\u062e\u06cc\u0631\u0647 \u0646\u0634\u062f\u0647. \u0622\u06cc\u0627 \u0645\u06cc\u200c\u062e\u0648\u0627\u0647\u06cc\u062f \u0630\u062e\u06cc\u0631\u0647 \u06a9\u0646\u06cc\u062f\u061f",
                L"\u062f\u06cc\u0632\u0627\u06cc\u0646 \u0686\u0627\u067e",MB_YESNOCANCEL|MB_ICONQUESTION);
            if(r==IDCANCEL) return 0;
            if(r==IDYES) designer_save(h,false);
        }
        DestroyWindow(h); return 0;
    case WM_DESTROY:
        clearInspector();
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(h,m,w,l);
}

static void designer_save(HWND h,bool asNew){
    DesignerState* st=(DesignerState*)GetWindowLongPtrW(h,GWLP_USERDATA);
    if(!st) return;
    if(st->design.kind==L"builtin") asNew=true;     // never overwrite a builtin
    if(asNew){
        // simple name prompt via input box (reuse a tiny dialog)
        std::wstring name=st->design.name+L" (\u06a9\u067e\u06cc)";
        st->design.kind=L"user"; st->design.name=name; st->design.id=0;
        int newId=Designs_Insert(st->design);
        st->design.id=newId;
    } else {
        Designs_Update(st->design);
    }
    // apply to selected sections
    for(int sid:st->targetSections) SectionDesign_Set(sid,st->design.id);
    // broadcast design push
    if(g_hFrame) PostMessageW(g_hFrame,WM_APP_DESIGN_PUSHED,(WPARAM)st->design.id,0);
    st->dirty=false;
    MessageBoxW(h,L"\u062f\u06cc\u0632\u0627\u06cc\u0646 \u0630\u062e\u06cc\u0631\u0647 \u0648 \u0628\u0631 \u0628\u062e\u0634\u200c\u0647\u0627 \u0627\u0639\u0645\u0627\u0644 \u0634\u062f.",
        L"\u062f\u06cc\u0632\u0627\u06cc\u0646 \u0686\u0627\u067e",MB_OK|MB_ICONINFORMATION);
}

static void OpenDesignerWindow(HWND owner,const DesignerLaunch& launch,
                               const PrintDesign* initial){
    static bool reg=false; const wchar_t* CLS=L"AzPrintDesigner";
    if(!reg){ WNDCLASSW wc={0}; wc.lpfnWndProc=DesignerProc; wc.hInstance=g_hInst;
        wc.hCursor=LoadCursor(NULL,IDC_ARROW); wc.lpszClassName=CLS;
        wc.hbrBackground=g_brBg; wc.style=CS_HREDRAW|CS_VREDRAW;
        RegisterClassW(&wc); reg=true; }
    DesignerState* st=new DesignerState();
    st->hMain=owner; st->targetSections=launch.sectionIds;
    if(initial) st->design=*initial;
    else {
        // load active design of first section, else T01
        if(!launch.sectionIds.empty())
            SectionDesign_Resolve(launch.sectionIds[0],st->design);
        else { std::vector<PrintDesign> b; Designs_Builtins(b);
            if(!b.empty()) st->design=b[0]; }
    }
    st->undo.seed(st->design);

    RECT wa; SystemParametersInfoW(SPI_GETWORKAREA,0,&wa,0);
    HWND h=CreateWindowExW(0,CLS,L"\u062f\u06cc\u0632\u0627\u06cc\u0646 \u0686\u0627\u067e",
        WS_POPUP|WS_VISIBLE|WS_CLIPCHILDREN,
        wa.left,wa.top,wa.right-wa.left,wa.bottom-wa.top,owner,NULL,g_hInst,NULL);
    SetWindowLongPtrW(h,GWLP_USERDATA,(LONG_PTR)st);

    createFlatButton(h,IDC_DSG_SAVE,L"\u0630\u062e\u06cc\u0631\u0647",ICO_SAVE,BS_PRIMARY,0,0,10,10);
    createFlatButton(h,IDC_DSG_SAVEAS,L"\u0630\u062e\u06cc\u0631\u0647 \u0628\u0647 \u0639\u0646\u0648\u0627\u0646 \u062c\u062f\u06cc\u062f",ICO_SAVE,BS_OUTLINE,0,0,10,10);
    createFlatButton(h,IDC_DSG_DOWNLOAD,L"\u062f\u0627\u0646\u0644\u0648\u062f \u062f\u06cc\u0632\u0627\u06cc\u0646",ICO_SAVE,BS_OUTLINE,0,0,10,10);
    createFlatButton(h,IDC_DSG_EXIT,L"\u062e\u0631\u0648\u062c",ICO_X,BS_DANGER,0,0,10,10);
    st->hPaperCombo=CreateWindowExW(0,L"COMBOBOX",L"",
        WS_CHILD|WS_VISIBLE|CBS_DROPDOWNLIST|WS_VSCROLL,0,0,10,200,h,
        (HMENU)IDC_DSG_PAPER,g_hInst,NULL);
    SendMessageW(st->hPaperCombo,WM_SETFONT,(WPARAM)g_fUI,TRUE);
    const wchar_t* papers[]={L"A3",L"A4",L"A5",L"A6",L"B5",L"Letter",L"R80",L"R58"};
    for(auto p:papers) SendMessageW(st->hPaperCombo,CB_ADDSTRING,0,(LPARAM)p);
    SendMessageW(st->hPaperCombo,CB_SELECTSTRING,(WPARAM)-1,(LPARAM)st->design.paper.c_str());
    st->hOrientCombo=CreateWindowExW(0,L"COMBOBOX",L"",
        WS_CHILD|WS_VISIBLE|CBS_DROPDOWNLIST,0,0,10,200,h,
        (HMENU)IDC_DSG_ORIENT,g_hInst,NULL);
    SendMessageW(st->hOrientCombo,WM_SETFONT,(WPARAM)g_fUI,TRUE);
    SendMessageW(st->hOrientCombo,CB_ADDSTRING,0,(LPARAM)L"\u0639\u0645\u0648\u062f\u06cc");
    SendMessageW(st->hOrientCombo,CB_ADDSTRING,0,(LPARAM)L"\u0627\u0641\u0642\u06cc");
    SendMessageW(st->hOrientCombo,CB_SETCURSEL,st->design.orientation,0);
    st->hZoom=CreateWindowExW(0,TRACKBAR_CLASSW,L"",
        WS_CHILD|WS_VISIBLE|TBS_HORZ,0,0,10,10,h,(HMENU)IDC_DSG_ZOOM,g_hInst,NULL);
    SendMessageW(st->hZoom,TBM_SETRANGE,TRUE,MAKELONG(25,400));
    SendMessageW(st->hZoom,TBM_SETPOS,TRUE,100);

    designer_layout(h);
    ShowWindow(h,SW_SHOW);
    EnableWindow(owner,FALSE);
    MSG msg;
    while(GetMessageW(&msg,NULL,0,0)){
        if(!IsWindow(h)) break;
        if(IsDialogMessageW(h,&msg)) continue;
        TranslateMessage(&msg); DispatchMessageW(&msg);
    }
    EnableWindow(owner,TRUE); SetForegroundWindow(owner);
    delete st;
}

} // anonymous namespace

// ---------------------------------------------------------- public entries ---
void PrintDesigner_Open(HWND hMain){
    Sections_Init(); Designs_Init();
    DesignerLaunch launch;
    if(!RunSectionPicker(hMain,launch)) return;
    OpenDesignerWindow(hMain,launch,nullptr);
}

// =====================================================================
//  RESTORE DESIGN (§4)
// =====================================================================
namespace {
struct RestoreState {
    PrintDesign loaded; bool valid;
    std::vector<int> sections;
    HWND hStatus, hList;
    RestoreState():valid(false){}
};

static void restore_loadFile(HWND h,RestoreState* st,const std::wstring& path){
    HANDLE fh=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,NULL,
        OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    if(fh==INVALID_HANDLE_VALUE){ st->valid=false;
        SetWindowTextW(st->hStatus,L"\u0641\u0627\u06cc\u0644 \u0628\u0627\u0632 \u0646\u0634\u062f"); return; }
    DWORD sz=GetFileSize(fh,NULL); std::string j;
    if(sz!=INVALID_FILE_SIZE&&sz>0){ j.resize(sz); DWORD rd; ReadFile(fh,&j[0],sz,&rd,NULL); j.resize(rd);}
    CloseHandle(fh);
    std::wstring err;
    if(Design_FromJson(j,st->loaded,err)){
        st->valid=true;
        std::wstring msg=L"\u062f\u06cc\u0632\u0627\u06cc\u0646 \u0645\u0639\u062a\u0628\u0631: "+st->loaded.name;
        SetWindowTextW(st->hStatus,msg.c_str());
    } else {
        st->valid=false;
        SetWindowTextW(st->hStatus,err.c_str());
    }
    InvalidateRect(h,NULL,TRUE);
}

static LRESULT CALLBACK RestoreProc(HWND h,UINT m,WPARAM w,LPARAM l){
    RestoreState* st=(RestoreState*)GetWindowLongPtrW(h,GWLP_USERDATA);
    switch(m){
    case AZ_DROPZONE_DROPPED:{
        std::wstring* p=(std::wstring*)l;
        if(p){ restore_loadFile(h,st,*p); delete p; }
        return 0;
    }
    case WM_COMMAND:{
        int id=LOWORD(w);
        if(id==IDC_RST_PICK){
            wchar_t file[MAX_PATH]={0};
            OPENFILENAMEW ofn; ZeroMemory(&ofn,sizeof(ofn)); ofn.lStructSize=sizeof(ofn);
            ofn.hwndOwner=h; ofn.lpstrFilter=L"AzadiTeb Template\0*.aztpl;*.json\0";
            ofn.lpstrFile=file; ofn.nMaxFile=MAX_PATH; ofn.Flags=OFN_FILEMUSTEXIST;
            if(GetOpenFileNameW(&ofn)) restore_loadFile(h,st,file);
        } else if(id==IDC_RST_APPLY){
            if(!st->valid){ MessageBoxW(h,
                L"\u0627\u0628\u062a\u062f\u0627 \u06cc\u06a9 \u0641\u0627\u06cc\u0644 \u0645\u0639\u062a\u0628\u0631 \u0628\u0627\u0631\u06af\u0630\u0627\u0631\u06cc \u06a9\u0646\u06cc\u062f.",
                L"\u0628\u0627\u0632\u06af\u0631\u062f\u0627\u0646\u06cc",MB_OK|MB_ICONWARNING); return 0; }
            // section picker
            DesignerLaunch launch;
            if(!RunSectionPicker(h,launch)) return 0;
            st->loaded.kind=L"user"; st->loaded.id=0;
            int newId=Designs_Insert(st->loaded);
            for(int sid:launch.sectionIds) SectionDesign_Set(sid,newId);
            if(g_hFrame) PostMessageW(g_hFrame,WM_APP_DESIGN_PUSHED,(WPARAM)newId,0);
            MessageBoxW(h,L"\u062f\u06cc\u0632\u0627\u06cc\u0646 \u0630\u062e\u06cc\u0631\u0647 \u0648 \u0627\u0639\u0645\u0627\u0644 \u0634\u062f.",
                L"\u0628\u0627\u0632\u06af\u0631\u062f\u0627\u0646\u06cc",MB_OK|MB_ICONINFORMATION);
            DestroyWindow(h);
        } else if(id==IDC_RST_CANCEL){ DestroyWindow(h); }
        return 0;
    }
    case WM_CTLCOLORSTATIC:{ HDC dc=(HDC)w; SetBkColor(dc,g_theme.surface);
        SetTextColor(dc,g_theme.text); return (LRESULT)g_brSurface; }
    case WM_ERASEBKGND:{ RECT rc; GetClientRect(h,&rc); FillRect((HDC)w,&rc,g_brSurface); return 1; }
    case WM_CLOSE: DestroyWindow(h); return 0;
    case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(h,m,w,l);
}
} // anon

void RestoreDesign_Open(HWND hMain){
    Sections_Init(); Designs_Init();
    static bool reg=false; const wchar_t* CLS=L"AzRestoreDesign";
    if(!reg){ WNDCLASSW wc={0}; wc.lpfnWndProc=RestoreProc; wc.hInstance=g_hInst;
        wc.hCursor=LoadCursor(NULL,IDC_ARROW); wc.lpszClassName=CLS;
        wc.hbrBackground=g_brSurface; RegisterClassW(&wc); reg=true; }
    int W=S(640),H=S(480);
    RECT pr; GetWindowRect(hMain,&pr);
    int x=pr.left+((pr.right-pr.left)-W)/2, y=pr.top+((pr.bottom-pr.top)-H)/2;
    RestoreState st;
    HWND h=CreateWindowExW(WS_EX_DLGMODALFRAME,CLS,
        L"\u0628\u0627\u0632\u06af\u0631\u062f\u0627\u0646\u06cc \u062f\u06cc\u0632\u0627\u06cc\u0646 \u0686\u0627\u067e",
        WS_POPUP|WS_CAPTION|WS_SYSMENU,x,y,W,H,hMain,NULL,g_hInst,NULL);
    SetWindowLongPtrW(h,GWLP_USERDATA,(LONG_PTR)&st);
    int mgn=S(14);
    uikit::AzDropZone_Create(h,IDC_RST_DROP,
        L"\u0641\u0627\u06cc\u0644 .aztpl \u0631\u0627 \u0627\u06cc\u0646\u062c\u0627 \u0631\u0647\u0627 \u06a9\u0646\u06cc\u062f \u06cc\u0627 \u06a9\u0644\u06cc\u06a9 \u06a9\u0646\u06cc\u062f",
        L"AzadiTeb Template\0*.aztpl;*.json\0",mgn,mgn,W-2*mgn,S(120));
    createFlatButton(h,IDC_RST_PICK,L"\u0627\u0646\u062a\u062e\u0627\u0628 \u0641\u0627\u06cc\u0644",ICO_SAVE,BS_OUTLINE,
        mgn,mgn+S(130),S(140),S(30));
    st.hStatus=CreateWindowExW(0,L"STATIC",
        L"\u0641\u0627\u06cc\u0644\u06cc \u0628\u0627\u0631\u06af\u0630\u0627\u0631\u06cc \u0646\u0634\u062f\u0647",
        WS_CHILD|WS_VISIBLE|SS_RIGHT,mgn,mgn+S(170),W-2*mgn,S(24),h,NULL,g_hInst,NULL);
    SendMessageW(st.hStatus,WM_SETFONT,(WPARAM)g_fUI,TRUE);
    createFlatButton(h,IDC_RST_APPLY,
        L"\u0630\u062e\u06cc\u0631\u0647 \u0628\u0647 \u0639\u0646\u0648\u0627\u0646 \u062f\u06cc\u0632\u0627\u06cc\u0646 \u062c\u062f\u06cc\u062f \u0648 \u0627\u0639\u0645\u0627\u0644",
        ICO_CHECK,BS_PRIMARY,W-mgn-S(280),H-S(54),S(280),S(34));
    createFlatButton(h,IDC_RST_CANCEL,L"\u0627\u0646\u0635\u0631\u0627\u0641",ICO_X,BS_OUTLINE,
        mgn,H-S(54),S(110),S(34));
    ShowWindow(h,SW_SHOW);
    EnableWindow(hMain,FALSE);
    MSG msg;
    while(GetMessageW(&msg,NULL,0,0)){
        if(!IsWindow(h)) break;
        if(IsDialogMessageW(h,&msg)) continue;
        TranslateMessage(&msg); DispatchMessageW(&msg);
    }
    EnableWindow(hMain,TRUE); SetForegroundWindow(hMain);
}
