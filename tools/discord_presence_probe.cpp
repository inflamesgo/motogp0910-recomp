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
  const auto screen=[&](uint32_t vtable) {
    words[0x8336F4B8]=vtable?0x9000A000:0;
    words[0x9000A000]=0x9000A100;words[0x9000A100]=vtable;
  };
  Check(Present(Capture(read)).details == "Main menu", "Uninitialized memory must show menus");
  words = {{0x828DA060, 2}, {0x82942D60, 0x90000000}, {0x82942DB0, 1},
      {0x90000040, 0x90001000}, {0x9000100C, 0x90002000}, {0x90002000, 2},
      {0x90001244, 0x90002004}, {0x90002004, 0}, {0x82926FBC, 0},
      {0x834F9494, 0x90003000}, {0x90003018, 0}, {0x90003070, 0},
      {0x82AEC2A8 + 0xF75A4, 0}};
  auto snapshot = Capture(read);
  Check(snapshot.event && snapshot.mode == 2 && snapshot.bike_class == 0, "Decode active event and class through validated pointers");
  Check(Present(snapshot).details=="Arcade"&&Present(snapshot).state=="MotoGP - 2009", "Missing rider metadata retains Arcade and class/year without a circuit-name fallback");
  words[0x828DA060] = 3; words[0x90002000] = 4;
  Check(Present(Capture(read)).details=="Career"&&Present(Capture(read)).state=="MotoGP - 2009 - Qualifying", "Career retains mode and qualifying fallback when rider/bike metadata is missing");
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
  screen(0x82042A14);
  const auto championship_menu=Present(Capture(read));
  Check(championship_menu.details=="Championship"&&championship_menu.state=="In menus"&&championship_menu.large_image=="motogp_logo"&&championship_menu.small_image=="mode_championship", "Returning to mode menus clears circuit, class and season while retaining the mode badge");
  screen(0);
  words[0x82942DB0] = 1; words[0x82942D60] = 0xFFFFFFFC;
  Check(!Capture(read).event, "Overflowing native pointers rejected");
  words[0x82942D60] = 0x90000000; words[0x9000100C] = 0xDEADBEEF;
  Check(!Capture(read).event, "Missing mode pointer rejected");
  Snapshot invalid{0, true, 10, 17, 99, 99};
  Check(Present(invalid).details=="Time Trial"&&Present(invalid).state.empty()&&Present(invalid).large_image=="motogp_logo"&&Present(invalid).small_image=="mode_time_trial", "Reserved track and invalid metadata never fabricated; known mode retains its badge");
  invalid.track = 20; Check(Present(invalid).details=="Time Trial"&&Present(invalid).large_text=="Istanbul Park", "DLC circuit name remains available on image hover, not the Time Trial heading");
  Snapshot lobby; lobby.lobby = true; lobby.online = true; lobby.members = 3; lobby.capacity = 12;
  const auto online_lobby=Present(lobby);
  const auto online_rpc=Command(online_lobby,12345,42,1)["args"]["activity"];
  Check(online_lobby.details=="Online lobby"&&online_lobby.state=="In lobby"&&online_lobby.small_image=="mode_online", "Online lobby uses custom status and network badge without a duplicate text counter");
  Check(online_rpc["party"]["size"]==Json::array({3,12})&&online_rpc["party"].size()==1&&!online_rpc.contains("secrets"), "Discord's native party counter uses actual lobby membership without room identifiers or join secrets");
  lobby.members=4;const auto joined=Present(lobby);
  Check(joined!=online_lobby&&joined.details==online_lobby.details&&joined.state==online_lobby.state&&Command(joined,12345,42,2)["args"]["activity"]["party"]["size"]==Json::array({4,12}), "A joining rider changes the published party size while retaining lobby status and activity context");
  lobby.members=3;Check(Present(lobby)==online_lobby,"A leaving rider restores the actual party count");
  for(const auto counts:{std::pair{0u,12u},std::pair{13u,12u},std::pair{1u,UINT32_MAX}}) {
    auto unknown=lobby;unknown.members=counts.first;unknown.capacity=counts.second;
    const auto activity=Present(unknown);
    Check(activity.state=="In lobby"&&!Command(activity,12345,42,1)["args"]["activity"].contains("party"),"Missing or inconsistent lobby membership omits the native counter without inventing a count");
  }
  auto malformed=online_lobby;malformed.party_size=13;
  Check(!Command(malformed,12345,42,1)["args"]["activity"].contains("party"),"RPC rejects inconsistent party metadata even from an independently constructed activity");
  lobby.online=false;lobby.frontend=5;
  Check(Present(lobby).small_image=="mode_lan"&&Present(lobby).small_text=="LAN"&&Command(Present(lobby),12345,42,1)["args"]["activity"]["party"]["size"]==Json::array({3,12}), "LAN lobby overrides a lingering Time Trial menu badge and uses the same actual native party counter");
  lobby.event = true; lobby.mode = 2; lobby.frontend = 2;
  Check(Present(lobby).details == "Arcade"&&!Command(Present(lobby),12345,42,1)["args"]["activity"].contains("party"), "Active event takes priority over a lingering room and removes lobby party metadata");
  Check(!Command(Present({}),12345,42,1)["args"]["activity"].contains("party"),"Leaving the lobby clears party metadata in the main menu");
  const auto text=[&](uint32_t address,std::string value) {
    value+='\0';
    for(size_t i=0;i<value.size();++i){auto& word=words[(address+uint32_t(i))&~3u];const auto shift=24-8*((address+i)&3);word=(word&~(0xFFu<<shift))|(uint32_t(uint8_t(value[i]))<<shift);}
  };
  words={{0x828DA060,5},{0x82942D60,0x90000000},{0x82942DB0,1},
      {0x90000040,0x90001000},{0x9000100C,0x90002000},{0x90002000,10},
      {0x90001244,0x90002004},{0x90002004,0},{0x82926FBC,0},
      {0x834F9494,0x90003000},{0x90003018,0},{0x90003070,0},
      {0x82AEC2A8+0xF75A4,0},{0x82AEC2A8+0xF75C4,0x90004000},
      {0x82AEC2A8+0xF75CC,0x90007000},{0x82AEC2A8+0xF75D0,0},
      {0x82946810,1},{0x82946814,0x90005000},{0x90005004,0x90006000},
      {0x829F3118,2},{0x829F311C,0x90007000},{0x90007004,0x90008000},{0x90007010,0x90008100},
      {0x90007000+44+4,0x90008200},{0x90007000+44+16,0x90008300}};
  text(0x90006000,"valentino_rossi");text(0x90008000,"Yamaha");text(0x90008100,"YZR-M1");
  text(0x90008200,"Honda");text(0x90008300,"RC212V");
  const auto selected=Capture(read);const auto racing=Present(selected);
  Check(selected.rider=="Valentino Rossi"&&selected.bike=="Yamaha YZR-M1","Decode selected rider key and real manufacturer/model strings");
  Check(racing.details=="Time Trial - Valentino Rossi"&&racing.state=="Yamaha YZR-M1","Visible heading shows mode and rider while the second row shows only the bike");
  Check(racing.large_image=="track_losail"&&racing.small_image=="mode_time_trial"&&racing.large_text=="Losail International Circuit - MotoGP - 2009","Track artwork, stopwatch badge and circuit/class/season hover text");
  const auto assets=Command(racing,12345,42,7)["args"]["activity"]["assets"];
  Check(assets["large_image"]=="track_losail"&&assets["small_image"]=="mode_time_trial"&&assets["small_text"]=="Time Trial","RPC serializes track artwork, mode badge and mode hover text");
  Check(!Command(Present({}),1,42,1)["args"]["activity"]["assets"].contains("small_image"),"Main menu uses the full game logo without a duplicate badge");
  for(const auto selection:{std::pair{2u,2u},std::pair{3u,4u},std::pair{4u,5u},std::pair{3u,10u}}) {
    auto context=selected;context.frontend=selection.first;context.mode=selection.second;
    const auto activity=Present(context);
    Check(activity.details==std::string(Mode(context))+" - Valentino Rossi"&&activity.state=="Yamaha YZR-M1"&&activity.large_image=="track_losail"&&activity.large_text.starts_with("Losail International Circuit"),"All four requested modes move the rider to the heading, retain bike on its own row, and keep the circuit in image hover");
    context.rider.clear();
    Check(Present(context).details==Mode(context)&&Present(context).state=="Yamaha YZR-M1","Missing rider data never substitutes a circuit or a fabricated rider name");
  }
  auto quick_race=selected;quick_race.frontend=1;quick_race.mode=1;
  Check(Present(quick_race).details=="Quick Race - Losail International Circuit"&&Present(quick_race).state=="Valentino Rossi - Yamaha YZR-M1","Quick Race retains its existing layout outside the four requested modes");
  struct BadgeCase {uint32_t frontend,mode;const char* key;const char* label;};
  for(const auto& entry:{BadgeCase{2,10,"mode_time_trial","Time Trial"},
      BadgeCase{4,5,"mode_championship","Championship"},BadgeCase{3,4,"mode_career","Career"},
      BadgeCase{2,2,"mode_arcade","Arcade"},BadgeCase{3,14,"mode_online","Online"},
      BadgeCase{1,1,"mode_quick_race","Quick Race"},BadgeCase{4,13,"mode_lan","LAN"}}) {
    Snapshot context;context.frontend=entry.frontend;context.event=true;context.mode=entry.mode;context.track=0;
    const auto activity=Present(context);
    Check(activity.large_image=="track_losail"&&activity.small_image==entry.key&&activity.small_text==entry.label,"Each requested mode has its own badge and matching hover label, using actual event overrides");
  }
  Snapshot unsupported;unsupported.frontend=6;
  Check(Present(unsupported).small_image.empty()&&Present(unsupported).small_text.empty(),"Unspecified modes do not invent a badge");
  size_t tracks=0;for(uint32_t slot=0;slot<21;++slot)if(!TrackAsset(slot).empty())++tracks;
  Check(tracks==20&&TrackAsset(17).empty()&&TrackAsset(UINT32_MAX).empty()&&TrackAsset(20)=="track_istanbul","All playable base/DLC tracks map to artwork; reserved/invalid tracks do not");
  words[0x828DA060]=3;words[0x90002000]=4;words[0x8291749C]=2;
  const uint32_t grid=0x8291E130;
  words[grid+1768]=0x90009900;words[grid+1764]=0;text(grid+1596,"Other player");
  words[grid+1824+1768]=0x90004000;words[grid+1824+1764]=1;text(grid+1824+1596,"Ayşe Deniz");
  const auto career_driver=Capture(read);const auto career_activity=Present(career_driver);
  Check(career_driver.rider=="Ayşe Deniz"&&career_driver.bike=="Honda RC212V","Career name and actual event bike come from the matching local grid entry, not the first/AI entry");
  Check(career_activity.details=="Career - Ayşe Deniz"&&career_activity.state=="Honda RC212V"&&career_activity.large_text.ends_with("Qualifying"),"Career shows the local custom rider in the heading, actual bike on the second row and session on image hover");
  text(grid+1824+1596,std::string("\xED\xA0\x80"));Check(Capture(read).rider.empty(),"Malformed native text is omitted instead of breaking JSON");
  words[0x82AEC2A8+0xF75C4]=0;Check(Capture(read).rider.empty()&&Capture(read).bike.empty(),"Missing Career identity never publishes another grid rider or a stale Arcade bike");
  words[0x90002000]=10;Check(Capture(read).rider=="Valentino Rossi","Time Trial overrides a stale Career frontend selection when choosing rider metadata");
  words[0x82AEC2A8+0xF75D0]=1;Check(Capture(read).rider.empty(),"Out-of-range rider IDs cannot fall back to a different rider");
  words[0x82942DB0]=0;screen(0x8201C054);const auto cleared=Present(Capture(read));
  Check(cleared.large_image=="motogp_logo"&&cleared.small_image=="mode_career"&&cleared.state=="In menus","Leaving a race clears rider, bike and circuit artwork while keeping the selected mode badge");
  words[0x828DA060]=0;screen(0x8200CF7C);const auto main_menu=Present(Capture(read));
  const auto main_assets=Command(main_menu,1,42,1)["args"]["activity"]["assets"];
  Check(main_menu.large_image=="motogp_logo"&&main_menu.small_image.empty()&&main_menu.small_text.empty()&&!main_assets.contains("small_image")&&!main_assets.contains("small_text"),"Returning from a badged event to the main menu removes both small image and hover fields");
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
void NavigationModel() {
  // Keep a complete stale Time Trial race and mode in memory throughout menu
  // navigation: screen selection, rather than clearing the old fixture, must win.
  std::map<uint32_t,uint32_t> words{{0x828DA060,5},{0x82942D60,0x90000000},
      {0x82942DB0,1},{0x90000040,0x90001000},{0x9000100C,0x90002000},
      {0x90002000,10},{0x82926FBC,0}};
  const auto read=[&](uint32_t at)->std::optional<uint32_t>{const auto it=words.find(at);return it==words.end()?std::nullopt:std::optional(it->second);};
  const auto screen=[&](uint32_t type,unsigned slot=2){
    const uint32_t record=0x90010000+slot*0x100,object=0x90020000+slot*0x100;
    words[0x8336F4B0+slot*4]=type?record:0;words[record]=object;words[object]=type;
  };
  Menu context=Menu::Unknown;
  const auto capture=[&](uint32_t type){screen(type);auto s=Capture(read,context);context=s.context;return s;};
  Check(Capture(read).event,"The stale-event fixture starts as a genuine Time Trial race");
  auto main=capture(0x8200CF7C);const auto main_activity=Present(main);
  Check(!main.event&&context==Menu::Main&&main_activity.details=="Main menu"&&main_activity.small_image.empty()&&main_activity.large_image=="motogp_logo","Returning to the actual main screen clears a stale Time Trial mode, race pointer, track image and badge");
  Check(Present(capture(0x8201789C)).details=="Arcade","First Arcade screen updates before season/rider selection despite an unchanged Time Trial frontend value");
  Check(Present(capture(0x82017374)).details=="Arcade","Arcade class selection remains Arcade");
  words[0x828DA060]=2;
  Check(Present(capture(0x82030184)).details=="Time Trial","First Time Trial class screen overrides a stale Arcade mode");
  Check(Present(capture(0x82040504)).details=="Time Trial","A shared season-selection screen inherits the verified Time Trial context");
  screen(0xDEADBEEF,6);screen(0x8200CF7C,3);
  words[0x8336F4D0]=0x90030000;words[0x90030000]=0x90030004;words[0x90030004]=0x8201789C;
  Check(Present(Capture(read,context)).details=="Time Trial","Unknown popups, the excluded slot 3 and the fading-out screen list cannot replace the current menu");
  screen(0,6);screen(0,3);
  Check(Present(capture(0x8203A0C4)).details=="Multiplayer","Multiplayer root is distinct from the prior single-player mode");
  for(auto frontend:{3u,2u}) {
    words[0x828DA060]=frontend;
    Check(Present(capture(0x8203AAEC)).details=="Online","Online Career/Arcade setup stays Online regardless of the reused frontend mode");
    Check(Present(capture(0x82040504)).small_image=="mode_online","Shared online selection retains the network badge");
    Check(Present(capture(0x82034D2C)).details=="Online","Online rider selection is not reported as single-player Career/Arcade");
  }
  Check(Present(capture(0x8203D7FC)).details=="LAN","Native System Link menu selects LAN context");
  Check(Present(capture(0x82035344)).details=="LAN","Shared multiplayer transmission selection preserves LAN context");
  auto configured_lan=capture(0x8203AAEC);configured_lan.network_lan=true;
  Check(Present(configured_lan).details=="LAN","Community LAN configuration is respected on the native Online screen");
  auto settings=capture(0x8203411C);
  Check(Present(settings).details=="Settings"&&!settings.event&&!CanShowLobby(settings.menu),"Settings screen overrides stale race state and cannot inherit lingering lobby counts");
  auto boards=capture(0x82036F14);
  Check(Present(boards).details=="Leaderboards"&&!boards.event&&!CanShowLobby(boards.menu),"Leaderboards has its own menu status and cannot inherit a lobby");
  Check(Present(capture(0x8203CF6C)).details=="Split Screen","Split screen setup is distinct from Career/Arcade");
  Check(Present(capture(0x8203CFB4)).details=="Split Screen"&&Present(capture(0x82040504)).details=="Split Screen","Split screen Career/Arcade and shared season setup retain split context");
  Check(CanShowLobby(capture(0x8203799C).menu)&&!CanShowLobby(Menu::Main),"Only the actual lobby or unknown-screen fallback may query room counts");
  Check(Present(capture(0x8201789C)).details=="Arcade","A new offline Arcade screen clears a previous network setup context");
  capture(0x8200CF7C);screen(0x82079FD4,7);
  Check(Capture(read,context).event&&Present(Capture(read,context)).details.starts_with("Time Trial"),"An active in-race screen outranks a stale underlying menu and preserves the actual event");
  screen(0,7);main=capture(0x8200CF7C);
  Check(!main.event&&Present(main).details=="Main menu","Race-to-menu transition clears again even when native event flags remain set");
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
    NavigationModel();
#ifdef _WIN32
    Ipc();
#endif
    std::cout << "Discord presence model, protocol and available platform checks passed\n";
    return 0;
  } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
