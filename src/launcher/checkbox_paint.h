#pragma once
#include <windows.h>
#include <commctrl.h>
#include <objidl.h>
#include <propidl.h>
#include <gdiplus.h>
#include "accent.h"

namespace launcher {
// Keep native keyboard/accessibility activation, but limit mouse activation to
// the painted square. Report the rest of the row as outside the native button
// so dragging out also clears its pressed state and cancels a pending click.
inline bool CheckboxPointerInput(HWND control,UINT message,WPARAM wp,LPARAM lp,
                                 const RECT& box,LRESULT& result){
  result=0;
  if(message==WM_SETCURSOR&&LOWORD(lp)==HTCLIENT){
    POINT point{};GetCursorPos(&point);ScreenToClient(control,&point);
    SetCursor(LoadCursorW(nullptr,IsWindowEnabled(control)&&PtInRect(&box,point)?IDC_HAND:IDC_ARROW));
    result=TRUE;return true;
  }
  if(message!=WM_LBUTTONDOWN&&message!=WM_LBUTTONDBLCLK&&
     message!=WM_LBUTTONUP&&message!=WM_MOUSEMOVE)return false;
  const POINT point{static_cast<short>(LOWORD(lp)),static_cast<short>(HIWORD(lp))};
  const bool inside=PtInRect(&box,point)!=FALSE;
  if((message==WM_LBUTTONDOWN||message==WM_LBUTTONDBLCLK)&&!inside)return true;
  if(message==WM_LBUTTONUP&&!inside){
    // Cancel even when no intervening mouse-move was delivered.
    SendMessageW(control,BM_SETSTATE,FALSE,0);
    if(GetCapture()==control)ReleaseCapture();
    return true;
  }
  if(!inside)lp=MAKELPARAM(-1,-1);
  result=DefSubclassProc(control,message==WM_LBUTTONDBLCLK?WM_LBUTTONDOWN:message,wp,lp);
  return true;
}
inline void PaintCheckbox(HDC dc,const RECT& bounds,bool checked,bool enabled,bool pressed){
  using namespace Gdiplus;
  Graphics g(dc);g.SetSmoothingMode(SmoothingModeAntiAlias);
  const float width=float(bounds.right-bounds.left),height=float(bounds.bottom-bounds.top);
  const float x=float(bounds.left)+.5f,y=float(bounds.top)+.5f,w=width-1,h=height-1,r=width*.22f;
  GraphicsPath shape;
  shape.AddArc(x,y,r*2,r*2,180,90);shape.AddArc(x+w-r*2,y,r*2,r*2,270,90);
  shape.AddArc(x+w-r*2,y+h-r*2,r*2,r*2,0,90);shape.AddArc(x,y+h-r*2,r*2,r*2,90,90);shape.CloseFigure();
  const auto accent=enabled?(pressed?kLauncherAccentPressed:kLauncherAccent):kLauncherAccentDisabled;
  const Color fill=checked?Color(255,GetRValue(accent),GetGValue(accent),GetBValue(accent)):Color(255,24,28,35);
  SolidBrush brush(fill);g.FillPath(&brush,&shape);
  Pen border(checked?fill:Color(255,66,73,85),1.f);g.DrawPath(&border,&shape);
  if(checked){
    Pen tick(enabled?Color(255,13,15,20):Color(255,159,165,176),width*.12f);
    tick.SetStartCap(LineCapRound);tick.SetEndCap(LineCapRound);tick.SetLineJoin(LineJoinRound);
    PointF points[]{{x+width*.23f,y+height*.50f},{x+width*.43f,y+height*.69f},{x+width*.78f,y+height*.29f}};
    g.DrawLines(&tick,points,3);
  }
}
}
