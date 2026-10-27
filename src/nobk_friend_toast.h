#pragma once
// Compact MotoGP pit-board presentation with the SpeedBreaker.exe notification
// timing. Event detection, queues and image I/O live elsewhere.
#include <imgui.h>
#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace motogp::nobk::friend_toast {
inline float Smooth(float t) {t=std::clamp(t,0.f,1.f);return t*t*(3.f-2.f*t);}
inline float Duration(std::string_view text) {
  return std::min(18.f,9.f+float(text.size()>60?text.size()-60:0)/20.f);
}
namespace detail {
inline ImU32 Color(int r,int g,int b,float alpha) {
  return IM_COL32(r,g,b,int(255.f*std::clamp(alpha,0.f,1.f)));
}
inline float Width(ImFont* font,float size,const std::string& text) {
  return font->CalcTextSizeA(size,FLT_MAX,0,text.c_str()).x;
}
inline size_t Next(std::string_view text,size_t at) {
  if(at>=text.size())return text.size();
  ++at;while(at<text.size()&&(static_cast<unsigned char>(text[at])&0xc0)==0x80)++at;
  return at;
}
inline void PopCodepoint(std::string& text) {
  if(text.empty())return;
  size_t at=text.size()-1;
  while(at&&(static_cast<unsigned char>(text[at])&0xc0)==0x80)--at;
  text.resize(at);
}
inline std::string Ellipsize(ImFont* font,float size,float width,std::string text) {
  if(Width(font,size,text)<=width)return text;
  while(!text.empty()&&Width(font,size,text+"...")>width)PopCodepoint(text);
  return text+"...";
}
inline std::vector<std::string> Wrap(ImFont* font,float size,float width,std::string_view text) {
  std::vector<std::string> lines;std::string current;bool truncated=false;
  size_t cursor=0;
  while(cursor<text.size()) {
    const auto end=text.find(' ',cursor);
    std::string word(text.substr(cursor,end==std::string_view::npos?text.size()-cursor:end-cursor));
    cursor=end==std::string_view::npos?text.size():end+1;
    if(word.empty())continue;
    const std::string candidate=current.empty()?word:current+" "+word;
    if(!current.empty()&&Width(font,size,candidate)>width) {
      lines.push_back(std::move(current));current.clear();
      if(lines.size()==5){truncated=true;break;}
    }
    if(!current.empty()){current=candidate;continue;}
    // Match the donor's handling of an oversized single word, preserving UTF-8
    // character boundaries instead of emitting a partial multibyte glyph.
    while(Width(font,size,word)>width&&Next(word,0)<word.size()) {
      size_t take=Next(word,0);
      for(size_t next=Next(word,take);next<=word.size()&&next>take;next=Next(word,take)) {
        if(Width(font,size,word.substr(0,next))>width)break;
        take=next;
      }
      lines.push_back(word.substr(0,take));word.erase(0,take);
      if(lines.size()==5){truncated=true;break;}
    }
    if(truncated)break;
    current=std::move(word);
  }
  if(!truncated&&!current.empty()) {
    if(lines.size()<5)lines.push_back(std::move(current));else truncated=true;
  }
  if(truncated&&!lines.empty()) {
    auto& last=lines.back();
    while(!last.empty()&&Width(font,size,last+"...")>width)PopCodepoint(last);
    last+="...";
  }
  return lines;
}
inline ImU32 AvatarColor(std::string_view name,float alpha) {
  constexpr std::array<std::array<int,3>,6> colors{{{227,25,46},{37,99,235},{22,163,74},
                                                 {147,51,234},{234,88,12},{8,145,178}}};
  uint32_t hash=0;for(unsigned char c:name)hash=hash*31+c;
  const auto& rgb=colors[hash%colors.size()];return Color(rgb[0],rgb[1],rgb[2],alpha);
}
}  // namespace detail

struct Layout {
  ImVec2 minimum{},maximum{};
  float scale=0,duration=0,alpha=0,progress=0;
  std::string name;
  std::vector<std::string> lines;
};
inline Layout Measure(ImFont* font,ImVec2 viewport,float age,const std::string& text,
                      const std::string& name={}) {
  Layout result;result.duration=Duration(name.empty()?text:name+" "+text);
  if(!font||!std::isfinite(viewport.x)||!std::isfinite(viewport.y)||
      viewport.x<=0||viewport.y<=0||!std::isfinite(age)||age<0||age>result.duration)return result;
  float s=std::clamp(viewport.y/1080.f,.7f,2.5f);
  // Fit the approved compact card without cropping its avatar or text column.
  if(460*s>viewport.x)s=viewport.x/460.f;
  result.scale=s;result.lines=detail::Wrap(font,20*s,270*s,text);
  result.name=detail::Ellipsize(font,30*s,270*s,name);
  const float width=404*s,height=std::max(108.f,84.f+24.f*float(result.lines.size()))*s;
  const float entrance=Smooth(age/.3f);
  result.alpha=entrance*(1-Smooth((age-(result.duration-.35f))/.35f));
  result.progress=std::max(0.f,1-age/result.duration);
  const float right=viewport.x-28*s+(1-entrance)*(width+40*s);
  result.minimum=ImVec2(right-width,28*s);result.maximum=ImVec2(right,28*s+height);
  return result;
}

