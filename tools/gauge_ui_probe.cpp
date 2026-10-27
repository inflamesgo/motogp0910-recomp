// Actual production asset loader, bitmap fonts and ImGui meshes. The capture
// source is a fixture; this does not execute a race or a native GPU driver.
#include "../src/gauge_hud.h"
#include "../src/gauge_capture.h"
#include <rex/ui/imgui_dialog.h>
#include <rex/ui/imgui_drawer.h>
#include <rex/ui/immediate_drawer.h>
#include <rex/ui/window.h>
#include <rex/ui/windowed_app_context.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>
#include <bit>
#include <limits>

namespace motogp::gauge {
TelemetryFrame fixture;
bool fixture_enabled=false;
void SetCaptureEnabled(bool enabled){fixture_enabled=enabled;}
void PublishFrame(){}
TelemetryFrame ReadFrame(){return fixture;}
void ResetCapture(){fixture={};fixture_enabled=false;}
}
namespace {
class Context final:public rex::ui::WindowedAppContext {
 protected:void NotifyUILoopOfPendingFunctions() override {} void PlatformQuitFromUIThread() override {}
};
class Texture final:public rex::ui::ImmediateTexture {
 public:Texture(uint32_t w,uint32_t h,const uint8_t* data):ImmediateTexture(w,h),pixels(data,data+size_t(w)*h*4){}
  std::vector<uint8_t> pixels;
};
class Drawer final:public rex::ui::ImmediateDrawer {
 public:
  std::unique_ptr<rex::ui::ImmediateTexture> CreateTexture(uint32_t w,uint32_t h,rex::ui::ImmediateTextureFilter,bool,const uint8_t* data) override {return std::make_unique<Texture>(w,h,data);}
  void BeginDrawBatch(const rex::ui::ImmediateDrawBatch&) override {}
  void Draw(const rex::ui::ImmediateDraw&) override {}
  void EndDrawBatch() override {}
};
void Check(bool value,const char* label){if(!value)throw std::runtime_error(label);}
uint64_t GeometryHash(){
  uint64_t hash=14695981039346656037ull;const auto* data=ImGui::GetDrawData();
  for(int l=0;l<data->CmdListsCount;++l)for(const auto& v:data->CmdLists[l]->VtxBuffer)
    for(const auto value:{std::bit_cast<uint32_t>(v.pos.x),std::bit_cast<uint32_t>(v.pos.y),v.col}){hash^=value;hash*=1099511628211ull;}
  return hash;
}
void CheckPlacement(const motogp::gauge::Telemetry& sample){
  const auto* data=ImGui::GetDrawData();
  const auto viewport=data->DisplaySize;
  const float left=sample.view_x*viewport.x,top=sample.view_y*viewport.y;
  const float width=sample.view_width*viewport.x,height=sample.view_height*viewport.y;
  // Clean Section's original lower edge around y=850 is lifted by 160 px.
  // Check emitted gauge geometry clears it in each local viewport.
  for(int l=0;l<data->CmdListsCount;++l)for(const auto& vertex:data->CmdLists[l]->VtxBuffer){
    Check(vertex.pos.x>left+width*.75f&&vertex.pos.x<left+width,"Gauge stays in the viewport's lower-right free area");
    Check(vertex.pos.y>top+height*(690.f/1080.f)&&vertex.pos.y<top+height,"Gauge clears the raised Clean Section overlay and bottom edge");
  }
}
void Dump(const std::filesystem::path& stem){
  unsigned char* pixels=nullptr;int width=0,height=0;ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&pixels,&width,&height);
  std::ofstream image(stem.string()+".rgba",std::ios::binary);image.write(reinterpret_cast<char*>(pixels),size_t(width)*height*4);
  const auto* data=ImGui::GetDrawData();std::ofstream file(stem.string()+".json");
  file<<"{\"size\":["<<data->DisplaySize.x<<','<<data->DisplaySize.y<<"],\"texture\":["<<width<<','<<height<<"],\"lists\":[";
  std::set<ImTextureID> textures;
  for(int l=0;l<data->CmdListsCount;++l){
    if(l)file<<',';const auto* list=data->CmdLists[l];file<<"{\"v\":[";
    for(int i=0;i<list->VtxBuffer.Size;++i){if(i)file<<',';const auto& v=list->VtxBuffer[i];file<<'['<<v.pos.x<<','<<v.pos.y<<','<<v.uv.x<<','<<v.uv.y<<','<<v.col<<']';}
    file<<"],\"i\":[";for(int i=0;i<list->IdxBuffer.Size;++i){if(i)file<<',';file<<list->IdxBuffer[i];}
    file<<"],\"cmd\":[";
    for(int i=0;i<list->CmdBuffer.Size;++i){if(i)file<<',';const auto& c=list->CmdBuffer[i];const auto texture=c.GetTexID();if(texture>1)textures.insert(texture);
      file<<'['<<c.ClipRect.x<<','<<c.ClipRect.y<<','<<c.ClipRect.z<<','<<c.ClipRect.w<<','<<c.IdxOffset<<','<<c.ElemCount<<','<<c.VtxOffset<<','<<texture<<']';}
    file<<"]}";
  }
  file<<"],\"images\":[";bool first=true;
  for(const auto id:textures){
    const auto* texture=reinterpret_cast<const Texture*>(id);const auto name=stem.filename().string()+"-"+std::to_string(id)+".rgba";
    std::ofstream bytes(stem.parent_path()/name,std::ios::binary);bytes.write(reinterpret_cast<const char*>(texture->pixels.data()),texture->pixels.size());
    if(!first)file<<',';first=false;file<<'['<<id<<','<<texture->width<<','<<texture->height<<",\""<<name<<"\"]";
  }
  file<<"]}";
}
}
int main(int argc,char** argv){try{
  Check(argc==3,"Usage: gauge_ui_probe ASSET_PACK NEW_FIXTURE_DIRECTORY");
  const std::filesystem::path assets=argv[1],output=argv[2];
  Check(!std::filesystem::exists(output),"Use a new isolated fixture directory");std::filesystem::create_directories(output);
  Context context;auto window=rex::ui::Window::Create(context,"Gauge renderer fixture");
  rex::ui::ImGuiDrawer imgui(window.get(),100);Drawer drawer;
  auto& io=imgui.GetIO();io.IniFilename=nullptr;io.LogFilename=nullptr;io.DeltaTime=1.f/60;
  io.DisplaySize={1920,1080};unsigned char* pixels;int width,height;io.Fonts->GetTexDataAsRGBA32(&pixels,&width,&height);io.Fonts->SetTexID(1);
  bool blocked=false;
  auto overlay=motogp::gauge::CreateOverlay(&imgui,&drawer,assets,output,[&]{return blocked;});
  Check(motogp::gauge::Available(),"Original images/fonts/geometry load successfully");
  auto frame=[&]{imgui.GetIO();ImGui::NewFrame();overlay->Draw();ImGui::Render();Check(ImGui::GetCurrentContext()->ErrorCountCurrentFrame==0,"No ImGui renderer errors");};
  motogp::gauge::SelectStyle(0);frame();Check(!motogp::gauge::fixture_enabled,"Original style leaves capture disabled");
  motogp::gauge::SelectStyle(1);frame();Check(motogp::gauge::fixture_enabled,"MotoGP17 enables capture only with usable assets");
  for(int index=0;index<5;++index){
    const int gears[]{0,1,4,6,6};const float rpms[]{0,6000,13500,18000,22000};
    motogp::gauge::fixture[0]=motogp::gauge::Telemetry{107.52f,rpms[index],3000,18000,gears[index],true,0,0,1,1,0};
    frame();Check(ImGui::GetDrawData()->TotalVtxCount>0,"Gauge produces textured geometry");CheckPlacement(*motogp::gauge::fixture[0]);Dump(output/("gauge-"+std::to_string(index)));
  }
  auto& rider=*motogp::gauge::fixture[0];rider.rpm=9000;rider.gear=3;rider.inputs_valid=true;
  auto inputs=[&](float gas,float brake,const char* name){
    rider.throttle=gas;rider.front_brake=brake;frame();CheckPlacement(rider);Dump(output/name);return GeometryHash();
  };
  const auto released=inputs(0,0,"inputs-released");
  const auto half_gas=inputs(.5f,0,"inputs-half-gas"),full_gas=inputs(1,0,"inputs-full-gas");
  Check(released!=half_gas&&half_gas!=full_gas,"Original throttle animation visibly responds at released, half and full pressure");
  const auto half_brake=inputs(0,.5f,"inputs-half-brake"),full_brake=inputs(0,1,"inputs-full-brake");
  Check(released!=half_brake&&half_brake!=full_brake&&half_brake!=half_gas,"Original front-brake animation is distinct and responds proportionally");
  const auto both=inputs(1,1,"inputs-both");Check(both!=full_gas&&both!=full_brake,"Both controls can display pressure simultaneously");
  rider.inputs_valid=false;Check(inputs(1,1,"inputs-unavailable")==released,"Unavailable inputs leave the original outline empty");
  rider.inputs_valid=true;Check(inputs(std::numeric_limits<float>::quiet_NaN(),-1,"inputs-invalid")==released,"Invalid input values do not fabricate pressure");
  Check(inputs(2,0,"inputs-clamped")==full_gas,"Over-range input remains within the authored full sweep");
  Check(motogp::gauge::TyrePanelAvailable(),"Original MotoGP17 tyre meshes and bitmap load as an optional complete panel");
  inputs(0,0,"tyres-native-fallback");
  const auto no_tyres=GeometryHash();
  rider.tyres_valid=true;rider.tyre_hud_visible=true;rider.tyres_colored=true;
  rider.front_tyre=1;rider.rear_tyre=1;rider.tyre_replaced=false;
  frame();Check(GeometryHash()==no_tyres,"A valid sample alone cannot duplicate the native tyre HUD");
  rider.tyre_replaced=true;frame();const auto full_tyres=GeometryHash();Dump(output/"tyres-full");
  Check(full_tyres!=no_tyres,"The original donor motorcycle panel appears only after native replacement");
  rider.front_tyre=.5f;rider.rear_tyre=.25f;frame();Dump(output/"tyres-worn");
  Check(GeometryHash()!=full_tyres,"Verified front and rear health changes affect their own donor fills/colors");
  bool front_clip=false,rear_clip=false;
  const auto* tyre_draw=ImGui::GetDrawData();
  for(int l=0;l<tyre_draw->CmdListsCount;++l)for(const auto& command:tyre_draw->CmdLists[l]->CmdBuffer){
    const auto& c=command.ClipRect;const float h=c.w-c.y;
    if(c.x>1460&&c.x<1463&&h>9&&h<10)front_clip=true;
    if(c.x>1499&&c.x<1502&&h>4&&h<5)rear_clip=true;
  }
  Check(front_clip&&rear_clip,"Front half fill and rear quarter fill use distinct original vector bounds");
  rider.tyres_colored=false;frame();Dump(output/"tyres-neutral");
  Check(GeometryHash()!=full_tyres,"Native wear-disabled mode retains neutral white donor silhouettes");
  rider.tyre_hud_visible=false;frame();Check(GeometryHash()==no_tyres,"Native tyre HUD visibility also gates the donor panel");
  rider.tyre_hud_visible=true;rider.front_tyre=std::numeric_limits<float>::quiet_NaN();frame();
  Check(GeometryHash()==no_tyres,"Invalid tyre values cannot publish an invented donor fill");
  rider.tyres_valid=false;rider.tyre_replaced=false;rider.tyre_hud_visible=false;
  motogp::gauge::fixture[1]=motogp::gauge::Telemetry{83,8500,3000,14000,3,false,0,.5f,1,.5f,1};
  motogp::gauge::fixture[0]->view_height=.5f;frame();Dump(output/"split");
  // Check each split-screen viewport separately against its own side HUD.
  const auto first_view=motogp::gauge::fixture[0];motogp::gauge::fixture[0].reset();frame();CheckPlacement(*motogp::gauge::fixture[1]);
  motogp::gauge::fixture[0]=first_view;motogp::gauge::fixture[1].reset();frame();CheckPlacement(*motogp::gauge::fixture[0]);
  io.DisplaySize={2560,1080};motogp::gauge::fixture[0]->view_height=1;frame();CheckPlacement(*motogp::gauge::fixture[0]);Dump(output/"ultrawide");
  io.DisplaySize={1920,1080};
  for(int style=2;style<motogp::gauge::kStyleCount;++style){
    const auto pack=assets.parent_path()/motogp::gauge::StyleKey(style)/"gauge.json";
    if(!std::filesystem::is_regular_file(pack))continue;
    Check(motogp::gauge::StyleAvailable(style),"Bundled classic images, digits and calibration load successfully");
    motogp::gauge::SelectStyle(style);
    auto& classic=*motogp::gauge::fixture[0];classic.rpm=9000;classic.max_rpm=19000;classic.gear=3;
    frame();Check(motogp::gauge::fixture_enabled&&ImGui::GetDrawData()->TotalVtxCount>0,"Classic gauge replaces the native gauge only after loading");
    const auto original_limit=GeometryHash();Dump(output/(std::string(motogp::gauge::StyleKey(style))+"-9000"));
    classic.max_rpm=23000;frame();
    Check(style!=3?GeometryHash()==original_limit:GeometryHash()!=original_limit,
        "Numbered faces retain absolute RPM; unnumbered URT2 follows the bike limit");
    classic.max_rpm=19000;classic.rpm=19000;classic.mph=0;classic.gear=0;frame();
    Dump(output/(std::string(motogp::gauge::StyleKey(style))+"-maximum-neutral"));
    blocked=true;frame();Check(!motogp::gauge::fixture_enabled&&ImGui::GetDrawData()->TotalVtxCount==0,"Other overlays suppress classic gauges too");blocked=false;
  }
  motogp::gauge::SelectStyle(1);
  blocked=true;frame();Check(!motogp::gauge::fixture_enabled&&ImGui::GetDrawData()->TotalVtxCount==0,"Other overlays suppress the custom gauge");
  blocked=false;motogp::gauge::fixture={};frame();Check(ImGui::GetDrawData()->TotalVtxCount==0,"No native draw sample means no stale gauge");
  overlay.reset();Check(!motogp::gauge::Available(),"Shutdown clears readiness");
  overlay=motogp::gauge::CreateOverlay(&imgui,&drawer,assets,output,[&]{return blocked;});Check(motogp::gauge::SelectedStyle()==1,"Gauge preference survives reload");
  overlay.reset();overlay=motogp::gauge::CreateOverlay(&imgui,&drawer,output/"missing",output,[]{return false;});Check(!motogp::gauge::Available(),"Missing assets keep original HUD available");
  overlay.reset();std::cout<<"Gauge asset/render/persistence fixtures passed\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAILED: "<<e.what()<<'\n';return 1;}}
