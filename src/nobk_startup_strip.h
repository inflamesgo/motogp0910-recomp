#pragma once
// MotoGP startup strip, adapted from the FIFA 09 v1.4.0 FriendsStrip. Lifecycle, input and
// account data remain with the existing panel; this helper never owns a window.
// Reference: ChunchunQ/fifa09-recomp v1.4.0 (MIT).
#include <imgui.h>
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <string>

namespace motogp::nobk::startup {
namespace detail {
inline ImU32 Color(int r,int g,int b,float alpha) {
  return IM_COL32(r,g,b,int(255.f*std::clamp(alpha,0.f,1.f)));
}
inline ImVec2 TextSize(ImFont* font,float size,const char* text) {
  return font->CalcTextSizeA(size,FLT_MAX,0,text);
}
inline float LetterWidth(ImFont* font,float size,char letter) {
  const char text[]{letter,0};return TextSize(font,size,text).x;
}
inline float WordWidth(ImFont* font,float size,const char* word) {
  const float tracking=size*.06f,stroke=std::max(1.f,size*.022f);
  float width=0;
  for(const char* p=word;*p;++p)width+=LetterWidth(font,size,*p)+tracking;
  return width-tracking+stroke;
}
inline float BrandWidth(ImFont* font,float size) {
  const float radius=size*.085f;
  return WordWidth(font,size,"NOBK")+radius*2+size*.22f+WordWidth(font,size,"FRIENDS");
}
inline void Word(ImDrawList* draw,ImFont* font,float size,ImVec2 position,
                 const char* word,ImU32 color) {
  const float tracking=size*.06f,stroke=std::max(1.f,size*.022f);
  for(const char* p=word;*p;++p) {
    const char letter[]{*p,0};
    draw->AddText(font,size,position,color,letter);
    draw->AddText(font,size,ImVec2(position.x+stroke,position.y),color,letter);
    position.x+=LetterWidth(font,size,*p)+tracking;
  }
}
inline void Brand(ImDrawList* draw,ImFont* font,float size,ImVec2 position,float alpha) {
  const float left=WordWidth(font,size,"NOBK"),radius=size*.085f;
  const float gap=radius*2+size*.22f;
  float dot_y=size*.5f;
  if(auto* baked=font->GetFontBaked(size))
    if(const auto* glyph=baked->FindGlyph('N'))
      dot_y=(glyph->Y0+glyph->Y1)*.5f*(baked->Size>0?size/baked->Size:1.f);
  const auto white=Color(255,255,255,alpha);
  Word(draw,font,size,position,"NOBK",white);
  draw->AddCircleFilled(ImVec2(position.x+left+gap*.5f,position.y+dot_y),radius,
                        Color(255,106,19,alpha),24);
  Word(draw,font,size,ImVec2(position.x+left+gap,position.y),"FRIENDS",white);
}
}  // namespace detail

struct Layout {
  ImVec2 minimum{},maximum{};
  float scale=0,key_width=0,brand_width=0,count_width=0,invite_width=0;
  std::string count,invite;
};

inline Layout Measure(ImFont* font,ImVec2 viewport,float alpha,int friends,int invites,
                      const std::string& key="F1",
                      const std::string& hint="",bool turkish=false) {
  Layout layout;
  if(!font||!std::isfinite(viewport.x)||!std::isfinite(viewport.y)||
     viewport.x<=0||viewport.y<=0)return layout;
  friends=std::max(0,friends);invites=std::max(0,invites);
  layout.count=std::to_string(friends)+(turkish?" arkadaş çevrimiçi":friends==1?" friend online":" friends online");
  layout.invite=std::to_string(invites)+(turkish?" davet":invites==1?" invite":" invites");
  float scale=std::clamp(viewport.y/1080.f,.7f,2.5f),width=0;
  const float available=std::max(1.f,viewport.x-24*scale);
  const auto measure=[&] {
    layout.key_width=detail::TextSize(font,20*scale,key.c_str()).x+24*scale;
    layout.brand_width=detail::BrandWidth(font,22*scale);
    layout.count_width=18*scale+detail::TextSize(font,18*scale,layout.count.c_str()).x;
    layout.invite_width=detail::TextSize(font,18*scale,layout.invite.c_str()).x+24*scale;
    width=24*scale+layout.key_width+24*scale+layout.brand_width+32*scale+
          layout.count_width+(invites>0?20*scale+layout.invite_width:0.f)+
          24*scale+(hint.empty()?0.f:1.f+24*scale+
          detail::TextSize(font,18*scale,hint.c_str()).x+24*scale);
  };
  // Only unusually narrow viewports need
  // proportional fitting; keep every word rather than clipping or ellipsizing.
  for(int attempt=0;attempt<6;++attempt) {
    measure();
    if(width<=available)break;
    scale*=available/width*.995f;
  }
  measure();layout.scale=scale;
  const float x=(viewport.x-width)*.5f;
  const float y=22*scale-(1-std::clamp(alpha,0.f,1.f))*16*scale;
  layout.minimum=ImVec2(x,y);layout.maximum=ImVec2(x+width,y+52*scale);
  return layout;
}

inline void Draw(ImDrawList* draw,ImFont* font,ImVec2 viewport,float alpha,int friends,int invites,
                 const std::string& key="F1",
                 const std::string& hint="",bool turkish=false) {
  if(!draw||!font||!std::isfinite(alpha)||alpha<=0)return;
  alpha=std::clamp(alpha,0.f,1.f);
  const auto layout=Measure(font,viewport,alpha,friends,invites,key,hint,turkish);
  const float s=layout.scale;
  if(s<=0)return;
  const auto a=layout.minimum,b=layout.maximum;
  const float cy=(a.y+b.y)*.5f,rounding=18*s;
  draw->PushClipRect(ImVec2(0,0),viewport,true);
  draw->AddRectFilled(ImVec2(a.x,a.y+6*s),ImVec2(b.x,b.y+6*s),
                      detail::Color(0,0,0,.24f*alpha),rounding);
  draw->AddRectFilled(a,b,detail::Color(12,14,18,.92f*alpha),rounding);
  draw->AddRect(a,b,detail::Color(190,196,208,.28f*alpha),rounding,0,1.f);

  float x=a.x+24*s;
  draw->AddRectFilled(ImVec2(x,cy-18*s),ImVec2(x+layout.key_width,cy+18*s),
                      detail::Color(32,35,41,alpha),5*s);
  draw->AddRect(ImVec2(x,cy-18*s),ImVec2(x+layout.key_width,cy+18*s),
                detail::Color(190,196,208,.65f*alpha),5*s,0,1.f);
  const auto key_size=detail::TextSize(font,20*s,key.c_str());
  draw->AddText(font,20*s,ImVec2(x+(layout.key_width-key_size.x)*.5f,cy-key_size.y*.5f),
                detail::Color(240,240,244,alpha),key.c_str());
  x+=layout.key_width+24*s;
  detail::Brand(draw,font,22*s,ImVec2(x,cy-detail::TextSize(font,22*s,"N").y*.5f),alpha);
  x+=layout.brand_width+32*s;
  draw->AddCircleFilled(ImVec2(x+6*s,cy),6*s,
                        friends>0?detail::Color(61,220,132,alpha):detail::Color(120,120,126,alpha),16);
  x+=18*s;
  const float count_y=cy-detail::TextSize(font,18*s,"N").y*.5f;
  draw->AddText(font,18*s,ImVec2(x,count_y),detail::Color(220,220,224,alpha),layout.count.c_str());
  x+=detail::TextSize(font,18*s,layout.count.c_str()).x;
  if(invites>0) {
    x+=20*s;
    draw->AddRectFilled(ImVec2(x,cy-16*s),ImVec2(x+layout.invite_width,cy+16*s),
                        detail::Color(255,106,19,alpha),16*s);
    draw->AddText(font,18*s,ImVec2(x+12*s,count_y),detail::Color(17,17,17,alpha),layout.invite.c_str());
    x+=layout.invite_width;
  }
  if(!hint.empty()) {
    x+=24*s;
    draw->AddLine(ImVec2(x,cy-13*s),ImVec2(x,cy+13*s),detail::Color(255,255,255,.32f*alpha),1.f);
    x+=1.f+24*s;
    draw->AddText(font,18*s,ImVec2(x,cy-detail::TextSize(font,18*s,"N").y*.5f),
                  detail::Color(168,168,176,alpha),hint.c_str());
  }
  draw->PopClipRect();
}
}  // namespace motogp::nobk::startup
