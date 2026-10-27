#include "achievement_ui.h"
#include "nobk_panel_visuals.h"
#include "config_file.h"
#include "overlay_theme.h"
#include <imgui.h>
#include <rex/ui/immediate_drawer.h>
#include <rex/ui/overlay/achievement_icon_cache.h>
#include <rex/logging.h>
#include <rex/cvar.h>
#include <rex/ui/keybinds.h>
#include <rex/ui/windowed_app_context.h>
#include <toml++/toml.hpp>
#include <algorithm>
#include <chrono>
#include <fstream>
#include <string_view>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
namespace motogp::achievements {
namespace {
using namespace motogp::overlay;
void Trophy(ImVec2 pos,float size,ImU32 color) {
  auto* draw=ImGui::GetWindowDrawList();
  const auto p=[&](float x,float y){return ImVec2(pos.x+x*size,pos.y+y*size);};
  draw->AddRectFilled(p(.25f,.15f),p(.75f,.57f),color,size*.10f,ImDrawFlags_RoundCornersBottom);
  draw->AddRect(p(.10f,.22f),p(.9f,.44f),color,size*.10f,0,size*.065f);
  draw->AddRectFilled(p(.45f,.55f),p(.55f,.77f),color);
  draw->AddRectFilled(p(.28f,.78f),p(.72f,.85f),color,size*.03f);
}
void Icon(const rex::system::AchievementInfo& info,bool unlocked,float size,rex::ui::AchievementIconCache* icons) {
  auto* texture=icons?icons->GetIcon(info):nullptr;
  if(texture) ImGui::ImageWithBg(reinterpret_cast<ImTextureID>(texture),ImVec2(size,size),ImVec2(0,0),ImVec2(1,1),ImVec4(0,0,0,0),ImVec4(1,1,1,1));
  else {Trophy(ImGui::GetCursorScreenPos(),size,ImGui::GetColorU32(unlocked?orange:muted)); ImGui::Dummy(ImVec2(size,size));}
}
std::string Fold(std::string_view value) {
  std::string out;out.reserve(value.size());
  for(size_t i=0;i<value.size();++i){
    uint32_t code=static_cast<unsigned char>(value[i]);
    if((code&0xe0)==0xc0&&i+1<value.size()&&(static_cast<unsigned char>(value[i+1])&0xc0)==0x80)
      code=((code&0x1f)<<6)|(static_cast<unsigned char>(value[++i])&0x3f);
    else if(code>=0x80){out+=value[i];continue;}
    // Search tolerates all Turkish I variants, including an English catalog
    // displayed with Turkish UI. Do not change the original display strings.
    if(code>='A'&&code<='Z')code+='a'-'A';
    else if(code==0x130||code==0x131)code='i';
    else if((code>=0xc0&&code<=0xd6)||(code>=0xd8&&code<=0xde))code+=0x20;
    else if(code==0x11e||code==0x15e||code==0x152)++code;
    if(code<0x80)out+=static_cast<char>(code);
    else {out+=static_cast<char>(0xc0|(code>>6));out+=static_cast<char>(0x80|(code&0x3f));}
  }
  return out;
}
}
void Preferences::Load(const std::filesystem::path& user_root,const std::filesystem::path& executable_root) {
  path=user_root/"achievement-ui.toml";
  try {baseline=ReadOptions(path);popup=baseline.popup;sound=baseline.sound;volume=baseline.volume;}
  catch(const std::exception& e) {REXLOG_WARN("Could not load achievement preferences: {}",e.what());}
  try {const auto file=executable_root/"config.json";
    if(std::filesystem::is_regular_file(file))turkish=motogp::config::Load(file)["launcher"]["language"].value_or(std::string("en"))=="tr";
    else if(std::filesystem::is_regular_file(executable_root/"motogp0910-launcher.toml"))turkish=toml::parse_file((executable_root/"motogp0910-launcher.toml").string())["language"].value_or(std::string("en"))=="tr";
  }
  catch(const std::exception&) {turkish=false;}
}
bool Preferences::Save() const {
  const Options now{popup.load(),sound.load(),volume.load()};
  if(!WriteOptions(path,baseline,now))return false;
  baseline=now;return true;
}
void Preferences::Refresh() {
  const auto now=std::chrono::steady_clock::now();
  if(path.empty()||now-refreshed<std::chrono::milliseconds(500))return;
  refreshed=now;
  // Don't replace a slider edit before its deactivation/save event.
  if(Options{popup.load(),sound.load(),volume.load()}!=baseline)return;
  try {baseline=ReadOptions(path);popup=baseline.popup;sound=baseline.sound;volume=baseline.volume;}
  catch(const std::exception&) {} // A transient write failure preserves the last good preferences.
}
Summary Progress(const rex::system::AchievementManager& manager) {
  Summary out;auto list=manager.ListAchievements();out.total=list.size();
  for(const auto& info:list) {out.available_score+=info.gamerscore; if(manager.IsUnlocked(info.id)) {++out.earned;out.score+=info.gamerscore;}}
  return out;
}
bool Matches(const rex::system::AchievementInfo& info,bool unlocked,const MenuState& state) {
  if((state.filter==1&&!unlocked)||(state.filter==2&&unlocked)) return false;
  const auto search=Fold(state.search);
  return search.empty() || Fold(info.label).find(search)!=std::string::npos || Fold(unlocked?info.description:info.unachieved_description.empty()?info.description:info.unachieved_description).find(search)!=std::string::npos;
}
namespace {
std::string UnlockDate(uint64_t filetime,bool tr) {
  constexpr uint64_t epoch=116444736000000000ULL;
  if(filetime<=epoch)return {};
  const auto seconds=(filetime-epoch)/10000000ULL;
  if(seconds>=4102444800ULL)return {};
  const std::chrono::year_month_day date{std::chrono::floor<std::chrono::days>(std::chrono::sys_seconds{std::chrono::seconds{static_cast<int64_t>(seconds)}})};
  constexpr const char* months[]{"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
  constexpr const char* months_tr[]{"Oca","Şub","Mar","Nis","May","Haz","Tem","Ağu","Eyl","Eki","Kas","Ara"};
  return std::to_string(unsigned(date.day()))+" "+(tr?months_tr[unsigned(date.month())-1]:months[unsigned(date.month())-1]);
}
void DrawApexContents(rex::system::AchievementManager& manager,MenuState& state,Preferences& preferences,
    rex::ui::AchievementIconCache* icons,float bottom_space,const motogp::nobk::visual::AchievementAppearance& appearance) {
  namespace visual=motogp::nobk::visual;
  preferences.Refresh();const bool tr=preferences.turkish;
  const float s=appearance.scale,alpha=appearance.alpha;
  ImFont* regular=appearance.regular?appearance.regular:ImGui::GetFont();
  ImFont* medium=appearance.semibold?appearance.semibold:regular;
  ImFont* bold=appearance.bold?appearance.bold:medium;
  const auto top=ImGui::GetCursorScreenPos();const float width=ImGui::GetContentRegionAvail().x,right=top.x+width;
  auto label=[&](const std::string& text,ImVec2 pos,float size,ImVec4 color,ImFont* font,float end) {
    visual::Label(ImGui::GetWindowDrawList(),font,size,pos,color,text,end,s,alpha);
  };
  auto right_label=[&](const std::string& text,float end,float y,float size,ImVec4 color,ImFont* font) {
    label(text,ImVec2(end-visual::TextWidth(font,size,text),y),size,color,font,end);
  };
  ImGui::PushFont(regular,30*s);
  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,ImVec2(7*s,6*s));
  ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding,3*s);
  const auto summary=Progress(manager);const auto catalog=manager.ListAchievements();
  label(Tr(tr,"ACHIEVEMENTS","BAŞARIMLAR"),top,56*s,white,bold,right);
  const auto count=std::to_string(summary.earned)+" / "+std::to_string(summary.total)+" "+Tr(tr,"UNLOCKED","KAZANILDI");
  label(count,ImVec2(top.x,top.y+72*s),33*s,white,medium,right-250*s);
  const auto percent=summary.total?summary.earned*100/summary.total:0;
  label(std::to_string(percent)+"%",ImVec2(top.x+visual::TextWidth(medium,33*s,count)+30*s,top.y+77*s),28*s,orange,bold,right-250*s);
  const auto score=std::to_string(summary.score)+" / "+std::to_string(summary.available_score)+" G";
  right_label(score,right,top.y+77*s,29*s,muted,medium);
  auto* draw=ImGui::GetWindowDrawList();
  const ImVec2 progress(top.x,top.y+127*s),progress_end(right,top.y+135*s);
  draw->AddRectFilled(progress,progress_end,visual::Color(182,188,199,.24f*alpha),4*s);
  if(summary.total&&summary.earned)draw->AddRectFilled(progress,ImVec2(top.x+width*float(summary.earned)/summary.total,progress_end.y),ImGui::GetColorU32(orange),4*s);
  const char* filters[]{Tr(tr,"ALL","TÜMÜ"),Tr(tr,"UNLOCKED","KAZANILAN"),Tr(tr,"LOCKED","KİLİTLİ")};
  constexpr float filter_x[]{0,110,310},filter_w[]{86,186,144};
  bool changed=false;
  ImGui::PushStyleColor(ImGuiCol_Button,ImVec4(0,0,0,0));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered,AccentTint(.10f,.35f));
  ImGui::PushStyleColor(ImGuiCol_ButtonActive,AccentTint(.18f,.5f));
  for(int i=0;i<3;++i) {
    ImGui::SetCursorScreenPos(ImVec2(top.x+filter_x[i]*s,top.y+160*s));
    ImGui::PushStyleColor(ImGuiCol_Text,state.filter==i?white:muted);
    if(ImGui::Button(filters[i],ImVec2(filter_w[i]*s,42*s))){state.filter=i;state.selected_id=0;changed=true;}
    ImGui::PopStyleColor();
    if(state.filter==i)draw->AddLine(ImVec2(top.x+filter_x[i]*s,top.y+202*s),ImVec2(top.x+(filter_x[i]+filter_w[i])*s,top.y+202*s),ImGui::GetColorU32(orange),2*s);
  }
  ImGui::PopStyleColor(3);
  const ImVec2 search(top.x+560*s,top.y+160*s);
  draw->AddCircle(ImVec2(search.x+9*s,search.y+17*s),8*s,ImGui::GetColorU32(muted),0,2*s);
  draw->AddLine(ImVec2(search.x+15*s,search.y+23*s),ImVec2(search.x+24*s,search.y+32*s),ImGui::GetColorU32(muted),2*s);
  ImGui::SetCursorScreenPos(ImVec2(search.x+34*s,search.y));ImGui::SetNextItemWidth(std::max(40*s,right-search.x-34*s));
  ImGui::PushStyleColor(ImGuiCol_FrameBg,ImVec4(0,0,0,0));
  ImGui::PushStyleColor(ImGuiCol_FrameBgHovered,ImVec4(0,0,0,0));
  ImGui::PushStyleColor(ImGuiCol_FrameBgActive,ImVec4(0,0,0,0));
  changed|=ImGui::InputTextWithHint("##achievement_search",Tr(tr,"Search achievements...","Başarımlarda ara..."),state.search,sizeof(state.search));
  ImGui::PopStyleColor(3);
  draw->AddLine(ImVec2(search.x,top.y+202*s),ImVec2(right,top.y+202*s),visual::Color(217,221,226,.25f*alpha),s);
  const auto selected=std::find_if(catalog.begin(),catalog.end(),[&](const auto& info){return info.id==state.selected_id&&Matches(info,manager.IsUnlocked(info.id),state);});
  if(selected==catalog.end()) {
    state.selected_id=0;
    for(const auto& info:catalog)if(Matches(info,manager.IsUnlocked(info.id),state)){state.selected_id=info.id;break;}
  }
  const char* popup_label=Tr(tr,"Pop-up in game","Oyun içi bildirim");
  const char* sound_label=Tr(tr,"Unlock sound","Başarım sesi");
  const float controls_width=ImGui::CalcTextSize(popup_label).x+ImGui::CalcTextSize(sound_label).x+2*ImGui::GetFrameHeight()+70*s;
  const bool compact=width-controls_width<150*s;
  const float footer=ImGui::GetFrameHeightWithSpacing()*(compact?2:1)+16*s;
  ImGui::SetCursorScreenPos(ImVec2(top.x-32*s,top.y+212*s));
  ImGui::BeginChild("##achievement_list",ImVec2(width+32*s,std::max(100*s,ImGui::GetContentRegionAvail().y-footer-bottom_space-12*s)),ImGuiChildFlags_None);
  if(changed)ImGui::SetScrollY(0);
  size_t shown=0;
  for(const auto& info:catalog) {
    const bool earned=manager.IsUnlocked(info.id);if(!Matches(info,earned,state))continue;++shown;
    const auto& description=earned?info.description:info.unachieved_description.empty()?info.description:info.unachieved_description;
    const float available=ImGui::GetContentRegionAvail().x-32*s,text_width=std::max(100*s,available-256*s);
    const float title_height=visual::WrappedHeight(bold,32*s,info.label,text_width);
    const float description_height=visual::WrappedHeight(regular,24*s,description,text_width);
    const float row_height=std::max(98*s,10*s+title_height+description_height+12*s);
    const auto origin=ImGui::GetCursorScreenPos();
    const int visible=std::max(0,int((origin.y-ImGui::GetWindowPos().y)/std::max(1.f,row_height+2*s)));
    const float slide=visual::RowOffset(appearance.now,appearance.opened,appearance.changed,visible,s);
    const ImVec2 pos(origin.x+32*s+slide,origin.y);
    ImGui::PushID(int(info.id));ImGui::SetCursorScreenPos(pos);
    const bool clicked=ImGui::InvisibleButton("##achievement_row",ImVec2(available,row_height),ImGuiButtonFlags_EnableNav);
    const auto mouse=ImGui::GetIO().MouseDelta;
    if(clicked||ImGui::IsItemFocused()||(ImGui::IsItemHovered()&&(mouse.x!=0||mouse.y!=0)))state.selected_id=info.id;
    const bool active=state.selected_id==info.id;
    auto* row_draw=ImGui::GetWindowDrawList();
    if(active){
      visual::Marker(row_draw,ImVec2(pos.x-18*s,pos.y+row_height*.5f),s,alpha);
      visual::Underline(row_draw,ImVec2(pos.x,pos.y+row_height-2*s),origin.x+32*s+available,s,alpha);
    } else row_draw->AddLine(ImVec2(pos.x,pos.y+row_height-2*s),ImVec2(origin.x+32*s+available,pos.y+row_height-2*s),visual::Color(217,221,226,.18f*alpha),s);
    ImGui::SetCursorScreenPos(ImVec2(pos.x+7*s,pos.y+(row_height-78*s)*.5f));Icon(info,earned,78*s,icons);
    visual::WrappedLabel(row_draw,bold,32*s,ImVec2(pos.x+126*s,pos.y+10*s),white,info.label,text_width);
    visual::WrappedLabel(row_draw,regular,24*s,ImVec2(pos.x+126*s,pos.y+10*s+title_height),muted,description,text_width);
    right_label(std::to_string(info.gamerscore)+" G",origin.x+32*s+available-10*s,pos.y+16*s,31*s,white,bold);
    auto status=earned?UnlockDate(manager.GetUnlockTime(info.id),tr):std::string{};
    if(status.empty())status=Tr(tr,earned?"UNLOCKED":"LOCKED",earned?"KAZANILDI":"KİLİTLİ");
    right_label(status,origin.x+32*s+available-10*s,pos.y+58*s,20*s,earned?orange:muted,regular);
    ImGui::SetCursorScreenPos(origin);ImGui::Dummy(ImVec2(available+32*s,row_height));ImGui::PopID();
  }
  if(!shown)ImGui::TextDisabled("%s",Tr(tr,summary.total?"No achievements match this filter.":"Achievement catalog is not available yet.",summary.total?"Bu filtreye uygun başarım bulunamadı.":"Başarım listesi henüz hazır değil."));
  ImGui::EndChild();
  ImGui::SetCursorScreenPos(ImVec2(top.x,ImGui::GetCursorScreenPos().y));ImGui::Dummy(ImVec2(0,10*s));
  bool popup=preferences.popup.load();if(ImGui::Checkbox(popup_label,&popup)){preferences.popup=popup;preferences.Save();}
  ImGui::SameLine(0,22*s);
  bool sound=preferences.sound.load();if(ImGui::Checkbox(sound_label,&sound)){preferences.sound=sound;preferences.Save();}
  if(!compact)ImGui::SameLine(0,22*s);
  const float volume_right=ImGui::GetCursorScreenPos().x+ImGui::GetContentRegionAvail().x;
  ImGui::SetNextItemWidth(std::max(40*s,ImGui::GetContentRegionAvail().x-72*s));int volume=preferences.volume.load();
  ImGui::PushStyleVar(ImGuiStyleVar_GrabMinSize,16*s);
  for(const auto color:{ImGuiCol_FrameBg,ImGuiCol_FrameBgHovered,ImGuiCol_FrameBgActive,ImGuiCol_SliderGrab,ImGuiCol_SliderGrabActive})
    ImGui::PushStyleColor(color,ImVec4(0,0,0,0));
  auto* footer_draw=ImGui::GetWindowDrawList();const int slider_start=footer_draw->VtxBuffer.Size;
  if(ImGui::SliderInt("##achievement_volume",&volume,0,100,"%d%%"))preferences.volume=volume;
  if(ImGui::IsItemDeactivatedAfterEdit())preferences.Save();
  ImGui::PopStyleColor(5);
  const bool editing=ImGui::IsItemActive()&&ImGui::GetIO().WantTextInput;
  if(!editing) {
    const auto white=ImGui::GetIO().Fonts->TexUvWhitePixel;
    // Retain the native slider's input and focus handling, replace only its
    // normal value text with the right-aligned percentage. Ctrl+click numeric
    // editing retains its native text and caret.
    for(int i=slider_start;i<footer_draw->VtxBuffer.Size;++i) {
      auto& vertex=footer_draw->VtxBuffer[i];
      if(vertex.uv.x!=white.x||vertex.uv.y!=white.y)vertex.col&=0x00FFFFFFu;
    }
    const auto a=ImGui::GetItemRectMin(),b=ImGui::GetItemRectMax();
    const float y=(a.y+b.y)*.5f,padding=2+8*s;
    const float first=a.x+padding,last=std::max(first,b.x-padding),handle=first+(last-first)*volume/100.f;
    footer_draw->AddLine(ImVec2(first,y),ImVec2(last,y),visual::Color(190,197,208,.5f*alpha),4*s);
    footer_draw->AddLine(ImVec2(first,y),ImVec2(handle,y),ImGui::GetColorU32(orange),4*s);
    footer_draw->AddCircleFilled(ImVec2(handle,y),8*s,ImGui::GetColorU32(orange));
    right_label(std::to_string(volume)+"%",volume_right,y-12*s,24*s,muted,regular);
  }
  ImGui::PopStyleVar(3);ImGui::PopFont();
}
}
void DrawContents(rex::system::AchievementManager& manager,MenuState& state,Preferences& preferences,rex::ui::AchievementIconCache* icons,float bottom_space,
    const motogp::nobk::visual::AchievementAppearance* appearance) {
  if(appearance){DrawApexContents(manager,state,preferences,icons,bottom_space,*appearance);return;}
  preferences.Refresh();
  const bool tr=preferences.turkish;
  const auto summary=Progress(manager);
  ImGui::Text("%zu / %zu %s",summary.earned,summary.total,Tr(tr,"unlocked","kazanıldı"));
  if(ImGui::GetContentRegionAvail().x>430.f)ImGui::SameLine();
  ImGui::TextColored(orange,"  %llu / %llu G",static_cast<unsigned long long>(summary.score),static_cast<unsigned long long>(summary.available_score));
  ImGui::ProgressBar(summary.total?float(summary.earned)/summary.total:0,ImVec2(-1,5),"");
  const char* filters[]={Tr(tr,"All","Tümü"),Tr(tr,"Unlocked","Kazanılan"),Tr(tr,"Locked","Kilitli")};
  for(int i=0;i<3;++i) {if(i) ImGui::SameLine(); const bool selected=state.filter==i; if(selected) ImGui::PushStyleColor(ImGuiCol_Button,AccentTint(.24f));
    if(ImGui::Button(filters[i])) state.filter=i;
    if(selected) ImGui::PopStyleColor();
  }
  ImGui::SetNextItemWidth(-1); ImGui::InputTextWithHint("##achievement_search",Tr(tr,"Search achievements...","Başarımlarda ara..."),state.search,sizeof(state.search));
  const bool compact=ImGui::GetContentRegionAvail().x<560.f;
  const float controls_height=ImGui::GetFrameHeightWithSpacing()*(compact?2.f:1.f)+12.f;
  ImGui::BeginChild("##achievement_list",ImVec2(0,std::max(40.f,ImGui::GetContentRegionAvail().y-controls_height-bottom_space)),ImGuiChildFlags_None);
  size_t shown=0;
  for(const auto& info:manager.ListAchievements()) {
    const bool earned=manager.IsUnlocked(info.id); if(!Matches(info,earned,state)) continue; ++shown;
    ImGui::PushID(static_cast<int>(info.id));
    const auto& description=earned?info.description:info.unachieved_description.empty()?info.description:info.unachieved_description;
    const float scale=appearance?appearance->scale:1.f;
    const float available=ImGui::GetContentRegionAvail().x;
    const float text_width=std::max(80.f,available-130.f*scale);
    const float title_height=ImGui::CalcTextSize(info.label.c_str(),nullptr,false,text_width).y;
    const float description_height=ImGui::CalcTextSize(description.c_str(),nullptr,false,text_width).y;
    const float row_height=std::max((appearance?86.f:96.f)*scale,title_height+description_height+46.f*scale);
    const auto origin=ImGui::GetCursorScreenPos();bool highlighted=false;
    if(appearance) {
      const int visible=std::max(0,int((origin.y-ImGui::GetWindowPos().y)/(row_height+ImGui::GetStyle().ItemSpacing.y)));
      const float slide=motogp::nobk::visual::RowOffset(appearance->now,appearance->opened,appearance->changed,visible,scale);
      const ImVec2 a(origin.x+slide,origin.y),b(a.x+available,a.y+row_height);
      highlighted=ImGui::IsMouseHoveringRect(a,b);
      motogp::nobk::visual::Strip(ImGui::GetWindowDrawList(),a,b,26.f*scale,highlighted,appearance->alpha);
      ImGui::SetCursorScreenPos(a);ImGui::PushStyleColor(ImGuiCol_ChildBg,ImVec4(0,0,0,0));
      ImGui::PushStyleColor(ImGuiCol_Text,white);
    }
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding,appearance?0.f:8.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,appearance?ImVec2(40.f*scale,10.f*scale):ImVec2(14,12));
    ImGui::BeginChild("##row",ImVec2(appearance?available:0,row_height),ImGuiChildFlags_AlwaysUseWindowPadding|(appearance?ImGuiChildFlags_None:ImGuiChildFlags_Borders),ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse);
    Icon(info,earned,(appearance?40.f:48.f)*scale,icons);ImGui::SameLine();
    ImGui::BeginGroup(); ImGui::PushTextWrapPos(ImGui::GetCursorPosX()+text_width);
    ImGui::TextUnformatted(info.label.c_str()); ImGui::TextColored(muted,"%s",description.c_str());
    ImGui::TextColored(earned?orange:muted,"%s  ·  %u G",Tr(tr,earned?"UNLOCKED":"LOCKED",earned?"KAZANILDI":"KİLİTLİ"),info.gamerscore);
    ImGui::PopTextWrapPos();ImGui::EndGroup(); ImGui::EndChild(); ImGui::PopStyleVar(2);
    if(appearance) {
      ImGui::PopStyleColor(2);
      const auto next=ImGui::GetCursorScreenPos();
      ImGui::SetCursorScreenPos(ImVec2(origin.x,next.y-ImGui::GetStyle().ItemSpacing.y));
      ImGui::Dummy(ImVec2(0,0));
    }
    ImGui::PopID();
  }
  if(!shown) {ImGui::Spacing(); ImGui::TextDisabled("%s",Tr(tr,summary.total?"No achievements match this filter.":"Achievement catalog is not available yet.",summary.total?"Bu filtreye uygun başarım bulunamadı.":"Başarım listesi henüz hazır değil."));}
  ImGui::EndChild();
  ImGui::Separator();
  bool popup=preferences.popup.load(); if(ImGui::Checkbox(Tr(tr,"Pop-up in game","Oyun içi bildirim"),&popup)) {preferences.popup=popup; preferences.Save();}
  ImGui::SameLine();
  bool sound=preferences.sound.load(); if(ImGui::Checkbox(Tr(tr,"Unlock sound","Başarım sesi"),&sound)) {preferences.sound=sound; preferences.Save();}
  if(!compact)ImGui::SameLine();
  int volume=preferences.volume.load(); ImGui::SetNextItemWidth((appearance||compact)?-1.f:170.f);
  if(ImGui::SliderInt("##achievement_volume",&volume,0,100,"%d%%")) preferences.volume=volume;
  if(ImGui::IsItemDeactivatedAfterEdit()) preferences.Save();
}
bool DrawMenu(rex::system::AchievementManager& manager,MenuState& state,Preferences& preferences,ImFont* font,rex::ui::AchievementIconCache* icons,ImFont* heading) {
  preferences.Refresh();
  Theme theme(font); const bool tr=preferences.turkish; const auto& io=ImGui::GetIO();
  ImVec2 size(std::min(900.f,std::max(280.f,io.DisplaySize.x-40)),std::min(680.f,std::max(260.f,io.DisplaySize.y-40)));
  ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x*.5f,io.DisplaySize.y*.5f),ImGuiCond_Always,ImVec2(.5f,.5f));
  ImGui::SetNextWindowSize(size,ImGuiCond_Always);
  bool close=false;
  if(ImGui::Begin("##motogp_achievements",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoMove)) {
    ImGui::TextColored(orange,"MotoGP 09/10");
    if(heading) ImGui::PushFont(heading,30.f);
    ImGui::TextUnformatted(Tr(tr,"Achievements","Başarımlar"));
    if(heading) ImGui::PopFont();
    ImGui::TextDisabled("%s",Tr(tr,"Your milestones. Every race counts.","Her yarış, yeni bir hedef."));
    ImGui::Spacing();
    DrawContents(manager,state,preferences,icons,ImGui::GetFrameHeightWithSpacing());
    ImGui::TextDisabled("%s",Tr(tr,"F7 / Esc to return to the game","Oyuna dönmek için F7 / Esc"));
    ImGui::SameLine(std::max(0.f,size.x-136.f)); close=ImGui::Button(Tr(tr,"Close","Kapat"),ImVec2(88,0));
    if(ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)&&ImGui::IsKeyPressed(ImGuiKey_Escape,false)) close=true;
  }
  ImGui::End(); return close;
}
void DrawToast(const rex::system::AchievementEvent& event,float alpha,bool tr,ImFont* font,rex::ui::AchievementIconCache* icons) {
  Theme theme(font); const auto& io=ImGui::GetIO(); const float width=std::min(410.f,std::max(240.f,io.DisplaySize.x-32.f));
  const float label_width=width-116;
  const float height=std::max(114.f,104.f+ImGui::CalcTextSize(event.achievement.label.c_str(),nullptr,false,label_width).y);
  ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x-20,io.DisplaySize.y-20),ImGuiCond_Always,ImVec2(1,1));
  ImGui::SetNextWindowSize(ImVec2(width,height),ImGuiCond_Always); ImGui::PushStyleVar(ImGuiStyleVar_Alpha,alpha);
  if(ImGui::Begin("##motogp_achievement_toast",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoNav|ImGuiWindowFlags_NoInputs|ImGuiWindowFlags_NoSavedSettings)) {
    const auto pos=ImGui::GetWindowPos();auto* draw=ImGui::GetWindowDrawList();draw->AddRectFilled(ImVec2(pos.x,pos.y+14),ImVec2(pos.x+4,pos.y+height-14),ImGui::GetColorU32(orange),2);
    Icon(event.achievement,true,48,icons);ImGui::SameLine(); ImGui::BeginGroup();
    ImGui::TextColored(orange,"%s",Tr(tr,"ACHIEVEMENT UNLOCKED","BAŞARIM KAZANILDI"));
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX()+label_width); ImGui::TextUnformatted(event.achievement.label.c_str()); ImGui::PopTextWrapPos();
    ImGui::TextDisabled("+%u G",event.achievement.gamerscore);ImGui::EndGroup();
  }
  ImGui::End();ImGui::PopStyleVar();
}
void ToastQueue::Push(const rex::system::AchievementEvent& event) {
  std::lock_guard lock(mutex_);
  const auto same=[&](const auto& old){return old.achievement.id==event.achievement.id&&old.unlocked_at_filetime==event.unlocked_at_filetime;};
  if(std::any_of(seen_.begin(),seen_.end(),same)||pending_.size()>=64) return;
  seen_.push_back(event);if(seen_.size()>128) seen_.pop_front();pending_.push_back(event);
}
std::optional<VisibleToast> ToastQueue::Next(double now,bool focused) {
  std::lock_guard lock(mutex_);bool fresh=false;
  if(visible_&&now-started_>=4.5) visible_.reset();
  if(!focused) return std::nullopt;
  if(!visible_&&!pending_.empty()) {visible_=std::move(pending_.front());pending_.pop_front();started_=now;fresh=true;}
  if(!visible_) return std::nullopt;
  const double age=std::max(0.0,now-started_);
  const float alpha=static_cast<float>(std::clamp(std::min(age/.25,(4.5-age)/.5),0.0,1.0));
  return VisibleToast{*visible_,alpha,fresh};
}
namespace {
class Menu final:public rex::ui::ImGuiDialog {
 public:
  Menu(rex::ui::ImGuiDrawer* d,rex::ui::ImmediateDrawer* immediate,rex::Runtime* runtime,rex::system::AchievementManager& manager,ImFont* font,ImFont* heading,std::shared_ptr<Preferences> p,CloseRequest close)
      :ImGuiDialog(d),manager_(manager),font_(font),heading_(heading),prefs_(std::move(p)),close_(std::move(close)),icons_(immediate,runtime) {}
  ~Menu() override {prefs_->Save();}
 protected:
  void OnDraw(ImGuiIO&) override {if(DrawMenu(manager_,state_,*prefs_,font_,&icons_,heading_)&&!closing_) {closing_=true;close_(lifetime_);}}
 private:
  rex::system::AchievementManager& manager_; MenuState state_; ImFont* font_; ImFont* heading_; std::shared_ptr<Preferences> prefs_; CloseRequest close_;
  rex::ui::AchievementIconCache icons_; std::shared_ptr<void> lifetime_=std::make_shared<int>(0);bool closing_=false;
};
class Notifications final:public rex::ui::AchievementNotificationDialog {
 public:
  Notifications(rex::ui::ImGuiDrawer* d,rex::ui::ImmediateDrawer* immediate,rex::Runtime* runtime,ImFont* font,std::shared_ptr<Preferences> p)
    :AchievementNotificationDialog(d),font_(font),prefs_(std::move(p)),icons_(immediate,runtime),sound_(prefs_) {}
  void Push(const rex::system::AchievementEvent& event) override {queue_.Push(event);}
 protected:
  void OnDraw(ImGuiIO& io) override {
    prefs_->Refresh();
    const double now=std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
    if(const auto toast=queue_.Next(now,!io.AppFocusLost)) {if(toast->play_sound) sound_.Play();if(prefs_->popup.load())DrawToast(toast->event,toast->alpha,prefs_->turkish,font_,&icons_);}
  }
 private:
  ToastQueue queue_;ImFont* font_;std::shared_ptr<Preferences> prefs_;rex::ui::AchievementIconCache icons_;Sound sound_;
};
}
void RequestClose(rex::ui::WindowedAppContext& context,rex::ui::Window* window,std::weak_ptr<void> lifetime) {
  context.CallInUIThreadDeferred([window,lifetime] {
    if(lifetime.expired()) return;
    const auto key=rex::ui::ParseVirtualKey(rex::cvar::Query<std::string>("bind_achievements"));
    rex::ui::KeyEvent event(window,key,1,false,false,false,false,false);
    rex::ui::ProcessKeyEvent(event);
  });
}
std::unique_ptr<rex::ui::ImGuiDialog> CreateMenu(rex::ui::ImGuiDrawer* drawer,rex::ui::ImmediateDrawer* immediate,rex::Runtime* runtime,rex::system::AchievementManager& manager,ImFont* font,ImFont* heading,std::shared_ptr<Preferences> p,CloseRequest close) {
  return std::make_unique<Menu>(drawer,immediate,runtime,manager,font,heading,std::move(p),std::move(close));
}
std::unique_ptr<rex::ui::AchievementNotificationDialog> CreateNotifications(rex::ui::ImGuiDrawer* drawer,rex::ui::ImmediateDrawer* immediate,rex::Runtime* runtime,ImFont* font,std::shared_ptr<Preferences> p) {
  return std::make_unique<Notifications>(drawer,immediate,runtime,font,std::move(p));
}
}
