#pragma once
#include <chrono>
#include <deque>

namespace motogp::discord {
inline constexpr auto kCaptureInterval=std::chrono::milliseconds(100);
inline constexpr auto kSettleTime=std::chrono::milliseconds(200);
// Discord documents five activity updates per twenty seconds. Allow responsive
// bursts, then retain only the latest pending state until the window reopens.
class UpdatePolicy {
 public:
  using Clock=std::chrono::steady_clock;
  bool Ready(Clock::time_point now,Clock::time_point changed) {
    while(!sent_.empty()&&now-sent_.front()>=std::chrono::seconds(20))sent_.pop_front();
    return now-changed>=kSettleTime&&sent_.size()<5;
  }
  void Sent(Clock::time_point now){sent_.push_back(now);}
 private:
  std::deque<Clock::time_point> sent_;
};
} // namespace motogp::discord
