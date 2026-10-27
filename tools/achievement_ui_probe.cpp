// Windows executable: actual pinned runtime ImGui and real achievement storage.
#include "../src/achievement_ui.h"
#include "../src/audio_settings.h"
#include "../src/nobk_panel.h"
#include "../src/nobk_panel_visuals.h"
#include "../src/nobk_startup_strip.h"
#include "../src/nobk_friend_toast.h"
#include "../src/in_game_settings.h"
#include <rex/cvar.h>
#include <rex/input/input_system.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <rex/ui/imgui_drawer.h>
#include <rex/ui/window.h>
#include <rex/ui/windowed_app_context.h>
#include <rex/ui/keybinds.h>
#include <rex/ui/immediate_drawer.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <set>
// Isolated panel fixture: no guest game, public HTTP requests or quit dialog.
REXCVAR_DEFINE_STRING(network_mode,"offline","Network","Panel mode fixture");
REXCVAR_DEFINE_STRING(online_server,"https://motogp.nobkonline.com","Network","Panel service fixture");
REXCVAR_DEFINE_STRING(lan_player_name,"","Network","Empty name prevents HTTP worker startup");
namespace motogp::quit { bool Pending(){return false;} }
// Isolate the real F1 renderer from guest draw hooks and physical devices.
namespace motogp::prompts {
Style probe_style=Style::Keyboard;
Style CurrentStyle(){return probe_style;}
}
using namespace motogp::achievements;
using rex::system::AchievementInfo;
using rex::system::AchievementEvent;
namespace {
class TestContext final:public rex::ui::WindowedAppContext {
 protected: void NotifyUILoopOfPendingFunctions() override {} void PlatformQuitFromUIThread() override {}
};
int checks=0;
void Check(bool ok,const char* message) {if(!ok) throw std::runtime_error(message);++checks;}
class CapturedTexture final:public rex::ui::ImmediateTexture {
 public:
  CapturedTexture(uint32_t w,uint32_t h,const uint8_t* data):ImmediateTexture(w,h),rgba(data,data+size_t(w)*h*4){}
  std::vector<uint8_t> rgba;
};
class CaptureDrawer final:public rex::ui::ImmediateDrawer {
 public:
  std::unique_ptr<rex::ui::ImmediateTexture> CreateTexture(uint32_t w,uint32_t h,rex::ui::ImmediateTextureFilter,bool,const uint8_t* data) override {
    return std::make_unique<CapturedTexture>(w,h,data);
  }
  void BeginDrawBatch(const rex::ui::ImmediateDrawBatch&) override {}
  void Draw(const rex::ui::ImmediateDraw&) override {}
  void EndDrawBatch() override {}
};
void Dump(const std::filesystem::path& stem) {
  unsigned char* pixels;int w,h;ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&pixels,&w,&h);
  std::ofstream texture(stem.string()+".rgba",std::ios::binary);texture.write(reinterpret_cast<char*>(pixels),w*h*4);
  auto* data=ImGui::GetDrawData();std::ofstream out(stem.string()+".json");
  out<<"{\"size\":["<<data->DisplaySize.x<<','<<data->DisplaySize.y<<"],\"texture\":["<<w<<','<<h<<"],\"lists\":[";
  std::set<ImTextureID> images;
  for(int l=0;l<data->CmdListsCount;++l) {
    if(l)out<<',';const auto* list=data->CmdLists[l];out<<"{\"v\":[";
    for(int i=0;i<list->VtxBuffer.Size;++i) {if(i)out<<',';auto& v=list->VtxBuffer[i];out<<'['<<v.pos.x<<','<<v.pos.y<<','<<v.uv.x<<','<<v.uv.y<<','<<v.col<<']';}
    out<<"],\"i\":[";for(int i=0;i<list->IdxBuffer.Size;++i) {if(i)out<<',';out<<list->IdxBuffer[i];}
    out<<"],\"cmd\":[";
    for(int i=0;i<list->CmdBuffer.Size;++i) {if(i)out<<',';auto& c=list->CmdBuffer[i];const auto tex=c.GetTexID();if(tex>1)images.insert(tex);
      out<<'['<<c.ClipRect.x<<','<<c.ClipRect.y<<','<<c.ClipRect.z<<','<<c.ClipRect.w<<','<<c.IdxOffset<<','<<c.ElemCount<<','<<c.VtxOffset<<','<<tex<<']';}
    out<<"]}";
  }out<<"],\"images\":[";
  bool first=true;
  for(const auto id:images) {
    const auto* image=reinterpret_cast<const CapturedTexture*>(id);
    const auto file=stem.filename().string()+"-"+std::to_string(id)+".rgba";
    std::ofstream bytes(stem.parent_path()/file,std::ios::binary);bytes.write(reinterpret_cast<const char*>(image->rgba.data()),image->rgba.size());
    if(!first)out<<',';first=false;
    out<<'['<<id<<','<<image->width<<','<<image->height<<",\""<<file<<"\"]";
  }
  out<<"]}";
}

