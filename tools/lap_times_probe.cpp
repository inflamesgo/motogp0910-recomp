// CPU-only checks for the native Lap Times read ABI and category isolation.
// No network connection, game data, GPU or user profile is required.
#include "online/stats.h"
#include "online/leaderboard_display.h"
#include <rex/memory.h>
#include <algorithm>
#include <array>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {
using namespace motogp::online;
unsigned checks=0;
void Check(bool value,const char* message){if(!value)throw std::runtime_error(message);++checks;}
uint32_t U32(const uint8_t* p){return rex::memory::load_and_swap<uint32_t>(p);}
uint64_t U64(const uint8_t* p){return rex::memory::load_and_swap<uint64_t>(p);}
struct Encoded {
 std::vector<uint8_t> bytes;
 uint32_t row=0,columns=0;
 Encoded(const Spec& spec,const Row& source):bytes(64+ResultsSize({spec},1)+32,0xA5){
  WriteResults(bytes.data(),64,{spec},{{4,{source}}});
  const auto view=U32(bytes.data()+68);
  Check(view==72 && U32(bytes.data()+view)==spec.view,"Native view pointer and ID");
  row=U32(bytes.data()+view+12);columns=U32(bytes.data()+row+44);
  Check(row==88 && columns==row+48,"Native 48-byte row and column pointers");
  Check(U64(bytes.data()+row)==source.peer && U32(bytes.data()+row+8)==source.rank,"Player identity and rank preserved");
  Check(static_cast<int64_t>(U64(bytes.data()+row+16))==-630800,"Signed 64-bit primary rating preserved");
  Check(std::memcmp(bytes.data()+row+24,"inflames",8)==0,"Native player name preserved");
  const auto end=64+ResultsSize({spec},1);
  Check(std::all_of(bytes.begin(),bytes.begin()+64,[](auto v){return v==0xA5;}) &&
   std::all_of(bytes.begin()+end,bytes.end(),[](auto v){return v==0xA5;}),"Output remains inside caller's capacity");
 }
 const uint8_t* Column(uint16_t id)const{
  const auto count=U32(bytes.data()+row+40);
  for(uint32_t i=0;i<count;++i){const auto* c=bytes.data()+columns+24*i;if(rex::memory::load_and_swap<uint16_t>(c)==id)return c;}
  throw std::runtime_error("Requested native column missing");
 }
 uint32_t Int(uint16_t id)const{const auto* c=Column(id);Check(c[8]==1,"Display value is native int32");return U32(c+16);}
};
}
int main(){try{
 using namespace motogp::online;
 const Spec display{43,{3,1,2,4,5,6,7,0xFFFA}};
 const Row original{0x4D47500000000002ull,1,"inflames",{
  {3,2,static_cast<uint64_t>(int64_t(-630800))},{1,1,196610},
  {4,1,0},{2,1,6308},{5,1,0},{6,1,10},{7,1,0},{13,1,1},{0x8011,1,0}}};
 const Encoded lap(display,original);
 Check(lap.Int(3)==6308,"Losail 1:45.13 must reach the native formatter as 6308 ticks");
 Check(lap.Int(5)==10,"Time Trial uses stored property 6, not zero-valued property 5");
 Check(lap.Int(6)==0 && lap.Int(7)==1,"Track and 2010 season use their native read columns");
 Check(lap.Int(1)==196610 && lap.Int(0xFFFA)==1,"Bike/class and rank are unchanged");
 for(uint32_t view=1;view<=378;++view){
  auto spec=display;spec.view=view;auto record=original;
  for(auto& c:record.columns)if(c.id==7)c.bits=(view-1)%21;
  const Encoded encoded(spec,record);
  if(view<=63){
   Check(encoded.Int(3)==6308 && encoded.Int(5)==10,"Every Lap Times class/track projects the display fields");
   Check(encoded.Int(6)==(view-1)%21 && encoded.Int(7)==1,"Lap Times track and season metadata are correctly mapped");
  }else{
   const auto* rating=encoded.Column(3);
   Check(rating[8]==2 && static_cast<int64_t>(U64(rating+16))==-630800,"Arcade and ghost categories retain raw int64 column 3");
   Check(encoded.Int(5)==0 && encoded.Int(6)==10 && encoded.Int(7)==(view-1)%21,"Other categories retain their existing metadata");
  }
 }
 for(const auto columns:{std::vector<uint16_t>{3,2,0xFFFA},std::vector<uint16_t>{3,2,5,4}}){
  const Encoded internal({43,columns},original);
  Check(internal.Column(3)[8]==2 && static_cast<int64_t>(U64(internal.Column(3)+16))==-630800,"Internal best-rating reads remain raw");
 }
 auto reordered=display;std::reverse(reordered.columns.begin(),reordered.columns.end());
 Check(Encoded(reordered,original).Int(3)==6308,"Column order does not affect Lap Times projection");
 for(uint64_t value:{uint64_t(0),uint64_t(INT32_MAX),UINT64_MAX}){
  auto bad=original;for(auto& c:bad.columns)if(c.id==2)c.bits=value;
  Check(Encoded(display,bad).Int(3)==UINT32_MAX,"Absent/invalid lap times stay unavailable instead of becoming zero-time records");
 }
 auto missing=original;
 std::erase_if(missing.columns,[](const auto& c){return c.id==2 || c.id==6 || c.id==7 || c.id==13;});
 const Encoded absent(display,missing);
 Check(absent.Int(3)==UINT32_MAX && absent.Int(5)==UINT32_MAX,"Missing time and mode metadata are not invented");
 Check(absent.Int(6)==0 && absent.Int(7)==UINT32_MAX,"Missing track safely uses the view; missing season remains unspecified");
 for(uint64_t value:{uint64_t(21),UINT64_MAX}){
  auto bad=original;for(auto& c:bad.columns)if(c.id==7)c.bits=value;
  auto spec=display;spec.view=51;
  Check(Encoded(spec,bad).Int(6)==8,"Invalid track metadata cannot index outside the native track table");
 }
 auto wrong_type=original;
 for(auto& c:wrong_type.columns)if(c.id==2 || c.id==6)c.type=2;
 const Encoded typed(display,wrong_type);
 Check(typed.Int(3)==UINT32_MAX && typed.Int(5)==UINT32_MAX,"Incorrect property types do not leak int64 data into int32 UI fields");
 auto championship=original;
 for(auto& c:championship.columns)if(c.id==6)c.bits=0x10000005;
 Check(Encoded(display,championship).Int(5)==0x10000005,"Championship bit survives native int32 marshaling");
 using lap_times::Label;using lap_times::ModeLabel;
 constexpr std::array expected{Label::Unknown,Label::QuickRace,Label::Native,
  Label::Native,Label::Native,Label::Native,Label::Native,Label::Preview,
  Label::Training,Label::Tutorial,Label::Native,Label::SplitScreen,
  Label::OnlineSplitScreen,Label::Lan,Label::Online,Label::Demo,Label::Replay};
 for(uint32_t mode=0;mode<expected.size();++mode){
  Check(ModeLabel(mode)==expected[mode],"All 17 original mode IDs have the intended label policy");
  Check(ModeLabel(mode|lap_times::kChampionship)==Label::Native,"Championship keeps the original localized label for every mode");
 }
 Check(ModeLabel(UINT32_MAX)==Label::Unknown && ModeLabel(99)==Label::Unknown &&
  ModeLabel(99|lap_times::kChampionship)==Label::Unknown,"Unknown mode metadata is never labeled Arcade or Championship");
 std::cout<<"Passed "<<checks<<" Lap Times ABI, category isolation and mode-label checks\n";
 return 0;
}catch(const std::exception& error){std::cerr<<"FAIL after "<<checks<<": "<<error.what()<<'\n';return 1;}}
