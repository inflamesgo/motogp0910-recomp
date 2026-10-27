#include "nobk_panel.h"
#include "nobk_panel_visuals.h"
#include "nobk_startup_strip.h"
#include "nobk_friend_toast.h"
#include "button_prompts.h"
#include "gamer_card_flags.h"
#include "online/social.h"
#include "online/discord_invites.h"
#include "achievement_ui.h"
#include "in_game_settings.h"
#include "overlay_theme.h"
#include "quit_menu.h"
#include "../third_party/nlohmann/json.hpp"

#include <rex/cvar.h>
#include <rex/logging.h>
#include <rex/runtime.h>
#include <rex/input/input_system.h>
#include <rex/ui/keybinds.h>
#include <rex/ui/overlay/achievement_icon_cache.h>
#include <rex/ui/window.h>
#include <rex/ui/image_decode.h>
#include <rex/ui/immediate_drawer.h>
#include <imgui.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cfloat>
#include <optional>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <ctime>
#include <deque>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#include <winhttp.h>
#include <wincodec.h>
#endif

REXCVAR_DEFINE_STRING(bind_nobk_panel,"F1","Keybinds","Open the NOBK panel (friends, server, records, achievements)");

namespace motogp::nobk {
namespace {
using namespace motogp::overlay;
using json = nlohmann::json;
using Clock = std::chrono::steady_clock;
struct VisualFonts {ImFontAtlas* atlas=nullptr;std::array<ImFont*,3> faces{};ImFont* startup=nullptr;};
VisualFonts visual_fonts;

// Native track index (view - 1) % 21. Slot 17 is reserved and empty in every package.
constexpr const char* kTracks[21]={"Losail","Motegi","Jerez","Le Mans","Mugello","Catalunya","Assen",
    "Laguna Seca","Sachsenring","Donington Park","Brno","Indianapolis","Misano","Estoril","Phillip Island",
    "Sepang","Valencia","","Silverstone","Shanghai","Istanbul Park"};

std::string Encode(std::string_view text) {
  constexpr char hex[]="0123456789ABCDEF";
  std::string out;
  for(unsigned char ch:text) {
    if((ch>='a'&&ch<='z')||(ch>='A'&&ch<='Z')||(ch>='0'&&ch<='9')||ch=='-'||ch=='_'||ch=='.'||ch=='~')out+=char(ch);
    else {out+='%';out+=hex[ch>>4];out+=hex[ch&15];}
  }
  return out;
}
bool SameName(std::string_view a,std::string_view b) {
  if(a.size()!=b.size())return false;
  for(size_t i=0;i<a.size();++i){auto x=a[i],y=b[i];if(x>='A'&&x<='Z')x+=32;if(y>='A'&&y<='Z')y+=32;if(x!=y)return false;}
  return true;
}
std::string Clean(const json& object,const char* key,size_t limit=40) {
  const auto it=object.find(key);
  if(it==object.end()||!it->is_string())return {};
  auto value=it->get<std::string>();
  if(value.size()>limit)value.resize(limit);
  std::erase_if(value,[](unsigned char ch){return ch<0x20||ch==0x7F;});
  return value;
}
int64_t Int(const json& object,const char* key) {
  const auto it=object.find(key);
  return it!=object.end()&&it->is_number_integer()?it->get<int64_t>():0;
}
std::string CountryCode(std::string value) {
  if(value.size()!=2)return {};
  for(char& c:value){if(c>='a'&&c<='z')c-=32;if(c<'A'||c>'Z')return {};}
  return value=="UK"?"GB":value;
}
std::string MemberSince(int64_t timestamp,bool tr) {
  if(timestamp<946684800||timestamp>=4102444800LL)return {};
  constexpr const char* months[]{"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
  constexpr const char* months_tr[]{"Ocak","Şubat","Mart","Nisan","Mayıs","Haziran","Temmuz","Ağustos","Eylül","Ekim","Kasım","Aralık"};
  const std::chrono::year_month_day date{std::chrono::floor<std::chrono::days>(std::chrono::sys_seconds{std::chrono::seconds{timestamp}})};
  const auto month=unsigned(date.month())-1;
  return std::string(tr?"Üyelik: ":"member since ")+(tr?months_tr[month]:months[month])+" "+std::to_string(int(date.year()));
}
bool OpenUrl(const std::string& url) {
#ifdef _WIN32
  const std::wstring wide(url.begin(),url.end());
  return reinterpret_cast<intptr_t>(ShellExecuteW(nullptr,L"open",wide.c_str(),nullptr,nullptr,SW_SHOWNORMAL))>32;
#else
  (void)url;return false;
#endif
}

// Small synchronous HTTPS GET to www.nobkonline.com on the panel's worker.
// Public read-only endpoints; redirects are never followed. Stop() closes the
// session handle, which aborts a request in progress.
class Http {
 public:
  ~Http(){Close();}
  void Close() {
#ifdef _WIN32
    std::lock_guard lock(mutex_);
    if(connection_)WinHttpCloseHandle(std::exchange(connection_,nullptr));
    if(session_)WinHttpCloseHandle(std::exchange(session_,nullptr));
#endif
    closed_=true;
  }
  // -> HTTP status (0 = network failure); body only for 200.
  int Get(const std::string& path,std::string& body,size_t limit=512*1024) {
    body.clear();
#ifdef _WIN32
    HINTERNET connection=nullptr;
    {
      std::lock_guard lock(mutex_);
      if(closed_)return 0;
      if(!session_) {
        session_=WinHttpOpen(L"MotoGP0910-NOBK/1",WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0);
        if(!session_)session_=WinHttpOpen(L"MotoGP0910-NOBK/1",WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0);
        if(!session_)return 0;
        WinHttpSetTimeouts(session_,3000,4000,4000,5000);
      }
      if(!connection_)connection_=WinHttpConnect(session_,L"www.nobkonline.com",INTERNET_DEFAULT_HTTPS_PORT,0);
      connection=connection_;
    }
    if(!connection)return 0;
    const std::wstring wide(path.begin(),path.end());
    HINTERNET request=WinHttpOpenRequest(connection,L"GET",wide.c_str(),nullptr,WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,WINHTTP_FLAG_SECURE);
    if(!request)return 0;
    struct Closer{HINTERNET h;~Closer(){WinHttpCloseHandle(h);}} closer{request};
    DWORD never=WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
    WinHttpSetOption(request,WINHTTP_OPTION_REDIRECT_POLICY,&never,sizeof(never));
    if(!WinHttpSendRequest(request,WINHTTP_NO_ADDITIONAL_HEADERS,0,WINHTTP_NO_REQUEST_DATA,0,0,0)||
       !WinHttpReceiveResponse(request,nullptr))return 0;
    DWORD status=0,size=sizeof(status);
    if(!WinHttpQueryHeaders(request,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX,&status,&size,WINHTTP_NO_HEADER_INDEX))return 0;
    if(status!=200)return int(status);
    char chunk[8192];
    for(;;) {
      DWORD got=0;
      if(!WinHttpReadData(request,chunk,sizeof(chunk),&got))return 0;
      if(!got)break;
      if(body.size()+got>limit)return 0;
      body.append(chunk,got);
    }
    return 200;
#else
    (void)path;(void)limit;return 0;
#endif
  }
 private:
  std::mutex mutex_;
  bool closed_=false;
#ifdef _WIN32
  HINTERNET session_=nullptr,connection_=nullptr;
#endif
};

struct ProfilePhoto {uint32_t width=0,height=0;std::vector<uint8_t> rgba;};
#ifdef _WIN32
template<class T> struct PhotoCom {
  T* value=nullptr;
  ~PhotoCom(){if(value)value->Release();}
  T** Address(){return &value;}
  T* operator->()const{return value;}
};
#endif
std::shared_ptr<const ProfilePhoto> DecodePhoto(std::string& bytes) {
#ifdef _WIN32
  if(bytes.empty()||bytes.size()>512*1024)return {};
  if(FAILED(CoInitializeEx(nullptr,COINIT_MULTITHREADED)))return {};
  struct Apartment{~Apartment(){CoUninitialize();}} apartment;
  PhotoCom<IWICImagingFactory> factory;
  if(FAILED(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(factory.Address()))))return {};
  PhotoCom<IWICStream> stream;
  if(FAILED(factory->CreateStream(stream.Address()))||FAILED(stream->InitializeFromMemory(reinterpret_cast<BYTE*>(bytes.data()),static_cast<DWORD>(bytes.size()))))return {};
  PhotoCom<IWICBitmapDecoder> decoder;
  if(FAILED(factory->CreateDecoderFromStream(stream.value,nullptr,WICDecodeMetadataCacheOnDemand,decoder.Address())))return {};
  GUID format{};if(FAILED(decoder->GetContainerFormat(&format))||(!IsEqualGUID(format,GUID_ContainerFormatJpeg)&&!IsEqualGUID(format,GUID_ContainerFormatPng)))return {};
  PhotoCom<IWICBitmapFrameDecode> frame;
  if(FAILED(decoder->GetFrame(0,frame.Address())))return {};
  UINT width=0,height=0;
  if(FAILED(frame->GetSize(&width,&height))||!width||!height||width>1024||height>1024)return {};
  // Keep bounded thumbnails in RAM/GPU memory, not full-size account photos.
  PhotoCom<IWICBitmapScaler> scaler;IWICBitmapSource* source=frame.value;
  const UINT edge=std::max(width,height);
  if(edge>128){
    width=std::max(1u,width*128/edge);height=std::max(1u,height*128/edge);
    if(FAILED(factory->CreateBitmapScaler(scaler.Address()))||FAILED(scaler->Initialize(frame.value,width,height,WICBitmapInterpolationModeFant)))return {};
    source=scaler.value;
  }
  PhotoCom<IWICFormatConverter> converter;
  if(FAILED(factory->CreateFormatConverter(converter.Address()))||FAILED(converter->Initialize(source,GUID_WICPixelFormat32bppRGBA,WICBitmapDitherTypeNone,nullptr,0.,WICBitmapPaletteTypeCustom)))return {};
  std::vector<uint8_t> pixels(size_t(width)*height*4);
  if(FAILED(converter->CopyPixels(nullptr,width*4,static_cast<UINT>(pixels.size()),pixels.data())))return {};
  return std::make_shared<ProfilePhoto>(ProfilePhoto{width,height,std::move(pixels)});
#else
  (void)bytes;return {};
#endif
}

// Photos have their own serial worker so profile/image requests never delay
// live presence polling. Only visible rows request them; GPU uploads stay on UI.
class FriendPhotos {
 public:
  explicit FriendPhotos(bool enabled):enabled_(enabled){
#ifdef MOTOGP_NOBK_VISUAL_PROBE
    enabled_=false; // Synthetic presentation fixtures never request public photos.
#endif
  }
  ~FriendPhotos(){Stop();}
  void SetOpen(bool open){
    std::lock_guard lock(mutex_);if(open_==open)return;open_=open;changed_.notify_one();
  }
  std::shared_ptr<const ProfilePhoto> Get(const std::string& name){
    if(!enabled_||name.empty())return {};
    std::string key=name;for(char& c:key)if(c>='A'&&c<='Z')c+=32;
    std::lock_guard lock(mutex_);if(stopping_||!open_)return {};
    if(!entries_.contains(key)&&entries_.size()>=128){
      auto oldest=entries_.end();
      for(auto it=entries_.begin();it!=entries_.end();++it)if(!it->second.queued&&(oldest==entries_.end()||it->second.used<oldest->second.used))oldest=it;
      if(oldest==entries_.end())return {};entries_.erase(oldest);
    }
    auto& entry=entries_[key];const auto now=Clock::now();entry.used=now;
    if(!entry.queued&&now>=entry.refresh){
      if(!worker_.joinable()){
        try{worker_=std::thread([this]{Run();});}catch(...){enabled_=false;return {};}
      }
      entry.name=name;entry.queued=true;pending_.push_back(key);changed_.notify_one();
    }
    return entry.photo;
  }
 private:
  struct Entry {std::string name;std::shared_ptr<const ProfilePhoto> photo;Clock::time_point refresh{},used{};bool queued=false;};
  std::pair<std::shared_ptr<const ProfilePhoto>,bool> FetchPhoto(const std::string& name){
    std::string body,path="/api/avatar/u/"+Encode(name);
    const auto status=http_.Get("/api/fifa09/player?name="+Encode(name),body,256*1024);
    if(status==200){
      const auto account=json::parse(body,nullptr,false);
      if(!account.is_object()||!SameName(Clean(account,"player"),name))return {{},false};
      const auto avatar=account.find("avatar");
      if(avatar!=account.end()&&avatar->is_string()){
        path=avatar->get<std::string>();if(path.empty())return {{},true};
        for(std::string_view origin:{"https://www.nobkonline.com","https://nobkonline.com"})if(path.starts_with(origin)){path.erase(0,origin.size());break;}
        if(!path.starts_with("/api/avatar/")||path.size()>200||path.find("..")!=std::string::npos||path.find('#')!=std::string::npos||path.find('\\')!=std::string::npos||
           std::any_of(path.begin(),path.end(),[](unsigned char c){return c<=0x20||c>=0x7F;}))return {{},false};
      }
    }else if(status!=404)return {{},false};
    const auto image_status=http_.Get(path,body,512*1024);
    if(image_status!=200)return {{},image_status==404};
    auto photo=DecodePhoto(body);const bool complete=bool(photo);return {std::move(photo),complete};
  }
  void Run(){
    for(;;){
      std::string key,name;
      {
        std::unique_lock lock(mutex_);changed_.wait(lock,[&]{return stopping_||(open_&&!pending_.empty());});
        if(stopping_)return;key=std::move(pending_.front());pending_.pop_front();name=entries_.at(key).name;
      }
      std::pair<std::shared_ptr<const ProfilePhoto>,bool> result;
      try{result=FetchPhoto(name);}catch(...){result={{},false};}
      {
        std::lock_guard lock(mutex_);if(stopping_)return;
        auto& entry=entries_.at(key);entry.queued=false;
        if(result.second)entry.photo=std::move(result.first);
        entry.refresh=Clock::now()+(result.second?std::chrono::minutes(5):std::chrono::minutes(1));
      }
    }
  }
  void Stop(){
    {std::lock_guard lock(mutex_);stopping_=true;changed_.notify_one();}
    http_.Close();if(worker_.joinable())worker_.join();
  }
  bool enabled_=false,open_=false,stopping_=false;
  std::mutex mutex_;std::condition_variable changed_;
  std::map<std::string,Entry> entries_;std::deque<std::string> pending_;
  Http http_;std::thread worker_;
};

enum class Where { Offline, Site, Menus, Room, Racing };
struct Friend { std::string name,country,room; Where where=Where::Offline; };
struct Room { std::string name,host; int members=0,capacity=0; bool racing=false,friends=false,mine=false; };
struct Recent { std::string name,board,time; int rank=0; int64_t at=0; bool mine=false,friend_=false; };
struct Event { std::string text; int64_t at=0; int kind=0; std::string friend_name,invite_id; };  // kind 0 info, 1 good, 2 bad
struct Snapshot {
  bool available=false,loaded=false,failed=false;
  std::string me;
  std::string country;
  int64_t member_since=0;
  std::vector<Friend> friends;
  std::vector<Room> rooms;
  std::vector<Recent> recent;
  std::deque<Event> events;
  int online=0,open_rooms=0,racing=0,riders=0,records=0;
  int my_records=-1,my_boards=0,my_podiums=0;
  bool server_up=false;
  uint64_t serial=0;
};
#ifdef MOTOGP_NOBK_VISUAL_PROBE
std::optional<Snapshot> visual_fixture;
#endif

std::string BoardName(const json& row,bool tr) {
  auto track=Clean(row,"track",32);
  const auto view=Int(row,"view");
  if(view>=1) {const auto i=(view-1)%21;if(kTracks[i][0])track=kTracks[i];}
  if(track.empty())track=std::string(tr?"Tablo #":"Board #")+std::to_string(view);
  const auto cls=Clean(row,"class",16);
  return cls.empty()?track:track+" · "+cls;
}
std::string Ago(int64_t when,bool tr) {
  const auto now=int64_t(std::time(nullptr));
  const auto s=std::max<int64_t>(0,now-when);
  if(s<60)return tr?"az önce":"just now";
  if(s<3600)return std::to_string(s/60)+(tr?" dk":" min");
  if(s<86400)return std::to_string(s/3600)+(tr?" sa":" h");
  return std::to_string(s/86400)+(tr?" gün":" d");
}

class State {
 public:
  State(std::shared_ptr<settings::InputGate> input_gate):gate(std::move(input_gate)) {
    // Independent servers reuse names and IDs; only the production service
    // maps to nobkonline.com accounts.
    const auto origin=rex::cvar::Query<std::string>("online_server");
    snapshot_.available=rex::cvar::Query<std::string>("network_mode")=="online"&&
        (origin=="https://motogp.nobkonline.com"||origin=="https://motogp.nobkonline.com/");
    snapshot_.me=rex::cvar::Query<std::string>("lan_player_name");
    if(snapshot_.available&&!snapshot_.me.empty())worker_=std::thread([this]{Run();});
  }
  ~State(){Stop();}
  void Toggle() {
    if(gate->IsOpen()){gate->Close();return;}
    if(!snapshot_.available||quit::Pending()||settings::GuestInputBlocked())return;
    gate->Open();
    if(gate->IsOpen()){std::lock_guard lock(mutex_);wake_=true;changed_.notify_one();}
  }
  void Close(){gate->Close();}
  Snapshot Read(){std::lock_guard lock(mutex_);return snapshot_;}
#ifdef MOTOGP_NOBK_VISUAL_PROBE
  void ApplyVisualPresence(const char* live,const char* friends,bool got_live,bool got_friends){
    const std::lock_guard lock(mutex_);
    Apply(json::parse(live),got_live,json{},false,json::parse(friends),got_friends,
        json{},false,json{},false,false);
  }
#endif
  bool PopToast(Event& out) {
    std::lock_guard lock(mutex_);
    if(toasts_.empty())return false;
    out=toasts_.front();toasts_.pop_front();return true;
  }
  void Stop() {
    {std::lock_guard lock(mutex_);stopping_=true;}
    changed_.notify_one();
    http_.Close();
    if(worker_.joinable())worker_.join();
    gate->Close();
  }
  void PollPad(rex::input::InputSystem* input) {
    if(!input||!snapshot_.available)return;
    const auto epoch=gate->Epoch();
    if(pad_epoch_!=epoch){pad_epoch_=epoch;combo_since_.reset();combo_used_=true;}
    if(!gate->Focused()){combo_since_.reset();combo_used_=true;return;}
    const auto now=Clock::now();
    if(now<next_pad_)return;
    next_pad_=now+std::chrono::milliseconds(50);
    bool held=false;
    {
      const std::lock_guard lock(settings::InputValueMutex());
      for(uint32_t user=0;user<4&&!held;++user) {
        rex::input::X_INPUT_STATE state{};
        if(input->GetStateForUI(user,&state)!=0)continue;
        const auto b=state.gamepad.buttons;
        held=(b&rex::input::X_INPUT_GAMEPAD_BACK)!=0;
      }
    }
    if(!held){combo_since_.reset();combo_used_=false;return;}
    if(!combo_since_)combo_since_=now;
    if(!combo_used_&&now-*combo_since_>=std::chrono::milliseconds(450)&&!gate->IsOpen()){combo_used_=true;Toggle();}
  }
  const std::shared_ptr<settings::InputGate> gate;

