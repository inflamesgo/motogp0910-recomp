#include "discord_presence.h"
#include "discord/activity.h"
#include "discord/client.h"
#include "discord/update_policy.h"
#include "discord/launch.h"
#include <rex/cvar.h>
#include <rex/memory.h>
#include <rex/runtime.h>
#include <rex/logging.h>
#include <chrono>
#include <mutex>

REXCVAR_DEFINE_BOOL(discord_presence_enabled,true,"Discord","Show game activity in the Discord desktop app on the next launch");
namespace motogp::discord {
namespace {
Client client;
std::mutex capture_mutex;
std::atomic<bool> enabled{false};
rex::Runtime* guest_runtime = nullptr;
LobbyProvider lobby_provider = nullptr;
SettingsProvider settings_provider = nullptr;
Menu menu_context=Menu::Unknown;
std::chrono::steady_clock::time_point next_capture{};
std::optional<uint32_t> Read(uint8_t* base, uint32_t address) {
  if (!address || (address & 3) || uint64_t(address) + 4 > 0x100000000ull) return {};
  auto* heap = guest_runtime->memory()->LookupHeap(address);
  rex::memory::HeapAllocationInfo info{};
  if (!heap || !heap->QueryRegionInfo(address, &info) ||
      !(info.state & rex::memory::kMemoryAllocationCommit) ||
      !(info.protect & rex::memory::kMemoryProtectRead) ||
      uint64_t(address) + 4 > uint64_t(info.base_address) + info.region_size) return {};
  return rex::memory::load_and_swap<uint32_t>(base + address);
}
}
void Start(rex::Runtime* runtime, LobbyProvider lobby,SettingsProvider settings,JoinHandler join) {
  Stop();
  if (!runtime || !rex::cvar::Query<bool>("discord_presence_enabled")) return;
#ifdef _WIN32
  if(join&&!RegisterLaunchCommand())REXLOG_WARN("DISCORD_INVITES: automatic game launch registration unavailable; open the game before accepting an invitation");
#endif
  { const std::lock_guard lock(capture_mutex); guest_runtime = runtime; lobby_provider = lobby; settings_provider=settings; menu_context=Menu::Unknown; next_capture = {}; }
  client.Publish(Present({}));
  client.Start(join);
  enabled.store(true, std::memory_order_release);
}
void Stop() {
  enabled.store(false, std::memory_order_release);
  { const std::lock_guard lock(capture_mutex); guest_runtime = nullptr; lobby_provider = nullptr; settings_provider=nullptr; menu_context=Menu::Unknown; }
  client.Stop();
}
void ObserveGuestFrame(uint8_t* base) {
  if (!enabled.load(std::memory_order_acquire) || !base) return;
  const std::unique_lock lock(capture_mutex, std::try_to_lock);
  if (!lock || !guest_runtime) return;
  const auto now = std::chrono::steady_clock::now();
  if (now < next_capture) return;
  next_capture = now + kCaptureInterval;
  auto snapshot = Capture([&](uint32_t address) { return Read(base, address); },menu_context);
  menu_context=snapshot.context;
  snapshot.network_lan=rex::cvar::Query<std::string>("network_mode")=="lan";
  std::optional<Lobby> lobby;
  if (!snapshot.event && CanShowLobby(snapshot.menu) && lobby_provider) {
    lobby=lobby_provider();
    if (lobby) {
      snapshot.lobby = true; snapshot.online = lobby->online;
      snapshot.members = lobby->members; snapshot.capacity = lobby->capacity;
    }
  }
  if(settings_provider&&settings_provider()) {
    snapshot.menu=Menu::Settings;snapshot.event=false;snapshot.lobby=false;
  }
  auto activity=Present(snapshot);
  if(snapshot.lobby&&snapshot.online&&lobby){activity.party_id=lobby->party_id;activity.join_secret=lobby->join_secret;}
  client.Publish(std::move(activity));
}
} // namespace motogp::discord
