#include "bike_capacity.h"

// Instruction hooks preserve the native allocator, row parser, IDs and physics.
// Change all four linked limits together; the complete supported catalog has
// 102 unique bikes, while the original title stopped appending at 100.
void MotogpBikeAllocation(PPCRegister& size){
  size.u64=motogp::bikes::kCapacity*motogp::bikes::kRecordBytes;
}
void MotogpBikeCount(PPCRegister& count){count.u64=motogp::bikes::kCapacity;}
void MotogpBikeCompare(PPCRegister& count,PPCCRRegister& result,PPCXERRegister& xer){
  result.compare<uint32_t>(count.u32,motogp::bikes::kCapacity,xer);
}
