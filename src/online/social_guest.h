#pragma once
#include <cstdint>
#include <optional>
namespace motogp::lan {class GuestBridge;}
namespace motogp::online::social {
std::optional<uint32_t> GuestMessage(lan::GuestBridge&,uint32_t message,uint32_t arguments);
std::optional<uint32_t> FriendEnumeratorSize(lan::GuestBridge&,uint32_t handle);
}
