#pragma once
#include <toml++/toml.hpp>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace motogp::config {
std::optional<std::string> Read(const std::filesystem::path& path);
toml::table Parse(std::string_view bytes,const std::filesystem::path& path);
toml::table Load(const std::filesystem::path& path);
const toml::node* Find(const toml::table& table,std::string_view key,std::string prefix={});
std::string Serialize(const toml::table& table,const std::filesystem::path& path);
// One-time migration. Existing JSON always wins; legacy inputs are backed up.
std::filesystem::path Ensure(const std::filesystem::path& root);

// The SDK reads a temporary TOML adapter; settings are saved to config.json.
// Keep SDK type validation and environment/CLI precedence intact.
class SdkBridge {
 public:
  ~SdkBridge(){Clear();}
  SdkBridge()=default;
  SdkBridge(const SdkBridge&)=delete;
  SdkBridge& operator=(const SdkBridge&)=delete;
  std::filesystem::path Prepare(toml::table table);
  void Clear()noexcept;
 private:
  std::filesystem::path path_;
};
}
