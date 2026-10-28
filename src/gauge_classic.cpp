#include "gauge_classic.h"
#include "gauge_capture.h"
#include "../third_party/nlohmann/json.hpp"

#include <rex/ui/image_decode.h>
#include <rex/ui/immediate_drawer.h>
#include <imgui.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace motogp::gauge {
namespace {
using Json=nlohmann::json;
float Number(const Json& value,float low=-4096,float high=4096){
  const float result=value.get<float>();
  if(!std::isfinite(result)||result<low||result>high)throw std::runtime_error("Invalid classic gauge geometry");
  return result;
}
template<size_t N> std::array<float,N> Numbers(const Json& value,float low=-4096,float high=4096){
  if(!value.is_array()||value.size()!=N)throw std::runtime_error("Invalid classic gauge vector");
  std::array<float,N> result{};for(size_t i=0;i<N;++i)result[i]=Number(value[i],low,high);return result;
}
struct Sprite {int texture=0;std::array<float,4> uv{},rect{};ImU32 tint=IM_COL32_WHITE;};
struct Digit {Sprite sprite;float advance=0;};
struct Readout {std::array<float,2> center{};float scale=1,ghost=0,line_height=0;int cells=1;bool pad_zero=false;ImU32 color=IM_COL32_WHITE;};
struct Light {Sprite sprite;float threshold=0;ImU32 off=0,on=0;int blink_frames=0;bool normalized_rpm=false;};
ImU32 Color(const Json& value){
  const auto color=Numbers<3>(value,0,255);return IM_COL32(int(color[0]),int(color[1]),int(color[2]),255);
}

class Renderer final:public ClassicRenderer {
 public:
  Renderer(rex::ui::ImmediateDrawer* drawer,const std::filesystem::path& directory,const char* style){
    if(!drawer)throw std::runtime_error("Classic gauge texture drawer unavailable");
    const auto path=directory/"gauge.json";
    const auto size=std::filesystem::file_size(path);
    if(!size||size>4*1024*1024)throw std::runtime_error("Invalid classic gauge pack size");
    std::vector<char> bytes(size);std::ifstream file(path,std::ios::binary);
    if(!file.read(bytes.data(),std::streamsize(size)))throw std::runtime_error("Cannot read classic gauge pack");
    const auto json=Json::parse(bytes.begin(),bytes.end());
    if(json.at("schema")!=1||json.at("kind")!="classic-gauge"||json.at("style")!=style)
      throw std::runtime_error("Unsupported classic gauge schema");
    const auto bounds=Numbers<4>(json.at("visible_bounds"),0,2048);
    if(bounds[2]<=bounds[0]||bounds[3]<=bounds[1])throw std::runtime_error("Invalid classic gauge bounds");
    bounds_=bounds;
    const auto& images=json.at("textures");
    if(!images.is_array()||images.empty()||images.size()>8)throw std::runtime_error("Invalid classic gauge image count");
    for(const auto& image:images){
      const auto hex=image.get<std::string>();
      if(hex.empty()||hex.size()%2||hex.size()>4*1024*1024)throw std::runtime_error("Invalid classic gauge image encoding");
      const auto nibble=[](char c)->uint8_t{
        if(c>='0'&&c<='9')return uint8_t(c-'0');
        if(c>='a'&&c<='f')return uint8_t(c-'a'+10);
        throw std::runtime_error("Invalid classic gauge image encoding");
      };
      std::vector<uint8_t> png(hex.size()/2);
      for(size_t i=0;i<png.size();++i)png[i]=uint8_t(nibble(hex[i*2])*16+nibble(hex[i*2+1]));
      if(png.size()<24||std::memcmp(png.data(),"\x89PNG\r\n\x1a\n",8))throw std::runtime_error("Expected classic gauge PNG");
      const auto dimension=[&](size_t at){return uint32_t(png[at])<<24|uint32_t(png[at+1])<<16|uint32_t(png[at+2])<<8|png[at+3];};
      if(!dimension(16)||!dimension(20)||dimension(16)>2048||dimension(20)>2048)throw std::runtime_error("Classic gauge image too large");
      int width=0,height=0;const auto rgba=rex::ui::DecodeImageRGBA(png.data(),png.size(),width,height);
      if(width<=0||height<=0||rgba.size()!=size_t(width)*height*4)throw std::runtime_error("Cannot decode classic gauge image");
      auto texture=drawer->CreateTexture(width,height,rex::ui::ImmediateTextureFilter::kLinear,false,rgba.data());
      if(!texture)throw std::runtime_error("Cannot create classic gauge texture");
      textures_.push_back(std::move(texture));
    }
    const auto& layers=json.at("static");
    if(!layers.is_array()||layers.empty()||layers.size()>8)throw std::runtime_error("Invalid classic gauge layers");
    for(const auto& layer:layers)static_.push_back(ReadSprite(layer));
    needle_=ReadSprite(json.at("needle"));pivot_=Numbers<2>(json.at("needle").at("pivot"));
    normalized_rpm_=json.at("needle").value("normalized_rpm",false);
    const auto& knots=json.at("needle").at("rpm_angles");
    if(!knots.is_array()||knots.size()<2||knots.size()>32)throw std::runtime_error("Invalid classic gauge calibration");
    for(const auto& knot:knots){
      const auto pair=Numbers<2>(knot,0,30000);
      if(pair[1]>1080||(!angles_.empty()&&(pair[0]<=angles_.back()[0]||pair[1]<=angles_.back()[1])))
        throw std::runtime_error("Invalid classic gauge calibration order");
      angles_.push_back(pair);
    }
    const auto& digits=json.at("digits");
    if(!digits.is_array()||digits.size()!=10)throw std::runtime_error("Invalid classic gauge digit count");
    for(size_t i=0;i<digits_.size();++i){digits_[i]={ReadSprite(digits[i]),Number(digits[i].at("advance"),.01f,512)};}
    speed_=ReadReadout(json.at("speed"));gear_=ReadReadout(json.at("gear"));
    if(speed_.cells!=3||gear_.cells!=1)throw std::runtime_error("Invalid classic gauge readout width");
    if(json.contains("lights")){
      const auto& lights=json.at("lights");
      if(!lights.is_array()||lights.size()>8)throw std::runtime_error("Invalid classic gauge light count");
      for(const auto& item:lights){
        Light light;light.sprite=ReadSprite(item.at("sprite"));
        light.normalized_rpm=item.value("normalized_rpm",false);
        light.threshold=Number(item.at("threshold"),0,light.normalized_rpm?1.f:30000.f);
        light.off=Color(item.at("off_color"));light.on=Color(item.at("on_color"));light.blink_frames=item.at("blink_frames").get<int>();
        if(light.blink_frames<0||light.blink_frames>120||light.blink_frames==1)throw std::runtime_error("Invalid classic gauge light animation");
        lights_.push_back(light);
      }
    }
  }

