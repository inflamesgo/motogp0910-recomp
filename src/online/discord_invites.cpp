#include "discord_invites.h"
#include "social.h"
#include "../discord/join_secret.h"
#include "../../third_party/nlohmann/json.hpp"
#include <algorithm>
#include <atomic>
#include <charconv>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>

namespace motogp::online::discord_invites {
namespace {
using Json=nlohmann::json;
using Clock=std::chrono::steady_clock;
std::mutex notice_mutex;
Notice notice;
void Notify(Outcome outcome){std::lock_guard lock(notice_mutex);notice.outcome=outcome;++notice.serial;}
bool Joinable(const lan::Room& room){return room.id&&room.host&&!room.state&&room.members&&room.members<room.capacity&&room.capacity<=20;}
bool SameRoom(const lan::Room& first,const lan::Room& second){return first.id==second.id&&first.host==second.host&&first.key==second.key;}
std::string Hex(uint64_t value){std::string text(16,'0');constexpr char hex[]="0123456789abcdef";for(int i=15;i>=0;--i){text[i]=hex[value&15];value>>=4;}return text;}
uint64_t Peer(const std::string& text){
  uint64_t value=0;const auto parsed=std::from_chars(text.data(),text.data()+text.size(),value,16);
  if(text.size()!=16||parsed.ec!=std::errc{}||parsed.ptr!=text.data()+text.size()||(value&0xFFFFFFFFFF000000ull)!=0x0009000000000000ull||!(value&0xFFFFFF))throw std::runtime_error("Invalid Discord lobby peer");
  return value;
}
std::vector<uint8_t> Unhex(const std::string& text){
  if(text.empty()||text.size()>2800||text.size()%2)throw std::runtime_error("Invalid Discord lobby descriptor");
  std::vector<uint8_t> bytes;
  for(size_t i=0;i<text.size();i+=2){unsigned value=0;const auto parsed=std::from_chars(text.data()+i,text.data()+i+2,value,16);
    if(parsed.ec!=std::errc{}||parsed.ptr!=text.data()+i+2)throw std::runtime_error("Invalid Discord lobby descriptor");bytes.push_back(uint8_t(value));}
  return bytes;
}
class State {
 public:
  State(std::shared_ptr<Client> client,RoomProvider provider):client_(std::move(client)),provider_(std::move(provider)),worker_([this]{Run();}){}
  ~State(){Stop();}
  void Stop(){
    {std::lock_guard lock(mutex_);stopping_=true;publication_={};queue_.clear();}
    changed_.notify_all();if(worker_.joinable())worker_.join();
  }
  Publication Read(const lan::Room& room){
    std::lock_guard lock(mutex_);
    return !stopping_&&Joinable(room)&&SameRoom(room,room_)&&Clock::now()<expires_?publication_:Publication{};
  }
  bool Queue(const std::string& secret){
    std::lock_guard lock(mutex_);
    if(stopping_||queue_.size()>=4)return false;
    if(active_join_==secret||std::find(queue_.begin(),queue_.end(),secret)!=queue_.end())return true;
    queue_.push_back(secret);Notify(Outcome::Pending);changed_.notify_one();return true;
  }
 private:
  Json Request(const Json& request){
    const auto body=request.dump();
    return Json::parse(client_->Social({reinterpret_cast<const uint8_t*>(body.data()),body.size()}));
  }
  void Accept(const std::string& secret){
    try{
      const auto reply=Request({{"action","discord_join"},{"secret",secret}});
      if(!reply.value("supported",false)){Notify(Outcome::Unsupported);return;}
      if(reply.value("already_joined",false)){Notify(Outcome::AlreadyJoined);return;}
      const auto& accepted=reply.at("accepted");
      auto room=lan::DecodeRoom(Unhex(accepted.at("room").get<std::string>()));
      const auto inviter=Peer(accepted.at("from").get<std::string>());
      if(!room||!Joinable(*room))throw std::runtime_error("Invalid Discord lobby descriptor");
      (void)Peer(Hex(room->host));room->endpoint=VirtualEndpoint(room->host);
      {
        std::lock_guard lock(mutex_);if(stopping_)return;
      }
      if(!social::AdoptInvitation(*room,inviter))throw std::runtime_error("Invitation receiver unavailable");
      Notify(Outcome::Accepted);
    }catch(...){Notify(Outcome::Unavailable);}
  }
  void Publish(){
    const auto room=provider_();const auto now=Clock::now();
    bool revoke=false;
    {
      std::lock_guard lock(mutex_);if(stopping_)return;
      if(!room||!Joinable(*room)){revoke=!publication_.secret.empty();publication_={};room_={};expires_={};next_={};}
      else if(!SameRoom(*room,room_)){publication_={};expires_={};next_={};room_=*room;}
    }
    if(!room||!Joinable(*room)){
      if(revoke)try{(void)Request({{"action","discord_revoke"}});}catch(...){}
      return;
    }
    if(now<next_)return;
    try{
      const auto began=Clock::now();
      const auto reply=Request({{"action","discord_publish"},{"host",Hex(room->host)},{"session",Hex(room->id)}});
      if(!reply.value("supported",false)){next_=Clock::now()+std::chrono::seconds(60);return;}
      const auto secret=reply.at("secret").get<std::string>(),party=reply.at("party_id").get<std::string>();
      const auto ttl=reply.at("expires_in").get<int>();
      if(!discord::ValidJoinSecret(secret)||!discord::ValidPartyId(party)||party!="mg0910:"+Hex(room->host)+":"+Hex(room->id)||
         reply.at("host").get<std::string>()!=Hex(room->host)||reply.at("session").get<std::string>()!=Hex(room->id)||ttl<1||ttl>120)throw std::runtime_error("Invalid Discord lobby publication");
      const auto current=provider_();
      std::lock_guard lock(mutex_);if(stopping_)return;
      if(current&&Joinable(*current)&&SameRoom(*current,*room)){room_=*room;publication_={party,secret};expires_=began+std::chrono::seconds(ttl);}
      next_=Clock::now()+std::chrono::seconds(20);
    }catch(...){next_=Clock::now()+std::chrono::seconds(15);}
  }
  void Run(){
    bool first=true;
    for(;;){
      std::string secret;
      {
        std::unique_lock lock(mutex_);
        if(!first)changed_.wait_for(lock,std::chrono::seconds(2),[&]{return stopping_||!queue_.empty();});
        if(stopping_)return;
        if(!queue_.empty()){secret=std::move(queue_.front());queue_.pop_front();active_join_=secret;}
      }
      first=false;
      if(!secret.empty()){
        Accept(secret);std::lock_guard lock(mutex_);active_join_.clear();
      }else try{Publish();}catch(...){} // Optional publication never unwinds into the game.
    }
  }
  std::shared_ptr<Client> client_;RoomProvider provider_;
  std::mutex mutex_;std::condition_variable changed_;bool stopping_=false;
  Publication publication_;lan::Room room_;Clock::time_point expires_{},next_{};
  std::deque<std::string> queue_;std::string active_join_;std::thread worker_;
};
std::atomic<std::shared_ptr<State>> current;
}
void Start(std::shared_ptr<Client> client,RoomProvider provider,bool enabled){
  Stop();if(enabled&&client&&provider)try{current.store(std::make_shared<State>(std::move(client),std::move(provider)));}catch(...){}
}
void Stop(){if(auto state=current.exchange({}))state->Stop();Notify(Outcome::None);}
Publication Read(const lan::Room& room){if(auto state=current.load())return state->Read(room);return {};}
bool Join(const std::string& secret){
  if(!discord::ValidJoinSecret(secret))return false;
  if(auto state=current.load())return state->Queue(secret);
  Notify(Outcome::OnlineRequired);return false;
}
Notice ReadNotice(){std::lock_guard lock(notice_mutex);return notice;}
}
