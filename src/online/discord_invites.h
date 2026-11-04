#pragma once
#include "client.h"
#include <functional>
#include <memory>
#include <optional>
#include <string>

namespace motogp::online::discord_invites {
using RoomProvider=std::function<std::optional<lan::Room>()>;
struct Publication {std::string party_id,secret;};
enum class Outcome {None,Pending,Accepted,AlreadyJoined,OnlineRequired,Unsupported,Unavailable};
struct Notice {Outcome outcome=Outcome::None;uint64_t serial=0;};
void Start(std::shared_ptr<Client> client,RoomProvider provider,bool enabled);
void Stop();
Publication Read(const lan::Room& room);
bool Join(const std::string& secret);
Notice ReadNotice();
}
