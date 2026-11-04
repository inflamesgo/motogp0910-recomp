#pragma once
#include <compare>
#include <cstdint>
#include <optional>

namespace rex {class Runtime;}
namespace motogp::online::categories {
enum class Category : uint8_t { MotoGP=1, Moto2=2, Cc250=3, Cc125=4 };
struct Scope {
  Category category=Category::MotoGP;
  uint32_t year=2009;
  auto operator<=>(const Scope&)const=default;
};
constexpr bool Valid(Scope s) {
  return (s.year==2009||s.year==2010)&&s.category>=Category::MotoGP&&s.category<=Category::Cc125&&
      !(s.category==Category::Moto2&&s.year!=2010)&&!(s.category==Category::Cc250&&s.year!=2009);
}
constexpr uint32_t NativeSlot(Category c) {
  return c==Category::MotoGP?2:c==Category::Cc125?0:1;
}
constexpr std::optional<Scope> ForView(uint32_t view,uint32_t season) {
  if(!view||view>378||season>1)return {};
  const auto slot=((view-1)%63)/21;
  return Scope{slot==2?Category::MotoGP:slot==0?Category::Cc125:
      season?Category::Moto2:Category::Cc250,2009+season};
}
constexpr bool Matches(Scope s,uint32_t view) {
  return Valid(s)&&view&&view<=378&&((view-1)%63)/21==NativeSlot(s.category);
}
void Initialize(rex::Runtime*,bool turkish);
void Clear();
std::optional<Scope> Current();
void ObserveScreen(uint8_t* base,uint32_t model);
bool Begin(uint8_t* base,uint32_t model,uint32_t screen);
uint32_t Action(uint32_t model,uint32_t row);
enum class ExitKind { Native, PushClasses, SelectClass, Ignore };
struct Exit {ExitKind kind=ExitKind::Native;uint32_t model=0;};
Exit Complete(uint8_t* base,uint32_t screen,int32_t result);
void ReturnToClasses();
enum class Drawing { None, Classes, Categories, Board };
class DrawScope {
  Drawing previous_;
 public:
  explicit DrawScope(Drawing);
  ~DrawScope();
};
uint32_t Label(uint32_t handle);
uint32_t ResolvedLabel(const uint8_t* base,uint32_t text);
uint32_t CategoryHeading();
uint32_t StringFormat();
}
