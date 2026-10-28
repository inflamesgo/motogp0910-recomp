#include "gauge_capture.h"
#include "gauge_hud.h"

#include <rex/hook.h>
#include <rex/memory.h>
#include <rex/runtime.h>
#include <algorithm>
#include <atomic>
#include <bit>
#include <cmath>
#include <cstdint>
#include <mutex>
#include <limits>

REX_EXTERN(__imp__sub_823191C8);
REX_EXTERN(__imp__sub_82318CB0);
REX_EXTERN(__imp__sub_82318248);
REX_EXTERN(__imp__sub_82317380);
REX_EXTERN(__imp__sub_82322620);
REX_EXTERN(__imp__sub_8236E858);

namespace motogp::gauge {
namespace {
constexpr uint32_t kViews=0x828C5C74,kUserManager=0x834F9494;
constexpr uint32_t kProfiles=0x82AEC2A8,kProfileStride=0x1EED44;
constexpr uint32_t kScreens=0x8336F4B0,kRaceMode=0x8292710C;
constexpr uint32_t kInputCache=0x833741B8,kInputStride=88;
std::atomic<bool> enabled{false};
std::atomic<uint64_t> generation{0};
std::mutex frame_mutex;
TelemetryFrame pending,published;
std::array<uint32_t,2> pending_views{},pending_bikes{};
struct TyreReplacement {uint32_t view=0,bike=0;uint64_t epoch=0;};
std::array<TyreReplacement,2> tyre_replacements{};

// Native rendering and presentation own staging. The UI owns the readiness
// switch, and receives only copied, immutable-by-convention frame values.
struct DrawScope {
  uint32_t view=0;
  uint64_t epoch=0;
  bool eligible=false,decided=false,replacing=false;
};
thread_local DrawScope* current_draw=nullptr;
struct HudShift {float x=0,y=0;bool uploaded=false;};
struct GpuTransform {std::array<uint32_t,16> bits{};bool valid=false;};
thread_local HudShift* hud_shift=nullptr;
thread_local GpuTransform last_gpu_transform{};
constexpr uint32_t kUiTransform=0x829949F0;
using NativeDraw=void(*)(PPCContext&,uint8_t*);

bool Readable(rex::Runtime* runtime,uint32_t address,uint32_t size) {
  if(!runtime||!address||!size||uint64_t(address)+size>0x100000000ull)return false;
  const uint64_t end=uint64_t(address)+size;
  for(uint64_t cursor=address;cursor<end;){
    auto* heap=runtime->memory()->LookupHeap(uint32_t(cursor));
    rex::memory::HeapAllocationInfo info{};
    if(!heap||!heap->QueryRegionInfo(uint32_t(cursor),&info)||
        !(info.state&rex::memory::kMemoryAllocationCommit)||
        !(info.protect&rex::memory::kMemoryProtectRead))return false;
    const uint64_t next=uint64_t(info.base_address)+info.region_size;
    if(next<=cursor)return false;
    cursor=next;
  }
  return true;
}
uint32_t Word(const uint8_t* base,uint32_t address) {
  return rex::memory::load_and_swap<uint32_t>(base+address);
}
float Float(const uint8_t* base,uint32_t address) {
  return rex::memory::load_and_swap<float>(base+address);
}
bool Range(float value,float minimum,float maximum) {
  return std::isfinite(value)&&value>=minimum&&value<=maximum;
}
std::optional<uint32_t> Property(rex::Runtime* runtime,const uint8_t* base,uint32_t object,uint32_t size){
  if(!Readable(runtime,object+12,4))return {};
  const auto value=Word(base,object+12);
  if(!Readable(runtime,value,size))return {};
  return value;
}
std::optional<bool> BoolProperty(rex::Runtime* runtime,const uint8_t* base,uint32_t object){
  const auto value=Property(runtime,base,object,1);
  return value?std::optional<bool>(base[*value]!=0):std::optional<bool>{};
}
std::optional<float> TyreFriction(rex::Runtime* runtime,const uint8_t* base,uint32_t wheel){
  // Read-only translation of native 823511C0 -> 82290978. Calling the native
  // leaf with a copied PPCContext would still use the caller's guest stack.
  if(!Readable(runtime,wheel+0x200,28)||!Readable(runtime,wheel+0x25C,4))return {};
  const auto curve=wheel+0x200;
  const auto count=Word(base,curve+4),samples=Word(base,curve+16);
  const float x=Float(base,wheel+0x25C),step=Float(base,curve+8),inverse=Float(base,curve+12);
  const float extent=Float(base,curve+20),scale=Float(base,curve+24);
  // This cap bounds validation of an optional resource, not the native curve
  // domain. A malformed curve falls back to the original tyre HUD.
  if(count<1||count>4096||(samples&3)||!Readable(runtime,samples,count*4)||
      !std::isfinite(x)||!std::isfinite(step)||!std::isfinite(inverse)||
      !std::isfinite(extent)||!std::isfinite(scale))return {};
  float result=0;
  if(count==1||extent==0){
    const float value=Float(base,samples);
    if(!std::isfinite(value))return {};
    result=value*scale;
  }else{
    const float scaled=inverse*x;
    if(step<=0||!std::isfinite(scaled)||double(scaled)<-2147483648.0||double(scaled)>=2147483648.0)return {};
    const int index=std::clamp(int(scaled),0,int(count)-2);
    const float low=Float(base,samples+index*4),high=Float(base,samples+(index+1)*4);
    if(!std::isfinite(low)||!std::isfinite(high))return {};
    const float fraction=std::clamp(x-float(index)*step,0.f,step)/step;
    result=std::fma(1.f-fraction,low,high*fraction)*scale;
  }
  return std::isfinite(result)?std::optional<float>(result):std::optional<float>{};
}
void CaptureTyres(rex::Runtime* runtime,const uint8_t* base,uint32_t physics,Telemetry& sample){
  const auto mode=Word(base,Word(base,kRaceMode)); // GameplayVisible validated both.
  sample.tyre_hud_visible=Readable(runtime,0x82946660,4)&&
      !(Word(base,0x82946660)&0x10)&&mode!=10;
  const auto minimum_pointer=Property(runtime,base,0x833597D0,4);
  const auto disabled=BoolProperty(runtime,base,0x83359818);
  const auto wear_enabled=BoolProperty(runtime,base,0x829272EC);
  if(!minimum_pointer||!disabled||!wear_enabled||uint64_t(physics)+0xC6C>0x100000000ull)return;
  const float minimum=Float(base,*minimum_pointer);
  if(!Range(minimum,0,.999999f))return;
  const auto front=TyreFriction(runtime,base,physics+0x5A4);
  const auto rear=TyreFriction(runtime,base,physics+0x5A4+0x264);
  if(!front||!rear)return;
  const auto condition=[&](float value){return value<=minimum?0.f:std::clamp((value-minimum)/(1.f-minimum),0.f,1.f);};
  sample.front_tyre=condition(*front);sample.rear_tyre=condition(*rear);
  sample.tyres_valid=true;sample.tyres_colored=!*disabled&&mode!=2&&*wear_enabled;
}

bool GameplayVisible(rex::Runtime* runtime,const uint8_t* base) {
  if(!base||!Readable(runtime,kScreens,32)||!Readable(runtime,kRaceMode,4))return false;
  const auto property=Word(base,kRaceMode);
  if(!Readable(runtime,property,4))return false;
  const auto mode=Word(base,property);
  if(!mode||mode>16||mode==15)return false; // Native attract playback is not player gameplay.
  // Native GetScreen(8), 8235A150, uses active records and excludes slot 3.
  // Live race HUD screens may coexist with the gauge. Every other active
  // screen (including unknown popups) keeps the host layer out of native UI.
  for(unsigned slot=0;slot<8;++slot){
    if(slot==3)continue;
    const auto record=Word(base,kScreens+slot*4);
    if(!record)continue;
    if(!Readable(runtime,record,4))return false;
    const auto object=Word(base,record);
    if(!Readable(runtime,object,4))return false;
    const auto type=Word(base,object);
    if(type!=0x82079C70&&type!=0x82071E88)return false; // StartingLights / ArcadeBonusTime.
  }
  return true;
}

std::optional<Telemetry> Capture(uint8_t* base,uint32_t view) {
  auto* runtime=rex::Runtime::instance();
  if(!base||runtime==nullptr||runtime->virtual_membase()!=base||
      (view&3)||!Readable(runtime,kViews,8)||!Readable(runtime,view,144))return {};
  Telemetry sample;
  if(view==Word(base,kViews))sample.view_index=0;
  else if(view==Word(base,kViews+4))sample.view_index=1;
  else return {};
  if(!GameplayVisible(runtime,base))return {};
  // Capture is also used by the separate tyre/CleanSection callbacks; do not
  // rely on FilledGauges having already applied these original guards.
  const auto disabled=BoolProperty(runtime,base,0x833562F8);
  if(!disabled||*disabled||Word(base,view+28)==2)return {};
  sample.view_x=Float(base,view+56);sample.view_y=Float(base,view+60);
  sample.view_width=Float(base,view+64);sample.view_height=Float(base,view+68);
  if(!Range(sample.view_x,0,1)||!Range(sample.view_y,0,1)||
      !Range(sample.view_width,.01f,1.001f)||!Range(sample.view_height,.01f,1.001f)||
      sample.view_x+sample.view_width>1.001f||sample.view_y+sample.view_height>1.001f)return {};
  const uint32_t bike=Word(base,view+140);
  if((bike&3)||!Readable(runtime,bike,1480)||Word(base,bike+1468)!=bike)return {};
  // Native 8231E028/82354090: this positive counter hides HUD elements while
  // the rider is detached; it is reset to zero on remount.
  if(int32_t(Word(base,bike+0x278))>0)return {};
  const uint32_t physics=Word(base,bike+1472);
  if(!physics||(physics&3)||uint64_t(physics)+652>0x100000000ull||
      !Readable(runtime,physics+640,12))return {};
  sample.mph=Float(base,bike+564);sample.rpm=Float(base,bike+556);
  sample.gear=int32_t(Word(base,bike+560));
  // The human physics initializer constructs the engine at +640. Its +4/+8
  // hold this bike's actual MinRpm/MaxRpm, not the shared 6000..18314 HUD scale.
  sample.min_rpm=Float(base,physics+644);sample.max_rpm=Float(base,physics+648);
  if(!Range(sample.mph,0,1000)||!Range(sample.min_rpm,0,60000)||
      !Range(sample.max_rpm,1000,60000)||sample.min_rpm>=sample.max_rpm||
      !Range(sample.rpm,0,sample.max_rpm*1.5f)||sample.gear<-1||sample.gear>6)return {};
  const uint32_t user=Word(base,view+4);
  if(user>=4||!Readable(runtime,kUserManager,4))return {};
  const uint32_t manager=Word(base,kUserManager);
  if((manager&3)||!Readable(runtime,manager,128))return {};
  const uint32_t slot=Word(base,manager+0x70+4*user);
  if(slot>=4)return {};
  const uint32_t profile=kProfiles+kProfileStride*slot;
  if(!Readable(runtime,profile+0x9647C,8))return {};
  sample.metric=Word(base,profile+0x9647C)!=0;
  if(Word(base,profile+0x96480)==0)return {};
  // Native 8236D7C0 copies X_INPUT_STATE.gamepad into this per-user cache:
  // +2/+3 are LT/RT bytes, +72 is the successful connection result.
  // Sample the game's cache, never repoll a host controller or infer from RPM.
  const auto input=kInputCache+kInputStride*user;
  if(Readable(runtime,input,kInputStride)&&Word(base,input+72)==1){
    sample.front_brake=float(base[input+2])/255.f;
    sample.throttle=float(base[input+3])/255.f;
    sample.inputs_valid=true;
  }
  CaptureTyres(runtime,base,physics,sample);
  return sample;
}

bool ReplaceChild(PPCContext& ctx,uint8_t* base) {
  auto* scope=current_draw;
  if(!scope||!scope->eligible||ctx.r4.u32!=scope->view||!SelectedAvailable())return false;
  if(scope->decided)return scope->replacing;
  scope->decided=true;
  auto sample=Capture(base,scope->view);
  if(!sample)return false;
  const std::lock_guard lock(frame_mutex);
  if(!enabled.load(std::memory_order_acquire)||
      generation.load(std::memory_order_acquire)!=scope->epoch)return false;
  const auto index=sample->view_index,bike=Word(base,scope->view+140);
  const auto& replacement=tyre_replacements[index];
  sample->tyre_replaced=replacement.view==scope->view&&replacement.bike==bike&&replacement.epoch==scope->epoch;
  pending[index]=sample;pending_views[index]=scope->view;pending_bikes[index]=bike;
  scope->replacing=true;
  return true;
}
GpuTransform MatrixBits(rex::Runtime* runtime,const uint8_t* base,uint32_t matrix){
  GpuTransform result;
  if((matrix&15)||!Readable(runtime,matrix,64))return result;
  for(unsigned i=0;i<result.bits.size();++i){
    result.bits[i]=Word(base,matrix+4*i);
    if(!Range(std::bit_cast<float>(result.bits[i]),-1000000,1000000))return {};
  }
  result.valid=true;return result;
}
void UploadHudTransform(PPCContext& context,uint8_t* base,NativeDraw upload){
  if(context.r3.u32!=4){upload(context,base);return;}
  const auto matrix=context.r4.u32;
  const auto original=MatrixBits(rex::Runtime::instance(),base,matrix);
  if(!original.valid){upload(context,base);return;}
  auto adjusted=original;
  if(hud_shift){
    for(unsigned row=0;row<4;++row){
      const float w=std::bit_cast<float>(original.bits[row*4+3]);
      for(unsigned column=0;column<2;++column){
        const float shift=column?hud_shift->y:hud_shift->x;
        const float value=std::bit_cast<float>(original.bits[row*4+column])+shift*w;
        if(!Range(value,-1000000,1000000)){upload(context,base);return;}
        adjusted.bits[row*4+column]=std::bit_cast<uint32_t>(value);
      }
    }
  }
  struct RestoreSource {
    uint8_t* base;uint32_t matrix;const GpuTransform& original;
    ~RestoreSource(){for(unsigned i=0;i<16;++i)
      rex::memory::store_and_swap<uint32_t>(base+matrix+4*i,original.bits[i]);}
  } restore{base,matrix,original};
  if(hud_shift)for(unsigned i=0;i<16;++i)
    rex::memory::store_and_swap<uint32_t>(base+matrix+4*i,adjusted.bits[i]);
  upload(context,base);last_gpu_transform=adjusted;
  if(hud_shift)hud_shift->uploaded=true;
}
void DrawShiftedHud(PPCContext& context,uint8_t* base,float x,float y,NativeDraw draw,NativeDraw restore_upload){
  if(x==0&&y==0){draw(context,base);return;}
  HudShift scope{x,y};
  auto* previous=hud_shift;
  // A top-level HUD starts from this view's current native transform. A nested
  // callback must restore the parent's already shifted GPU state instead.
  const auto before=previous&&previous->uploaded&&last_gpu_transform.valid
      ?last_gpu_transform:MatrixBits(rex::Runtime::instance(),base,kUiTransform);
  struct RestoreScope {HudShift* previous;~RestoreScope(){hud_shift=previous;}} restore_scope{previous};
  hud_shift=&scope;draw(context,base);
  if(scope.uploaded&&before.valid){
    const auto source=MatrixBits(rex::Runtime::instance(),base,kUiTransform);
    if(source.valid){
      struct RestoreSource {
        uint8_t* base;const GpuTransform& source;
        ~RestoreSource(){for(unsigned i=0;i<16;++i)
          rex::memory::store_and_swap<uint32_t>(base+kUiTransform+4*i,source.bits[i]);}
      } restore_source{base,source};
      for(unsigned i=0;i<16;++i)
        rex::memory::store_and_swap<uint32_t>(base+kUiTransform+4*i,before.bits[i]);
      PPCContext restored=context;restored.r3.u64=4;restored.r4.u64=kUiTransform;
      restore_upload(restored,base);last_gpu_transform=before;
    }
  }
}
void DrawCleanSection(PPCContext& context,uint8_t* base,uint32_t view,NativeDraw draw,NativeDraw restore_upload){
  const auto sample=enabled.load(std::memory_order_acquire)&&SelectedAvailable()
      ?Capture(base,view):std::optional<Telemetry>{};
  // Positive clip-space Y lifts the distinct, tall CleanSection indicator.
  DrawShiftedHud(context,base,0,sample?(2.f*160.f/1080.f)*sample->view_height:0,
      draw,restore_upload);
}
bool DrawTyreHud(PPCContext& context,uint8_t* base,uint32_t view,NativeDraw draw,NativeDraw restore_upload){
  const auto epoch=generation.load(std::memory_order_acquire);
  const auto sample=enabled.load(std::memory_order_acquire)&&SelectedAvailable()
      ?Capture(base,view):std::optional<Telemetry>{};
  if(sample){
    const auto index=sample->view_index,bike=Word(base,view+140);
    const bool replace=SelectedStyle()==1&&TyrePanelAvailable()&&sample->tyres_valid&&sample->tyre_hud_visible;
    const std::lock_guard lock(frame_mutex);
    if(enabled.load(std::memory_order_acquire)&&generation.load(std::memory_order_acquire)==epoch&&SelectedAvailable()){
      if(replace){
        tyre_replacements[index]={view,bike,epoch};
        if(pending[index]&&pending_views[index]==view&&pending_bikes[index]==bike)
          pending[index]->tyre_replaced=true;
        return true;
      }
      tyre_replacements[index]={};
      if(pending[index]&&pending_views[index]==view)pending[index]->tyre_replaced=false;
    }
  }
  // The original small motorcycle/tyre panel is a different native HUD object.
  // Keep its original position beside CleanSection: both native widgets move
  // up together so their relative alignment survives the custom gauge.
  DrawShiftedHud(context,base,0,sample?(2.f*160.f/1080.f)*sample->view_height:0,
      draw,restore_upload);
  return false;
}
}

void SetCaptureEnabled(bool value) {
  const std::lock_guard lock(frame_mutex);
  if(enabled.load(std::memory_order_relaxed)==value)return;
  generation.fetch_add(1,std::memory_order_release);
  enabled.store(value,std::memory_order_release);
  pending={};published={};pending_views={};pending_bikes={};tyre_replacements={};
}
void PublishFrame() {
  auto* runtime=rex::Runtime::instance();
  const bool gameplay=runtime&&GameplayVisible(runtime,runtime->virtual_membase());
  const std::lock_guard lock(frame_mutex);
  // A menu, fall or view/bike transition can happen after native capture but
  // before presentation. Revalidate identities and visibility for each view.
  published=gameplay&&enabled.load(std::memory_order_acquire)?pending:TelemetryFrame{};
  if(runtime)for(unsigned i=0;i<published.size();++i)if(published[i]){
    const auto current=Capture(runtime->virtual_membase(),pending_views[i]);
    if(!current||current->view_index!=i||Word(runtime->virtual_membase(),pending_views[i]+140)!=pending_bikes[i]){
      published[i].reset();continue;
    }
    auto& sample=*published[i];
    sample.tyres_valid=current->tyres_valid;sample.tyres_colored=current->tyres_colored;
    sample.tyre_hud_visible=current->tyre_hud_visible;
    if(current->tyres_valid){sample.front_tyre=current->front_tyre;sample.rear_tyre=current->rear_tyre;}
    sample.tyre_replaced=sample.tyre_replaced&&SelectedStyle()==1&&TyrePanelAvailable()&&
        current->tyres_valid&&current->tyre_hud_visible;
  }
  pending={};pending_views={};pending_bikes={};tyre_replacements={};
}
TelemetryFrame ReadFrame() {
  const std::lock_guard lock(frame_mutex);
  return published;
}
void ResetCapture() {
  const std::lock_guard lock(frame_mutex);
  enabled.store(false,std::memory_order_release);
  generation.fetch_add(1,std::memory_order_release);
  pending={};published={};pending_views={};pending_bikes={};tyre_replacements={};
}
#ifdef MOTOGP_GAUGE_PROBE
std::optional<Telemetry> CaptureForProbe(uint8_t* base,uint32_t view){return Capture(base,view);}
bool StageForProbe(uint8_t* base,uint32_t view){
  DrawScope scope{view,generation.load(std::memory_order_acquire),enabled.load(std::memory_order_acquire)};
  const auto previous=current_draw;current_draw=&scope;
  PPCContext context{};context.r4.u32=view;
  const bool result=ReplaceChild(context,base);current_draw=previous;
  return result;
}
void DrawCleanSectionForProbe(PPCContext& context,uint8_t* base,uint32_t view,NativeDraw draw,NativeDraw restore_upload){DrawCleanSection(context,base,view,draw,restore_upload);}
bool DrawTyreHudForProbe(PPCContext& context,uint8_t* base,uint32_t view,NativeDraw draw,NativeDraw restore_upload){return DrawTyreHud(context,base,view,draw,restore_upload);}
void UploadHudTransformForProbe(PPCContext& context,uint8_t* base,NativeDraw upload){UploadHudTransform(context,base,upload);}
#endif
} // namespace motogp::gauge

