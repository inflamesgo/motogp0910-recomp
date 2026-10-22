#include "discord_presence.h"
#include "discord/activity.h"
#include "discord/client.h"
#include <rex/cvar.h>
#include <rex/memory.h>
#include <rex/runtime.h>
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
void Start(rex::Runtime* runtime, LobbyProvider lobby) {
  Stop();
  if (!runtime || !rex::cvar::Query<bool>("discord_presence_enabled")) return;
  { const std::lock_guard lock(capture_mutex); guest_runtime = runtime; lobby_provider = lobby; next_capture = {}; }
  client.Publish(Present({}));
  client.Start();
  enabled.store(true, std::memory_order_release);
}
void Stop() {
  enabled.store(false, std::memory_order_release);
  { const std::lock_guard lock(capture_mutex); guest_runtime = nullptr; lobby_provider = nullptr; }
  client.Stop();
}
void ObserveGuestFrame(uint8_t* base) {
  if (!enabled.load(std::memory_order_acquire) || !base) return;
  const std::unique_lock lock(capture_mutex, std::try_to_lock);
  if (!lock || !guest_runtime) return;
  const auto now = std::chrono::steady_clock::now();
  if (now < next_capture) return;
  next_capture = now + std::chrono::milliseconds(500);
  auto snapshot = Capture([&](uint32_t address) { return Read(base, address); });
  if (!snapshot.event && lobby_provider) {
    if (const auto lobby = lobby_provider()) {
      snapshot.lobby = true; snapshot.online = lobby->online;
      snapshot.members = lobby->members; snapshot.capacity = lobby->capacity;
    }
  }
  client.Publish(Present(snapshot));
}
} // namespace motogp::discord
