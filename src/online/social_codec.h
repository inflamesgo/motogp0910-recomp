#pragma once
#include "relay.h"
#include <algorithm>
#include <array>
#include <string_view>
#include <stdexcept>

namespace motogp::online::social {
// MotoGP's 825DA0D8 stores 32-bit guest pointers with extsw/std. Both the
// zero-extended and sign-extended representations are valid argument cells.
inline uint32_t ArgumentPointer(uint64_t raw) {
  const auto low=static_cast<uint32_t>(raw);
  return raw==uint64_t(low)||raw==uint64_t(int64_t(int32_t(low)))?low:0;
}
// Xbox's 84-byte XINVITE_INFO: local user, inviter, title, 60-byte host
// XSESSION_INFO, then fFromGameInvite. The inviter may be a lobby guest.
inline std::array<uint8_t,84> InviteInfo(uint64_t local,uint64_t inviter,const lan::Room& room) {
  std::array<uint8_t,84> out{};
  auto put32=[&](size_t at,uint32_t value){for(int i=3;i>=0;--i){out[at+i]=uint8_t(value);value>>=8;}};
  auto put64=[&](size_t at,uint64_t value){for(int i=7;i>=0;--i){out[at+i]=uint8_t(value);value>>=8;}};
  put64(0,local);put64(8,inviter);put32(16,lan::kTitle);put64(20,room.id);
  const auto endpoint=VirtualEndpoint(room.host);
  put32(28,endpoint.ip);put32(32,endpoint.ip);
  out[36]=uint8_t(endpoint.port>>8);out[37]=uint8_t(endpoint.port);out[38]=2;
  for(unsigned i=0;i<5;++i)out[39+i]=uint8_t(room.host>>(i*8));
  put64(44,room.host);
  std::copy(room.key.begin(),room.key.end(),out.begin()+64);put32(80,1);
  return out;
}
inline std::array<uint8_t,196> FriendRecord(uint64_t peer,std::string_view name) {
  if(name.empty()||name.size()>15||std::any_of(name.begin(),name.end(),[](unsigned char c){return c<32||c==127;}))throw std::runtime_error("Invalid native friend name");
  std::array<uint8_t,196> row{};
  for(int i=7;i>=0;--i){row[i]=uint8_t(peer);peer>>=8;}
  std::copy(name.begin(),name.end(),row.begin()+8);return row;
}
}
