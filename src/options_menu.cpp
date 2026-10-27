#include "options_menu.h"
#include "gauge_hud.h"
#include <rex/hook.h>
#include <rex/memory.h>
#include <rex/runtime.h>
#include <rex/cvar.h>
#include <rex/logging.h>
#include <array>
#include <atomic>
#include <cstring>
#include <stdexcept>
#include <string>
REX_EXTERN(__imp__sub_8223B4E0);
REX_EXTERN(__imp__sub_8223B950);
REX_EXTERN(__imp__sub_8223BA68);
REX_EXTERN(__imp__sub_8223A5C8);
REX_EXTERN(__imp__sub_822390B0);
REX_EXTERN(__imp__sub_82239BC0);
REX_EXTERN(__imp__sub_82238E10);
REX_EXTERN(__imp__sub_82239178);
REX_EXTERN(__imp__sub_8223B120);
REX_EXTERN(__imp__sub_8223A688);
REX_EXTERN(__imp__sub_8223AEE8);
REX_EXTERN(__imp__sub_822483E0);
REX_EXTERN(__imp__sub_82248630);
REX_EXTERN(__imp__sub_82248840);
REX_EXTERN(__imp__sub_8224DF60);
REX_EXTERN(__imp__sub_8224EAA0);
REX_EXTERN(__imp__sub_8224CFB8);
REX_EXTERN(__imp__sub_8224D130);
REX_EXTERN(__imp__sub_826EDB38);
namespace motogp::options {
namespace {
std::atomic<uint32_t> online_label{0};
std::atomic<uint32_t> profile_label{0};
std::atomic<uint32_t> splitscreen_warning{0};
std::atomic<uint32_t> gauge_storage{0};
constexpr uint32_t kGaugeChoiceVtable=0x8201EEC8;
constexpr uint32_t kGaugeBytes=352,kGaugeIndex=36,kGaugeTable=52;
constexpr uint32_t kGaugeLabel=128,kGaugeOriginal=160,kGaugeMotoGP17=192,kGaugeMotoGP06=224,kGaugeURT2=256,kGaugeURT3=288,kGaugeStyleIds=320;
static_assert(kGaugeTable+12*(gauge::kStyleCount+1)<=kGaugeLabel);
static_assert(kGaugeStyleIds+4*gauge::kStyleCount<=kGaugeBytes);
uint32_t Read(uint8_t* base,uint32_t at){return rex::memory::load_and_swap<uint32_t>(base+at);}
void Write(uint8_t* base,uint32_t at,uint32_t value){rex::memory::store_and_swap<uint32_t>(base+at,value);}
bool IsMatchSettings(uint8_t* base,uint32_t menu){
  if(!menu)return false;const auto type=Read(base,menu);
  return type==kCreateVtable||type==kCustomVtable||type==kOnlineCreateVtable;
}
// Keep native action IDs, compact only their display-order array. Both native
// navigation and accept handlers continue resolving the remaining original IDs.
bool Filter(uint8_t* base,uint32_t menu,uint32_t screen,unsigned capacity,unsigned max_id,uint32_t removed,bool update_count=false){
  std::array<uint32_t,12> before{},after{};unsigned count=0,kept=0;uint32_t seen=0;
  for(;count<capacity;++count){const auto id=Read(base,menu+32+4*count);if(id==UINT32_MAX)break;
    if(id>max_id||(seen&(1u<<id)))return false;seen|=1u<<id;before[count]=id;if(id!=removed)after[kept++]=id;}
  if(!count||count==capacity||!kept)return false;
  const auto selected=screen?Read(base,screen+24):UINT32_MAX;
  for(unsigned i=0;i<kept;++i)Write(base,menu+32+4*i,after[i]);Write(base,menu+32+4*kept,UINT32_MAX);
  if(update_count)Write(base,menu+24,kept+1);
  if(screen&&selected!=UINT32_MAX){unsigned next=selected<kept?selected:kept-1;
    if(selected<count)for(unsigned i=0;i<kept;++i)if(after[i]==before[selected]){next=i;break;}
    Write(base,screen+24,next);}
  return true;
}
bool GaugeReady(){return gauge_storage.load(std::memory_order_acquire)&&gauge::Available();}
void RefreshGaugeChoices(uint8_t* base,uint32_t storage){
  unsigned count=0,selected=0;bool changed=false;
  for(int style=0;style<gauge::kStyleCount;++style){
    if(style&&!gauge::StyleAvailable(style))continue;
    changed|=Read(base,storage+kGaugeStyleIds+4*count)!=uint32_t(style);
    Write(base,storage+kGaugeStyleIds+4*count,uint32_t(style));
    const auto record=storage+kGaugeTable+12*count;
    Write(base,record,UINT32_MAX);Write(base,record+8,0);
    Write(base,record+4,storage+(style==1?kGaugeMotoGP17:style==2?kGaugeMotoGP06:style==3?kGaugeURT2:style==4?kGaugeURT3:kGaugeOriginal));
    if(style==gauge::SelectedStyle())selected=count;
    ++count;
  }
  // Native decrement probes record[-1]; both end sentinels must be readable.
  for(const auto record:{storage+kGaugeTable-12,storage+kGaugeTable+12*count}){
    Write(base,record,UINT32_MAX);Write(base,record+4,0);Write(base,record+8,0);
  }
  changed|=Read(base,storage+24)!=count;
  Write(base,storage+24,count);Write(base,storage+kGaugeIndex,selected);
  if(changed)Write(base,storage+32,UINT32_MAX);
}
void InitializeGauge(rex::Runtime* runtime,bool tr){
  const auto storage=runtime->memory()->SystemHeapAlloc(kGaugeBytes);
  if(!storage){REXLOG_WARN("Gauge style menu allocation failed; retaining the original Display Settings rows");return;}
  auto* base=runtime->virtual_membase();std::memset(base+storage,0,kGaugeBytes);
  const char* label=tr?"Gösterge stili":"Gauge Style";
  const char* original=tr?"Orijinal":"Original";
  std::memcpy(base+storage+kGaugeLabel,label,std::strlen(label)+1);
  std::memcpy(base+storage+kGaugeOriginal,original,std::strlen(original)+1);
  std::memcpy(base+storage+kGaugeMotoGP17,"MotoGP 17",10);
  std::memcpy(base+storage+kGaugeMotoGP06,"MotoGP 06",10);
  std::memcpy(base+storage+kGaugeURT2,"MotoGP URT 2",13);
  std::memcpy(base+storage+kGaugeURT3,"MotoGP URT 3",13);
  // This is the native 36-byte choice object initialized by 82219450.
  // Its label handle is intercepted by exact address below; original banks
  // and profile schemas are never modified.
  Write(base,storage,kGaugeChoiceVtable);
  Write(base,storage+16,storage+kGaugeIndex);
  Write(base,storage+20,storage+kGaugeTable);
  Write(base,storage+32,UINT32_MAX);
  RefreshGaugeChoices(base,storage);
  gauge_storage.store(storage,std::memory_order_release);
}
void ClearGaugeRow(uint8_t* base,uint32_t menu){
  const auto storage=gauge_storage.load(std::memory_order_acquire);
  if(!storage)return;
  for(unsigned i=0;i<8;++i){
    const auto row=menu+68+40*i;
    if(Read(base,row+28)==storage){Write(base,row+16,0);Write(base,row+28,0);}
  }
}
bool AppendGauge(uint8_t* base,uint32_t menu){
  const auto storage=gauge_storage.load(std::memory_order_acquire);
  unsigned count=0;int existing=-1;
  for(;count<9;++count){
    const auto id=Read(base,menu+32+4*count);
    if(id==UINT32_MAX)break;
    if(id==kGaugeAction)existing=int(count);
    else if(id>7)return false;
  }
  if(!count||count>=9||(existing>=0&&unsigned(existing)+1!=count))return false;
  const unsigned position=existing>=0?unsigned(existing):count;
  if(position>=8)return false; // Eight settings + terminator fit nine native rows.
  const auto row=menu+68+40*position;
  if(Read(base,row+16)!=4||Read(base,row+28)!=storage){
    // Match 821FF608 and the title's selector row initialization. Retain
    // native focus animation fields after the row has been initialized.
    std::memset(base+row,0,40);
    Write(base,row+4,1);Write(base,row+16,4);
    Write(base,row+20,Read(base,0x8200084C));
    Write(base,row+24,Read(base,0x820024F0));
    Write(base,row+28,storage);
  }
  RefreshGaugeChoices(base,storage);
  if(existing<0){
    Write(base,menu+32+4*count,kGaugeAction);
    Write(base,menu+32+4*(count+1),UINT32_MAX);
    ++count;
  }
  // The row after the choice remains a proper native navigation terminator.
  Write(base,menu+68+40*count+16,0);Write(base,menu+68+40*count+28,0);
  Write(base,menu+24,count+1);
  return true;
}
}
void Initialize(rex::Runtime* runtime){
  const auto address=runtime->memory()->SystemHeapAlloc(8);if(!address)throw std::runtime_error("Could not allocate the PC Online label");
  std::memcpy(runtime->virtual_membase()+address,"ONLINE",7);online_label.store(address,std::memory_order_release);
  const bool tr=rex::cvar::Query<std::string>("game_text_language")=="turkish";
  const char* profile=tr?"Profili incele":"View profile";
  const auto profile_address=runtime->memory()->SystemHeapAlloc(32);
  if(!profile_address)throw std::runtime_error("Could not allocate the profile action label");
  std::memcpy(runtime->virtual_membase()+profile_address,profile,std::strlen(profile)+1);
  profile_label.store(profile_address,std::memory_order_release);
  const char* split=tr?
      "Bölünmüş ekran için iki kontrolcü bağlayın veya bir kontrolcü bağlayıp launcher'dan klavye kontrolünü etkinleştirin.":
      "Split-screen requires two controllers, or one controller with keyboard controls enabled in the launcher.";
  const auto split_bytes=std::strlen(split)+1;
  const auto split_address=runtime->memory()->SystemHeapAlloc(split_bytes);
  if(!split_address)throw std::runtime_error("Could not allocate the split-screen warning");
  std::memcpy(runtime->virtual_membase()+split_address,split,split_bytes);
  splitscreen_warning.store(split_address,std::memory_order_release);
  InitializeGauge(runtime,tr);
}
uint32_t OnlineLabel(){return online_label.load(std::memory_order_acquire);}
uint32_t GaugeLabel(uint32_t handle){
  const auto storage=gauge_storage.load(std::memory_order_acquire);
  return storage&&handle==storage+4?storage+kGaugeLabel:0;
}
void CommitGaugeChoice(uint8_t* base,uint32_t menu){
  if(!menu||Read(base,menu)!=kDisplaySettingsVtable||!GaugeReady())return;
  const auto storage=gauge_storage.load(std::memory_order_acquire);
  bool present=false;
  for(unsigned i=0;i<8;++i){
    const auto id=Read(base,menu+32+4*i);if(id==UINT32_MAX)break;
    if(id==kGaugeAction&&Read(base,menu+68+40*i+28)==storage){present=true;break;}
  }
  if(!present)return;
  const auto value=Read(base,storage+kGaugeIndex);
  if(value<Read(base,storage+24)&&value<unsigned(gauge::kStyleCount)){
    const auto style=Read(base,storage+kGaugeStyleIds+4*value);
    if(style<unsigned(gauge::kStyleCount)&&(!style||gauge::StyleAvailable(int(style))))gauge::SelectStyle(int(style));
  }
}
bool PrepareDisplayGauge(uint8_t* base,uint32_t menu,uint32_t screen){
  if(!menu||Read(base,menu)!=kDisplaySettingsVtable)return false;
  if(GaugeReady())return AppendGauge(base,menu);
  ClearGaugeRow(base,menu);
  return Filter(base,menu,screen,9,8,kGaugeAction,true);
}
bool RemoveStorage(uint8_t* base,uint32_t menu,uint32_t screen){return menu&&Read(base,menu)==kGameSettingsVtable&&Filter(base,menu,screen,7,5,5,true);}
bool RemoveParty(uint8_t* base,uint32_t menu,uint32_t screen){return menu&&Read(base,menu)==kMultiplayerVtable&&Filter(base,menu,screen,5,3,1);}
bool RemovePremium(uint8_t* base,uint32_t menu,uint32_t screen){return IsMatchSettings(base,menu)&&Filter(base,menu,screen,12,10,7);}
bool RemovePrivateSlots(uint8_t* base,uint32_t menu,uint32_t screen){
  if(!IsMatchSettings(base,menu)||!Filter(base,menu,screen,12,10,8))return false;
  // Native commit 8224E428 copies +136 to private capacity and sets public
  // capacity to 20 minus it. +132 is a different selector; leave it intact.
  Write(base,menu+136,0);return true;
}
void MatchOptions(uint8_t* base,uint32_t menu,uint32_t screen){RemovePremium(base,menu,screen);RemovePrivateSlots(base,menu,screen);}
bool RemoveMarketplace(uint8_t* base,uint32_t menu,uint32_t screen){
  const auto read=[&](uint32_t at){return rex::memory::load_and_swap<uint32_t>(base+at);};
  const auto write=[&](uint32_t at,uint32_t value){rex::memory::store_and_swap<uint32_t>(base+at,value);};
  if(!menu||read(menu)!=kVtable)return false;
  for(unsigned i=0;i<5;++i)if(read(menu+32+4*i)!=i)return false;
  if(read(menu+56)!=UINT32_MAX||(read(menu+52)!=5&&read(menu+52)!=UINT32_MAX))return false;
  write(menu+52,UINT32_MAX);
  if(screen&&read(screen+24)==5)write(screen+24,4);
  return true;
}
}
REX_HOOK_RAW(sub_8223B4E0){motogp::options::RemoveMarketplace(base,ctx.r3.u32,ctx.r4.u32);__imp__sub_8223B4E0(ctx,base);}
REX_HOOK_RAW(sub_8223B950){motogp::options::RemoveMarketplace(base,ctx.r3.u32,ctx.r4.u32);__imp__sub_8223B950(ctx,base);}
REX_HOOK_RAW(sub_8223BA68){motogp::options::RemoveMarketplace(base,ctx.r3.u32,ctx.r4.u32);__imp__sub_8223BA68(ctx,base);}
REX_HOOK_RAW(sub_822390B0){const auto menu=ctx.r3.u32;__imp__sub_822390B0(ctx,base);motogp::options::PrepareDisplayGauge(base,menu,0);}
REX_HOOK_RAW(sub_82239BC0){motogp::options::PrepareDisplayGauge(base,ctx.r3.u32,0);__imp__sub_82239BC0(ctx,base);}
REX_HOOK_RAW(sub_82238E10){const auto menu=ctx.r3.u32;motogp::options::PrepareDisplayGauge(base,menu,ctx.r4.u32);__imp__sub_82238E10(ctx,base);motogp::options::CommitGaugeChoice(base,menu);}
REX_HOOK_RAW(sub_82239178){motogp::options::PrepareDisplayGauge(base,ctx.r3.u32,ctx.r4.u32);__imp__sub_82239178(ctx,base);}
REX_HOOK_RAW(sub_8223A5C8){const auto menu=ctx.r3.u32;__imp__sub_8223A5C8(ctx,base);motogp::options::RemoveStorage(base,menu,0);}
REX_HOOK_RAW(sub_8223B120){motogp::options::RemoveStorage(base,ctx.r3.u32,ctx.r4.u32);__imp__sub_8223B120(ctx,base);}
REX_HOOK_RAW(sub_8223A688){motogp::options::RemoveStorage(base,ctx.r3.u32,ctx.r4.u32);__imp__sub_8223A688(ctx,base);}
REX_HOOK_RAW(sub_8223AEE8){motogp::options::RemoveStorage(base,ctx.r3.u32,ctx.r4.u32);__imp__sub_8223AEE8(ctx,base);}
REX_HOOK_RAW(sub_822483E0){motogp::options::RemoveParty(base,ctx.r3.u32,ctx.r4.u32);__imp__sub_822483E0(ctx,base);}
REX_HOOK_RAW(sub_82248630){motogp::options::RemoveParty(base,ctx.r3.u32,ctx.r4.u32);__imp__sub_82248630(ctx,base);}
REX_HOOK_RAW(sub_82248840){motogp::options::RemoveParty(base,ctx.r3.u32,ctx.r4.u32);__imp__sub_82248840(ctx,base);}
REX_HOOK_RAW(sub_8224DF60){motogp::options::MatchOptions(base,ctx.r3.u32,ctx.r4.u32);__imp__sub_8224DF60(ctx,base);}
REX_HOOK_RAW(sub_8224EAA0){motogp::options::MatchOptions(base,ctx.r3.u32,ctx.r4.u32);__imp__sub_8224EAA0(ctx,base);}
REX_HOOK_RAW(sub_8224CFB8){motogp::options::MatchOptions(base,ctx.r3.u32,ctx.r4.u32);__imp__sub_8224CFB8(ctx,base);}
REX_HOOK_RAW(sub_8224D130){const auto menu=ctx.r3.u32;__imp__sub_8224D130(ctx,base);
  if(motogp::options::IsMatchSettings(base,menu)){
    motogp::options::Write(base,menu+128,0); // Off for hosting, Any for search.
    motogp::options::Write(base,menu+136,0); // All player capacity is public.
  }
}
REX_HOOK_RAW(sub_826EDB38){const auto label=motogp::options::OnlineLabel();
  if(label&&ctx.r3.u32==motogp::options::kOnlineHandle&&motogp::options::Read(base,ctx.r3.u32)==731){ctx.r3.u64=label;return;}
  if(const auto gauge=motogp::options::GaugeLabel(ctx.r3.u32)){ctx.r3.u64=gauge;return;}
  // Bank 0's two controller-warning variants share the PC split-screen rules.
  // Only replace their text; leave native eligibility and dialog input intact.
  const auto split=motogp::options::splitscreen_warning.load(std::memory_order_acquire);
  if(split&&ctx.r3.u32){
    const auto id=motogp::options::Read(base,ctx.r3.u32);
    if(id==2065||id==2066){ctx.r3.u64=split;return;}
  }
  // The native label getter receives a handle containing a bank/index pair.
  // Replace both Gamer Card labels and the lobby help text without changing
  // action IDs, original message banks or the active language's other strings.
  const auto profile=motogp::options::profile_label.load(std::memory_order_acquire);
  if(profile&&ctx.r3.u32){
    const auto id=motogp::options::Read(base,ctx.r3.u32);
    if(id==1137||id==1433||id==1416){ctx.r3.u64=profile;return;}
  }
  __imp__sub_826EDB38(ctx,base);
}
