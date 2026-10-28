#include "leaderboard_scope_capture.h"
#include <rex/hook.h>
REX_EXTERN(__imp__sub_8223C9D8);
REX_EXTERN(__imp__sub_822FEE30);
REX_HOOK_RAW(sub_8223C9D8) {
  const motogp::online::scope_capture::NativeSetupScope setup(base,ctx.r3.u32);
  const motogp::online::scope_capture::ReadScope read(base,ctx.r3.u32);
  __imp__sub_8223C9D8(ctx,base);
}
REX_HOOK_RAW(sub_822FEE30) {
  if(ctx.lr==0x8223CADC) {
    if(const auto season=motogp::online::scope_capture::SetupSeason(ctx.r5.u32))ctx.r7.u64=*season;
  }
  __imp__sub_822FEE30(ctx,base);
}
