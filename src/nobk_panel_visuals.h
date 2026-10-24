#pragma once
// F1 presentation only: proportions follow the approved left-side Apex concept.
#include <imgui.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cfloat>
#include <string>
#include <string_view>
#include "button_prompt_types.h"

namespace motogp::nobk::visual {
struct ButtonHints { const char* previous; const char* next; const char* select; const char* website; const char* back; const char* decline; };
inline ButtonHints Hints(prompts::Style style) {
  switch(style) {
    case prompts::Style::PlayStation:return {"L1","R1","Cross","Triangle","Circle","Square"};
    case prompts::Style::Xbox:return {"LB","RB","A","Y","B","X"};
    default:return {"Q","E","Enter","W","Esc","Delete"};
  }
}
inline float Smooth(float t){t=std::clamp(t,0.f,1.f);return t*t*(3.f-2.f*t);}
struct Layout {
  float scale,left,width,gutter,body_left,body_top,body_width,body_height;
};
inline Layout Measure(ImVec2 viewport) {
  const float s=std::clamp(std::min(viewport.y/1080.f,viewport.x/1280.f),.25f,2.5f);
  const float width=std::max(1.f,std::min(1060.f*s,viewport.x));
  const float left=356.f*s,top=214.f*s;
  return {s,0,width,36.f*s,left,top,std::max(1.f,std::min(640.f*s,width-left)),
      std::max(40.f,viewport.y-top-132.f*s)};
}
inline float Entrance(double now,double opened){return Smooth(float(now-opened)/.22f);}
inline float RowOffset(double now,double opened,double changed,int visible,float scale) {
  const auto age=float(std::min(now-opened,now-changed));
  return 18.f*scale*(1.f-Smooth((age-.025f*std::max(0,visible))/.22f));
}
inline ImU32 Color(int r,int g,int b,float alpha=1.f) {
  return IM_COL32(r,g,b,int(std::clamp(alpha,0.f,1.f)*255.f));
}
inline void ApexLines(ImDrawList* draw,float scale,float alpha,double elapsed) {
  // Cover-inspired strands, sampled once. Only light/color moves; neither
  // the path nor the menu shifts. Silver-violet and champagne echo the cover.
  static const auto paths=[] {
    const ImVec2 controls[7][3][4]={
      {{{300,206},{294,280},{274,332},{292,371}},
       {{292,371},{308,405},{362,449},{332,499}},
       {{332,499},{299,568},{122,647},{0,706}}},
      {{{310,212},{307,284},{290,338},{307,377}},
       {{307,377},{323,411},{377,452},{348,508}},
       {{348,508},{316,581},{141,662},{0,724}}},
      {{{327,244},{321,303},{309,341},{321,381}},
       {{321,381},{338,417},{390,465},{364,515}},
       {{364,515},{333,584},{177,657},{48,718}}},
      {{{320,186},{314,260},{294,312},{312,351}},
       {{312,351},{328,385},{382,429},{352,479}},
       {{352,479},{319,548},{142,627},{20,686}}},
      {{{284,218},{278,292},{258,344},{276,383}},
       {{276,383},{292,417},{346,461},{316,511}},
       {{316,511},{283,580},{106,659},{-16,718}}},
      {{{325,206},{319,280},{299,332},{317,371}},
       {{317,371},{333,405},{387,449},{357,499}},
       {{357,499},{324,568},{147,647},{25,706}}},
      {{{306,228},{300,302},{280,354},{298,393}},
       {{298,393},{314,427},{368,471},{338,521}},
       {{338,521},{305,590},{128,669},{6,728}}}
    };
    std::array<std::array<ImVec2,73>,7> result{};
    for(int strand=0;strand<7;++strand)for(int i=0;i<=72;++i) {
      const int segment=std::min(i/24,2);const float t=float(i-segment*24)/24.f,u=1-t;
      const auto* c=controls[strand][segment];
      result[strand][i]=ImVec2(u*u*u*c[0].x+3*u*u*t*c[1].x+3*u*t*t*c[2].x+t*t*t*c[3].x,
          u*u*u*c[0].y+3*u*u*t*c[1].y+3*u*t*t*c[2].y+t*t*t*c[3].y);
    }
    return result;
  }();
  struct Strand {int r,g,b;float strength,width,begin,end;double period,phase;};
  constexpr Strand strands[]{
      {255,106,19,.82f,1.15f,0,1,8.5,.14},
      {179,163,199,.34f,.85f,.04f,1,11.0,.57},
      {246,205,140,.42f,.85f,.10f,.80f,9.7,.02},
      {210,77,54,.29f,.75f,.01f,.38f,12.3,.79},
      {236,219,187,.23f,.70f,.54f,.99f,15.0,.31},
      {130,124,162,.27f,.75f,.28f,.92f,13.1,.68},
      {219,145,62,.27f,.80f,.59f,1.f,17.0,.44}};
  for(int k=6;k>=0;--k) {
    const auto& strand=strands[k];
    const float head=float(std::fmod(std::max(0.0,elapsed)/strand.period+strand.phase,1.0))*1.36f-.18f;
    for(int i=0;i<72;++i) {
      const float progress=(i+.5f)/72.f;
      const float edge=Smooth((progress-strand.begin)/.10f)*Smooth((strand.end-progress)/.13f);
      if(edge<=0)continue;
      const float light=Smooth(1.f-std::abs(progress-head)/.075f);
      const int r=strand.r+int((255-strand.r)*light*.85f);
      const int g=strand.g+int((235-strand.g)*light*.85f);
      const int b=strand.b+int((201-strand.b)*light*.75f);
      const auto a=paths[k][i],bpos=paths[k][i+1];
      const ImVec2 first(a.x*scale,a.y*scale),last(bpos.x*scale,bpos.y*scale);
      const float opacity=alpha*edge;
      draw->AddLine(first,last,Color(r,g,b,opacity*(.025f+.03f*light)),(k?5.f:8.f)*scale);
      draw->AddLine(first,last,Color(r,g,b,opacity*(.075f+.10f*light)),(k?2.2f:3.6f)*scale);
      draw->AddLine(first,last,Color(r,g,b,opacity*(strand.strength+.16f*light)),strand.width*scale);
    }
  }
}
inline void Underline(ImDrawList* draw,ImVec2 a,float right,float scale,float alpha) {
  for(int i=3;i>0;--i)draw->AddLine(a,ImVec2(right,a.y),Color(255,106,19,alpha*(i==1?.8f:.045f)),float(i*i)*scale);
}
inline void Marker(ImDrawList* draw,ImVec2 center,float scale,float alpha) {
  draw->AddTriangleFilled(ImVec2(center.x-8*scale,center.y-11*scale),
      ImVec2(center.x+9*scale,center.y),ImVec2(center.x-8*scale,center.y+11*scale),Color(255,106,19,alpha));
}
inline void Strip(ImDrawList* draw,ImVec2 a,ImVec2 b,float /*slant*/,bool selected,float alpha,float underline_inset=1.f) {
  const float scale=std::max(.25f,(b.y-a.y)/100.f);
  if(selected){
    Marker(draw,ImVec2(a.x+10*scale,(a.y+b.y)*.5f),scale,alpha);
    Underline(draw,ImVec2(a.x+110*scale,b.y-underline_inset*scale),b.x,scale,alpha);
  } else draw->AddLine(ImVec2(a.x+36*scale,b.y-1),ImVec2(b.x,b.y-1),Color(217,221,226,.18f*alpha),1.f);
}
inline void Text(ImDrawList* draw,ImFont* font,float size,ImVec2 at,ImU32 color,
    const std::string& value,float right) {
  if(right<=at.x||value.empty())return;
  const ImVec4 clip(at.x,at.y-2.f,right,at.y+size*1.6f);
  draw->AddText(font,size,at,color,value.c_str(),nullptr,0,&clip);
}
inline float TextWidth(ImFont* font,float size,const std::string& text) {
  return font->CalcTextSizeA(size*1.35f,FLT_MAX,0,text.c_str()).x*.85f;
}
inline void Label(ImDrawList* draw,ImFont* font,float size,ImVec2 at,ImVec4 color,
    const std::string& text,float right,float scale,float alpha) {
  const ImVec2 pos(at.x,at.y-size*.175f);
  const float clip=at.x+(right-at.x)/.85f;const int first=draw->VtxBuffer.Size;
  const auto old_min=draw->GetClipRectMin(),old_max=draw->GetClipRectMax();
  // CPU glyph clipping happens before the compact-letter transform. Expand
  // its input scissor when needed so right-aligned times/scores keep their
  // last glyph; the transformed vertices still end inside the original clip.
  const bool expand=at.x+std::min(font->CalcTextSizeA(size*1.35f,FLT_MAX,0,text.c_str()).x+scale,clip-at.x)>old_max.x;
  if(expand)draw->PushClipRect(ImVec2(at.x+(old_min.x-at.x)/.85f,old_min.y),
      ImVec2(at.x+(old_max.x-at.x)/.85f,old_max.y),false);
  Text(draw,font,size*1.35f,ImVec2(pos.x+scale,pos.y+scale),Color(0,0,0,.45f*alpha),text,clip);
  Text(draw,font,size*1.35f,pos,ImGui::GetColorU32(color),text,clip);
  for(int i=first;i<draw->VtxBuffer.Size;++i)draw->VtxBuffer[i].pos.x=at.x+(draw->VtxBuffer[i].pos.x-at.x)*.85f;
  if(expand)draw->PopClipRect();
}
inline float WrappedHeight(ImFont* font,float size,const std::string& text,float width) {
  return font->CalcTextSizeA(size*1.35f,FLT_MAX,width/.85f,text.c_str()).y;
}
inline void WrappedLabel(ImDrawList* draw,ImFont* font,float size,ImVec2 at,ImVec4 color,
    const std::string& text,float width) {
  const int first=draw->VtxBuffer.Size;
  draw->AddText(font,size*1.35f,ImVec2(at.x,at.y-size*.175f),ImGui::GetColorU32(color),text.c_str(),nullptr,width/.85f);
  for(int i=first;i<draw->VtxBuffer.Size;++i)draw->VtxBuffer[i].pos.x=at.x+(draw->VtxBuffer[i].pos.x-at.x)*.85f;
}
inline std::string Initial(std::string_view name) {
  if(name.empty())return "?";
  const auto c=static_cast<unsigned char>(name[0]);
  size_t bytes=c<0x80?1:c>=0xC2&&c<=0xDF?2:c>=0xE0&&c<=0xEF?3:c>=0xF0&&c<=0xF4?4:0;
  if(!bytes||name.size()<bytes)return "?";
  for(size_t i=1;i<bytes;++i)if((static_cast<unsigned char>(name[i])&0xC0)!=0x80)return "?";
  std::string out(name.substr(0,bytes));
  if(bytes==1&&out[0]>='a'&&out[0]<='z')out[0]-=32;
  return out;
}
struct AchievementAppearance {
  float scale=1.f,alpha=1.f;
  double now=0,opened=0,changed=0;
  ImFont* regular=nullptr;
  ImFont* semibold=nullptr;
  ImFont* bold=nullptr;
};
} // namespace motogp::nobk::visual
