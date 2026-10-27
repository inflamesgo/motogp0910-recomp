#pragma once
#include <cstdint>
#include <optional>
#include <string>
namespace rex { class Runtime; }
namespace motogp::discord {
struct Lobby { bool online; uint32_t members, capacity; std::string party_id={},join_secret={}; };
using LobbyProvider = std::optional<Lobby>(*)();
using SettingsProvider = bool(*)();
using JoinHandler=bool(*)(const std::string&);
void Start(rex::Runtime* runtime, LobbyProvider lobby = nullptr,SettingsProvider settings=nullptr,JoinHandler join=nullptr);
void Stop();
void ObserveGuestFrame(uint8_t* base);
}
