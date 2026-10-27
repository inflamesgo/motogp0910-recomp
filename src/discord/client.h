#pragma once
#include "activity.h"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <functional>

namespace motogp::discord {
class Client {
 public:
  ~Client() { Stop(); }
  using JoinHandler=std::function<bool(const std::string&)>;
  void Start(JoinHandler join={});
  void Stop();
  void Publish(Activity activity);
 private:
  void Run();
  std::atomic<bool> running_{false};
  std::mutex mutex_;
  std::condition_variable wake_;
  std::thread worker_;
  Activity activity_ = Present({});
  JoinHandler join_;
  int64_t started_ = 0;
  std::chrono::steady_clock::time_point changed_{};
};
}
