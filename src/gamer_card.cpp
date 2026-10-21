#include "gamer_card.h"
#include "in_game_settings.h"
#include "quit_menu.h"
#include "overlay_theme.h"
#include "gamer_card_flags.h"
#include "../third_party/nlohmann/json.hpp"

#include <rex/cvar.h>
#include <rex/hook.h>
#include <rex/memory.h>
#include <rex/runtime.h>
#include <rex/input/input_system.h>
#include <rex/ui/immediate_drawer.h>
#include <rex/ui/image_decode.h>
#include <rex/ui/window.h>
#include <imgui.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

REX_EXTERN(__imp__sub_82303260);
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#include <winhttp.h>
#include <wincodec.h>
#endif

namespace motogp::gamer_card {
namespace {
constexpr uint64_t kPeerBase = 0x0009000000000000ull;
constexpr uint64_t kPeerMask = 0xFFFFFFFFFF000000ull;
const auto& kAccent=overlay::orange;
const auto& kWhite=overlay::white;
const auto& kMuted=overlay::muted;
const char* Tr(bool tr, const char* en, const char* translated) {
  return tr ? translated : en;
}

std::string Country(std::string value) {
  if (value.size() != 2) return {};
  for (char& ch : value) {
    if (ch >= 'a' && ch <= 'z') ch -= 32;
    if (ch < 'A' || ch > 'Z') return {};
  }
  return value;
}

std::string EncodeName(std::string_view name) {
  constexpr char hex[] = "0123456789ABCDEF";
  std::string url;
  for (unsigned char ch : name) {
    if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
        (ch >= '0' && ch <= '9') || ch == '-' || ch == '_' || ch == '.' || ch == '~') {
      url += static_cast<char>(ch);
    } else {
      url += '%'; url += hex[ch >> 4]; url += hex[ch & 15];
    }
  }
  return url;
}

bool OpenProfile(std::string_view name) {
#ifdef _WIN32
  const auto url="https://nobkonline.com/profile/?u="+EncodeName(name);
  const std::wstring wide(url.begin(),url.end());
  return reinterpret_cast<intptr_t>(ShellExecuteW(nullptr,L"open",wide.c_str(),
      nullptr,nullptr,SW_SHOWNORMAL))>32;
#else
  (void)name;
  return false;
#endif
}

struct Player {
  std::string name, country;
  // Absent: older API, use avatar-by-name. Empty: explicitly no photo.
  std::optional<std::string> avatar_path;
  int64_t member_since=0;
};
struct Avatar { uint32_t width=0, height=0; std::vector<uint8_t> rgba; };
struct PublicReply { uint32_t status=0; std::string body; };
struct Record {
  uint32_t view=0, rank=0, total=0;
  int track=-1, season=-1;
  int64_t mode=-1, updated=0;
  std::string track_name, category, bike, result;
};
struct Results {
  std::vector<Record> rows;
  unsigned tracks=0, firsts=0;
};

std::string Text(const nlohmann::json& object,const char* key,size_t limit=96) {
  const auto value=object.find(key);
  if(value==object.end()||!value->is_string())return {};
  auto text=value->get<std::string>();
  if(text.size()>limit||std::any_of(text.begin(),text.end(),[](unsigned char c){return c<32||c==127;}))return {};
  return text;
}
int64_t Number(const nlohmann::json& object,const char* key,int64_t fallback=-1) {
  const auto value=object.find(key);
  if(value==object.end()||!value->is_number_integer())return fallback;
  if(value->is_number_unsigned()&&value->get<uint64_t>()>uint64_t(INT64_MAX))return fallback;
  return value->get<int64_t>();
}
constexpr std::string_view kTracks[]{"Losail International Circuit","Twin Ring Motegi","Jerez",
  "Le Mans","Mugello","Catalunya","TT Circuit Assen","Laguna Seca","Sachsenring",
  "Donington Park","Brno","Indianapolis","Misano","Estoril","Phillip Island",
  "Sepang","Valencia","","Silverstone","Shanghai","Istanbul Park"};

std::shared_ptr<const Results> ReadResults(const nlohmann::json& json,uint64_t serial) {
  const auto records=json.find("records");
  if(records==json.end()||!records->is_array()||records->size()>378) {
    REXLOG_WARN("PLAYER_CARD request={} stage=records event=unavailable reason=invalid_records_array",serial);
    return {};
  }
  auto results=std::make_shared<Results>();
  std::set<int> tracks;
  std::set<uint32_t> views;
  for(const auto& item:*records) {
    if(!item.is_object())continue;
    const auto view=Number(item,"view"),rank=Number(item,"rank"),total=Number(item,"total",0);
    if(view<1||view>378||rank<1||rank>UINT32_MAX||total<0||total>UINT32_MAX||
        !views.insert(static_cast<uint32_t>(view)).second)continue;
    Record row;
    row.view=static_cast<uint32_t>(view);row.rank=static_cast<uint32_t>(rank);row.total=static_cast<uint32_t>(total);
    row.track=int((view-1)%21);
    row.track_name=Text(item,"track");
    // Native track indices include an unused slot 17; never compact DLC slots.
    if(!kTracks[row.track].empty())row.track_name=kTracks[row.track];
    if(row.track_name.empty())row.track_name="Board #"+std::to_string(view);
    row.category=Text(item,"class",40);
    row.bike=Text(item,"bike",64);
    if(row.bike.empty())row.bike=Text(item,"manufacturer",64);
    row.updated=Number(item,"updated",0);
    if(const auto columns=item.find("columns");columns!=item.end()&&columns->is_object()) {
      const auto season=Number(*columns,"13");
      if(season==0||season==1)row.season=int(season);
      row.mode=Number(*columns,"6");
    }
    if(const auto display=item.find("display");display!=item.end()&&display->is_object())
      row.result=Text(*display,"text",32);
    if(row.result.empty())row.result="—";
    if(row.rank==1)++results->firsts;
    if(!kTracks[row.track].empty())tracks.insert(row.track);
    results->rows.push_back(std::move(row));
  }
  results->tracks=static_cast<unsigned>(tracks.size());
  std::stable_sort(results->rows.begin(),results->rows.end(),[](const auto& a,const auto& b){
    return a.rank!=b.rank?a.rank<b.rank:a.updated>b.updated;
  });
  REXLOG_INFO("PLAYER_CARD request={} stage=records event=ready rows={} tracks={} firsts={}",
      serial,results->rows.size(),results->tracks,results->firsts);
  return results;
}

PublicReply PublicGet(std::string_view relative_path, size_t max_body,
    uint64_t serial, std::string_view stage) {
#ifdef _WIN32
  // This is the site's public read-only API, not the authenticated launcher
  // endpoint. No password, launcher token or community credential is sent.
  struct Handle {
    HINTERNET value;
    ~Handle() { if (value) WinHttpCloseHandle(value); }
  };
  const auto started=std::chrono::steady_clock::now();
  const auto elapsed=[&] { return std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now()-started).count(); };
  const auto failure=[&](const char* operation) {
    const auto error=GetLastError();
    REXLOG_WARN("PLAYER_CARD request={} stage={} event=http_failed operation={} win_error={} elapsed_ms={}",
        serial,stage,operation,error,elapsed());
    return std::runtime_error(std::string("Public profile request failed at ")+operation);
  };
  REXLOG_INFO("PLAYER_CARD request={} stage={} event=http_start host=www.nobkonline.com path={}",
      serial,stage,relative_path);
  Handle session{WinHttpOpen(L"MotoGP0910-PlayerCard/1",
      WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME,
      WINHTTP_NO_PROXY_BYPASS, 0)};
  if (!session.value) session.value = WinHttpOpen(L"MotoGP0910-PlayerCard/1",
      WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME,
      WINHTTP_NO_PROXY_BYPASS, 0);
  if (!session.value || !WinHttpSetTimeouts(session.value, 3000, 4000, 4000, 4000))
    throw failure("session");
  Handle connection{WinHttpConnect(session.value, L"www.nobkonline.com",
      INTERNET_DEFAULT_HTTPS_PORT, 0)};
  // Paths are built locally or validated against the site's avatar prefix.
  // Server-returned hosts and redirects are never followed.
  const std::wstring path(relative_path.begin(), relative_path.end());
  Handle request{connection.value ? WinHttpOpenRequest(connection.value,
      L"GET", path.c_str(), nullptr, WINHTTP_NO_REFERER,
      WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE) : nullptr};
  if (!request.value) throw failure("open");
  DWORD redirects = WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
  if (!WinHttpSetOption(request.value, WINHTTP_OPTION_REDIRECT_POLICY,
      &redirects, sizeof(redirects)) ||
      !WinHttpSendRequest(request.value, WINHTTP_NO_ADDITIONAL_HEADERS,
          0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
      !WinHttpReceiveResponse(request.value, nullptr))
    throw failure("send_receive");
  DWORD status = 0, size = sizeof(status);
  if (!WinHttpQueryHeaders(request.value,
      WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
      WINHTTP_HEADER_NAME_BY_INDEX, &status, &size, WINHTTP_NO_HEADER_INDEX)) throw failure("status");
  wchar_t type[128]{}; DWORD type_size=sizeof(type);
  std::string content_type="unknown";
  if (WinHttpQueryHeaders(request.value,WINHTTP_QUERY_CONTENT_TYPE,
      WINHTTP_HEADER_NAME_BY_INDEX,type,&type_size,WINHTTP_NO_HEADER_INDEX)) {
    content_type.clear();
    for (const auto ch:type) {
      if (!ch) break;
      content_type+=ch>=0x20&&ch<0x7F?static_cast<char>(ch):'?';
    }
  }
  if (status!=200) {
    REXLOG_INFO("PLAYER_CARD request={} stage={} event=http_result status={} content_type={} read_bytes=0 elapsed_ms={}",
        serial,stage,status,content_type,elapsed());
    return {status,{}};
  }

  std::string body;
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
  for (;;) {
    const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
        deadline - std::chrono::steady_clock::now()).count();
    if (remaining <= 0) {
      REXLOG_WARN("PLAYER_CARD request={} stage={} event=http_failed reason=body_deadline elapsed_ms={}",serial,stage,elapsed());
      throw std::runtime_error("Public profile response timed out");
    }
    DWORD timeout = static_cast<DWORD>(std::min<int64_t>(remaining, 4000));
    if (!WinHttpSetOption(request.value, WINHTTP_OPTION_RECEIVE_TIMEOUT,
        &timeout, sizeof(timeout)))
      throw failure("receive_timeout");
    char chunk[4096]; DWORD received = 0;
    if (!WinHttpReadData(request.value, chunk, sizeof(chunk), &received))
      throw failure("read");
    if (!received) break;
    if (body.size() + received > max_body) {
      REXLOG_WARN("PLAYER_CARD request={} stage={} event=http_failed reason=body_limit limit={}",serial,stage,max_body);
      throw std::runtime_error("Player profile response is too large");
    }
    body.append(chunk, received);
  }
  REXLOG_INFO("PLAYER_CARD request={} stage={} event=http_result status={} content_type={} read_bytes={} elapsed_ms={}",
      serial,stage,status,content_type,body.size(),elapsed());
  return {status,std::move(body)};
#else
  (void)relative_path; (void)max_body; (void)serial; (void)stage;
  throw std::runtime_error("Player cards currently target Windows");
#endif
}

