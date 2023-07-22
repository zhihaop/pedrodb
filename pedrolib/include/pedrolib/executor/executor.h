#ifndef PEDROLIB_EXECUTOR_EXECUTOR_H
#define PEDROLIB_EXECUTOR_EXECUTOR_H

#include "pedrolib/concurrent/latch.h"
#include "pedrolib/duration.h"
#include "pedrolib/noncopyable.h"
#include "pedrolib/nonmovable.h"
#include "pedrolib/timestamp.h"

#include <functional>

namespace pedrolib {

using Callback = std::function<void()>;

struct Executor : noncopyable, nonmovable {
  Executor() = default;
  virtual ~Executor() = default;
  virtual void Schedule(Callback cb) = 0;
  virtual uint64_t ScheduleAfter(Duration delay, Callback cb) = 0;
  virtual uint64_t ScheduleEvery(Duration delay, Duration interval,
                                 Callback cb) = 0;
  virtual void ScheduleCancel(uint64_t) = 0;
  virtual void Close() = 0;
  virtual void Join() = 0;
  [[nodiscard]] virtual size_t Size() const noexcept = 0;
};

template <typename Iterator, typename Task>
void for_each(Executor* executor, Iterator begin, Iterator end, Task&& task) {
  size_t n = std::distance(begin, end);
  size_t p = executor->Size();
  size_t m = n / p;
  size_t t = n % p;

  Latch latch(p);
  Iterator last = begin;
  for (size_t i = 0; i < p; ++i) {
    Iterator first = last;
    Iterator next = first + m + (i < t);
    last = next;
    executor->Schedule([&, first, next, task] {
      std::for_each(first, next, task);
      latch.CountDown();
    });
  }
  latch.Await();
}

}  // namespace pedrolib

#endif  // PEDROLIB_EXECUTOR_EXECUTOR_H