#ifndef PEDRONET_TIMER_QUEUE
#define PEDRONET_TIMER_QUEUE

#include "pedronet/channel/timer_channel.h"
#include "pedronet/core/executor.h"
#include "pedronet/selector/selector.h"

#include <queue>

namespace pedronet {

struct TimerStruct : core::noncopyable, core::nonmovable {
  uint64_t id;
  Callback callback;
  core::Duration interval;

  TimerStruct(uint64_t id, Callback callback, const core::Duration &interval)
      : id(id), callback(std::move(callback)), interval(interval) {}
};

struct TimerOrder {
  core::Timestamp expire;
  std::weak_ptr<TimerStruct> timer;

  TimerOrder(core::Timestamp expire, const std::weak_ptr<TimerStruct> &timer)
      : expire(expire), timer(timer) {}

  bool operator<(const TimerOrder &other) const noexcept {
    return expire > other.expire;
  }
};

class TimerQueue {

  TimerChannel &channel_;
  core::Timestamp next_expire_ = core::Timestamp::Max();
  std::priority_queue<TimerOrder> schedule_timer_;
  std::queue<std::weak_ptr<TimerStruct>> expired_timers_;
  std::queue<std::weak_ptr<TimerStruct>> pending_timers_;

  std::mutex mu_;
  uint64_t sequences_{};
  std::unordered_map<uint64_t, std::shared_ptr<TimerStruct>> timers_;
  core::Executor &executor_;

  void updateExpire(core::Timestamp now);

  uint64_t createTimer(Callback cb, const core::Duration &delay,
                       const core::Duration &interval);

  void selectExpiredTimer(core::Timestamp now);

  void processExpireTimer();

  void processPendingTimer(core::Timestamp now);

public:
  TimerQueue(TimerChannel &channel, core::Executor &executor);

  ~TimerQueue() { channel_.SetEventCallBack({}); }

  uint64_t ScheduleAfter(const core::Duration& delay, Callback callback);

  uint64_t ScheduleEvery(const core::Duration& delay, const core::Duration& interval,
                         Callback callback);

  void Cancel(uint64_t timer_id);
};
} // namespace pedronet
#endif // PEDRONET_TIMER_QUEUE