// ============================================================================
//  gdiplus.cpp — v1.3.0 GDI+ rendering layer
//
//  Why GDI+?  The UI redesign asked for "richer colours, soft lighting, open
//  layers" and a real background image. Plain GDI RoundRect/FillRect can't do
//  anti-aliasing, gradients, alpha-blended shadows or JPEG decoding. GDI+ is
//  part of every Windows since XP, links statically-friendly (-lgdiplus) and
//  keeps us at a SINGLE exe with no external runtime.
//
//  Everything here is defensive: if GDI+ fails to start (extremely old / weird
//  systems) the helpers fall back to the existing plain-GDI fillRoundRect so
//  the program still runs.  The background image is embedded as RCDATA
//  (id 103 = light, id 104 = dark) and decoded once, cached as an HBITMAP.
// ============================================================================
#include "app.h"
#include <objbase.h>
#include <gdiplus.h>

using namespace Gdiplus;

static ULONG_PTR s_gdipToken = 0;
static bool      s_gdipOK    = false;

void gdipStartup(){
    GdiplusStartupInput in;
    if(GdiplusStartup(&s_gdipToken, &in, NULL) == Ok)
        s_gdipOK = true;
}
void gdipShutdown(){
    if(s_gdipOK){ GdiplusShutdown(s_gdipToken); s_gdipOK=false; }
}

static inline Color C(COLORREF c, int a=255){
    return Color((BYTE)a, GetRValue(c), GetGValue(c), GetBValue(c));
}

//  Build a rounded-rect GraphicsPath (radius clamped to half the shorter side).
static void roundPath(GraphicsPath& p, const Rect& r, int rad){
    int w=r.Width, h=r.Height;
    int d = rad*2;
    if(d > w) d = w;
    if(d > h) d = h;
    if(d < 2){ p.AddRectangle(r); return; }
    p.AddArc(r.X,           r.Y,           d, d, 180, 90);
    p.AddArc(r.X+w-d,       r.Y,           d, d, 270, 90);
    p.AddArc(r.X+w-d,       r.Y+h-d,       d, d,   0, 90);
    p.AddArc(r.X,           r.Y+h-d,       d, d,  90, 90);
    p.CloseFigure();
}

void gpRoundRect(HDC dc, RECT rc, int rad, COLORREF fill, COLORREF border, int alpha){
    if(!s_gdipOK){ fillRoundRect(dc,rc,rad*2,fill,border); return; }
    Graphics g(dc); g.SetSmoothingMode(SmoothingModeAntiAlias);
    Rect r(rc.left, rc.top, rc.right-rc.left-1, rc.bottom-rc.top-1);
    GraphicsPath p; roundPath(p,r,rad);
    SolidBrush br(C(fill,alpha)); g.FillPath(&br,&p);
    if(border!=CLR_INVALID){ Pen pn(C(border,alpha),1.0f); g.DrawPath(&pn,&p); }
}

void gpGradRoundRect(HDC dc, RECT rc, int rad, COLORREF top, COLORREF bottom, COLORREF border){
    if(!s_gdipOK){ fillRoundRect(dc,rc,rad*2,top,border); return; }
    Graphics g(dc); g.SetSmoothingMode(SmoothingModeAntiAlias);
    Rect r(rc.left, rc.top, rc.right-rc.left-1, rc.bottom-rc.top-1);
    if(r.Width<=0||r.Height<=0) return;
    GraphicsPath p; roundPath(p,r,rad);
    LinearGradientBrush br(Rect(r.X,r.Y,r.Width,r.Height+1),
        C(top), C(bottom), LinearGradientModeVertical);
    g.FillPath(&br,&p);
    if(border!=CLR_INVALID){ Pen pn(C(border),1.0f); g.DrawPath(&pn,&p); }
}

void gpFillAlpha(HDC dc, RECT rc, int rad, COLORREF fill, int alpha){
    gpRoundRect(dc, rc, rad, fill, CLR_INVALID, alpha);
}

