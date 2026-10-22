#pragma once
#include "activity.h"
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

namespace motogp::discord {
class Client {
 public:
  ~Client() { Stop(); }
  void Start();
  void Stop();
  void Publish(Activity activity);
 private:
  void Run();
  std::atomic<bool> running_{false};
  std::mutex mutex_;
  std::condition_variable wake_;
  std::thread worker_;
  Activity activity_ = Present({});
  int64_t started_ = 0;
};
}
