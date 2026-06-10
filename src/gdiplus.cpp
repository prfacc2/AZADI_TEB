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