REX_HOOK_RAW(sub_823191C8) {
  using namespace motogp::gauge;
  DrawScope scope{ctx.r4.u32,generation.load(std::memory_order_acquire),
      enabled.load(std::memory_order_acquire)};
  struct Restore {
    DrawScope* previous;
    ~Restore(){current_draw=previous;}
  } restore{current_draw};
  current_draw=&scope;
  // Retain native visibility, viewport, matrix and update work. Only its
  // three draw calls are suppressed after a complete sample is accepted.
  // This draw callback has no semantic scalar return value.
  __imp__sub_823191C8(ctx,base);
}
REX_HOOK_RAW(sub_82318CB0) {
  if(!motogp::gauge::ReplaceChild(ctx,base))__imp__sub_82318CB0(ctx,base);
}
REX_HOOK_RAW(sub_82318248) {
  if(!motogp::gauge::ReplaceChild(ctx,base))__imp__sub_82318248(ctx,base);
}
REX_HOOK_RAW(sub_82317380) {
  motogp::gauge::DrawCleanSection(ctx,base,ctx.r4.u32,__imp__sub_82317380,__imp__sub_8236E858);
}
REX_HOOK_RAW(sub_82322620) {
  motogp::gauge::DrawTyreHud(ctx,base,ctx.r4.u32,__imp__sub_82322620,__imp__sub_8236E858);
}
REX_HOOK_RAW(sub_8236E858) {
  motogp::gauge::UploadHudTransform(ctx,base,__imp__sub_8236E858);
}