Player ReadPlayer(uint32_t id,uint64_t serial,std::shared_ptr<const Results>& results) {
  const auto response=PublicGet("/api/motogp/player?id="+std::to_string(id),256*1024,serial,"identity");
  if(response.status!=200) throw std::runtime_error("Player identity endpoint did not return HTTP 200");
  const auto json = nlohmann::json::parse(response.body,nullptr,false);
  if(json.is_discarded())throw std::runtime_error("Player identity response is not valid JSON");
  REXLOG_INFO("PLAYER_CARD request={} stage=identity event=json_fields id={} name={} country={} avatar={}",
      serial,json.contains("id"),json.contains("name"),json.contains("country"),json.contains("avatar"));
  // Numeric IDs belong to this game's service, not to the site's account ID.
  // Resolve the name from the matching record instead of guessing from XUID.
  if (!json.is_object() || json.value("missing", false) ||
      !json.contains("id") || !json["id"].is_number_unsigned() ||
      json["id"].get<uint64_t>() != id ||
      !json.contains("name") || !json["name"].is_string())
    throw std::runtime_error("Player profile identity did not match");
  Player player{json["name"].get<std::string>(), {}, {}};
  if (player.name.empty() || player.name.size() > 64 ||
      std::any_of(player.name.begin(), player.name.end(),
          [](unsigned char ch) { return ch < 0x20 || ch == 0x7F; }))
    throw std::runtime_error("Player profile name is invalid");
  if (const auto country = json.find("country"); country != json.end() && country->is_string())
    player.country = Country(country->get<std::string>());
  REXLOG_INFO("PLAYER_CARD request={} stage=identity event=ready player_id={} name={} country={}",
      serial,id,player.name,player.country.empty()?"absent":player.country);
  // Records arrive with the identity; switching tabs needs no new HTTP request.
  try { results=ReadResults(json,serial); }
  catch(const std::exception&) { REXLOG_WARN("PLAYER_CARD request={} stage=records event=unavailable reason=invalid_record",serial); }
  return player;
}

bool SameName(std::string_view first,std::string_view second) {
  if(first.size()!=second.size())return false;
  const auto fold=[](unsigned char ch){return ch>='A'&&ch<='Z'?ch+32:ch;};
  for(size_t i=0;i<first.size();++i)if(fold(first[i])!=fold(second[i]))return false;
  return true;
}

std::optional<std::string> AvatarPath(std::string value) {
  for (std::string_view origin:{"https://www.nobkonline.com","https://nobkonline.com"})
    if(value.starts_with(origin)) { value.erase(0,origin.size()); break; }
  if(!value.starts_with("/api/avatar/")||value.size()>200||value.find("..")!=std::string::npos||
      value.find('#')!=std::string::npos||value.find('\\')!=std::string::npos||
      std::any_of(value.begin(),value.end(),[](unsigned char ch){return ch<=0x20||ch>=0x7F;}))return {};
  return value;
}

