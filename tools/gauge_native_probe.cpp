// Tool-mode title fixture: native Display Settings selector and navigation,
// using the actual generated PPC routines. No GPU or game save is initialized.
#include "motogp0910_init.h"
#include "../src/options_menu.h"
#include "../src/gauge_hud.h"
#include "../src/gauge_capture.h"
#include <rex/cvar.h>
#include <rex/kernel/init.h>
#include <rex/memory.h>
#include <rex/runtime.h>
#include <array>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <vector>
#include <limits>
#include <cmath>

REXCVAR_DEFINE_STRING(game_text_language,"english","Fixture","Native label language");
REX_EXTERN(sub_8227E838);
REX_EXTERN(sub_8227F318);
REX_EXTERN(sub_826EDB38);
namespace {
unsigned checks=0;
void Check(bool okay,const char* why){if(!okay)throw std::runtime_error(why);++checks;}
using Matrix=std::array<float,16>;
std::vector<Matrix> uploaded_transforms;
void RecordTransform(PPCContext& context,uint8_t* base){
  Matrix matrix{};
  for(unsigned i=0;i<matrix.size();++i)matrix[i]=rex::memory::load_and_swap<float>(base+context.r4.u32+4*i);
  uploaded_transforms.push_back(matrix);
}
void DrawTransform(PPCContext& context,uint8_t* base){
  motogp::gauge::UploadHudTransformForProbe(context,base,RecordTransform);
}
uint32_t nested_view=0;
void DrawNestedTyre(PPCContext& context,uint8_t* base){
  DrawTransform(context,base);
  motogp::gauge::DrawTyreHudForProbe(context,base,nested_view,DrawTransform,RecordTransform);
  DrawTransform(context,base);
}
}
int main(int argc,char** argv){try{
  Check(argc==2,"Usage: gauge_native_probe PATCHED_COMPLETE_GAME_ROOT");
  rex::Runtime runtime(std::filesystem::canonical(argv[1]));
  rex::RuntimeConfig config{.kernel_init=rex::kernel::InitializeKernel,.tool_mode=true};
  Check(!XFAILED(runtime.Setup(PPCImageConfig,std::move(config)))&&!XFAILED(runtime.LoadXexImage("game:\\default.xex")),"Load actual patched title data");
  auto* base=runtime.virtual_membase();const auto fixture=runtime.memory()->SystemHeapAlloc(0x20000);
  Check(fixture!=0,"Allocate isolated native menu fixture");
  const auto menu=fixture,screen=fixture+0x1000;
  auto read=[&](uint32_t address){return rex::memory::load_and_swap<uint32_t>(base+address);};
  auto write=[&](uint32_t address,uint32_t value){rex::memory::store_and_swap<uint32_t>(base+address,value);};
  motogp::options::Initialize(&runtime);
  for(const bool gamma:{false,true})for(const uint32_t conditional:{4u,5u}){
    std::memset(base+fixture,0,0x2000);
    std::memset(base+menu+428,0xA5,760-428);
    write(menu,motogp::options::kDisplaySettingsVtable);
    for(unsigned i=0;i<9;++i)write(menu+32+4*i,UINT32_MAX);
    std::vector<uint32_t> ids{0,1,2,3,conditional,6};if(gamma)ids.push_back(7);
    for(unsigned i=0;i<ids.size();++i){
      write(menu+32+4*i,ids[i]);write(menu+68+40*i+4,1);write(menu+68+40*i+16,1);
      // Real initialized rows have widget handles. The native navigation
      // routine finds its terminator by the first null +28, not by row type.
      write(menu+68+40*i+28,fixture+0x1400+8*i);
    }
    write(menu+24,uint32_t(ids.size()+1));write(screen+24,0);
    motogp::gauge::SetAvailable(true);motogp::gauge::SelectStyle(0);
    Check(motogp::options::PrepareDisplayGauge(base,menu,screen),"Append Display gauge selector");
    Check(read(menu+24)==ids.size()+2&&read(menu+32+4*ids.size())==8&&read(menu+36+4*ids.size())==UINT32_MAX,"Custom style is last; native count includes terminator");
    for(unsigned i=0;i<ids.size();++i)Check(read(menu+32+4*i)==ids[i],"Existing actions retain their original IDs/order");
    for(unsigned i=428;i<760;++i)Check(base[menu+i]==0xA5,"Display selector metadata stays inside original object capacity");
    const auto row=menu+68+40*uint32_t(ids.size()),widget=read(row+28);
    Check(read(row+16)==4&&widget!=0,"New row uses native choice widget");
    auto navigate=[&](int delta){
      PPCContext context{};context.r1.u32=fixture+0x1F000;context.r3.u32=menu+68;context.r4.u32=screen+24;context.r5.s64=delta;context.r6.u32=0;
      sub_8227F318(context,base);
    };
    write(screen+24,uint32_t(ids.size()-1));navigate(1);Check(read(screen+24)==ids.size(),"Native navigation reaches the appended last row");
    navigate(1);Check(read(screen+24)==0,"Native navigation wraps after the added row");
    navigate(-1);Check(read(screen+24)==ids.size(),"Reverse native navigation reaches the last row");
    const auto index=read(widget+16);
    auto step=[&](int delta){
      PPCContext context{};context.r1.u32=fixture+0x1F000;context.r3.u32=row;context.r4.s64=delta;
      sub_8227E838(context,base);motogp::options::CommitGaugeChoice(base,menu);
    };
    step(-1);Check(read(index)==0&&motogp::gauge::SelectedStyle()==0,"Left at first option is bounded by a readable leading sentinel");
    step(1);Check(read(index)==1&&motogp::gauge::SelectedStyle()==1,"Actual native selector chooses MotoGP17");
    step(1);Check(read(index)==1,"Right at final option is bounded by trailing sentinel");
    step(-1);Check(read(index)==0&&motogp::gauge::SelectedStyle()==0,"Actual native selector restores original style");
    for(int style=2;style<motogp::gauge::kStyleCount;++style)motogp::gauge::SetStyleAvailable(style,true);
    motogp::options::PrepareDisplayGauge(base,menu,screen);
    Check(read(widget+24)==motogp::gauge::kStyleCount,"Complete classic assets add their own choices to the same row");
    for(int style=1;style<motogp::gauge::kStyleCount;++style){step(1);Check(motogp::gauge::SelectedStyle()==style,"Native forward navigation selects each loaded gauge");}
    step(1);Check(read(index)==motogp::gauge::kStyleCount-1,"Final trailing sentinel bounds forward navigation");
    for(int style=motogp::gauge::kStyleCount-2;style>=0;--style){step(-1);Check(motogp::gauge::SelectedStyle()==style,"Native reverse navigation selects each loaded gauge");}
    motogp::gauge::SetStyleAvailable(1,false);motogp::options::PrepareDisplayGauge(base,menu,screen);
    step(1);Check(motogp::gauge::SelectedStyle()==2,"Missing MotoGP 17 does not shift the MotoGP 06 style ID");
    motogp::gauge::SetStyleAvailable(2,false);motogp::options::PrepareDisplayGauge(base,menu,screen);
    step(1);Check(motogp::gauge::SelectedStyle()==3,"URT2 remains selectable when both other packs are missing");
    step(1);Check(motogp::gauge::SelectedStyle()==4,"URT3 keeps its own style ID when preceding packs are missing");
    PPCContext label{};label.r3.u32=widget+4;sub_826EDB38(label,base);
    Check(std::strcmp(reinterpret_cast<char*>(base+label.r3.u32),"Gauge Style")==0,"Exact native label handle resolves title-cased custom text");
    Check(motogp::options::GaugeLabel(widget+8)==0,"Unrelated label handles are not intercepted");
    Check(motogp::options::PrepareDisplayGauge(base,menu,screen)&&read(menu+24)==ids.size()+2,"Repeated draw preparation does not duplicate the row");
    write(screen+24,uint32_t(ids.size()));motogp::gauge::SetAvailable(false);
    Check(motogp::options::PrepareDisplayGauge(base,menu,screen)&&read(menu+24)==ids.size()+1,"Unavailable assets remove the optional row cleanly");
    Check(read(screen+24)==ids.size()-1&&read(row+16)==0&&read(row+28)==0,"Removed row is not focusable and cursor stays in range");
    for(unsigned i=428;i<760;++i)Check(base[menu+i]==0xA5,"Removal preserves adjacent native fields");
  }
  // Populate the native structures in actual runtime-managed guest memory.
  // The adapter itself performs heap/readability and range validation.
  const auto view=fixture+0x2000,bike=fixture+0x4000,physics=fixture+0x5000,manager=fixture+0x10000;
  auto floating=[&](uint32_t address,float value){rex::memory::store_and_swap<float>(base+address,value);};
  std::memset(base+view,0,0x100);std::memset(base+bike,0,0x600);std::memset(base+physics,0,0x1000);std::memset(base+manager,0,128);
  write(0x828C5C74,view);write(0x828C5C78,0);write(view+4,0);write(view+140,bike);
  floating(view+64,1);floating(view+68,1);write(bike+1468,bike);write(bike+1472,physics);
  floating(bike+556,13500);write(bike+560,4);floating(bike+564,107.52f);
  floating(physics+644,3000);floating(physics+648,18000);
  write(0x834F9494,manager);write(manager+0x70,0);
  constexpr uint32_t profile=0x82AEC2A8;write(profile+0x9647C,1);write(profile+0x96480,1);
  const auto record=fixture+0x11000,active_screen=fixture+0x11100,mode=fixture+0x11200;
  for(unsigned i=0;i<8;++i)write(0x8336F4B0+4*i,0);
  write(0x8292710C,mode);write(mode,10); // Native Time Trial RaceMode property.
  const auto disable_value=fixture+0x11210,tyre_disable=fixture+0x11211,wear_enabled=fixture+0x11212;
  const auto minimum=fixture+0x11220;
  write(0x833562F8+12,disable_value);base[disable_value]=0;
  write(0x83359818+12,tyre_disable);base[tyre_disable]=0;
  write(0x829272EC+12,wear_enabled);base[wear_enabled]=1;
  write(0x833597D0+12,minimum);floating(minimum,.73f);write(0x82946660,0);
  const auto front=physics+0x5A4,rear=front+0x264;
  const auto front_values=fixture+0x13000,rear_values=fixture+0x13100;
  for(const auto wheel:{front,rear}){
    const auto curve=wheel+0x200,values=wheel==front?front_values:rear_values;
    write(curve+4,3);floating(curve+8,.5f);floating(curve+12,2);
    write(curve+16,values);floating(curve+20,1);floating(curve+24,1);
    floating(values,1);floating(values+4,.865f);floating(values+8,.73f);floating(wheel+0x25C,0);
  }
  constexpr uint32_t input=0x833741B8;
  std::memset(base+input,0,88*4);
  const auto sample=motogp::gauge::CaptureForProbe(base,view);
  Check(sample&&sample->rpm==13500&&sample->max_rpm==18000&&sample->min_rpm==3000&&sample->gear==4&&sample->metric,"Adapter reads real engine values and original unit flag");
  write(input+72,1);base[input+2]=64;base[input+3]=128;
  const auto partial=motogp::gauge::CaptureForProbe(base,view);
  Check(partial&&partial->inputs_valid&&partial->front_brake==64.f/255.f&&partial->throttle==128.f/255.f,"Native LT/RT pressure is normalized independently without estimating from speed");
  base[input+2]=0;base[input+3]=255;
  Check(motogp::gauge::CaptureForProbe(base,view)->throttle==1&&motogp::gauge::CaptureForProbe(base,view)->front_brake==0,"Full throttle and released front brake remain independent");
  base[input+2]=255;base[input+3]=0;
  Check(motogp::gauge::CaptureForProbe(base,view)->front_brake==1&&motogp::gauge::CaptureForProbe(base,view)->throttle==0,"Full front brake does not fill the throttle bar");
  write(input+72,0);
  const auto disconnected=motogp::gauge::CaptureForProbe(base,view);
  Check(disconnected&&!disconnected->inputs_valid&&disconnected->front_brake==0&&disconnected->throttle==0,"Disconnected controller cannot retain a stale brake fill");
  write(input+88+72,1);base[input+88+2]=32;base[input+88+3]=224;
  write(view+4,1);write(manager+0x74,0);
  const auto other_user=motogp::gauge::CaptureForProbe(base,view);
  Check(other_user&&other_user->inputs_valid&&other_user->front_brake==32.f/255.f&&other_user->throttle==224.f/255.f,"Each native local user receives its own input pressure");
  write(view+4,0);write(input+72,1);base[input+2]=0;base[input+3]=0;
  write(profile+0x9647C,0);Check(!motogp::gauge::CaptureForProbe(base,view)->metric,"MPH preference is preserved");write(profile+0x9647C,1);
  write(mode,1);
  floating(front+0x25C,.25f);floating(rear+0x25C,.75f);
  const auto tyres=motogp::gauge::CaptureForProbe(base,view);
  Check(tyres&&tyres->tyres_valid&&tyres->tyres_colored&&tyres->tyre_hud_visible&&
      std::abs(tyres->front_tyre-.75f)<.00001f&&std::abs(tyres->rear_tyre-.25f)<.00001f,
      "Actual friction curve interpolation produces independent front/rear native condition");
  floating(front+0x25C,0);floating(rear+0x25C,1);
  const auto endpoints=motogp::gauge::CaptureForProbe(base,view);
  Check(endpoints->front_tyre==1&&endpoints->rear_tyre==0,"Native friction threshold maps fresh and worn endpoints");
  write(front+0x204,1);floating(front_values,.865f);
  Check(std::abs(motogp::gauge::CaptureForProbe(base,view)->front_tyre-.5f)<.00001f,"Single-sample native tyre curves use their constant value");
  write(front+0x204,3);floating(front_values,1);floating(front+0x208,0);
  const auto bad_curve=motogp::gauge::CaptureForProbe(base,view);
  Check(bad_curve&&!bad_curve->tyres_valid,"Invalid optional tyre curve retains the gauge and native tyre fallback");floating(front+0x208,.5f);
  write(front+0x210,0xFFFFFFF0);
  Check(!motogp::gauge::CaptureForProbe(base,view)->tyres_valid,"Unreadable tyre sample table cannot fabricate condition");write(front+0x210,front_values);
  base[wear_enabled]=0;
  Check(motogp::gauge::CaptureForProbe(base,view)->tyres_valid&&!motogp::gauge::CaptureForProbe(base,view)->tyres_colored,"Disabled game tyre wear retains native neutral-white semantics");base[wear_enabled]=1;
  write(0x82946660,0x10);
  Check(!motogp::gauge::CaptureForProbe(base,view)->tyre_hud_visible,"Native global tyre visibility flag is authoritative");write(0x82946660,0);
  write(mode,10);Check(!motogp::gauge::CaptureForProbe(base,view)->tyre_hud_visible,"Time Trial preserves the native tyre HUD exclusion");
  base[disable_value]=1;Check(!motogp::gauge::CaptureForProbe(base,view),"FilledGauges disable property is checked outside gauge children too");base[disable_value]=0;
  write(view+28,2);Check(!motogp::gauge::CaptureForProbe(base,view),"Native view-type exclusion remains authoritative");write(view+28,0);
  motogp::gauge::SetAvailable(true);motogp::gauge::SelectStyle(1);motogp::gauge::SetCaptureEnabled(true);
  const auto matrix_address=fixture+0x12000;
  auto layout=[&](const Matrix& matrix){
    for(unsigned i=0;i<matrix.size();++i){floating(matrix_address+4*i,matrix[i]);floating(0x829949F0+4*i,matrix[i]);}
    PPCContext context{};context.r3.u32=4;context.r4.u32=matrix_address;
    uploaded_transforms.clear();
    motogp::gauge::DrawCleanSectionForProbe(context,base,view,DrawTransform,RecordTransform);
    for(unsigned i=0;i<matrix.size();++i)Check(read(matrix_address+4*i)==std::bit_cast<uint32_t>(matrix[i]),"Clean Section restores every source matrix bit");
    Check(context.r3.u32==4&&context.r4.u32==matrix_address,"GPU-state restoration preserves the caller context");
  };
  const Matrix orthographic{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
  const Matrix perspective{1.1f,0,0,0,0,1.9555556f,0,0,0,0,1.0001f,1,0,0,-.1f,0};
  const Matrix rotated{.8f,.3f,0,0,-.3f,.8f,0,0,0,0,1,0,.1f,-.2f,0,1};
  for(const auto& matrix:{orthographic,perspective,rotated})for(const float view_height:{1.f,.5f}){
    floating(view+68,view_height);layout(matrix);
    Check(uploaded_transforms.size()==2&&uploaded_transforms.back()==matrix,"Following HUD elements receive the original GPU transform");
    const auto& shifted=uploaded_transforms.front();
    for(const std::array<float,4> vertex: {std::array<float,4>{.2f,.6f,1,1},std::array<float,4>{.5f,-.8f,2,1}}){
      std::array<float,4> before{},after{};
      for(unsigned c=0;c<4;++c)for(unsigned r=0;r<4;++r){before[c]+=vertex[r]*matrix[r*4+c];after[c]+=vertex[r]*shifted[r*4+c];}
      Check(before[0]==after[0]&&before[2]==after[2]&&before[3]==after[3],"Lifting Clean Section preserves horizontal placement, depth and perspective");
      Check(std::abs(after[1]/after[3]-before[1]/before[3]-(2.f*160.f/1080.f)*view_height)<.000001f,"Clean Section moves up by the requested viewport-relative pixel distance");
    }
  }
  floating(view+68,1);
  motogp::gauge::SelectStyle(0);layout(orthographic);
  Check(uploaded_transforms.size()==1&&uploaded_transforms[0]==orthographic,"Original gauge style retains the original Clean Section layout");motogp::gauge::SelectStyle(1);
  motogp::gauge::SetAvailable(false);layout(orthographic);
  Check(uploaded_transforms.size()==1&&uploaded_transforms[0]==orthographic,"Unavailable replacement does not move the native HUD");motogp::gauge::SetAvailable(true);
  write(record,active_screen);write(active_screen,0x82079FD4);write(0x8336F4B0,record);layout(orthographic);
  Check(uploaded_transforms.size()==1&&uploaded_transforms[0]==orthographic,"Native menus retain the original indicator placement");write(0x8336F4B0,0);
  PPCContext outside{};outside.r3.u32=4;outside.r4.u32=matrix_address;uploaded_transforms.clear();DrawTransform(outside,base);
  Check(uploaded_transforms.size()==1&&uploaded_transforms[0]==orthographic,"Translation scope cannot leak to unrelated HUD matrix uploads");
  write(mode,1);motogp::gauge::SetTyrePanelAvailable(false);
  auto tyre_draw=[&]{
    for(unsigned i=0;i<16;++i){floating(matrix_address+4*i,orthographic[i]);floating(0x829949F0+4*i,orthographic[i]);}
    PPCContext context{};context.r3.u32=4;context.r4.u32=matrix_address;
    uploaded_transforms.clear();
    const bool replaced=motogp::gauge::DrawTyreHudForProbe(context,base,view,DrawTransform,RecordTransform);
    Check(context.r3.u32==4&&context.r4.u32==matrix_address,"Tyre draw preserves caller context during GPU restoration");
    for(unsigned i=0;i<16;++i)Check(read(matrix_address+4*i)==std::bit_cast<uint32_t>(orthographic[i])&&read(0x829949F0+4*i)==std::bit_cast<uint32_t>(orthographic[i]),"Tyre translation restores all source/global matrix bits");
    return replaced;
  };
  for(const int style:{1,2,3,4})for(const float view_width:{1.f,.5f})for(const float view_height:{1.f,.5f}){
    motogp::gauge::SetStyleAvailable(style,true);motogp::gauge::SelectStyle(style);floating(view+64,view_width);floating(view+68,view_height);
    Check(!tyre_draw()&&uploaded_transforms.size()==2&&uploaded_transforms.back()==orthographic,"Custom-style native tyre fallback restores original GPU state");
    const auto& translated=uploaded_transforms.front();
    Check(translated[12]==0&&std::abs(translated[13]-(2.f*160.f/1080.f)*view_height)<.000001f,
        "Small tyre HUD keeps native horizontal alignment and follows the tall CleanSection upward offset");
  }
  floating(view+64,1);floating(view+68,1);motogp::gauge::SelectStyle(3);nested_view=view;
  PPCContext nested{};nested.r3.u32=4;nested.r4.u32=matrix_address;uploaded_transforms.clear();
  motogp::gauge::DrawCleanSectionForProbe(nested,base,view,DrawNestedTyre,RecordTransform);
  Check(uploaded_transforms.size()==5&&uploaded_transforms[0]==uploaded_transforms[2]&&uploaded_transforms[0]==uploaded_transforms[3]&&uploaded_transforms.back()==orthographic,
      "Nested tyre scope restores the parent's shifted GPU state before the outer scope restores native state");
  Check(uploaded_transforms[1][12]==0&&uploaded_transforms[1][13]==uploaded_transforms[0][13]&&uploaded_transforms[0][13]>0,
      "Nested native tyre and CleanSection translations preserve their shared vertical alignment");
  motogp::gauge::SelectStyle(0);Check(!tyre_draw()&&uploaded_transforms.size()==1&&uploaded_transforms[0]==orthographic,"Original style keeps native tyre placement");
  motogp::gauge::SelectStyle(1);motogp::gauge::SetTyrePanelAvailable(true);
  // Both native draw orders must yield exactly one host tyre panel.
  for(const bool tyre_first:{false,true}){
    if(tyre_first)Check(tyre_draw()&&uploaded_transforms.empty(),"Ready donor tyre panel suppresses native tyre sprites");
    Check(motogp::gauge::StageForProbe(base,view),"Gauge sample stages around either tyre callback order");
    if(!tyre_first)Check(tyre_draw()&&uploaded_transforms.empty(),"Tyre callback after gauge capture records replacement in the pending sample");
    motogp::gauge::PublishFrame();
    Check(motogp::gauge::ReadFrame()[0]&&motogp::gauge::ReadFrame()[0]->tyre_replaced,"Published donor panel has matching native-suppression evidence");
  }
  Check(motogp::gauge::StageForProbe(base,view),"Fresh frame stages a gauge without a tyre callback");motogp::gauge::PublishFrame();
  Check(!motogp::gauge::ReadFrame()[0]->tyre_replaced,"Suppression evidence cannot leak into the next frame");
  floating(front+0x208,0);
  Check(!tyre_draw()&&uploaded_transforms.size()==2,"Invalid tyre curve keeps moved native fallback even with donor artwork ready");floating(front+0x208,.5f);
  write(mode,10);Check(!tyre_draw(),"Native Time Trial tyre visibility cannot be overridden by donor readiness");
  motogp::gauge::SetTyrePanelAvailable(false);
  write(bike+0x278,1);Check(!motogp::gauge::StageForProbe(base,view),"Fallen rider uses the native off-bike visibility predicate");write(bike+0x278,0);
  Check(motogp::gauge::StageForProbe(base,view),"Remount restores gauge capture");write(bike+0x278,2);motogp::gauge::PublishFrame();
  Check(!motogp::gauge::ReadFrame()[0],"A fall after capture but before presentation discards stale gauge telemetry");write(bike+0x278,0);
  Check(motogp::gauge::StageForProbe(base,view),"Valid rider stages after remount");write(view+140,bike+16);motogp::gauge::PublishFrame();
  Check(!motogp::gauge::ReadFrame()[0],"A same-frame bike pointer change cannot publish the previous rider's gauge");write(view+140,bike);
  Check(motogp::gauge::StageForProbe(base,view),"Validated native draw sample may replace only its gauge children");
  motogp::gauge::PublishFrame();Check(motogp::gauge::ReadFrame()[0].has_value(),"Presentation publishes an owning telemetry snapshot");
  motogp::gauge::PublishFrame();Check(!motogp::gauge::ReadFrame()[0],"A frame without a draw removes stale telemetry");
  floating(bike+556,std::numeric_limits<float>::quiet_NaN());Check(!motogp::gauge::StageForProbe(base,view),"NaN RPM retains native rendering");floating(bike+556,13500);
  floating(physics+648,0);Check(!motogp::gauge::StageForProbe(base,view),"Missing engine calibration retains native rendering");floating(physics+648,18000);
  Check(!motogp::gauge::StageForProbe(base,fixture+0x3000),"Unknown view retains native rendering");
  write(profile+0x96480,0);Check(!motogp::gauge::StageForProbe(base,view),"Native HUD visibility remains authoritative");write(profile+0x96480,1);
  motogp::gauge::SelectStyle(0);Check(!motogp::gauge::StageForProbe(base,view),"Original style cannot suppress native children");motogp::gauge::SelectStyle(1);
  write(record,active_screen);
  // Exercise each native menu family, not just Pause. Retain a valid bike and
  // old race data so stale telemetry cannot make the host gauge leak through.
  for(const auto type:{0x82079FD4u,0x820803DCu,0x82076724u,0x820760DCu,0x82080A84u,
      0x8207BABCu,0x8207D508u,0x8207D990u,0x82032DFCu,0x8200CF7Cu,0xDEADBEEFu}){
    Check(motogp::gauge::StageForProbe(base,view),"A valid gameplay sample may stage before a menu opens");
    write(0x8336F4B0,record);write(active_screen,type);
    motogp::gauge::PublishFrame();
    Check(!motogp::gauge::ReadFrame()[0],"Opening a menu before presentation discards the staged gauge");
    Check(!motogp::gauge::StageForProbe(base,view),"Continue/results/replay/settings/popups do not capture a custom gauge");
    write(0x8336F4B0,0);
  }
  for(const auto type:{0x82079C70u,0x82071E88u}){
    write(0x8336F4B0,record);write(active_screen,type);
    Check(motogp::gauge::StageForProbe(base,view),"Starting lights and Arcade bonus notifications retain the racing gauge");
  }
  write(0x8336F4B0,0);write(0x8336F4B0+12,record);write(active_screen,0x82079FD4);
  Check(motogp::gauge::StageForProbe(base,view),"Excluded native slot 3 does not hide a racing gauge");write(0x8336F4B0+12,0);
  write(mode,15);Check(!motogp::gauge::StageForProbe(base,view),"Attract playback does not display the custom gauge");write(mode,10);
  const auto second_view=fixture+0x6000,second_bike=fixture+0x8000,second_physics=fixture+0x9000;
  std::memcpy(base+second_view,base+view,144);std::memcpy(base+second_bike,base+bike,1480);std::memcpy(base+second_physics,base+physics,652);
  write(0x828C5C78,second_view);write(second_view+140,second_bike);write(second_bike+1468,second_bike);write(second_bike+1472,second_physics);write(second_bike+560,2);
  floating(view+68,.5f);floating(second_view+60,.5f);floating(second_view+68,.5f);
  Check(motogp::gauge::StageForProbe(base,view)&&motogp::gauge::StageForProbe(base,second_view),"Both local views stage independently");
  motogp::gauge::PublishFrame();const auto split=motogp::gauge::ReadFrame();
  Check(split[0]&&split[1]&&split[0]->gear==4&&split[1]->gear==2&&split[1]->view_y==.5f,"Each split-screen snapshot retains its own rider/view data");
  motogp::gauge::SetCaptureEnabled(false);Check(!motogp::gauge::ReadFrame()[0]&&!motogp::gauge::ReadFrame()[1],"Closing readiness clears current and pending frames");
  motogp::gauge::ResetCapture();motogp::gauge::SetAvailable(false);
  std::cout<<"Native Display Settings and telemetry fixtures passed: "<<checks<<" checks\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAILED: "<<e.what()<<'\n';return 1;}}
