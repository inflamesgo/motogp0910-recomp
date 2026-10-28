#include "social_guest.h"
#include "social.h"
#include "social_codec.h"
#include "lan/guest_bridge.h"
#include <rex/system/xenumerator.h>
#include <algorithm>
#include <cstring>
#include <map>
#include <mutex>

namespace motogp::online::social {
namespace {
constexpr uint32_t Error(uint32_t value){return 0x80070000u|value;}
std::mutex enumerator_mutex;
std::map<const rex::system::XEnumerator*,uint32_t> enumerators;
bool Writable(rex::system::KernelState* kernel,uint32_t address,size_t size) {
  if(!address||uint64_t(address)+size>0x100000000ull)return false;
  const auto end=uint64_t(address)+size;
  for(uint64_t cursor=address;cursor<end;) {
    auto* heap=kernel->memory()->LookupHeap(static_cast<uint32_t>(cursor));
    rex::memory::HeapAllocationInfo info{};
    if(!heap||!heap->QueryRegionInfo(static_cast<uint32_t>(cursor),&info)||
        !(info.state&rex::memory::kMemoryAllocationCommit)||!(info.protect&rex::memory::kMemoryProtectWrite))return false;
    const uint64_t next=uint64_t(info.base_address)+info.region_size;if(next<=cursor)return false;cursor=next;
  }
  return true;
}
class FriendsEnumerator final:public rex::system::XEnumerator {
 public:
  FriendsEnumerator(lan::GuestBridge& bridge,uint32_t amount,std::vector<Friend> friends)
      :XEnumerator(bridge.Kernel(),amount,196),kernel_(bridge.Kernel()),friends_(std::move(friends)),size_(amount*196){
    std::lock_guard lock(enumerator_mutex);enumerators[this]=size_;
  }
  ~FriendsEnumerator(){std::lock_guard lock(enumerator_mutex);enumerators.erase(this);}
  uint32_t WriteItems(uint32_t output,uint8_t* data,uint32_t* written)override {
    std::lock_guard lock(mutex_);if(written)*written=0;if(done_)return 18;
    if(!Writable(kernel_,output,size_))return 87; // Recheck deferred enumerations.
    std::memset(data,0,size_);
    for(size_t i=0;i<friends_.size();++i){
      const auto row=FriendRecord(friends_[i].peer,friends_[i].name);
      std::copy(row.begin(),row.end(),data+i*196);
    }
    done_=true;if(written)*written=static_cast<uint32_t>(friends_.size());return friends_.empty()?18:0;
  }
 private:
  rex::system::KernelState* kernel_;
  std::vector<Friend> friends_;
  uint32_t size_;
  bool done_=false;
  std::mutex mutex_;
};
}
std::optional<uint32_t> FriendEnumeratorSize(lan::GuestBridge& bridge,uint32_t handle){
  const auto object=bridge.Kernel()->object_table()->LookupObject<rex::system::XEnumerator>(handle);
  std::lock_guard lock(enumerator_mutex);
  const auto found=enumerators.find(object.get());return found==enumerators.end()?std::nullopt:std::optional(found->second);
}
std::optional<uint32_t> GuestMessage(lan::GuestBridge& bridge,uint32_t message,uint32_t arguments){
  if(message!=0x58020&&message!=0x58023)return {};
  const uint32_t count=message==0x58020?5:2;
  if(!bridge.Valid(arguments,count*16))return Error(87);
  auto* base=bridge.Kernel()->memory()->virtual_membase();
  auto arg=[&](unsigned i)->uint32_t {
    const auto value=rex::memory::load_and_swap<uint64_t>(base+arguments+i*16+8);
    return ArgumentPointer(value);
  };
  auto value=[&](unsigned i)->std::optional<uint32_t> {
    const auto pointer=arg(i);if(!bridge.Valid(pointer,4))return {};
    return rex::memory::load_and_swap<uint32_t>(base+pointer);
  };
  const auto user=value(0);if(!user||*user)return Error(87);
  if(message==0x58023){
    const auto bytes=rex::memory::load_and_swap<uint32_t>(base+arguments+16);
    // 825DA268 passes the output buffer directly to 825DA0D8, whose argument
    // size is always 4. Unlike FIFA's wrapper, this is not a pointer-to-pointer.
    const uint32_t output=arg(1);
    if(bytes!=4&&bytes!=84)return Error(87);
    if(!bridge.Valid(output,84,true))return Error(87);
    const auto info=AcceptedInfo();if(!info)return Error(1168);
    std::copy(info->begin(),info->end(),base+output);return 0;
  }
  const auto size_out=arg(3),handle_out=arg(4);
  if(!bridge.Valid(size_out,4,true)||!bridge.Valid(handle_out,4,true)||size_out==handle_out)return Error(87);
  rex::memory::store_and_swap<uint32_t>(base+size_out,0);
  rex::memory::store_and_swap<uint32_t>(base+handle_out,0);
  const auto first=value(1),amount=value(2);
  if(!first||!amount||!*amount||*amount>100||*first>100)return Error(87);
  const auto snapshot=Read();if(!snapshot.supported||!snapshot.error.empty())return Error(1231);
  const size_t begin=std::min<size_t>(*first,snapshot.friends.size());
  const size_t end=std::min<size_t>(begin+*amount,snapshot.friends.size());
  std::vector<Friend> friends(snapshot.friends.begin()+begin,snapshot.friends.begin()+end);
  auto enumerator=rex::system::make_object<FriendsEnumerator>(bridge,*amount,std::move(friends));
  const auto result=enumerator->Initialize(0,0xFC,0x58021,0x58022,0);
  if(result&0x80000000u){enumerator->ReleaseHandle();return result;}
  rex::memory::store_and_swap<uint32_t>(base+size_out,*amount*196);
  rex::memory::store_and_swap<uint32_t>(base+handle_out,enumerator->handle());return 0;
}
}