Player ReadAccount(Player player,uint64_t serial) {
  // The general /profile/?u= page itself uses this shared public endpoint,
  // even for accounts whose only game is MotoGP. It is not a FIFA stats lookup
  // by MotoGP numeric ID: the already resolved account name is the query key.
  const auto response=PublicGet("/api/fifa09/player?name="+EncodeName(player.name),256*1024,serial,"account");
  if(response.status!=200)throw std::runtime_error("Shared account endpoint did not return HTTP 200");
  const auto json=nlohmann::json::parse(response.body,nullptr,false);
  if(json.is_discarded())throw std::runtime_error("Shared account response is not valid JSON");
  REXLOG_INFO("PLAYER_CARD request={} stage=account event=json_fields player={} country={} avatar={}",
      serial,json.contains("player"),json.contains("country"),json.contains("avatar"));
  if(!json.is_object()||!json.contains("player")||!json["player"].is_string()||
      !SameName(json["player"].get<std::string>(),player.name))
    throw std::runtime_error("Shared account name did not match the selected player");
  if(const auto country=json.find("country");country!=json.end()&&country->is_string()) {
    const auto parsed=Country(country->get<std::string>());
    if(!parsed.empty())player.country=parsed;
  }
  const auto joined=Number(json,"member_since",0);
  if(joined>=946684800&&joined<4102444800LL)player.member_since=joined;
  if(const auto avatar=json.find("avatar");avatar!=json.end()&&avatar->is_string()) {
    const auto value=avatar->get<std::string>();
    if(value.empty())player.avatar_path=std::string{};
    else if(auto path=AvatarPath(value))player.avatar_path=std::move(*path);
    else REXLOG_WARN("PLAYER_CARD request={} stage=account event=avatar_path_rejected",serial);
  }
  REXLOG_INFO("PLAYER_CARD request={} stage=account event=ready country={} avatar={}",serial,
      player.country.empty()?"absent":player.country,
      !player.avatar_path?"by_name":player.avatar_path->empty()?"none":"profile_path");
  return player;
}

#ifdef _WIN32
template<class T> struct ComObject {
  T* value=nullptr;
  ~ComObject(){if(value)value->Release();}
  T** Address(){return &value;}
  T* operator->()const{return value;}
};
#endif

std::shared_ptr<const Avatar> ReadAvatar(const Player& player,uint64_t serial) {
  if(player.avatar_path&&player.avatar_path->empty()) {
    REXLOG_INFO("PLAYER_CARD request={} stage=avatar event=fallback reason=account_has_no_photo",serial);
    return {};
  }
  const auto path=player.avatar_path.value_or("/api/avatar/u/"+EncodeName(player.name));
  auto response=PublicGet(path,512*1024,serial,"avatar");
  if(response.status!=200) {
    REXLOG_INFO("PLAYER_CARD request={} stage=avatar event=fallback reason=http_status status={}",serial,response.status);
    return {};
  }
#ifdef _WIN32
  // NOBK serves JPEG photos as well as PNG. The SDK helper supports PNG only;
  // use Windows' image codecs without changing the pinned runtime binaries.
  const auto initialized=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
  if(FAILED(initialized)) {
    REXLOG_WARN("PLAYER_CARD request={} stage=avatar event=fallback reason=com_init hr={:08X}",serial,uint32_t(initialized));
    return {};
  }
  struct Apartment {~Apartment(){CoUninitialize();}} apartment;
  const auto failed=[&](HRESULT hr,const char* operation) {
    if(SUCCEEDED(hr))return false;
    REXLOG_WARN("PLAYER_CARD request={} stage=avatar event=fallback reason=decode_failed operation={} hr={:08X}",serial,operation,uint32_t(hr));
    return true;
  };
  ComObject<IWICImagingFactory> factory;
  if(failed(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,
      IID_PPV_ARGS(factory.Address())),"factory"))return {};
  ComObject<IWICStream> stream;
  if(failed(factory->CreateStream(stream.Address()),"stream")||
      failed(stream->InitializeFromMemory(reinterpret_cast<BYTE*>(response.body.data()),
          static_cast<DWORD>(response.body.size())),"memory"))return {};
  ComObject<IWICBitmapDecoder> decoder;
  if(failed(factory->CreateDecoderFromStream(stream.value,nullptr,WICDecodeMetadataCacheOnDemand,
      decoder.Address()),"decoder"))return {};
  GUID format{};
  if(failed(decoder->GetContainerFormat(&format),"format"))return {};
  const bool jpeg=IsEqualGUID(format,GUID_ContainerFormatJpeg),png=IsEqualGUID(format,GUID_ContainerFormatPng);
  if(!jpeg&&!png) {
    REXLOG_WARN("PLAYER_CARD request={} stage=avatar event=fallback reason=unsupported_format",serial);
    return {};
  }
  ComObject<IWICBitmapFrameDecode> frame;
  if(failed(decoder->GetFrame(0,frame.Address()),"frame"))return {};
  UINT width=0,height=0;
  if(failed(frame->GetSize(&width,&height),"size"))return {};
  // Validate dimensions before allocating or decoding pixel data.
  if(!width||!height||width>1024||height>1024) {
    REXLOG_WARN("PLAYER_CARD request={} stage=avatar event=fallback reason=dimensions width={} height={}",serial,width,height);
    return {};
  }
  ComObject<IWICFormatConverter> converter;
  if(failed(factory->CreateFormatConverter(converter.Address()),"converter")||
      failed(converter->Initialize(frame.value,GUID_WICPixelFormat32bppRGBA,WICBitmapDitherTypeNone,
          nullptr,0.,WICBitmapPaletteTypeCustom),"convert"))return {};
  std::vector<uint8_t> pixels(size_t(width)*height*4);
  if(failed(converter->CopyPixels(nullptr,width*4,static_cast<UINT>(pixels.size()),pixels.data()),"pixels"))return {};
  REXLOG_INFO("PLAYER_CARD request={} stage=avatar event=decoded format={} width={} height={}",serial,jpeg?"JPEG":"PNG",width,height);
  return std::make_shared<Avatar>(Avatar{width,height,std::move(pixels)});
#else
  return {};
#endif
}

enum class Status { Loading, Ready, Unavailable };
struct Snapshot {
  uint64_t serial = 0;
  Status status = Status::Loading;
  Player player;
  std::shared_ptr<const Avatar> avatar;
  std::shared_ptr<const Results> results;
  std::shared_ptr<const Avatar> flags;
  uint32_t preferred_view=0;
};

std::shared_ptr<const Avatar> ReadFlags() {
  int width=0,height=0;
  auto pixels=rex::ui::DecodeImageRGBA(flags::png,sizeof(flags::png),width,height);
  if(width!=768||height!=544||pixels.size()!=size_t(width)*height*4) {
    REXLOG_WARN("PLAYER_CARD stage=flags event=decode_failed");return {};
  }
  return std::make_shared<Avatar>(Avatar{uint32_t(width),uint32_t(height),std::move(pixels)});
}

class State {
 public:
  State(rex::Runtime* runtime, std::shared_ptr<settings::InputGate> input_gate)
      : gate(std::move(input_gate)), kernel_(runtime->kernel_state()) {
    // Independent servers may reuse the same numeric IDs. Never resolve one
    // of their players against the production NOBK account directory.
    const auto origin = rex::cvar::Query<std::string>("online_server");
    available_ = rex::cvar::Query<std::string>("network_mode") == "online" &&
        (origin == "https://motogp.nobkonline.com" || origin == "https://motogp.nobkonline.com/");
    REXLOG_INFO("PLAYER_CARD event=initialize enabled={} photo_decoder=WIC_JPEG_PNG",available_);
  }
  ~State() { Stop(); }