// ---------------------------------------------------------------------------
//  v1.8.0  rounded-corner background fix
//  gpRoundRect / gpGradRoundRect only fill the rounded path, so the 4 corner
//  triangles (inside the bounding rect but outside the path) keep whatever was
//  in the DC before — that produced the "wrong colour / black corner" artefact
//  on rounded controls when the surrounding pixels were not pre-painted with
//  the theme background.  The *Bg variants below paint those corner gaps with
//  `bg` FIRST, so corners always blend into the theme surface.
// ---------------------------------------------------------------------------

//  Paint just the 4 corner gaps of a rounded rect with `bg`.
void gpFillCorners(HDC dc, RECT rc, int rad, COLORREF bg){
    int w = rc.right-rc.left, h = rc.bottom-rc.top;
    if(w<=0||h<=0) return;
    int d = rad*2;
    if(d>w) d=w;
    if(d>h) d=h;
    if(d<2) return;                 // square — nothing to patch
    int r = d/2;
    if(!s_gdipOK){
        // Plain-GDI fallback: subtract a round-rect region from the full rect
        // and flood the remainder (the corners) with bg.
        HRGN full  = CreateRectRgn(rc.left, rc.top, rc.right, rc.bottom);
        HRGN round = CreateRoundRectRgn(rc.left, rc.top, rc.right+1, rc.bottom+1, d, d);
        CombineRgn(full, full, round, RGN_DIFF);
        HBRUSH br = CreateSolidBrush(bg);
        FillRgn(dc, full, br);
        DeleteObject(br); DeleteObject(full); DeleteObject(round);
        return;
    }
    Graphics g(dc); g.SetSmoothingMode(SmoothingModeAntiAlias);
    // Build a region = boundingRect - roundedPath, fill with bg.
    Rect b(rc.left, rc.top, w-1, h-1);
    GraphicsPath rp; roundPath(rp, b, rad);
    Region reg(b);                  // whole bounding box
    reg.Exclude(&rp);               // remove the rounded interior → corners only
    SolidBrush br(C(bg));
    g.FillRegion(&br, &reg);
    (void)r;
}

void gpRoundRectBg(HDC dc, RECT rc, int rad, COLORREF fill, COLORREF border, COLORREF bg, int alpha){
    if(bg!=CLR_INVALID) gpFillCorners(dc, rc, rad, bg);
    gpRoundRect(dc, rc, rad, fill, border, alpha);
}

void gpGradRoundRectBg(HDC dc, RECT rc, int rad, COLORREF top, COLORREF bottom, COLORREF border, COLORREF bg){
    if(bg!=CLR_INVALID) gpFillCorners(dc, rc, rad, bg);
    gpGradRoundRect(dc, rc, rad, top, bottom, border);
}

//  Soft drop shadow: draw several expanding translucent rounded rects so the
//  edge fades out — a cheap, dependency-free blur that gives cards real depth.
void gpShadow(HDC dc, RECT rc, int rad, int spread, int alpha){
    if(!s_gdipOK) return;
    Graphics g(dc); g.SetSmoothingMode(SmoothingModeAntiAlias);
    int layers = spread; if(layers<1) layers=1; if(layers>24) layers=24;
    for(int i=layers;i>=1;i--){
        int a = (alpha * (layers-i+1)) / (layers*layers);  // fades toward edge
        if(a<1) a=1;
        Rect r(rc.left-i, rc.top-i+2, (rc.right-rc.left)+2*i-1,
               (rc.bottom-rc.top)+2*i-1);
        GraphicsPath p; roundPath(p,r,rad+i);
        SolidBrush br(Color((BYTE)a,0,0,0));
        g.FillPath(&br,&p);
    }
}

void gpLine(HDC dc, int x1,int y1,int x2,int y2, COLORREF col, float w, int alpha){
    if(!s_gdipOK){
        HPEN pn=CreatePen(PS_SOLID,(int)(w+0.5f),col);
        HGDIOBJ op=SelectObject(dc,pn);
        MoveToEx(dc,x1,y1,0); LineTo(dc,x2,y2);
        SelectObject(dc,op); DeleteObject(pn); return;
    }
    Graphics g(dc); g.SetSmoothingMode(SmoothingModeAntiAlias);
    Pen pn(C(col,alpha), w);
    g.DrawLine(&pn,(REAL)x1,(REAL)y1,(REAL)x2,(REAL)y2);
}

