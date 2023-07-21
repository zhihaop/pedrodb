#ifndef PEDRONET_EVENTLOOP_H
#define PEDRONET_EVENTLOOP_H

#include "pedrolib/executor/executor.h"
#include "pedronet/callbacks.h"
#include "pedronet/channel/channel.h"
#include "pedronet/channel/event_channel.h"
#include "pedronet/channel/timer_channel.h"
#include "pedronet/core/thread.h"
#include "pedronet/event.h"
#include "pedronet/selector/selector.h"
#include "pedronet/timer_queue.h"

#include <pedrolib/concurrent/latch.h>
#include <atomic>

namespace pedronet {

class EventLoop : public Executor {
  inline const static Duration kSelectTimeout{std::chrono::seconds(10)};

  std::unique_ptr<Selector> selector_;
  EventChannel event_channel_;
  TimerChannel timer_channel_;
  TimerQueue timer_queue_;

  std::mutex mu_;
  std::queue<Callback> pending_tasks_;
  std::queue<Callback> running_tasks_;

  std::atomic_int32_t state_{1};
  std::unordered_map<Channel*, Callback> channels_;

  pedrolib::Latch close_latch_{1};

  int32_t state() const noexcept {
    return state_.load(std::memory_order_acquire);
  }
  
  void ProcessScheduleTask();

 public:
  explicit EventLoop(std::unique_ptr<Selector> selector);

  Selector* GetSelector() noexcept { return selector_.get(); }

  void Deregister(Channel* channel);

  void Register(Channel* channel, Callback register_callback,
                Callback deregister_callback);

  bool CheckUnderLoop() const noexcept {
    return core::Thread::Current().CheckUnderLoop(this);
  }

  size_t Size() const noexcept override;

  void AssertUnderLoop() const;

  void Schedule(Callback cb) override;

  uint64_t ScheduleAfter(Duration delay, Callback cb) override {
    return timer_queue_.ScheduleAfter(delay, std::move(cb));
  }

  uint64_t ScheduleEvery(Duration delay, Duration interval,
                         Callback cb) override {
    return timer_queue_.ScheduleEvery(delay, interval, std::move(cb));
  }

  void ScheduleCancel(uint64_t id) override { timer_queue_.Cancel(id); }

  template <typename Runnable>
  void Run(Runnable&& runnable) {
    if (CheckUnderLoop()) {
      runnable();
      return;
    }
    Schedule(std::forward<Runnable>(runnable));
  }

  bool Closed() const noexcept { return state() == 0; }

  void Close() override;

  void Loop();

  // TODO join before exit.
  ~EventLoop() override = default;

  void Join() override;
};

}  // namespace pedronet

#endif  // PEDRONET_EVENTLOOP_H