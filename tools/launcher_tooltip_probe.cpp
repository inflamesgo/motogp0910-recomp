#include "../src/launcher/tooltips.h"
#include "../src/launcher/checkbox_paint.h"
#include <iostream>
#include <stdexcept>

namespace {
void Check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void Pump(unsigned milliseconds){
  const auto until=GetTickCount64()+milliseconds;
  do{MSG message;while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){TranslateMessage(&message);DispatchMessageW(&message);}Sleep(5);}while(GetTickCount64()<until);
}
LRESULT CALLBACK PointerProc(HWND control,UINT message,WPARAM wp,LPARAM lp,UINT_PTR,DWORD_PTR){
  LRESULT result{};const RECT box{1,5,19,23};
  if(launcher::CheckboxPointerInput(control,message,wp,lp,box,result))return result;
  return DefSubclassProc(control,message,wp,lp);
}
}
int main(){try{
  INITCOMMONCONTROLSEX controls{sizeof(controls),ICC_WIN95_CLASSES};InitCommonControlsEx(&controls);
  WNDCLASSW cls{};cls.lpfnWndProc=DefWindowProcW;cls.hInstance=GetModuleHandleW(nullptr);cls.lpszClassName=L"LauncherTooltipProbe";RegisterClassW(&cls);
  auto owner=CreateWindowExW(0,cls.lpszClassName,L"Tooltip boundary fixture",WS_OVERLAPPEDWINDOW|WS_VISIBLE,20,20,540,190,nullptr,nullptr,cls.hInstance,nullptr);
  const auto owner_error=GetLastError();
  auto control=CreateWindowExW(0,L"BUTTON",L"Show Discord activity",WS_CHILD|WS_VISIBLE|BS_OWNERDRAW,10,20,460,30,owner,nullptr,cls.hInstance,nullptr);
  if(!owner||!control)throw std::runtime_error("Create native fixture windows: owner="+std::to_string(owner_error)+", control="+std::to_string(GetLastError()));
  SetWindowSubclass(control,PointerProc,1,0);ShowWindow(owner,SW_SHOW);SetForegroundWindow(owner);Pump(50);
  launcher::Tooltips help;help.Create(owner,1);
  double scale=1;HFONT font{};
  const auto s=[&](int n){return int(n*scale+.5);};
  auto bounds=[&](HWND child){return launcher::CheckboxTooltipBounds(child,RECT{s(1),s(5),s(19),s(23)},font,s(29));};
  const auto hit=[&](POINT point){TTHITTESTINFOW info{};info.hwnd=control;info.pt=point;info.ti.cbSize=launcher::Tooltips::kInfoSize;return SendMessageW(help.window,TTM_HITTESTW,0,reinterpret_cast<LPARAM>(&info))!=0;};
  const auto configure=[&](double value,const wchar_t* label){
    scale=value;auto previous=font;font=CreateFontW(-s(14),0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,0,0,L"Arial");
    SetWindowPos(control,nullptr,10,20,s(460),s(30),SWP_NOZORDER);SendMessageW(control,WM_SETFONT,reinterpret_cast<WPARAM>(font),TRUE);SetWindowTextW(control,label);
    Check(help.Set(control,L"Controller activity hover help",bounds),"Register bounded native checkbox tooltip");
    if(previous)DeleteObject(previous);
  };
  for(const auto value:{.75,1.,1.5})for(const auto label:{L"Show Discord activity",L"Discord etkinliğini göster"}){
    configure(value,label);RECT client{};GetClientRect(control,&client);
    Check(hit(POINT{s(10),s(14)}),"Painted checkbox square has hover help");
    Check(hit(POINT{s(35),client.bottom/2}),"Painted label has hover help");
    Check(!hit(POINT{client.right-s(4),client.bottom/2}),"Empty row to the right has no hover target");
    Check(!hit(POINT{s(35),0}),"Empty vertical margin has no hover target");
    Check(SendMessageW(help.window,TTM_GETTOOLCOUNT,0,0)==1,"Scale and localization update the existing tooltip");
    SetWindowTextW(control,L"X");Check(!hit(POINT{s(100),client.bottom/2}),"Changing text immediately shrinks the native tooltip rectangle");
  }
  configure(1,L"Show Discord activity");SendMessageW(help.window,TTM_SETDELAYTIME,TTDT_INITIAL,30);SendMessageW(help.window,TTM_SETDELAYTIME,TTDT_RESHOW,30);
  const auto move=[&](POINT point,unsigned wait){ClientToScreen(control,&point);SetCursorPos(point.x,point.y);Pump(wait);};
  move(POINT{430,15},150);Check(!IsWindowVisible(help.window),"Real mouse hover over empty row cannot show a popup");
  move(POINT{10,14},400);Check(IsWindowVisible(help.window),"Real square hover shows the native popup");
  move(POINT{430,15},150);Check(!IsWindowVisible(help.window),"Moving into blank space dismisses the popup");
  move(POINT{35,15},400);Check(IsWindowVisible(help.window),"Real label hover shows help despite square-only mouse activation");
  DestroyWindow(owner);DeleteObject(font);
  std::cout<<"Checkbox tooltip bounds, localization, scale and native hover checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAILED: "<<e.what()<<'\n';return 1;}}
