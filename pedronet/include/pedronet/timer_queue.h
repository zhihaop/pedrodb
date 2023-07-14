#ifndef PEDRONET_TIMER_QUEUE
#define PEDRONET_TIMER_QUEUE

#include <mutex>
#include <queue>
#include "pedrolib/executor/executor.h"
#include "pedronet/channel/timer_channel.h"

namespace pedronet {

struct TimerStruct : pedrolib::noncopyable, pedrolib::nonmovable {
  uint64_t id;
  Callback callback;
  Duration interval;

  TimerStruct(uint64_t id, Callback callback, const Duration& interval)
      : id(id), callback(std::move(callback)), interval(interval) {}
};

struct TimerOrder {
  Timestamp expire;
  std::weak_ptr<TimerStruct> timer;

  TimerOrder(Timestamp expire, const std::weak_ptr<TimerStruct>& timer)
      : expire(expire), timer(timer) {}

  bool operator<(const TimerOrder& other) const noexcept {
    return expire > other.expire;
  }
};

class TimerQueue {

  TimerChannel& channel_;
  Timestamp next_expire_ = Timestamp::Max();
  std::priority_queue<TimerOrder> schedule_timer_;
  std::queue<std::weak_ptr<TimerStruct>> expired_timers_;
  std::queue<std::weak_ptr<TimerStruct>> pending_timers_;

  std::mutex mu_;
  uint64_t sequences_{};
  std::unordered_map<uint64_t, std::shared_ptr<TimerStruct>> timers_;
  Executor& executor_;

  void updateExpire(Timestamp now);

  uint64_t createTimer(Callback cb, const Duration& delay,
                       const Duration& interval);

  void selectExpiredTimer(Timestamp now);

  void processExpireTimer();

  void processPendingTimer(Timestamp now);

 public:
  TimerQueue(TimerChannel& channel, Executor& executor);

  ~TimerQueue() { channel_.SetEventCallBack({}); }

  uint64_t ScheduleAfter(const Duration& delay, Callback callback);

  uint64_t ScheduleEvery(const Duration& delay, const Duration& interval,
                         Callback callback);

  void Cancel(uint64_t timer_id);
};
}  // namespace pedronet
#endif  // PEDRONET_TIMER_QUEUE