  uint32_t Show(uint32_t user, uint64_t xuid,uint32_t preferred_view) {
    if (user >= 4 || (xuid & kPeerMask) != kPeerBase || !(xuid & 0xFFFFFF)) {
      REXLOG_WARN("PLAYER_CARD event=open_rejected reason=identity user={} xuid={:016X}",user,xuid);
      return 87;
    }
    if (!available_) { REXLOG_INFO("PLAYER_CARD event=open_rejected reason=service_not_supported"); return 1168; }
    if (quit::Pending() || settings::GuestInputBlocked()) {
      REXLOG_INFO("PLAYER_CARD event=open_rejected reason=other_menu_or_held_input"); return 170;
    }
    std::lock_guard lock(mutex_);
    if (stopping_) return 1168;
    if (gate->Blocking()) return 170;
    // Only copy the identity. Lookup is on demand on a worker; neither the
    // UI thread nor the guest's lobbies/leaderboards wait for HTTP.
    if (!worker_.joinable()) worker_ = std::thread([this] { Run(); });
    gate->Open();
    if (!gate->IsOpen()) return 170;
    snapshot_ = {snapshot_.serial + 1, Status::Loading, {}, {}};
    snapshot_.preferred_view=preferred_view;
    requested_id_ = static_cast<uint32_t>(xuid & 0xFFFFFF);
    request_serial_ = snapshot_.serial;
    REXLOG_INFO("PLAYER_CARD request={} event=open user={} xuid={:016X} player_id={} preferred_view={}",
        request_serial_,user,xuid,*requested_id_,preferred_view);
    kernel_->BroadcastNotification(0x9, true); // XN_SYS_UI
    notified_ = true;
    changed_.notify_one();
    return 0;
  }
  Snapshot Read() {
    std::lock_guard lock(mutex_);
    return snapshot_;
  }
  void Dismiss() {
    std::lock_guard lock(mutex_);
    if(gate->IsOpen())REXLOG_INFO("PLAYER_CARD request={} event=close",snapshot_.serial);
    gate->Close();
    if (notified_) { kernel_->BroadcastNotification(0x9, false); notified_ = false; }
  }
  void Stop() {
    {
      std::lock_guard lock(mutex_);
      stopping_ = true;
      requested_id_.reset();
      gate->Close();
      if (notified_) { kernel_->BroadcastNotification(0x9, false); notified_ = false; }
    }
    changed_.notify_one();
    if (worker_.joinable()) worker_.join();
  }
  const std::shared_ptr<settings::InputGate> gate;

 private:
  void Run() {
    std::shared_ptr<const Avatar> flag_atlas;
    try { flag_atlas=ReadFlags(); }
    catch(const std::exception&) { REXLOG_WARN("PLAYER_CARD stage=flags event=decode_failed"); }
    std::unique_lock lock(mutex_);
    while (!stopping_) {
      changed_.wait(lock, [this] { return stopping_ || requested_id_.has_value(); });
      if (stopping_) break;
      const auto id = *requested_id_;
      const auto serial = request_serial_;
      requested_id_.reset();
      lock.unlock();
      Player player; bool loaded = false;
      std::shared_ptr<const Results> results;
      try { player = ReadPlayer(id,serial,results); loaded = true; }
      catch (const std::exception& error) {
        REXLOG_WARN("PLAYER_CARD request={} stage=identity event=failed reason={}",serial,error.what());
      }
      lock.lock();
      // Closing/reopening or selecting another player must not publish an
      // older response into the new card.
      if (!stopping_ && serial == snapshot_.serial && gate->IsOpen()) {
        snapshot_.player = player;
        snapshot_.results=std::move(results);
        snapshot_.flags=flag_atlas;
        snapshot_.status = loaded ? Status::Ready : Status::Unavailable;
        if (loaded) {
          // Publish the name first. An absent or slow picture must not delay
          // the profile action or return stale artwork for another player.
          lock.unlock();
          // Metadata is optional. A missing shared profile must not disable a
          // valid MotoGP identity or prevent the legacy avatar-by-name route.
          try { player=ReadAccount(player,serial); }
          catch(const std::exception& error) {
            REXLOG_WARN("PLAYER_CARD request={} stage=account event=failed reason={}",serial,error.what());
          }
          lock.lock();
          if(stopping_||serial!=snapshot_.serial||!gate->IsOpen()) {
            REXLOG_INFO("PLAYER_CARD request={} stage=account event=discarded reason=card_changed_or_closed",serial);
            continue;
          }
          snapshot_.player=player;
          lock.unlock();
          std::shared_ptr<const Avatar> avatar;
          try { avatar=ReadAvatar(player,serial); }
          catch (const std::exception& error) {
            REXLOG_WARN("PLAYER_CARD request={} stage=avatar event=fallback reason={}",serial,error.what());
          }
          lock.lock();
          if (!stopping_ && serial==snapshot_.serial && gate->IsOpen()) snapshot_.avatar=std::move(avatar);
          else REXLOG_INFO("PLAYER_CARD request={} stage=avatar event=discarded reason=card_changed_or_closed",serial);
        }
      } else REXLOG_INFO("PLAYER_CARD request={} stage=identity event=discarded reason=card_changed_or_closed",serial);
    }
  }
  rex::system::KernelState* kernel_;
  bool available_ = false, stopping_ = false, notified_ = false;
  std::mutex mutex_;
  std::condition_variable changed_;
  std::thread worker_;
  Snapshot snapshot_;
  std::optional<uint32_t> requested_id_;
  uint64_t request_serial_ = 0;
};

std::atomic<std::shared_ptr<State>> current;
thread_local uint32_t selected_view=0;

ImU32 Color(int red,int green,int blue,int alpha=255) {
  // GetColorU32 applies the current style alpha to custom geometry too.
  return ImGui::GetColorU32(ImVec4(red/255.f,green/255.f,blue/255.f,alpha/255.f));
}

bool AnimationsEnabled() {
#ifdef _WIN32
  BOOL enabled=TRUE;
  SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION,0,&enabled,0);
  return enabled!=FALSE;
#else
  return true;
#endif
}

void Helmet(ImDrawList* draw, ImVec2 position, float size) {
  const ImVec2 center(position.x + size * .5f, position.y + size * .48f);
  draw->AddRectFilled(position, {position.x + size, position.y + size}, Color(12, 15, 20), 3);
  draw->AddRect(position, {position.x + size, position.y + size}, Color(49, 54, 66), 3);
  draw->AddCircleFilled(center, size * .32f, Color(177, 180, 186), 40);
  draw->AddQuadFilled({center.x - size * .24f, center.y - size * .07f},
      {center.x + size * .29f, center.y - size * .02f},
      {center.x + size * .26f, center.y + size * .19f},
      {center.x - size * .19f, center.y + size * .12f}, Color(22, 25, 32));
  draw->AddTriangleFilled({center.x - size * .26f, center.y + size * .14f},
      {center.x + size * .25f, center.y + size * .28f},
      {center.x - size * .17f, center.y + size * .35f}, Color(137, 142, 152));
  draw->AddTriangleFilled({position.x + size * .13f, position.y + size * .91f},
      {position.x + size * .80f, position.y + size * .91f},
      {position.x + size * .40f, position.y + size * .70f}, Color(77, 84, 99));
}

