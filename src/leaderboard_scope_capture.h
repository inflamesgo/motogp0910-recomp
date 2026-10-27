#pragma once
#include "leaderboard_categories.h"
namespace motogp::online::scope_capture {
bool IsStandaloneBoard(const uint8_t* base,uint32_t board);
std::optional<categories::Scope> Resolve(uint32_t view,bool standalone,const uint8_t* base);
std::optional<categories::Scope> ResolveForRead(uint32_t view,const uint8_t* base);
class ReadScope {
  std::optional<bool> previous_;
 public:
  ReadScope(const uint8_t* base,uint32_t board);
  ~ReadScope();
};
class NativeSetupScope {
  std::optional<categories::Scope> previous_;
 public:
  NativeSetupScope(const uint8_t* base,uint32_t board);
  ~NativeSetupScope();
};
std::optional<uint32_t> SetupSeason(uint32_t native_slot);
}
