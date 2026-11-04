#include "leaderboard_display.h"
#include "../leaderboard_scope_capture.h"
#include "leaderboard_filter.h"
#include <map>
#include <rex/cvar.h>
#include <rex/hook.h>
#include <rex/memory.h>
#include <rex/runtime.h>
#include <atomic>
#include <cstring>
#include <stdexcept>
#include <string>

REX_EXTERN(__imp__sub_8223E558);
REX_EXTERN(__imp__sub_8223E630);
REX_EXTERN(__imp__sub_8223CEB8);
REX_EXTERN(__imp__sub_8223CF40);
REX_EXTERN(__imp__sub_8223F340);

namespace motogp::online::lap_times {
namespace {
thread_local bool drawing=false;
std::atomic<uint32_t> label_bank{0};
uint32_t LabelAddress(uint32_t mode){
 const auto label=ModeLabel(mode);
 const auto bank=label_bank.load(std::memory_order_acquire);
 if(label==Label::Native || !bank)return 0;
 const unsigned language=rex::cvar::Query<std::string>("game_text_language")=="turkish"?1:0;
 uint32_t offset=0;
 for(unsigned lang=0;lang<2;++lang)for(size_t i=0;i<kLabels[lang].size();++i){
  if(lang==language && i==static_cast<size_t>(label))return bank+offset;
  offset+=static_cast<uint32_t>(kLabels[lang][i].size()+1);
 }
 return 0;
}
}
void Initialize(rex::Runtime* runtime){
 uint32_t size=0;
 for(const auto& language:kLabels)for(auto text:language)size+=static_cast<uint32_t>(text.size()+1);
 const auto address=runtime->memory()->SystemHeapAlloc(size);
 if(!address)throw std::runtime_error("Could not allocate Lap Times mode labels");
 auto* output=runtime->virtual_membase()+address;
 for(const auto& language:kLabels)for(auto text:language){
  std::memcpy(output,text.data(),text.size());output[text.size()]=0;output+=text.size()+1;
 }
 label_bank.store(address,std::memory_order_release);
}
}

REX_HOOK_RAW(sub_8223CF40){
 const motogp::online::categories::DrawScope header(
  motogp::online::scope_capture::IsStandaloneBoard(base,ctx.r3.u32)?
   motogp::online::categories::Drawing::Board:motogp::online::categories::Drawing::None);
 // Base leaderboard draw: +24 selects Lap Times (0), Arcade (1),
 // All Time Ghost (2) or Monthly Ghost (3). All timed rows share the getter.
 // Scope the override to this call, so another board/thread cannot inherit it.
 const bool previous=motogp::online::lap_times::drawing;
 const auto category=ctx.r3.u32?rex::memory::load_and_swap<uint32_t>(base+ctx.r3.u32+24):UINT32_MAX;
 motogp::online::lap_times::drawing=category==0||category==2||category==3;
 struct Restore {bool value;~Restore(){motogp::online::lap_times::drawing=value;}} restore{previous};
 __imp__sub_8223CF40(ctx,base);
}
REX_HOOK_RAW(sub_8223F340){
 // This call site draws the row's mode text, after its native column setup.
 if(motogp::online::lap_times::drawing && ctx.lr==0x8223DD14 && ctx.r3.u32){
  const auto mode=rex::memory::load_and_swap<uint32_t>(base+ctx.r3.u32+124);
  if(const auto label=motogp::online::lap_times::LabelAddress(mode)){
   ctx.r3.u64=label;return;
  }
 }
 __imp__sub_8223F340(ctx,base);
}

namespace motogp::online::lap_times {
namespace {
// Track menus own these board objects until their native destructor runs.
// The standalone Lap Times page never inherits a track-menu mode filter.
thread_local std::map<uint32_t,BoardMode> track_boards;
bool OnlineTrackCaller(uint32_t caller){
 if(rex::cvar::Query<std::string>("network_mode")!="online")return false;
 return (caller>=0x82208CE8 && caller<0x822094A0) ||
        (caller>=0x82234680 && caller<0x82234B78);
}
BoardMode TrackMode(uint32_t board,uint32_t caller,uint8_t* base){
 if(OnlineTrackCaller(caller)){
  const auto mode=FrontendBoardMode(rex::memory::load_and_swap<uint32_t>(base+0x828DA060));
  track_boards[board]=mode;
  return mode;
 }
 const auto found=track_boards.find(board);
 return found==track_boards.end()?BoardMode::Any:found->second;
}
}
}
REX_HOOK_RAW(sub_8223E558){
 // Time Trial launches an additional, unconsumed copy of its initial query.
 // Both native readers reset the same completion state and output buffer.
 // Keep one request alive until the menu consumes it; Offline/LAN stay native.
 if(ctx.lr==0x82234790 && rex::cvar::Query<std::string>("network_mode")=="online")return;
 const motogp::online::BoardModeScope scope(motogp::online::lap_times::TrackMode(ctx.r3.u32,ctx.lr,base));
 const motogp::online::scope_capture::ReadScope season(base,ctx.r3.u32);
 __imp__sub_8223E558(ctx,base);
}
REX_HOOK_RAW(sub_8223E630){
 const motogp::online::BoardModeScope scope(motogp::online::lap_times::TrackMode(ctx.r3.u32,ctx.lr,base));
 const motogp::online::scope_capture::ReadScope season(base,ctx.r3.u32);
 __imp__sub_8223E630(ctx,base);
}
REX_HOOK_RAW(sub_8223CEB8){
 motogp::online::lap_times::track_boards.erase(ctx.r3.u32);
 __imp__sub_8223CEB8(ctx,base);
}
