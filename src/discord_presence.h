#pragma once
#include <cstdint>
#include <optional>
namespace rex { class Runtime; }
namespace motogp::discord {
struct Lobby { bool online; uint32_t members, capacity; };
using LobbyProvider = std::optional<Lobby>(*)();
using SettingsProvider = bool(*)();
void Start(rex::Runtime* runtime, LobbyProvider lobby = nullptr,SettingsProvider settings=nullptr);
void Stop();
void ObserveGuestFrame(uint8_t* base);
}
