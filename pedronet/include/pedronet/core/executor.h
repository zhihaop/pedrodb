#ifndef PEDRONET_CORE_EXECUTOR_H
#define PEDRONET_CORE_EXECUTOR_H

#include "pedronet/callbacks.h"
#include "pedronet/core/duration.h"
#include "pedronet/core/noncopyable.h"
#include "pedronet/core/nonmovable.h"

namespace pedronet::core {

struct Executor : core::noncopyable, core::nonmovable {
  virtual void Schedule(Callback cb) = 0;
  virtual uint64_t ScheduleAfter(core::Duration delay, Callback cb) = 0;
  virtual uint64_t ScheduleEvery(core::Duration delay, core::Duration interval,
                                 Callback cb) = 0;
  virtual void ScheduleCancel(uint64_t) = 0;
};

} // namespace pedronet::core

#endif // PEDRONET_CORE_EXECUTOR_H