std::string Membership(int64_t timestamp,bool tr) {
  if(!timestamp)return Tr(tr,"Membership date unavailable","Üyelik tarihi alınamadı");
  constexpr const char* months[]{"January","February","March","April","May","June","July",
    "August","September","October","November","December"};
  constexpr const char* months_tr[]{"Ocak","Şubat","Mart","Nisan","Mayıs","Haziran","Temmuz",
    "Ağustos","Eylül","Ekim","Kasım","Aralık"};
  const std::chrono::year_month_day date{std::chrono::floor<std::chrono::days>(std::chrono::sys_seconds{std::chrono::seconds{timestamp}})};
  const auto month=unsigned(date.month())-1;
  return tr?std::string(months_tr[month])+" "+std::to_string(int(date.year()))+" tarihinden beri üye":
      std::string("Member since ")+months[month]+" "+std::to_string(int(date.year()));
}

std::string Mode(int64_t mode,bool tr) {
  if(mode<0||mode>UINT32_MAX)return "—";
  const auto value=uint32_t(mode);
  if((value&~0x10000000u)>16)return "—";
  if(value&0x10000000u)return Tr(tr,"Championship","Şampiyona");
  // The native 8223F340 getter groups 3..6 as Career, 10 as Time Trial,
  // and 2 as Arcade. Preserve those stored mode bits instead of guessing.
  switch(value) {
    case 1:return Tr(tr,"Quick Race","Hızlı yarış");
    case 2:return Tr(tr,"Arcade","Arcade");
    case 3:case 4:case 5:case 6:return Tr(tr,"Career","Kariyer");
    case 7:return Tr(tr,"Preview","Önizleme");
    case 8:return Tr(tr,"Training","Antrenman");
    case 9:return Tr(tr,"Tutorial","Eğitim");
    case 10:return Tr(tr,"Time Trial","Zamana karşı");
    case 11:return "Splitscreen";
    case 12:return Tr(tr,"Online Splitscreen","Çevrimiçi splitscreen");
    case 13:return "LAN";
    case 14:return Tr(tr,"Online","Çevrimiçi");
    case 15:return "Demo";
    case 16:return Tr(tr,"Replay","Tekrar");
    default:return "—";
  }
}

void Flag(rex::ui::ImmediateTexture* atlas,std::string_view country,float scale) {
  if(!atlas||country.size()!=2)return;
  for(size_t i=0;i<(sizeof(flags::codes)-1)/2;++i) {
    if(country!=std::string_view(flags::codes+i*2,2))continue;
    const float x=float(i%16)*48.f+4.f,y=float(i/16)*32.f+1.f;
    const auto position=ImGui::GetCursorScreenPos();
    ImGui::GetWindowDrawList()->AddImage(reinterpret_cast<ImTextureID>(atlas),position,
        {position.x+28.f*scale,position.y+21.f*scale},
        {x/atlas->width,y/atlas->height},{(x+40.f)/atlas->width,(y+30.f)/atlas->height},
        ImGui::GetColorU32(ImVec4(1,1,1,1)));
    ImGui::Dummy({28.f*scale,21.f*scale});
    if(ImGui::IsItemHovered())ImGui::SetTooltip("%.*s",int(country.size()),country.data());
    return;
  }
  // End SameLine cleanly for unrecognized country codes without a fake flag.
  ImGui::Dummy({0,21.f*scale});
}

struct View {
  int tab=0, category=0;
  float scroll=0.f;
  bool reset_scroll=true;
};
std::vector<std::string> Categories(const Results* results,bool tr) {
  std::vector<std::string> categories{Tr(tr,"All classes","Tüm sınıflar")};
  if(results)for(const auto& row:results->rows)
    if(!row.category.empty()&&std::find(categories.begin()+1,categories.end(),row.category)==categories.end())
      categories.push_back(row.category);
  return categories;
}
std::vector<const Record*> OrderedRecords(const Results& results,uint32_t preferred_view,
    const std::vector<std::string>& categories,int category) {
  std::vector<const Record*> rows;
  for(const auto& row:results.rows)
    if(category==0||row.category==categories[category])rows.push_back(&row);
  if(preferred_view) {
    const auto weight=[&](const Record* row) {
      if(row->view==preferred_view)return 0;
      return row->track==int((preferred_view-1)%21)?1:2;
    };
    std::stable_sort(rows.begin(),rows.end(),[&](auto a,auto b){return weight(a)<weight(b);});
  }
  return rows;
}

void Overview(const Snapshot& state,bool tr,ImFont* heading,float scale) {
  if(!state.results) {
    ImGui::TextDisabled("%s",Tr(tr,"Statistics unavailable","İstatistikler alınamadı"));return;
  }
  const auto& results=*state.results;
  const float width=ImGui::GetContentRegionAvail().x/3.f;
  const std::string values[]{std::to_string(results.tracks)+" / 20",std::to_string(results.firsts),std::to_string(results.rows.size())};
  const char* labels[]{Tr(tr,"TRACKS RECORDED","KAYITLI PİSTLER"),Tr(tr,"RECORDS HELD","LİDER OLDUĞU TABLOLAR"),Tr(tr,"LEADERBOARDS","SIRALAMA TABLOLARI")};
  const float start=ImGui::GetCursorPosX();
  for(int i=0;i<3;++i) {
    if(i)ImGui::SameLine(start+width*i);
    ImGui::BeginGroup();
    if(heading)ImGui::PushFont(heading,30.f*scale);
    ImGui::TextColored(kAccent,"%s",values[i].c_str());
    if(heading)ImGui::PopFont();
    ImGui::TextDisabled("%s",labels[i]);
    ImGui::EndGroup();
  }
  ImGui::Spacing();
  ImGui::PushStyleColor(ImGuiCol_PlotHistogram,kAccent);
  ImGui::PushStyleColor(ImGuiCol_FrameBg,overlay::card);
  ImGui::ProgressBar(float(results.tracks)/20.f,{-1,5.f*scale},"");
  ImGui::PopStyleColor(2);
  ImGui::TextDisabled("%s",Tr(tr,"Track coverage · 20 playable circuits, including DLC","Pist kapsamı · DLC dahil 20 oynanabilir pist"));
  if(state.preferred_view) {
    const auto record=std::find_if(results.rows.begin(),results.rows.end(),[&](const auto& row){return row.view==state.preferred_view;});
    ImGui::Spacing();ImGui::Separator();
    if(record!=results.rows.end()) {
      ImGui::TextDisabled("%s",Tr(tr,"PERSONAL BEST · SELECTED LEADERBOARD","KİŞİSEL EN İYİ · SEÇİLİ TABLO"));
      ImGui::TextColored(kAccent,"%s",record->result.c_str());ImGui::SameLine();
      ImGui::TextUnformatted(record->track_name.c_str());
      ImGui::TextDisabled("%s · %s",record->category.c_str(),Mode(record->mode,tr).c_str());
    } else ImGui::TextDisabled("%s",Tr(tr,"No published personal best on the selected board.","Seçili tabloda yayımlanmış kişisel tur kaydı yok."));
  }
}

