#ifndef PEDRONET_EVENTLOOPGROUP_H
#define PEDRONET_EVENTLOOPGROUP_H

#include "pedronet/core/static_vector.h"
#include "pedronet/eventloop.h"
namespace pedronet {

class EventLoopGroup : public core::Executor {
  core::StaticVector<EventLoop> loops_;
  core::StaticVector<std::thread> threads_;
  std::atomic_size_t next_;
  size_t size_;

  size_t next() {
    for (;;) {
      size_t c = next_.load(std::memory_order_acquire);
      size_t n = (c + 1) % size_;
      if (!next_.compare_exchange_strong(c, n)) {
        continue;
      }
      return c;
    }
  }

public:
  explicit EventLoopGroup(size_t threads)
      : loops_(threads), threads_(threads), size_(threads), next_(0) {}
  template <typename Selector>
  static std::shared_ptr<EventLoopGroup> Create(size_t threads) {
    auto group = std::make_shared<EventLoopGroup>(threads);

    auto &loops = group->loops_;
    for (size_t i = 0; i < threads; ++i) {
      auto selector = std::make_unique<Selector>();
      loops.emplace_back(std::move(selector));
    }
    return group;
  }

  ~EventLoopGroup() { Join(); }

  EventLoop &Next() { return loops_[next()]; }

  void Join() {
    for (auto &t : threads_) {
      if (t.joinable()) {
        t.join();
      }
    }
    threads_.clear();
  }

  void Schedule(Callback cb) override { Next().Schedule(std::move(cb)); }

  uint64_t ScheduleAfter(core::Duration delay, Callback cb) override {
    size_t loop_id = next();
    uint64_t timer_id = loops_[loop_id].ScheduleAfter(delay, std::move(cb));
    return timer_id * loops_.size() + loop_id;
  }

  uint64_t ScheduleEvery(core::Duration delay, core::Duration interval,
                         Callback cb) override {
    size_t loop_id = next();
    uint64_t timer_id =
        loops_[loop_id].ScheduleEvery(delay, interval, std::move(cb));
    return timer_id * loops_.size() + loop_id;
  }

  void ScheduleCancel(uint64_t id) override {
    size_t loop_id = id % loops_.size();
    size_t timer_id = id / loops_.size();
    loops_[loop_id].ScheduleCancel(timer_id);
  }

  void Start() {
    for (size_t i = 0; i < loops_.size(); ++i) {
      threads_.emplace_back([&loop = loops_[i]] { loop.Loop(); });
    }
  }

  void Close() {
    for (auto &loop : loops_) {
      loop.Close();
    }
  }
};
} // namespace pedronet

#endif // PEDRONET_EVENTLOOPGROUP_H
