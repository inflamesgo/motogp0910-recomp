#include "leaderboard_categories.h"
#include <rex/memory.h>
#include <rex/runtime.h>
#include <array>
#include <atomic>
#include <cstring>
#include <stdexcept>

namespace motogp::online::categories {
namespace {
constexpr uint32_t kClassModel=0x8203639C,kMain=0x8203A0C4,kMultiplayer=0x8200CF7C;
constexpr uint32_t kNativeClassHeading=0x8288C704;
constexpr uint32_t kStride=24,kLabelBytes=9*kStride;
enum LabelIndex {Season2009,Season2010,MotoGP,Moto2,Cc250,Cc125,SeasonTitle,ClassTitle,Format};
enum class Stage { Seasons, Classes };
struct Session {
  bool active=false;
  Stage stage=Stage::Seasons;
  uint32_t year=2009,model=0,screen=0,last_class=0;
  std::array<uint8_t,40> row{};
};
thread_local Session session;
thread_local Drawing drawing=Drawing::None;
std::atomic<uint32_t> labels{0},selected_scope{0};
uint32_t Read(const uint8_t* base,uint32_t p){return rex::memory::load_and_swap<uint32_t>(base+p);}
void Write(uint8_t* base,uint32_t p,uint32_t v){rex::memory::store_and_swap<uint32_t>(base+p,v);}
uint32_t Address(LabelIndex i){return labels.load(std::memory_order_acquire)+uint32_t(i)*kStride;}
void Rows(uint8_t* base,uint32_t screen) {
  const bool classes=session.stage==Stage::Classes;
  const uint32_t count=classes?3:2;
  const std::array<uint32_t,3> text=classes?
      std::array{Address(MotoGP),Address(session.year==2010?Moto2:Cc250),Address(Cc125)}:
      std::array{Address(Season2009),Address(Season2010),0u};
  // Four native records include the disabled end sentinel. Preserve the native
  // 40-byte record layout; never enlarge the guest allocation.
  for(uint32_t i=0;i<4;++i) {
    const auto row=session.model+48+i*40;
    std::memcpy(base+row,session.row.data(),session.row.size());
    Write(base,row+16,i<count?1:0);
    Write(base,row+20,0);Write(base,row+24,0x3F800000);
    Write(base,row+28,i<count?text[i]:0);Write(base,row+32,0);base[row+36]=0;
  }
  Write(base,session.model+24,count);
  if(screen) {
    Write(base,screen+24,classes?session.last_class:session.year-2009);
    Write(base,screen+48,0); // The new native frame owns its entrance animation.
  }
}
}
void Initialize(rex::Runtime* runtime,bool turkish) {
  const auto bank=runtime->memory()->SystemHeapAlloc(kLabelBytes);
  if(!bank)throw std::runtime_error("Could not allocate leaderboard category labels");
  std::memset(runtime->virtual_membase()+bank,0,kLabelBytes);
  const std::array<const char*,9> names{turkish?"2009 SEZONU":"2009 SEASON",turkish?"2010 SEZONU":"2010 SEASON",
      "MotoGP","Moto2","250cc","125cc",turkish?"SEZON":"SEASON",turkish?"SINIF":"CLASS","%s"};
  for(size_t i=0;i<names.size();++i)
    std::memcpy(runtime->virtual_membase()+bank+i*kStride,names[i],std::strlen(names[i]));
  labels.store(bank,std::memory_order_release);Clear();
}
void Clear(){session={};selected_scope.store(0,std::memory_order_release);}
std::optional<Scope> Current() {
  const auto value=selected_scope.load(std::memory_order_acquire);
  const Scope scope{Category(value&0xFF),value>>8};
  return Valid(scope)?std::optional(scope):std::nullopt;
}
void ObserveScreen(uint8_t* base,uint32_t model) {
  if(model&&(Read(base,model)==kMain||Read(base,model)==kMultiplayer))Clear();
}
bool Begin(uint8_t* base,uint32_t model,uint32_t screen) {
  if(!model||!screen||!labels.load(std::memory_order_acquire)||Read(base,model)!=kClassModel)return false;
  // A native Back operation reconstructs its parent frame. Keep the stage/year
  // rather than resetting to Seasons on every constructor invocation.
  if(!session.active){session={};session.active=true;selected_scope.store(0,std::memory_order_release);}
  session.model=model;session.screen=screen;
  std::memcpy(session.row.data(),base+model+48,session.row.size());Rows(base,screen);
  return true;
}
uint32_t Action(uint32_t model,uint32_t row) {
  if(!session.active||session.model!=model||row>2)return UINT32_MAX;
  return session.stage==Stage::Classes?2-row:row<2?row:UINT32_MAX;
}
Exit Complete(uint8_t* base,uint32_t screen,int32_t result) {
  if(!session.active||screen!=session.screen)return {};
  if(result==-2) {
    selected_scope.store(0,std::memory_order_release);
    if(session.stage==Stage::Classes)session.stage=Stage::Seasons;
    else Clear();
    return {}; // Native Back pops exactly one real frame.
  }
  if(result<0)return {};
  const auto row=Read(base,screen+24);
  if(session.stage==Stage::Seasons) {
    if(row>1)return {ExitKind::Ignore};
    session.year=2009+row;session.stage=Stage::Classes;session.last_class=0;
    selected_scope.store(0,std::memory_order_release);
    return {ExitKind::PushClasses,session.model};
  }
  if(row>2)return {ExitKind::Ignore};
  session.last_class=row;
  const auto category=row==0?Category::MotoGP:row==2?Category::Cc125:
      session.year==2010?Category::Moto2:Category::Cc250;
  selected_scope.store(uint32_t(category)|(session.year<<8),std::memory_order_release);
  return {ExitKind::SelectClass,session.model};
}
void ReturnToClasses() {
  if(session.active)session.stage=Stage::Classes;
  selected_scope.store(0,std::memory_order_release);
}
DrawScope::DrawScope(Drawing value):previous_(drawing){drawing=value;}
DrawScope::~DrawScope(){drawing=previous_;}
uint32_t Label(uint32_t handle) {
  if(!labels.load(std::memory_order_acquire))return 0;
  if(drawing==Drawing::Classes&&session.active&&handle==kNativeClassHeading)
    return Address(session.stage==Stage::Seasons?SeasonTitle:ClassTitle);
  return 0;
}
uint32_t ResolvedLabel(const uint8_t* base,uint32_t text) {
  if(!text||!labels.load(std::memory_order_acquire)||(drawing!=Drawing::Categories&&drawing!=Drawing::Board))return 0;
  const auto scope=Current();if(!scope)return 0;
  // The native middle-class heading is one shared string. Replace its resolved
  // text only inside these scoped draws, without changing the message banks.
  constexpr char combined[]="250cc / Moto2";
  if(std::memcmp(base+text,combined,sizeof(combined))!=0)return 0;
  return Address(scope->category==Category::Moto2?Moto2:scope->category==Category::Cc250?Cc250:
      scope->category==Category::MotoGP?MotoGP:Cc125);
}
uint32_t CategoryHeading() {
  if(!labels.load(std::memory_order_acquire)||drawing!=Drawing::Categories)return 0;
  const auto scope=Current();if(!scope)return 0;
  return Address(scope->category==Category::Moto2?Moto2:scope->category==Category::Cc250?Cc250:
      scope->category==Category::MotoGP?MotoGP:Cc125);
}
uint32_t StringFormat(){return labels.load(std::memory_order_acquire)?Address(Format):0;}
}