 private:
  void Push(std::string text,int kind,bool toast=true,std::string friend_name={}) {
    // mutex_ held by caller
    Event e{std::move(text),int64_t(std::time(nullptr)),kind,std::move(friend_name)};
    snapshot_.events.push_front(e);
    while(snapshot_.events.size()>30)snapshot_.events.pop_back();
    if(toast&&toasts_.size()<6)toasts_.push_back(std::move(e));
  }
  bool Fetch(const std::string& path,json& out) {
    std::string body;
    const auto status=http_.Get(path,body);
    if(status!=200)return false;
    out=json::parse(body,nullptr,false);
    return !out.is_discarded()&&out.is_object();
  }
  void Run() {
    Clock::time_point next_friends{},next_me{},next_recent{},next_account{};
    bool first=true;
    for(;;) {
      {
        std::unique_lock lock(mutex_);
        if(!first) {
          const auto wait=gate->IsOpen()?std::chrono::seconds(5):std::chrono::seconds(20);
          changed_.wait_for(lock,wait,[&]{return stopping_||wake_;});
        }
        if(stopping_)return;
        wake_=false;
      }
      const bool open=gate->IsOpen();
      const auto now=Clock::now();
      json live,status,friends,me,recent;
      const bool got_live=Fetch("/api/motogp/live",live);
      const bool got_status=open?Fetch("/api/motogp/status",status):false;
      const bool got_friends=now>=next_friends&&Fetch("/api/account/friends_of?name="+Encode(snapshot_.me),friends);
      if(got_friends)next_friends=now+std::chrono::seconds(open?20:60);
      const bool got_me=now>=next_me&&Fetch("/api/motogp/player?name="+Encode(snapshot_.me),me);
      if(got_me)next_me=now+std::chrono::seconds(open?15:45);
      const bool got_recent=open&&now>=next_recent&&Fetch("/api/motogp/recent?limit=12",recent);
      if(got_recent)next_recent=now+std::chrono::seconds(15);
      {
        std::lock_guard lock(mutex_);
        if(stopping_)return;
        Apply(live,got_live,status,got_status,friends,got_friends,me,got_me,recent,got_recent,first);
      }
      first=false;
      // Optional shared account metadata uses the same public name-based
      // endpoint as Rider License. Publish live data first; no UI-thread I/O,
      // launcher credentials or extra per-frame work is introduced.
      if(open&&Clock::now()>=next_account){
        json account;
        const bool got_account=Fetch("/api/fifa09/player?name="+Encode(snapshot_.me),account)&&
            SameName(Clean(account,"player"),snapshot_.me);
        next_account=Clock::now()+(got_account?std::chrono::minutes(5):std::chrono::minutes(1));
        if(got_account){
          std::lock_guard lock(mutex_);
          if(stopping_)return;
          if(account.contains("country")&&account["country"].is_string())snapshot_.country=CountryCode(Clean(account,"country",8));
          if(account.contains("member_since")&&account["member_since"].is_number_integer()){
            const auto joined=Int(account,"member_since");
            snapshot_.member_since=joined>=946684800&&joined<4102444800LL?joined:0;
          }
          ++snapshot_.serial;
        }
      }
    }
  }
  void Apply(const json& live,bool got_live,const json& status,bool got_status,const json& friends,bool got_friends,
      const json& me,bool got_me,const json& recent,bool got_recent,bool first) {
    auto& s=snapshot_;
    s.failed=!got_live;
    if(got_friends&&friends.contains("friends")&&friends["friends"].is_array()) {
      std::vector<Friend> list;
      for(const auto& f:friends["friends"]) {
        if(!f.is_object())continue;
        Friend item{Clean(f,"username"),Clean(f,"country",2),{},f.value("online",false)?Where::Site:Where::Offline};
        if(!item.name.empty()&&list.size()<100)list.push_back(std::move(item));
      }
      // keep the MotoGP presence of the previous poll until live data merges below
      for(auto& f:list)for(const auto& old:s.friends)if(SameName(f.name,old.name)&&old.where>=Where::Menus){f.where=old.where;f.room=old.room;}
      s.friends=std::move(list);
    }
    if(got_live) {
      std::map<std::string,std::pair<Where,std::string>> where;   // lower-case name -> presence
      auto lower=[](std::string v){for(auto& c:v)if(c>='A'&&c<='Z')c+=32;return v;};
      if(live.contains("online")&&live["online"].is_array())
        for(const auto& o:live["online"])if(o.is_object())where[lower(Clean(o,"name"))]={Where::Menus,{}};
      std::vector<Room> rooms;
      if(live.contains("rooms")&&live["rooms"].is_array())for(const auto& r:live["rooms"]) {
        if(!r.is_object())continue;
        Room room{Clean(r,"name",31),Clean(r,"host_name"),0,int(Int(r,"capacity")),r.value("in_race",false),false,false};
        if(r.contains("members")&&r["members"].is_array())for(const auto& m:r["members"]) {
          if(!m.is_object())continue;
          ++room.members;
          const auto name=Clean(m,"name");
          where[lower(name)]={room.racing?Where::Racing:Where::Room,room.name};
          if(SameName(name,s.me))room.mine=true;
          for(const auto& f:s.friends)if(SameName(f.name,name))room.friends=true;
        }
        if(rooms.size()<40)rooms.push_back(std::move(room));
      }
      std::stable_sort(rooms.begin(),rooms.end(),[](const Room& a,const Room& b){
        if(a.mine!=b.mine)return a.mine;if(a.friends!=b.friends)return a.friends;return a.members>b.members;});
      for(auto& f:s.friends) {
        const auto before=f.where;const auto before_room=f.room;
        const auto it=where.find(lower(f.name));
        if(it!=where.end()){f.where=it->second.first;f.room=it->second.second;}
        else if(f.where>=Where::Menus){f.where=Where::Offline;f.room.clear();}
        if(first||!s.loaded)continue;
        bool hosts=false;for(const auto& r:rooms)if(SameName(r.host,f.name)&&r.name==f.room)hosts=true;
        if(hosts&&f.where==Where::Room&&(before<Where::Room||before_room!=f.room))
          Push(f.name+" ▸ "+f.room,1);
      }
      // Keep a trusted per-friend baseline across failed/stale live reads.
      // First sight of an account seeds it, including a delayed friends list;
      // only a known offline friend entering MotoGP generates this toast.
      if(!live.value("stale",false)){
        std::map<std::string,bool> next_presence;
        for(const auto& f:s.friends){
          const auto key=lower(f.name);const bool online=f.where>=Where::Menus;
          const auto previous=online_presence_.find(key);
          if(previous!=online_presence_.end()&&!previous->second&&online)
            Push(f.name+" · MotoGP",0,true,f.name);
          next_presence[key]=online;
        }
        online_presence_=std::move(next_presence);
      }
      std::stable_sort(s.friends.begin(),s.friends.end(),[](const Friend& a,const Friend& b){
        if(a.where!=b.where)return a.where>b.where;return a.name<b.name;});
      s.rooms=std::move(rooms);
      s.server_up=!live.value("stale",false);
      s.online=int(where.size());
    }
    if(got_status) {
      s.online=int(Int(status,"online"));s.open_rooms=int(Int(status,"rooms"));s.racing=int(Int(status,"in_race"));
      s.riders=int(Int(status,"racers"));s.records=int(Int(status,"records"));s.server_up=status.value("server_up",false);
    } else if(got_live) {
      s.open_rooms=int(s.rooms.size());s.racing=int(std::count_if(s.rooms.begin(),s.rooms.end(),[](const Room& r){return r.racing;}));
    }
    if(got_me&&me.contains("records")&&me["records"].is_array()) {
      std::map<int64_t,std::string> held;
      int podiums=0,boards=0;
      for(const auto& r:me["records"]) {
        if(!r.is_object())continue;
        ++boards;
        const auto rank=Int(r,"rank");
        if(rank>=1&&rank<=3)++podiums;
        if(rank==1)held[Int(r,"view")]=BoardName(r,tr_);
      }
      if(s.my_records>=0&&!first) {
        for(const auto& [view,name]:held)if(!held_.count(view))Push((tr_?"Yeni rekor: ":"New record: ")+name,1);
        for(const auto& [view,name]:held_)if(!held.count(view))Push((tr_?"Rekorun geçildi: ":"Your record was beaten: ")+name,2);
      }
      held_=std::move(held);
      s.my_records=int(held_.size());s.my_boards=boards;s.my_podiums=podiums;
    } else if(got_me) { s.my_records=0; }
    if(got_recent&&recent.contains("records")&&recent["records"].is_array()) {
      std::vector<Recent> list;
      for(const auto& r:recent["records"]) {
        if(!r.is_object())continue;
        Recent item{Clean(r,"name"),BoardName(r,tr_),{},int(Int(r,"rank")),Int(r,"updated"),false,false};
        if(r.contains("display")&&r["display"].is_object())item.time=Clean(r["display"],"text",16);
        item.mine=SameName(item.name,s.me);
        for(const auto& f:s.friends)if(SameName(f.name,item.name))item.friend_=true;
        list.push_back(std::move(item));
      }
      s.recent=std::move(list);
    }
    if(got_live)s.loaded=true;
    ++s.serial;
  }

