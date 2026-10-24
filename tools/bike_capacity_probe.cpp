// Exercise the real generated table initializer, append, lookup and bitmap
// loader. Fake only metadata parsing, allocation plumbing and external I/O;
// no game profile, GPU, network or original asset modification is involved.
#include "motogp0910_init.h"
#include "../src/bike_capacity.h"
#include <rex/hook.h>
#include <rex/kernel/init.h>
#include <rex/memory.h>
#include <rex/runtime.h>
#include <rex/system/function_dispatcher.h>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
rex::Runtime* runtime;
uint32_t fixture,allocation,requested,key;
unsigned parsed=0,loads=0,expectedParameters=2;
uint32_t Read(uint8_t* base,uint32_t at){return rex::memory::load_and_swap<uint32_t>(base+at);}
void Write(uint8_t* base,uint32_t at,uint32_t value){rex::memory::store_and_swap<uint32_t>(base+at,value);}
void Check(bool value,const char* why){if(!value)throw std::runtime_error(why);}
void Zero(PPCContext& ctx,uint8_t*){ctx.r3.u64=0;}
void Context(PPCContext& ctx,uint8_t*){ctx.r3.u64=fixture+0x400;}
void KeyReference(PPCContext& ctx,uint8_t* base){Write(base,ctx.r3.u32,key);}
}
REX_HOOK_RAW(sub_822E8A08){(void)ctx;(void)base;} // Property registration, not the table logic under test.
REX_HOOK_RAW(sub_826EF090){
  requested=ctx.r3.u32;allocation=runtime->memory()->SystemHeapAlloc(requested+32);
  Check(allocation!=0,"Native allocator fixture failed");
  std::memset(base+allocation,0xA5,requested+32);ctx.r3.u64=allocation;
}
REX_HOOK_RAW(sub_822E8E58){
  const auto row=Read(base,ctx.r4.u32+8);
  Check(row==allocation+parsed*44,"Native append selected the wrong row");
  Write(base,row+20,key);++parsed;ctx.r3.u64=0;
}
REX_HOOK_RAW(sub_822A5E68){(void)ctx;(void)base;}
REX_HOOK_RAW(sub_8270BAF0){
  // Append's diagnostic label formatter uses guest CRT thread state. Its label
  // is not read by the table path; keep this fixture independent of guest TLS.
  base[ctx.r3.u32]=0;ctx.r3.u64=0;
}
REX_HOOK_RAW(sub_82396E38){ctx.r3.u64=1;(void)base;}
REX_HOOK_RAW(sub_8270A9D0){
  // The native list starts with a shared default and appends the class default,
  // plus one entry per used bike. Sorting is outside this bitmap fixture.
  Check(ctx.r4.u32==expectedParameters,"Unexpected parameter list size");
  bool moto2Default=false;
  for(unsigned i=0;i<ctx.r4.u32;++i)
    moto2Default|=std::string(reinterpret_cast<char*>(base+Read(base,ctx.r3.u32+i*8+4)))=="generic_600";
  Check(moto2Default,"Native Moto2 default parameter was not selected");
}
REX_HOOK_RAW(sub_826EC198){
  const auto path=std::string(reinterpret_cast<char*>(base+ctx.r5.u32))+"/"+
      reinterpret_cast<char*>(base+ctx.r6.u32)+".params";
  std::memcpy(base+ctx.r3.u32,path.c_str(),path.size()+1);
}
REX_HOOK_RAW(sub_826F8A30){
  Check(std::string(reinterpret_cast<char*>(base+ctx.r3.u32)).starts_with("bikes_2010/"),
      "Native Moto2 parameter directory changed");
  ++loads;ctx.r3.u64=0; // Stop after the real bitmap and path-building stages.
}
int main(int argc,char** argv){try{
  Check(argc==2,"Usage: bike_capacity_probe PATCHED_GAME_ROOT");
  rex::Runtime instance(std::filesystem::canonical(argv[1]));runtime=&instance;
  rex::RuntimeConfig config{.kernel_init=rex::kernel::InitializeKernel,.tool_mode=true};
  Check(!XFAILED(instance.Setup(PPCImageConfig,std::move(config)))&&
      !XFAILED(instance.LoadXexImage("game:\\default.xex")),"Native image setup failed");
  auto* base=instance.virtual_membase();fixture=instance.memory()->SystemHeapAlloc(0x20000);
  Check(fixture!=0,"Fixture memory allocation failed");
  const auto thunk=[&](PPCFunc* fn){return instance.function_dispatcher()->AllocateThunk(fn,0);};
  const auto zero=thunk(Zero),context=thunk(Context),reference=thunk(KeyReference);
  Check(zero&&context&&reference,"Virtual callback registration failed");
  Write(base,fixture+0x104,fixture+0x200);Write(base,fixture+0x200,fixture+0x280);
  Write(base,fixture+0x288,zero);
  Write(base,fixture+0x300,fixture+0x380);Write(base,fixture+0x380,context);Write(base,fixture+0x390,zero);
  Write(base,fixture+0x400,fixture+0x480);Write(base,fixture+0x49C,zero);Write(base,fixture+0x4AC,zero);
  Write(base,fixture+0x500,fixture+0x580);Write(base,fixture+0x584,reference);
  Write(base,0x829C5CE0,1);Write(base,0x829C5CC0,fixture+0x500);
  Write(base,0x829F3118,0);Write(base,0x829F311C,0);
  const auto call=[&](PPCFunc* fn,uint32_t arg,uint32_t second=0){
    PPCContext ctx{};ctx.r1.u64=fixture+0x1F000;ctx.lr=0x12345678;
    ctx.r31.u64=0x1122334455667788;ctx.r3.u64=arg;ctx.r4.u64=second;
    fn(ctx,base);
    Check(ctx.r1.u32==fixture+0x1F000&&ctx.lr==0x12345678&&ctx.r31.u64==0x1122334455667788,
        "Native capacity path damaged the calling convention");
    return ctx.r3.u32;
  };
  std::cerr<<"Checking native table initialization\n";
  call(sub_822E9248,fixture+0x100);
  Check(requested==motogp::bikes::kCapacity*44,"Allocation does not cover the expanded table");
  Check(Read(base,0x829F311C)==allocation&&Read(base,0x829F3118)==0,"Native table initialization failed");
  for(unsigned i=0;i<motogp::bikes::kCapacity;++i)
    Check(Read(base,allocation+i*44)==0&&Read(base,allocation+i*44+32)==UINT32_MAX,
        "An expanded row was not initialized with native defaults");
  std::cerr<<"Checking native append and lookup\n";
  for(unsigned i=0;i<motogp::bikes::kCapacity;++i){
    key=fixture+0x4000+i*48;const auto name="probe_bike_"+std::to_string(i);
    std::memcpy(base+key,name.c_str(),name.size()+1);
    Check(call(sub_822E9118,fixture+0x300)==0,"A valid expanded row was rejected");
    Check(call(sub_822E87A0,key)==i,"Native name lookup fell back to bike zero");
    if(i==101)Check(Read(base,0x829F3118)==102,"Complete DLC-size catalog was truncated");
  }
  Check(call(sub_822E9118,fixture+0x300)==1&&parsed==motogp::bikes::kCapacity,
      "Append exceeded the new capacity");
  for(unsigned i=0;i<32;++i)Check(base[allocation+requested+i]==0xA5,"Native table wrote beyond its allocation");
  std::cerr<<"Checking native used-bike bitmap\n";
  Write(base,0x82926FC0,1);
  for(unsigned i=0;i<20;++i)Write(base,0x8291E130+i*1824+12,fixture+0x600);
  Write(base,fixture+0x600,0);
  const auto bitmap=fixture+0x1F000-2736+80;
  std::memset(base+bitmap,0xCC,motogp::bikes::kCapacity);
  Check(call(sub_8229FF00,3)==0&&loads==1,"Native Moto2 bitmap fixture did not reach its expected I/O stop");
  for(unsigned i=0;i<motogp::bikes::kCapacity;++i)Check(base[bitmap+i]==0,"Expanded used-bike marker was left uninitialized");
  Write(base,fixture+0x604,1);Write(base,0x8291E130+12,fixture+0x604);
  Write(base,0x8291E130+1764,101);expectedParameters=3;
  std::memset(base+bitmap,0xCC,motogp::bikes::kCapacity);
  Check(call(sub_8229FF00,3)==0&&loads==2,"Expanded bike did not reach the native parameter list");
  for(unsigned i=0;i<motogp::bikes::kCapacity;++i)
    Check(base[bitmap+i]==(i==101?1:0),"Native grid did not mark the expanded bike correctly");
  PPCRegister count{};PPCXERRegister xer{1,0,0};PPCCRRegister result{};
  for(uint32_t n:{99u,100u,101u,111u,112u,UINT32_MAX}){
    count.u32=n;MotogpBikeCompare(count,result,xer);
    Check(bool(result.lt)==(n<motogp::bikes::kCapacity)&&result.so==1,"Capacity comparison lost unsigned bounds or XER summary");
  }
  instance.memory()->SystemHeapFree(allocation);instance.memory()->SystemHeapFree(fixture);
  std::cout<<"Native bike allocation, row initialization, 102-bike lookup, capacity rejection, bitmap and ABI checks passed\n";
  return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
