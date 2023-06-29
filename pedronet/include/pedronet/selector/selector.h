#ifndef PEDRONET_SELECTOR_SELECTOR_H
#define PEDRONET_SELECTOR_SELECTOR_H

#include "pedronet/core/duration.h"
#include "pedronet/core/file.h"
#include "pedronet/core/noncopyable.h"
#include "pedronet/core/nonmovable.h"
#include "pedronet/core/timestamp.h"
#include "pedronet/event.h"

#include <functional>
#include <memory>

namespace pedronet {

struct Channel;

struct Selected {
  core::Timestamp now;
  std::vector<Channel *> channels;
  std::vector<ReceiveEvents> events;
  core::File::Error error;
};

struct Selector : core::noncopyable, core::nonmovable {
  virtual void Add(Channel *channel, SelectEvents events) = 0;
  virtual void Remove(Channel *channel) = 0;
  virtual void Update(Channel *channel, SelectEvents events) = 0;
  virtual void Wait(core::Duration timeout, Selected *selected) = 0;
  virtual ~Selector() = default;
};
} // namespace pedronet
#endif // PEDRONET_SELECTOR_SELECTOR_H