void Records(const Snapshot& state,View& view,bool tr,float scale,bool ready) {
  const auto categories=Categories(state.results.get(),tr);
  view.category=std::clamp(view.category,0,int(categories.size())-1);
  ImGui::BeginDisabled(!ready);
  const auto filter=categories[view.category]+"  [X / F]";
  if(ImGui::Button(filter.c_str())) {
    view.category=(view.category+1)%int(categories.size());view.reset_scroll=true;
  }
  ImGui::EndDisabled();ImGui::SameLine();
  ImGui::TextDisabled("%s",Tr(tr,"Personal bests · ranked separately per board","Kişisel en iyi turlar · her tablo ayrı sıralanır"));
  if(ImGui::BeginChild("##player_records",{0,0},ImGuiChildFlags_None,ImGuiWindowFlags_NoNavInputs)) {
    if(view.reset_scroll)ImGui::SetScrollY(0);
    else if(view.scroll)ImGui::SetScrollY(ImGui::GetScrollY()+view.scroll);
    if(!state.results)ImGui::TextWrapped("%s",Tr(tr,"Could not load records for this player.","Oyuncunun tur kayıtları alınamadı."));
    else {
      const auto rows=OrderedRecords(*state.results,state.preferred_view,categories,view.category);
      if(rows.empty())ImGui::TextWrapped("%s",Tr(tr,"No published records in this category.","Bu kategoride yayımlanmış tur kaydı yok."));
      else if(ImGui::BeginTable("##personal_bests",5,ImGuiTableFlags_RowBg|ImGuiTableFlags_BordersInnerH|ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("#",ImGuiTableColumnFlags_WidthFixed,52.f*scale);
        ImGui::TableSetupColumn(Tr(tr,"Track / class","Pist / sınıf"),ImGuiTableColumnFlags_WidthStretch,2.f);
        ImGui::TableSetupColumn(Tr(tr,"Best result","En iyi sonuç"),ImGuiTableColumnFlags_WidthFixed,100.f*scale);
        ImGui::TableSetupColumn(Tr(tr,"Bike / season","Motor / sezon"),ImGuiTableColumnFlags_WidthStretch,1.2f);
        ImGui::TableSetupColumn(Tr(tr,"Mode","Mod"),ImGuiTableColumnFlags_WidthStretch,1.2f);
        ImGui::TableHeadersRow();
        for(const auto* row:rows) {
          ImGui::TableNextRow(0,48.f*scale);
          if(row->view==state.preferred_view)ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0,ImGui::GetColorU32(overlay::AccentTint(.12f)));
          ImGui::TableNextColumn();ImGui::TextColored(row->rank==1?kAccent:kWhite,"%u",row->rank);
          if(row->total)ImGui::TextDisabled("/ %u",row->total);
          ImGui::TableNextColumn();ImGui::TextWrapped("%s",row->track_name.c_str());ImGui::TextDisabled("%s",row->category.c_str());
          ImGui::TableNextColumn();ImGui::TextColored(kAccent,"%s",row->result.c_str());
          ImGui::TableNextColumn();ImGui::TextWrapped("%s",row->bike.empty()?"—":row->bike.c_str());
          if(row->season>=0)ImGui::TextDisabled("%d",2009+row->season);
          ImGui::TableNextColumn();ImGui::TextWrapped("%s",Mode(row->mode,tr).c_str());
        }
        ImGui::EndTable();
      }
    }
  }
  ImGui::EndChild();view.scroll=0;view.reset_scroll=false;
}