// ----------------------------------------------------- background image -----
//  Decode the embedded JPEG (RCDATA 103/104) once and cache it. The image is
//  loaded from an IStream wrapped around the resource bytes.
static Image* s_bgLight = NULL;
static Image* s_bgDark  = NULL;

static Image* loadBgImage(int resId){
    HRSRC hr = FindResourceW(g_hInst, MAKEINTRESOURCEW(resId), RT_RCDATA);
    if(!hr) return NULL;
    HGLOBAL hg = LoadResource(g_hInst, hr);
    DWORD   sz = SizeofResource(g_hInst, hr);
    void*  dat = LockResource(hg);
    if(!dat || !sz) return NULL;
    HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, sz);
    if(!mem) return NULL;
    void* p = GlobalLock(mem); memcpy(p, dat, sz); GlobalUnlock(mem);
    IStream* st = NULL;
    if(CreateStreamOnHGlobal(mem, TRUE, &st) != S_OK){ GlobalFree(mem); return NULL; }
    Image* img = Image::FromStream(st);
    st->Release();   // stream owns the HGLOBAL now (TRUE), released with image use
    if(img && img->GetLastStatus()!=Ok){ delete img; img=NULL; }
    return img;
}

bool gpDrawBackground(HDC dc, RECT rc, bool dark, COLORREF scrim, int scrimA){
    if(!s_gdipOK) return false;
    Image*& slot = dark ? s_bgDark : s_bgLight;
    if(!slot) slot = loadBgImage(dark ? 104 : 103);
    if(!slot) return false;

    Graphics g(dc);
    g.SetInterpolationMode(InterpolationModeHighQualityBicubic);
    g.SetPixelOffsetMode(PixelOffsetModeHalf);

    int W = rc.right-rc.left, H = rc.bottom-rc.top;
    if(W<=0||H<=0) return false;
    REAL iw = (REAL)slot->GetWidth(), ih = (REAL)slot->GetHeight();
    if(iw<=0||ih<=0) return false;
    // cover-fit (fill the whole area, crop overflow, keep aspect)
    REAL scale = (W/iw > H/ih) ? W/iw : H/ih;
    REAL dw = iw*scale, dh = ih*scale;
    REAL dx = rc.left + (W-dw)/2, dy = rc.top + (H-dh)/2;
    g.DrawImage(slot, RectF(dx,dy,dw,dh), 0,0,iw,ih, UnitPixel);

    // legibility scrim
    if(scrimA>0){
        SolidBrush br(C(scrim, scrimA));
        g.FillRectangle(&br, rc.left, rc.top, W, H);
    }
    return true;
}

// ------------------------------------------------- tinted raster icons ------
//  Real (raster) icons embedded as RCDATA PNGs are drawn white-on-alpha and
//  recoloured to any theme colour at draw time via a GDI+ colour matrix. This
//  gives the print buttons proper image icons that still adapt to the theme.
static Image* s_iconCache[8] = {0};   // small fixed cache keyed by slot
static int    s_iconResId[8] = {0};

static Image* loadResImage(int resId){
    HRSRC hr = FindResourceW(g_hInst, MAKEINTRESOURCEW(resId), RT_RCDATA);
    if(!hr) return NULL;
    HGLOBAL hg = LoadResource(g_hInst, hr);
    DWORD   sz = SizeofResource(g_hInst, hr);
    void*  dat = LockResource(hg);
    if(!dat || !sz) return NULL;
    HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, sz);
    if(!mem) return NULL;
    void* p = GlobalLock(mem); memcpy(p, dat, sz); GlobalUnlock(mem);
    IStream* st = NULL;
    if(CreateStreamOnHGlobal(mem, TRUE, &st) != S_OK){ GlobalFree(mem); return NULL; }
    Image* img = Image::FromStream(st);
    st->Release();
    if(img && img->GetLastStatus()!=Ok){ delete img; img=NULL; }
    return img;
}
static Image* cachedResImage(int resId){
    for(int i=0;i<8;i++) if(s_iconResId[i]==resId && s_iconCache[i]) return s_iconCache[i];
    for(int i=0;i<8;i++) if(!s_iconCache[i]){
        s_iconCache[i]=loadResImage(resId); s_iconResId[i]=resId; return s_iconCache[i];
    }
    return loadResImage(resId);   // cache full → load transient (rare)
}

