#ifndef PEDRONET_SELECTOR_H
#define PEDRONET_SELECTOR_H

#include "core/noncopyable.h"
#include "core/nonmoveable.h"
#include "core/timestamp.h"
#include "event.h"

#include <functional>
#include <memory>


namespace pedronet {

struct Channel;
using CallBack = std::function<void()>;
using ChannelPtr = std::shared_ptr<Channel>;
using SelectorCallback = std::function<void(ReceiveEvents, core::Timestamp)>;

struct Selector : core::noncopyable, core::nonmoveable {
 

  // For pedronet::Channel.
  virtual void Update(Channel *channel, SelectEvents events,
                      const CallBack &cb) = 0;
  virtual void Register(const ChannelPtr &channel, const CallBack &cb) = 0;
  virtual void Deregister(const ChannelPtr &channel, const CallBack &cb) = 0;
  virtual ~Selector() = default;
};
} // namespace pedronet
#endif // PEDRONET_SELECTOR_H