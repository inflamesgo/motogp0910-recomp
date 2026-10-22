#pragma once
#include "activity.h"
#include "../../third_party/nlohmann/json.hpp"
#include <cstring>
#include <vector>

namespace motogp::discord {
using Json = nlohmann::json;
inline constexpr uint32_t kMaxFrame = 64 * 1024;
inline std::string Frame(uint32_t opcode, const std::string& payload) {
  std::string result(8, '\0');
  const auto size = static_cast<uint32_t>(payload.size());
  for (int i = 0; i < 4; ++i) {
    result[i] = char(opcode >> (i * 8));
    result[i + 4] = char(size >> (i * 8));
  }
  result += payload;
  return result;
}
inline uint32_t Word(const char* bytes) {
  uint32_t result = 0;
  for (int i = 0; i < 4; ++i) result |= uint32_t(static_cast<unsigned char>(bytes[i])) << (i * 8);
  return result;
}
inline Json Command(const Activity& a, int64_t started, uint32_t pid, uint64_t nonce) {
  Json activity = {{"details", a.details}, {"timestamps", {{"start", started}}}, {"instance", false}};
  if (!a.state.empty()) activity["state"] = a.state;
  return {{"cmd", "SET_ACTIVITY"}, {"args", {{"pid", pid}, {"activity", activity}}}, {"nonce", std::to_string(nonce)}};
}
inline Json Clear(uint32_t pid, uint64_t nonce) {
  return {{"cmd", "SET_ACTIVITY"}, {"args", {{"pid", pid}, {"activity", nullptr}}}, {"nonce", std::to_string(nonce)}};
}
} // namespace motogp::discord
