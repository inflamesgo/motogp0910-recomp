#pragma once
#include <array>
#include <algorithm>
#include <string>
#include <string_view>
#include <vector>
namespace launcher {
inline constexpr bool IsLegacyDefaultBinding(unsigned index,std::string_view value){
  switch(index){
    case 0:return value=="Semicolon,Space"||value=="Space";
    case 1:return value=="Quote,Backspace"||value=="Backspace";
    case 2:return value=="L";
    case 3:return value=="P";
    case 4:return value=="Q,I";
    case 5:return value=="E,O";
    case 6:return value=="1";
    case 7:return value=="3";
    case 8:return value=="W";
    case 9:return value=="S";
    case 12:return value=="F";
    case 13:return value=="Up";
    case 14:return value=="Down";
    case 15:return value=="Left";
    case 16:return value=="Right";
    case 17:return value=="K";
    case 18:return value=="Shift+Up";
    case 19:return value=="Shift+Down";
    case 20:return value=="Shift+Left";
    case 21:return value=="Shift+Right";
    case 22:return value=="Z,Tab";
    case 23:return value=="X,Return";
    default:return false;
  }
}
struct BindingSpec { const char* key; const char* defaults; const wchar_t* english; const wchar_t* turkish; };
inline constexpr std::array<BindingSpec,25> kBindings{{
  {"keybind_a", "R,Return", L"Tuck in / Confirm (A/Cross)", L"Kapan / Onayla (A/Cross)"},
  {"keybind_b", "Q,Backspace", L"Gear down / Back (B/Circle)", L"Vites küçült / Geri (B/Circle)"},
  {"keybind_x", "Space", L"Rear brake (X/Square)", L"Arka fren (X/Square)"},
  {"keybind_y", "E", L"Gear up (Y/Triangle)", L"Vites büyüt (Y/Triangle)"},
  {"keybind_left_trigger", "S", L"Brake (LT/L2)", L"Ön fren (LT/L2)"},
  {"keybind_right_trigger", "W", L"Accelerate (RT/R2)", L"Gaz (RT/R2)"},
  {"keybind_left_shoulder", "C", L"Look behind (LB/L1)", L"Arkaya bak (LB/L1)"},
  {"keybind_right_shoulder", "V", L"Change view (RB/R1)", L"Kamera değiştir (RB/R1)"},
  {"keybind_lstick_up", "Up", L"Lean rider forward (LS Up)", L"Sürücüyü öne eğ (LS Yukarı)"},
  {"keybind_lstick_down", "Down", L"Lean rider back (LS Down)", L"Sürücüyü geriye eğ (LS Aşağı)"},
  {"keybind_lstick_left", "A", L"Turn left (LS Left)", L"Sola dön (LS Sol)"},
  {"keybind_lstick_right", "D", L"Turn right (LS Right)", L"Sağa dön (LS Sağ)"},
  {"keybind_lstick_press", "", L"Unassigned (LS/L3)", L"Atanmamış (LS/L3)"},
  {"keybind_rstick_up", "", L"Unassigned (RS Up)", L"Atanmamış (RS Yukarı)"},
  {"keybind_rstick_down", "", L"Unassigned (RS Down)", L"Atanmamış (RS Aşağı)"},
  {"keybind_rstick_left", "", L"Unassigned (RS Left)", L"Atanmamış (RS Sol)"},
  {"keybind_rstick_right", "", L"Unassigned (RS Right)", L"Atanmamış (RS Sağ)"},
  {"keybind_rstick_press", "", L"Unassigned (RS/R3)", L"Atanmamış (RS/R3)"},
  {"keybind_dpad_up", "1", L"Celebration gesture (D-pad Up)", L"Kutlama hareketi (D-pad Yukarı)"},
  {"keybind_dpad_down", "2", L"Change view (D-pad Down)", L"Kamera değiştir (D-pad Aşağı)"},
  {"keybind_dpad_left", "3", L"Apology gesture (D-pad Left)", L"Özür hareketi (D-pad Sol)"},
  {"keybind_dpad_right", "4", L"Angry gesture (D-pad Right)", L"Öfke hareketi (D-pad Sağ)"},
  {"keybind_back", "Tab", L"Second Chance (Back/Select)", L"İkinci Şans (Back/Select)"},
  {"keybind_start", "Escape", L"Pause (Start/Start)", L"Duraklat (Start/Start)"},
  {"keybind_guide", "", L"Unassigned (Guide/PS)", L"Atanmamış (Guide/PS)"},
}};
// Values retain the SDK's comma-separated alternative syntax. Canonicalize
// modifier order/aliases only when editing alternatives, never on config load.
inline std::string CanonicalBindingAlternative(std::string_view value){
  while(!value.empty()&&value.front()==' ')value.remove_prefix(1);
  while(!value.empty()&&value.back()==' ')value.remove_suffix(1);
  bool ctrl=false,alt=false,shift=false;
  for(auto plus=value.find('+');plus!=std::string_view::npos;plus=value.find('+')){
    const auto modifier=value.substr(0,plus);
    if(modifier=="Ctrl"||modifier=="Control")ctrl=true;
    else if(modifier=="Alt")alt=true;
    else if(modifier=="Shift")shift=true;
    else return std::string(value);
    value.remove_prefix(plus+1);
  }
  return std::string(ctrl?"Ctrl+":"")+(alt?"Alt+":"")+(shift?"Shift+":"")+std::string(value);
}
inline std::vector<std::string> BindingAlternatives(std::string_view value){
  std::vector<std::string> result;
  while(!value.empty()){
    const auto comma=value.find(',');auto key=CanonicalBindingAlternative(value.substr(0,comma));
    if(!key.empty())result.push_back(std::move(key));
    if(comma==std::string_view::npos)break;
    value.remove_prefix(comma+1);
  }
  return result;
}
inline std::string JoinBindingAlternatives(const std::vector<std::string>& keys){
  std::string result;
  for(const auto& key:keys){if(!result.empty())result+=',';result+=key;}
  return result;
}
inline std::string AddBindingAlternative(std::string_view value,std::string_view alternative){
  auto keys=BindingAlternatives(value);auto key=CanonicalBindingAlternative(alternative);
  if(!key.empty()&&std::find(keys.begin(),keys.end(),key)==keys.end())keys.push_back(std::move(key));
  return JoinBindingAlternatives(keys);
}
inline std::string RemoveBindingAlternative(std::string_view value,size_t index){
  auto keys=BindingAlternatives(value);
  if(index<keys.size())keys.erase(keys.begin()+index);
  return JoinBindingAlternatives(keys);
}
}
