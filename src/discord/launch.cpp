#include "launch.h"
#include "activity.h"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <array>
#include <filesystem>
#endif

namespace motogp::discord {
bool RegisterLaunchCommand(){
#ifdef _WIN32
  std::array<wchar_t,32768> path{};
  const auto length=GetModuleFileNameW(nullptr,path.data(),static_cast<DWORD>(path.size()));
  if(!length||length>=path.size())return false;
  const auto executable=std::filesystem::path(std::wstring_view(path.data(),length));
  std::filesystem::path launcher;
  for(const auto& directory:{executable.parent_path(),executable.parent_path().parent_path()}){
    auto candidate=directory/L"MotoGP09-10Launcher.exe";std::error_code error;
    if(std::filesystem::is_regular_file(candidate,error)){launcher=std::move(candidate);break;}
  }
  const auto command=LaunchCommand(launcher.native());if(command.empty())return false;
  std::wstring protocol=L"discord-";for(const char c:std::string_view(kApplicationId))protocol+=wchar_t(c);
  struct Key{HKEY value=nullptr;~Key(){if(value)RegCloseKey(value);}} root,open;
  const auto name=L"Software\\Classes\\"+protocol;
  if(RegCreateKeyExW(HKEY_CURRENT_USER,name.c_str(),0,nullptr,0,KEY_WRITE,nullptr,&root.value,nullptr)!=ERROR_SUCCESS)return false;
  const auto write=[](HKEY key,const wchar_t* name,const std::wstring& value){return RegSetValueExW(key,name,0,REG_SZ,reinterpret_cast<const BYTE*>(value.c_str()),static_cast<DWORD>((value.size()+1)*sizeof(wchar_t)))==ERROR_SUCCESS;};
  if(!write(root.value,nullptr,L"URL:MotoGP 09/10 Discord invitation")||!write(root.value,L"URL Protocol",L""))return false;
  if(RegCreateKeyExW(root.value,L"shell\\open\\command",0,nullptr,0,KEY_WRITE,nullptr,&open.value,nullptr)!=ERROR_SUCCESS)return false;
  return write(open.value,nullptr,command);
#else
  return false;
#endif
}
}
