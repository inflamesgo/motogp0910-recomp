#pragma once
#include <cstdint>
#include <optional>
namespace rex { class Runtime; }
namespace motogp::discord {
struct Lobby { bool online; uint32_t members, capacity; };
using LobbyProvider = std::optional<Lobby>(*)();
void Start(rex::Runtime* runtime, LobbyProvider lobby = nullptr);
void Stop();
void ObserveGuestFrame(uint8_t* base);
}
