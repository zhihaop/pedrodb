#ifndef PEDRODB_THREAD_POOL_EXECUTOR_H
#define PEDRODB_THREAD_POOL_EXECUTOR_H
#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <queue>
#include <thread>
#include "pedrolib/collection/static_vector.h"
#include "pedrolib/executor/executor.h"
namespace pedrolib {

namespace detail {
struct PriorityCallback {
  uint64_t id{};
  Timestamp expired;
  Duration interval;
  std::weak_ptr<Callback> callback;

  bool operator<(const PriorityCallback& other) const noexcept {
    return expired > other.expired;
  }
};

}  // namespace detail

class ThreadPoolExecutor : public Executor {
  std::mutex mu_;
  std::atomic_bool shutdown_{false};
  StaticVector<std::thread> workers_;

  std::condition_variable non_empty_;

  std::priority_queue<detail::PriorityCallback> queue_;
  std::unordered_map<uint64_t, std::shared_ptr<Callback>> callbacks_;
  uint64_t id_{};

  void worker() {
    while (!shutdown_.load(std::memory_order_acquire)) {
      std::unique_lock<std::mutex> lock(mu_);
      while (true) {
        if (shutdown_.load(std::memory_order_acquire)) {
          return;
        }
        if (queue_.empty()) {
          non_empty_.wait(lock);
          continue;
        }
        auto& top = queue_.top();
        Duration d = top.expired - Timestamp::Now();
        if (d <= Duration::Microseconds(100)) {
          break;
        }
        non_empty_.wait_for(lock, std::chrono::microseconds(d.usecs));
      }

      auto top = queue_.top();
      queue_.pop();

      if (top.callback.expired()) {
        continue;
      }

      auto callback = top.callback.lock();
      if (callback == nullptr) {
        continue;
      }

      if (top.interval != Duration::Zero()) {
        detail::PriorityCallback detail{
            .id = top.id,
            .expired = Timestamp::Now() + top.interval,
            .interval = top.interval,
            .callback = top.callback,
        };
        queue_.emplace(std::move(detail));
      } else {
        callbacks_.erase(top.id);
      }
      lock.unlock();

      if (*callback) {
        (*callback)();
      }
    }
  }

  uint64_t schedule(const Duration& delay, const Duration& interval,
                    Callback cb) {
    uint64_t id = id_++;
    detail::PriorityCallback detail{
        .id = id,
        .expired = Timestamp::Now() + delay,
        .interval = interval,
        .callback = callbacks_[id] = std::make_shared<Callback>(std::move(cb)),
    };
    queue_.emplace(std::move(detail));
    return id;
  }

  void close() {
    callbacks_.clear();
    shutdown_.store(true, std::memory_order_release);
    non_empty_.notify_all();
  }

  void join() {
    for (auto& thread : workers_) {
      if (thread.joinable()) {
        thread.join();
      }
    }
  }

 public:
  explicit ThreadPoolExecutor()
      : ThreadPoolExecutor(std::thread::hardware_concurrency()) {}

  explicit ThreadPoolExecutor(size_t threads) : workers_(threads) {
    for (size_t i = 0; i < threads; ++i) {
      workers_.emplace_back([this] { worker(); });
    }
  }

  ~ThreadPoolExecutor() override {
    close();
    join();
  }

  size_t Size() const noexcept { return workers_.size(); }

  void Schedule(Callback cb) override {
    std::unique_lock<std::mutex> lock(mu_);
    bool wakeup = queue_.empty();
    schedule(Duration::Zero(), Duration::Zero(), std::move(cb));
    if (wakeup) {
      non_empty_.notify_all();
    }
  }
  
  uint64_t ScheduleAfter(Duration delay, Callback cb) override {
    std::unique_lock<std::mutex> lock(mu_);
    bool wakeup = queue_.empty();
    uint64_t id = schedule(delay, Duration::Zero(), std::move(cb));
    if (wakeup) {
      non_empty_.notify_all();
    }
    return id;
  }
  
  uint64_t ScheduleEvery(Duration delay, Duration interval,
                         Callback cb) override {
    std::unique_lock<std::mutex> lock(mu_);
    bool wakeup = queue_.empty();
    uint64_t id = schedule(delay, interval, std::move(cb));
    if (wakeup) {
      non_empty_.notify_all();
    }
    return id;
  }
  
  void ScheduleCancel(uint64_t id) override {
    std::unique_lock<std::mutex> lock(mu_);
    callbacks_.erase(id);
  }

  void Close() override {
    std::unique_lock<std::mutex> lock(mu_);
    close();
  }

  void Join() override { join(); }
};
}  // namespace pedrolib

#endif  // PEDRODB_THREAD_POOL_EXECUTOR_H
