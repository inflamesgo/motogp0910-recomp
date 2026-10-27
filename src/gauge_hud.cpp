#include "gauge_hud.h"
#include "gauge_capture.h"
#include "gauge_classic.h"
#include "../third_party/nlohmann/json.hpp"

#include <rex/logging.h>
#include <rex/ui/imgui_dialog.h>
#include <rex/ui/immediate_drawer.h>
#include <rex/ui/image_decode.h>
#include <imgui.h>
#include <toml++/toml.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cwctype>
#include <cstring>
#include <fstream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#endif

namespace motogp::gauge {
namespace {
using Json=nlohmann::json;
struct Vertex {ImVec2 position,uv;ImU32 color;};
struct Mesh {int texture=-1;std::vector<Vertex> vertices;std::vector<uint16_t> indices;};
using Layer=std::vector<Mesh>;
struct Text {
  std::string font,role;
  std::array<float,4> bounds{};
  std::array<float,6> matrix{};
  std::array<float,4> color{};
  float height=0;
  int alignment=0;
};
struct Glyph {int x=0,y=0,width=0,height=0,xoffset=0,yoffset=0,advance=0;};
struct Font {int texture=-1,width=0,height=0,size=0;std::map<char,Glyph> glyphs;};

std::vector<uint8_t> Bytes(const std::filesystem::path& path,size_t maximum) {
  const auto size=std::filesystem::file_size(path);
  if(!size||size>maximum)throw std::runtime_error("Gauge resource size is invalid");
  std::vector<uint8_t> bytes(static_cast<size_t>(size));
  std::ifstream file(path,std::ios::binary);
  if(!file.read(reinterpret_cast<char*>(bytes.data()),std::streamsize(bytes.size())))throw std::runtime_error("Cannot read gauge resource");
  return bytes;
}
float Number(const Json& value,float low=-8192,float high=8192) {
  const float number=value.get<float>();
  if(!std::isfinite(number)||number<low||number>high)throw std::runtime_error("Gauge geometry is out of range");
  return number;
}
template<size_t N> std::array<float,N> Numbers(const Json& values) {
  if(!values.is_array()||values.size()!=N)throw std::runtime_error("Gauge geometry has an invalid shape");
  std::array<float,N> result{};
  for(size_t i=0;i<N;++i)result[i]=Number(values[i]);
  return result;
}
ImU32 Color(const std::array<float,4>& color) {
  return IM_COL32(int(std::clamp(color[0],0.f,1.f)*255+.5f),int(std::clamp(color[1],0.f,1.f)*255+.5f),
      int(std::clamp(color[2],0.f,1.f)*255+.5f),int(std::clamp(color[3],0.f,1.f)*255+.5f));
}
ImU32 MultiplyColor(ImU32 a,ImU32 b) {
  if(b==IM_COL32_WHITE)return a;
  ImU32 result=0;
  for(unsigned shift=0;shift<32;shift+=8)
    result|=((((a>>shift)&255)*((b>>shift)&255)+127)/255)<<shift;
  return result;
}
ImU32 TyreColor(float health) {
  // The original 09/10 tyre indicator's bad/yellow/good color stops.
  health=std::clamp(health,0.f,1.f);
  const std::array<float,3> bad{218,53,26},yellow{218,218,26},good{134,218,26};
  const auto& a=health<=.5f?bad:yellow;const auto& b=health<=.5f?yellow:good;
  const float t=health<=.5f?health*2:(health-.5f)*2;
  return IM_COL32(int(a[0]+(b[0]-a[0])*t+.5f),int(a[1]+(b[1]-a[1])*t+.5f),int(a[2]+(b[2]-a[2])*t+.5f),255);
}
ImVec2 Transform(const std::array<float,6>& matrix,float x,float y) {
  return {matrix[0]*x+matrix[2]*y+matrix[4],matrix[1]*x+matrix[3]*y+matrix[5]};
}

class Assets {
 public:
  std::array<Layer,68> rpm;
  std::array<Layer,8> gears;
  std::array<Layer,17> throttle,brake;
  bool input_animation=false;
  Layer metric,imperial,base;
  Layer tyre_front,tyre_rear,tyre_background,tyre_neutral_body,tyre_temperature_covers;
  std::array<float,4> tyre_front_bounds{},tyre_rear_bounds{};
  bool tyre_panel=false;
  std::vector<Text> speed,gear_labels;
  std::map<std::string,Font> fonts;
  std::vector<std::unique_ptr<rex::ui::ImmediateTexture>> textures;

