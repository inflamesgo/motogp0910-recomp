#pragma once
#include "client.h"
#include <array>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace rex::system { class KernelState; }
namespace motogp::online::social {
struct Friend {uint64_t peer=0;std::string name;bool online=false;};
struct Invite {std::string id,from;};
struct Snapshot {
  bool loaded=false,supported=false,can_invite=false;
  std::vector<Friend> friends;
  std::vector<Invite> invites;
  std::string error,action;
};
void Start(rex::system::KernelState*,std::shared_ptr<Client>);
void Stop();
Snapshot Read();
bool Send(uint64_t peer);
bool Answer(const std::string& id,bool accept);
std::optional<lan::Room> AcceptedRoom();
std::optional<std::array<uint8_t,84>> AcceptedInfo();
void RequestOpen();
bool ConsumeOpenRequest();
}