 public:
  // The UI publishes the language while the public-data worker may be reading it.
  std::atomic<bool> tr_{false};
 private:
  std::mutex mutex_;
  std::condition_variable changed_;
  bool stopping_=false,wake_=false;
  Snapshot snapshot_;
  std::deque<Event> toasts_;
  std::map<int64_t,std::string> held_;
  std::map<std::string,bool> online_presence_;
  Http http_;
  std::thread worker_;
  Clock::time_point next_pad_{};
  std::optional<Clock::time_point> combo_since_;
  bool combo_used_=false;
  uint64_t pad_epoch_=UINT64_MAX;
};

std::atomic<std::shared_ptr<State>> current;
std::atomic<rex::input::InputSystem*> input_system{nullptr};

ImU32 Rgb(int r,int g,int b,int a=255){return IM_COL32(r,g,b,a);}
const char* WhereText(const Friend& f,bool tr) {
  switch(f.where) {
    case Where::Racing:return tr?"Yarışta":"Racing";
    case Where::Room:return tr?"Lobide":"In a lobby";
    case Where::Menus:return tr?"Menülerde":"In menus";
    case Where::Site:return tr?"FIFA 09'da":"In FIFA 09";  // friends_of: online in the FIFA 09 lobby
    default:return tr?"Çevrimdışı":"Offline";
  }
}
ImU32 WhereColor(Where w) {
  switch(w) {
    case Where::Racing:return Rgb(255,106,19);
    case Where::Room:return Rgb(106,225,18);
    case Where::Menus:return Rgb(255,190,12);
    case Where::Site:return Rgb(96,165,250);
    default:return Rgb(90,96,110);
  }
}

class Dialog final : public rex::ui::ImGuiDialog, public rex::ui::WindowInputListener {
 public:
  Dialog(rex::ui::ImGuiDrawer* drawer,rex::ui::Window* window,rex::system::AchievementManager* achievements,
      std::shared_ptr<State> state,ImFont* font,ImFont* heading,bool tr,
      rex::ui::ImmediateDrawer* immediate,rex::Runtime* runtime,std::shared_ptr<achievements::Preferences> preferences)
      :ImGuiDialog(drawer),window_(window),achievements_(achievements),state_(std::move(state)),font_(font),heading_(heading),tr_(tr),
       achievement_preferences_(std::move(preferences)),achievement_icons_(immediate,runtime),
       immediate_(immediate),friend_photos_(state_->Read().available) {
    state_->tr_=tr;
    if(immediate) {
      int width=0,height=0;
      const auto rgba=rex::ui::DecodeImageRGBA(gamer_card::flags::png,sizeof(gamer_card::flags::png),width,height);
      if(width==768&&height==544&&!rgba.empty())flag_texture_=immediate->CreateTexture(width,height,rex::ui::ImmediateTextureFilter::kLinear,false,rgba.data());
    }
    if(window_)window_->AddInputListener(this,61);
  }
  ~Dialog() override {Backdrop(0);if(window_)window_->RemoveInputListener(this);RestoreNavigation(GetIO());state_->Close();}
  void OnKeyDown(rex::ui::KeyEvent& e) override {
    const auto bind=rex::ui::ParseVirtualKey(rex::cvar::Query<std::string>("bind_nobk_panel"));
    if(bind!=rex::ui::VirtualKey::kNone&&e.virtual_key()==bind&&!e.is_alt_pressed()&&!e.is_ctrl_pressed()) {
      if(!e.prev_state())state_->Toggle();
      e.set_handled(true);return;
    }
    if(state_->gate->IsOpen()&&!(e.is_alt_pressed()&&e.virtual_key()==rex::ui::VirtualKey::kF4))e.set_handled(true);
  }
  void OnKeyChar(rex::ui::KeyEvent& e) override {if(state_->gate->IsOpen())e.set_handled(true);}
  void OnMouseDown(rex::ui::MouseEvent& e) override {if(state_->gate->IsOpen())e.set_handled(true);}