  Assets(rex::ui::ImmediateDrawer* drawer,const std::filesystem::path& root) {
    if(!drawer)throw std::runtime_error("Gauge texture drawer is unavailable");
    const auto bytes=Bytes(root/"gauge.json",2*1024*1024);
    const auto json=Json::parse(bytes.begin(),bytes.end());
    if(json.at("schema")!=1||json.at("canvas")!=Json::array({1920,1080})||
        json.at("anchor")!=Json::array({1519,727})||json.at("bounds")!=Json::array({280,280}))throw std::runtime_error("Unsupported gauge pack schema");
    const auto& images=json.at("textures");
    if(!images.is_array()||images.empty()||images.size()>32)throw std::runtime_error("Invalid gauge texture count");
    for(size_t i=0;i<images.size();++i){
      const auto filename=images[i].get<std::string>();
      // Imported packs contain numbered PNGs only; do not follow arbitrary
      // paths supplied by an edited metadata file.
      const auto expected="texture-"+std::string(i<10?"0":"")+std::to_string(i)+".png";
      if(filename!=expected)throw std::runtime_error("Invalid gauge texture filename");
      std::vector<uint8_t> image;
      if(json.contains("embedded_textures")){
        const auto hex=json.at("embedded_textures").at(filename).get<std::string>();
        if(hex.empty()||hex.size()%2||hex.size()>16*1024*1024)throw std::runtime_error("Invalid embedded gauge texture size");
        auto nibble=[](char c)->uint8_t{
          if(c>='0'&&c<='9')return uint8_t(c-'0');
          if(c>='a'&&c<='f')return uint8_t(c-'a'+10);
          throw std::runtime_error("Invalid embedded gauge texture encoding");
        };
        image.resize(hex.size()/2);
        for(size_t byte=0;byte<image.size();++byte)image[byte]=uint8_t((nibble(hex[byte*2])<<4)|nibble(hex[byte*2+1]));
      }else image=Bytes(root/filename,8*1024*1024);
      if(image.size()<24||std::memcmp(image.data(),"\x89PNG\r\n\x1a\n",8))throw std::runtime_error("Expected PNG gauge texture");
      auto dimension=[&](size_t offset){return uint32_t(image[offset])<<24|uint32_t(image[offset+1])<<16|uint32_t(image[offset+2])<<8|image[offset+3];};
      if(!dimension(16)||!dimension(20)||dimension(16)>2048||dimension(20)>2048)throw std::runtime_error("Gauge texture dimensions exceed limits");
      int width=0,height=0;
      const auto rgba=rex::ui::DecodeImageRGBA(image.data(),image.size(),width,height);
      if(width<=0||height<=0||width>2048||height>2048||rgba.size()!=size_t(width)*height*4)throw std::runtime_error("Cannot decode gauge texture");
      auto texture=drawer->CreateTexture(width,height,rex::ui::ImmediateTextureFilter::kLinear,false,rgba.data());
      if(!texture)throw std::runtime_error("Cannot create gauge GPU texture");
      textures.push_back(std::move(texture));
    }
    if(json.at("rpm_frames").size()!=rpm.size()||json.at("gear_frames").size()!=gears.size())throw std::runtime_error("Invalid gauge animation count");
    for(size_t i=0;i<rpm.size();++i)rpm[i]=ReadLayer(json["rpm_frames"][i]);
    for(size_t i=0;i<gears.size();++i)gears[i]=ReadLayer(json["gear_frames"][i]);
    if(json.contains("throttle_frames")||json.contains("brake_frames")){
      if(json.at("throttle_frames").size()!=throttle.size()||json.at("brake_frames").size()!=brake.size())
        throw std::runtime_error("Invalid gauge input animation count");
      for(size_t i=0;i<throttle.size();++i){
        throttle[i]=ReadLayer(json["throttle_frames"][i]);brake[i]=ReadLayer(json["brake_frames"][i]);
      }
      input_animation=true;
    }
    metric=ReadLayer(json.at("units_kmh"));imperial=ReadLayer(json.at("units_mph"));base=ReadLayer(json.at("static"));
    if(json.contains("tyre_status")){
      // Optional: an absent or damaged tyre extension retains the main gauge
      // and native tyre fallback instead of hiding an unrelated native UI.
      try{
        const auto& tyres=json.at("tyre_status");
        if(tyres.at("draw_order")!=Json::array({"front","rear","background"}))
          throw std::runtime_error("Invalid tyre layer order");
        tyre_front=ReadLayer(tyres.at("front"));tyre_rear=ReadLayer(tyres.at("rear"));
        tyre_background=ReadLayer(tyres.at("background"));
        if(tyres.contains("neutral_body"))tyre_neutral_body=ReadLayer(tyres.at("neutral_body"));
        if(tyres.contains("temperature_covers"))tyre_temperature_covers=ReadLayer(tyres.at("temperature_covers"));
        tyre_front_bounds=Numbers<4>(tyres.at("front_bounds"));tyre_rear_bounds=Numbers<4>(tyres.at("rear_bounds"));
        for(const auto& bounds:{tyre_front_bounds,tyre_rear_bounds})
          if(bounds[2]<=bounds[0]||bounds[3]<=bounds[1])throw std::runtime_error("Invalid tyre bounds");
        tyre_panel=!tyre_front.empty()&&!tyre_rear.empty()&&!tyre_background.empty();
      }catch(const std::exception&){
        tyre_front.clear();tyre_rear.clear();tyre_background.clear();
        tyre_neutral_body.clear();tyre_temperature_covers.clear();tyre_panel=false;
      }
    }
    speed=ReadText(json.at("speed"));gear_labels=ReadText(json.at("gear_labels"));
    for(const auto& [name,definition]:json.at("fonts").items()){
      Font font;
      font.texture=definition.at("texture").get<int>();font.width=definition.at("width").get<int>();
      font.height=definition.at("height").get<int>();font.size=definition.at("size").get<int>();
      if(font.texture<0||size_t(font.texture)>=textures.size()||font.width<=0||font.height<=0||font.size<1||font.size>200||
          textures[font.texture]->width!=uint32_t(font.width)||textures[font.texture]->height!=uint32_t(font.height))throw std::runtime_error("Invalid bitmap font");
      for(char c:std::string("0123456789N")){
        const auto& g=definition.at("glyphs").at(std::to_string(static_cast<unsigned char>(c)));
        Glyph glyph{g.at("x").get<int>(),g.at("y").get<int>(),g.at("width").get<int>(),g.at("height").get<int>(),
            g.at("xoffset").get<int>(),g.at("yoffset").get<int>(),g.at("xadvance").get<int>()};
        if(glyph.x<0||glyph.y<0||glyph.width<0||glyph.height<0||glyph.x+glyph.width>font.width||glyph.y+glyph.height>font.height||
            std::abs(glyph.xoffset)>200||std::abs(glyph.yoffset)>200||glyph.advance<0||glyph.advance>200)throw std::runtime_error("Invalid bitmap glyph");
        font.glyphs.emplace(c,glyph);
      }
      fonts.emplace(name,std::move(font));
    }
    for(const auto* runs:{&speed,&gear_labels})for(const auto& run:*runs)if(!fonts.contains(run.font))throw std::runtime_error("Missing gauge bitmap font");
  }