struct Action { bool profile = false, close = false; };
Action DrawCard(const Snapshot& state, rex::ui::ImmediateTexture* texture,
    rex::ui::ImmediateTexture* flag_atlas,View& view,bool tr, ImFont* font, ImFont* heading, bool ready,float alpha) {
  const auto& io = ImGui::GetIO();
  const float scale = std::clamp(std::min(io.DisplaySize.x / 960.f, io.DisplaySize.y / 640.f), .65f, 1.5f);
  const ImVec2 size(std::min(760.f * scale, std::max(200.f, io.DisplaySize.x - 24.f)),
      std::min(560.f * scale, std::max(200.f, io.DisplaySize.y - 24.f)));
  auto* background = ImGui::GetBackgroundDrawList();
  ImGui::PushStyleVar(ImGuiStyleVar_Alpha,ImGui::GetStyle().Alpha*alpha);
  background->AddRectFilled({0, 0}, io.DisplaySize, Color(0, 0, 0, 155));
  if (font) ImGui::PushFont(font, 18.f * scale);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {24.f * scale, 20.f * scale});
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 2.f);
  ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 2.f);
  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {14.f * scale, 10.f * scale});
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {14.f * scale, 10.f * scale});
  ImGui::PushStyleColor(ImGuiCol_WindowBg,overlay::ink);
  ImGui::PushStyleColor(ImGuiCol_Border,{.19f,.21f,.26f,1.f});
  ImGui::PushStyleColor(ImGuiCol_Text, kWhite);
  ImGui::PushStyleColor(ImGuiCol_TextDisabled, kMuted);
  ImGui::PushStyleColor(ImGuiCol_Button,overlay::card);
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered,overlay::AccentTint(.24f));
  ImGui::PushStyleColor(ImGuiCol_ButtonActive,overlay::AccentTint(.36f));
  ImGui::PushStyleColor(ImGuiCol_NavCursor,kAccent);
  ImGui::SetNextWindowPos({io.DisplaySize.x * .5f, io.DisplaySize.y * .5f}, ImGuiCond_Always, {.5f, .5f});
  ImGui::SetNextWindowSize(size, ImGuiCond_Always);
  Action action;
  if (ImGui::Begin("##motogp_player_card", nullptr, ImGuiWindowFlags_NoDecoration |
      ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoNavInputs)) {
    const auto origin = ImGui::GetWindowPos();
    auto* draw = ImGui::GetWindowDrawList();
    const float header = 68.f * scale;
    draw->AddRectFilledMultiColor(origin, {origin.x + size.x, origin.y + header},
        ImGui::GetColorU32(overlay::AccentTint(.18f)),ImGui::GetColorU32(overlay::ink),
        ImGui::GetColorU32(overlay::ink),ImGui::GetColorU32(overlay::AccentTint(.08f)));
    draw->AddLine({origin.x, origin.y + header}, {origin.x + size.x, origin.y + header},
        ImGui::GetColorU32(kAccent), 2.f * scale);
    for (int i = 0; i < 3; ++i) {
      const float x = origin.x + (14.f + i * 15.f) * scale;
      const int first_vertex = draw->VtxBuffer.Size;
      draw->AddQuadFilled({x, origin.y + 49.f * scale}, {x + 12.f * scale, origin.y + 49.f * scale},
          {x + 36.f * scale, origin.y + 12.f * scale}, {x + 24.f * scale, origin.y + 12.f * scale},
          ImGui::GetColorU32(kAccent));
      // Match the native title's permanent left-to-right fade across all three
      // stripes. Preserve edge antialiasing and the separate opening animation.
      for (int vertex_index = first_vertex; vertex_index < draw->VtxBuffer.Size; ++vertex_index) {
        auto& vertex = draw->VtxBuffer[vertex_index];
        const float fade = std::clamp((vertex.pos.x - origin.x - 14.f * scale) / (66.f * scale), 0.f, 1.f);
        auto color = ImGui::ColorConvertU32ToFloat4(vertex.col);
        color.w *= fade;
        vertex.col = ImGui::ColorConvertFloat4ToU32(color);
      }
    }
    ImGui::SetCursorPos({86.f * scale, 14.f * scale});
    if (heading) ImGui::PushFont(heading, 30.f * scale);
    ImGui::TextUnformatted(Tr(tr, "PLAYER CARD", "OYUNCU KARTI"));
    if (heading) ImGui::PopFont();
    ImGui::SetCursorPos({24.f * scale, header + 18.f * scale});
    const float avatar = 104.f * scale;
    if (texture) {
      // Fit the complete photo rather than stretching/cropping a non-square
      // response. The site's normal avatars are already square.
      const auto position=ImGui::GetCursorScreenPos();
      draw->AddRectFilled(position,{position.x+avatar,position.y+avatar},Color(12,15,20));
      const float fit=avatar/std::max(texture->width,texture->height);
      const ImVec2 dimensions(texture->width*fit,texture->height*fit);
      const ImVec2 first(position.x+(avatar-dimensions.x)*.5f,position.y+(avatar-dimensions.y)*.5f);
      draw->AddImage(reinterpret_cast<ImTextureID>(texture),first,{first.x+dimensions.x,first.y+dimensions.y},
          {0,0},{1,1},ImGui::GetColorU32(ImVec4(1,1,1,1)));
      draw->AddRect(position,{position.x+avatar,position.y+avatar},Color(49,54,66));
    } else Helmet(draw, ImGui::GetCursorScreenPos(), avatar);
    ImGui::Dummy({avatar, avatar}); ImGui::SameLine(0, 24.f * scale);
    ImGui::BeginGroup();
    ImGui::TextDisabled("NOBK ONLINE");
    ImGui::PushTextWrapPos(size.x - 24.f * scale);
    if (heading) ImGui::PushFont(heading, 30.f * scale);
    ImGui::TextColored(kAccent,"%s",state.status == Status::Ready ? state.player.name.c_str() :
        Tr(tr, state.status == Status::Loading ? "Loading player..." : "Player unavailable",
            state.status == Status::Loading ? "Oyuncu yükleniyor..." : "Oyuncu bilgisi alınamadı"));
    if (heading) ImGui::PopFont();
    if(!state.player.country.empty()&&flag_atlas) {
      ImGui::SameLine(0,12.f*scale);
      ImGui::SetCursorPosY(ImGui::GetCursorPosY()+5.f*scale);
      Flag(flag_atlas,state.player.country,scale);
    }
    ImGui::TextDisabled("%s",Membership(state.player.member_since,tr).c_str());
    ImGui::TextDisabled("MotoGP 09/10");
    ImGui::PopTextWrapPos(); ImGui::EndGroup();
    ImGui::SetCursorPos({24.f*scale,header+144.f*scale});
    ImGui::BeginDisabled(!ready);
    for(int tab=0;tab<2;++tab) {
      if(tab)ImGui::SameLine();
      ImGui::PushStyleColor(ImGuiCol_Button,view.tab==tab?overlay::AccentTint(.18f):overlay::card);
      ImGui::PushStyleColor(ImGuiCol_Text,view.tab==tab?kAccent:kMuted);
      if(ImGui::Button(tab==0?Tr(tr,"Overview","Genel bilgiler"):Tr(tr,"Records","Tur kayıtları"),{150.f*scale,36.f*scale})) {
        view.tab=tab;view.reset_scroll=true;
      }
      ImGui::PopStyleColor(2);
    }
    ImGui::EndDisabled();ImGui::SameLine();ImGui::TextDisabled("LB / RB · Q / E");
    ImGui::SetCursorPos({24.f*scale,header+194.f*scale});
    const float body_height=std::max(20.f*scale,size.y-ImGui::GetCursorPosY()-96.f*scale);
    if(ImGui::BeginChild("##player_card_body",{0,body_height},ImGuiChildFlags_None,ImGuiWindowFlags_NoNavInputs)) {
      if(state.status!=Status::Ready)ImGui::TextWrapped("%s",state.status==Status::Loading?
          Tr(tr,"Loading player...","Oyuncu yükleniyor..."):Tr(tr,"Could not load this player's profile. Close and try again.","Oyuncunun profili yüklenemedi. Kapatıp tekrar dene."));
      else if(view.tab==0)Overview(state,tr,heading,scale);
      else Records(state,view,tr,scale,ready);
    }
    ImGui::EndChild();
    ImGui::SetCursorPos({24.f * scale, size.y - 76.f * scale});
    ImGui::Separator();
    ImGui::BeginDisabled(!ready || state.status != Status::Ready);
    action.profile = ImGui::Button(Tr(tr, "Open NOBK profile", "NOBK profilini aç"));
    ImGui::EndDisabled(); ImGui::SameLine();
    ImGui::BeginDisabled(!ready);
    action.close = ImGui::Button(Tr(tr, "Back", "Geri"));
    ImGui::EndDisabled();
    ImGui::SetCursorPos({24.f * scale, size.y - 24.f * scale});
    ImGui::TextDisabled("%s", Tr(tr, "A / Enter: Profile   B / Esc: Back   D-pad / Arrows: Scroll", "A / Enter: Profil   B / Esc: Geri   Yön tuşları: Kaydır"));
  }
  ImGui::End();
  ImGui::PopStyleColor(8); ImGui::PopStyleVar(6);
  if (font) ImGui::PopFont();
  return action;
}

class Dialog final : public rex::ui::ImGuiDialog, public rex::ui::WindowInputListener {
 public:
  Dialog(rex::ui::ImGuiDrawer* drawer, rex::ui::Window* window,
      rex::ui::ImmediateDrawer* immediate, std::shared_ptr<State> state,
      ImFont* font, ImFont* heading, bool turkish)
      : ImGuiDialog(drawer), window_(window), state_(std::move(state)),
        immediate_(immediate), font_(font), heading_(heading), tr_(turkish) {
    if (window_) window_->AddInputListener(this, 62);
  }
  ~Dialog() override {
    if (window_) window_->RemoveInputListener(this);
    state_->Dismiss();
  }
  void OnKeyDown(rex::ui::KeyEvent& event) override {
    // ImGui has already received the key at priority 64. Let key-UP events
    // reach MnK, but do not forward card navigation to the native menu/F7.
    if (state_->gate->IsOpen() && !(event.is_alt_pressed() &&
        event.virtual_key() == rex::ui::VirtualKey::kF4)) event.set_handled(true);
  }
  void OnKeyChar(rex::ui::KeyEvent& event) override {
    if (state_->gate->IsOpen()) event.set_handled(true);
  }
  void OnMouseDown(rex::ui::MouseEvent& event) override {
    if (state_->gate->IsOpen()) event.set_handled(true);
  }

