#include "../src/discord/activity.h"
#include "../src/discord/rpc.h"
#include "../src/discord/client.h"
#include <chrono>
#include <iostream>
#include <map>
#include <stdexcept>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include "../src/launcher/core.h"
#endif
using namespace motogp::discord;
using namespace std::chrono_literals;
void Check(bool condition, const char* message) {
  if (!condition) throw std::runtime_error(message);
}
void Model() {
  std::map<uint32_t, uint32_t> words;
  const auto read = [&](uint32_t address) -> std::optional<uint32_t> {
    const auto it = words.find(address);
    return it == words.end() ? std::nullopt : std::optional(it->second);
  };
  Check(Present(Capture(read)).details == "Main menu", "Uninitialized memory must show menus");
  words = {{0x828DA060, 2}, {0x82942D60, 0x90000000}, {0x82942DB0, 1},
      {0x90000040, 0x90001000}, {0x9000100C, 0x90002000}, {0x90002000, 2},
      {0x90001244, 0x90002004}, {0x90002004, 0}, {0x82926FBC, 0},
      {0x834F9494, 0x90003000}, {0x90003018, 0}, {0x90003070, 0},
      {0x82AEC2A8 + 0xF75A4, 0}};
  auto snapshot = Capture(read);
  Check(snapshot.event && snapshot.mode == 2 && snapshot.bike_class == 0, "Decode active event and class through validated pointers");
  Check(Present(snapshot) == Activity{"Arcade - Losail International Circuit", "MotoGP - 2009"}, "Arcade details");
  words[0x828DA060] = 3; words[0x90002000] = 4;
  Check(Present(Capture(read)) == Activity{"Career - Losail International Circuit", "MotoGP - 2009 - Qualifying"}, "Career retains mode during qualifying");
  words[0x828DA060] = 4; words[0x90002000] = 5;
  Check(Present(Capture(read)).details.starts_with("Championship"), "Championship is distinct from Career");
  words[0x90002000] = 10;
  Check(Present(Capture(read)).details.starts_with("Time Trial"), "Actual event overrides stale frontend selection");
  words[0x90002000] = 14;
  Check(Present(Capture(read)).details.starts_with("Online"), "Online event");
  words[0x90002000] = 13;
  Check(Present(Capture(read)).details.starts_with("LAN"), "LAN event");
  words[0x90002004] = 1; words[0x82AEC2A8 + 0xF75A4] = 2;
  Check(Present(Capture(read)).state == "Moto2 - 2010", "2010 class normalization");
  words[0x82942DB0] = 0;
  Check(Present(Capture(read)) == Activity{"Championship", "In menus"}, "Returning to menus clears circuit, class and season");
  words[0x82942DB0] = 1; words[0x82942D60] = 0xFFFFFFFC;
  Check(!Capture(read).event, "Overflowing native pointers rejected");
  words[0x82942D60] = 0x90000000; words[0x9000100C] = 0xDEADBEEF;
  Check(!Capture(read).event, "Missing mode pointer rejected");
  Snapshot invalid{0, true, 10, 17, 99, 99};
  Check(Present(invalid) == Activity{"Time Trial", ""}, "Reserved track and invalid metadata never fabricated");
  invalid.track = 20; Check(Present(invalid).details.ends_with("Istanbul Park"), "DLC circuit");
  Snapshot lobby; lobby.lobby = true; lobby.online = true; lobby.members = 3; lobby.capacity = 12;
  Check(Present(lobby) == Activity{"Online lobby", "3 / 12 riders"}, "Lobby uses room counts without sharing names");
  lobby.event = true; lobby.mode = 2; lobby.frontend = 2;
  Check(Present(lobby).details == "Arcade", "Active event takes priority over a lingering room");
  auto command = Command({"Career", "MotoGP - 2010"}, 12345, 42, 7);
  Check(command["args"]["activity"]["timestamps"]["start"] == 12345 && command["nonce"] == "7", "RPC activity schema and seconds timestamp");
  Check(Clear(42, 8)["args"]["activity"].is_null(), "Explicit activity clearing");
  const auto frame = Frame(1, command.dump());
  Check(Word(frame.data()) == 1 && Word(frame.data() + 4) == frame.size() - 8, "Little-endian IPC frame");
#ifdef _WIN32
  using namespace launcher;
  Check(Settings::Load(toml::table{}).discord_presence_enabled, "Launcher default enabled");
  Settings settings; settings.discord_presence_enabled = false;
  toml::table config; Put(config, "custom_flag", int64_t(42)); settings.Store(config);
  Check(!Settings::Load(config).discord_presence_enabled && Get(config, "custom_flag", int64_t(0)) == 42, "Launcher toggle persists and unknown settings survive");
  const auto args = Arguments(L"C:\\game", settings, config);
  Check(std::count(args.begin(), args.end(), L"--discord_presence_enabled=false") == 1, "Launcher forwards choice once");
#endif
}
#ifdef _WIN32
struct Server {
  HANDLE pipe = INVALID_HANDLE_VALUE;
  Server() {
    pipe = CreateNamedPipeW(L"\\\\.\\pipe\\discord-ipc-0", PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED | FILE_FLAG_FIRST_PIPE_INSTANCE,
        PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT, 1, 65536, 65536, 0, nullptr);
    Check(pipe != INVALID_HANDLE_VALUE, "Create isolated mock Discord pipe");
  }
  ~Server() { if (pipe != INVALID_HANDLE_VALUE) CloseHandle(pipe); }
  void Transfer(bool write, char* bytes, DWORD size) {
    OVERLAPPED op{}; op.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    DWORD count = 0;
    bool ok = write ? WriteFile(pipe, bytes, size, &count, &op) : ReadFile(pipe, bytes, size, &count, &op);
    if (!ok && GetLastError() == ERROR_IO_PENDING) {
      if (WaitForSingleObject(op.hEvent, 7000) != WAIT_OBJECT_0) {
        CancelIoEx(pipe, &op); GetOverlappedResult(pipe, &op, &count, TRUE);
      } else ok = GetOverlappedResult(pipe, &op, &count, FALSE);
    }
    CloseHandle(op.hEvent); Check(ok && count == size, "Mock pipe transfer within deadline");
  }
  void Connect() {
    OVERLAPPED op{}; op.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    bool ok = ConnectNamedPipe(pipe, &op);
    const auto error = GetLastError();
    if (!ok && error == ERROR_PIPE_CONNECTED) ok = true;
    else if (!ok && error == ERROR_IO_PENDING) {
      DWORD count = 0;
      if (WaitForSingleObject(op.hEvent, 3000) != WAIT_OBJECT_0) {
        CancelIoEx(pipe, &op); GetOverlappedResult(pipe, &op, &count, TRUE);
      } else ok = GetOverlappedResult(pipe, &op, &count, FALSE);
    }
    CloseHandle(op.hEvent); Check(ok, "Mock pipe client connection within deadline");
  }
  std::pair<uint32_t, Json> Read() {
    char header[8]; Transfer(false, header, 8);
    const auto size = Word(header + 4); Check(size <= kMaxFrame, "Bounded IPC payload");
    std::string payload(size, '\0'); Transfer(false, payload.data(), size);
    return {Word(header), Json::parse(payload)};
  }
  void Send(uint32_t opcode, const Json& value, bool fragmented = false) {
    auto bytes = Frame(opcode, value.dump());
    if (fragmented) { Transfer(true, bytes.data(), 3); bytes.erase(0, 3); }
    Transfer(true, bytes.data(), static_cast<DWORD>(bytes.size()));
  }
};
void Ipc() {
  // Construct client first so the server handle closes before a failing client's destructor joins.
  Client client;
  Server server;
  client.Start();
  server.Connect();
  const auto [opcode, handshake] = server.Read();
  Check(opcode == 0 && handshake["v"] == 1 && handshake["client_id"] == kApplicationId, "Real pipe handshake");
  std::this_thread::sleep_for(200ms);
  DWORD available = 0; Check(PeekNamedPipe(server.pipe, nullptr, 0, nullptr, &available, nullptr) && available == 0, "Wait for READY before publishing");
  server.Send(1, Json{{"cmd", "DISPATCH"}, {"evt", "READY"}}, true);
  auto initial = server.Read();
  Check(initial.first == 1 && initial.second["args"]["activity"]["details"] == "Main menu", "Publish initial activity after fragmented READY");
  server.Send(3, Json{{"ping", "test"}});
  const auto pong = server.Read(); Check(pong.first == 4 && pong.second["ping"] == "test", "PING/PONG");
  client.Publish({"Career - Brno", "MotoGP - 2010 - Race"});
  const auto career = server.Read();
  Check(career.second["args"]["activity"]["details"] == "Career - Brno", "Publish changed activity after rate limit");
  client.Stop();
  const auto clear = server.Read(); Check(clear.second["args"]["activity"].is_null(), "Shutdown clears activity");
  client.Start(); // reconnect to an absent reader must still stop immediately.
  const auto start = std::chrono::steady_clock::now(); client.Stop();
  Check(std::chrono::steady_clock::now() - start < 1s, "Bounded shutdown with Discord absent");
}
#endif
int main() {
  try {
    Model();
#ifdef _WIN32
    Ipc();
#endif
    std::cout << "Discord presence model, protocol and available platform checks passed\n";
    return 0;
  } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
