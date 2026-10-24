#include "../src/online/social_codec.h"
#include "../src/online/client.h"
#include <cstring>
#include <iostream>

int main(int argc,char** argv){try{
  using namespace motogp::online::social;
  auto check=[](bool ok,const char* reason){if(!ok)throw std::runtime_error(reason);};
  auto u32=[](const uint8_t* p){return uint32_t(p[0])<<24|uint32_t(p[1])<<16|uint32_t(p[2])<<8|p[3];};
  auto u64=[&](const uint8_t* p){return uint64_t(u32(p))<<32|u32(p+4);};
  check(ArgumentPointer(0xE0001234ull)==0xE0001234&&ArgumentPointer(0xFFFFFFFFE0001234ull)==0xE0001234,"Native argument cells accept both canonical pointer extensions");
  check(ArgumentPointer(0x12345678E0001234ull)==0,"Malformed argument-cell upper bits must be rejected");
  motogp::lan::Room room;room.id=0x1020304050607080ull;room.host=0x0009000000010203ull;
  for(unsigned i=0;i<16;++i)room.key[i]=uint8_t(i+1);
  const auto info=InviteInfo(0xE000000000000001ull,0x0009000000040506ull,room);
  check(info.size()==84&&u64(info.data())==0xE000000000000001ull,"Keep the primary local user's profile XUID");
  check(u64(info.data()+8)==0x0009000000040506ull&&u32(info.data()+16)==0x434307F0,"Inviter and title are big endian; inviter need not be host");
  check(u64(info.data()+20)==room.id&&u64(info.data()+44)==room.host,"Native session and host identities keep their documented offsets");
  check(u32(info.data()+28)==0x0A010203&&u32(info.data()+32)==0x0A010203&&info[36]==0x78&&info[37]==0x5A,"Use the authenticated relay's virtual host endpoint");
  check(std::equal(room.key.begin(),room.key.end(),info.begin()+64)&&u32(info.data()+80)==1,"Preserve the exact native room key and invite flag");
  const auto row=FriendRecord(0x00090000000000F1ull,"Arkadaş");
  check(row.size()==196&&u64(row.data())==0x00090000000000F1ull&&std::strcmp(reinterpret_cast<const char*>(row.data()+8),"Arkadaş")==0,"Native friend enumeration uses MotoGP identity, full UTF-8 and 196-byte stride");
  check(std::all_of(row.begin()+24,row.end(),[](auto byte){return byte==0;}),"Unused native friend fields stay initialized");
  for(const auto name:{"","1234567890123456","bad\nname"}){bool rejected=false;try{FriendRecord(1,name);}catch(const std::exception&){rejected=true;}check(rejected,"Malformed native friend names must be rejected");}
  if(argc==3) {
    const std::filesystem::path profile=argv[2];std::filesystem::create_directories(profile);
    motogp::online::Client client(argv[1],"Social QA",profile/"social.profile");
    const std::string poll="{\"action\":\"poll\"}";
    const auto request=std::span(reinterpret_cast<const uint8_t*>(poll.data()),poll.size());
    for(int i=0;i<2;++i) {
      const auto reply=client.Social(request);
      check(std::string(reply.begin(),reply.end())=="{\"supported\":false}","Missing optional social routes report unsupported instead of failing");
    }
    for(int i=0;i<2;++i) {
      bool rejected=false;try{client.Social(request);}catch(const std::exception&){rejected=true;}
      check(rejected,"Social authentication and server failures are not hidden as missing support");
    }
    const auto reply=client.Social(request);
    check(std::string(reply.begin(),reply.end())=="{\"supported\":true}","Supported social responses pass through unchanged");
    bool rejected=false;try{client.Read(1,1,1);}catch(const std::exception&){rejected=true;}
    check(rejected,"A missing required statistics route remains an error");
    std::cout<<"Real HTTP optional-social capability checks passed\n";
  } else check(argc==1,"Usage: online_social_probe [LOCAL_SERVER_URL ISOLATED_PROFILE_DIRECTORY]");
  std::cout<<"Online social native invitation/friend ABI checks passed\n";
  return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
