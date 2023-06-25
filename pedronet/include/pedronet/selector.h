#ifndef PEDRONET_SELECTOR_H
#define PEDRONET_SELECTOR_H

#include "core/duration.h"
#include "core/file.h"
#include "core/noncopyable.h"
#include "core/nonmoveable.h"
#include "core/timestamp.h"
#include "event.h"

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

using SelectorCallback = std::function<void(ReceiveEvents events, core::Timestamp)>;

struct Selector : core::noncopyable, core::nonmoveable {
  virtual void Add(Channel *channel, SelectEvents events) = 0;
  virtual void Remove(Channel *channel) = 0;
  virtual void Update(Channel *channel, SelectEvents events) = 0;
  virtual void Wait(core::Duration timeout, Selected *selected) = 0;
  virtual ~Selector() = default;
};
} // namespace pedronet
#endif // PEDRONET_SELECTOR_H