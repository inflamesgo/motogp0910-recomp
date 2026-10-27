#pragma once
#include "client.h"
#include <array>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace rex::system { class KernelState; }
namespace motogp::online::social {
struct Friend {uint64_t peer=0;std::string name;bool online=false;};
struct Invite {std::string id,from;};
enum class SendState { Pending, Sent, Failed };
struct Snapshot {
  bool loaded=false,supported=false,can_invite=false;
  std::vector<Friend> friends;
  std::vector<Invite> invites;
  std::set<std::string> answering;
  // Local presentation feedback only; Sent means acknowledged, not accepted.
  std::map<uint64_t,SendState> outgoing;
  std::string error,action;
};
using RoomProvider=std::function<std::optional<lan::Room>()>;
void Start(rex::system::KernelState*,std::shared_ptr<Client>,RoomProvider={});
void Stop();
Snapshot Read();
bool Send(uint64_t peer);
bool Answer(const std::string& id,bool accept);
// Authenticated external invitation resolvers reuse the exact native accepted
// invitation path; this never accepts a room directly from a Discord payload.
bool AdoptInvitation(const lan::Room& room,uint64_t inviter);
std::optional<lan::Room> AcceptedRoom();
std::optional<std::array<uint8_t,84>> AcceptedInfo();
void RequestOpen();
bool ConsumeOpenRequest();
}
