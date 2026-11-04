#pragma once
#include <string>
#include <string_view>

namespace motogp::discord {
// Discord's registered-game protocol starts this command; the invitation
// secret arrives later over ACTIVITY_JOIN, never as a command-line argument.
inline std::wstring LaunchCommand(std::wstring_view launcher){
  if(launcher.empty()||launcher.find_first_of(L"\"\r\n")!=std::wstring_view::npos||launcher.find(L'\0')!=std::wstring_view::npos)return {};
  return L"\""+std::wstring(launcher)+L"\" --discord-invite";
}
bool RegisterLaunchCommand();
}