  void Draw(ImDrawList* draw,const Telemetry& sample,ImVec2 viewport) const {
    const ImVec2 minimum(sample.view_x*viewport.x,sample.view_y*viewport.y);
    const ImVec2 size(sample.view_width*viewport.x,sample.view_height*viewport.y);
    const float viewport_scale=std::min(size.x/1920.f,size.y/1080.f);
    // Restore the donor's 280 px face and original lower-right placement.
    // The native Clean Section overlay moves up separately to leave a gap.
    const float scale=viewport_scale;
    const ImVec2 origin(minimum.x+size.x-401*scale,minimum.y+size.y-353*scale);
    draw->PushClipRect(minimum,ImVec2(minimum.x+size.x,minimum.y+size.y),true);
    // The donor face has no numbered RPM scale. Its complete authored sweep
    // represents zero through this bike's actual engine maximum RPM.
    const float fraction=std::clamp(sample.rpm/sample.max_rpm,0.f,1.f);
    const size_t frame=std::min(rpm.size()-1,size_t(fraction*float(rpm.size()-1)));
    DrawLayer(draw,rpm[frame],origin,scale);
    DrawLayer(draw,gears[std::clamp(sample.gear,0,6)+1],origin,scale);
    DrawLayer(draw,base,origin,scale);
    if(input_animation){
      auto input_frame=[&](float value){
        if(!sample.inputs_valid||!std::isfinite(value))return size_t(0);
        return size_t(std::clamp(value,0.f,1.f)*float(throttle.size()-1));
      };
      DrawLayer(draw,brake[input_frame(sample.front_brake)],origin,scale);
      DrawLayer(draw,throttle[input_frame(sample.throttle)],origin,scale);
    }
    for(const auto& run:gear_labels){
      int delta=0;
      if(run.role.starts_with("gear:"))delta=std::stoi(run.role.substr(5));
      const int value=std::max(0,sample.gear)+delta;
      if(value>=0&&value<=6)DrawText(draw,run,value?std::to_string(value):"N",origin,scale);
    }
    const int velocity=int(std::clamp(sample.mph*(sample.metric?1.609f:1.f),0.f,1999.f));
    for(const auto& run:speed)DrawText(draw,run,std::to_string(velocity),origin,scale);
    DrawLayer(draw,sample.metric?metric:imperial,origin,scale);
    if(tyre_panel&&sample.tyres_valid&&sample.tyre_hud_visible&&sample.tyre_replaced&&
        std::isfinite(sample.front_tyre)&&std::isfinite(sample.rear_tyre)){
      DrawLayer(draw,tyre_neutral_body,origin,scale);
      const auto tyre=[&](const Layer& layer,const std::array<float,4>& bounds,float health){
        health=std::clamp(health,0.f,1.f);
        const float fill=sample.tyres_colored?health:1.f;
        if(fill<=0)return;
        draw->PushClipRect(ImVec2(origin.x+bounds[0]*scale,origin.y+(bounds[3]-(bounds[3]-bounds[1])*fill)*scale),
            ImVec2(origin.x+bounds[2]*scale,origin.y+bounds[3]*scale),true);
        DrawLayer(draw,layer,origin,scale,sample.tyres_colored?TyreColor(health):IM_COL32_WHITE);
        draw->PopClipRect();
      };
      tyre(tyre_front,tyre_front_bounds,sample.front_tyre);
      tyre(tyre_rear,tyre_rear_bounds,sample.rear_tyre);
      // Donor depth order: the motorcycle bitmap and wheel outlines cover
      // the original white vector fills. No invented damage/temp layers.
      DrawLayer(draw,tyre_background,origin,scale);
      DrawLayer(draw,tyre_temperature_covers,origin,scale);
    }
    draw->PopClipRect();
  }

