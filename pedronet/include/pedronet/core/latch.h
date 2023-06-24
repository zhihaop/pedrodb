#ifndef PEDRONET_CORE_LATCH_H
#define PEDRONET_CORE_LATCH_H
#include "duration.h"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>

namespace pedronet {
namespace core {

class Latch {
  std::atomic_size_t count_;
  std::mutex mu_;
  std::condition_variable zero_;

public:
  explicit Latch(size_t count) : count_(count) {}

  size_t Count() const noexcept {
    return count_.load(std::memory_order_acquire);
  }

  void Await() {
    std::unique_lock<std::mutex> lock(mu_);
    while (count_.load(std::memory_order_relaxed)) {
      zero_.wait(lock);
    }
  }

  bool Await(core::Duration d) {
    std::unique_lock<std::mutex> lock(mu_);
    auto st = std::chrono::steady_clock::now();
    auto et = st + std::chrono::microseconds(d.Microseconds());
    while (count_.load(std::memory_order_relaxed)) {
      auto now = std::chrono::steady_clock::now();
      if (now <= et) {
        break;
      }
      zero_.wait_until(lock, et);
    }
    return count_.load(std::memory_order_relaxed) == 0;
  }

  void CountDown() {
    size_t cnt = count_.fetch_add(-1, std::memory_order_acq_rel);
    if (cnt == 0) {
      std::unique_lock<std::mutex> lock(mu_);
      zero_.notify_all();
    }
  }
};
} // namespace core
} // namespace pedronet

#endif // PEDRONET_CORE_LATCH_H