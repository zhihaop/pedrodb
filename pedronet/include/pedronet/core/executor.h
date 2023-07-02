#ifndef PEDRONET_CORE_EXECUTOR_H
#define PEDRONET_CORE_EXECUTOR_H

#include "pedronet/callbacks.h"

#include <pedrolib/noncopyable.h>
#include <pedrolib/nonmovable.h>

namespace pedronet::core {

struct Executor : pedrolib::noncopyable, pedrolib::nonmovable {
  virtual void Schedule(Callback cb) = 0;
  virtual uint64_t ScheduleAfter(Duration delay, Callback cb) = 0;
  virtual uint64_t ScheduleEvery(Duration delay, Duration interval,
                                 Callback cb) = 0;
  virtual void ScheduleCancel(uint64_t) = 0;
};

} // namespace pedronet::core

#endif // PEDRONET_CORE_EXECUTOR_H