 protected:
  void OnDraw(ImGuiIO& io) override {
    photo_uploads_=0;friend_toast_visible_=false;
    if(!startup_finished_){
      auto snapshot=state_->Read();
#ifdef MOTOGP_NOBK_VISUAL_PROBE
      if(visual_fixture)snapshot=*visual_fixture;
#endif
      DrawStartupStrip(io,snapshot);
    }
    DrawToast(io);
    const bool invite_request=online::social::ConsumeOpenRequest();
    if(invite_request){if(!state_->gate->IsOpen())state_->Toggle();tab_=0;selected_=0;invite_picker_=true;}
    const bool open=state_->gate->IsOpen();friend_photos_.SetOpen(open||friend_toast_visible_);
    if(!open&&!friend_toast_visible_&&!toast_photo_.empty()){
      photo_textures_.erase(toast_photo_);toast_photo_.clear();
    }
    if(!open){Backdrop(0);RestoreNavigation(io);armed_=false;previous_=0;return;}
    auto snapshot=state_->Read();
#ifdef MOTOGP_NOBK_VISUAL_PROBE
    if(visual_fixture)snapshot=*visual_fixture;
#endif
    if(photo_serial_!=snapshot.serial){
      photo_serial_=snapshot.serial;
      std::erase_if(photo_textures_,[&](const auto& entry){return std::none_of(snapshot.friends.begin(),snapshot.friends.end(),[&](const Friend& f){return f.name==entry.first;});});
    }
    if(epoch_!=state_->gate->Epoch()){epoch_=state_->gate->Epoch();armed_=false;previous_=0;selected_=0;invite_picker_=invite_request;opened_at_=ImGui::GetTime();tab_at_=opened_at_;animated_tab_=-1;ImGui::SetNextWindowFocus();}
    const auto buttons=state_->gate->Buttons();
    if(!armed_&&state_->gate->NeutralReady()&&!buttons)armed_=true;
    const uint16_t fresh=armed_?buttons&~previous_:0;
    previous_=buttons;
    reveal_selected_=false;
    using namespace rex::input;
    if(armed_) {
      if((fresh&X_INPUT_GAMEPAD_B)||ImGui::IsKeyPressed(ImGuiKey_Escape,false)){Backdrop(0);state_->Close();RestoreNavigation(io);return;}
      const bool typing=io.WantTextInput;
      if((fresh&X_INPUT_GAMEPAD_LEFT_SHOULDER)||(!typing&&ImGui::IsKeyPressed(ImGuiKey_Q,false))){tab_=(tab_+3)%4;selected_=0;}
      if((fresh&X_INPUT_GAMEPAD_RIGHT_SHOULDER)||(!typing&&ImGui::IsKeyPressed(ImGuiKey_E,false))){tab_=(tab_+1)%4;selected_=0;}
      if(tab_!=3&&((fresh&X_INPUT_GAMEPAD_DPAD_DOWN)||ImGui::IsKeyPressed(ImGuiKey_DownArrow))){++selected_;reveal_selected_=true;}
      if(tab_!=3&&((fresh&X_INPUT_GAMEPAD_DPAD_UP)||ImGui::IsKeyPressed(ImGuiKey_UpArrow))){--selected_;reveal_selected_=true;}
      activate_=(fresh&X_INPUT_GAMEPAD_A)||ImGui::IsKeyPressed(ImGuiKey_Enter,false);
      decline_=(fresh&X_INPUT_GAMEPAD_X)||ImGui::IsKeyPressed(ImGuiKey_Delete,false);
      web_=(fresh&X_INPUT_GAMEPAD_Y)!=0;
    } else activate_=web_=decline_=false;
    AchievementNavigation(io,armed_?buttons:0);
    Draw(io,snapshot);
    if(tab_!=3)RestoreNavigation(io);
  }

