#pragma once
#include <rex/ppc/context.h>

namespace motogp::bikes {
inline constexpr uint32_t kCapacity=112;
inline constexpr uint32_t kRecordBytes=44;
// Native 8229FF00's used-bike bytes begin at stack+80; its next buffer is +192.
static_assert(80+kCapacity<=192);
}

void MotogpBikeAllocation(PPCRegister& size);
void MotogpBikeCount(PPCRegister& count);
void MotogpBikeCompare(PPCRegister& count,PPCCRRegister& result,PPCXERRegister& xer);