 protected:
  void OnDraw(ImGuiIO&) override {
    if (!state_->gate->IsOpen()) { armed_ = false; previous_ = 0; return; }
    const auto snapshot = state_->Read();
    const auto epoch = state_->gate->Epoch();
    if (serial_ != snapshot.serial || epoch_ != epoch) {
      if (serial_ != snapshot.serial) {
        texture_.reset(); avatar_.reset();
        view_={};scroll_at_=0.;
        shown_at_=ImGui::GetTime();animate_=AnimationsEnabled();
      }
      serial_ = snapshot.serial; epoch_ = epoch; armed_ = false; previous_ = 0;
      ImGui::SetNextWindowFocus();
    }
    const auto buttons = state_->gate->Buttons();
    if (!armed_ && state_->gate->NeutralReady() && !buttons) armed_ = true;
    const float progress=animate_?std::clamp(static_cast<float>((ImGui::GetTime()-shown_at_)/.18),0.f,1.f):1.f;
    const float alpha=progress*progress*(3.f-2.f*progress);
    const bool ready = armed_ && state_->gate->NeutralReady() && progress>=1.f;
    const uint16_t fresh = ready ? buttons & ~previous_ : 0;
    previous_ = buttons;
    if(snapshot.flags&&snapshot.flags!=flags_) {
      flags_=snapshot.flags;flag_texture_.reset();
      if(flags_&&immediate_)flag_texture_=immediate_->CreateTexture(flags_->width,flags_->height,
          rex::ui::ImmediateTextureFilter::kLinear,false,flags_->rgba.data());
      if(flags_)REXLOG_INFO("PLAYER_CARD request={} stage=flags event=texture_upload success={}",snapshot.serial,flag_texture_!=nullptr);
    }
    if (snapshot.avatar!=avatar_) {
      avatar_=snapshot.avatar;
      texture_.reset();
      if (avatar_ && immediate_) texture_=immediate_->CreateTexture(avatar_->width,avatar_->height,
          rex::ui::ImmediateTextureFilter::kLinear,false,avatar_->rgba.data());
      if(avatar_)REXLOG_INFO("PLAYER_CARD request={} stage=avatar event=texture_upload success={} width={} height={}",
          snapshot.serial,texture_!=nullptr,avatar_->width,avatar_->height);
    }
    if(ready) {
      const bool previous=(fresh&rex::input::X_INPUT_GAMEPAD_LEFT_SHOULDER)||ImGui::IsKeyPressed(ImGuiKey_Q,false);
      const bool next=(fresh&rex::input::X_INPUT_GAMEPAD_RIGHT_SHOULDER)||ImGui::IsKeyPressed(ImGuiKey_E,false);
      if(previous!=next) { view_.tab=1-view_.tab;view_.reset_scroll=true; }
      if(view_.tab==1) {
        if((fresh&rex::input::X_INPUT_GAMEPAD_X)||ImGui::IsKeyPressed(ImGuiKey_F,false)) {
          view_.category=(view_.category+1)%int(Categories(snapshot.results.get(),tr_).size());view_.reset_scroll=true;
        }
        const auto now=ImGui::GetTime();
        const uint16_t directions=buttons&(rex::input::X_INPUT_GAMEPAD_DPAD_UP|rex::input::X_INPUT_GAMEPAD_DPAD_DOWN);
        const bool repeat=directions&&(now>=scroll_at_||(fresh&directions));
        const bool down=(repeat&&(directions&rex::input::X_INPUT_GAMEPAD_DPAD_DOWN))||ImGui::IsKeyPressed(ImGuiKey_DownArrow);
        const bool up=(repeat&&(directions&rex::input::X_INPUT_GAMEPAD_DPAD_UP))||ImGui::IsKeyPressed(ImGuiKey_UpArrow);
        if(down!=up)view_.scroll=(down?1.f:-1.f)*60.f*std::clamp(ImGui::GetIO().DisplaySize.y/640.f,.65f,1.5f);
        if(repeat)scroll_at_=now+.12;
      }
    }
    auto action = DrawCard(snapshot, texture_.get(),flag_texture_.get(),view_, tr_, font_, heading_, ready,alpha);
    if (ready) {
      action.close |= (fresh & rex::input::X_INPUT_GAMEPAD_B) || ImGui::IsKeyPressed(ImGuiKey_Escape, false);
      action.profile |= (fresh & rex::input::X_INPUT_GAMEPAD_A) || ImGui::IsKeyPressed(ImGuiKey_Enter, false);
    }
    if (action.close) { state_->Dismiss(); return; }
    if (action.profile && snapshot.status == Status::Ready) {
      const bool opened=OpenProfile(snapshot.player.name);
      REXLOG_INFO("PLAYER_CARD request={} event=open_profile success={}",snapshot.serial,opened);
    }
  }

 private:
  rex::ui::Window* window_;
  std::shared_ptr<State> state_;
  rex::ui::ImmediateDrawer* immediate_;
  std::shared_ptr<const Avatar> avatar_;
  std::shared_ptr<const Avatar> flags_;
  std::unique_ptr<rex::ui::ImmediateTexture> texture_;
  std::unique_ptr<rex::ui::ImmediateTexture> flag_texture_;
  View view_;
  double scroll_at_=0.;
  ImFont* font_;
  ImFont* heading_;
  bool tr_, armed_ = false;
  bool animate_=true;
  double shown_at_=0.;
  uint64_t serial_ = 0, epoch_ = 0;
  uint16_t previous_ = 0;
};
}

void Initialize(rex::Runtime* runtime, std::shared_ptr<settings::InputGate> gate) {
  current.store(std::make_shared<State>(runtime, std::move(gate)), std::memory_order_release);
}
void Shutdown() {
  if (auto state = current.exchange({}, std::memory_order_acq_rel)) state->Stop();
}
bool Active() {
  const auto state = current.load(std::memory_order_acquire);
  return state && state->gate->Blocking();
}
std::unique_ptr<rex::ui::ImGuiDialog> CreateMenu(rex::ui::ImGuiDrawer* drawer,
    rex::ui::Window* window, rex::ui::ImmediateDrawer* immediate,
    ImFont* font, ImFont* heading, bool turkish) {
  auto state = current.load(std::memory_order_acquire);
  return state ? std::make_unique<Dialog>(drawer, window, immediate, std::move(state), font, heading, turkish) : nullptr;
}
}

REX_HOOK_RAW(__imp__XamShowGamerCardUIForXUID) {
  (void)base;
  // Native 8239C430 sets r5=0 and forwards r3=user / r4=selected XUID.
  // Both the lobby and leaderboard reach it through vtable slot +56 of
  // 82303260. Do not substitute the launcher's signed-in account here.
  const auto state = motogp::gamer_card::current.load(std::memory_order_acquire);
  try { ctx.r3.u64 = state ? state->Show(ctx.r3.u32, ctx.r4.u64,motogp::gamer_card::selected_view) : 1168; }
  catch (const std::exception& error) {
    if (state) state->Dismiss();
    REXLOG_WARN("PLAYER_CARD: could not open: {}", error.what());
    ctx.r3.u64 = 8;
  }
}

REX_HOOK_RAW(sub_82303260) {
  // Only the verified leaderboard call site carries a selected board. The
  // native selector 82300088 stores its current view at 833362D8. Copy the
  // integer synchronously; a lobby call must never inherit the last board.
  auto& selection=motogp::gamer_card::selected_view;
  struct Restore {uint32_t& target;uint32_t previous;~Restore(){target=previous;}} restore{selection,selection};
  selection=0;
  if(ctx.lr==0x8223EDD4) {
    const auto view=rex::memory::load_and_swap<uint32_t>(base+0x833362D8);
    if(view>=1&&view<=378)selection=view;
  }
  __imp__sub_82303260(ctx,base);
}
