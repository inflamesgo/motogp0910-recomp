#include "gamer_card.h"
#include "in_game_settings.h"
#include "quit_menu.h"
#include "overlay_theme.h"
#include "../third_party/nlohmann/json.hpp"

#include <rex/cvar.h>
#include <rex/hook.h>
#include <rex/runtime.h>
#include <rex/input/input_system.h>
#include <rex/ui/immediate_drawer.h>
#include <rex/ui/window.h>
#include <imgui.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <optional>
#include <stdexcept>
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
};
struct Avatar { uint32_t width=0, height=0; std::vector<uint8_t> rgba; };
struct PublicReply { uint32_t status=0; std::string body; };

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

Player ReadPlayer(uint32_t id,uint64_t serial) {
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
};

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

  uint32_t Show(uint32_t user, uint64_t xuid) {
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
    requested_id_ = static_cast<uint32_t>(xuid & 0xFFFFFF);
    request_serial_ = snapshot_.serial;
    REXLOG_INFO("PLAYER_CARD request={} event=open user={} xuid={:016X} player_id={}",
        request_serial_,user,xuid,*requested_id_);
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
    std::unique_lock lock(mutex_);
    while (!stopping_) {
      changed_.wait(lock, [this] { return stopping_ || requested_id_.has_value(); });
      if (stopping_) break;
      const auto id = *requested_id_;
      const auto serial = request_serial_;
      requested_id_.reset();
      lock.unlock();
      Player player; bool loaded = false;
      try { player = ReadPlayer(id,serial); loaded = true; }
      catch (const std::exception& error) {
        REXLOG_WARN("PLAYER_CARD request={} stage=identity event=failed reason={}",serial,error.what());
      }
      lock.lock();
      // Closing/reopening or selecting another player must not publish an
      // older response into the new card.
      if (!stopping_ && serial == snapshot_.serial && gate->IsOpen()) {
        snapshot_.player = player;
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

struct Action { bool profile = false, close = false; };
Action DrawCard(const Snapshot& state, rex::ui::ImmediateTexture* texture,
    bool tr, ImFont* font, ImFont* heading, bool ready,float alpha) {
  const auto& io = ImGui::GetIO();
  const float scale = std::clamp(std::min(io.DisplaySize.x / 960.f, io.DisplaySize.y / 640.f), .65f, 1.5f);
  const ImVec2 size(std::min(760.f * scale, std::max(200.f, io.DisplaySize.x - 24.f)),
      std::min(390.f * scale, std::max(200.f, io.DisplaySize.y - 24.f)));
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
      draw->AddQuadFilled({x, origin.y + 49.f * scale}, {x + 12.f * scale, origin.y + 49.f * scale},
          {x + 36.f * scale, origin.y + 12.f * scale}, {x + 24.f * scale, origin.y + 12.f * scale},
          ImGui::GetColorU32(kAccent));
    }
    ImGui::SetCursorPos({76.f * scale, 14.f * scale});
    if (heading) ImGui::PushFont(heading, 30.f * scale);
    ImGui::TextUnformatted(Tr(tr, "PLAYER CARD", "OYUNCU KARTI"));
    if (heading) ImGui::PopFont();
    ImGui::SetCursorPos({24.f * scale, header + 24.f * scale});
    const float avatar = 132.f * scale;
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
    ImGui::TextColored(kAccent, "NOBK ONLINE");
    ImGui::PushTextWrapPos(size.x - 24.f * scale);
    if (heading) ImGui::PushFont(heading, 30.f * scale);
    ImGui::TextUnformatted(state.status == Status::Ready ? state.player.name.c_str() :
        Tr(tr, state.status == Status::Loading ? "Loading player..." : "Player unavailable",
            state.status == Status::Loading ? "Oyuncu yükleniyor..." : "Oyuncu bilgisi alınamadı"));
    if (heading) ImGui::PopFont();
    if (!state.player.country.empty()) ImGui::Text("%s: %s", Tr(tr, "Country", "Ülke"), state.player.country.c_str());
    ImGui::TextDisabled("MotoGP 09/10");
    ImGui::PopTextWrapPos(); ImGui::EndGroup();
    ImGui::SetCursorPos({24.f * scale, header + 177.f * scale});
    ImGui::PushTextWrapPos(size.x - 24.f * scale);
    ImGui::TextColored(kMuted, "%s", Tr(tr,
        state.status == Status::Unavailable ? "Could not load this player's profile. Close and try again." :
            "Open this player's NOBK Online account profile in your browser.",
        state.status == Status::Unavailable ? "Oyuncunun profili yüklenemedi. Kapatıp tekrar dene." :
            "Oyuncunun NOBK Online hesap profilini tarayıcında aç."));
    ImGui::PopTextWrapPos();
    ImGui::SetCursorPos({24.f * scale, size.y - 76.f * scale});
    ImGui::Separator();
    ImGui::BeginDisabled(!ready || state.status != Status::Ready);
    action.profile = ImGui::Button(Tr(tr, "Open NOBK profile", "NOBK profilini aç"));
    ImGui::EndDisabled(); ImGui::SameLine();
    ImGui::BeginDisabled(!ready);
    action.close = ImGui::Button(Tr(tr, "Back", "Geri"));
    ImGui::EndDisabled();
    ImGui::SetCursorPos({24.f * scale, size.y - 24.f * scale});
    ImGui::TextDisabled("%s", Tr(tr, "A / Enter: Profile   B / Esc: Back", "A / Enter: Profil   B / Esc: Geri"));
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
    if (snapshot.avatar!=avatar_) {
      avatar_=snapshot.avatar;
      texture_.reset();
      if (avatar_ && immediate_) texture_=immediate_->CreateTexture(avatar_->width,avatar_->height,
          rex::ui::ImmediateTextureFilter::kLinear,false,avatar_->rgba.data());
      if(avatar_)REXLOG_INFO("PLAYER_CARD request={} stage=avatar event=texture_upload success={} width={} height={}",
          snapshot.serial,texture_!=nullptr,avatar_->width,avatar_->height);
    }
    auto action = DrawCard(snapshot, texture_.get(), tr_, font_, heading_, ready,alpha);
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
  std::unique_ptr<rex::ui::ImmediateTexture> texture_;
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
  try { ctx.r3.u64 = state ? state->Show(ctx.r3.u32, ctx.r4.u64) : 1168; }
  catch (const std::exception& error) {
    if (state) state->Dismiss();
    REXLOG_WARN("PLAYER_CARD: could not open: {}", error.what());
    ctx.r3.u64 = 8;
  }
}
