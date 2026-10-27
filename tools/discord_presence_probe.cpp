#include "../src/discord/activity.h"
#include "../src/discord/rpc.h"
#include "../src/discord/client.h"
#include "../src/discord/update_policy.h"
#include "../src/discord/launch.h"
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
  Check(Present(snapshot).details=="Arcade"&&Present(snapshot).state.empty(), "Missing rider metadata retains Arcade and class/year without a circuit-name fallback");
  words[0x828DA060] = 3; words[0x90002000] = 4;
  Check(Present(Capture(read)).details=="Career · Qualifying"&&Present(Capture(read)).state.empty(), "Career retains mode and qualifying fallback when rider/bike metadata is missing");
  words[0x828DA060] = 4; words[0x90002000] = 5;
  Check(Present(Capture(read)).details.starts_with("Championship"), "Championship is distinct from Career");
  words[0x90002000] = 10;
  Check(Present(Capture(read)).details.starts_with("Time Trial"), "Actual event overrides stale frontend selection");
  words[0x90002000] = 14;
  Check(Present(Capture(read)).details.starts_with("Online"), "Online event");
  words[0x90002000] = 13;
  Check(Present(Capture(read)).details.starts_with("LAN"), "LAN event");
  words[0x90002004] = 1; words[0x82AEC2A8 + 0xF75A4] = 2;
  Check(Present(Capture(read)).details == "LAN"&&Present(Capture(read)).large_text=="Losail International Circuit · Moto2 | 2010", "2010 class normalization is shown on the track image");
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
      {0x90004000,0x8206BBCC},{0x9000400C,0x90006000},{0x9000406C,0},
      {0x82AEC2A8+0xF75CC,0x90007000},{0x82AEC2A8+0xF75D0,0},
      {0x82946810,1},{0x82946814,0x90005000},{0x90005004,0x90006000},
      {0x829F3118,2},{0x829F311C,0x90007000},{0x90007004,0x90008000},{0x90007010,0x90008100},
      {0x90007000+44+4,0x90008200},{0x90007000+44+16,0x90008300}};
  text(0x90006000,"Valentino Rossi");text(0x90008000,"Yamaha");text(0x90008100,"YZR-M1");
  text(0x90008200,"Honda");text(0x90008300,"RC212V");
  size_t text_reads=0;
  auto counted_read=[&](uint32_t address){++text_reads;return read(address);};
  Check(detail::Text(counted_read,0x90008000)=="Yamaha"&&text_reads==2,"Native text validates each aligned word once instead of querying guest memory for every byte");
  const auto selected=Capture(read);const auto racing=Present(selected);
  Check(selected.rider=="Valentino Rossi"&&selected.bike=="Yamaha YZR-M1","Decode the selected BuiltInRiderData display name and actual bike");
  Check(racing.details=="Time Trial"&&racing.state=="Valentino Rossi | Yamaha YZR-M1","Heading shows mode/session; the lower row combines full rider and bike names");
  words[0x8292710C]=0x90002008;words[0x90002008]=10;
  words[0x90002000]=1;words[0x82926FBC]=3;
  const auto actual_trial=Capture(read,Menu::TimeTrial);const auto trial_activity=Present(actual_trial);
  Check(actual_trial.mode==10&&trial_activity.details=="Time Trial"&&trial_activity.state=="Valentino Rossi | Yamaha YZR-M1"&&trial_activity.small_image=="mode_time_trial"&&trial_activity.large_text.starts_with("Le Mans"),"Time Trial uses the global native mode despite a Quick Race event config: rider heading, bike row, stopwatch and circuit hover");
  for(auto selection:{std::pair{2u,2u},std::pair{3u,6u},std::pair{4u,5u},std::pair{5u,10u}}) {
    words[0x828DA060]=selection.first;words[0x90002008]=selection.second;
    Check(Capture(read).mode==selection.second,"All four rider-heading modes prefer the live global mode over a stale Quick Race event config");
  }
  words[0x90002008]=0;
  Check(Capture(read).mode==1,"Uninitialized global mode retains a valid event-config fallback");
  words.erase(0x8292710C);words[0x828DA060]=5;words[0x90002000]=10;words[0x82926FBC]=0;
  auto corrected=selected;corrected.rider="Updated rider";corrected.bike="Updated bike";
  Check(Present(corrected)!=racing&&SessionKey(Present(corrected))==SessionKey(racing),"Late rider/bike metadata updates the text without resetting the current session timer");
  corrected.track=1;
  Check(SessionKey(Present(corrected))!=SessionKey(racing),"A different circuit starts a different session timer");
  Check(!Command(racing,1,42,1)["args"]["activity"].contains("session_key"),"Internal timing metadata never enters the Discord payload");
  Check(racing.large_image=="track_losail"&&racing.small_image=="mode_time_trial"&&racing.large_text=="Losail International Circuit · MotoGP | 2009","Track artwork and stopwatch badge retain circuit/class/year hover text");
  const auto assets=Command(racing,12345,42,7)["args"]["activity"]["assets"];
  Check(assets["large_image"]=="track_losail"&&assets["large_text"]=="Losail International Circuit · MotoGP | 2009"&&assets["small_image"]=="mode_time_trial"&&assets["small_text"]=="Time Trial","RPC serializes circuit/class/year hover, track artwork and mode badge");
  Check(!Command(Present({}),1,42,1)["args"]["activity"]["assets"].contains("small_image"),"Main menu uses the full game logo without a duplicate badge");
  struct ContextHeading {uint32_t frontend,mode;const char* heading;};
  for(const auto selection:{ContextHeading{2,2,"Arcade"},ContextHeading{3,4,"Career · Qualifying"},
      ContextHeading{4,5,"Championship · Race"},ContextHeading{3,10,"Time Trial"}}) {
    auto context=selected;context.frontend=selection.frontend;context.mode=selection.mode;
    const auto activity=Present(context);
    Check(activity.details==std::string(selection.heading)&&activity.state=="Valentino Rossi | Yamaha YZR-M1"&&activity.large_image=="track_losail"&&activity.large_text.starts_with("Losail International Circuit"),"All four modes keep mode/session above full rider/bike names and move class/year to track hover");
    context.rider.clear();
    Check(Present(context).details==std::string(selection.heading)&&Present(context).state=="Yamaha YZR-M1","Missing rider data leaves only the bike without an empty separator or fabricated name");
    context.rider="Valentino Rossi";context.bike.clear();
    Check(Present(context).state=="Valentino Rossi","Missing bike data leaves only the rider without an empty separator");
    context.rider.clear();
    Check(Present(context).state.empty(),"Missing rider and bike do not duplicate class/year on the lower row");
  }
  struct HeadingCase {uint32_t mode;const char* label;};
  for(const auto& entry:{HeadingCase{1,"Championship · Race"},HeadingCase{2,"Arcade"},
      HeadingCase{3,"Grand Prix · Practice"},HeadingCase{4,"Grand Prix · Qualifying"},HeadingCase{5,"Grand Prix · Race"},
      HeadingCase{6,"Career"},HeadingCase{7,"Preview"},HeadingCase{8,"Training"},
      HeadingCase{9,"Tutorial"},HeadingCase{10,"Time Trial"},HeadingCase{11,"Split Screen"},
      HeadingCase{12,"Online"},HeadingCase{13,"LAN"},HeadingCase{14,"Online"},HeadingCase{16,"Replay"}}) {
    auto event=selected;event.frontend=0;event.context=Menu::Unknown;event.mode=entry.mode;
    const auto activity=Present(event);const auto payload=Command(activity,1,42,1)["args"]["activity"];
    Check(payload["details"]==std::string(entry.label)&&payload["state"]=="Valentino Rossi | Yamaha YZR-M1"&&payload["assets"]["large_text"].get<std::string>().starts_with("Losail International Circuit"),"Every active mode publishes mode/session above full local rider/bike names, with circuit/class/year artwork hover");
    event.rider.clear();
    Check(Present(event).details==std::string(entry.label),"Missing rider metadata never puts the circuit back into any mode's heading");
  }
  auto single_race=selected;single_race.frontend=1;single_race.mode=1;
  const auto championship=Present(single_race);
  Check(championship.details=="Championship · Race"&&championship.state=="Valentino Rossi | Yamaha YZR-M1"&&championship.small_image=="mode_championship"&&championship.small_text=="Championship"&&championship.large_text.starts_with("Losail International Circuit"),"Native Quick Race is displayed as Championship with race phase, rider heading, bike row, trophy badge and circuit hover");
  single_race.event=false;single_race.menu=Menu::Selection;single_race.context=Menu::Championship;
  Check(Present(single_race).details=="Championship","Single-race Championship setup retains the Championship label");
  auto long_names=selected;long_names.rider=std::string(75,'A');long_names.bike="Tech 3 ";
  for(unsigned i=0;i<22;++i)long_names.bike+="ç";
  const auto bounded=Present(long_names);
  Check(bounded.state.size()<=127&&bounded.state.ends_with(long_names.bike)&&bounded.state.find("... | ")!=std::string::npos&&
      Json::parse(Command(bounded,1,42,1).dump())["args"]["activity"]["state"]==bounded.state,
      "Combined long text preserves the entire bike while shortening only the rider inside the UTF-8 RPC limit");
  auto changed_mode=selected;changed_mode.context=Menu::Career;changed_mode.frontend=3;changed_mode.mode=2;
  Check(Mode(changed_mode)=="Arcade","Actual Arcade event overrides retained Career menu context");
  changed_mode.context=Menu::Arcade;changed_mode.frontend=2;changed_mode.mode=6;
  Check(Mode(changed_mode)=="Career","Actual Career event overrides retained Arcade menu context");
  changed_mode.context=Menu::SplitScreen;
  Check(Mode(changed_mode)=="Split Screen","Split-screen Career remains split screen despite its shared Career event mode");
  struct BadgeCase {uint32_t frontend,mode;const char* key;const char* label;};
  for(const auto& entry:{BadgeCase{2,10,"mode_time_trial","Time Trial"},
      BadgeCase{4,5,"mode_championship","Championship"},BadgeCase{3,4,"mode_career","Career"},
      BadgeCase{2,2,"mode_arcade","Arcade"},BadgeCase{3,14,"mode_online","Online"},
      BadgeCase{1,1,"mode_championship","Championship"},BadgeCase{4,13,"mode_lan","LAN"}}) {
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
  Check(career_activity.details=="Career · Qualifying"&&career_activity.state=="Ayşe Deniz | Honda RC212V"&&career_activity.large_text=="Losail International Circuit · MotoGP | 2009","Career shows session above the full custom rider and bike, with circuit/class/year on hover");
  text(grid+1824+1596,std::string("\xED\xA0\x80"));Check(Capture(read).rider.empty(),"Malformed native text is omitted instead of breaking JSON");
  words[0x82AEC2A8+0xF75C4]=0;Check(Capture(read).rider.empty()&&Capture(read).bike.empty(),"Missing Career identity never publishes another grid rider or a stale Arcade bike");
  words[0x82AEC2A8+0xF75C4]=0x90004000;
  words[0x90002000]=10;Check(Capture(read).rider=="Valentino Rossi","Time Trial overrides a stale Career frontend selection when choosing rider metadata");
  words[0x82AEC2A8+0xF75D0]=UINT32_MAX;Check(Capture(read).rider=="Valentino Rossi","Missing legacy selection index cannot hide a valid native rider name");
  words[0x90004000]=0xDEADBEEF;Check(Capture(read).rider.empty(),"Unknown rider adapters cannot publish unrelated livery-table names");
  words[0x82942DB0]=0;screen(0x8201C054);const auto cleared=Present(Capture(read));
  Check(cleared.large_image=="motogp_logo"&&cleared.small_image=="mode_career"&&cleared.state=="In menus","Leaving a race clears rider, bike and circuit artwork while keeping the selected mode badge");
  words[0x828DA060]=0;screen(0x8200CF7C);const auto main_menu=Present(Capture(read));
  const auto main_assets=Command(main_menu,1,42,1)["args"]["activity"]["assets"];
  Check(main_menu.large_image=="motogp_logo"&&main_menu.small_image.empty()&&main_menu.small_text.empty()&&!main_assets.contains("small_image")&&!main_assets.contains("small_text"),"Returning from a badged event to the main menu removes both small image and hover fields");
  auto command = Command({"Career", "MotoGP · 2010"}, 12345, 42, 7);
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
void ChampionshipSessions() {
  constexpr uint32_t profile=0x82AEC2A8,selected=0x90004000,table=0x90007000;
  std::map<uint32_t,uint32_t> words{
    {0x828DA060,4},{0x82942D60,0x90000000},{0x82942DB0,1},
    {0x90000040,0x90001000},{0x9000100C,0x90002000},{0x90002000,3},
    {0x90001244,0x90002004},{0x90002004,1},{0x82926FBC,0},
    {0x8292710C,0x90002008},{0x90002008,2},
    {0x834F9494,0x90003000},{0x90003018,0},{0x90003070,0},
    {profile+0xF75A4,3},{profile+0xF75C4,selected},{profile+0xF75D0,UINT32_MAX},
    {selected,0x8206BBCC},{selected+12,0x90006000},{selected+108,101},
    {0x829F3118,112},{0x829F311C,table},
    {table+101*44+4,0x90008000},{table+101*44+16,0x90008100}};
  const auto read=[&](uint32_t at)->std::optional<uint32_t>{
    const auto it=words.find(at);return it==words.end()?std::nullopt:std::optional(it->second);
  };
  const auto text=[&](uint32_t address,std::string value){
    value+='\0';
    for(size_t i=0;i<value.size();++i){auto& word=words[(address+uint32_t(i))&~3u];
      const auto shift=24-8*((address+i)&3);word=(word&~(0xFFu<<shift))|(uint32_t(uint8_t(value[i]))<<shift);}
  };
  text(0x90006000,"Yuki Takahashi");text(0x90008000,"Tech 3");text(0x90008100,"Mistral 610");
  std::string previous;
  struct Session {uint32_t mode;const char* name;};
  for(const auto phase:{Session{3,"Practice"},Session{4,"Qualifying"},Session{5,"Race"}}) {
    words[0x90002000]=phase.mode;
    const auto snapshot=Capture(read,Menu::Arcade);const auto activity=Present(snapshot);
    Check(activity.details==std::string("Championship · ")+phase.name&&
        activity.state=="Yuki Takahashi | Tech 3 Mistral 610"&&activity.small_image=="mode_championship"&&
        activity.large_text=="Losail International Circuit · Moto2 | 2010",
        "Moto2 Championship uses the native rider, session and trophy despite stale Arcade mode/context and an absent legacy rider ID");
    Check(previous.empty()||previous!=SessionKey(activity),"Changing Championship session starts a new activity timer even with unchanged global mode");
    previous=SessionKey(activity);
    words[0x90002008]=phase.mode;
    Check(Present(Capture(read,Menu::Arcade)).details==activity.details,
        "Live session modes also retain the Championship owner instead of remembered Arcade context");
    words[0x90002008]=2;
  }
  words[0x90002000]=1;words[0x90002008]=3;
  Check(Present(Capture(read,Menu::Arcade)).details=="Championship · Practice",
      "A live practice mode takes precedence over a stale single-race event config");
  struct DriverMode {uint32_t frontend,mode;const char* heading;};
  for(const auto m:{DriverMode{2,2,"Arcade"},DriverMode{5,10,"Time Trial"},
      DriverMode{2,14,"Online"},DriverMode{2,13,"LAN"},DriverMode{9,11,"Split Screen"}}) {
    words[0x828DA060]=m.frontend;words[0x90002008]=m.mode;
    Check(Present(Capture(read)).details==std::string(m.heading)&&
        Present(Capture(read)).state=="Yuki Takahashi | Tech 3 Mistral 610",
        "The real selected rider is available outside Championship without an old livery-table index");
  }
  words[0x828DA060]=5;words[0x90002008]=10;words[0x90002000]=5;
  Check(Present(Capture(read)).large_text=="Losail International Circuit · Moto2 | 2010"&&Present(Capture(read)).details=="Time Trial",
      "Time Trial cannot inherit a stale Championship race phase from its event config");
  words[0x90002008]=3;words[0x90002000]=3;words[0x828DA060]=2;
  Check(Present(Capture(read,Menu::Network)).details=="Online · Practice",
      "Online practice cannot become single-player Arcade through the shared frontend mode");
  words[0x828DA060]=3;words[0x90002008]=6;words[selected]=0x82069B9C;
  words[selected+60]=0x90009000;text(0x90009000+56,"Ayşe Deniz");
  Check(Present(Capture(read)).details=="Career · Practice"&&Present(Capture(read)).state=="Ayşe Deniz",
      "Custom Career rider uses the native name accessor even before a matching grid exists");
  words[0x828DA060]=4;words[0x90002008]=3;words[selected]=0x8206BBCC;
  words[0x8291749C]=2;
  words[0x8291E130+1768]=0x90009900;text(0x8291E130+1596,"Another rider");
  words[0x8291E130+1824+1768]=selected;words[0x8291E130+1824+1764]=101;
  text(0x8291E130+1824+1596,"Actual local rider");
  Check(Present(Capture(read,Menu::Arcade)).details=="Championship · Practice"&&Present(Capture(read,Menu::Arcade)).state=="Actual local rider | Tech 3 Mistral 610",
      "All modes prefer the matching actual grid name over frontend metadata, never grid entry zero");
  words[profile+0xF75C4]=0;
  Check(Capture(read).rider.empty(),"A missing local selection cannot expose another grid rider");
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
  const auto multiplayer=Present(capture(0x8203A0C4));
  Check(multiplayer.details=="Online"&&multiplayer.small_image=="mode_online"&&context==Menu::Network,"Multiplayer entry uses the same Online label, badge and setup context as its online submenus");
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
  const auto dlc=capture(0x8200C574);const auto dlc_activity=Present(dlc);
  Check(dlc_activity.details=="DLC"&&dlc_activity.state=="In menus"&&!dlc.event&&
      !CanShowLobby(dlc.menu)&&dlc_activity.large_image=="motogp_logo"&&dlc_activity.small_image.empty(),
      "The native DLC browser has its own menu status without an old race, lobby or fabricated badge");
  Check(Present(capture(0x8203CF6C)).details=="Split Screen","Split screen setup is distinct from Career/Arcade");
  Check(Present(capture(0x8203CFB4)).details=="Split Screen"&&Present(capture(0x82040504)).details=="Split Screen","Split screen Career/Arcade and shared season setup retain split context");
  Check(CanShowLobby(capture(0x8203799C).menu)&&!CanShowLobby(Menu::Main),"Only the actual lobby or unknown-screen fallback may query room counts");
  Check(Present(capture(0x8201789C)).details=="Arcade","A new offline Arcade screen clears a previous network setup context");
  capture(0x8200CF7C);screen(0x82079FD4,7);
  Check(Capture(read,context).event&&Present(Capture(read,context)).details.starts_with("Time Trial"),"An active in-race screen outranks a stale underlying menu and preserves the actual event");
  screen(0,7);main=capture(0x8200CF7C);
  Check(!main.event&&Present(main).details=="Main menu","Race-to-menu transition clears again even when native event flags remain set");
  screen(0,2);screen(0x82079FD4,7);
  words[0x8292710C]=0x90002008;words[0x90002008]=15;
  for(auto native_mode:{3u,4u,5u})for(auto frontend:{0u,2u,3u,4u,5u})for(auto previous:{Menu::Unknown,Menu::Main,Menu::Arcade,Menu::Career,Menu::Championship,Menu::TimeTrial}) {
    words[0x90002000]=native_mode;words[0x828DA060]=frontend;
    const auto attract=Capture(read,previous);const auto activity=Present(attract);
    Check(!attract.event&&attract.menu==Menu::Main&&attract.track==UINT32_MAX&&attract.rider.empty()&&attract.bike.empty()&&activity==Present({})&&!CanShowLobby(attract.menu),"Global demo mode overrides ordinary session modes and stale frontend/context without a circuit photo, rider, mode badge or lobby counter");
  }
  words[0x90002000]=3;Check(!Capture(read).event,"Starting presence during an already-running attract race also stays in Main menu");
  words[0x82942D60]=0;
  Check(Capture(read,Menu::Career).menu==Menu::Main,"Demo loading clears stale context before the race object exists");
  words[0x82942D60]=0x90000000;
  screen(0x8201789C,7);
  Check(Present(Capture(read,Menu::Main)).details=="Arcade","An actual setup menu remains authoritative when leaving a demo");
  screen(0x82079FD4,7);
  for(auto selection:{std::pair{2u,Menu::Arcade},std::pair{3u,Menu::Career},std::pair{4u,Menu::Championship}}) {
    words[0x828DA060]=selection.first;
    for(auto native_mode:{3u,4u,5u}) {
      words[0x90002000]=native_mode;words[0x90002008]=native_mode;
      Check(Capture(read,selection.second).event,"Actual player practice, qualifying and race sessions are not hidden as attract playback");
    }
  }
  words[0x828DA060]=0;words[0x90002000]=3;
  Check(Capture(read,Menu::Main).event,"A real non-demo session is not hidden just because FEMODE/context was reset");
  words.erase(0x8292710C);
  Check(Capture(read,Menu::Main).event,"An unavailable global mode does not fabricate demo playback");
  words[0x90002000]=15;
  Check(!Capture(read,Menu::Championship).event,"Explicit native demo mode also remains Main menu");
}
void ChampionshipSelectionMenus() {
  // Leave a complete old Time Trial event alive: none of these setup screens
  // may leak its rider, photo, phase or active-event status into the activity.
  std::map<uint32_t,uint32_t> words{{0x828DA060,4},{0x82942D60,0x90000000},
      {0x82942DB0,1},{0x90000040,0x90001000},{0x9000100C,0x90002000},
      {0x90002000,10},{0x82926FBC,0},{0x8336F4B8,0x90010000},
      {0x90010000,0x90020000}};
  const auto read=[&](uint32_t at)->std::optional<uint32_t>{
    const auto it=words.find(at);return it==words.end()?std::nullopt:std::optional(it->second);
  };
  // Both native Championship entry paths use the same selection components.
  for(uint32_t frontend:{1u,4u}) {
    words[0x828DA060]=frontend;
    for(Menu initial:{Menu::Unknown,Menu::Main,Menu::Arcade,Menu::Championship}) {
      Menu context=initial;
      for(uint32_t type:{0x82017374u,0x8203F4F4u,0x8203FB4Cu, // class
          0x82017284u,0x8203EE1Cu,0x8203EA54u, // rider and preview
          0x820186B4u,0x8204223Cu, // transmission
          0x8201841Cu,0x82040504u,0x82040C64u, // season
          0x82017B2Cu,0x82041A04u}) { // track
        words[0x90020000]=type;
        const auto s=Capture(read,context);context=s.context;const auto a=Present(s);
        Check(!s.event&&a.details=="Championship"&&a.state=="In menus"&&
            a.small_image=="mode_championship"&&a.large_image=="motogp_logo"&&s.rider.empty(),
            "Championship class/rider/transmission/season/track setup keeps its owner despite Arcade-named screen types and stale event data");
      }
    }
    for(Menu context:{Menu::Network,Menu::LAN,Menu::SplitScreen,Menu::TimeTrial}) {
      for(uint32_t type:{0x82017374u,0x82017284u,0x820186B4u,0x8201841Cu,0x82017B2Cu,0x82040504u}) {
        words[0x90020000]=type;
        Check(Capture(read,context).context==context,
            "Shared selections preserve explicit network/split-screen/Time Trial context when Championship frontend data is stale");
      }
    }
  }
  words[0x828DA060]=4;words[0x90020000]=0x8201789C;
  Check(Present(Capture(read,Menu::Championship)).details=="Arcade",
      "Entering the actual Arcade main screen clears Championship even before frontend state updates");
  words[0x828DA060]=2;
  for(uint32_t type:{0x82017374u,0x82017284u,0x820186B4u,0x8201841Cu,0x82017B2Cu}) {
    words[0x90020000]=type;
    const auto a=Present(Capture(read,Menu::Championship));
    Check(a.details=="Arcade"&&a.small_image=="mode_arcade",
        "Real Arcade class/rider/transmission/season/track screens retain Arcade rather than stale Championship history");
  }
  struct Setup {uint32_t frontend;Menu previous;const char* label;};
  for(const auto setup:{Setup{2,Menu::Network,"Online"},Setup{3,Menu::Network,"Online"},
      Setup{2,Menu::LAN,"LAN"},Setup{3,Menu::LAN,"LAN"},
      Setup{3,Menu::Career,"Career"},Setup{9,Menu::SplitScreen,"Split Screen"},
      Setup{10,Menu::SplitScreen,"Split Screen"},Setup{5,Menu::TimeTrial,"Time Trial"}}) {
    words[0x828DA060]=setup.frontend;Menu context=setup.previous;
    for(uint32_t type:{0x82017374u,0x8203F4F4u,0x82017284u,0x8203EE1Cu,
        0x820186B4u,0x8204223Cu,0x8201841Cu,0x82040504u,0x82017B2Cu,0x82041A04u}) {
      words[0x90020000]=type;
      const auto s=Capture(read,context);context=s.context;
      Check(!s.event&&Present(s).details==setup.label,
          "Every supported setup owner survives both Arcade-derived and common base selections without advertising a stale event");
    }
    words[0x90020000]=0x8201789C;
    Check(Present(Capture(read,context)).details=="Arcade",
        "Explicit offline Arcade entry resets each previous setup owner");
  }
  for(const auto setup:{Setup{1,Menu::Unknown,"Championship"},Setup{4,Menu::Main,"Championship"},
      Setup{2,Menu::Unknown,"Arcade"},Setup{3,Menu::Main,"Career"},
      Setup{5,Menu::Unknown,"Time Trial"},Setup{9,Menu::Unknown,"Split Screen"},
      Setup{10,Menu::Main,"Split Screen"}}) {
    words[0x828DA060]=setup.frontend;words[0x90020000]=0x82040504;
    Check(Present(Capture(read,setup.previous)).details==setup.label,
        "A common selection can recover its verified native frontend owner when the preceding screen was not captured");
  }
  words[0x90020000]=0x8200CF7C;
  Check(Present(Capture(read,Menu::Championship)).details=="Main menu",
      "Leaving Championship selection for Main menu clears the selection owner");
}
void InvitationModel(){
  const std::string secret=std::string(kJoinPrefix)+std::string(32,'A');
  Activity lobby; lobby.details="Online lobby";lobby.state="In lobby";
  lobby.party_size=1;lobby.party_capacity=20;lobby.party_id="mg0910:0009000000000001:000000000000002a";lobby.join_secret=secret;
  const auto command=Command(lobby,123,42,1)["args"]["activity"];
  Check(command["instance"]==true&&command["secrets"]["join"]==secret&&command["party"]["id"]==lobby.party_id,"Joinable activity carries a bounded opaque capability and shared party id");
  const auto event=Json{{"cmd","DISPATCH"},{"evt","ACTIVITY_JOIN"},{"data",{{"secret",secret}}}};
  Check(JoinEvent(event)==secret,"Discord join dispatch extracts only the validated capability");
  for(const auto& malformed:{Json{},Json{{"cmd","DISPATCH"},{"evt","ACTIVITY_SPECTATE"},{"data",{{"secret",secret}}}},Json{{"cmd","DISPATCH"},{"evt","ACTIVITY_JOIN"},{"data",{{"secret",42}}}},Json{{"cmd","DISPATCH"},{"evt","ACTIVITY_JOIN"},{"data",{{"secret","https://external.invalid/"}}}}})
    Check(!JoinEvent(malformed),"Wrong events, types and arbitrary URLs never become join requests");
  for(const auto& invalid:{std::string{},secret+"A",std::string(kJoinPrefix)+std::string(32,'/'),std::string(128,'A')}){
    lobby.join_secret=invalid;Check(!Command(lobby,123,42,1)["args"]["activity"].contains("secrets"),"Malformed capabilities are not advertised");
  }
  lobby.join_secret=secret;lobby.party_size=lobby.party_capacity;
  Check(!Command(lobby,123,42,1)["args"]["activity"].contains("secrets"),"A full lobby keeps its party counter but cannot advertise joining");
  lobby.party_size=1;lobby.party_id.clear();
  Check(!Command(lobby,123,42,1)["args"]["activity"].contains("secrets"),"Joining requires a real party id");
  Activity menus=Present({});auto published=menus;published.party_id="mg0910:0009000000000001:000000000000002a";published.join_secret=secret;
  Check(!StartsNewRaceTimer(menus,published),"Join publication does not restart menu or lobby elapsed time");
  Check(SubscribeJoin(7)==Json{{"cmd","SUBSCRIBE"},{"evt","ACTIVITY_JOIN"},{"nonce","7"}},"Subscribe uses the documented Discord RPC event");
  Check(LaunchCommand(L"D:\\MotoGP Game\\MotoGP09-10Launcher.exe")==L"\"D:\\MotoGP Game\\MotoGP09-10Launcher.exe\" --discord-invite","Cold launch invokes the quoted public launcher without an invitation secret");
  Check(LaunchCommand(L"bad\"command").empty()&&LaunchCommand(L"").empty(),"Invalid launcher paths cannot become protocol commands");
}
void UpdateTiming() {
  UpdatePolicy updates;
  const auto start=UpdatePolicy::Clock::time_point{}+1s;
  Check(!updates.Ready(start+199ms,start)&&updates.Ready(start+200ms,start),"Short coalescing delay suppresses transient menu states without a five-second stall");
  for(int i=0;i<5;++i) {
    const auto now=start+200ms+i*300ms;
    Check(updates.Ready(now,now-200ms),"Meaningful activity changes can publish in a responsive burst");updates.Sent(now);
  }
  Check(!updates.Ready(start+3s,start+2s),"A sixth update waits for Discord's documented rolling-window budget");
  Check(!updates.Ready(start+20199ms,start+2s)&&updates.Ready(start+20200ms,start+2s),"Capacity reopens exactly when the earliest update expires");
  Check(!updates.Ready(start+20300ms,start+20200ms),"A newer pending state gets its own short coalescing delay rather than flushing an obsolete menu");
}
void ElapsedTiming() {
  Snapshot practice;practice.event=true;practice.frontend=4;practice.mode=3;
  practice.phase=3;practice.track=0;practice.season=1;practice.bike_class=3;
  practice.rider="Raffaele De Rosa";practice.bike="Tech 3 Mistral 610";
  const auto race=Present(practice),main=Present({});
  Check(StartsNewRaceTimer(main,race),"A track photo appearing after menus starts the race clock at zero");
  Activity previous=main;
  for(Menu menu:{Menu::Championship,Menu::Arcade,Menu::Career,Menu::TimeTrial,
      Menu::Network,Menu::LAN,Menu::Settings,Menu::Leaderboards,Menu::DLC,Menu::Main}) {
    Snapshot s;s.menu=menu;s.context=menu;const auto next=Present(s);
    Check(!StartsNewRaceTimer(previous,next)&&!StartsNewRaceTimer(race,next),
        "Menu changes and returning from racing to menus keep the current elapsed clock");
    previous=next;
  }
  Snapshot lobby;lobby.lobby=true;lobby.online=true;lobby.members=1;lobby.capacity=20;
  const auto room=Present(lobby);lobby.members=2;
  Check(!StartsNewRaceTimer(main,room)&&!StartsNewRaceTimer(room,Present(lobby))&&
      !StartsNewRaceTimer(room,main),"Lobby entry, member count changes and lobby exit do not reset the menu clock");
  auto renamed=practice;renamed.rider="Updated rider";renamed.bike="Updated bike";
  Check(!StartsNewRaceTimer(race,Present(renamed)),"Late rider and bike text do not restart an active race clock");
  auto equivalent=practice;equivalent.mode=2;
  Check(!StartsNewRaceTimer(Present(equivalent),race),
      "A native mode update for the same Championship practice/photo cannot restart its clock twice");
  auto qualifying=practice;qualifying.mode=4;qualifying.phase=4;
  Check(StartsNewRaceTimer(race,Present(qualifying)),"A genuinely new qualifying session starts a fresh race clock");
  auto track=practice;track.track=1;
  Check(StartsNewRaceTimer(race,Present(track)),"A different circuit photo starts a new race clock");
  Check(StartsNewRaceTimer(main,race)&&!StartsNewRaceTimer(race,race),
      "Returning to the same track through menus restarts once; unchanged captures do not");
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
  Snapshot race;race.event=true;race.mode=10;race.track=0;race.season=0;race.bike_class=0;race.rider="Valentino Rossi";race.bike="Yamaha YZR-M1";
  const auto changed_at=std::chrono::steady_clock::now();
  client.Publish(Present(race));
  const auto racing=server.Read();
  Check(racing.second["args"]["activity"]["details"]=="Time Trial"&&std::chrono::steady_clock::now()-changed_at<1500ms,"Real Windows pipe publishes a menu/race change promptly instead of waiting five seconds");
  std::this_thread::sleep_for(1100ms);
  race.rider="Updated rider";client.Publish(Present(race));
  const auto corrected=server.Read();
  Check(corrected.second["args"]["activity"]["timestamps"]==racing.second["args"]["activity"]["timestamps"],"Real worker preserves elapsed time when only rider metadata changes");
  Snapshot settings;settings.menu=Menu::Settings;
  client.Publish(Present(settings));client.Publish(Present({}));
  const auto latest=server.Read();
  Check(latest.second["args"]["activity"]["details"]=="Main menu","Rapid consecutive publishes coalesce to the latest screen instead of queueing obsolete menus");
  Check(latest.second["args"]["activity"]["timestamps"]==racing.second["args"]["activity"]["timestamps"],
      "Returning to menus preserves the race clock even after wall time has advanced");
  client.Publish(Present({}));std::this_thread::sleep_for(100ms);
  available=0;Check(PeekNamedPipe(server.pipe,nullptr,0,nullptr,&available,nullptr)&&available==0,"Unchanged captures do not resend the same activity");
  client.Publish(Present(race));const auto second_race=server.Read();
  Check(second_race.second["args"]["activity"]["timestamps"]["start"].get<int64_t>()>
      racing.second["args"]["activity"]["timestamps"]["start"].get<int64_t>(),
      "A track photo returning after menus sends a fresh race timestamp over the real IPC pipe");
  client.Stop();
  const auto clear = server.Read(); Check(clear.second["args"]["activity"].is_null(), "Shutdown clears activity");
  client.Start(); // reconnect to an absent reader must still stop immediately.
  const auto start = std::chrono::steady_clock::now(); client.Stop();
  Check(std::chrono::steady_clock::now() - start < 1s, "Bounded shutdown with Discord absent");
}
void IpcInvites(){
  Server server;Client client;std::atomic<unsigned> joins=0;
  const std::string secret=std::string(kJoinPrefix)+std::string(32,'B');
  client.Start([&](const std::string& value){if(value==secret)++joins;return true;});
  server.Connect();Check(server.Read().first==0,"Invitation client performs the normal handshake");
  server.Send(1,Json{{"cmd","DISPATCH"},{"evt","READY"}},true);
  const auto subscription=server.Read();Check(subscription.second["cmd"]=="SUBSCRIBE"&&subscription.second["evt"]=="ACTIVITY_JOIN","Ready subscribes once to game joins");
  Check(server.Read().second["cmd"]=="SET_ACTIVITY","Subscription does not block ordinary presence");
  Activity lobby;lobby.details="Online lobby";lobby.state="In lobby";lobby.party_size=1;lobby.party_capacity=20;
  lobby.party_id="mg0910:0009000000000001:000000000000002a";lobby.join_secret=secret;client.Publish(lobby);
  const auto advertised=server.Read().second["args"]["activity"];
  Check(advertised["secrets"]["join"]==secret,"Real pipe worker publishes the lobby join capability");
  server.Send(1,Json{{"cmd","DISPATCH"},{"evt","ACTIVITY_JOIN"},{"data",{{"secret","not-a-capability"}}}});
  server.Send(1,Json{{"cmd","DISPATCH"},{"evt","ACTIVITY_JOIN"},{"data",{{"secret",secret}}}},true);
  server.Send(3,Json{{"ping","invite"}});Check(server.Read().first==4,"Join callbacks preserve PING/PONG handling");
  Check(joins.load()==1,"Only the valid join dispatch reaches the callback");
  server.Send(1,Json{{"cmd","SUBSCRIBE"},{"evt","ERROR"},{"data",{{"message","unsupported"}}}});
  const auto degraded=server.Read().second["args"]["activity"];
  Check(degraded["details"]=="Online lobby"&&!degraded.contains("secrets")&&!degraded["instance"].get<bool>(),"Rejected optional subscription clears joining while keeping presence connected");
  lobby.state="Still connected";client.Publish(lobby);
  Check(server.Read().second["args"]["activity"]["state"]=="Still connected","Presence continues after invitation capability rejection");
  client.Stop();Check(server.Read().second["args"]["activity"].is_null(),"Invitation client clears activity on shutdown");
}
#endif
int main() {
  try {
    Model();
    ChampionshipSessions();
    NavigationModel();
    ChampionshipSelectionMenus();
    UpdateTiming();
    ElapsedTiming();
    InvitationModel();
#ifdef _WIN32
    Ipc();
    IpcInvites();
#endif
    std::cout << "Discord presence model, protocol and available platform checks passed\n";
    return 0;
  } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
