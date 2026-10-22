#pragma once
#include <cstdint>
#include <optional>
#include <iterator>
#include <string>
#include <string_view>

namespace motogp::discord {
// Public application ID; never a bot token or client secret.
inline constexpr char kApplicationId[] = "1562961932061904906";
struct Snapshot {
  uint32_t frontend = 0;
  bool event = false;
  uint32_t mode = 0, track = UINT32_MAX, season = UINT32_MAX, bike_class = UINT32_MAX;
  bool lobby = false, online = false;
  uint32_t members = 0, capacity = 0;
  bool operator==(const Snapshot&) const = default;
};
// Read only on the guest frame thread. The supplied reader validates each word.
template<class Reader> Snapshot Capture(Reader read) {
  Snapshot s;
  s.frontend = read(0x828DA060).value_or(0);
  const auto race = read(0x82942D60).value_or(0);
  if (!race || !read(0x82942DB0).value_or(0)) return s;
  const auto property = [&](uint32_t object, uint32_t offset) -> std::optional<uint32_t> {
    if (!object || uint64_t(object) + offset + 4 > 0x100000000ull) return {};
    const auto pointer = read(object + offset).value_or(0);
    return pointer ? read(pointer) : std::optional<uint32_t>{};
  };
  if (uint64_t(race) + 0x44 > 0x100000000ull) return s;
  const auto config = read(race + 0x40).value_or(0);
  const auto mode = property(config, 0x0C);
  if (!mode || !*mode || *mode > 16) return s;
  s.event = true;
  s.mode = *mode;
  s.track = read(0x82926FBC).value_or(UINT32_MAX);
  s.season = property(config, 0x244).value_or(UINT32_MAX);
  // Primary local user's frontend profile selection, not the Xbox bike enum.
  const auto manager = read(0x834F9494).value_or(0);
  if (manager && uint64_t(manager) + 0x80 <= 0x100000000ull) {
    const auto user = read(manager + 0x18).value_or(UINT32_MAX);
    const auto slot = user < 4 ? read(manager + 0x70 + user * 4).value_or(UINT32_MAX) : UINT32_MAX;
    if (slot < 4) s.bike_class = read(0x82AEC2A8 + slot * 0x1EED44 + 0xF75A4).value_or(UINT32_MAX);
  }
  return s;
}
inline std::string_view Track(uint32_t slot) {
  static constexpr std::string_view names[] = {
    "Losail International Circuit", "Twin Ring Motegi", "Jerez", "Le Mans", "Mugello",
    "Catalunya", "TT Circuit Assen", "Laguna Seca", "Sachsenring", "Donington Park",
    "Brno", "Indianapolis", "Misano", "Estoril", "Phillip Island", "Sepang", "Valencia",
    "", "Silverstone", "Shanghai", "Istanbul Park"};
  return slot < std::size(names) ? names[slot] : std::string_view{};
}
inline std::string_view Mode(const Snapshot& s) {
  if (s.event) {
    switch (s.mode) {
      case 8: return "Training";
      case 9: return "Tutorial";
      case 10: return "Time Trial";
      case 11: return "Split Screen";
      case 12: case 14: return "Online";
      case 13: return "LAN";
      case 16: return "Replay";
    }
  }
  switch (s.frontend) {
    case 1: return "Quick Race";
    case 2: return "Arcade";
    case 3: case 10: return "Career";
    case 4: return "Championship";
    case 5: return "Time Trial";
    case 6: return "Tutorial";
    case 7: return "Challenge";
    case 8: return "Replay";
    case 9: return "Split Screen";
  }
  if (s.event) {
    switch (s.mode) {
      case 1: return "Quick Race";
      case 2: return "Arcade";
      case 3: case 4: case 5: return "Grand Prix";
      case 6: return "Career";
      case 7: return "Preview";
      case 11: return "Split Screen";
      case 15: return "Demo";
    }
  }
  return "Main menu";
}
struct Activity {
  std::string details, state;
  bool operator==(const Activity&) const = default;
};
inline Activity Present(const Snapshot& s) {
  Activity a{std::string(Mode(s)), {}};
  if (!s.event) {
    if (s.lobby) {
      a.details = s.online ? "Online lobby" : "LAN lobby";
      if (s.capacity && s.members <= s.capacity)
        a.state = std::to_string(s.members) + " / " + std::to_string(s.capacity) + " riders";
      return a;
    }
    a.state = a.details == "Main menu" ? "MotoGP 09/10" : "In menus";
    return a;
  }
  if (const auto track = Track(s.track); !track.empty()) a.details += " - " + std::string(track);
  switch (s.bike_class) {
    case 0: a.state = "MotoGP"; break;
    case 1: a.state = "125cc"; break;
    case 2: a.state = s.season == 1 ? "Moto2" : "250cc"; break;
    case 3: a.state = "Moto2"; break;
  }
  if (s.season <= 1) {
    if (!a.state.empty()) a.state += " - ";
    a.state += s.season == 0 ? "2009" : "2010";
  }
  if (s.mode >= 3 && s.mode <= 5) {
    if (!a.state.empty()) a.state += " - ";
    a.state += s.mode == 3 ? "Practice" : s.mode == 4 ? "Qualifying" : "Race";
  }
  return a;
}
} // namespace motogp::discord
