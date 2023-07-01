#ifndef PEDRONET_REPEATER_H
#define PEDRONET_REPEATER_H

#include <pedronet/eventloopgroup.h>

template <typename T, typename Op> T AtomicApply(std::atomic<T> &val, Op &&op) {
  T n{}, m{};
  do {
    n = val.load();
    m = op(n);
  } while (!val.compare_exchange_strong(n, m));
  return m;
}

class Reporter {
  std::atomic_size_t bytes_{0};
  std::atomic_size_t counts_{0};
  std::atomic_size_t max_bytes_{0};
  std::atomic_size_t min_bytes_{size_t{0} - 1};

  std::function<void(size_t, size_t, size_t, size_t)> callback_;

  uint64_t id_{};
  pedronet::core::Executor &executor_;

public:
  explicit Reporter(pedronet::core::Executor &executor) : executor_(executor) {}
  void Trace(size_t bytes) {
    bytes_.fetch_add(bytes);
    counts_.fetch_add(1);
    AtomicApply(max_bytes_, [bytes](size_t x) { return std::max(x, bytes); });
    AtomicApply(min_bytes_, [bytes](size_t x) { return std::min(x, bytes); });
  }

  void SetCallback(std::function<void(size_t, size_t, size_t, size_t)> cb) {
    callback_ = std::move(cb);
  }

  void Start(const pedronet::core::Duration &interval) {
    id_ = executor_.ScheduleEvery(
        pedronet::core::Duration::Zero(), interval, [this] {
          size_t bytes = bytes_.exchange(0);
          size_t count = counts_.exchange(0);
          size_t min_bytes = min_bytes_.exchange(0);
          size_t max_bytes = max_bytes_.exchange(size_t{0} - 1);
          if (callback_) {
            callback_(bytes, count, min_bytes, max_bytes);
          }
        });
  }

  void Close() { executor_.ScheduleCancel(id_); }
};

#endif // PEDRONET_REPEATER_H