 private:
  size_t vertex_budget=0;
  Layer ReadLayer(const Json& array) {
    if(!array.is_array()||array.size()>512)throw std::runtime_error("Invalid gauge layer");
    Layer result;
    for(const auto& object:array){
      Mesh mesh;mesh.texture=object.at("texture").get<int>();
      if(mesh.texture< -1||mesh.texture>=int(textures.size()))throw std::runtime_error("Invalid gauge texture index");
      const auto& vertices=object.at("vertices");const auto& indices=object.at("indices");
      if(vertices.size()>4096||indices.size()>12288||indices.size()%3||vertex_budget+vertices.size()>250000)throw std::runtime_error("Gauge mesh exceeds limits");
      vertex_budget+=vertices.size();
      for(const auto& values:vertices){
        const auto v=Numbers<8>(values);
        if(v[2]<-2||v[2]>3||v[3]<-2||v[3]>3)throw std::runtime_error("Gauge UV outside supported range");
        mesh.vertices.push_back({{v[0],v[1]},{v[2],v[3]},Color({v[4],v[5],v[6],v[7]})});
      }
      for(const auto& value:indices){
        const auto index=value.get<unsigned>();
        if(index>=mesh.vertices.size())throw std::runtime_error("Gauge triangle is outside vertex array");
        mesh.indices.push_back(uint16_t(index));
      }
      result.push_back(std::move(mesh));
    }
    return result;
  }
  std::vector<Text> ReadText(const Json& array) {
    if(!array.is_array()||array.size()>8)throw std::runtime_error("Invalid gauge text count");
    std::vector<Text> result;
    for(const auto& object:array){
      const auto& definition=object.at("text");Text run;
      run.font=definition.at("font").get<std::string>();run.role=object.at("role").get<std::string>();
      if(run.role!="speed"&&run.role!="gear:-2"&&run.role!="gear:-1"&&run.role!="gear:0"&&run.role!="gear:1"&&run.role!="gear:2")throw std::runtime_error("Invalid gauge text role");
      run.bounds=Numbers<4>(definition.at("bounds"));run.matrix=Numbers<6>(object.at("matrix"));
      run.height=Number(definition.at("height"),1,200);run.alignment=definition.at("alignment").get<int>();
      const auto source=Numbers<4>(definition.at("color"));
      const auto multiply=Numbers<4>(object.at("color_transform")[0]),add=Numbers<4>(object.at("color_transform")[1]);
      for(size_t i=0;i<4;++i)run.color[i]=std::clamp(source[i]*multiply[i]+add[i],0.f,1.f);
      result.push_back(std::move(run));
    }
    return result;
  }
  void DrawLayer(ImDrawList* draw,const Layer& layer,ImVec2 origin,float scale,ImU32 tint=IM_COL32_WHITE) const {
    for(const auto& mesh:layer){
      if(mesh.indices.empty())continue;
      if(mesh.texture<0){
        for(size_t i=0;i<mesh.indices.size();i+=3){
          const auto& a=mesh.vertices[mesh.indices[i]];const auto& b=mesh.vertices[mesh.indices[i+1]];const auto& c=mesh.vertices[mesh.indices[i+2]];
          draw->AddTriangleFilled(ImVec2(origin.x+a.position.x*scale,origin.y+a.position.y*scale),
              ImVec2(origin.x+b.position.x*scale,origin.y+b.position.y*scale),
              ImVec2(origin.x+c.position.x*scale,origin.y+c.position.y*scale),MultiplyColor(a.color,tint));
        }
      }else{
        draw->PushTextureID(reinterpret_cast<ImTextureID>(textures[mesh.texture].get()));
        draw->PrimReserve(int(mesh.indices.size()),int(mesh.vertices.size()));
        const auto first=draw->_VtxCurrentIdx;
        for(const auto index:mesh.indices)draw->PrimWriteIdx(ImDrawIdx(first+index));
        for(const auto& v:mesh.vertices)draw->PrimWriteVtx(ImVec2(origin.x+v.position.x*scale,origin.y+v.position.y*scale),v.uv,MultiplyColor(v.color,tint));
        draw->PopTextureID();
      }
    }
  }
  void DrawText(ImDrawList* draw,const Text& run,const std::string& text,ImVec2 origin,float scale) const {
    const auto& font=fonts.at(run.font);const float font_scale=run.height/font.size;
    float advance=0;for(const char c:text)advance+=font.glyphs.at(c).advance*font_scale;
    float x=run.bounds[0]+2;
    if(run.alignment==2)x+=(run.bounds[2]-run.bounds[0]-4-advance)*.5f;
    else if(run.alignment==1)x+=run.bounds[2]-run.bounds[0]-4-advance;
    for(const char c:text){
      const auto& glyph=font.glyphs.at(c);
      const float left=x+glyph.xoffset*font_scale,top=run.bounds[1]+2+glyph.yoffset*font_scale;
      const float right=left+glyph.width*font_scale,bottom=top+glyph.height*font_scale;
      auto convert=[&](float u,float v){const auto p=Transform(run.matrix,u,v);return ImVec2(origin.x+p.x*scale,origin.y+p.y*scale);};
      draw->AddImageQuad(reinterpret_cast<ImTextureID>(textures[font.texture].get()),convert(left,top),convert(right,top),convert(right,bottom),convert(left,bottom),
          ImVec2(float(glyph.x)/font.width,float(glyph.y)/font.height),ImVec2(float(glyph.x+glyph.width)/font.width,float(glyph.y)/font.height),
          ImVec2(float(glyph.x+glyph.width)/font.width,float(glyph.y+glyph.height)/font.height),ImVec2(float(glyph.x)/font.width,float(glyph.y+glyph.height)/font.height),Color(run.color));
      x+=glyph.advance*font_scale;
    }
  }
};

class Overlay final:public rex::ui::ImGuiDialog {
 public:
  Overlay(rex::ui::ImGuiDrawer* drawer,rex::ui::ImmediateDrawer* immediate,
      const std::filesystem::path& assets,const std::filesystem::path& user_root,std::function<bool()> blocked)
      :ImGuiDialog(drawer),preferences_(user_root/"hud-display.toml"),blocked_(std::move(blocked)) {
    SetAvailable(false);SetCaptureEnabled(false);
    try{
      if(std::filesystem::exists(preferences_)){
        if(std::filesystem::file_size(preferences_)>65536)throw std::runtime_error("Gauge preferences exceed size limit");
        const auto table=toml::parse_file(preferences_.string());
        const auto key=table["gauge_style"].value_or(std::string("original"));
        SelectStyle(0);
        for(int style=1;style<kStyleCount;++style)if(key==StyleKey(style))SelectStyle(style);
      }
      saved_=SelectedStyle();
    }catch(const std::exception& e){preferences_writable_=false;REXLOG_WARN("Cannot load gauge preferences: {}",e.what());}
    try{
      if(std::filesystem::is_regular_file(assets/"gauge.json")){
        assets_=std::make_unique<Assets>(immediate,assets);SetAvailable(true);SetTyrePanelAvailable(assets_->tyre_panel);
      }
    }catch(const std::exception& e){REXLOG_WARN("MotoGP 17 gauge unavailable; retaining original HUD: {}",e.what());}
    for(int style=2;style<kStyleCount;++style)try{
      const auto directory=assets.parent_path()/StyleKey(style);
      if(std::filesystem::is_regular_file(directory/"gauge.json")){
        classic_[style-2]=LoadClassicRenderer(immediate,directory,StyleKey(style));SetStyleAvailable(style,true);
      }
    }catch(const std::exception& e){REXLOG_WARN("{} gauge unavailable; retaining other gauge styles: {}",StyleKey(style),e.what());}
  }
  ~Overlay() override {SetCaptureEnabled(false);SetAvailable(false);ResetCapture();Save();}
 protected:
  void OnDraw(ImGuiIO& io) override {
    if(ImGui::GetTime()>=save_retry_)Save();
    const auto style=SelectedStyle();
    const bool show=StyleAvailable(style)&&((style==1&&assets_)||(style>=2&&style<kStyleCount&&classic_[style-2]))&&!(blocked_&&blocked_());
    SetCaptureEnabled(show);
    if(!show)return;
    for(const auto& sample:ReadFrame())if(sample){
      if(style==1)assets_->Draw(ImGui::GetBackgroundDrawList(),*sample,io.DisplaySize);
      else classic_[style-2]->Draw(ImGui::GetBackgroundDrawList(),*sample,io.DisplaySize);
    }
  }
 private:
  void Save() {
    const int selected=SelectedStyle();
    if(!preferences_writable_||selected==saved_)return;
    save_retry_=ImGui::GetTime()+2.;
#ifdef _WIN32
    uint64_t hash=14695981039346656037ull;
    for(auto c:preferences_.lexically_normal().wstring()){hash^=static_cast<uint16_t>(towlower(c));hash*=1099511628211ull;}
    const auto name=L"Local\\MotoGPGaugePreferences-"+std::to_wstring(hash);
    HANDLE lock=CreateMutexW(nullptr,FALSE,name.c_str());if(!lock)return;
    const auto wait=WaitForSingleObject(lock,0);
    if(wait!=WAIT_OBJECT_0&&wait!=WAIT_ABANDONED){CloseHandle(lock);return;}
    struct Unlock {HANDLE lock;~Unlock(){ReleaseMutex(lock);CloseHandle(lock);}} unlock{lock};
#endif
    auto temporary=preferences_;temporary+=".tmp";
    try{
      auto table=std::filesystem::exists(preferences_)?toml::parse_file(preferences_.string()):toml::table{};
      table.insert_or_assign("gauge_style",StyleKey(selected));
      std::filesystem::create_directories(preferences_.parent_path());
      std::ofstream file(temporary,std::ios::binary|std::ios::trunc);file<<table;file.close();
      if(!file)throw std::runtime_error("Cannot write gauge preferences");
#ifdef _WIN32
      if(!MoveFileExW(temporary.c_str(),preferences_.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error("Cannot replace gauge preferences");
#else
      std::filesystem::rename(temporary,preferences_);
#endif
      saved_=selected;
    }catch(const std::exception& e){
      std::error_code ec;std::filesystem::remove(temporary,ec);
      if(!save_warned_){save_warned_=true;REXLOG_WARN("Cannot save gauge preferences: {}",e.what());}
    }
  }
  std::filesystem::path preferences_;
  std::function<bool()> blocked_;
  std::unique_ptr<Assets> assets_;
  std::array<std::unique_ptr<ClassicRenderer>,kStyleCount-2> classic_;
  int saved_=0;
  bool preferences_writable_=true,save_warned_=false;
  double save_retry_=0;
};
}

std::unique_ptr<rex::ui::ImGuiDialog> CreateOverlay(rex::ui::ImGuiDrawer* drawer,
    rex::ui::ImmediateDrawer* immediate,const std::filesystem::path& assets,
    const std::filesystem::path& user_root,std::function<bool()> blocked) {
  return std::make_unique<Overlay>(drawer,immediate,assets,user_root,std::move(blocked));
}
}
