#ifndef PEDRONET_CHANNEL_CHANNEL_H
#define PEDRONET_CHANNEL_CHANNEL_H

#include "pedronet/core/debug.h"
#include "pedronet/core/file.h"
#include "pedronet/core/noncopyable.h"
#include "pedronet/core/nonmovable.h"
#include "pedronet/core/timestamp.h"
#include "pedronet/event.h"

#include <memory>

namespace pedronet {
struct Channel : core::noncopyable, core::nonmovable {

  // For pedronet::Selector.
  virtual core::File &File() noexcept = 0;
  virtual const core::File &File() const noexcept = 0;
  virtual void HandleEvents(ReceiveEvents events, core::Timestamp now) = 0;
  virtual std::string String() const = 0;
  virtual ~Channel() = default;
};

} // namespace pedronet

PEDRONET_CLASS_FORMATTER(pedronet::Channel);
#endif // PEDRONET_CHANNEL_CHANNEL_H