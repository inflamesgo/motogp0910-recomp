#pragma once
#include <string_view>

namespace motogp::discord {
inline constexpr std::string_view kJoinPrefix="mg0910.v1.";
inline bool ValidJoinSecret(std::string_view secret){
  if(secret.size()!=kJoinPrefix.size()+32||!secret.starts_with(kJoinPrefix))return false;
  for(unsigned char c:secret.substr(kJoinPrefix.size()))
    if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-'))return false;
  return true;
}
inline bool ValidPartyId(std::string_view party){
  if(party.size()!=40||!party.starts_with("mg0910:")||party[23]!=':')return false;
  for(size_t i=7;i<party.size();++i)if(i!=23){const char c=party[i];if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')))return false;}
  return true;
}
}
