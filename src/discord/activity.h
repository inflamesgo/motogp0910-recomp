#pragma once
#include "roster.h"
#include "navigation.h"
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
  std::string rider, bike;
  Menu menu=Menu::Unknown,context=Menu::Unknown;
  bool network_lan=false;
  bool operator==(const Snapshot&) const = default;
};
namespace detail {
template<class Reader> std::optional<uint32_t> At(Reader& read,uint32_t base,uint32_t offset=0) {
  if(!base||uint64_t(base)+offset+4>0x100000000ull)return {};
  return read(base+offset);
}
// Native strings are UTF-8. Reject unterminated, control or malformed data
// instead of allowing a stale pointer to break JSON serialization.
template<class Reader> std::string Text(Reader& read,uint32_t pointer,size_t limit=80) {
  if(!pointer||uint64_t(pointer)+limit>0x100000000ull)return {};
  std::string out;
  for(size_t i=0;i<limit;++i) {
    const auto address=pointer+uint32_t(i);
    const auto word=read(address&~3u);if(!word)return {};
    const auto byte=uint8_t(*word>>(24-8*(address&3)));
    if(!byte) {
      for(size_t k=0;k<out.size();) {
        const auto first=uint8_t(out[k++]);if(first<0x80)continue;
        unsigned count=first>=0xF0&&first<=0xF4?3:first>=0xE0&&first<=0xEF?2:first>=0xC2&&first<=0xDF?1:0;
        if(!count||k+count>out.size())return {};
        uint32_t cp=first&((1u<<(6-count))-1);
        for(unsigned j=0;j<count;++j){const auto next=uint8_t(out[k++]);if((next&0xC0)!=0x80)return {};cp=(cp<<6)|(next&0x3F);}
        if(cp<(count==1?0x80u:count==2?0x800u:0x10000u)||cp>0x10FFFF||(cp>=0xD800&&cp<=0xDFFF))return {};
      }
      return out;
    }
    if(byte<0x20||byte==0x7F)return {};
    out+=char(byte);
  }
  return {};
}
template<class Reader> std::string Bike(Reader& read,uint32_t record) {
  const auto count=read(0x829F3118).value_or(0),table=read(0x829F311C).value_or(0);
  if(!count||count>512||!table||record<table||uint64_t(record)+44>uint64_t(table)+count*44||(record-table)%44)return {};
  const auto manufacturer=Text(read,At(read,record,4).value_or(0),32);
  const auto model=Text(read,At(read,record,16).value_or(0),48);
  return manufacturer.empty()?model:model.empty()?manufacturer:manufacturer+" "+model;
}
template<class Reader> void Driver(Reader& read,uint32_t profile,Snapshot& s) {
  const bool career=s.mode==6||((s.context==Menu::Career||
      (s.context==Menu::Unknown&&(s.frontend==3||s.frontend==10))||
      (s.context==Menu::SplitScreen&&s.frontend==10))&&s.mode>=3&&s.mode<=5);
  const auto selection=At(read,profile,0xF75C4).value_or(0);
  // Career may use a separate selection adapter; never reuse an Arcade bike
  // when its actual event entry cannot be identified.
  if(!career)s.bike=Bike(read,At(read,profile,0xF75CC).value_or(0));
  if(!career) {
    const auto id=At(read,profile,0xF75D0).value_or(UINT32_MAX);
    const auto count=read(0x82946810).value_or(0),table=read(0x82946814).value_or(0);
    if(count&&count<=256&&table&&id<count&&uint64_t(table)+id*124+8<=0x100000000ull) {
      const auto key=Text(read,At(read,table+id*124,4).value_or(0));
      s.rider=RiderName(key);
    }
  }
  // The native grid builder stores this exact selection-interface identity.
  // Match it, rather than assuming the first grid entry belongs to the user.
  const auto count=read(0x8291749C).value_or(0);
  if(!selection||count>64)return;
  for(uint32_t i=0;i<count;++i) {
    const uint32_t entry=0x8291E130+i*1824;
    if(At(read,entry,1768).value_or(0)!=selection)continue;
    if(career)s.rider=Text(read,entry+1596);
    const auto bike=At(read,entry,1764).value_or(UINT32_MAX);
    const auto table=read(0x829F311C).value_or(0),bikes=read(0x829F3118).value_or(0);
    if(table&&bike<bikes&&bike<512&&uint64_t(table)+bike*44+44<=0x100000000ull)
      if(const auto name=Bike(read,table+bike*44);!name.empty())s.bike=name;
    break;
  }
}
} // namespace detail
// Read only on the guest frame thread. The supplied reader validates each word.
template<class Reader> Snapshot Capture(Reader read,Menu previous=Menu::Unknown) {
  Snapshot s;
  s.frontend = read(0x828DA060).value_or(0);
  s.menu=CurrentMenu(read);
  s.context=MenuContext(s.menu,previous);
  // Frontend mode and even the race object can survive a return to menus.
  // A currently displayed menu is authoritative and must clear race metadata.
  if(s.menu!=Menu::Unknown&&s.menu!=Menu::Race)return s;
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
    if (slot < 4) {
      const auto profile=0x82AEC2A8+slot*0x1EED44;
      s.bike_class=detail::At(read,profile,0xF75A4).value_or(UINT32_MAX);
      detail::Driver(read,profile,s);
    }
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
inline std::string_view TrackAsset(uint32_t slot) {
  static constexpr std::string_view keys[]{
    "track_losail","track_motegi","track_jerez","track_le_mans","track_mugello",
    "track_catalunya","track_assen","track_laguna_seca","track_sachsenring","track_donington",
    "track_brno","track_indianapolis","track_misano","track_estoril","track_phillip_island",
    "track_sepang","track_valencia","","track_silverstone","track_shanghai","track_istanbul"};
  return slot<std::size(keys)?keys[slot]:std::string_view{};
}
inline std::string_view Mode(const Snapshot& s) {
  const auto context=[&]() -> std::string_view {
    switch(s.context) {
      case Menu::Arcade:return "Arcade";
      case Menu::Career:return "Career";
      case Menu::Championship:return "Championship";
      case Menu::TimeTrial:return "Time Trial";
      case Menu::Network:return s.network_lan?"LAN":"Online";
      case Menu::LAN:return "LAN";
      case Menu::SplitScreen:return "Split Screen";
      default:return {};
    }
  };
  if(!s.event) {
    switch(s.menu) {
      case Menu::Main:return "Main menu";
      case Menu::Settings:return "Settings";
      case Menu::Leaderboards:return "Leaderboards";
      case Menu::Multiplayer:return "Multiplayer";
      case Menu::Selection: {
        const auto selected=context();return selected.empty()?"Race setup":selected;
      }
      case Menu::Unknown:return "Main menu";
      default: {
        const auto selected=context();return selected.empty()?"Main menu":selected;
      }
    }
  }
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
  if(const auto selected=context();!selected.empty())return selected;
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
  std::string large_image="motogp_logo",large_text="MotoGP 09/10",small_image,small_text;
  uint32_t party_size=0,party_capacity=0;
  bool operator==(const Activity&) const = default;
};
inline std::string_view ModeAsset(std::string_view mode) {
  if(mode=="Time Trial")return "mode_time_trial";
  if(mode=="Championship")return "mode_championship";
  if(mode=="Career")return "mode_career";
  if(mode=="Arcade")return "mode_arcade";
  if(mode=="Online")return "mode_online";
  if(mode=="Quick Race")return "mode_quick_race";
  if(mode=="LAN")return "mode_lan";
  return {};
}
inline Activity Present(const Snapshot& s) {
  const auto mode=Mode(s);
  Activity a{std::string(mode), {}};
  a.small_image=ModeAsset(mode);
  if(!a.small_image.empty())a.small_text=mode;
  if (!s.event) {
    if (s.lobby) {
      a.details = s.online ? "Online lobby" : "LAN lobby";
      a.small_text=s.online?"Online":"LAN";
      a.small_image=ModeAsset(a.small_text);
      a.state="In lobby";
      if(s.members&&s.members<=s.capacity&&s.capacity<=INT32_MAX) {
        a.party_size=s.members;
        a.party_capacity=s.capacity;
      }
      return a;
    }
    a.state = a.details == "Main menu" ? "MotoGP 09/10" : "In menus";
    return a;
  }
  const bool rider_heading=mode=="Arcade"||mode=="Career"||mode=="Championship"||mode=="Time Trial";
  if(rider_heading) {
    if(!s.rider.empty())a.details+=" - "+s.rider;
  } else if (const auto track = Track(s.track); !track.empty()) a.details += " - " + std::string(track);
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
  if(const auto asset=TrackAsset(s.track);!asset.empty()) {
    a.large_image=asset;a.large_text=Track(s.track);
    if(!a.state.empty())a.large_text+=" - "+a.state;
  }
  if(rider_heading) {
    if(!s.bike.empty())a.state=s.bike;
  } else if(!s.rider.empty()||!s.bike.empty()) {
    if(s.mode>=3&&s.mode<=5)a.details=std::string(Mode(s))+" - "+(s.mode==3?"Practice":s.mode==4?"Qualifying":"Race")+
        (Track(s.track).empty()?std::string{}:" - "+std::string(Track(s.track)));
    a.state=s.rider;
    if(!s.bike.empty()){if(!a.state.empty())a.state+=" - ";a.state+=s.bike;}
    // Discord's text fields are bounded; keep UTF-8 characters whole.
    if(a.state.size()>127){size_t end=124;while((uint8_t(a.state[end])&0xC0)==0x80)--end;a.state.resize(end);a.state+="...";}
  }
  return a;
}
} // namespace motogp::discord
