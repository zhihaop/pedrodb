#ifndef PEDROLIB_EXECUTOR_EXECUTOR_H
#define PEDROLIB_EXECUTOR_EXECUTOR_H

#include "pedrolib/duration.h"
#include "pedrolib/noncopyable.h"
#include "pedrolib/nonmovable.h"
#include "pedrolib/timestamp.h"

#include <functional>

namespace pedrolib {

using Callback = std::function<void()>;

struct Executor : pedrolib::noncopyable, pedrolib::nonmovable {
  virtual void Schedule(Callback cb) = 0;
  virtual uint64_t ScheduleAfter(Duration delay, Callback cb) = 0;
  virtual uint64_t ScheduleEvery(Duration delay, Duration interval,
                                 Callback cb) = 0;
  virtual void ScheduleCancel(uint64_t) = 0;
};

} // namespace pedrolib

#endif // PEDROLIB_EXECUTOR_EXECUTOR_H