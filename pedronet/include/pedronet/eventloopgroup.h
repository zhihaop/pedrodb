#ifndef PEDRONET_EVENTLOOPGROUP_H
#define PEDRONET_EVENTLOOPGROUP_H

#include "pedronet/core/static_vector.h"
#include "pedronet/eventloop.h"
namespace pedronet {

class EventLoopGroup;
using EventLoopGroupPtr = std::shared_ptr<EventLoopGroup>;

class EventLoopGroup : public core::Executor {
  core::StaticVector<EventLoop> loops_;
  core::StaticVector<std::thread> threads_;
  std::atomic_size_t next_;
  size_t size_;

  size_t next() noexcept;

public:
  explicit EventLoopGroup(size_t threads)
      : loops_(threads), threads_(threads), size_(threads), next_(0) {}

  template <typename Selector> static EventLoopGroupPtr Create(size_t threads) {
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

  void Join();

  void Schedule(Callback cb) override { Next().Schedule(std::move(cb)); }

  uint64_t ScheduleAfter(core::Duration delay, Callback cb) override;

  uint64_t ScheduleEvery(core::Duration delay, core::Duration interval,
                         Callback cb) override;

  void ScheduleCancel(uint64_t id) override;

  void Start();

  void Close();
};

} // namespace pedronet

#endif // PEDRONET_EVENTLOOPGROUP_H