//  Draw RCDATA PNG `resId` centred & aspect-fit inside `rc`, recoloured to
//  `tint`.  Returns false if GDI+/resource unavailable so callers can fall back
//  to the vector drawIcon().
bool gpDrawTintedImageRes(HDC dc, int resId, RECT rc, COLORREF tint){
    if(!s_gdipOK) return false;
    Image* img = cachedResImage(resId);
    if(!img) return false;

    Graphics g(dc);
    g.SetInterpolationMode(InterpolationModeHighQualityBicubic);
    g.SetPixelOffsetMode(PixelOffsetModeHalf);
    g.SetSmoothingMode(SmoothingModeAntiAlias);

    int W = rc.right-rc.left, H = rc.bottom-rc.top;
    if(W<=0||H<=0) return false;
    REAL iw=(REAL)img->GetWidth(), ih=(REAL)img->GetHeight();
    if(iw<=0||ih<=0) return false;
    REAL scale = (W/iw < H/ih) ? W/iw : H/ih;   // contain-fit
    REAL dw=iw*scale, dh=ih*scale;
    REAL dx=rc.left+(W-dw)/2, dy=rc.top+(H-dh)/2;

    // colour matrix: replace RGB with tint, keep source alpha
    REAL r=GetRValue(tint)/255.0f, gg=GetGValue(tint)/255.0f, b=GetBValue(tint)/255.0f;
    ColorMatrix cm = {
        0,0,0,0,0,
        0,0,0,0,0,
        0,0,0,0,0,
        0,0,0,1,0,
        r,gg,b,0,1 };
    ImageAttributes ia; ia.SetColorMatrix(&cm);
    g.DrawImage(img, RectF(dx,dy,dw,dh), 0,0,iw,ih, UnitPixel, &ia);
    return true;
}

//  v1.6.0: draw a profile photo from a file, cropped/scaled into a CIRCLE that
//  fits the given rect. Used for user avatars (settings panel / reception info
//  panel). Returns false if GDI+ is off or the file can't be loaded, so callers
//  fall back to the initials/guest icon.
bool gpDrawImageFileCircle(HDC dc, const std::wstring& path, RECT rc){
    if(!s_gdipOK || path.empty()) return false;
    Image img(path.c_str());
    if(img.GetLastStatus()!=Ok) return false;
    REAL iw=(REAL)img.GetWidth(), ih=(REAL)img.GetHeight();
    if(iw<=0||ih<=0) return false;
    int W=rc.right-rc.left, H=rc.bottom-rc.top;
    if(W<=0||H<=0) return false;

    Graphics g(dc);
    g.SetInterpolationMode(InterpolationModeHighQualityBicubic);
    g.SetPixelOffsetMode(PixelOffsetModeHalf);
    g.SetSmoothingMode(SmoothingModeAntiAlias);

    // circular clip
    GraphicsPath clip;
    clip.AddEllipse(rc.left, rc.top, W-1, H-1);
    g.SetClip(&clip);

    // cover-fit (fill the circle, crop overflow)
    REAL scale = (W/iw > H/ih) ? W/iw : H/ih;
    REAL dw=iw*scale, dh=ih*scale;
    REAL dx=rc.left+(W-dw)/2, dy=rc.top+(H-dh)/2;
    g.DrawImage(&img, RectF(dx,dy,dw,dh), 0,0,iw,ih, UnitPixel);
    g.ResetClip();
    return true;
}