 private:
  void DrawStartupStrip(ImGuiIO& io,const Snapshot& snapshot) {
    if(!snapshot.available){startup_finished_=true;return;}
    if(!std::isfinite(io.DisplaySize.x)||!std::isfinite(io.DisplaySize.y)||
        io.DisplaySize.x<=0||io.DisplaySize.y<=0)return;
    if(!startup_started_){
      if(snapshot.me.empty())return;
      startup_started_=true;
    }
    // Loading can stall presentation for seconds. Advance this short, one-shot
    // animation by bounded draw steps so a stalled frame cannot consume its
    // entrance or entire lifetime. Other overlays keep their existing clocks.
    const float step=std::isfinite(io.DeltaTime)?std::clamp(io.DeltaTime,0.f,1.f/30.f):0.f;
    startup_elapsed_+=step;
    const bool show=startup_elapsed_<3.&&!state_->gate->Blocking()&&
        !settings::GuestInputBlocked()&&!quit::Pending();
    startup_phase_=std::clamp(startup_phase_+(show?1.f:-1.f)*step/.3f,0.f,1.f);
    if(startup_phase_<=0){startup_finished_=startup_elapsed_>=3.;return;}
    const int friends=int(std::count_if(snapshot.friends.begin(),snapshot.friends.end(),
        [](const Friend& item){return item.where!=Where::Offline;}));
    const auto invites=online::social::Read();
    auto key=rex::cvar::Query<std::string>("bind_nobk_panel");if(key.empty())key="F1";
    auto* face=visual_fonts.atlas==io.Fonts&&visual_fonts.startup?visual_fonts.startup:Face(1);
    const char* controller_hint="";
    switch(prompts::CurrentStyle()) {
      case prompts::Style::Xbox:
        controller_hint=Tr(tr_,"Controller: hold Back","Kontrolcü: Back'i basılı tut");break;
      case prompts::Style::PlayStation:
        controller_hint=Tr(tr_,"Controller: hold Select","Kontrolcü: Select'i basılı tut");break;
      case prompts::Style::Keyboard:break;
    }
    startup::Draw(ImGui::GetForegroundDrawList(),face,io.DisplaySize,visual::Smooth(startup_phase_),
        friends,int(invites.invites.size()),key,controller_hint,tr_);
  }
  void Backdrop(float alpha) {
#if defined(_WIN32) && !defined(MOTOGP_NOBK_VISUAL_PROBE)
    using SetBackdrop=uint32_t(*)(uint32_t,float,float,float,float);
    static const auto set=[] {
      const auto module=GetModuleHandleW(L"rexruntime.dll");
      const auto function=module?reinterpret_cast<SetBackdrop>(GetProcAddress(module,"Motogp_SetPanelBackdrop")):nullptr;
      if(!function)REXLOG_WARN("F1 backdrop blur requires the matching updated runtime; retaining the soft contrast scrim");
      return function;
    }();
    if(set){
      const auto values=visual::BackdropFor(layout_,GetIO().DisplaySize);
      set(1,.96f*alpha,values.fade_begin,values.fade_end,values.radius);
    }
#endif
  }
  void RestoreNavigation(ImGuiIO& io) {
    if(!nav_owned_)return;
    achievement_preferences_->Save(); // Commit a slider edit when leaving the tab or closing F1.
    for(auto key:{ImGuiKey_GamepadDpadUp,ImGuiKey_GamepadDpadDown,ImGuiKey_GamepadDpadLeft,ImGuiKey_GamepadDpadRight,ImGuiKey_GamepadFaceDown})io.AddKeyEvent(key,false);
    io.ConfigFlags=old_flags_;io.BackendFlags=old_backend_;nav_owned_=false;nav_previous_=0;
  }
  void AchievementNavigation(ImGuiIO& io,uint16_t buttons) {
    if(tab_!=3){RestoreNavigation(io);return;}
    if(!nav_owned_) {
      old_flags_=io.ConfigFlags;old_backend_=io.BackendFlags;
      io.ConfigFlags|=ImGuiConfigFlags_NavEnableKeyboard|ImGuiConfigFlags_NavEnableGamepad;
      io.BackendFlags|=ImGuiBackendFlags_HasGamepad;nav_owned_=true;
    }
    using namespace rex::input;
    for(auto [bit,key]:{std::pair{X_INPUT_GAMEPAD_DPAD_UP,ImGuiKey_GamepadDpadUp},
        {X_INPUT_GAMEPAD_DPAD_DOWN,ImGuiKey_GamepadDpadDown},{X_INPUT_GAMEPAD_DPAD_LEFT,ImGuiKey_GamepadDpadLeft},
        {X_INPUT_GAMEPAD_DPAD_RIGHT,ImGuiKey_GamepadDpadRight},{X_INPUT_GAMEPAD_A,ImGuiKey_GamepadFaceDown}})
      if((buttons&bit)!=(nav_previous_&bit))io.AddKeyEvent(key,bool(buttons&bit));
    nav_previous_=buttons;
  }
  void DrawToast(ImGuiIO& io) {
    const double now=ImGui::GetTime();
    const auto expired=[&]{return !toast_||now-toast_at_>ToastDuration();};
    const auto discord=online::discord_invites::ReadNotice();
    if(discord.serial!=discord_notice_){
      discord_notice_=discord.serial;
      using Outcome=online::discord_invites::Outcome;
      const char* message=nullptr;int kind=0;
      switch(discord.outcome){
        case Outcome::Pending:message=Tr(tr_,"Joining the Discord lobby invitation…","Discord lobi davetine katılınıyor…");break;
        case Outcome::Accepted:message=Tr(tr_,"Discord invitation accepted. Connecting to the lobby…","Discord daveti kabul edildi. Lobiye bağlanılıyor…");kind=1;break;
        case Outcome::AlreadyJoined:message=Tr(tr_,"You are already in this lobby.","Zaten bu lobidesin.");break;
        case Outcome::OnlineRequired:message=Tr(tr_,"Sign in through the launcher and start Online mode to accept Discord invitations.","Discord davetlerini kabul etmek için launcher'dan giriş yapıp Online modunda başlat.");kind=2;break;
        case Outcome::Unsupported:message=Tr(tr_,"This server needs the Discord invitation update.","Bu sunucunun Discord daveti güncellemesine ihtiyacı var.");kind=2;break;
        case Outcome::Unavailable:message=Tr(tr_,"Discord invitation unavailable. The lobby may have closed, filled up or started racing.","Discord daveti kullanılamıyor. Lobi kapanmış, dolmuş veya yarış başlamış olabilir.");kind=2;break;
        case Outcome::None:break;
      }
      if(message){toast_=Event{message,0,kind};toast_at_=now;}
      if(discord.outcome==Outcome::Pending||discord.outcome==Outcome::Accepted)state_->Close();
    }
    const auto social=online::social::Read();
    std::set<std::string> pending;
    for(const auto& invite:social.invites)pending.insert(invite.id);
    std::erase_if(invite_notified_,[&](const auto& id){return !pending.contains(id);});
    if(toast_&&!toast_->invite_id.empty()&&!pending.contains(toast_->invite_id))toast_.reset();
    if(expired())for(const auto& invite:social.invites)if(!invite_notified_.contains(invite.id)) {
      invite_notified_.insert(invite.id);
      toast_=Event{{},0,1,invite.from,invite.id};toast_at_=now;break;
    }
    if(expired()) {
      Event next;
      if(state_->PopToast(next)){toast_=next;toast_at_=now;}
      else toast_.reset();
    }
    if(!toast_||state_->gate->IsOpen())return;
    const float t=float(now-toast_at_);
    if(!toast_->friend_name.empty()){
      // Match the startup strip: friend presence belongs to production Online
      // sessions, even if an event remains queued in an unavailable session.
      if(!state_->Read().available){toast_.reset();return;}
      friend_toast_visible_=true;friend_photos_.SetOpen(true);
      if(toast_photo_!=toast_->friend_name){
        if(!toast_photo_.empty())photo_textures_.erase(toast_photo_);
        toast_photo_=toast_->friend_name;
      }
      const auto* photo=FriendPhoto(toast_->friend_name);
      const float u=photo&&photo->width>photo->height?(1.f-float(photo->height)/photo->width)*.5f:0;
      const float v=photo&&photo->height>photo->width?(1.f-float(photo->width)/photo->height)*.5f:0;
      // The runtime uploads its font atlas once. Reuse the same semibold
      // family at its existing high-resolution locked bake for all sizes.
      friend_toast::Draw(ImGui::GetForegroundDrawList(),Face(1),io.DisplaySize,t,
          toast_->friend_name,ToastActivity(),reinterpret_cast<ImTextureID>(photo),
          ImVec2(u,v),ImVec2(1-u,1-v),toast_->invite_id.empty()?
          Tr(tr_,"FRIEND ONLINE","ARKADAŞ ÇEVRİMİÇİ"):Tr(tr_,"LOBBY INVITE","LOBİ DAVETİ"),!toast_->invite_id.empty());
      return;
    }
    const float alpha=std::clamp(std::min(t/.25f,(6.f-t)/.4f),0.f,1.f);
    Theme theme(font_);
    const ImVec2 size(360,0);
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x-24,24),ImGuiCond_Always,ImVec2(1,0));
    ImGui::SetNextWindowSize(size);
    ImGui::SetNextWindowBgAlpha(.94f*alpha);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha,alpha);
    if(ImGui::Begin("##nobk_toast",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoInputs|ImGuiWindowFlags_NoFocusOnAppearing|
        ImGuiWindowFlags_NoNav|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_AlwaysAutoResize)) {
      Brand(14.f);
      const ImVec4 color=toast_->kind==2?ImVec4(1,.45f,.4f,1):toast_->kind==1?orange:white;
      ImGui::PushTextWrapPos(ImGui::GetCursorPosX()+312);
      ImGui::TextColored(color,"%s",toast_->text.c_str());
      ImGui::PopTextWrapPos();
      auto bind=rex::cvar::Query<std::string>("bind_nobk_panel");if(bind.empty())bind="F1";
      ImGui::TextDisabled("%s",(bind+(tr_?" · panel":" · panel")).c_str());
    }
    ImGui::End();
    ImGui::PopStyleVar();
  }
  std::string FriendToastActivity()const {
    return Tr(tr_,"Playing MotoGP 09/10","MotoGP 09/10 oynuyor");
  }
  std::string ToastActivity()const {
    if(!toast_||toast_->invite_id.empty())return FriendToastActivity();
    auto key=rex::cvar::Query<std::string>("bind_nobk_panel");if(key.empty())key="F1";
    return key+Tr(tr_," · Respond"," · Yanıtla");
  }
  float ToastDuration()const {
    return toast_&&!toast_->friend_name.empty()?friend_toast::Duration(toast_->friend_name+" "+ToastActivity()):6.f;
  }
  void Brand(float size) {
    // NOBK·FRIENDS wordmark: caps, orange dot.
    auto* draw=ImGui::GetWindowDrawList();
    const auto pos=ImGui::GetCursorScreenPos();
    ImFont* f=heading_?heading_:ImGui::GetFont();
    const char* a="NOBK";const char* b="FRIENDS";
    const auto wa=f->CalcTextSizeA(size,FLT_MAX,0,a).x;
    draw->AddText(f,size,pos,ImGui::GetColorU32(white),a);
    draw->AddCircleFilled(ImVec2(pos.x+wa+size*.32f,pos.y+size*.55f),size*.13f,ImGui::GetColorU32(orange));
    draw->AddText(f,size,ImVec2(pos.x+wa+size*.64f,pos.y),ImGui::GetColorU32(white),b);
    const auto wb=f->CalcTextSizeA(size,FLT_MAX,0,b).x;
    ImGui::Dummy(ImVec2(wa+size*.64f+wb,size*1.1f));
  }
  ImFont* Face(int weight=0) const {
    if(visual_fonts.atlas==ImGui::GetIO().Fonts&&visual_fonts.faces[weight])return visual_fonts.faces[weight];
    return weight&&heading_?heading_:(font_?font_:ImGui::GetFont());
  }
  void Header(const Snapshot& snapshot) {
    const float s=layout_.scale,shift=-18*s*(1-alpha_);
    auto* draw=ImGui::GetWindowDrawList();
    const ImVec2 logo(94*s+shift,76*s);
    float x=logo.x;
    for(int i=0;i<3;++i) {
      const char* word=i==0?"NOBK":i==1?".":"FRIENDS";
      ImFont* f=Face(i==0?2:0);const float size=40*s;
      draw->AddText(f,size,ImVec2(x,logo.y),ImGui::GetColorU32(i==1?orange:white),word);
      x+=f->CalcTextSizeA(size,FLT_MAX,0,word).x;
    }
    const std::string name=snapshot.me.empty()?Tr(tr_,"RIDER","PİLOT"):snapshot.me;
    Initial(ImVec2(398*s+shift,70*s),56*s,name,0,true);
    RowLabel(name,479*s+shift,76*s,26*s,white,layout_.width-24*s,Face(1));
    float metadata_x=479*s+shift;
    std::string metadata;
    if(!snapshot.country.empty()){
      if(Flag(ImVec2(metadata_x,109*s),snapshot.country,.52f,false))metadata_x+=32*s;
      metadata=snapshot.country;
    }
    const auto membership=MemberSince(snapshot.member_since,tr_);
    if(!membership.empty()){if(!metadata.empty())metadata+=" · ";metadata+=membership;}
    if(!metadata.empty())metadata+=" · ";
    metadata+=std::to_string(snapshot.friends.size())+Tr(tr_," friends"," arkadaş");
    RowLabel(metadata,metadata_x,108*s,18*s,ImVec4(.72f,.73f,.76f,1),layout_.width-24*s);
  }
  void Tabs() {
    const float s=layout_.scale,shift=-18*s*(1-alpha_);
    auto* draw=ImGui::GetWindowDrawList();const auto hints=visual::Hints(prompts::CurrentStyle());
    const char* names[]{Tr(tr_,"FRIENDS","ARKADAŞLAR"),Tr(tr_,"SERVER","SUNUCU"),
        Tr(tr_,"RECORDS","REKORLAR"),Tr(tr_,"ACHIEVEMENTS","BAŞARIMLAR")};
    const float x=96*s+shift,top=250*s,height=60*s;
    draw->AddLine(ImVec2(64*s+shift,268*s),ImVec2(64*s+shift,448*s),visual::Color(255,106,19,.18f*alpha_),2*s);
    for(int side=0;side<2;++side) {
      const char* label=side?hints.next:hints.previous;
      const ImVec2 a(x,(side?508.f:196.f)*s),b(x+34*s,a.y+28*s);
      draw->AddRect(a,b,visual::Color(213,216,220,.65f*alpha_),4*s,0,1.4f*s);
      const auto size=Face()->CalcTextSizeA(18*s,FLT_MAX,0,label);
      visual::Text(draw,Face(),18*s,ImVec2((a.x+b.x-size.x)*.5f,(a.y+b.y-size.y)*.5f),
          ImGui::GetColorU32(white),label,b.x);
    }
    for(int i=0;i<4;++i) {
      const ImVec2 pos(x,top+i*height);const bool active=i==tab_;
      RowLabel(names[i],pos.x,pos.y,28*s,active?white:ImVec4(.63f,.65f,.68f,1),318*s,Face(active?1:0));
      ImGui::SetCursorScreenPos(ImVec2(50*s+shift,pos.y-8*s));ImGui::PushID(i);
      if(ImGui::InvisibleButton("##nobk_tab",ImVec2(252*s,48*s))){tab_=i;selected_=0;}
      ImGui::PopID();
    }
    const float target=(268+60*tab_)*s;
    if(nav_marker_y_<0)nav_marker_y_=target;
    nav_marker_y_+=(target-nav_marker_y_)*std::min(1.f,ImGui::GetIO().DeltaTime*18.f);
    visual::Marker(draw,ImVec2(64*s+shift,nav_marker_y_),s,alpha_);
  }
  void Curve() {
    visual::ApexLines(ImGui::GetWindowDrawList(),layout_.scale,alpha_,ImGui::GetTime()-opened_at_);
  }
  void Draw(ImGuiIO& io,const Snapshot& snapshot) {
    layout_=visual::Measure(io.DisplaySize);const float s=layout_.scale;
    if(tab_!=0) {
      layout_.body_left=364*s;layout_.body_top=(tab_==1?208.f:168.f)*s;
      layout_.body_width=std::max(1.f,std::min((tab_==3?936.f:836.f)*s,io.DisplaySize.x-layout_.body_left-24*s));
      layout_.width=std::min(io.DisplaySize.x,layout_.body_left+layout_.body_width+32*s);
      layout_.body_height=std::max(40.f,io.DisplaySize.y-layout_.body_top-24*s);
    }
    const double now=ImGui::GetTime();alpha_=visual::Entrance(now,opened_at_);
    bool reset=animated_tab_!=tab_;
    if(reset){animated_tab_=tab_;tab_at_=now;}
    auto* background=ImGui::GetBackgroundDrawList();
    Backdrop(alpha_);
    visual::SoftScrim(background,layout_,io.DisplaySize,alpha_);
    Theme theme(font_);
    ImGui::PushFont(Face(),20*s);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(0,0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,0);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize,0);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,ImVec2(10*s,2*s));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,ImVec2(9*s,6*s));
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha,alpha_);
    ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize,8*s);
    ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarRounding,4*s);
    ImGui::PushStyleColor(ImGuiCol_ChildBg,ImVec4(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarBg,ImVec4(0,0,0,0));
    ImGui::SetNextWindowPos(ImVec2(0,0),ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(layout_.width,io.DisplaySize.y),ImGuiCond_Always);
    if(ImGui::Begin("##nobk_panel",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoSavedSettings|
        ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoBackground|ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse)) {
      Curve();Header(snapshot);Tabs();
      if(animated_tab_!=tab_){animated_tab_=tab_;tab_at_=now;reset=true;}
      ImGui::SetCursorScreenPos(ImVec2(layout_.body_left,layout_.body_top));
      ImGui::BeginChild("##nobk_body",ImVec2(layout_.body_width,layout_.body_height),ImGuiChildFlags_None);
      if(reset)ImGui::SetScrollY(0);
      if(tab_==3)Achievements();
      else if(!snapshot.available)ImGui::TextWrapped("%s",Tr(tr_,"The NOBK panel works in Online mode on the NOBK Online server.","NOBK paneli NOBK Online sunucusunda Online modda çalışır."));
      else if(!snapshot.loaded)ImGui::TextDisabled("%s",snapshot.failed?Tr(tr_,"Cannot reach nobkonline.com right now.","nobkonline.com'a şu an ulaşılamıyor."):Tr(tr_,"Loading…","Yükleniyor…"));
      else if(tab_==0)Friends(snapshot);
      else if(tab_==1)Server(snapshot);
      else if(tab_==2)Records(snapshot,reset);
      ImGui::EndChild();
      if(web_)OpenUrl(tab_==0?"https://nobkonline.com/hesabim/":tab_==2?"https://nobkonline.com/motogp/records/":"https://nobkonline.com/motogp/");
    }
    ImGui::End();ImGui::PopStyleColor(2);ImGui::PopStyleVar(8);ImGui::PopFont();
  }
  bool Row(int index,float height,float action_width=0,bool friend_row=false,bool content_row=false) {
    const auto pos=ImGui::GetCursorScreenPos();const float s=layout_.scale;
    const float width=ImGui::GetContentRegionAvail().x;
    const int visible=std::max(0,int((pos.y-ImGui::GetWindowPos().y)/std::max(1.f,height+2*s)));
    const float slide=visual::RowOffset(ImGui::GetTime(),opened_at_,tab_at_,visible,s);
    row_position_=ImVec2(pos.x+slide,pos.y);row_height_=height;
    ImGui::SetCursorScreenPos(row_position_);ImGui::PushID(index);
    const bool clicked=ImGui::InvisibleButton("##row",ImVec2(std::max(1.f,width-action_width),height));
    const bool hovered=ImGui::IsItemHovered();ImGui::PopID();
    const auto delta=ImGui::GetIO().MouseDelta;
    if(clicked||(hovered&&!reveal_selected_&&(delta.x!=0||delta.y!=0)))selected_=index;
    row_selected_=index==selected_;
    auto* draw=ImGui::GetWindowDrawList();
    if(content_row) {
      if(row_selected_) {
        visual::Marker(draw,ImVec2(row_position_.x+10*s,pos.y+height*.5f),s,alpha_);
        visual::Underline(draw,ImVec2(row_position_.x+36*s,pos.y+height-2*s),pos.x+width,s,alpha_);
      } else draw->AddLine(ImVec2(row_position_.x+36*s,pos.y+height-2*s),ImVec2(pos.x+width,pos.y+height-2*s),visual::Color(217,221,226,.18f*alpha_),s);
    } else visual::Strip(draw,row_position_,ImVec2(pos.x+width+slide,pos.y+height),0,row_selected_,alpha_,friend_row?20.f:1.f);
    if(row_selected_&&reveal_selected_)ImGui::SetScrollHereY(.5f);
    ImGui::SetCursorScreenPos(pos);
    return clicked||(row_selected_&&activate_);
  }
  ImVec4 RowText(bool secondary=false)const {
    return secondary?ImVec4(.65f,.67f,.7f,1):white;
  }
  rex::ui::ImmediateTexture* FriendPhoto(const std::string& name) {
    if(!immediate_)return nullptr;
    const auto photo=friend_photos_.Get(name);
    if(!photo){photo_textures_.erase(name);return nullptr;}
    auto& cached=photo_textures_[name];
    if(cached.photo!=photo&&photo_uploads_<1){
      ++photo_uploads_;
      auto texture=immediate_->CreateTexture(photo->width,photo->height,rex::ui::ImmediateTextureFilter::kLinear,false,photo->rgba.data());
      cached.photo=photo;if(texture)cached.texture=std::move(texture);
    }
    return cached.texture.get();
  }
  void Initial(ImVec2 pos,float size,const std::string& name,ImU32 status,bool profile=false,rex::ui::ImmediateTexture* photo=nullptr) {
    auto* draw=ImGui::GetWindowDrawList();const float s=layout_.scale;
    const ImVec2 center(pos.x+size*.5f,pos.y+size*.5f);
    draw->AddCircleFilled(center,size*.5f,profile?ImGui::GetColorU32(orange):visual::Color(33,35,37,.92f*alpha_));
    if(photo){
      const float u=photo->width>photo->height?(1.f-float(photo->height)/photo->width)*.5f:0;
      const float v=photo->height>photo->width?(1.f-float(photo->width)/photo->height)*.5f:0;
      draw->AddImageRounded(reinterpret_cast<ImTextureID>(photo),pos,ImVec2(pos.x+size,pos.y+size),
          ImVec2(u,v),ImVec2(1-u,1-v),ImGui::GetColorU32(ImVec4(1,1,1,1)),size*.5f);
    }
    draw->AddCircle(center,size*.5f,profile?visual::Color(255,106,19,.55f*alpha_):visual::Color(210,212,217,.25f*alpha_),0,1.5f*s);
    if(!photo){
      const auto letter=visual::Initial(name);ImFont* f=Face(1);
      const auto ts=f->CalcTextSizeA(size*.46f,FLT_MAX,0,letter.c_str());
      draw->AddText(f,size*.46f,ImVec2(center.x-ts.x*.5f,center.y-ts.y*.5f),ImGui::GetColorU32(white),letter.c_str());
    }
    if(!profile&&status!=0) {
      const ImVec2 dot(center.x+size*.39f,center.y+size*.34f);
      draw->AddCircleFilled(dot,9*s,visual::Color(8,9,10,.9f*alpha_));
      auto color=ImGui::ColorConvertU32ToFloat4(status);color.w*=alpha_;
      draw->AddCircleFilled(dot,6.5f*s,ImGui::ColorConvertFloat4ToU32(color));
    }
  }
  bool Flag(ImVec2 pos,const std::string& country,float factor=1.f,bool label=true) {
    if(!flag_texture_||country.size()!=2)return false;
    std::string code=country;for(char& c:code)if(c>='a'&&c<='z')c-=32;
    if(code=="UK")code="GB";
    for(size_t i=0;i<(sizeof(gamer_card::flags::codes)-1)/2;++i) {
      if(code!=std::string_view(gamer_card::flags::codes+i*2,2))continue;
      const float x=float(i%16)*48+4,y=float(i/16)*32+1,s=layout_.scale;
      ImGui::GetWindowDrawList()->AddImage(reinterpret_cast<ImTextureID>(flag_texture_.get()),pos,
          ImVec2(pos.x+46*s*factor,pos.y+34.5f*s*factor),ImVec2(x/768.f,y/544.f),ImVec2((x+40)/768.f,(y+30)/544.f),
          ImGui::GetColorU32(ImVec4(1,1,1,1)));
      if(label)RowLabel(code,pos.x+56*s,pos.y+7.25f*s,20*s,RowText(true),pos.x+124*s);
      return true;
    }
    return false;
  }
  void RowLabel(const std::string& text,float x,float y,float size,ImVec4 color,float right,ImFont* font=nullptr) {
    visual::Label(ImGui::GetWindowDrawList(),font?font:Face(),size,ImVec2(x,y),color,text,right,layout_.scale,alpha_);
  }
  enum class TicketIcon { None, Arrow, Check, Cross };
  bool ActionTicket(const char* id,ImVec2 a,float width,const char* label,bool quiet,bool disabled,TicketIcon icon) {
    const float s=layout_.scale;const ImVec2 b(a.x+width,a.y+36*s);
    ImGui::SetCursorScreenPos(a);ImGui::BeginDisabled(disabled);
    const bool clicked=ImGui::InvisibleButton(id,ImVec2(width,36*s));
    const bool hover=ImGui::IsItemHovered()||ImGui::IsItemFocused();ImGui::EndDisabled();
    auto* draw=ImGui::GetWindowDrawList();
    const std::array<ImVec2,5> points{{a,ImVec2(b.x,a.y),ImVec2(b.x,b.y-8*s),ImVec2(b.x-8*s,b.y),ImVec2(a.x,b.y)}};
    const auto edge=quiet?visual::Color(168,172,180,(hover?.85f:.55f)*alpha_):
        visual::Color(255,106,19,(hover?1.f:.8f)*alpha_);
    draw->AddConvexPolyFilled(points.data(),int(points.size()),visual::Color(16,19,23,.88f*alpha_));
    draw->AddPolyline(points.data(),int(points.size()),edge,ImDrawFlags_Closed,(hover?1.8f:1.2f)*s);
    const float label_right=b.x-(icon==TicketIcon::None?8.f:30.f)*s;
    float size=16*s;const float available=std::max(s,label_right-a.x-10*s);
    const float measured=visual::TextWidth(Face(1),size,label);
    if(measured>available)size*=available/measured;
    RowLabel(label,a.x+10*s,a.y+(36*s-size)*.5f,size,quiet?muted:white,label_right,Face(1));
    const float cy=(a.y+b.y)*.5f;
    if(icon==TicketIcon::Check) {
      draw->AddLine(ImVec2(b.x-24*s,cy),ImVec2(b.x-20*s,cy+4*s),edge,2*s);
      draw->AddLine(ImVec2(b.x-20*s,cy+4*s),ImVec2(b.x-12*s,cy-5*s),edge,2*s);
    } else if(icon==TicketIcon::Arrow)draw->AddTriangleFilled(ImVec2(b.x-23*s,cy-7*s),ImVec2(b.x-13*s,cy),ImVec2(b.x-23*s,cy+7*s),edge);
    else if(icon==TicketIcon::Cross) {
      draw->AddLine(ImVec2(b.x-23*s,cy-5*s),ImVec2(b.x-13*s,cy+5*s),edge,2*s);
      draw->AddLine(ImVec2(b.x-23*s,cy+5*s),ImVec2(b.x-13*s,cy-5*s),edge,2*s);
    }
    return clicked;
  }
  bool InviteTicket(ImVec2 a,float width,std::optional<online::social::SendState> feedback) {
    using SendState=online::social::SendState;
    const bool pending=feedback==SendState::Pending,sent=feedback==SendState::Sent;
    const char* label=pending?Tr(tr_,"SENDING…","GÖNDERİLİYOR…"):sent?Tr(tr_,"SENT","GÖNDERİLDİ"):
        feedback==SendState::Failed?Tr(tr_,"RETRY","TEKRAR DENE"):Tr(tr_,"INVITE","DAVET ET");
    return ActionTicket("##invite_ticket",a,width,label,sent||pending,pending,
        pending?TicketIcon::None:sent?TicketIcon::Check:TicketIcon::Arrow);
  }
  void Friends(const Snapshot& snapshot) {
    const float s=layout_.scale;const auto social=online::social::Read();
    const auto hints=visual::Hints(prompts::CurrentStyle());
    if(invite_picker_) {
      ImGui::TextWrapped("%s",(std::string(Tr(tr_,"Select a friend to invite to your lobby. ","Lobine davet etmek için bir arkadaş seç. "))+hints.select+Tr(tr_,": invite.",": davet et.")).c_str());
      if(!social.loaded)ImGui::TextColored(muted,"%s",Tr(tr_,"Loading friends/invites…","Arkadaşlar/davetler yükleniyor…"));
      else if(!social.supported)ImGui::TextColored(orange,"%s",Tr(tr_,"This server does not support community invitations yet.","Bu sunucu topluluk davetlerini henüz desteklemiyor."));
      else if(!social.can_invite)ImGui::TextWrapped("%s",Tr(tr_,"Host a lobby before sending invitations.","Davet göndermeden önce bir lobi kur."));
    }
    if(!social.error.empty()&&social.action!="send")ImGui::TextColored(orange,"%s",Tr(tr_,"Invitation service unavailable. Try again shortly.","Davet servisine ulaşılamıyor. Birazdan yeniden dene."));
    const int incoming=int(social.invites.size());
    selected_=std::clamp(selected_,0,std::max(0,incoming+int(snapshot.friends.size())-1));
    for(int i=0;i<incoming;++i) {
      const auto& invite=social.invites[i];const auto pos=ImGui::GetCursorScreenPos();
      const float right=pos.x+ImGui::GetContentRegionAvail().x;
      const float ticket_width=112*s,actions_width=2*ticket_width+12*s;
      // Clicking the row selects it; joining requires the explicit action or A/Enter.
      Row(i,116*s,actions_width+12*s,true);const auto a=row_position_;
      const float action_left=right-actions_width,text_left=a.x+142*s,text_right=action_left-14*s;
      const ImVec2 avatar(a.x+36*s,a.y+22*s);
      const bool visible=ImGui::IsRectVisible(avatar,ImVec2(avatar.x+62*s,avatar.y+62*s));
      Initial(avatar,62*s,invite.from,0,false,visible?FriendPhoto(invite.from):nullptr);
      RowLabel(Tr(tr_,"LOBBY INVITATION","LOBİ DAVETİ"),text_left,a.y+8*s,13*s,orange,text_right,Face(1));
      RowLabel(invite.from,text_left,a.y+30*s,28*s,RowText(),text_right,Face(1));
      RowLabel(Tr(tr_,"Invited you to their lobby","Seni lobisine davet etti"),text_left,a.y+68*s,16*s,RowText(true),text_right);
      const bool pending=social.answering.contains(invite.id);
      ImGui::PushID(invite.id.c_str());
      const bool accept=ActionTicket("##accept",ImVec2(action_left,a.y+32*s),ticket_width,
          Tr(tr_,"ACCEPT","KABUL ET"),false,pending,TicketIcon::Check)||(selected_==i&&activate_&&!pending);
      const float decline_left=action_left+ticket_width+12*s;
      const bool decline=ActionTicket("##decline",ImVec2(decline_left,a.y+32*s),ticket_width,
          Tr(tr_,"DECLINE","REDDET"),true,pending,TicketIcon::Cross)||(selected_==i&&decline_&&!pending);
      ImGui::PopID();
      if(prompts::CurrentStyle()!=prompts::Style::Keyboard) {
        for(const auto& [x,label]:{std::pair{action_left,hints.select},std::pair{decline_left,hints.decline}}) {
          const float size=14*s,width=visual::TextWidth(Face(),size,label);
          RowLabel(label,x+std::max(0.f,(ticket_width-width)*.5f),a.y+75*s,size,RowText(true),x+ticket_width);
        }
      }
      if(decline)online::social::Answer(invite.id,false);
      else if(accept&&online::social::Answer(invite.id,true))state_->Close();
      ImGui::SetCursorScreenPos(pos);ImGui::Dummy(ImVec2(0,row_height_));
    }
    if(snapshot.friends.empty()) {
      ImGui::TextWrapped("%s",Tr(tr_,"No friends yet. Add friends on nobkonline.com (your profile or My account); they show up here with what they are doing in MotoGP.",
          "Henüz arkadaşın yok. nobkonline.com'da (profil ya da Hesabım) arkadaş ekle; burada MotoGP'de ne yaptıklarıyla görünürler."));return;
    }
    for(int i=0;i<int(snapshot.friends.size());++i) {
      const auto& f=snapshot.friends[i];const auto pos=ImGui::GetCursorScreenPos();
      const float right=pos.x+ImGui::GetContentRegionAvail().x;
      const auto mapped=std::find_if(social.friends.begin(),social.friends.end(),[&](const auto& entry){return SameName(entry.name,f.name);});
      const bool can_invite=social.supported&&social.can_invite&&mapped!=social.friends.end()&&mapped->online;
      const float ticket_width=124*s;
      if(Row(incoming+i,100*s,can_invite?ticket_width+12*s:0,true)) {
        if(invite_picker_){if(can_invite)online::social::Send(mapped->peer);}
        else OpenUrl("https://nobkonline.com/profile/?u="+Encode(f.name));
      }
      const auto a=row_position_;const float avatar=62*s;
      const ImVec2 avatar_pos(a.x+36*s,a.y+(row_height_-avatar)*.5f);
      const bool visible=ImGui::IsRectVisible(avatar_pos,ImVec2(avatar_pos.x+avatar,avatar_pos.y+avatar));
      Initial(avatar_pos,avatar,f.name,WhereColor(f.where),false,visible?FriendPhoto(f.name):nullptr);
      const float nx=a.x+142*s,name_size=(row_selected_?34.f:32.f)*s;
      const float country_x=a.x+282*s,status_left=a.x+(can_invite?410.f:455.f)*s;
      const float status_right=right-(can_invite?ticket_width+12*s:4*s);
      RowLabel(f.name,nx,a.y+(row_height_-name_size)*.5f,name_size,RowText(),country_x-12*s,Face(row_selected_?2:0));
      Flag(ImVec2(country_x,a.y+(row_height_-34.5f*s)*.5f),f.country);
      const auto status=WhereText(f,tr_);
      float status_size=(can_invite?20.f:24.f)*s;
      const float status_width=visual::TextWidth(Face(),status_size,status);
      if(status_width>status_right-status_left)status_size*=std::max(s,status_right-status_left)/status_width;
      RowLabel(status,status_left,a.y+(row_height_-status_size)*.5f,status_size,RowText(),status_right);
      if(can_invite) {
        std::optional<online::social::SendState> feedback;
        if(const auto found=social.outgoing.find(mapped->peer);found!=social.outgoing.end())feedback=found->second;
        ImGui::PushID(i);
        if(InviteTicket(ImVec2(right-ticket_width,a.y+32*s),ticket_width,feedback))online::social::Send(mapped->peer);
        ImGui::PopID();
      }
      ImGui::SetCursorScreenPos(pos);ImGui::Dummy(ImVec2(0,row_height_));
    }
  }
  void RightLabel(const std::string& text,float right,float y,float size,ImVec4 color,ImFont* font=nullptr) {
    ImFont* face=font?font:Face();
    RowLabel(text,right-visual::TextWidth(face,size,text),y,size,color,right,face);
  }
  void SectionTitle(const char* title) {
    const float s=layout_.scale;const auto p=ImGui::GetCursorScreenPos();
    RowLabel(title,p.x,p.y,58*s,white,p.x+ImGui::GetContentRegionAvail().x,Face(2));
    ImGui::Dummy(ImVec2(0,76*s));
  }
  void Subheading(const char* title) {
    const float s=layout_.scale;const auto p=ImGui::GetCursorScreenPos();const float w=ImGui::GetContentRegionAvail().x;
    RowLabel(title,p.x,p.y,27*s,ImVec4(.69f,.71f,.75f,1),p.x+w,Face(2));
    const float text=visual::TextWidth(Face(2),27*s,title);
    if(text+22*s<w)ImGui::GetWindowDrawList()->AddLine(ImVec2(p.x+text+20*s,p.y+19*s),ImVec2(p.x+w,p.y+19*s),visual::Color(217,221,226,.25f*alpha_),s);
    ImGui::Dummy(ImVec2(0,44*s));
  }
  void Server(const Snapshot& snapshot) {
    const float s=layout_.scale;ImGui::Indent(layout_.gutter);
    const auto top=ImGui::GetCursorScreenPos();const float width=ImGui::GetContentRegionAvail().x,right=top.x+width;
    SectionTitle(Tr(tr_,"SERVER","SUNUCU"));
    auto* draw=ImGui::GetWindowDrawList();const auto status=snapshot.server_up?ImVec4(.42f,.88f,.08f,1):ImVec4(1,.36f,.28f,1);
    const float status_x=top.x+visual::TextWidth(Face(2),58*s,Tr(tr_,"SERVER","SUNUCU"))+48*s;
    draw->AddCircleFilled(ImVec2(status_x,top.y+30*s),7*s,ImGui::GetColorU32(status));
    RowLabel(snapshot.server_up?Tr(tr_,"Server online","Sunucu açık"):Tr(tr_,"Server not answering","Sunucu cevap vermiyor"),status_x+20*s,top.y+14*s,27*s,white,right);
    draw->AddLine(ImVec2(top.x,top.y+76*s),ImVec2(right,top.y+76*s),visual::Color(217,221,226,.25f*alpha_),s);
    const auto counters=ImGui::GetCursorScreenPos();const float column=width/3;
    const int values[]{snapshot.online,snapshot.open_rooms,snapshot.racing};
    const char* labels[]{Tr(tr_,"ONLINE","ÇEVRİMİÇİ"),Tr(tr_,"OPEN ROOMS","AÇIK ODA"),Tr(tr_,"RACING","YARIŞTA")};
    for(int i=0;i<3;++i) {
      const float x=counters.x+i*column+46*s;
      RowLabel(std::to_string(values[i]),x,counters.y+14*s,88*s,i==0?orange:white,counters.x+(i+1)*column-10*s,Face(2));
      RowLabel(labels[i],x,counters.y+118*s,30*s,ImVec4(.71f,.73f,.77f,1),counters.x+(i+1)*column-8*s);
      if(i)draw->AddLine(ImVec2(counters.x+i*column,counters.y+26*s),ImVec2(counters.x+i*column,counters.y+150*s),visual::Color(217,221,226,.2f*alpha_),s);
    }
    ImGui::Dummy(ImVec2(0,174*s));ImGui::Dummy(ImVec2(0,22*s));
    const auto totals=ImGui::GetCursorScreenPos();
    draw->AddLine(totals,ImVec2(right,totals.y),visual::Color(217,221,226,.25f*alpha_),s);
    const auto riders=std::to_string(snapshot.riders),records=std::to_string(snapshot.records);
    RowLabel(riders,totals.x,totals.y+17*s,43*s,white,right,Face(2));
    RowLabel(Tr(tr_,"Riders","Pilot"),totals.x+visual::TextWidth(Face(2),43*s,riders)+18*s,totals.y+25*s,29*s,ImVec4(.71f,.73f,.77f,1),totals.x+238*s);
    draw->AddLine(ImVec2(totals.x+242*s,totals.y+22*s),ImVec2(totals.x+242*s,totals.y+57*s),visual::Color(217,221,226,.25f*alpha_),s);
    RowLabel(records,totals.x+296*s,totals.y+17*s,43*s,white,right,Face(2));
    RowLabel(Tr(tr_,"Records","Rekor"),totals.x+296*s+visual::TextWidth(Face(2),43*s,records)+18*s,totals.y+25*s,29*s,ImVec4(.71f,.73f,.77f,1),right);
    ImGui::Dummy(ImVec2(0,82*s));ImGui::Dummy(ImVec2(0,28*s));
    Subheading(Tr(tr_,"OPEN LOBBIES","AÇIK LOBİLER"));ImGui::Unindent(layout_.gutter);
    if(snapshot.rooms.empty()) {
      ImGui::Indent(layout_.gutter);
      ImGui::TextWrapped("%s",Tr(tr_,"No room is open right now. Host one from Player Match; it shows up here and on nobkonline.com/motogp.",
          "Şu an açık oda yok. Player Match'ten bir oda kur; burada ve nobkonline.com/motogp'de görünür."));
      ImGui::Unindent(layout_.gutter);return;
    }
    selected_=std::clamp(selected_,0,int(snapshot.rooms.size())-1);
    for(int i=0;i<int(snapshot.rooms.size());++i) {
      const auto& room=snapshot.rooms[i];const auto pos=ImGui::GetCursorScreenPos();
      Row(i,116*s,0,false,true);const auto a=row_position_;
      const auto count=std::to_string(room.members)+" / "+std::to_string(room.capacity);
      const auto host=std::string(Tr(tr_,"Host: ","Kurucu: "))+room.host+(room.friends?Tr(tr_," · friends inside"," · arkadaşların var"):"");
      RowLabel(room.name,a.x+44*s,a.y+10*s,38*s,!row_selected_&&(room.mine||room.friends)?orange:white,right-210*s,Face(2));
      RowLabel(host,a.x+44*s,a.y+66*s,25*s,RowText(true),right-210*s);
      RightLabel(count,right-10*s,a.y+10*s,37*s,white,Face(2));
      const auto state=room.racing?Tr(tr_,"Racing","Yarışta"):Tr(tr_,"In lobby","Lobide");
      const auto color=room.racing?orange:ImVec4(.6f,.84f,.12f,1);
      const float state_width=visual::TextWidth(Face(),27*s,state);
      draw->AddCircleFilled(ImVec2(right-state_width-28*s,a.y+81*s),6*s,ImGui::GetColorU32(color));
      RightLabel(state,right-10*s,a.y+63*s,27*s,color);
      ImGui::SetCursorScreenPos(pos);ImGui::Dummy(ImVec2(0,row_height_));
    }
  }
  void Records(const Snapshot& snapshot,bool reset) {
    const float s=layout_.scale;ImGui::Indent(layout_.gutter);
    SectionTitle(Tr(tr_,"RECORDS","REKORLAR"));
    const auto summary=ImGui::GetCursorScreenPos();const float width=ImGui::GetContentRegionAvail().x;
    const int values[]{snapshot.my_records,snapshot.my_podiums,snapshot.my_boards};
    const char* labels[]{Tr(tr_,"Records held","Elindeki rekor"),Tr(tr_,"Podiums","Podyum"),Tr(tr_,"Boards","Tablo")};
    auto* draw=ImGui::GetWindowDrawList();
    for(int i=0;i<3;++i) {
      const float x=summary.x+i*width/3;
      const auto value=snapshot.my_records>=0&&values[i]>=0?std::to_string(values[i]):std::string("-");
      RowLabel(value,x,summary.y+4*s,50*s,orange,x+width/3,Face(2));
      RowLabel(labels[i],x+visual::TextWidth(Face(2),50*s,value)+16*s,summary.y+20*s,25*s,RowText(true),x+width/3-6*s);
      if(i)draw->AddLine(ImVec2(x-20*s,summary.y+7*s),ImVec2(x-20*s,summary.y+57*s),visual::Color(217,221,226,.2f*alpha_),s);
    }
    ImGui::Dummy(ImVec2(0,82*s));
    Subheading(Tr(tr_,"LATEST RECORDS","SON REKORLAR"));ImGui::Unindent(layout_.gutter);
    const float footer=snapshot.events.empty()?0:46*s+std::min(3,int(snapshot.events.size()))*42*s;
    ImGui::BeginChild("##record_list",ImVec2(0,std::max(100*s,ImGui::GetContentRegionAvail().y-footer-12*s)),ImGuiChildFlags_None);
    if(reset)ImGui::SetScrollY(0);
    selected_=std::clamp(selected_,0,std::max(0,int(snapshot.recent.size())-1));
    if(snapshot.recent.empty())ImGui::TextDisabled("%s",Tr(tr_,"Loading…","Yükleniyor…"));
    for(int i=0;i<int(snapshot.recent.size());++i) {
      const auto& record=snapshot.recent[i];const auto pos=ImGui::GetCursorScreenPos();const float right=pos.x+ImGui::GetContentRegionAvail().x;
      Row(i,96*s,0,false,true);const auto a=row_position_;
      // Record entries have no presence field: use an initial, without a status dot.
      Initial(ImVec2(a.x+72*s,a.y+9*s),62*s,record.name,0,false);
      const float name_x=a.x+162*s,rank_x=a.x+342*s;
      RowLabel(record.name,name_x,a.y+7*s,35*s,!row_selected_&&(record.mine||record.friend_)?orange:white,rank_x-12*s,Face(2));
      const auto rank=record.rank>0?"P"+std::to_string(record.rank):std::string("-");
      RowLabel(rank,rank_x,a.y+15*s,25*s,record.rank==1?orange:RowText(true),right-182*s,Face(1));
      if(record.rank==1)RowLabel(Tr(tr_,"took the record","rekoru aldı"),rank_x+46*s,a.y+19*s,19*s,orange,right-180*s);
      auto board=record.board;if(record.at>0)board+=" · "+Ago(record.at,tr_);
      RowLabel(board,name_x,a.y+54*s,23*s,RowText(true),right-10*s);
      RightLabel(record.time,right-10*s,a.y+13*s,38*s,record.rank==1?orange:white,Face(2));
      ImGui::SetCursorScreenPos(pos);ImGui::Dummy(ImVec2(0,row_height_));
    }
    ImGui::EndChild();
    if(!snapshot.events.empty()) {
      ImGui::Indent(layout_.gutter);Subheading(Tr(tr_,"THIS SESSION","BU OTURUM"));
      ImGui::BeginChild("##session_feed",ImVec2(0,std::max(40*s,footer-46*s)),ImGuiChildFlags_None);
      for(const auto& event:snapshot.events) {
        const auto pos=ImGui::GetCursorScreenPos();const float right=pos.x+ImGui::GetContentRegionAvail().x;
        const auto color=event.kind==2?ImVec4(1,.4f,.32f,1):orange;
        draw=ImGui::GetWindowDrawList();draw->AddCircleFilled(ImVec2(pos.x+8*s,pos.y+18*s),6*s,ImGui::GetColorU32(color));
        RowLabel(event.text,pos.x+34*s,pos.y+2*s,24*s,event.kind==1?orange:white,right-100*s);
        RightLabel(Ago(event.at,tr_),right-8*s,pos.y+7*s,19*s,RowText(true));
        ImGui::Dummy(ImVec2(0,40*s));
      }
      ImGui::EndChild();ImGui::Unindent(layout_.gutter);
    }
  }
  void Achievements() {
    if(!achievements_){ImGui::TextDisabled("%s",Tr(tr_,"Achievements are not available.","Başarımlar kullanılamıyor."));return;}
    const visual::AchievementAppearance appearance{layout_.scale,alpha_,ImGui::GetTime(),opened_at_,tab_at_,Face(),Face(1),Face(2)};
    ImGui::Indent(layout_.gutter);
    achievements::DrawContents(*achievements_,achievement_state_,*achievement_preferences_,&achievement_icons_,0,&appearance);
    ImGui::Unindent(layout_.gutter);
  }

  rex::ui::Window* window_;
  rex::system::AchievementManager* achievements_;
  std::shared_ptr<State> state_;
  ImFont* font_;ImFont* heading_;
  bool tr_;
  std::shared_ptr<achievements::Preferences> achievement_preferences_;
  achievements::MenuState achievement_state_;
  rex::ui::AchievementIconCache achievement_icons_;
  std::unique_ptr<rex::ui::ImmediateTexture> flag_texture_;
  rex::ui::ImmediateDrawer* immediate_=nullptr;
  FriendPhotos friend_photos_;
  struct PhotoTexture {std::shared_ptr<const ProfilePhoto> photo;std::unique_ptr<rex::ui::ImmediateTexture> texture;};
  std::map<std::string,PhotoTexture> photo_textures_;
  std::string toast_photo_;
  bool friend_toast_visible_=false;
  unsigned photo_uploads_=0;
  uint64_t photo_serial_=UINT64_MAX;
  uint64_t discord_notice_=0;
  bool nav_owned_=false;
  ImGuiConfigFlags old_flags_=0;
  ImGuiBackendFlags old_backend_=0;
  uint16_t nav_previous_=0;
  bool armed_=false,activate_=false,web_=false,reveal_selected_=false,decline_=false,invite_picker_=false;
  std::set<std::string> invite_notified_;
  uint16_t previous_=0;
  uint64_t epoch_=0;
  visual::Layout layout_{};
  double opened_at_=0,tab_at_=0;
  int animated_tab_=-1;
  float alpha_=1,row_height_=0;
  float nav_marker_y_=-1;
  ImVec2 row_position_{};
  bool row_selected_=false;
  int tab_=0,selected_=0;
  std::optional<Event> toast_;
  double toast_at_=0;
  bool startup_started_=false,startup_finished_=false;
  double startup_elapsed_=0;
  float startup_phase_=0;
};
}  // namespace