inline void Draw(ImDrawList* draw,ImFont* font,ImVec2 viewport,float age,
                 const std::string& name,const std::string& text,ImTextureID photo=0,
                 ImVec2 uv0=ImVec2(0,0),ImVec2 uv1=ImVec2(1,1),
                 const char* title="FRIEND ONLINE") {
  if(!draw)return;
  const auto layout=Measure(font,viewport,age,text,name);
  const float s=layout.scale,alpha=layout.alpha;
  if(s<=0||alpha<=0)return;
  const auto a=layout.minimum,b=layout.maximum;
  draw->PushClipRect(ImVec2(0,0),viewport,true);
  const std::array<ImVec2,6> card{{a,ImVec2(b.x-20*s,a.y),ImVec2(b.x,a.y+20*s),
      ImVec2(b.x,b.y-20*s),ImVec2(b.x-20*s,b.y),ImVec2(a.x,b.y)}};
  auto shadow=card;for(auto& point:shadow){point.x+=4*s;point.y+=6*s;}
  draw->AddConvexPolyFilled(shadow.data(),int(shadow.size()),detail::Color(0,0,0,.28f*alpha));
  draw->AddConvexPolyFilled(card.data(),int(card.size()),detail::Color(12,14,18,.94f*alpha));
  draw->AddPolyline(card.data(),int(card.size()),detail::Color(190,196,208,.48f*alpha),ImDrawFlags_Closed,1.f);
  draw->AddRectFilled(a,ImVec2(a.x+4*s,b.y),detail::Color(255,106,19,alpha));
  const float radius=32*s;const ImVec2 center(a.x+54*s,a.y+49*s);
  draw->AddCircle(center,radius+3*s,detail::Color(190,196,208,.55f*alpha),48,1.f);
  if(photo) {
    draw->AddImageRounded(photo,ImVec2(center.x-radius,center.y-radius),
                          ImVec2(center.x+radius,center.y+radius),uv0,uv1,
                          detail::Color(255,255,255,alpha),radius);
  }else {
    draw->AddCircleFilled(center,radius,detail::AvatarColor(name,alpha),40);
    std::string initial=name.empty()?"?":name.substr(0,detail::Next(name,0));
    if(initial.size()==1&&initial[0]>='a'&&initial[0]<='z')initial[0]-='a'-'A';
    const auto size=font->CalcTextSizeA(radius*1.1f,FLT_MAX,0,initial.c_str());
    draw->AddText(font,radius*1.1f,ImVec2(center.x-size.x*.5f,center.y-size.y*.5f),
                  detail::Color(255,255,255,alpha),initial.c_str());
  }
  const ImVec2 dot(center.x+radius*.72f,center.y+radius*.72f);
  draw->AddCircleFilled(dot,radius*.24f+2*s,detail::Color(12,14,18,alpha),20);
  draw->AddCircleFilled(dot,radius*.24f,detail::Color(61,220,132,alpha),20);
  const float tx=a.x+110*s;
  draw->PushClipRect(ImVec2(tx,a.y),ImVec2(b.x-24*s,b.y-12*s),true);
  draw->AddText(font,14*s,ImVec2(tx,a.y+14*s),detail::Color(255,106,19,alpha),title);
  draw->AddText(font,30*s,ImVec2(tx,a.y+30*s),detail::Color(244,244,248,alpha),layout.name.c_str());
  for(size_t i=0;i<layout.lines.size();++i)
    draw->AddText(font,20*s,ImVec2(tx,a.y+(66+24*float(i))*s),
                  detail::Color(168,168,176,alpha),layout.lines[i].c_str());
  draw->PopClipRect();
  draw->AddRectFilled(ImVec2(a.x+16*s,b.y-7*s),ImVec2(b.x-32*s,b.y-5*s),
                      detail::Color(190,196,208,.18f*alpha));
  draw->AddRectFilled(ImVec2(a.x+16*s,b.y-7*s),
                      ImVec2(a.x+16*s+(b.x-a.x-48*s)*layout.progress,b.y-5*s),
                      detail::Color(255,106,19,alpha));
  draw->PopClipRect();
}
}  // namespace motogp::nobk::friend_toast
