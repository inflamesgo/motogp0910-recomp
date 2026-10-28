// NOBK version/download contract adapted from ChunchunQ/fifa09-recomp
// launcher/Updater.cs (MIT). See third_party/fifa09-launcher-LICENSE.txt.
#include "updates.h"
#include "update_protocol.h"
#include "../../third_party/nlohmann/json.hpp"
#include <winhttp.h>
#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <stdexcept>
#include <system_error>
#include <utility>

namespace launcher::updates {
namespace {
constexpr size_t kManifestLimit = 128 * 1024;
constexpr size_t kGitHubLimit = 1024 * 1024;
constexpr size_t kGitHubNotesLimit = 64 * 1024;
constexpr uint64_t kDownloadLimit = 512ull * 1024 * 1024;
constexpr wchar_t kStagingPrefix[] = L"MotoGP0910-update-";
using Clock = std::chrono::steady_clock;

struct Cancelled : std::runtime_error { Cancelled() : std::runtime_error("Update cancelled") {} };
struct InternetHandle {
  HINTERNET value{};
  ~InternetHandle() { if (value) WinHttpCloseHandle(value); }
  InternetHandle() = default;
  InternetHandle(const InternetHandle&) = delete;
  InternetHandle& operator=(const InternetHandle&) = delete;
};

std::array<uint32_t, 3> Version(std::string_view text) {
  // Match System.Version + Normalize: 2-4 numeric components; an absent build
  // becomes zero and revision does not participate in update comparison.
  std::array<uint32_t, 4> parts{};
  size_t count = 0;
  for (;;) {
    if (count == parts.size()) throw std::runtime_error("Update version has too many components");
    const auto dot = text.find('.');
    auto part = text.substr(0, dot);
    const auto first = part.find_first_not_of(" \t\r\n");
    if (first == std::string_view::npos) throw std::runtime_error("Invalid numeric update version");
    part = part.substr(first, part.find_last_not_of(" \t\r\n") - first + 1);
    if (part.front() == '+') part.remove_prefix(1);
    if (part.empty()) throw std::runtime_error("Invalid numeric update version");
    auto [end, error] = std::from_chars(part.data(), part.data() + part.size(), parts[count]);
    if (error != std::errc{} || end != part.data() + part.size() || parts[count] > 0x7fffffff)
      throw std::runtime_error("Invalid numeric update version");
    ++count;
    if (dot == std::string_view::npos) break;
    text.remove_prefix(dot + 1);
  }
  if (count < 2) throw std::runtime_error("Update version has too few components");
  return {parts[0], parts[1], parts[2]};
}

std::wstring CurrentText(std::string_view text) {
  const auto version = Version(text);
  return std::to_wstring(version[0]) + L"." + std::to_wstring(version[1]) + L"." + std::to_wstring(version[2]);
}

struct Url { std::wstring host, target, path; INTERNET_PORT port{}; };
Url HttpsUrl(std::wstring_view text) {
  if (text.empty() || text.size() > 8192 || text.find(L'\0') != std::wstring_view::npos ||
      text.find(L'\r') != std::wstring_view::npos || text.find(L'\n') != std::wstring_view::npos)
    throw std::runtime_error("Invalid update URL");
  URL_COMPONENTS parts{}; parts.dwStructSize = sizeof(parts);
  parts.dwHostNameLength = parts.dwUrlPathLength = parts.dwExtraInfoLength =
      parts.dwUserNameLength = parts.dwPasswordLength = static_cast<DWORD>(-1);
  if (!WinHttpCrackUrl(text.data(), static_cast<DWORD>(text.size()), 0, &parts) ||
      parts.nScheme != INTERNET_SCHEME_HTTPS || !parts.dwHostNameLength ||
      parts.dwUserNameLength || parts.dwPasswordLength)
    throw std::runtime_error("Updates require HTTPS without embedded credentials");
  Url url;
  url.host.assign(parts.lpszHostName, parts.dwHostNameLength);
  if (parts.dwUrlPathLength) url.path.assign(parts.lpszUrlPath, parts.dwUrlPathLength);
  url.target = url.path.empty() ? L"/" : url.path;
  if (parts.dwExtraInfoLength) {
    std::wstring_view extra(parts.lpszExtraInfo, parts.dwExtraInfoLength);
    if (extra.find(L'#') != std::wstring_view::npos) throw std::runtime_error("Update URL must not contain a fragment");
    url.target += extra;
  }
  url.port = parts.nPort;
  return url;
}

std::string Digest(std::string_view value) {
  if (value.size() != 64) throw std::runtime_error("Update SHA-256 must contain 64 hexadecimal characters");
  std::string result(value);
  for (auto& c : result) {
    if (c >= 'A' && c <= 'F') c = static_cast<char>(c - 'A' + 'a');
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')))
      throw std::runtime_error("Invalid update SHA-256");
  }
  return result;
}

void ValidatePackage(const Release& release) {
  if (!release.CanInstall()) throw std::runtime_error("Update does not provide an HTTPS ZIP and checksum");
  Version(release.version);
  Digest(release.sha256);
  HttpsUrl(detail::WideText(release.url));
}

class Request {
  InternetHandle session_, connection_, request_;
  const std::atomic_bool* cancelled_;
  Clock::time_point deadline_;
 public:
  uint64_t length = 0;
  void Check() const {
    if (cancelled_ && cancelled_->load()) throw Cancelled();
    if (Clock::now() > deadline_) throw std::runtime_error("Update request timed out");
  }
  Request(std::wstring_view address, const Configuration& configuration, bool package, const std::atomic_bool* cancelled)
      : cancelled_(cancelled), deadline_(Clock::now() + (package ? std::chrono::seconds(600) : std::chrono::seconds(8))) {
    Check();
    const auto url = HttpsUrl(address);
    const auto agent = L"NOBK-Launcher/" + CurrentText(configuration.current_version);
    session_.value = WinHttpOpen(agent.c_str(), WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session_.value) session_.value = WinHttpOpen(agent.c_str(), WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session_.value || !WinHttpSetTimeouts(session_.value, 2000, 3000, 3000, 3000))
      throw std::runtime_error("Could not initialize update connection");
    connection_.value = WinHttpConnect(session_.value, url.host.c_str(), url.port, 0);
    if (!connection_.value) throw std::runtime_error("Could not connect to update service");
    request_.value = WinHttpOpenRequest(connection_.value, L"GET", url.target.c_str(), nullptr,
        WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
    DWORD redirect = WINHTTP_OPTION_REDIRECT_POLICY_DISALLOW_HTTPS_TO_HTTP;
    if (!request_.value || !WinHttpSetOption(request_.value, WINHTTP_OPTION_REDIRECT_POLICY, &redirect, sizeof(redirect)))
      throw std::runtime_error("Could not create update request");
    const auto* headers = package ? L"Accept: application/zip\r\n" : L"Accept: application/json\r\n";
    if (!WinHttpSendRequest(request_.value, headers, static_cast<DWORD>(-1), WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
        !WinHttpReceiveResponse(request_.value, nullptr)) {
      Check();
      throw std::runtime_error("Could not reach update service");
    }
    Check();
    DWORD status{}, size = sizeof(status);
    if (!WinHttpQueryHeaders(request_.value, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
          nullptr, &status, &size, nullptr) || status != 200)
      throw std::runtime_error("Update service did not return HTTP 200");
    DWORD content_length{}; size = sizeof(content_length);
    if (WinHttpQueryHeaders(request_.value, WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER,
          nullptr, &content_length, &size, nullptr)) length = content_length;
  }
  DWORD Read(std::array<char, 32768>& buffer) {
    Check(); DWORD count{};
    if (!WinHttpReadData(request_.value, buffer.data(), static_cast<DWORD>(buffer.size()), &count)) {
      Check(); throw std::runtime_error("Update download was interrupted");
    }
    Check(); return count;
  }
};

std::string ReadMetadata(std::wstring_view address, const Configuration& configuration,
                         const std::atomic_bool* cancelled, size_t limit = kManifestLimit) {
  Request request(address, configuration, false, cancelled);
  if (request.length > limit) throw std::runtime_error("Update metadata exceeds the size limit");
  std::string body; std::array<char, 32768> buffer{};
  while (const auto count = request.Read(buffer)) {
    if (body.size() + count > limit) throw std::runtime_error("Update metadata exceeds the size limit");
    body.append(buffer.data(), count);
  }
  return body;
}

std::optional<Release> GitHubRelease(const Configuration& configuration, const std::atomic_bool* cancelled) {
  const auto body = ReadMetadata(configuration.github_releases_url, configuration, cancelled, kGitHubLimit);
  const auto releases = nlohmann::json::parse(body, nullptr, false);
  if (!releases.is_array() || releases.size() > 20) throw std::runtime_error("Invalid GitHub Releases response");
  std::optional<Release> best;
  std::string manifest_url;
  for (const auto& item : releases) {
    if (!item.is_object() || !item.contains("draft") || !item["draft"].is_boolean() ||
        !item.contains("prerelease") || !item["prerelease"].is_boolean() ||
        item["draft"].get<bool>() || item["prerelease"].get<bool>()) continue;
    if (!item.contains("tag_name") || !item["tag_name"].is_string() ||
        !item.contains("assets") || !item["assets"].is_array() || item["assets"].size() > 64) continue;
    auto version = item["tag_name"].get<std::string>();
    if (version.size() > 65) continue;
    if (version.starts_with('v') || version.starts_with('V')) version.erase(0, 1);
    try {
      if (!IsNewer(version, configuration.current_version) || (best && !IsNewer(version, best->version))) continue;
    } catch (const std::exception&) { continue; }  // Not a numeric stable release tag.
    Release candidate; candidate.version = version; candidate.publisher = "GitHub";
    std::string attached_manifest;
    size_t packages = 0, manifests = 0;
    for (const auto& asset : item["assets"]) {
      if (!asset.is_object() || !asset.contains("name") || !asset["name"].is_string() ||
          !asset.contains("browser_download_url") || !asset["browser_download_url"].is_string()) continue;
      const auto name = asset["name"].get<std::string>();
      const auto url = asset["browser_download_url"].get<std::string>();
      if (url.size() > 8192) continue;
      if (name == "version.json") { attached_manifest = url; ++manifests; }
      if (!name.starts_with("MotoGP0910-Windows-x64-") || !name.ends_with(".zip")) continue;
      ++packages;
      candidate.url = url;
      if (asset.contains("digest") && asset["digest"].is_string()) {
        const auto digest = asset["digest"].get<std::string>();
        if (digest.starts_with("sha256:")) candidate.sha256 = digest.substr(7);
      }
    }
    // Never guess between multiple Windows packages or use GitHub's source ZIP.
    if (packages != 1 || manifests > 1) continue;
    if (item.contains("body") && item["body"].is_string()) {
      auto notes = item["body"].get<std::string>();
      if (notes.size() > kGitHubNotesLimit) {
        auto length = kGitHubNotesLimit;
        while (length && (static_cast<unsigned char>(notes[length]) & 0xc0) == 0x80) --length;
        notes.resize(length); notes += "\n…";
      }
      // All launcher languages show the publisher's actual GitHub description.
      // Manifest notes are a fallback only for an empty/missing description.
      if (notes.find_first_not_of(" \t\r\n") != std::string::npos) candidate.notes["en"] = std::move(notes);
    }
    best = std::move(candidate); manifest_url = std::move(attached_manifest);
  }
  if (!best) return {};
  // GitHub's asset digest is authoritative when present. Older uploads can
  // attach our existing version.json schema alongside the full Windows ZIP.
  if (!manifest_url.empty()) {
    auto manifest = ParseManifest(ReadMetadata(detail::WideText(manifest_url), configuration, cancelled));
    if (Version(manifest.version) != Version(best->version) || manifest.url != best->url)
      throw std::runtime_error("GitHub manifest does not match its release asset");
    if (!best->sha256.empty() && Digest(best->sha256) != Digest(manifest.sha256))
      throw std::runtime_error("GitHub manifest and asset checksums disagree");
    best->sha256 = manifest.sha256;
    if (best->notes.empty() && !manifest.notes.empty()) best->notes = std::move(manifest.notes);
  }
  ValidatePackage(*best);
  return best;
}

std::filesystem::path TemporaryDirectory() {
  std::array<wchar_t, 32768> buffer{};
  const auto count = GetTempPathW(static_cast<DWORD>(buffer.size()), buffer.data());
  if (!count || count >= buffer.size()) throw std::runtime_error("Cannot locate update staging directory");
  for (int attempt = 0; attempt < 8; ++attempt) {
    std::array<unsigned char, 16> bytes{};
    if (BCryptGenRandom(nullptr, bytes.data(), static_cast<ULONG>(bytes.size()), BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0)
      throw std::runtime_error("Could not create unique update staging name");
    std::wstring name(kStagingPrefix);
    for (auto byte : bytes) { name += L"0123456789abcdef"[byte >> 4]; name += L"0123456789abcdef"[byte & 15]; }
    auto directory = std::filesystem::path(buffer.data()) / name;
    if (std::filesystem::create_directory(directory)) return directory;
  }
  throw std::runtime_error("Could not create update staging directory");
}
}  // namespace

std::string Release::NotesFor(std::string_view language) const {
  auto found = notes.find(std::string(language));
  if (found == notes.end()) found = notes.find("en");
  return found == notes.end() ? std::string{} : found->second;
}

bool Release::CanInstall() const {
  auto equal_ascii = [](std::string_view a, std::string_view b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
      auto c = a[i]; if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
      if (c != b[i]) return false;
    }
    return true;
  };
  return url.size() >= 12 && equal_ascii(std::string_view(url).substr(0, 8), "https://") &&
      equal_ascii(std::string_view(url).substr(url.size() - 4), ".zip") && sha256.size() == 64;
}

bool Configured(const Configuration& configuration) {
  return (!configuration.manifest_url.empty() || !configuration.github_releases_url.empty()) && !configuration.current_version.empty() &&
      configuration.current_version != "0.0.0";
}

bool IsNewer(std::string_view candidate, std::string_view installed) { return Version(candidate) > Version(installed); }

std::wstring VersionRequestUrl(const Configuration& configuration) {
  if (!Configured(configuration) || configuration.manifest_url.empty()) return {};
  HttpsUrl(configuration.manifest_url);
  auto address = configuration.manifest_url;
  address += address.find(L'?') == std::wstring::npos ? L"?v=" :
      address.ends_with(L'?') || address.ends_with(L'&') ? L"v=" : L"&v=";
  return address + CurrentText(configuration.current_version);
}

Release ParseManifest(std::string_view json) {
  if (json.size() > kManifestLimit) throw std::runtime_error("Update manifest exceeds the size limit");
  const auto document = nlohmann::json::parse(json, nullptr, false);
  if (!document.is_object()) throw std::runtime_error("Invalid update manifest JSON");
  Release result;
  auto text = [&](const char* key, size_t limit) {
    const auto found = document.find(key);
    if (found == document.end() || found->is_null()) return std::string{};
    if (!found->is_string()) throw std::runtime_error("Invalid update manifest field type");
    auto value = found->get<std::string>();
    if (value.size() > limit) throw std::runtime_error("Invalid update manifest field length");
    return value;
  };
  result.version = text("version", 64); result.url = text("url", 8192); result.sha256 = text("sha256", 128);
  if (const auto found = document.find("notes"); found != document.end()) {
    if (!found->is_null()) {
      if (!found->is_object()) throw std::runtime_error("Update notes must be a language dictionary");
      for (auto note = found->begin(); note != found->end(); ++note) {
        if (!note->is_string() || note->get_ref<const std::string&>().size() > 4096 || note.key().size() > 32)
          throw std::runtime_error("Invalid update release notes");
        result.notes.emplace(note.key(), note->get<std::string>());
      }
    }
  }
  Version(result.version);
  return result;
}

CheckResult CheckForUpdate(const Configuration& configuration, const std::atomic_bool* cancelled) {
  if (!Configured(configuration)) return {};
  std::optional<Release> github, nobk;
  std::string errors;
  // Always check both sources, in this order. Otherwise simultaneous updates
  // could never be compared. One unavailable publisher cannot hide the other.
  if (!configuration.github_releases_url.empty()) try {
    github = GitHubRelease(configuration, cancelled);
  } catch (const Cancelled&) { return {CheckStatus::Cancelled, {}, {}}; }
    catch (const std::exception& error) { errors = std::string("GitHub: ") + error.what(); }
  if (!configuration.manifest_url.empty()) try {
    auto release = ParseManifest(ReadMetadata(VersionRequestUrl(configuration), configuration, cancelled));
    if (IsNewer(release.version, configuration.current_version)) {
      ValidatePackage(release); release.publisher = "NOBK Online"; nobk = std::move(release);
    }
  } catch (const Cancelled&) { return {CheckStatus::Cancelled, {}, {}}; }
    catch (const std::exception& error) { if (!errors.empty()) errors += "; "; errors += std::string("NOBK: ") + error.what(); }
  if (cancelled && cancelled->load()) return {CheckStatus::Cancelled, {}, {}};
  if (!github && !nobk) return {errors.empty() ? CheckStatus::UpToDate : CheckStatus::Failed, {}, errors};
  Release primary = github ? *github : *nobk;
  if (github && nobk) {
    const bool nobk_newer = IsNewer(nobk->version, github->version);
    primary = nobk_newer ? *nobk : *github;
    const auto& secondary = nobk_newer ? *github : *nobk;
    if (Digest(primary.sha256) != Digest(secondary.sha256))
      primary.companion = Package{secondary.version, secondary.url, secondary.sha256};
  }
  return {CheckStatus::Available, std::move(primary), errors};
}

StagedUpdate DownloadVerified(const Configuration& configuration, const Release& release,
                             const std::atomic_bool* cancelled, Progress progress) {
  if (!Configured(configuration)) throw std::runtime_error("Update service is not configured");
  ValidatePackage(release);
  if (!IsNewer(release.version, configuration.current_version)) throw std::runtime_error("Update is not newer than this build");
  StagedUpdate staged{release, TemporaryDirectory(), {}};
  try {
    staged.archive = staged.directory / L"update.zip";
    uint64_t completed = 0;
    auto download = [&](const Release& package, const std::filesystem::path& archive) {
      ValidatePackage(package);
      Request request(detail::WideText(package.url), configuration, true, cancelled);
      if (request.length > kDownloadLimit) throw std::runtime_error("Update package exceeds the size limit");
      std::ofstream file(archive, std::ios::binary);
      if (!file) throw std::runtime_error("Cannot create temporary update archive");
      std::array<char, 32768> buffer{}; uint64_t received{};
      while (const auto count = request.Read(buffer)) {
        if (received + count > kDownloadLimit) throw std::runtime_error("Update package exceeds the size limit");
        file.write(buffer.data(), count);
        if (!file) throw std::runtime_error("Could not write temporary update archive");
        received += count;
        if (progress) progress(completed + received, release.companion ? 0 : request.length);
      }
      file.close();
      if (!file || !received || (request.length && received != request.length)) throw std::runtime_error("Incomplete update package");
      request.Check();
      if (detail::FileHash(archive) != Digest(package.sha256)) throw std::runtime_error("Update package SHA-256 mismatch");
      request.Check();
      std::ifstream input(archive, std::ios::binary); std::array<unsigned char, 4> signature{};
      input.read(reinterpret_cast<char*>(signature.data()), static_cast<std::streamsize>(signature.size()));
      if (!input || signature != std::array<unsigned char, 4>{'P', 'K', 3, 4}) throw std::runtime_error("Update package is not a ZIP archive");
      completed += received;
    };
    download(release, staged.archive);
    if (release.companion) {
      const auto& companion = *release.companion;
      if (!IsNewer(companion.version, configuration.current_version) || IsNewer(companion.version, release.version))
        throw std::runtime_error("Invalid companion update version");
      staged.companion_archive = staged.directory / L"companion.zip";
      download(Release{companion.version, companion.url, companion.sha256, {}}, staged.companion_archive);
    }
    if (progress) progress(completed, completed);
    return staged;
  } catch (...) { DiscardStaged(staged); throw; }
}

void DiscardStaged(const StagedUpdate& staged) {
  if (staged.directory.empty()) return;
  // Limit cleanup to our direct, randomly named staging child of Windows TEMP.
  std::array<wchar_t, 32768> buffer{};
  const auto count = GetTempPathW(static_cast<DWORD>(buffer.size()), buffer.data());
  const auto name = staged.directory.filename().wstring();
  if (!count || count >= buffer.size() || !name.starts_with(kStagingPrefix) ||
      name.size() != std::wstring_view(kStagingPrefix).size() + 32 ||
      staged.directory.parent_path().lexically_normal() !=
          (std::filesystem::path(buffer.data()) / L"update.zip").parent_path().lexically_normal() ||
      !std::all_of(name.end() - 32, name.end(), [](wchar_t c) { return (c >= L'0' && c <= L'9') || (c >= L'a' && c <= L'f'); })) return;
  std::error_code error;
  std::filesystem::remove_all(staged.directory, error);
}
}  // namespace launcher::updates