#ifdef MOTOGP_NOBK_VISUAL_PROBE
void ApplyVisualPresence(const char* live,const char* friends,bool got_live,bool got_friends){
  if(auto state=current.load(std::memory_order_acquire))state->ApplyVisualPresence(live,friends,got_live,got_friends);
}
void SetVisualInput(rex::input::InputSystem* input) {
  input_system.store(input,std::memory_order_release);
}
void SetVisualFixture(bool enabled,bool reference) {
  if(!enabled){visual_fixture.reset();return;}
  Snapshot s;s.available=s.loaded=s.server_up=true;s.me="Rider One";
  s.online=18;s.open_rooms=3;s.racing=1;s.riders=42;s.records=286;
  s.my_records=9;s.my_podiums=14;s.my_boards=20;
  for(int i=0;i<24;++i) {
    const std::string name=i==0?"Deniz":i==1?"Çağrı":i==2?"Alex":i==3?"A very long rider name to verify text clipping":("Rider "+std::to_string(i+1));
    s.friends.push_back({name,i%2?"TR":"GB",i%4==1?"Evening race":"",i%4==0?Where::Menus:i%4==1?Where::Room:i%4==2?Where::Racing:Where::Offline});
  }
  if(reference) {
    s.me="inflames";s.friends={
        {"Grea","CN","",Where::Offline},{"Lightbay","AU","",Where::Menus},
        {"Sadooo","TR","",Where::Room},{"To96","TR","",Where::Racing},
        {"erutoru","VN","",Where::Offline},{"oneslov","TR","",Where::Offline},
        {"wasmir","FR","",Where::Offline}};
  }
  s.rooms={{"Evening race","Deniz",3,20,false,true,false},{"Moto2 Championship","Alex",8,20,true,false,false},{"My room","Rider One",1,20,false,false,true}};
  const auto now=int64_t(std::time(nullptr));
  s.events={{"A friend entered MotoGP",now-60,1},{"A new room opened",now-120,0}};
  for(int i=0;i<16;++i)s.recent.push_back({i%2?"Deniz":"Alex",std::string(kTracks[i%17])+" · Moto2","1:43.08",i%3+1,now-i*120,false,i%2==0});
  s.recent[0]={"MgOnur","Donington Park · 125cc","1:38.30",4,now-6*3600,false,false};
  s.recent[1]={"Sadooo","Le Mans · 250cc","1:22.03",1,now-18*3600,false,true};
  s.recent[2]={"Sadooo","Le Mans · 125cc","1:28.07",1,now-18*3600,false,true};
  visual_fixture=std::move(s);
}
#endif

