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
#include <cfloat>
#include <mutex>
#include <optional>
#include <set>
#include <stop_token>
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
    if(row.result.empty())row.result="-";
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

struct RequestCancelled : std::runtime_error {
  RequestCancelled() : std::runtime_error("Player card request cancelled") {}
};
struct PublicHttpFailure : std::runtime_error {
  uint32_t code;
  PublicHttpFailure(const char* operation,uint32_t error)
      : std::runtime_error(std::string("Public profile request failed at ")+operation),code(error) {}
};

// Owned by the profile worker, so all requests can reuse the session and its
// keep-alive connection. Only request cancellation crosses threads. These are
// public read-only endpoints; no launcher token or account password is sent.
class PublicClient {
#ifdef _WIN32
  struct Handle {
    HINTERNET value=nullptr;
    Handle()=default;
    Handle(const Handle&)=delete;
    Handle& operator=(const Handle&)=delete;
    void Close() { if(value)WinHttpCloseHandle(std::exchange(value,nullptr)); }
    ~Handle() { Close(); }
  };
  struct Exchange {
    std::mutex mutex;
    std::condition_variable_any changed;
    DWORD status=0,error=0,bytes=0;
    bool closed=false,callbacks=false;
    Handle request;

    static void CALLBACK Notify(HINTERNET,DWORD_PTR context,DWORD status,
        void* information,DWORD length) {
      if(!context)return;
      auto& self=*reinterpret_cast<Exchange*>(context);
      std::lock_guard lock(self.mutex);
      if(status==WINHTTP_CALLBACK_STATUS_HANDLE_CLOSING)self.closed=true;
      else if(status==WINHTTP_CALLBACK_STATUS_REQUEST_ERROR) {
        self.error=static_cast<WINHTTP_ASYNC_RESULT*>(information)->dwError;
      } else {
        self.status=status;
        if(status==WINHTTP_CALLBACK_STATUS_READ_COMPLETE)self.bytes=length;
      }
      self.changed.notify_all();
    }
    bool Wait(DWORD expected,std::stop_token cancel,
        std::chrono::steady_clock::time_point deadline) {
      std::unique_lock lock(mutex);
      changed.wait_until(lock,cancel,deadline,[&]{return status==expected||error;});
      if(cancel.stop_requested())throw RequestCancelled();
      if(error) { SetLastError(error);return false; }
      if(status!=expected) { SetLastError(ERROR_WINHTTP_TIMEOUT);return false; }
      status=0;
      return true;
    }
    ~Exchange() {
      request.Close();
      // HANDLE_CLOSING is the last callback. Its context and read buffer must
      // outlive it, including when a card closes during an unfinished read.
      if(callbacks) {
        std::unique_lock lock(mutex);
        changed.wait(lock,[&]{return closed;});
      }
    }
  };
  Handle session_,connection_;
#endif
  bool first_request_=true;
 public:
  PublicReply Get(std::string_view relative_path,size_t max_body,uint64_t serial,
      std::string_view stage,std::stop_token cancel) {
    const auto started=std::chrono::steady_clock::now();
    const auto deadline=started+std::chrono::seconds(5);
    // The observed first-connection timeout recovered immediately when the
    // same profile was reopened. Retry that read-only identity request once,
    // within the original budget, instead of requiring a manual reopen.
    const bool cold=std::exchange(first_request_,false)&&stage=="identity";
    try {
      return GetOnce(relative_path,max_body,serial,stage,cancel,
          cold?started+std::chrono::seconds(2):deadline);
    } catch(const PublicHttpFailure& error) {
#ifdef _WIN32
      const bool transient=error.code==ERROR_WINHTTP_TIMEOUT||error.code==ERROR_WINHTTP_CANNOT_CONNECT||
          error.code==ERROR_WINHTTP_CONNECTION_ERROR||error.code==ERROR_WINHTTP_NAME_NOT_RESOLVED||
          error.code==ERROR_WINHTTP_RESEND_REQUEST;
      if(cancel.stop_requested())throw RequestCancelled();
      const auto remaining=std::chrono::duration_cast<std::chrono::milliseconds>(
          deadline-std::chrono::steady_clock::now()).count();
      if(!cold||!transient||remaining<250)throw;
      REXLOG_INFO("PLAYER_CARD request={} stage={} event=http_retry attempt=2 reason=initial_connection win_error={} remaining_ms={}",
          serial,stage,error.code,remaining);
      return GetOnce(relative_path,max_body,serial,stage,cancel,deadline);
#else
      throw;
#endif
    }
  }
 private:
  PublicReply GetOnce(std::string_view relative_path,size_t max_body,uint64_t serial,
      std::string_view stage,std::stop_token cancel,
      std::chrono::steady_clock::time_point deadline) {
#ifdef _WIN32
  if(cancel.stop_requested())throw RequestCancelled();
  const auto started=std::chrono::steady_clock::now();
  // Unlike separate resolve/connect/read timeouts, this bounds the entire
  // HTTP exchange. Optional metadata cannot occupy the worker indefinitely.
  const auto elapsed=[&] { return std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now()-started).count(); };
  const auto failure=[&](const char* operation) {
    const auto error=GetLastError();
    REXLOG_WARN("PLAYER_CARD request={} stage={} event=http_failed operation={} win_error={} elapsed_ms={}",
        serial,stage,operation,error,elapsed());
    return PublicHttpFailure(operation,error);
  };
  REXLOG_INFO("PLAYER_CARD request={} stage={} event=http_start host=www.nobkonline.com path={}",
      serial,stage,relative_path);
  if(!session_.value) {
    session_.value=WinHttpOpen(L"MotoGP0910-PlayerCard/1",
      WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME,
      WINHTTP_NO_PROXY_BYPASS, WINHTTP_FLAG_ASYNC);
    if(!session_.value)session_.value=WinHttpOpen(L"MotoGP0910-PlayerCard/1",
      WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME,
      WINHTTP_NO_PROXY_BYPASS, WINHTTP_FLAG_ASYNC);
    if(!session_.value)throw failure("session");
    if(!WinHttpSetTimeouts(session_.value,3000,4000,4000,4000)) {
      const auto error=failure("session_timeouts");session_.Close();throw error;
    }
  }
  if(!connection_.value)connection_.value=WinHttpConnect(session_.value,
      L"www.nobkonline.com",INTERNET_DEFAULT_HTTPS_PORT,0);
  if(!connection_.value)throw failure("connect");
  // Paths are built locally or validated against the site's avatar prefix.
  // Server-returned hosts and redirects are never followed.
  const std::wstring path(relative_path.begin(), relative_path.end());
  std::string body;
  char chunk[4096];
  Exchange exchange;
  auto& request=exchange.request;
  request.value=WinHttpOpenRequest(connection_.value,
      L"GET", path.c_str(), nullptr, WINHTTP_NO_REFERER,
      WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
  if (!request.value) throw failure("open");
  DWORD_PTR context=reinterpret_cast<DWORD_PTR>(&exchange);
  if(!WinHttpSetOption(request.value,WINHTTP_OPTION_CONTEXT_VALUE,&context,sizeof(context)))throw failure("context");
  if(WinHttpSetStatusCallback(request.value,&Exchange::Notify,
      WINHTTP_CALLBACK_FLAG_SENDREQUEST_COMPLETE|WINHTTP_CALLBACK_FLAG_HEADERS_AVAILABLE|
      WINHTTP_CALLBACK_FLAG_READ_COMPLETE|WINHTTP_CALLBACK_FLAG_REQUEST_ERROR|
      WINHTTP_CALLBACK_FLAG_HANDLES,0)==WINHTTP_INVALID_STATUS_CALLBACK)throw failure("callback");
  exchange.callbacks=true;
  const auto complete=[&](BOOL initiated,DWORD expected,const char* operation) {
    if(!initiated&&GetLastError()!=ERROR_IO_PENDING)throw failure(operation);
    if(!exchange.Wait(expected,cancel,deadline))throw failure(operation);
    REXLOG_INFO("PLAYER_CARD request={} stage={} event=http_phase operation={} elapsed_ms={}",
        serial,stage,operation,elapsed());
  };
  DWORD redirects = WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
  if (!WinHttpSetOption(request.value, WINHTTP_OPTION_REDIRECT_POLICY,
      &redirects, sizeof(redirects)))throw failure("redirect_policy");
  REXLOG_INFO("PLAYER_CARD request={} stage={} event=http_phase operation=setup elapsed_ms={}",
      serial,stage,elapsed());
  complete(WinHttpSendRequest(request.value,WINHTTP_NO_ADDITIONAL_HEADERS,
      0,WINHTTP_NO_REQUEST_DATA,0,0,context),WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE,"send");
  complete(WinHttpReceiveResponse(request.value,nullptr),WINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE,"headers");
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

  for (;;) {
    if(cancel.stop_requested())throw RequestCancelled();
    if(std::chrono::steady_clock::now()>=deadline) {
      SetLastError(ERROR_WINHTTP_TIMEOUT);throw failure("read");
    }
    if(!WinHttpReadData(request.value,chunk,sizeof(chunk),nullptr)&&GetLastError()!=ERROR_IO_PENDING)
      throw failure("read");
    if(!exchange.Wait(WINHTTP_CALLBACK_STATUS_READ_COMPLETE,cancel,deadline))throw failure("read");
    const DWORD received=exchange.bytes;
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
  (void)relative_path; (void)max_body; (void)serial; (void)stage; (void)cancel; (void)deadline;
  throw std::runtime_error("Player cards currently target Windows");
#endif
  }
};

Player ReadPlayer(PublicClient& client,uint32_t id,uint64_t serial,
    std::shared_ptr<const Results>& results,std::stop_token cancel) {
  const auto response=client.Get("/api/motogp/player?id="+std::to_string(id),256*1024,serial,"identity",cancel);
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

Player ReadAccount(PublicClient& client,Player player,uint64_t serial,std::stop_token cancel) {
  // The general /profile/?u= page itself uses this shared public endpoint,
  // even for accounts whose only game is MotoGP. It is not a FIFA stats lookup
  // by MotoGP numeric ID: the already resolved account name is the query key.
  const auto response=client.Get("/api/fifa09/player?name="+EncodeName(player.name),256*1024,serial,"account",cancel);
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

std::shared_ptr<const Avatar> ReadAvatar(PublicClient& client,const Player& player,
    uint64_t serial,std::stop_token cancel,bool& complete) {
  complete=false;
  if(player.avatar_path&&player.avatar_path->empty()) {
    REXLOG_INFO("PLAYER_CARD request={} stage=avatar event=fallback reason=account_has_no_photo",serial);
    complete=true;
    return {};
  }
  const auto path=player.avatar_path.value_or("/api/avatar/u/"+EncodeName(player.name));
  auto response=client.Get(path,512*1024,serial,"avatar",cancel);
  if(response.status!=200) {
    REXLOG_INFO("PLAYER_CARD request={} stage=avatar event=fallback reason=http_status status={}",serial,response.status);
    complete=response.status==404;
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
  complete=true;
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
    cancellation_.request_stop();
    cancellation_=std::stop_source{};
    requested_id_.reset();
    const auto id=static_cast<uint32_t>(xuid&0xFFFFFF);
    const auto serial=snapshot_.serial+1;
    snapshot_={};
    snapshot_.serial=serial;
    snapshot_.preferred_view=preferred_view;
    bool cached=false,fresh=false;
    const auto now=std::chrono::steady_clock::now();
    for(const auto& entry:cache_) {
      if(entry.id!=id||now-entry.stored>std::chrono::minutes(5))continue;
      snapshot_=entry.data;
      snapshot_.serial=serial;
      snapshot_.preferred_view=preferred_view;
      cached=true;
      fresh=entry.complete&&now-entry.stored<std::chrono::seconds(60);
      break;
    }
    request_serial_=serial;
    if(!fresh)requested_id_=id;
    REXLOG_INFO("PLAYER_CARD request={} event=open user={} xuid={:016X} player_id={} preferred_view={}",
        request_serial_,user,xuid,id,preferred_view);
    REXLOG_INFO("PLAYER_CARD request={} event=cache hit={} fresh={}",serial,cached,fresh);
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
    cancellation_.request_stop();
    requested_id_.reset();
    if (notified_) { kernel_->BroadcastNotification(0x9, false); notified_ = false; }
  }
  void Stop() {
    {
      std::lock_guard lock(mutex_);
      stopping_ = true;
      cancellation_.request_stop();
      requested_id_.reset();
      gate->Close();
      if (notified_) { kernel_->BroadcastNotification(0x9, false); notified_ = false; }
    }
    changed_.notify_one();
    if (worker_.joinable()) worker_.join();
  }
  const std::shared_ptr<settings::InputGate> gate;

 private:
  struct CachedProfile {
    uint32_t id;
    Snapshot data;
    std::chrono::steady_clock::time_point stored;
    bool complete;
  };
  // mutex_ protects the cache and visible snapshot together. Shared immutable
  // image/record data is retained for at most eight players; nothing hits disk.
  void Remember(uint32_t id,bool complete) {
    std::erase_if(cache_,[&](const auto& entry){return entry.id==id;});
    if(cache_.size()>=8)cache_.erase(cache_.begin());
    cache_.push_back({id,snapshot_,std::chrono::steady_clock::now(),complete});
  }
  void Run() {
    PublicClient client;
    std::shared_ptr<const Avatar> flag_atlas;
    try { flag_atlas=ReadFlags(); }
    catch(const std::exception&) { REXLOG_WARN("PLAYER_CARD stage=flags event=decode_failed"); }
    std::unique_lock lock(mutex_);
    while (!stopping_) {
      changed_.wait(lock, [this] { return stopping_ || requested_id_.has_value(); });
      if (stopping_) break;
      const auto id = *requested_id_;
      const auto serial = request_serial_;
      const auto cancel=cancellation_.get_token();
      requested_id_.reset();
      lock.unlock();
      Player player; bool loaded = false;
      std::shared_ptr<const Results> results;
      try { player = ReadPlayer(client,id,serial,results,cancel); loaded = true; }
      catch(const RequestCancelled&) {
        REXLOG_INFO("PLAYER_CARD request={} stage=identity event=cancelled",serial);
      }
      catch (const std::exception& error) {
        REXLOG_WARN("PLAYER_CARD request={} stage=identity event=failed reason={}",serial,error.what());
      }
      lock.lock();
      // Closing/reopening or selecting another player must not publish an
      // older response into the new card.
      if (!stopping_ && !cancel.stop_requested() && serial == snapshot_.serial && gate->IsOpen()) {
        snapshot_.flags=flag_atlas;
        if(loaded) {
          // A cached photo can remain visible during a same-player refresh.
          // A renamed account must never inherit its old account artwork.
          if(!SameName(snapshot_.player.name,player.name))snapshot_.avatar.reset();
          else {
            if(player.country.empty())player.country=snapshot_.player.country;
            player.member_since=snapshot_.player.member_since;
            player.avatar_path=snapshot_.player.avatar_path;
          }
          snapshot_.player=player;
          snapshot_.results=std::move(results);
          snapshot_.status=Status::Ready;
          Remember(id,false);
        } else if(snapshot_.status!=Status::Ready)snapshot_.status=Status::Unavailable;
        if (loaded) {
          // Publish the name first. An absent or slow picture must not delay
          // the profile action or return stale artwork for another player.
          lock.unlock();
          // Metadata is optional. A missing shared profile must not disable a
          // valid MotoGP identity or prevent the legacy avatar-by-name route.
          bool account_loaded=false;
          try { player=ReadAccount(client,player,serial,cancel);account_loaded=true; }
          catch(const RequestCancelled&) {
            REXLOG_INFO("PLAYER_CARD request={} stage=account event=cancelled",serial);
          }
          catch(const std::exception& error) {
            REXLOG_WARN("PLAYER_CARD request={} stage=account event=failed reason={}",serial,error.what());
          }
          lock.lock();
          if(stopping_||cancel.stop_requested()||serial!=snapshot_.serial||!gate->IsOpen()) {
            REXLOG_INFO("PLAYER_CARD request={} stage=account event=discarded reason=card_changed_or_closed",serial);
            continue;
          }
          snapshot_.player=player;
          Remember(id,false);
          lock.unlock();
          std::shared_ptr<const Avatar> avatar;
          bool avatar_loaded=false;
          try { avatar=ReadAvatar(client,player,serial,cancel,avatar_loaded); }
          catch(const RequestCancelled&) {
            REXLOG_INFO("PLAYER_CARD request={} stage=avatar event=cancelled",serial);
          }
          catch (const std::exception& error) {
            REXLOG_WARN("PLAYER_CARD request={} stage=avatar event=fallback reason={}",serial,error.what());
          }
          lock.lock();
          if (!stopping_ && !cancel.stop_requested() && serial==snapshot_.serial && gate->IsOpen()) {
            if(avatar_loaded)snapshot_.avatar=std::move(avatar);
            Remember(id,account_loaded&&avatar_loaded);
          }
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
  std::stop_source cancellation_;
  std::vector<CachedProfile> cache_;
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
  if(mode<0||mode>UINT32_MAX)return "-";
  const auto value=uint32_t(mode);
  if((value&~0x10000000u)>16)return "-";
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
    default:return "-";
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
  int tab=0, category=0, first_row=0;
  float scroll=0.f, drag_offset=-1.f;
  bool reset_scroll=true;
};
std::vector<std::string> Categories(const Results* results,bool tr) {
  std::vector<std::string> categories{Tr(tr,"All classes","Tüm sınıflar")};
  if(results)for(const auto& row:results->rows)
    if(row.season!=1&&!row.category.empty()&&std::find(categories.begin()+1,categories.end(),row.category)==categories.end())
      categories.push_back(row.category);
  return categories;
}
std::vector<const Record*> OrderedRecords(const Results& results,uint32_t preferred_view,
    const std::vector<std::string>& categories,int category) {
  std::vector<const Record*> rows;
  // 2009 is the agreed initial scope. Do not relabel known 2010 records as 2009.
  for(const auto& row:results.rows)
    if(row.season!=1&&(category==0||row.category==categories[category]))rows.push_back(&row);
  if(preferred_view) {
    const auto weight=[&](const Record* row) {
      if(row->view==preferred_view)return 0;
      return row->track==int((preferred_view-1)%21)?1:2;
    };
    std::stable_sort(rows.begin(),rows.end(),[&](auto a,auto b){return weight(a)<weight(b);});
  }
  return rows;
}

// Coordinates below share one design canvas. Text and hit targets use the same
// scale, so saved child-window scroll and ImGui line spacing cannot clip stats.
struct Canvas {
  ImDrawList* draw;
  ImVec2 origin;
  float scale;
  ImFont* font;
  ImVec2 Point(float x,float y)const{return {origin.x+x*scale,origin.y+y*scale};}
  void Line(float x,float y,float end_x,float end_y)const {
    draw->AddLine(Point(x,y),Point(end_x,end_y),Color(48,54,64),scale);
  }
  float Text(float x,float y,float pixels,ImVec4 color,std::string text,float width=10000.f)const {
    const auto measure=[&](const std::string& value){return font->CalcTextSizeA(pixels*scale,FLT_MAX,0,value.c_str()).x;};
    if(measure(text)>width*scale) {
      while(!text.empty()&&measure(text+"...")>width*scale) {
        size_t end=text.size()-1;
        while(end&&(static_cast<unsigned char>(text[end])&0xC0)==0x80)--end;
        text.resize(end);
      }
      text+="...";
    }
    draw->AddText(font,pixels*scale,Point(x,y),ImGui::GetColorU32(color),text.c_str());
    return measure(text)/scale;
  }
};
void Stripes(const Canvas& c) {
  // Subdivide the native diagonal direction, not the entire three-stripe box.
  // Shared vertices/colors at each band avoid seams; smoothstep avoids a harsh
  // transparent edge and reaches full orange at the upper end of each stripe.
  const auto uv=ImGui::GetFontTexUvWhitePixel();
  for(int stripe=0;stripe<3;++stripe)for(int band=0;band<24;++band) {
    const float a=band/24.f,b=(band+1)/24.f;
    const auto point=[&](float t,float edge){return c.Point(18.f+stripe*15.f+24.f*t+edge,49.f-36.f*t);};
    const auto tint=[&](float t) {
      auto color=kAccent;
      color.w=(.015f+.985f*t*t*(3.f-2.f*t))*(.66f+.17f*stripe);
      return ImGui::GetColorU32(color);
    };
    c.draw->PrimReserve(6,4);
    const auto index=c.draw->_VtxCurrentIdx;
    c.draw->PrimWriteIdx(static_cast<ImDrawIdx>(index));
    c.draw->PrimWriteIdx(static_cast<ImDrawIdx>(index+1));
    c.draw->PrimWriteIdx(static_cast<ImDrawIdx>(index+2));
    c.draw->PrimWriteIdx(static_cast<ImDrawIdx>(index));
    c.draw->PrimWriteIdx(static_cast<ImDrawIdx>(index+2));
    c.draw->PrimWriteIdx(static_cast<ImDrawIdx>(index+3));
    c.draw->PrimWriteVtx(point(a,0),uv,tint(a));
    c.draw->PrimWriteVtx(point(a,12),uv,tint(a));
    c.draw->PrimWriteVtx(point(b,12),uv,tint(b));
    c.draw->PrimWriteVtx(point(b,0),uv,tint(b));
  }
}
void Portrait(const Canvas& c,rex::ui::ImmediateTexture* texture,float side) {
  const auto position=c.Point(32,96);
  const float size=side*c.scale;
  if(!texture){Helmet(c.draw,position,size);return;}
  c.draw->AddRectFilled(position,{position.x+size,position.y+size},Color(12,15,20));
  const float fit=size/std::max(texture->width,texture->height);
  const ImVec2 dimensions(texture->width*fit,texture->height*fit);
  const ImVec2 first(position.x+(size-dimensions.x)*.5f,position.y+(size-dimensions.y)*.5f);
  c.draw->AddImage(reinterpret_cast<ImTextureID>(texture),first,{first.x+dimensions.x,first.y+dimensions.y},
      {0,0},{1,1},ImGui::GetColorU32(ImVec4(1,1,1,1)));
  c.draw->AddRect(position,{position.x+size,position.y+size},Color(49,54,66));
}
void Overview(const Snapshot& state,const Canvas& c,float x,float width,bool tr) {
  if(!state.results){c.Text(x,160,16,kMuted,Tr(tr,"Statistics unavailable","İstatistikler alınamadı"),width);return;}
  const auto categories=Categories(state.results.get(),tr);
  const auto rows=OrderedRecords(*state.results,0,categories,0);
  std::set<int> tracks;
  unsigned firsts=0;
  for(const auto* row:rows){if(row->track!=17)tracks.insert(row->track);if(row->rank==1)++firsts;}
  const std::string values[]{std::to_string(tracks.size())+" / 20",std::to_string(firsts),std::to_string(rows.size())};
  const char* labels[]{Tr(tr,"TRACKS RECORDED","KAYITLI PİSTLER"),Tr(tr,"RECORDS HELD","LİDER OLDUĞU TABLOLAR"),Tr(tr,"LEADERBOARDS","SIRALAMA TABLOLARI")};
  const float column=width/3.f;
  for(int i=0;i<3;++i) {
    const float at=x+i*column;
    c.Text(at,158,32,kAccent,values[i],column-14);
    c.Text(at,202,12.5f,kMuted,labels[i],column-14);
    if(i)c.Line(at-24,160,at-24,223);
  }
  const float fraction=std::clamp(float(tracks.size())/20.f,0.f,1.f),bar=width-54.f;
  c.draw->AddRectFilled(c.Point(x,258),c.Point(x+bar,267),ImGui::GetColorU32(overlay::card),2*c.scale);
  if(fraction)c.draw->AddRectFilled(c.Point(x,258),c.Point(x+bar*fraction,267),ImGui::GetColorU32(kAccent),2*c.scale);
  c.Text(x+bar+14,254,15,kWhite,std::to_string(int(fraction*100.f+.5f))+"%",40);
  c.Line(x,315,x+width,315);
  c.Text(x,338,13,kMuted,Tr(tr,"PERSONAL BEST","KİŞİSEL EN İYİ"),width);
  if(!state.preferred_view) {
    c.Text(x,382,16,kMuted,Tr(tr,"No track selected.","Seçili pist yok."),width);return;
  }
  const auto record=std::find_if(rows.begin(),rows.end(),[&](auto row){return row->view==state.preferred_view;});
  if(record==rows.end()) {
    c.Text(x,382,16,kMuted,Tr(tr,"No published lap for these conditions.","Bu koşullarda kayıtlı turu yok."),width);return;
  }
  const auto& row=**record;
  const float time_width=std::min(206.f,width*.28f),rank_width=92.f,details=x+time_width;
  c.Text(x,378,36,kAccent,row.result,time_width-16);
  c.Text(details,380,17,kWhite,row.track_name,width-time_width-rank_width-16);
  c.Text(details,411,14,kMuted,row.category+" · 2009 · "+Mode(row.mode,tr),width-time_width-rank_width-16);
  c.Line(x+width-rank_width,374,x+width-rank_width,440);
  c.Text(x+width-rank_width+20,378,13,kMuted,Tr(tr,"Rank","Sıra"),rank_width-20);
  c.Text(x+width-rank_width+20,402,26,kAccent,std::to_string(row.rank),rank_width-20);
  if(row.total)c.Text(x+width-rank_width+44,412,16,kMuted,"/ "+std::to_string(row.total),rank_width-44);
}
void Records(const Snapshot& state,View& view,const Canvas& c,float x,float width,float bottom,bool tr,bool ready) {
  const auto categories=Categories(state.results.get(),tr);
  view.category=std::clamp(view.category,0,int(categories.size())-1);
  ImGui::SetCursorScreenPos(c.Point(x,143));
  ImGui::SetNextItemWidth(174*c.scale);
  ImGui::BeginDisabled(!ready);
  if(ImGui::BeginCombo("##license_class",categories[view.category].c_str())) {
    for(int i=0;i<int(categories.size());++i)
      if(ImGui::Selectable(categories[i].c_str(),view.category==i)){view.category=i;view.reset_scroll=true;}
    ImGui::EndCombo();
  }
  ImGui::EndDisabled();
  c.Text(x+188,149,12,kMuted,"[X / F]",70);
  if(!state.results){c.Text(x,211,16,kMuted,Tr(tr,"Records unavailable","Tur kayıtları alınamadı"),width);return;}
  const auto rows=OrderedRecords(*state.results,state.preferred_view,categories,view.category);
  c.Text(x+width-90,149,13,kMuted,std::to_string(rows.size())+Tr(tr," records"," kayıt"),90);
  const float track=x+100.f,time=x+width-236.f,mode=x+width-112.f;
  c.Text(x+12,194,13,kMuted,Tr(tr,"Rank","Sıra"),80);
  c.Text(track,194,13,kMuted,Tr(tr,"Track / class","Pist / sınıf"),time-track-16);
  c.Text(time,194,13,kMuted,Tr(tr,"Best lap","En iyi tur"),110);
  c.Text(mode,194,13,kMuted,Tr(tr,"Mode","Mod"),100);
  const float top=220.f,available=bottom-top;
  const int visible=std::max(1,int(available/46.f));
  const float row_height=available/visible;
  const int maximum=std::max(0,int(rows.size())-visible);
  if(view.reset_scroll)view.first_row=0;
  else if(view.scroll)view.first_row+=view.scroll>0?1:-1;
  const auto first=c.Point(x,top),last=c.Point(x+width,bottom);
  if(ready&&ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)&&ImGui::IsMouseHoveringRect(first,last))
    view.first_row-=int(ImGui::GetIO().MouseWheel*3);
  view.first_row=std::clamp(view.first_row,0,maximum);
  // Only complete rows are drawn. Scrolling changes the first row, never the
  // whole-card position, so header, filters and footer cannot be scrolled away.
  if(rows.empty())c.Text(x,top+20,16,kMuted,Tr(tr,"No published records in this category.","Bu kategoride yayımlanmış tur kaydı yok."),width);
  for(int i=0;i<visible&&view.first_row+i<int(rows.size());++i) {
    const auto& row=*rows[view.first_row+i];
    const float y=top+i*row_height;
    if(row.view==state.preferred_view)c.draw->AddRectFilled(c.Point(x,y),c.Point(x+width-18,y+row_height),ImGui::GetColorU32(overlay::AccentTint(.10f)));
    c.Text(x+12,y+12,20,row.rank==1?kAccent:kWhite,std::to_string(row.rank),36);
    if(row.total)c.Text(x+42,y+15,14,kMuted,"/ "+std::to_string(row.total),52);
    c.Text(track,y+6,15,kWhite,row.track_name,time-track-18);
    c.Text(track,y+29,12.5f,kMuted,row.category+" · 2009",time-track-18);
    c.Text(time,y+13,17,kAccent,row.result,112);
    c.Text(mode,y+15,14,kWhite,Mode(row.mode,tr),92);
    c.Line(x,y+row_height,x+width-18,y+row_height);
  }
  if(maximum) {
    const float height=available,thumb=std::max(22.f,height*visible/rows.size());
    const float thumb_y=top+(height-thumb)*view.first_row/maximum;
    ImGui::SetCursorScreenPos(c.Point(x+width-10,top));
    ImGui::BeginDisabled(!ready);
    ImGui::InvisibleButton("##license_scrollbar",{10*c.scale,height*c.scale});
    if(ImGui::IsItemActive()) {
      const float mouse=(ImGui::GetIO().MousePos.y-c.origin.y)/c.scale;
      if(view.drag_offset<0)view.drag_offset=mouse>=thumb_y&&mouse<=thumb_y+thumb?mouse-thumb_y:thumb*.5f;
      const float fraction=std::clamp((mouse-top-view.drag_offset)/(height-thumb),0.f,1.f);
      view.first_row=int(fraction*maximum+.5f);
    } else view.drag_offset=-1;
    ImGui::EndDisabled();
    c.draw->AddRectFilled(c.Point(x+width-7,top),c.Point(x+width-3,bottom),Color(35,41,50),2*c.scale);
    c.draw->AddRectFilled(c.Point(x+width-7,thumb_y),c.Point(x+width-3,thumb_y+thumb),Color(119,134,154),2*c.scale);
  }
  view.scroll=0;view.reset_scroll=false;
}

struct Action { bool profile = false, close = false; };
Action DrawCard(const Snapshot& state, rex::ui::ImmediateTexture* texture,
    rex::ui::ImmediateTexture* flag_atlas,View& view,bool tr, ImFont* font, ImFont* heading, bool ready,float alpha) {
  (void)heading;
  const auto& io=ImGui::GetIO();
  const float scale=std::max(.1f,std::min({io.DisplaySize.x/1440.f,io.DisplaySize.y/900.f,1.35f}));
  const ImVec2 size(1100.f*scale,580.f*scale);
  ImGui::PushStyleVar(ImGuiStyleVar_Alpha,ImGui::GetStyle().Alpha*alpha);
  ImGui::GetBackgroundDrawList()->AddRectFilled({0,0},io.DisplaySize,Color(0,0,0,155));
  ImGui::PushFont(font?font:ImGui::GetFont(),16.f*scale);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{0,0});
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,2.f);
  ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding,2.f);
  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,{12.f*scale,7.f*scale});
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,{10.f*scale,8.f*scale});
  ImGui::PushStyleColor(ImGuiCol_WindowBg,overlay::ink);
  ImGui::PushStyleColor(ImGuiCol_Border,ImVec4(.19f,.21f,.26f,1));
  ImGui::PushStyleColor(ImGuiCol_Text,kWhite);
  ImGui::PushStyleColor(ImGuiCol_TextDisabled,kMuted);
  ImGui::PushStyleColor(ImGuiCol_Button,overlay::card);
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered,overlay::AccentTint(.24f));
  ImGui::PushStyleColor(ImGuiCol_ButtonActive,overlay::AccentTint(.36f));
  ImGui::PushStyleColor(ImGuiCol_NavCursor,kAccent);
  ImGui::PushStyleColor(ImGuiCol_FrameBg,overlay::card);
  ImGui::PushStyleColor(ImGuiCol_PopupBg,overlay::ink);
  ImGui::SetNextWindowPos({io.DisplaySize.x*.5f,io.DisplaySize.y*.5f},ImGuiCond_Always,{.5f,.5f});
  ImGui::SetNextWindowSize(size,ImGuiCond_Always);
  Action action;
  if(ImGui::Begin("##motogp_player_card",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoSavedSettings|
      ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoNavInputs|ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse)) {
    ImGui::SetScrollY(0);
    Canvas c{ImGui::GetWindowDrawList(),ImGui::GetWindowPos(),scale,font?font:ImGui::GetFont()};
    c.draw->AddRectFilledMultiColor(c.Point(0,0),c.Point(1100,64),
        ImGui::GetColorU32(overlay::AccentTint(.10f)),ImGui::GetColorU32(overlay::ink),
        ImGui::GetColorU32(overlay::ink),ImGui::GetColorU32(overlay::ink));
    Stripes(c);
    c.Text(96,21,24,kWhite,Tr(tr,"RIDER LICENSE","PİLOT LİSANSI"),900);
    c.draw->AddLine(c.Point(0,64),c.Point(1100,64),ImGui::GetColorU32(kAccent),scale);
    c.Line(260,84,260,496);
    Portrait(c,texture,196);
    const auto name=state.status==Status::Ready?state.player.name:
        Tr(tr,state.status==Status::Loading?"Loading...":"Player unavailable",state.status==Status::Loading?"Yükleniyor...":"Oyuncu bilgisi alınamadı");
    const float name_width=c.Text(32,310,26,kAccent,name,154);
    if(flag_atlas&&!state.player.country.empty()){
      ImGui::SetCursorScreenPos(c.Point(32+name_width+12,315));Flag(flag_atlas,state.player.country,scale);
    }
    c.Text(32,365,15,kMuted,Tr(tr,"Member since","Üyelik tarihi"),196);
    auto membership=Membership(state.player.member_since,tr);
    if(state.player.member_since) {
      if(tr)membership.erase(membership.find(" tarihinden"));
      else membership.erase(0,std::string_view("Member since ").size());
    } else membership="-";
    c.Text(32,389,15,kMuted,membership,196);
    constexpr float content_x=292.f,content_width=776.f;
    for(int tab=0;tab<2;++tab) {
      ImGui::SetCursorScreenPos(c.Point(content_x+tab*170,84));
      ImGui::BeginDisabled(!ready);
      ImGui::PushID(tab);
      if(ImGui::InvisibleButton("license_tab",{164*scale,38*scale})){view.tab=tab;view.reset_scroll=true;}
      ImGui::PopID();
      ImGui::EndDisabled();
      c.Text(content_x+tab*170+32,92,18,view.tab==tab?kAccent:kMuted,tab?Tr(tr,"Records","Tur kayıtları"):Tr(tr,"Overview","Genel bilgiler"),135);
    }
    c.Line(content_x,124,content_x+content_width,124);
    c.draw->AddLine(c.Point(content_x+view.tab*170,124),c.Point(content_x+view.tab*170+164,124),ImGui::GetColorU32(kAccent),3*scale);
    if(state.status!=Status::Ready)c.Text(content_x,170,16,kMuted,state.status==Status::Loading?
        Tr(tr,"Loading player...","Oyuncu yükleniyor..."):Tr(tr,"Could not load this profile. Close and try again.","Profil yüklenemedi. Kapatıp tekrar dene."),content_width);
    else if(view.tab==0)Overview(state,c,content_x,content_width,tr);
    else Records(state,view,c,content_x,content_width,505,tr,ready);
    c.Line(32,516,1068,516);
    ImGui::SetCursorScreenPos(c.Point(32,534));
    ImGui::BeginDisabled(!ready||state.status!=Status::Ready);
    action.profile=ImGui::Button(Tr(tr,"Open NOBK Online profile","NOBK Online profilini aç"));
    ImGui::EndDisabled();
    c.Text(474,543,12,kMuted,view.tab==1?
        Tr(tr,"LB / RB: Tabs   A / Enter: Profile   B / Esc: Back   D-pad: Scroll","LB / RB: Sekmeler   A / Enter: Profil   B / Esc: Geri   Yön: Kaydır"):
        Tr(tr,"LB / RB: Tabs   A / Enter: Profile   B / Esc: Back","LB / RB: Sekmeler   A / Enter: Profil   B / Esc: Geri"),594);
  }
  ImGui::End();
  ImGui::PopStyleColor(10);ImGui::PopStyleVar(6);ImGui::PopFont();
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