#ifdef MOTOGP_NOBK_VISUAL_PROBE
using rex::X_STATUS;
using rex::X_RESULT;
class StripDriver final:public rex::input::InputDriver {
 public:
  rex::input::X_INPUT_STATE state{};
  StripDriver():InputDriver(nullptr,0){}
  rex::X_STATUS Setup()override{return X_STATUS_SUCCESS;}
  void EnumerateDevices(std::vector<rex::input::DeviceInfo>& devices)override{
    rex::input::DeviceInfo info{};info.id=static_cast<rex::input::DeviceId>(1);devices.push_back(info);
  }
  rex::X_RESULT GetDeviceState(rex::input::DeviceId,rex::input::X_INPUT_STATE* out)override{*out=state;return X_ERROR_SUCCESS;}
  rex::X_RESULT GetDeviceCapabilities(rex::input::DeviceId,uint32_t,rex::input::X_INPUT_CAPABILITIES* out)override{*out={};return X_ERROR_SUCCESS;}
  rex::X_RESULT SetDeviceVibration(rex::input::DeviceId,rex::input::X_INPUT_VIBRATION*)override{return X_ERROR_SUCCESS;}
  rex::X_RESULT GetDeviceKeystroke(rex::input::DeviceId,uint32_t,rex::input::X_INPUT_KEYSTROKE*)override{return X_ERROR_EMPTY;}
};
int StartupStripChecks(const char* font_path,const char* output) {
  const std::filesystem::path out=output;
  Check(!std::filesystem::exists(out),"Startup strip checks use a new isolated fixture");std::filesystem::create_directories(out);
  for(int scenario=0;scenario<4;++scenario){
    rex::cvar::SetFlagByName("network_mode",scenario==0?"offline":scenario==1?"lan":"online");
    rex::cvar::SetFlagByName("online_server",scenario==3?"https://custom.invalid":"https://motogp.nobkonline.com");
    TestContext context;auto window=rex::ui::Window::Create(context,"Startup strip fixture");
    rex::input::InputSystem input(nullptr);auto driver=std::make_unique<StripDriver>();auto* pad=driver.get();
    input.AddDriver(std::move(driver));input.SetDeviceAssignment(std::make_unique<rex::input::SlotAssignment>());
    Check(input.Setup()==X_STATUS_SUCCESS,"Real controller assignment initializes");
    auto gate=std::make_shared<motogp::settings::InputGate>();gate->Initialize(&input);gate->SetFocused(true);
    motogp::settings::SetGuestGate(nullptr,nullptr,gate.get());
    rex::ui::ImGuiDrawer drawer(window.get(),100);auto& io=drawer.GetIO();
    io.IniFilename=nullptr;io.LogFilename=nullptr;io.DisplaySize={1920,1080};io.DeltaTime=1.f/60;
    const ImWchar ranges[]{0x20,0x1ff,0};
    auto* font=io.Fonts->AddFontFromFileTTF(font_path,48,nullptr,ranges);
    font->Flags|=ImFontFlags_LockBakedSizes;
    motogp::nobk::ConfigureFonts(io.Fonts,font_path,font_path,font_path);
    unsigned char* pixels;int width,height;io.Fonts->GetTexDataAsRGBA32(&pixels,&width,&height);io.Fonts->SetTexID(1);
    int renderer_errors=0;
    ImGui::GetCurrentContext()->ErrorCallback=[](ImGuiContext*,void* value,const char*){++*static_cast<int*>(value);};
    ImGui::GetCurrentContext()->ErrorCallbackUserData=&renderer_errors;
    rex::system::AchievementManager manager;auto preferences=std::make_shared<Preferences>();preferences->sound=false;
    motogp::nobk::Initialize(nullptr,gate);motogp::nobk::SetVisualInput(&input);
    motogp::nobk::SetVisualFixture(false);
    auto menu=motogp::nobk::CreateMenu(&drawer,nullptr,&manager,font,font,false,nullptr,nullptr,preferences);
    auto frame=[&]{gate->PollGuest();drawer.GetIO();
      Check(io.Fonts->IsBuilt(),"Startup strip cannot invalidate the runtime's once-uploaded font atlas");
      ImGui::NewFrame();menu->Draw();ImGui::Render();
      Check(io.Fonts->IsBuilt(),"Startup labels and font measurements preserve the uploaded atlas");
      Check(renderer_errors==0,"Startup strip submits valid production ImGui geometry without recovered errors");};
    frame();
    Check(ImGui::GetDrawData()->TotalVtxCount==0,"A missing local account name does not consume or show the startup strip");
    if(scenario!=2){
      for(int i=0;i<210;++i)frame();
      Check(ImGui::GetDrawData()->TotalVtxCount==0,"Offline, LAN and custom services show no production NOBK startup strip");
    }else{
      motogp::nobk::SetVisualFixture(true,true);
      io.DeltaTime=4.f;frame();io.DeltaTime=1.f/60;
      Check(ImGui::GetDrawData()->TotalVtxCount>0,"Signed-in production snapshot shows startup strip without opening F1");Dump(out/"entering");
      int entrance_alpha=0;
      for(const auto* list:ImGui::GetDrawData()->CmdLists)
        for(const auto& vertex:list->VtxBuffer)entrance_alpha=std::max(entrance_alpha,int(vertex.col>>24));
      Check(entrance_alpha>0&&entrance_alpha<64,"A four-second first frame cannot skip the startup entrance");
      for(int i=0;i<30;++i)frame();Dump(out/"steady");
      io.DeltaTime=4.f;frame();io.DeltaTime=1.f/60;
      Check(ImGui::GetDrawData()->TotalVtxCount>0,"A later loading stall cannot consume the startup strip's remaining lifetime");
      Check(!gate->Blocking()&&!io.WantCaptureKeyboard&&!io.WantCaptureMouse,"Passive startup strip captures no guest keyboard, mouse or controller input");
      for(auto size:{ImVec2(1280,720),ImVec2(2560,1080),ImVec2(320,240)}){
        const auto layout=motogp::nobk::startup::Measure(font,size,1,123,2,"PageDown","Back'i basılı tut",true);
        Check(layout.minimum.x>=0&&layout.maximum.x<=size.x&&layout.minimum.y>=0&&layout.maximum.y<size.y,
              "Long localized labels, remapped keys and pending invites fit narrow and wide viewports");
        Check(layout.maximum.y<size.y*.25f&&std::abs(layout.minimum.x+layout.maximum.x-size.x)<.01f,
              "Startup strip stays centered at the top without entering the main panel area");
      }
      gate->Open();for(int i=0;i<22;++i)frame();gate->Close();gate->PollGuest();
      for(int i=0;i<24;++i)frame();Check(ImGui::GetDrawData()->TotalVtxCount>0,"Closing F1 before expiry may finish the same startup presentation");
      for(int i=0;i<150;++i)frame();Check(ImGui::GetDrawData()->TotalVtxCount==0,"Startup strip expires after its three-second window and exit fade");
      for(int i=0;i<90;++i)frame();Check(ImGui::GetDrawData()->TotalVtxCount==0,"Startup strip never restarts each frame or on later snapshots");
      motogp::nobk::SetVisualFixture(false);
      auto poll=[&](uint16_t buttons,int milliseconds){pad->state.gamepad.buttons=buttons;
        const auto until=std::chrono::steady_clock::now()+std::chrono::milliseconds(milliseconds);
        do{motogp::nobk::PollPad();gate->PollGuest();std::this_thread::sleep_for(std::chrono::milliseconds(10));}while(std::chrono::steady_clock::now()<until);};
      using namespace rex::input;
      poll(0,80);poll(X_INPUT_GAMEPAD_START,550);Check(!gate->IsOpen(),"Start alone retains its native function");
      poll(0,80);poll(X_INPUT_GAMEPAD_BACK,200);Check(!gate->IsOpen(),"Short Back press does not open F1");
      poll(X_INPUT_GAMEPAD_BACK,400);Check(gate->IsOpen(),"Holding Back alone opens the actual production panel gate");
      gate->Close();poll(X_INPUT_GAMEPAD_BACK,600);Check(!gate->IsOpen(),"Closing while Back remains held cannot immediately reopen the panel");
      gate->SetFocused(false);poll(0,80);poll(X_INPUT_GAMEPAD_BACK,550);Check(!gate->IsOpen(),"Unfocused Back cannot open the panel");
      gate->SetFocused(true);poll(X_INPUT_GAMEPAD_BACK,550);Check(!gate->IsOpen(),"Refocusing requires release before rearming held Back");
      poll(0,80);poll(X_INPUT_GAMEPAD_BACK,600);Check(gate->IsOpen(),"Release and a new Back hold rearm the shortcut");gate->Close();poll(0,80);
    }
    menu.reset();motogp::nobk::SetVisualFixture(false);motogp::nobk::Shutdown();gate->PollGuest();motogp::settings::SetGuestGate(nullptr);
  }
  std::cout<<"Startup strip lifecycle, renderer and held-Back checks passed\n";return 0;
}
#include "nobk_toast_checks.inc"
#endif
}
int main(int argc,char** argv) {try {
#ifdef MOTOGP_NOBK_VISUAL_PROBE
  if(argc==4&&std::string_view(argv[3])=="--startup-strip-only")return StartupStripChecks(argv[1],argv[2]);
  if(argc==4&&std::string_view(argv[3])=="--friend-toast-only")return FriendToastChecks(argv[1],argv[2]);
#endif
  Check(argc==3,"Usage: achievement_ui_probe font.ttf output-directory");
  const std::filesystem::path out=argv[2];std::filesystem::create_directories(out);
  Preferences preferences;preferences.path=out/"fixture-preferences.toml";preferences.sound=false;preferences.volume=38;
  Check(preferences.Save(),"Preferences save atomically");Preferences loaded;loaded.Load(out,out);
  // The production filename is separate from the test fixture; load explicitly from an isolated root.
  std::filesystem::copy_file(preferences.path,out/"achievement-ui.toml",std::filesystem::copy_options::overwrite_existing);
  loaded.Load(out,out);Check(!loaded.sound && loaded.volume==38,"Sound preferences survive reload");
  {std::ofstream t(out/"achievement-ui.toml");t<<"sound_enabled = true\nsound_volume = 1000\n";}
  loaded.Load(out,out);Check(loaded.volume==100,"Invalid volume clamps safely");
  {std::ofstream t(out/"motogp0910-launcher.toml");t<<"language = 'tr'\n";}
  loaded.Load(out,out);Check(loaded.turkish,"Launcher language reused");
  { // Two independent UI processes edit different fields from a stale baseline.
    const auto file=out/"shared-preferences.toml";
    {std::ofstream f(file);f<<"future_setting='preserve'\nsound_volume=25\n";}
    const auto old=ReadOptions(file);auto launcher=old,overlay=old;launcher.popup=false;overlay.volume=13;
    Check(WriteOptions(file,old,launcher)&&WriteOptions(file,old,overlay),"Launcher and overlay merge independent preference changes");
    auto merged=ReadOptions(file);Check(!merged.popup&&merged.sound&&merged.volume==13,"Stale writer cannot replace other UI's preference changes");
    Check(toml::parse_file(file.string())["future_setting"].value_or(std::string{})=="preserve","Unknown preference fields survive");
    Preferences live;live.Load(out,out);auto changed=ReadOptions(live.path),next=changed;next.popup=false;next.sound=false;next.volume=9;
    Check(WriteOptions(live.path,changed,next),"External launcher change saved");live.Refresh();
    Check(!live.popup&&!live.sound&&live.volume==9,"Running overlay reloads launcher notification and sound changes");
    {std::ofstream f(file);f<<"[broken";}Check(!WriteOptions(file,old,launcher),"Corrupt preferences are never silently overwritten");
  }
  rex::system::AchievementManager manager;manager.SetUnlockSavePath(out/"fixture-unlocks.toml");
  std::vector<AchievementInfo> catalog;
  for(int i=0;i<18;++i) {AchievementInfo info;info.id=i+1;info.gamerscore=10+i*5;info.label=i%2?"Clean racing":"First victory";info.description="Keep pushing. Complete the challenge and make your mark on the circuit.";info.unachieved_description="Complete the race challenge to unlock this achievement.";catalog.push_back(info);}
  catalog[2].label="A very long achievement title that wraps clearly without touching the score or spilling outside the card";
  catalog[3].label="Şampiyonluğa doğru: güçlü bir başlangıç";
  manager.RegisterAchievements(catalog);manager.UnlockAchievement(1);manager.UnlockAchievement(3);
  const auto before=Progress(manager);Check(before.earned==2 && before.total==18 && before.score==30,"Actual manager progress and gamerscore");
  MenuState state;state.filter=1;Check(Matches(catalog[0],true,state)&&!Matches(catalog[1],false,state),"Unlocked filter");
  state.filter=2;Check(!Matches(catalog[0],true,state)&&Matches(catalog[1],false,state),"Locked filter");
  state.filter=0;std::memcpy(state.search,"VICTORY",8);Check(Matches(catalog[0],true,state)&&!Matches(catalog[1],false,state),"Case-insensitive search");
  {AchievementInfo info;info.label="ÇEVİK ŞAMPİYON";MenuState query;std::strcpy(query.search,"çevik");Check(Matches(info,false,query),"Turkish uppercase achievement titles match lowercase UTF-8 searches");
   info.label="ÖDÜL VE GÜÇ";std::strcpy(query.search,"ödül");Check(Matches(info,false,query),"Turkish umlauts retain case-insensitive search");
   info.label="Final";info.unachieved_description="DOĞRU YOL";std::strcpy(query.search,"doğru");Check(Matches(info,false,query),"Turkish case folding also covers locked descriptions");
   info.label="IŞIK";info.unachieved_description.clear();std::strcpy(query.search,"ışık");Check(Matches(info,false,query),"Turkish dotted and dotless I are searchable regardless of keyboard layout");
   info.label="FIRST";std::strcpy(query.search,"first");Check(Matches(info,false,query),"English I case folding remains unchanged when UI and game languages differ");
   info.label="ÉTOILE";std::strcpy(query.search,"étoile");Check(Matches(info,false,query),"French accented uppercase titles are also searchable");
   info.label="CAMPEÓN";std::strcpy(query.search,"campeón");Check(Matches(info,false,query),"Spanish accented uppercase titles are also searchable");}
  ToastQueue queue;AchievementEvent first{catalog[0],100},second{catalog[1],200};
  std::vector<std::thread> producers;for(int i=0;i<8;++i) producers.emplace_back([&]{for(int j=0;j<100;++j)queue.Push(first);});for(auto& thread:producers) thread.join();
  Check(!queue.Next(0,false),"Background notifications wait without sound");auto current=queue.Next(10,true);Check(current && current->play_sound && current->event.achievement.id==1,"First visible notification sounds exactly once");
  current=queue.Next(10.2,true);Check(current && !current->play_sound && current->alpha>0,"Next frame does not repeat sound");queue.Push(first);queue.Push(second);
  current=queue.Next(14.6,true);Check(current && current->play_sound && current->event.achievement.id==2,"Duplicates suppressed and queue advances");
  Check(!queue.Next(19.2,true),"Toasts expire without stale duplicates");
  ToastQueue bounded;for(int i=0;i<1000;++i) {auto e=first;e.achievement.id=i;bounded.Push(e);}for(int i=0;i<64;++i) Check(bounded.Next(i*5,true).has_value(),"Bounded queue retains first 64 events");Check(!bounded.Next(320,true),"Burst queue is bounded");
  preferences.sound=true;preferences.volume=25;rex::cvar::SetFlagByName("audio_master_volume","100");rex::cvar::SetFlagByName("audio_mute","false");Check(std::abs(CueGain(preferences)-.25f)<.001f,"Soft default cue gain");
  auto samples=CueSamples(CueGain(preferences));int peak=0;for(auto v:samples) peak=std::max(peak,std::abs(int(v)));Check(samples.size()==21600 && samples.front()==0 && std::abs(int(samples.back()))<2 && peak<2100,"450ms cue has soft level and no edge clicks");
  std::ofstream wav(out/"achievement-unlock-preview.wav",std::ios::binary);const int size=static_cast<int>(samples.size()*2),riff=size+36;const int fmt_size=16,rate=48000,byte_rate=96000;const int16_t pcm=1,channels=1,align=2,bits=16;
  wav.write("RIFF",4);wav.write(reinterpret_cast<const char*>(&riff),4);wav.write("WAVEfmt ",8);wav.write(reinterpret_cast<const char*>(&fmt_size),4);wav.write(reinterpret_cast<const char*>(&pcm),2);wav.write(reinterpret_cast<const char*>(&channels),2);wav.write(reinterpret_cast<const char*>(&rate),4);wav.write(reinterpret_cast<const char*>(&byte_rate),4);wav.write(reinterpret_cast<const char*>(&align),2);wav.write(reinterpret_cast<const char*>(&bits),2);wav.write("data",4);wav.write(reinterpret_cast<const char*>(&size),4);wav.write(reinterpret_cast<const char*>(samples.data()),size);wav.close();
  rex::cvar::SetFlagByName("audio_master_volume","40");Check(std::abs(CueGain(preferences)-.10f)<.001f,"Cue follows game master volume");
  rex::cvar::SetFlagByName("audio_mute","true");Check(CueGain(preferences)==0,"Game mute silences cue");rex::cvar::SetFlagByName("audio_mute","false");preferences.sound=false;Check(CueGain(preferences)==0,"User can disable cue");preferences.sound=true;
  {Sound sound(std::make_shared<Preferences>());sound.Play();}Check(true,"Windows sound worker shuts down with playback pending");
  auto* context=ImGui::CreateContext();auto& io=ImGui::GetIO();io.IniFilename=nullptr;io.LogFilename=nullptr;
  static const ImWchar ranges[]={0x20,0x1ff,0};auto* font=io.Fonts->AddFontFromFileTTF(argv[1],17,nullptr,ranges);auto* heading=io.Fonts->AddFontFromFileTTF(argv[1],30,nullptr,ranges);Check(font!=nullptr && heading!=nullptr,"Packaged Open Sans fonts load");
  unsigned char* pixels;int tw,th;io.Fonts->GetTexDataAsRGBA32(&pixels,&tw,&th);io.Fonts->SetTexID(1);io.DeltaTime=1.f/60;
  preferences.path.clear();
  for(bool tr:{false,true})for(ImVec2 size:{ImVec2(1280,720),ImVec2(1920,1080),ImVec2(3840,2160),ImVec2(2560,1080),ImVec2(640,480)})for(int filter=0;filter<3;++filter) {
    preferences.turkish=tr;state={};state.filter=filter;io.DisplaySize=size;
    for(int frame=0;frame<2;++frame) {ImGui::NewFrame();Check(!DrawMenu(manager,state,preferences,font,nullptr,heading),"Render does not request spontaneous close");DrawToast({catalog[2],100},1,tr,font);ImGui::Render();}
    auto* window=ImGui::FindWindowByName("##motogp_achievements");auto* toast=ImGui::FindWindowByName("##motogp_achievement_toast");
    Check(window && window->Pos.x>=0 && window->Pos.y>=0 && window->Pos.x+window->Size.x<=size.x+1 && window->Pos.y+window->Size.y<=size.y+1,"Responsive menu remains within viewport");
    Check(toast && toast->Pos.x>=0 && toast->Pos.y>=0 && toast->Pos.x+toast->Size.x<=size.x && toast->Pos.y+toast->Size.y<=size.y,"Responsive notification remains within viewport");
    Check(toast->ContentSize.y<=toast->InnerRect.GetHeight()+1,"Long titles and gamerscore fit inside toast");
    Check(context->ColorStack.Size==0 && context->StyleVarStack.Size==0 && context->FontStack.Size==0,"No style or font stack leaks");
    if(size.x==1280 && filter==0) Dump(out/(tr?"achievements-turkish":"achievements-english"));
  }
  rex::system::AchievementManager empty;state={};io.DisplaySize=ImVec2(1280,720);ImGui::NewFrame();DrawMenu(empty,state,preferences,font);ImGui::Render();Check(ImGui::GetDrawData()->TotalVtxCount>0,"Empty catalog displays usable UI");
  Check(Progress(manager).earned==before.earned && Progress(manager).score==before.score,"Viewing UI leaves achievement progress untouched");
  rex::system::AchievementManager restored;restored.SetUnlockSavePath(out/"fixture-unlocks.toml");restored.RegisterAchievements(catalog);restored.LoadUnlockState();Check(Progress(restored).earned==2,"Existing unlock persistence still loads");
  ImGui::DestroyContext(context);
  {
    TestContext ctx;auto window=rex::ui::Window::Create(ctx,"Achievement lifecycle probe");
    rex::ui::ImGuiDrawer drawer(window.get(),100);
    auto& dialog_io=drawer.GetIO();dialog_io.DisplaySize=ImVec2(1280,720);dialog_io.DeltaTime=1.f/60;
    dialog_io.Fonts->GetTexDataAsRGBA32(&pixels,&tw,&th);dialog_io.Fonts->SetTexID(1);
    auto prefs=std::make_shared<Preferences>();std::unique_ptr<rex::ui::ImGuiDialog> menu;
    rex::ui::RegisterBind("bind_achievements","F7","Achievement probe",[&] {
      if(menu) menu.reset(); else menu=CreateMenu(&drawer,nullptr,nullptr,manager,nullptr,nullptr,prefs,[&](std::weak_ptr<void> token){RequestClose(ctx,window.get(),token);});
    });
    auto toggle=[&]{rex::ui::KeyEvent key(window.get(),rex::ui::ParseVirtualKey(rex::cvar::Query<std::string>("bind_achievements")),1,false,false,false,false,false);Check(rex::ui::ProcessKeyEvent(key),"Actual SDK achievement key dispatch");};
    auto frame=[&]{drawer.GetIO();ImGui::NewFrame();if(menu)menu->Draw();ImGui::Render();};
    toggle();Check(menu && drawer.HasDialogs(),"F7 creates and attaches custom menu");frame();frame();
    dialog_io.AddKeyEvent(ImGuiKey_Escape,true);frame();Check(menu!=nullptr,"Esc close remains alive until draw ends");
    ctx.ExecutePendingFunctionsFromUIThread();Check(!menu && !drawer.HasDialogs(),"Deferred Esc safely resets SDK-owned dialog");
    dialog_io.AddKeyEvent(ImGuiKey_Escape,false);frame();
    toggle();frame();frame();dialog_io.AddKeyEvent(ImGuiKey_Escape,true);frame();toggle();
    ctx.ExecutePendingFunctionsFromUIThread();Check(!menu,"Expired deferred close cannot reopen manually closed menu");
    rex::cvar::SetFlagByName("bind_achievements","F8");toggle();Check(menu!=nullptr,"Remapped overlay binding opens normally");toggle();Check(!menu,"Remapped binding closes normally");
    rex::ui::UnregisterBind("bind_achievements");
  }
  for(const char* mode:{"offline","lan","online"}) {
    rex::cvar::SetFlagByName("network_mode",mode);
    TestContext ctx;auto window=rex::ui::Window::Create(ctx,"NOBK achievements integration probe");
    rex::input::InputSystem input(window.get());
    auto gate=std::make_shared<motogp::settings::InputGate>();gate->Initialize(&input);gate->SetFocused(true);
    motogp::settings::SetGuestGate(nullptr,nullptr,gate.get());
    rex::ui::ImGuiDrawer drawer(window.get(),100);auto& panel_io=drawer.GetIO();
    panel_io.IniFilename=nullptr;panel_io.LogFilename=nullptr;panel_io.DeltaTime=1.f/60;panel_io.DisplaySize=ImVec2(1280,720);
    auto* panel_font=panel_io.Fonts->AddFontFromFileTTF(argv[1],17,nullptr,ranges);
    auto* panel_heading=panel_io.Fonts->AddFontFromFileTTF(argv[1],30,nullptr,ranges);
    const auto input_font=std::filesystem::path(argv[1]);
    auto regular=input_font.parent_path()/"OpenSans-Regular.ttf",bold=input_font.parent_path()/"OpenSans-Bold.ttf";
    if(!std::filesystem::is_regular_file(regular))regular=input_font.parent_path().parent_path()/"launcher-fonts/OpenSans-Regular.ttf";
    if(!std::filesystem::is_regular_file(bold))bold=input_font.parent_path().parent_path()/"launcher-fonts/OpenSans-Bold.ttf";
    motogp::nobk::ConfigureFonts(panel_io.Fonts,regular.string().c_str(),argv[1],bold.string().c_str());
    panel_io.Fonts->GetTexDataAsRGBA32(&pixels,&tw,&th);panel_io.Fonts->SetTexID(1);
    auto prefs=std::make_shared<Preferences>();prefs->sound=false;
    motogp::nobk::Initialize(nullptr,gate);
    CaptureDrawer texture_drawer;
    auto menu=motogp::nobk::CreateMenu(&drawer,window.get(),&manager,panel_font,panel_heading,false,&texture_drawer,nullptr,prefs);
    auto* listener=dynamic_cast<rex::ui::WindowInputListener*>(menu.get());Check(listener!=nullptr,"Panel installs input listener");
    auto frame=[&]{gate->PollGuest();drawer.GetIO();ImGui::NewFrame();menu->Draw();ImGui::Render();
      Check(ImGui::GetCurrentContext()->ErrorCountCurrentFrame==0,"F1 layout submits no ImGui errors");};
    auto toggle=[&]{rex::ui::KeyEvent key(window.get(),rex::ui::VirtualKey::kF1,1,false,false,false,false,false);listener->OnKeyDown(key);};
    toggle();frame();frame();
    const bool online=std::string_view(mode)=="online";
    Check(gate->IsOpen()==online,"F1 opens only in Online mode");
    if(online) {
#ifdef MOTOGP_NOBK_VISUAL_PROBE
      motogp::nobk::SetVisualFixture(true);
      for(const auto style:{motogp::prompts::Style::Keyboard,motogp::prompts::Style::Xbox,motogp::prompts::Style::PlayStation}) {
        motogp::prompts::probe_style=style;
        for(int i=0;i<40;++i)frame();
        const auto hints=motogp::nobk::visual::Hints(style);
        Check(std::string_view(hints.previous)==(style==motogp::prompts::Style::Keyboard?"Q":style==motogp::prompts::Style::Xbox?"LB":"L1"),"F1 previous-tab prompt matches active device");
        Check(std::string_view(hints.next)==(style==motogp::prompts::Style::Keyboard?"E":style==motogp::prompts::Style::Xbox?"RB":"R1"),"F1 next-tab prompt matches active device");
        Dump(out/(style==motogp::prompts::Style::Keyboard?"nobk-prompts-keyboard":style==motogp::prompts::Style::Xbox?"nobk-prompts-xbox":"nobk-prompts-playstation"));
      }
      motogp::prompts::probe_style=motogp::prompts::Style::Keyboard;
      for(const char* page:{"friends","server","records","achievements"}) {
        for(int i=0;i<40;++i)frame();
        Dump(out/(std::string("nobk-redesign-")+page));
        if(std::string_view(page)=="records") {
          ImGuiWindow* body=nullptr;
          for(auto* candidate:ImGui::GetCurrentContext()->Windows)
            if(candidate->Active&&std::string_view(candidate->Name).find("##record_list")!=std::string_view::npos)body=candidate;
          Check(body&&body->ScrollMax.y>0,"Redesigned records remain scrollable");
          panel_io.AddMousePosEvent(body->InnerRect.Min.x+100,body->InnerRect.Min.y+100);
          panel_io.AddMouseWheelEvent(0,-3);frame();frame();const float scroll=body->Scroll.y;
          panel_io.AddMousePosEvent(body->InnerRect.Min.x+120,body->InnerRect.Min.y+120);frame();frame();
          Check(scroll>0&&std::abs(body->Scroll.y-scroll)<1,"Mouse movement does not undo manual records scrolling");
        }
        panel_io.AddKeyEvent(ImGuiKey_E,true);frame();panel_io.AddKeyEvent(ImGuiKey_E,false);frame();
      }
      motogp::nobk::SetVisualFixture(true,true);
      panel_io.DisplaySize=ImVec2(1672,941);motogp::prompts::probe_style=motogp::prompts::Style::PlayStation;
      panel_io.AddMousePosEvent(1600,900);
      for(int i=0;i<30;++i)frame();
      for(int i=0;i<2;++i){panel_io.AddKeyEvent(ImGuiKey_DownArrow,true);frame();panel_io.AddKeyEvent(ImGuiKey_DownArrow,false);frame();}
      for(int i=0;i<30;++i)frame();
      Dump(out/"apex-reference");
      const auto colors=[] {
        uint64_t hash=1469598103934665603ull;
        for(const auto* batch:ImGui::GetDrawData()->CmdLists)for(const auto& vertex:batch->VtxBuffer)hash=(hash^vertex.col)*1099511628211ull;
        return hash;
      };
      const auto flag_positions=[] {
        std::vector<ImVec2> positions;
        for(const auto* batch:ImGui::GetDrawData()->CmdLists)for(const auto& command:batch->CmdBuffer)if(command.GetTexID()>1)
          for(unsigned i=0;i<command.ElemCount;++i)positions.push_back(batch->VtxBuffer[batch->IdxBuffer[command.IdxOffset+i]+command.VtxOffset].pos);
        return positions;
      };
      const auto original_colors=colors();const auto original_flags=flag_positions();
      for(int i=0;i<180;++i)frame();
      Check(colors()!=original_colors,"Apex highlights animate with elapsed time");
      const auto moved_flags=flag_positions();
      Check(original_flags.size()==moved_flags.size()&&std::equal(original_flags.begin(),original_flags.end(),moved_flags.begin(),
          [](ImVec2 a,ImVec2 b){return a.x==b.x&&a.y==b.y;}),"Animated decorative lines do not shift friend columns or flags");
      Dump(out/"apex-lines-motion");
      panel_io.DisplaySize=ImVec2(1280,720);motogp::prompts::probe_style=motogp::prompts::Style::Keyboard;
      motogp::nobk::SetVisualFixture(false);
#endif
      const auto old_flags=panel_io.ConfigFlags;const auto old_backend=panel_io.BackendFlags;
      panel_io.AddKeyEvent(ImGuiKey_Q,true);frame();panel_io.AddKeyEvent(ImGuiKey_Q,false);frame();frame();
      auto* context=ImGui::GetCurrentContext();ImGuiWindow* list=nullptr;
      for(auto* candidate:context->Windows)if(candidate->Active&&std::string_view(candidate->Name).find("achievement_list")!=std::string_view::npos&&std::string_view(candidate->Name).find("/##row_")==std::string_view::npos)list=candidate;
      Check(list&&list->ScrollMax.y>0,"F1 renders the full scrollable local catalog before online data loads");
      Check(panel_io.ConfigFlags&ImGuiConfigFlags_NavEnableGamepad,"F1 achievement controls enable controller navigation");
      const float before_scroll=list->Scroll.y;
      panel_io.AddMousePosEvent(list->Pos.x+80,list->Pos.y+50);panel_io.AddMouseWheelEvent(0,-3);frame();frame();
      Check(list->Scroll.y>before_scroll,"Mouse wheel scrolls the embedded achievement list");
      // Focus the actual search field by mouse, then type shortcut letters.
      ImGuiWindow* body=list->ParentWindow;
      // The approved search field shares the filter row and aligns right.
      panel_io.AddMousePosEvent(list->InnerRect.Max.x-80,list->Pos.y-12);panel_io.AddMouseButtonEvent(0,true);frame();
      panel_io.AddMouseButtonEvent(0,false);frame();
      Check(panel_io.WantTextInput,"Panel achievement search accepts text input");
      for(auto key:{ImGuiKey_Q,ImGuiKey_E,ImGuiKey_W}){panel_io.AddKeyEvent(key,true);panel_io.AddInputCharacter(key==ImGuiKey_Q?'q':key==ImGuiKey_E?'e':'w');frame();panel_io.AddKeyEvent(key,false);frame();}
      Check(list->Active&&gate->IsOpen(),"Typing Q/E/W in achievement search cannot switch tabs or close the panel");
      panel_io.AddKeyEvent(ImGuiMod_Ctrl,true);panel_io.AddKeyEvent(ImGuiKey_A,true);frame();
      panel_io.AddKeyEvent(ImGuiKey_A,false);panel_io.AddKeyEvent(ImGuiMod_Ctrl,false);frame();
      panel_io.AddKeyEvent(ImGuiKey_Backspace,true);frame();panel_io.AddKeyEvent(ImGuiKey_Backspace,false);frame();frame();
      Check(list->ScrollMax.y>0,"Clearing the search restores the complete achievement list");
      ImGui::ClearActiveID();
      const float scale=motogp::nobk::visual::Measure(panel_io.DisplaySize).scale;
      const auto click=[&](float x,float y) {
        panel_io.AddMousePosEvent(x,y);panel_io.AddMouseButtonEvent(0,true);frame();
        panel_io.AddMouseButtonEvent(0,false);frame();
        // Button actions happen on release; ImGui updates ScrollMax from the
        // new content extent at the next BeginChild, one frame later.
        frame();
      };
      const float all_scroll=list->ScrollMax.y;
      const float filters_y=body->InnerRect.Min.y+181*scale;
      click(body->InnerRect.Min.x+239*scale,filters_y);
      Check(list->ScrollMax.y<all_scroll,"F1 Unlocked filter displays the smaller earned catalog");
      click(body->InnerRect.Min.x+79*scale,filters_y);
      Check(list->ScrollMax.y>0,"F1 All filter restores the complete scrollable catalog");
      const float controls_y=list->Pos.y+list->Size.y+35*scale;
      const float popup_x=body->InnerRect.Min.x+48*scale;
      const bool original_popup=prefs->popup.load();
      click(popup_x,controls_y);
      Check(prefs->popup.load()!=original_popup,"Restyled pop-up checkbox updates shared preferences");
      click(popup_x,controls_y);
      Check(prefs->popup.load()==original_popup,"Pop-up preference can be restored through the same control");
      const int original_volume=prefs->volume.load();const float volume_x=body->InnerRect.Max.x-90*scale;
      click(volume_x,controls_y);
      Check(prefs->volume.load()>original_volume+10,"Thin volume slider retains mouse value changes");
      panel_io.AddKeyEvent(ImGuiMod_Ctrl,true);frame();click(volume_x,controls_y);
      Check(panel_io.WantTextInput,"Volume slider retains native Ctrl-click numeric input");
      panel_io.AddKeyEvent(ImGuiKey_A,true);frame();panel_io.AddKeyEvent(ImGuiKey_A,false);panel_io.AddKeyEvent(ImGuiMod_Ctrl,false);frame();
      const auto volume_text=std::to_string(original_volume);panel_io.AddInputCharactersUTF8(volume_text.c_str());frame();
      panel_io.AddKeyEvent(ImGuiKey_Enter,true);frame();panel_io.AddKeyEvent(ImGuiKey_Enter,false);frame();
      Check(prefs->volume.load()==original_volume,"Numeric volume edit restores the original preference");
      ImGui::ClearActiveID();
      for(bool tr:{false,true})for(ImVec2 size:{ImVec2(1280,720),ImVec2(1920,1080),ImVec2(640,480)}) {
        prefs->turkish=tr;panel_io.DisplaySize=size;frame();frame();
        auto* host=ImGui::FindWindowByName("##nobk_panel");
        Check(host&&host->Pos.x>=0&&host->Pos.y>=0&&host->Pos.x+host->Size.x<=size.x+1&&host->Pos.y+host->Size.y<=size.y+1,"Embedded achievement host stays inside the viewport");
        Check(context->ColorStack.Size==0&&context->StyleVarStack.Size==0&&context->FontStack.Size==0,"Embedded achievement styles remain balanced");
        if(size.x==1280)Dump(out/(tr?"nobk-achievements-turkish":"nobk-achievements-english"));
      }
      panel_io.AddKeyEvent(ImGuiKey_Escape,true);frame();panel_io.AddKeyEvent(ImGuiKey_Escape,false);frame();
      Check(!gate->IsOpen()&&!gate->Blocking(),"Esc closes F1 and releases guest input after neutral polling");
      Check(panel_io.ConfigFlags==old_flags&&panel_io.BackendFlags==old_backend,"Closing F1 restores previous navigation flags");
      Check(Progress(manager).earned==before.earned&&Progress(manager).score==before.score,"Embedded catalog never changes unlock progress");
    }
    menu.reset();motogp::nobk::Shutdown();gate->PollGuest();motogp::settings::SetGuestGate(nullptr);
  }
  std::cout<<checks<<" achievement UI, storage, notification and sound checks passed\n";return 0;
} catch(const std::exception& e) {std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
