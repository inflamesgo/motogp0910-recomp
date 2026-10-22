#include "client.h"
#include "rpc.h"
#include <chrono>
#include <utility>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

namespace motogp::discord {
using Clock = std::chrono::steady_clock;
using namespace std::chrono_literals;
namespace {
int64_t Now() {
  return std::chrono::duration_cast<std::chrono::seconds>(
      std::chrono::system_clock::now().time_since_epoch()).count();
}
#ifdef _WIN32
class Pipe {
 public:
  ~Pipe() { Close(); }
  bool Open() {
    for (int i = 0; i < 10; ++i) {
      const auto path = L"\\\\?\\pipe\\discord-ipc-" + std::to_wstring(i);
      handle_ = CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr,
          OPEN_EXISTING, FILE_FLAG_OVERLAPPED | SECURITY_SQOS_PRESENT | SECURITY_IDENTIFICATION, nullptr);
      if (handle_ != INVALID_HANDLE_VALUE) return true;
    }
    return false;
  }
  void Close() {
    if (handle_ != INVALID_HANDLE_VALUE) CloseHandle(handle_);
    handle_ = INVALID_HANDLE_VALUE;
  }
  // Every transfer is bounded; this worker never blocks a guest/UI thread.
  bool Transfer(bool write, char* data, DWORD size) {
    OVERLAPPED op{};
    op.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!op.hEvent) return false;
    DWORD transferred = 0;
    bool ok = write ? WriteFile(handle_, data, size, &transferred, &op)
                    : ReadFile(handle_, data, size, &transferred, &op);
    if (!ok && GetLastError() == ERROR_IO_PENDING) {
      if (WaitForSingleObject(op.hEvent, 250) != WAIT_OBJECT_0) {
        CancelIoEx(handle_, &op);
        // Cancellation must finish before the stack OVERLAPPED is released.
        GetOverlappedResult(handle_, &op, &transferred, TRUE);
        ok = false;
      } else ok = GetOverlappedResult(handle_, &op, &transferred, FALSE);
    }
    CloseHandle(op.hEvent);
    return ok && transferred == size;
  }
  bool Send(uint32_t opcode, const std::string& payload) {
    auto frame = Frame(opcode, payload);
    return payload.size() <= kMaxFrame && Transfer(true, frame.data(), static_cast<DWORD>(frame.size()));
  }
  bool Receive(std::string& pending) {
    DWORD available = 0;
    if (!PeekNamedPipe(handle_, nullptr, 0, nullptr, &available, nullptr)) return false;
    if (!available) return true;
    char bytes[4096];
    const DWORD size = (std::min)(available, DWORD(sizeof(bytes)));
    if (!Transfer(false, bytes, size)) return false;
    pending.append(bytes, size);
    return pending.size() <= kMaxFrame + 8;
  }
 private:
  HANDLE handle_ = INVALID_HANDLE_VALUE;
};
#endif
}
void Client::Publish(Activity activity) {
  const std::lock_guard lock(mutex_);
  if (activity_ == activity) return;
  if (activity_.details != activity.details) started_ = Now();
  activity_ = std::move(activity);
}
void Client::Start() {
#ifdef _WIN32
  if (running_.exchange(true)) return;
  { const std::lock_guard lock(mutex_); started_ = Now(); }
  try { worker_ = std::thread([this] { Run(); }); }
  catch (...) { running_ = false; }
#endif
}
void Client::Stop() {
  running_ = false;
  wake_.notify_all();
  if (worker_.joinable()) worker_.join();
}
void Client::Run() {
#ifdef _WIN32
  uint64_t nonce = 0;
  while (running_) {
    try {
      Pipe pipe;
      if (pipe.Open() && pipe.Send(0, Json{{"v", 1}, {"client_id", kApplicationId}}.dump())) {
        bool ready = false;
        std::string pending;
        Activity sent;
        int64_t sent_started = -1;
        auto deadline = Clock::now() + 5s;
        auto next_update = Clock::now();
        bool healthy = true;
        while (running_ && healthy) {
          healthy = pipe.Receive(pending);
          while (healthy && pending.size() >= 8) {
            const auto opcode = Word(pending.data()), size = Word(pending.data() + 4);
            if (size > kMaxFrame) { healthy = false; break; }
            if (pending.size() < size + 8) break;
            const auto payload = pending.substr(8, size);
            pending.erase(0, size + 8);
            if (opcode == 2) healthy = false;
            else if (opcode == 3) healthy = pipe.Send(4, payload);
            else if (opcode == 1) {
              const auto event = Json::parse(payload, nullptr, false);
              if (!event.is_object()) { healthy = false; break; }
              if (event.value("evt", Json{}) == "ERROR") healthy = false;
              if (event.value("cmd", Json{}) == "DISPATCH" && event.value("evt", Json{}) == "READY") ready = true;
            }
          }
          if (!healthy || (!ready && Clock::now() >= deadline)) break;
          if (ready && Clock::now() >= next_update) {
            Activity activity;
            int64_t started;
            { const std::lock_guard lock(mutex_); activity = activity_; started = started_; }
            if (activity != sent || started != sent_started) {
              healthy = pipe.Send(1, Command(activity, started, GetCurrentProcessId(), ++nonce).dump());
              sent = std::move(activity); sent_started = started;
              next_update = Clock::now() + 5s;
            }
          }
          std::unique_lock lock(mutex_);
          wake_.wait_for(lock, 100ms, [this] { return !running_; });
        }
        if (ready && !running_) pipe.Send(1, Clear(GetCurrentProcessId(), ++nonce).dump());
      }
    } catch (...) {
      // Optional local integration: malformed replies or allocation failures
      // must not unwind into the game or create a rapid reconnect loop.
    }
    std::unique_lock lock(mutex_);
    wake_.wait_for(lock, 15s, [this] { return !running_; });
  }
#endif
}
} // namespace motogp::discord
