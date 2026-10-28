#pragma once
#include <windows.h>
#include <commctrl.h>
#include <cstddef>
#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <utility>
namespace launcher {
// Use the system tooltip rather than another painted popup. Owning the strings
// keeps TOOLINFO text valid through localization and native hover callbacks.
class Tooltips {
 public:
  // sizeof(TOOLINFO) includes lpReserved (v6). Our launcher intentionally uses
  // the default common-controls activation context, which can be v5 on Windows.
  // These older sizes contain every field we use and work with both versions.
  static constexpr UINT kInfoSize=offsetof(TOOLINFOW,lParam)+sizeof(LPARAM);
  static constexpr UINT kLegacyInfoSize=offsetof(TOOLINFOW,lpszText)+sizeof(LPWSTR);
  HWND window{};
  Tooltips()=default;
  Tooltips(const Tooltips&)=delete;
  Tooltips& operator=(const Tooltips&)=delete;
  ~Tooltips(){
    for(const auto& [control,entry]:texts_)if(entry.bounds&&IsWindow(control))RemoveWindowSubclass(control,BoundsProc,28);
    if(IsWindow(window))DestroyWindow(window);
  }
  void Create(HWND owner,double scale){
    window=CreateWindowExW(WS_EX_TOPMOST,TOOLTIPS_CLASSW,nullptr,WS_POPUP|TTS_ALWAYSTIP|TTS_NOPREFIX,
        CW_USEDEFAULT,CW_USEDEFAULT,CW_USEDEFAULT,CW_USEDEFAULT,owner,nullptr,GetModuleHandleW(nullptr),nullptr);
    if(!window){OutputDebugStringW(L"MotoGP launcher: hover help unavailable; continuing without tooltips.\n");return;}
    SetWindowSubclass(window,VisibilityProc,27,reinterpret_cast<DWORD_PTR>(this));
    SendMessageW(window,TTM_SETDELAYTIME,TTDT_INITIAL,550);
    SendMessageW(window,TTM_SETDELAYTIME,TTDT_RESHOW,100);
    SendMessageW(window,TTM_SETDELAYTIME,TTDT_AUTOPOP,15000);
    SetScale(scale);
  }
  void SetScale(double scale){if(window)SendMessageW(window,TTM_SETMAXTIPWIDTH,0,static_cast<LPARAM>(360*scale+.5));}
  void Pop()const{if(window)SendMessageW(window,TTM_POP,0,0);}
  void Activate(bool active){active_=active;if(window){Pop();SendMessageW(window,TTM_ACTIVATE,active,0);}}
  using Bounds=std::function<RECT(HWND)>;
  bool Set(HWND control,const wchar_t* text,Bounds bounds={}){
    if(!IsWindow(window)||!IsWindow(control))return false;
    wchar_t cls[32]{};GetClassNameW(control,cls,32);
    if(std::wstring_view(cls)==L"STATIC")SetWindowLongPtrW(control,GWL_STYLE,GetWindowLongPtrW(control,GWL_STYLE)|SS_NOTIFY);
    auto [entry,inserted]=texts_.try_emplace(control,Entry{text,kInfoSize,bounds});
    if(!inserted){
      if(bool(entry->second.bounds)!=bool(bounds)){
        auto previous=Info(control,entry->second);SendMessageW(window,TTM_DELTOOLW,0,reinterpret_cast<LPARAM>(&previous));
        RemoveWindowSubclass(control,BoundsProc,28);inserted=true;
      }
      entry->second.text=text;entry->second.bounds=std::move(bounds);
    }
    auto tool=Info(control,entry->second);
    if(!inserted){
      SendMessageW(window,TTM_UPDATETIPTEXTW,0,reinterpret_cast<LPARAM>(&tool));UpdateRegion(control);return true;
    }
    if(SendMessageW(window,TTM_ADDTOOLW,0,reinterpret_cast<LPARAM>(&tool))){AttachBounds(control,entry->second);return true;}
    tool.cbSize=kLegacyInfoSize;
    if(SendMessageW(window,TTM_ADDTOOLW,0,reinterpret_cast<LPARAM>(&tool))){entry->second.info_size=tool.cbSize;AttachBounds(control,entry->second);return true;}
    // Hover help must never prevent startup, saving, language changes or opening
    // a settings dialog. Remove failed records so a later update can retry.
    texts_.erase(entry);
    OutputDebugStringW(L"MotoGP launcher: one hover hint could not be registered; continuing.\n");return false;
  }
 private:
  bool active_=true;
  static LRESULT CALLBACK VisibilityProc(HWND window,UINT message,WPARAM wp,LPARAM lp,UINT_PTR id,DWORD_PTR data){
    const auto& self=*reinterpret_cast<Tooltips*>(data);
    // A queued hover timer can outlive TTM_ACTIVATE(false). Keep its pending
    // show transaction hidden while a dropdown owns the interaction.
    if(message==WM_WINDOWPOSCHANGING&&!self.active_){auto& pos=*reinterpret_cast<WINDOWPOS*>(lp);if(pos.flags&SWP_SHOWWINDOW)pos.flags=(pos.flags&~SWP_SHOWWINDOW)|SWP_HIDEWINDOW;}
    if(message==WM_NCDESTROY)RemoveWindowSubclass(window,VisibilityProc,id);
    return DefSubclassProc(window,message,wp,lp);
  }
  struct Entry {std::wstring text;UINT info_size=kInfoSize;Bounds bounds;};
  static TOOLINFOW Info(HWND control,Entry& entry){
    TOOLINFOW tool{entry.info_size};tool.uFlags=TTF_SUBCLASS|(entry.bounds?0:TTF_IDISHWND);
    // A rectangle tool must belong to the child itself. IDISHWND deliberately
    // ignores rect and would make the entire wide checkbox row a hover target.
    tool.hwnd=entry.bounds?control:GetParent(control);tool.uId=reinterpret_cast<UINT_PTR>(control);
    tool.lpszText=entry.text.data();if(entry.bounds)tool.rect=entry.bounds(control);return tool;
  }
  void AttachBounds(HWND control,const Entry& entry){if(entry.bounds)SetWindowSubclass(control,BoundsProc,28,reinterpret_cast<DWORD_PTR>(this));}
  void UpdateRegion(HWND control){
    auto entry=texts_.find(control);if(entry==texts_.end()||!entry->second.bounds||!IsWindow(window))return;
    auto tool=Info(control,entry->second);Pop();SendMessageW(window,TTM_NEWTOOLRECTW,0,reinterpret_cast<LPARAM>(&tool));
  }
  static LRESULT CALLBACK BoundsProc(HWND control,UINT message,WPARAM wp,LPARAM lp,UINT_PTR id,DWORD_PTR data){
    const auto result=DefSubclassProc(control,message,wp,lp);
    if(message==WM_SIZE||message==WM_SETFONT||message==WM_SETTEXT)reinterpret_cast<Tooltips*>(data)->UpdateRegion(control);
    if(message==WM_NCDESTROY)RemoveWindowSubclass(control,BoundsProc,id);
    return result;
  }
  std::map<HWND,Entry> texts_;
};
}
