#include "leaderboard_scope_capture.h"
#include <rex/cvar.h>
#include <rex/memory.h>
namespace motogp::online::scope_capture {
namespace {
thread_local std::optional<bool> standalone_read;
thread_local std::optional<categories::Scope> standalone_setup;
}
bool IsStandaloneBoard(const uint8_t* base,uint32_t board) {
  // Native standalone model embeds its shared board at +32. Track/race board
  // constructors share code but do not have this enclosing model's vtable.
  return base&&board>=32&&rex::memory::load_and_swap<uint32_t>(base+board-32)==0x82036F14;
}
std::optional<categories::Scope> Resolve(uint32_t view,bool standalone,const uint8_t* base) {
  if(standalone) {
    const auto scope=categories::Current();
    return scope&&categories::Matches(*scope,view)?scope:std::nullopt;
  }
  if(!base)return {};
  return categories::ForView(view,rex::memory::load_and_swap<uint32_t>(base+0x82926FC0));
}
std::optional<categories::Scope> ResolveForRead(uint32_t view,const uint8_t* base) {
  return Resolve(view,standalone_read.value_or(false),base);
}
ReadScope::ReadScope(const uint8_t* base,uint32_t board):previous_(standalone_read) {
  standalone_read=IsStandaloneBoard(base,board);
}
ReadScope::~ReadScope(){standalone_read=previous_;}
NativeSetupScope::NativeSetupScope(const uint8_t* base,uint32_t board):previous_(standalone_setup) {
  standalone_setup=rex::cvar::Query<std::string>("network_mode")=="online"&&IsStandaloneBoard(base,board)?
      categories::Current():std::nullopt;
}
NativeSetupScope::~NativeSetupScope(){standalone_setup=previous_;}
std::optional<uint32_t> SetupSeason(uint32_t native_slot) {
  if(!standalone_setup||!categories::Valid(*standalone_setup)||
      native_slot!=categories::NativeSlot(standalone_setup->category))return {};
  return standalone_setup->year-2009;
}
}