void ConfigureFonts(ImFontAtlas* atlas,const char* regular,const char* semibold,const char* bold) {
  visual_fonts={};visual_fonts.atlas=atlas;
  static const ImWchar ranges[]={0x20,0x1ff,0};
  const char* paths[]{regular,semibold,bold};
  for(size_t i=0;i<3;++i)if(paths[i]&&std::filesystem::is_regular_file(paths[i])) {
    ImFontConfig config;config.OversampleH=2;config.OversampleV=2;
    visual_fonts.faces[i]=atlas->AddFontFromFileTTF(paths[i],40,&config,ranges);
    if(visual_fonts.faces[i])visual_fonts.faces[i]->Flags|=ImFontFlags_LockBakedSizes;
  }
#ifdef _WIN32
  // Match the released FIFA 09 strip's own font chain without redistributing
  // Windows fonts. Other overlays keep their existing bundled typography.
  for(const char* path:{"C:\\Windows\\Fonts\\segoeuisb.ttf","C:\\Windows\\Fonts\\segoeui.ttf","C:\\Windows\\Fonts\\arial.ttf"}){
    std::error_code error;if(!std::filesystem::is_regular_file(path,error))continue;
    ImFontConfig config;config.OversampleH=2;config.OversampleV=2;
    visual_fonts.startup=atlas->AddFontFromFileTTF(path,48,&config,ranges);
    if(visual_fonts.startup){visual_fonts.startup->Flags|=ImFontFlags_LockBakedSizes;break;}
  }
#endif
}
void Initialize(rex::Runtime* runtime,std::shared_ptr<settings::InputGate> gate) {
  input_system.store(runtime?static_cast<rex::input::InputSystem*>(runtime->input_system()):nullptr,std::memory_order_release);
  current.store(std::make_shared<State>(std::move(gate)),std::memory_order_release);
}
void Shutdown() {
  input_system.store(nullptr,std::memory_order_release);
  if(auto state=current.exchange({},std::memory_order_acq_rel))state->Stop();
}
bool Active() {
  const auto state=current.load(std::memory_order_acquire);
  return state&&state->gate->Blocking();
}
void PollPad() {
  if(auto state=current.load(std::memory_order_acquire))state->PollPad(input_system.load(std::memory_order_acquire));
}
std::unique_ptr<rex::ui::ImGuiDialog> CreateMenu(rex::ui::ImGuiDrawer* drawer,rex::ui::Window* window,
    rex::system::AchievementManager* achievements,ImFont* font,ImFont* heading,bool turkish,
    rex::ui::ImmediateDrawer* immediate,rex::Runtime* runtime,std::shared_ptr<achievements::Preferences> preferences) {
  auto state=current.load(std::memory_order_acquire);
  return state?std::make_unique<Dialog>(drawer,window,achievements,std::move(state),font,heading,turkish,immediate,runtime,std::move(preferences)):nullptr;
}
}  // namespace motogp::nobk