  void Draw(ImDrawList* draw,const Telemetry& sample,ImVec2 viewport) const override {
    const ImVec2 minimum(sample.view_x*viewport.x,sample.view_y*viewport.y);
    const ImVec2 size(sample.view_width*viewport.x,sample.view_height*viewport.y);
    const float reference=std::min(size.x/1920.f,size.y/1080.f);
    // Give each classic face the same 280 px visible height and right/bottom
    // margins as MotoGP 17, independent of transparent donor atlas padding.
    const float scale=280.f/(bounds_[3]-bounds_[1])*reference;
    const ImVec2 origin(minimum.x+size.x-120*reference-bounds_[2]*scale,
        minimum.y+size.y-73*reference-bounds_[3]*scale);
    draw->PushClipRect(minimum,ImVec2(minimum.x+size.x,minimum.y+size.y),true);
    for(const auto& sprite:static_)DrawSprite(draw,sprite,origin,scale,IM_COL32_WHITE,0);
    // Numbered faces use absolute RPM tick positions. Unnumbered faces can
    // explicitly map their complete authored sweep to this bike's RPM limit.
    const float input=normalized_rpm_?sample.rpm/sample.max_rpm:sample.rpm;
    const float rpm=std::clamp(input,angles_.front()[0],angles_.back()[0]);
    auto upper=std::upper_bound(angles_.begin(),angles_.end(),rpm,[](float value,const auto& knot){return value<knot[0];});
    if(upper==angles_.begin())++upper;if(upper==angles_.end())--upper;
    const auto& low=*(upper-1);const auto& high=*upper;
    const float degrees=low[1]+(high[1]-low[1])*(rpm-low[0])/(high[0]-low[0]);
    DrawSprite(draw,needle_,origin,scale,IM_COL32_WHITE,degrees*0.017453292519943295f);
    for(const auto& light:lights_){
      const float light_rpm=light.normalized_rpm?sample.rpm/sample.max_rpm:sample.rpm;
      const bool lit=light_rpm>=light.threshold&&(!light.blink_frames||ImGui::GetFrameCount()%light.blink_frames<light.blink_frames/2);
      DrawSprite(draw,light.sprite,origin,scale,lit?light.on:light.off,0);
    }
    const int velocity=int(std::clamp(sample.mph*(sample.metric?1.609f:1.f),0.f,999.f));
    DrawReadout(draw,speed_,velocity,origin,scale);
    DrawReadout(draw,gear_,std::clamp(sample.gear,0,6),origin,scale);
    draw->PopClipRect();
  }
 private:
  Sprite ReadSprite(const Json& value) const {
    Sprite sprite; sprite.texture=value.at("texture").get<int>();
    if(sprite.texture<0||size_t(sprite.texture)>=textures_.size())throw std::runtime_error("Invalid classic gauge texture index");
    const auto source=Numbers<4>(value.at("source"),0,2048);
    const auto& texture=textures_[sprite.texture];
    if(source[2]<=source[0]||source[3]<=source[1]||source[2]>texture->width||source[3]>texture->height)
      throw std::runtime_error("Invalid classic gauge sprite crop");
    sprite.uv={source[0]/texture->width,source[1]/texture->height,source[2]/texture->width,source[3]/texture->height};
    sprite.rect=Numbers<4>(value.at("rect"));
    if(sprite.rect[2]<=sprite.rect[0]||sprite.rect[3]<=sprite.rect[1])throw std::runtime_error("Invalid classic gauge sprite rectangle");
    if(value.contains("tint")){
      const auto tint=Numbers<4>(value.at("tint"),0,255);sprite.tint=IM_COL32(int(tint[0]),int(tint[1]),int(tint[2]),int(tint[3]));
    }
    return sprite;
  }
  Readout ReadReadout(const Json& value) const {
    Readout result;result.center=Numbers<2>(value.at("center"));
    result.scale=Number(value.at("scale"),.001f,10);result.ghost=Number(value.at("ghost_alpha"),0,1);
    result.line_height=Number(value.at("line_height"),.01f,512);
    result.cells=value.at("cells").get<int>();
    result.pad_zero=value.at("pad_zero").get<bool>();
    result.color=Color(value.at("color"));return result;
  }
  void DrawSprite(ImDrawList* draw,const Sprite& sprite,ImVec2 origin,float scale,ImU32 color,float angle) const {
    if(sprite.tint!=IM_COL32_WHITE){
      ImU32 tinted=0;for(unsigned shift=0;shift<32;shift+=8)tinted|=((((color>>shift)&255)*((sprite.tint>>shift)&255)+127)/255)<<shift;
      color=tinted;
    }
    const float c=std::cos(angle),s=std::sin(angle);
    const auto point=[&](float x,float y){
      if(angle!=0){const float dx=x-pivot_[0],dy=y-pivot_[1];x=pivot_[0]+c*dx-s*dy;y=pivot_[1]+s*dx+c*dy;}
      return ImVec2(origin.x+x*scale,origin.y+y*scale);
    };
    const auto& r=sprite.rect;const auto& uv=sprite.uv;
    draw->AddImageQuad(reinterpret_cast<ImTextureID>(textures_[sprite.texture].get()),
        point(r[0],r[1]),point(r[2],r[1]),point(r[2],r[3]),point(r[0],r[3]),
        ImVec2(uv[0],uv[1]),ImVec2(uv[2],uv[1]),ImVec2(uv[2],uv[3]),ImVec2(uv[0],uv[3]),color);
  }
  void DrawReadout(ImDrawList* draw,const Readout& readout,int value,ImVec2 origin,float scale) const {
    const float cell=digits_[8].advance;
    const float x=readout.center[0]-readout.cells*cell*readout.scale*.5f;
    const float y=readout.center[1]-readout.line_height*readout.scale*.5f;
    auto text=std::to_string(value);
    if(readout.pad_zero&&text.size()<size_t(readout.cells))text.insert(0,size_t(readout.cells)-text.size(),'0');
    for(int i=0;i<readout.cells;++i){
      const ImVec2 at(origin.x+(x+i*cell*readout.scale)*scale,origin.y+y*scale);
      if(readout.ghost>0)DrawSprite(draw,digits_[8].sprite,at,scale*readout.scale,
          (readout.color&0x00ffffffu)|(ImU32(readout.ghost*255+.5f)<<24),0);
      const int digit=i-(readout.cells-int(text.size()));
      if(digit>=0)DrawSprite(draw,digits_[text[size_t(digit)]-'0'].sprite,at,scale*readout.scale,readout.color,0);
    }
  }
  std::vector<std::unique_ptr<rex::ui::ImmediateTexture>> textures_;
  std::vector<Sprite> static_;
  Sprite needle_;
  std::array<float,2> pivot_{};
  std::array<float,4> bounds_{};
  std::vector<std::array<float,2>> angles_;
  bool normalized_rpm_=false;
  std::vector<Light> lights_;
  std::array<Digit,10> digits_;
  Readout speed_,gear_;
};
}
std::unique_ptr<ClassicRenderer> LoadClassicRenderer(rex::ui::ImmediateDrawer* drawer,
    const std::filesystem::path& directory,const char* style){return std::make_unique<Renderer>(drawer,directory,style);}
}
