#include "pedronet/eventloopgroup.h"

namespace pedronet {

size_t EventLoopGroup::next() noexcept {
  return next_.fetch_add(1, std::memory_order_relaxed) % size_;
}

void EventLoopGroup::Join() {
  HandleJoin();
}

uint64_t EventLoopGroup::ScheduleAfter(Duration delay, Callback cb) {
  size_t loop_id = next();
  uint64_t timer_id = loops_[loop_id].ScheduleAfter(delay, std::move(cb));
  return timer_id * loops_.size() + loop_id;
}
uint64_t EventLoopGroup::ScheduleEvery(Duration delay, Duration interval,
                                       Callback cb) {
  size_t loop_id = next();
  uint64_t timer_id =
      loops_[loop_id].ScheduleEvery(delay, interval, std::move(cb));
  return timer_id * loops_.size() + loop_id;
}
void EventLoopGroup::ScheduleCancel(uint64_t id) {
  size_t loop_id = id % loops_.size();
  size_t timer_id = id / loops_.size();
  loops_[loop_id].ScheduleCancel(timer_id);
}

void EventLoopGroup::Close() {
  for (auto& loop : loops_) {
    loop.Close();
  }
}

void EventLoopGroup::HandleJoin() {
  for (auto& t : threads_) {
    if (t.joinable()) {
      t.join();
    }
  }
  threads_.clear();
}

size_t EventLoopGroup::Size() const noexcept {
  return threads_.size();
}
}  // namespace pedronet