#include "leaderboard_categories.h"
#include <rex/cvar.h>
#include <rex/hook.h>
#include <rex/memory.h>
#include <array>

REX_EXTERN(__imp__sub_82240A90);
REX_EXTERN(__imp__sub_82240CD8);
REX_EXTERN(__imp__sub_822410B0);
REX_EXTERN(__imp__sub_82240F00);
REX_EXTERN(__imp__sub_822416F8);
REX_EXTERN(__imp__sub_82241928);
REX_EXTERN(__imp__sub_821F3518);
REX_EXTERN(sub_821F2FE0);
REX_EXTERN(__imp__sub_826EC198);
REX_EXTERN(sub_8235ACB0);
namespace {
bool Online(){return rex::cvar::Query<std::string>("network_mode")=="online";}
void AnnounceSeason(const PPCContext& context,uint8_t* base,uint32_t row) {
  // Match the Time Trial season callback 821F73F8, including the native
  // random choice between the two 2010 takes. Use a separate call context
  // so audio cannot overwrite the pending menu transition's registers.
  PPCContext call=context;
  if(row==0) {
    call.r3.u64=374;call.r4.u64=1;call.r5.u64=0;call.r6.u64=1;
    __imp__sub_821F3518(call,base);
  } else if(row==1) {
    // The native audio helper consumes the guest cue array synchronously.
    // Reserve a 16-byte-aligned guest frame with the array above its ABI
    // parameter area, rather than passing a host pointer or caller locals.
    call.r1.u32-=128;
    rex::memory::store_and_swap<uint32_t>(base+call.r1.u32,context.r1.u32);
    const auto cues=call.r1.u32+80;
    rex::memory::store_and_swap<uint32_t>(base+cues,375);
    rex::memory::store_and_swap<uint32_t>(base+cues+4,376);
    call.r3.u64=cues;call.r4.u64=2;call.r5.u64=1;call.r6.u64=0;
    call.r7.u64=0;call.r8.u64=1;
    sub_821F2FE0(call,base);
  }
}
struct Word {
  uint8_t* pointer;uint32_t saved;
  Word(uint8_t* base,uint32_t address,uint32_t value):pointer(base+address),saved(rex::memory::load_and_swap<uint32_t>(pointer)){
    rex::memory::store_and_swap<uint32_t>(pointer,value);
  }
  ~Word(){rex::memory::store_and_swap<uint32_t>(pointer,saved);}
};
}
REX_HOOK_RAW(sub_82240A90) {
  const auto model=ctx.r3.u32,screen=ctx.r4.u32;
  __imp__sub_82240A90(ctx,base);
  if(Online())motogp::online::categories::Begin(base,model,screen);
}
REX_HOOK_RAW(sub_82240CD8) {
  using namespace motogp::online;
  const auto model=ctx.r3.u32;
  if(Online()&&categories::Action(model,0)!=UINT32_MAX) {
    const Word a(base,model+32,categories::Action(model,0)),b(base,model+36,categories::Action(model,1)),
        c(base,model+40,categories::Action(model,2));
    // Return the native result unchanged. The native controller must complete
    // its exit animation before the selection callback changes stages.
    __imp__sub_82240CD8(ctx,base);
    return;
  }
  __imp__sub_82240CD8(ctx,base);
}
REX_HOOK_RAW(sub_822410B0) {
  using namespace motogp::online;
  if(Online()) {
    const auto screen=ctx.r3.u32;
    const auto exit=categories::Complete(base,screen,ctx.r4.s32);
    if(exit.kind==categories::ExitKind::Ignore)return;
    if(exit.kind==categories::ExitKind::PushClasses) {
      AnnounceSeason(ctx,base,rex::memory::load_and_swap<uint32_t>(base+screen+24));
      // Push the same native selector as a NEW frame, preserving the season
      // parent on the engine's real menu stack. r4 is the result callback,
      // not a title: zero installs the native no-op (823996A0), swallowing
      // both selections and Back in the new frame.
      ctx.r3.u64=exit.model;ctx.r4.u64=0x822410B0;ctx.r5.u64=screen;ctx.r6.s64=-1;
      ctx.r7.u64=8;ctx.r8.u64=1;ctx.r9.u64=0;
      sub_8235ACB0(ctx,base);return;
    }
    if(exit.kind==categories::ExitKind::SelectClass) {
      // The original callback uses the selected row only for its announcement;
      // its actual class flags already came from the reordered input actions.
      // Keep the displayed index intact in the native parent-history payload.
      __imp__sub_822410B0(ctx,base);return;
    }
  }
  __imp__sub_822410B0(ctx,base);
}
REX_HOOK_RAW(sub_82240F00) {
  const motogp::online::categories::DrawScope draw(Online()?motogp::online::categories::Drawing::Classes:
      motogp::online::categories::Drawing::None);
  __imp__sub_82240F00(ctx,base);
}
REX_HOOK_RAW(sub_822416F8) {
  const motogp::online::categories::DrawScope draw(Online()?motogp::online::categories::Drawing::Categories:
      motogp::online::categories::Drawing::None);
  __imp__sub_822416F8(ctx,base);
}
REX_HOOK_RAW(sub_82241928) {
  if(Online()&&ctx.r4.s32==-2)motogp::online::categories::ReturnToClasses();
  __imp__sub_82241928(ctx,base);
}
REX_HOOK_RAW(sub_821F3518) {
  // 821E41F8/821F7540/821F8E10 use cue 285 for the native Moto2 selection.
  // Correct the reordered rows only at this leaderboard announcement site.
  if(Online()&&ctx.lr==0x8224112C) {
    const auto scope=motogp::online::categories::Current();
    if(scope)switch(scope->category) {
      case motogp::online::categories::Category::MotoGP:ctx.r3.u64=687;break;
      case motogp::online::categories::Category::Moto2:ctx.r3.u64=285;break;
      case motogp::online::categories::Category::Cc250:ctx.r3.u64=283;break;
      case motogp::online::categories::Category::Cc125:ctx.r3.u64=282;break;
    }
  }
  __imp__sub_821F3518(ctx,base);
}
REX_HOOK_RAW(sub_826EC198) {
  // 822417F8..8224182C builds "%s / %s" from separate 250cc and Moto2
  // handles. Override this one formatting site, not the shared language bank.
  if(Online()&&ctx.lr==0x8224182C) {
    if(const auto label=motogp::online::categories::CategoryHeading()) {
      ctx.r4.u64=motogp::online::categories::StringFormat();ctx.r5.u64=label;
    }
  }
  __imp__sub_826EC198(ctx,base);
}
