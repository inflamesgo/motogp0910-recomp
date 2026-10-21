#include "motogp0910_init.h"
#include "../src/quit_menu.h"
#include "../src/localization.h"
#include <rex/runtime.h>
#include <rex/system/function_dispatcher.h>
#include <rex/ui/window.h>
#include <rex/graphics/xenos.h>
#include <rex/kernel/init.h>
#include <rex/input/input_system.h>
#include <rex/input/mnk/mnk_input_driver.h>
#include <array>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <stdexcept>
using namespace rex::input;
using rex::X_RESULT;using rex::X_STATUS;
REX_EXTERN(sub_8227F318);
REX_EXTERN(sub_821E4EE0);
REX_EXTERN(sub_826EC558);
REX_EXTERN(sub_821FCCA8);
REX_EXTERN(sub_822AE9F8);
REX_EXTERN(sub_8229C2C0);
namespace {
int checks=0;
void Check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);++checks;}
struct Driver final:InputDriver {
 X_INPUT_STATE state{};Driver():InputDriver(nullptr,0){}
 X_STATUS Setup()override{return X_STATUS_SUCCESS;}
 void EnumerateDevices(std::vector<DeviceInfo>& out)override{DeviceInfo info{};info.id=static_cast<DeviceId>(1);out.push_back(info);}
 X_RESULT GetDeviceState(DeviceId,X_INPUT_STATE* out)override{*out=state;return X_ERROR_SUCCESS;}
 X_RESULT GetDeviceCapabilities(DeviceId,uint32_t,X_INPUT_CAPABILITIES* out)override{*out={};return X_ERROR_SUCCESS;}
 X_RESULT SetDeviceVibration(DeviceId,X_INPUT_VIBRATION*)override{return X_ERROR_SUCCESS;}
 X_RESULT GetDeviceKeystroke(DeviceId,uint32_t,X_INPUT_KEYSTROKE*)override{return X_ERROR_EMPTY;}
};
uint32_t fixture_record=0,fixture_question=0;
int native_opens=0;
void FixtureQuestion(PPCContext& ctx,uint8_t*){
 const auto handle=ctx.r3.u32;ctx.r3.u64=fixture_question+(handle==0x8288C870?32:handle==0x8288C874?40:0);
}
void FixtureNativeOpen(PPCContext& ctx,uint8_t* base){
 const auto read=[&](uint32_t p){return rex::memory::load_and_swap<uint32_t>(base+p);};
 const auto write=[&](uint32_t p,uint32_t v){rex::memory::store_and_swap<uint32_t>(base+p,v);};
 Check(ctx.r7.u32==4&&ctx.r8.s32==-1,"Native confirmation uses the original modal layer and input owner");
 Check(read(ctx.r3.u32)==fixture_question,"Native body comes from the localized game question");
 Check(read(ctx.r5.u32)==fixture_question+32&&read(ctx.r5.u32+4)==fixture_question+40,"Explicit native Yes/No labels replace default Accept/Cancel");
 Check(read(ctx.r5.u32+12)==13,"Native flags preserve Back navigation");
 write(fixture_record+4,ctx.r6.u32);
 std::memcpy(base+fixture_record+8,base+ctx.r3.u32,20);
 write(fixture_record+8+16,ctx.r9.u32);write(fixture_record+8+28,1);
 write(0x8336F4C0,fixture_record);++native_opens;
}
}
int main(int argc,char** argv){try{
 Check(argc==3,"Usage: quit_menu_probe GAME_ROOT FONT_TTF");
 Driver* pad=nullptr;rex::input::mnk::MnkInputDriver* keyboard=nullptr;
 rex::Runtime runtime(std::filesystem::canonical(argv[1]));rex::RuntimeConfig config{.kernel_init=rex::kernel::InitializeKernel,.tool_mode=true};
 config.input_factory=[&](bool){auto input=std::make_unique<InputSystem>(nullptr);auto driver=std::make_unique<Driver>();pad=driver.get();input->AddDriver(std::move(driver));auto mnk=std::make_unique<rex::input::mnk::MnkInputDriver>(nullptr,0);keyboard=mnk.get();Check(keyboard->Setup()==X_STATUS_SUCCESS,"Actual MnK driver initializes");input->AddDriver(std::move(mnk));input->SetDeviceAssignment(std::make_unique<SlotAssignment>());return input;};
 Check(!XFAILED(runtime.Setup(PPCImageConfig,std::move(config)))&&!XFAILED(runtime.LoadXexImage("game:\\default.xex")),"Load actual patched game and input system");
 auto* memory=runtime.memory();auto* base=runtime.virtual_membase();const auto allocation=memory->SystemHeapAlloc(0x4000);Check(allocation!=0,"Fixture allocation");
 std::memset(base+allocation,0,0x4000);
 const auto write=[&](uint32_t a,uint32_t v){rex::memory::store_and_swap<uint32_t>(base+a,v);};const auto read=[&](uint32_t a){return rex::memory::load_and_swap<uint32_t>(base+a);};
 motogp::quit::Initialize(&runtime);motogp::quit::SetFocused(true);motogp::quit::PollFrontendInput();const auto label=motogp::quit::LabelAddress();
 { // The original native sequence owns fade and video cleanup. Only its
   // request byte is changed; final Press Start and unrelated modes reject it.
   constexpr std::array<uint32_t,6> addresses={0x82942CC0,0x82942CC4,0x82942CB0,0x828DA060,0x82942CBC,0x8288B188};
   std::array<uint32_t,6> saved{};for(size_t i=0;i<saved.size();++i)saved[i]=read(addresses[i]);
   write(0x82942CC0,0);write(0x82942CC4,4);write(0x82942CB0,0);write(0x828DA060,11);write(0x82942CBC,30);write(0x8288B188,allocation);
   Check(motogp::quit::IntroSkipAllowed(base)&&motogp::quit::BeginIntroSkip(base),"Loaded non-final intro accepts a native skip request");
   Check(base[0x82942CB2]==1&&read(0x82942CC0)==0,"Skip sets only the native fade request, never jumps the index");
   Check(!motogp::quit::BeginIntroSkip(base),"A pending transition cannot be requested twice");
   motogp::quit::FinishIntroSkip(base);Check(base[0x82942CB2]==1,"Request persists while native fade is in progress");
   write(0x82942CC0,1);motogp::quit::FinishIntroSkip(base);Check(!base[0x82942CB2],"Advancing one entry clears the owned request so the next intro cannot be skipped automatically");
   for(uint32_t index:{3u,4u,0xffffffffu}){write(0x82942CC0,index);Check(!motogp::quit::BeginIntroSkip(base),"Final title and invalid sequence indices cannot be skipped");}
   write(0x82942CC0,0);write(0x82942CBC,10);Check(!motogp::quit::BeginIntroSkip(base),"Skip waits for the native initial frame guard");write(0x82942CBC,30);
   write(0x828DA060,15);Check(!motogp::quit::BeginIntroSkip(base),"Frontend/gameplay mode cannot use intro skipping");write(0x828DA060,11);
   rex::cvar::SetFlagByName("allow_intro_skip","false");Check(!motogp::quit::BeginIntroSkip(base),"Launcher option disables manual intro skipping");rex::cvar::SetFlagByName("allow_intro_skip","true");
   write(0x82942CC4,33);Check(!motogp::quit::BeginIntroSkip(base),"Corrupt sequence count is rejected");
   write(0x82942CC4,4);write(0x82942CC0,0);
   motogp::quit::RequestIntroSkip();motogp::quit::PollIntroInput(base);Check(base[0x82942CB2]==1,"Fresh keyboard request reaches the native intro fade path");
   write(0x82942CC0,1);motogp::quit::FinishIntroSkip(base);
   pad->state.gamepad.buttons=X_INPUT_GAMEPAD_A;motogp::quit::PollFrontendInput();motogp::quit::PollIntroInput(base);Check(base[0x82942CB2]==1,"Cross/A from the real input-system path requests intro skipping");
   X_INPUT_STATE masked{};static_cast<InputSystem*>(runtime.input_system())->GetState(0,&masked);Check(!(masked.gamepad.buttons&X_INPUT_GAMEPAD_A),"The consumed Cross/A is release-masked from the guest");
   write(0x82942CC0,2);motogp::quit::FinishIntroSkip(base);motogp::quit::PollFrontendInput();motogp::quit::PollIntroInput(base);Check(!base[0x82942CB2],"Holding Cross/A cannot skip the next entry");
   pad->state.gamepad.buttons=0;motogp::quit::PollFrontendInput();motogp::quit::PollIntroInput(base);
   motogp::quit::SetFocused(false);motogp::quit::RequestIntroSkip();motogp::quit::PollIntroInput(base);Check(!base[0x82942CB2],"Unfocused startup cannot consume a keyboard request");motogp::quit::SetFocused(true);motogp::quit::PollFrontendInput();
   for(size_t i=0;i<saved.size();++i)write(addresses[i],saved[i]);
 }
 const auto iterator=allocation+0x700;write(iterator,label);
 for(unsigned char expected:std::string("QUIT GAME\0",10)){
   PPCContext decode{};decode.r3.u32=iterator;sub_826EC558(decode,base);
   Check(decode.r3.u32==expected,"Actual native text decoder reads every QUIT GAME codepoint");
 }
 Check(read(iterator)==label+10,"Native decoder advances exactly through UTF8 label and terminator");
 const auto menu=allocation,screen=allocation+0x500,stack=allocation+0x3000;
 write(menu,motogp::quit::kMainMenuVtable);constexpr std::array<uint32_t,7> ids={2,0,1,3,4,5,0xffffffff};
 for(unsigned i=0;i<ids.size();++i)write(menu+0x20+4*i,ids[i]);
 for(unsigned i=0;i<7;++i){const auto row=menu+0x3c+i*40;write(row+4,1);write(row+16,i<6?1:0);write(row+28,i<6?allocation+0x800+32*i:0);}
 std::array<uint8_t,0x3c+6*40> prefix;std::memcpy(prefix.data(),base+menu,prefix.size());
 const auto quit_row=menu+0x3c+6*40;Check(motogp::quit::AppendItem(base,menu,label),"Append seventh option");
 Check(std::memcmp(prefix.data(),base+menu,prefix.size())==0,"Original six rows and IDs unchanged");
 Check(read(quit_row+28)==label&&read(quit_row+40+28)==0,"New native label and terminator");
 Check(motogp::quit::AppendItem(base,menu,label),"Append is idempotent");
 const auto list=allocation+0x900,descriptor=allocation+0xA00;write(list,menu+0x3c);
 for(unsigned i=0;i<7;++i)Check(motogp::quit::IsWhiteAccentDraw(base,menu,list,i,0x8228380C)==(i==6),"Only added tile selects white accent in actual list-render call");
 Check(motogp::quit::IsWhiteAccentDraw(base,menu,list,6,0x82283868),"Both native strip passes select white");
 Check(!motogp::quit::IsWhiteAccentDraw(base,menu,list,6,0x12345678),"Other mesh callers stay unchanged");
 write(list,allocation);Check(!motogp::quit::IsWhiteAccentDraw(base,menu,list,6,0x8228380C),"Other menu arrays stay unchanged");write(list,menu+0x3c);
 for(unsigned i=0;i<7;++i)Check(motogp::quit::IsQuitLabelDraw(base,menu,menu+0x3c,i,0x82283540)==(i==6),"Only added label scopes cap recoloring");
 Check(!motogp::quit::IsQuitLabelDraw(base,menu,allocation+0x800,6,0x82283540),"Unrelated label arrays stay original");
 Check(!motogp::quit::IsQuitLabelDraw(base,menu,menu+0x3c,6,0x12345678),"Unrelated label render callers stay original");
 Check(motogp::quit::IsWhiteCapPass(0x82280334)&&motogp::quit::IsWhiteCapPass(0x822803A8),"Both colored cap layers select white");
 Check(!motogp::quit::IsWhiteCapPass(0x822802FC)&&!motogp::quit::IsWhiteCapPass(0x82280370),"Gray background passes remain untouched");
 Check(read(0x821038A8+32)==0x82379D38,"Original type-7 strip material resolves to scoped native draw hook");
 rex::graphics::xenos::xe_gpu_texture_fetch_t fetch{};fetch.type=rex::graphics::xenos::FetchConstantType::kTexture;fetch.format=rex::graphics::xenos::TextureFormat::k_DXT4_5;fetch.base_address=0x12345;fetch.swizzle=0x688;fetch.tiled=1;fetch.size_2d.width=1023;fetch.size_2d.height=127;
 for(unsigned i=0;i<6;++i)write(descriptor+28+4*i,reinterpret_cast<uint32_t*>(&fetch)[i]);
 std::array<uint8_t,52> original_descriptor;std::memcpy(original_descriptor.data(),base+descriptor,52);
 const auto white=motogp::quit::WhiteAccentDescriptor(base,descriptor);Check(white&&white!=descriptor,"White view gets a stable separate native texture descriptor");
 rex::graphics::xenos::xe_gpu_texture_fetch_t white_fetch{};for(unsigned i=0;i<6;++i)reinterpret_cast<uint32_t*>(&white_fetch)[i]=read(white+28+4*i);
 Check((white_fetch.swizzle&0x1ff)==(2|(2<<3)|(2<<6))&&(white_fetch.swizzle&0xe00)==(fetch.swizzle&0xe00),"White view preserves original blue brightness layers and alpha channel");
 white_fetch.swizzle=fetch.swizzle;Check(std::memcmp(&white_fetch,&fetch,sizeof(fetch))==0,"Original pixels, pitch, tiling, size, endian and sampler descriptors remain exact");
 Check(std::memcmp(original_descriptor.data(),base+descriptor,52)==0&&std::memcmp(base+white,base+descriptor,28)==0,"Shared blue descriptor and native resource header remain unchanged");
 Check(motogp::quit::WhiteAccentDescriptor(base,descriptor)==white,"Repeated frames reuse the same white view");
 auto move=[&](int selection,int delta){write(screen+0x18,selection);PPCContext ctx{};ctx.r1.u64=stack;ctx.lr=0x12345678;ctx.r30.u64=0x1122334455667788;ctx.r31.u64=0x8877665544332211;ctx.r3.u32=menu+0x3c;ctx.r4.u32=screen+0x18;ctx.r5.s64=delta;ctx.r6.u64=0;sub_8227F318(ctx,base);Check(ctx.r1.u32==stack&&ctx.lr==0x12345678&&ctx.r30.u64==0x1122334455667788&&ctx.r31.u64==0x8877665544332211,"Original menu navigation preserves ABI");return read(screen+0x18);};
 Check(move(5,1)==6,"Original navigation reaches Quit game");Check(move(6,1)==0,"Original navigation wraps after seventh item");Check(move(0,-1)==6,"Original navigation wraps back to Quit game");
 write(screen+0x18,6);
 for(uint32_t event:{0u,0x40u,0x400u,0x2000u,0x800u})Check(motogp::quit::IsQuitSelection(base,menu,screen,event)==bool(event&motogp::quit::kAcceptEvents),"Only native accept events activate Quit game");
 base[menu+0x244]=1;Check(!motogp::quit::IsQuitSelection(base,menu,screen,0x400),"Profile/loading transition blocks quitting");base[menu+0x244]=0;
 base[menu+0x245]=1;Check(!motogp::quit::IsQuitSelection(base,menu,screen,0x400),"Second native transition blocks quitting");base[menu+0x245]=0;
 for(unsigned i=0;i<6;++i){write(screen+0x18,i);Check(!motogp::quit::IsQuitSelection(base,menu,screen,0x400),"Original choices stay original choices");}write(screen+0x18,6);
 write(menu,0);Check(!motogp::quit::AppendItem(base,menu,label),"Unknown menu layout untouched");write(menu,motogp::quit::kMainMenuVtable);
 pad->state.gamepad.buttons=X_INPUT_GAMEPAD_A;PPCContext ctx{};ctx.r1.u64=stack;ctx.lr=0x12345678;ctx.r3.u32=menu;ctx.r4.u32=screen;ctx.r5.u32=0x400;ctx.r23.u64=0xAABBCCDD;ctx.r31.u64=0xDEADBEEF;
 sub_821FCCA8(ctx,base);Check(motogp::quit::Pending()&&ctx.r3.s32==-1,"Actual MainMenu hook consumes new action");Check(ctx.r1.u32==stack&&ctx.lr==0x12345678&&ctx.r23.u64==0xAABBCCDD&&ctx.r31.u64==0xDEADBEEF,"Hook preserves nonvolatile state");
 X_INPUT_STATE state{};auto* input=static_cast<InputSystem*>(runtime.input_system());input->GetState(0,&state);Check(state.gamepad.buttons==0,"Guest input is neutral while confirmation owns it");input->GetStateForUI(0,&state);Check(state.gamepad.buttons&X_INPUT_GAMEPAD_A,"UI can still read actual pad");
 ctx.r3.u32=menu;sub_821FCCA8(ctx,base);Check(ctx.r3.s32==-1,"Pending modal blocks original menu actions");
 // These observers verify native-service arguments and callback/input lifetime,
 // not the title's renderer or visual appearance. Native Windows review remains necessary.
 auto* dispatcher=runtime.function_dispatcher();
 auto* original_question=dispatcher->GetFunction(0x826EDB38);
 auto* original_open=dispatcher->GetFunction(0x82356E40);
 fixture_record=allocation+0x3000;fixture_question=allocation+0x3100;
 std::memcpy(base+fixture_question,"Are you sure?",14);
 std::memcpy(base+fixture_question+32,"Yes",4);std::memcpy(base+fixture_question+40,"No",3);
 const auto saved_popup=read(0x8336E274),saved_current=read(0x8336F4C0),saved_closing=read(0x8336F4E0);
 write(0x8336E274,allocation+0x3200);write(0x8336F4C0,0);write(0x8336F4E0,0);
 dispatcher->SetFunction(0x826EDB38,FixtureQuestion);dispatcher->SetFunction(0x82356E40,FixtureNativeOpen);
 int exits=0;motogp::quit::SetExitHandler([&]{++exits;});
 motogp::quit::TickConfirmation(ctx,base);
 Check(native_opens==0,"Held opening A cannot open and accept its own native prompt");
 pad->state.gamepad.buttons=0;motogp::quit::PollFrontendInput();motogp::quit::TickConfirmation(ctx,base);
 Check(native_opens==1&&motogp::quit::Pending(),"Released input opens the native confirmation exactly once");
 Check(read(fixture_record+8+28)==0&&rex::memory::load_and_swap<float>(base+fixture_record+8+48)==0.f,"Native quit starts on Yes with matching animation state");
 input->GetState(0,&state);pad->state.gamepad.buttons=X_INPUT_GAMEPAD_A;input->GetState(0,&state);
 Check(state.gamepad.buttons&X_INPUT_GAMEPAD_A,"Native dialog receives guest controls instead of an ImGui blocker");
 auto result=[&](int choice){PPCContext callback{};callback.r3.u32=fixture_record+8;callback.r4.s64=choice;dispatcher->GetFunction(read(fixture_record+4))(callback,base);};
 result(1);write(0x8336F4C0,0);
 Check(!motogp::quit::Pending()&&exits==0,"Native No returns to the menu without exiting");
 input->GetState(0,&state);Check(!(state.gamepad.buttons&X_INPUT_GAMEPAD_A),"Native dismissal masks held controls");
 pad->state.gamepad.buttons=0;motogp::quit::PollFrontendInput();motogp::quit::Request();
 write(0x8336F4E0,fixture_record);motogp::quit::TickConfirmation(ctx,base);
 Check(native_opens==1,"Another closing native popup is not replaced");
 write(0x8336F4E0,0);motogp::quit::TickConfirmation(ctx,base);
 Check(native_opens==2,"Native confirmation opens after the existing fade finishes");
 write(fixture_record+8+28,0);motogp::quit::SetFocused(false);motogp::quit::TickConfirmation(ctx,base);
 Check(read(fixture_record+8+28)==1,"Focus loss restores the safe native No selection");
 motogp::quit::SetFocused(true);pad->state.gamepad.buttons=X_INPUT_GAMEPAD_A;motogp::quit::PollFrontendInput();
 motogp::quit::TickConfirmation(ctx,base);input->GetState(0,&state);
 Check(!(state.gamepad.buttons&X_INPUT_GAMEPAD_A),"Held input on refocus cannot confirm a native exit");
 pad->state.gamepad.buttons=0;motogp::quit::PollFrontendInput();motogp::quit::TickConfirmation(ctx,base);
 const auto cookie=read(fixture_record+8+16);write(fixture_record+8+16,0);result(0);
 Check(exits==0&&motogp::quit::Pending(),"A stale or unrelated native result cannot exit");
 write(fixture_record+8+16,cookie);result(-2);write(0x8336F4C0,0);
 Check(!motogp::quit::Pending()&&exits==0,"Native Back cancels exit");
 // Keep boot/frontend coverage separate from the native-dialog observers.
 const uint32_t mode_ptr=0x8292710C,mode_value=0x82927110;
 const auto saved_ptr=read(mode_ptr),saved_value=read(mode_value),saved_boot=read(0x828DA060);
 write(mode_ptr,0);Check(!motogp::quit::IsTitleMode(base),"An uninitialized frontend property cannot identify title mode");
 write(mode_ptr,mode_value);write(mode_value,15);
 Check(motogp::quit::IsTitleMode(base),"Original frontend mode 15 identifies the secondary title");
 Check(!motogp::quit::ObserveTitleDraw(base,0,1.0),"An unrelated in-game Press Start draw cannot arm exit");
 Check(!motogp::quit::ObserveTitleDraw(base,1,.5,0x822AE94C),"Secondary title fade cannot arm exit");
 write(mode_value,3);Check(!motogp::quit::ObserveTitleDraw(base,1,1.0,0x822AE94C),"Other frontend modes cannot arm exit");
 write(mode_value,11);write(0x828DA060,3);
 Check(!motogp::quit::ObserveTitleDraw(base,0,0,0x8229CA24),"Frontend property 11 cannot impersonate the separate boot phase");
 write(mode_value,15);PPCContext phase{};phase.r1.u32=stack;phase.lr=0x12345678;phase.r3.u32=11;
 sub_821E4EE0(phase,base);
 Check(read(0x828DA060)==11&&read(mode_value)==15,"Original boot transition preserves the distinct frontend property");
 Check(read(0x8229CA18)==0x38800000&&read(0x8229CA20)==0x4BFFF759,"Original boot Press Start draw passes flag 0");
 write(mode_ptr,0);Check(motogp::quit::ObserveTitleDraw(base,0,0,0x8229CA24),"Boot title works before the frontend property exists");
 write(mode_ptr,mode_value);
 Check(!motogp::quit::ObserveTitleDraw(base,0,0,0x12345678),"Unknown flag-0 callers cannot arm title exit");
 rex::cvar::SetFlagByName("mnk_mode","true");rex::cvar::SetFlagByName("keybind_b","Quote,Backspace");
 pad->state.gamepad.buttons=0;motogp::quit::PollFrontendInput();
 Check(!motogp::quit::PollTitleInput(base,11),"A neutral poll preserves the observed boot draw for a later Back edge");
 rex::ui::KeyEvent backspace(nullptr,rex::ui::VirtualKey::kBack,1,false,false,false,false,false);
 keyboard->OnKeyDown(backspace);input->GetStateForUI(0,&state);
 Check(state.gamepad.buttons&X_INPUT_GAMEPAD_B,"Actual Backspace binding supplies native B");
 Check(read(0x8229C630)==0x3D00822B&&read(0x8229C640)==0x3868E9F8,"Boot installs the actual secondary frontend updater");
 sub_8229C2C0(ctx,base);Check(motogp::quit::Pending(),"Boot update consumes fresh Back and requests native confirmation");
 const auto before_title=native_opens;motogp::quit::TickConfirmation(ctx,base);
 Check(native_opens==before_title,"Held title Back cannot immediately dismiss the new native prompt");
 keyboard->OnKeyUp(backspace);motogp::quit::PollFrontendInput();motogp::quit::TickConfirmation(ctx,base);
 Check(native_opens==before_title+1,"Releasing title Back opens the native confirmation");
 result(-2);write(0x8336F4C0,0);
 Check(!motogp::quit::Pending()&&exits==0,"Native cancellation returns to boot without exiting");
 write(mode_ptr,saved_ptr);write(mode_value,saved_value);write(0x828DA060,saved_boot);
 pad->state.gamepad.buttons=0;motogp::quit::PollFrontendInput();motogp::quit::Request();motogp::quit::TickConfirmation(ctx,base);
 result(0);result(0);Check(exits==1,"Native Yes schedules the existing exit handler only once");
 dispatcher->SetFunction(0x826EDB38,original_question);dispatcher->SetFunction(0x82356E40,original_open);
 write(0x8336E274,saved_popup);write(0x8336F4C0,saved_current);write(0x8336F4E0,saved_closing);
 motogp::quit::Stop();memory->SystemHeapFree(allocation);
 rex::cvar::SetFlagByName("game_text_language","turkish");motogp::quit::Initialize(&runtime);
 Check(std::string(reinterpret_cast<char*>(base+motogp::quit::LabelAddress()))=="OYUNDAN ÇIK","Native added menu label follows Turkish written language");
 motogp::quit::Stop();
 std::cout<<checks<<" native quit menu, original navigation, ABI, confirmation and input checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
