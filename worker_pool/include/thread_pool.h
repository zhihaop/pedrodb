#ifndef PRACTICE_THREAD_POOL_H
#define PRACTICE_THREAD_POOL_H
#include "blocking_queue.h"
#include <functional>
#include <thread>

namespace pedro {

using Runnable = std::function<void(void)>;

class FixedThreadPoolExecutor {

  const size_t core_size_;
  std::atomic_bool shutdown_{};

  std::vector<std::thread> workers_;
  pedro::BlockingQueue<Runnable> queue_;

  void init_workers() {
    for (size_t i = 0; i < core_size_; ++i) {
      workers_.emplace_back([&] {
        while (!shutdown_.load(std::memory_order_acquire)) {
          
          // take all the task until the queue is empty.
          while (true) {
            std::optional<Runnable> item = queue_.Take();
            if (!item.has_value()) {
              break;
            }
            Runnable runnable = std::move(*item);
            if (runnable) {
              runnable();
            }
          }
        }
      });
    }
  }

public:
  FixedThreadPoolExecutor(size_t core_size, size_t queue_size)
      : core_size_(core_size), queue_(queue_size) {
    init_workers();
  }

  ~FixedThreadPoolExecutor() {
    Shutdown();
    Join();
  }

  template <typename R> bool TrySubmit(R &&runnable) {
    if (shutdown_.load(std::memory_order_acquire)) {
      return false;
    }
    if (!queue_.Offer(runnable)) {
      return false;
    }
    return true;
  }

  template <typename R> bool Submit(R &&runnable) {
    if (shutdown_.load(std::memory_order_acquire)) {
      return false;
    }
    if (!queue_.Put(runnable)) {
      return false;
    }
    return true;
  }

  void Shutdown() noexcept {
    shutdown_.store(true, std::memory_order_release);
    queue_.Close();
  }

  void Join() {
    for (auto &&t : workers_) {
      if (t.joinable())
        t.join();
    }
    workers_.clear();
  }
};
} // namespace pedro
#endif // PRACTICE_THREAD_POOL_H
