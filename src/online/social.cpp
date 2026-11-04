#include "social.h"
#include "social_codec.h"
#include "relay.h"
#include <rex/system/kernel_state.h>
#include "../../third_party/nlohmann/json.hpp"
#include <rex/system/xam/user_profile.h>
#include <atomic>
#include <algorithm>
#include <charconv>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>

namespace motogp::online::social {
namespace {
using Json=nlohmann::json;
uint64_t Peer(const std::string& text) {
  uint64_t value=0;
  const auto result=std::from_chars(text.data(),text.data()+text.size(),value,16);
  if(text.size()!=16||result.ec!=std::errc{}||result.ptr!=text.data()+text.size()||
      (value&0xFFFFFFFFFF000000ull)!=0x0009000000000000ull||!(value&0xFFFFFF))throw std::runtime_error("Invalid social player identity");
  return value;
}
std::string Hex(uint64_t value) {
  std::string text(16,'0');constexpr char hex[]="0123456789abcdef";
  for(int i=15;i>=0;--i){text[i]=hex[value&15];value>>=4;}return text;
}
std::vector<uint8_t> Unhex(const std::string& text) {
  if(text.empty()||text.size()>2800||text.size()%2)throw std::runtime_error("Invalid invited room");
  std::vector<uint8_t> out;
  for(size_t i=0;i<text.size();i+=2){unsigned value=0;const auto result=std::from_chars(text.data()+i,text.data()+i+2,value,16);
    if(result.ec!=std::errc{}||result.ptr!=text.data()+i+2)throw std::runtime_error("Invalid invited room");out.push_back(uint8_t(value));}
  return out;
}
class State {
 public:
  State(rex::system::KernelState* kernel,std::shared_ptr<Client> client,RoomProvider provider):kernel_(kernel),client_(std::move(client)),provider_(std::move(provider)),worker_([this]{Run();}){}
  ~State(){Stop();}
  void Stop(){
    {std::lock_guard lock(mutex_);stopping_=true;}changed_.notify_all();
    if(worker_.joinable())worker_.join();
  }
  Snapshot Read(){std::lock_guard lock(mutex_);return snapshot_;}
  bool Queue(Json action){
    const auto room=provider_?provider_():std::nullopt;
    std::lock_guard lock(mutex_);
    if(stopping_||!snapshot_.supported||queue_.size()>=8)return false;
    SyncRoom(room);
    if(action.value("action","")=="send") {
      const auto peer=Peer(action.at("to").get<std::string>());
      const auto found=std::find_if(snapshot_.friends.begin(),snapshot_.friends.end(),[&](const auto& f){return f.peer==peer&&f.online;});
      if(!snapshot_.can_invite||found==snapshot_.friends.end()||(provider_&&!Joinable(room)))return false;
      const auto pending=snapshot_.outgoing.find(peer);
      if(pending!=snapshot_.outgoing.end()&&pending->second==SendState::Pending)return false;
      snapshot_.outgoing[peer]=SendState::Pending;
    }
    if(action.value("action","")=="accept"||action.value("action","")=="decline") {
      const auto id=action.at("id").get<std::string>();
      if(std::none_of(snapshot_.invites.begin(),snapshot_.invites.end(),[&](const auto& invite){return invite.id==id;})||
          !snapshot_.answering.insert(id).second)return false;
    }
    snapshot_.action="sending";snapshot_.error.clear();
    queue_.push_back({std::move(action),room});changed_.notify_one();return true;
  }
  std::optional<lan::Room> Room(){std::lock_guard lock(mutex_);return accepted_;}
  bool Adopt(const lan::Room& room,uint64_t inviter){
    const auto valid_peer=[](uint64_t peer){return (peer&0xFFFFFFFFFF000000ull)==0x0009000000000000ull&&(peer&0xFFFFFF);};
    if(!room.id||room.state||!room.members||room.members>=room.capacity||room.capacity>20||!valid_peer(room.host)||!valid_peer(inviter))return false;
    {
      std::lock_guard lock(mutex_);if(stopping_)return false;
      accepted_=room;accepted_inviter_=inviter;
    }
    kernel_->BroadcastNotification(0x02000002,0);
    return true;
  }
  std::optional<std::array<uint8_t,84>> Info(){
    std::lock_guard lock(mutex_);if(!accepted_)return {};
    return InviteInfo(kernel_->user_profile()->xuid(),accepted_inviter_,*accepted_);
  }
 private:
  static bool Joinable(const std::optional<lan::Room>& room) {
    return room&&room->id&&room->host&&!room->state&&room->members<room->capacity;
  }
  static bool SameRoom(const std::optional<lan::Room>& a,const std::optional<lan::Room>& b) {
    return a&&b&&a->id==b->id&&a->host==b->host&&a->key==b->key;
  }
  void SyncRoom(const std::optional<lan::Room>& room) {
    if(!provider_)return;
    if(!Joinable(room)||!SameRoom(room,feedback_room_))snapshot_.outgoing.clear();
    feedback_room_=room;
  }
  struct Action {Json body;std::optional<lan::Room> room;};
  void Run(){
    bool first=true;
    for(;;){
      Json request={{"action","poll"}};
      std::optional<lan::Room> request_room;
      {
        std::unique_lock lock(mutex_);
        if(!first)changed_.wait_for(lock,std::chrono::seconds(snapshot_.supported?3:15),[&]{return stopping_||!queue_.empty();});
        if(stopping_)return;
        if(!queue_.empty()){request=std::move(queue_.front().body);request_room=std::move(queue_.front().room);queue_.pop_front();}
      }
      first=false;
      const bool sending=request.value("action","")=="send";
      const uint64_t recipient=sending?Peer(request.at("to").get<std::string>()):0;
      if(sending&&provider_) {
        const auto room=provider_();
        std::lock_guard lock(mutex_);SyncRoom(room);
        if(!Joinable(room)||!SameRoom(room,request_room))continue;
      }
      try {
        const auto body=request.dump();
        const auto raw=client_->Social({reinterpret_cast<const uint8_t*>(body.data()),body.size()});
        const auto reply=Json::parse(raw);
        Snapshot next;next.loaded=true;next.supported=reply.value("supported",false);next.can_invite=reply.value("can_invite",false);next.action=request.value("action","");
        if(next.supported){
          const auto& friends=reply.at("friends");const auto& invites=reply.at("invites");
          if(!friends.is_array()||friends.size()>100||!invites.is_array()||invites.size()>16)throw std::runtime_error("Invalid social snapshot");
          for(const auto& row:friends){auto name=row.at("name").get<std::string>();const auto peer=Peer(row.at("peer").get<std::string>());
            (void)FriendRecord(peer,name);
            next.friends.push_back({peer,std::move(name),row.at("online").get<bool>()});}
          for(const auto& row:invites){auto id=row.at("id").get<std::string>();auto from=row.at("from").get<std::string>();
            if(id.size()!=16||from.empty()||from.size()>15)throw std::runtime_error("Invalid invitation");next.invites.push_back({std::move(id),std::move(from)});}
        }
        std::optional<lan::Room> accepted;
        uint64_t inviter=0;
        if(reply.contains("accepted")){
          if(request.value("action","")!="accept")throw std::runtime_error("Unexpected accepted invitation");
          const auto& row=reply.at("accepted");accepted=lan::DecodeRoom(Unhex(row.at("room").get<std::string>()));
          inviter=Peer(row.at("from").get<std::string>());
          if(!accepted||!accepted->id||accepted->state||accepted->members>=accepted->capacity)throw std::runtime_error("Invalid invited room");
          (void)Peer(Hex(accepted->host)); // Validate the host's MotoGP namespace independently of the inviter.
          accepted->endpoint=VirtualEndpoint(accepted->host);
        }
        const auto room=provider_?provider_():std::nullopt;
        {
          std::lock_guard lock(mutex_);if(stopping_)return;
          SyncRoom(room);
          next.answering=std::move(snapshot_.answering);
          if(request.contains("id"))next.answering.erase(request.at("id").get<std::string>());
          std::erase_if(next.answering,[&](const auto& id){
            return std::none_of(next.invites.begin(),next.invites.end(),[&](const auto& invite){return invite.id==id;});
          });
          next.outgoing=std::move(snapshot_.outgoing);
          if(sending&&(!provider_||SameRoom(room,request_room)))
            next.outgoing[recipient]=next.supported?SendState::Sent:SendState::Failed;
          if(!next.supported||!next.can_invite)next.outgoing.clear();
          else std::erase_if(next.outgoing,[&](const auto& entry){
            return std::none_of(next.friends.begin(),next.friends.end(),[&](const auto& f){return f.peer==entry.first&&f.online;});
          });
          snapshot_=std::move(next);if(accepted){accepted_=std::move(accepted);accepted_inviter_=inviter;}
        }
        if(accepted)kernel_->BroadcastNotification(0x02000002,0); // XN_LIVE_INVITE_ACCEPTED, primary user.
      }catch(const std::exception&){
        const auto room=provider_?provider_():std::nullopt;
        std::lock_guard lock(mutex_);if(stopping_)return;
        SyncRoom(room);
        if(request.contains("id"))snapshot_.answering.erase(request.at("id").get<std::string>());
        if(sending&&(!provider_||SameRoom(room,request_room)))snapshot_.outgoing[recipient]=SendState::Failed;
        snapshot_.loaded=true;snapshot_.action=request.value("action","");snapshot_.error="Friends/invites service unavailable";
      }
    }
  }
  rex::system::KernelState* kernel_;
  std::shared_ptr<Client> client_;
  RoomProvider provider_;
  std::mutex mutex_;
  std::condition_variable changed_;
  bool stopping_=false;
  Snapshot snapshot_;
  std::optional<lan::Room> accepted_;
  uint64_t accepted_inviter_=0;
  std::optional<lan::Room> feedback_room_;
  std::deque<Action> queue_;
  std::thread worker_;
};
std::atomic<std::shared_ptr<State>> current;
std::atomic<bool> open_requested{false};
}
void Start(rex::system::KernelState* kernel,std::shared_ptr<Client> client,RoomProvider provider){Stop();if(kernel&&client)current.store(std::make_shared<State>(kernel,std::move(client),std::move(provider)));}
void Stop(){open_requested=false;if(auto state=current.exchange({}))state->Stop();}
Snapshot Read(){if(auto state=current.load())return state->Read();return {};}
bool Send(uint64_t peer){if(auto state=current.load())return state->Queue({{"action","send"},{"to",Hex(peer)}});return false;}
bool Answer(const std::string& id,bool accept){if(auto state=current.load())return state->Queue({{"action",accept?"accept":"decline"},{"id",id}});return false;}
bool AdoptInvitation(const lan::Room& room,uint64_t inviter){if(auto state=current.load())return state->Adopt(room,inviter);return false;}
std::optional<lan::Room> AcceptedRoom(){if(auto state=current.load())return state->Room();return {};}
std::optional<std::array<uint8_t,84>> AcceptedInfo(){if(auto state=current.load())return state->Info();return {};}
void RequestOpen(){open_requested=true;}
bool ConsumeOpenRequest(){return open_requested.exchange(false);}
}
