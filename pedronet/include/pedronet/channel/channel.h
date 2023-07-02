#ifndef PEDRONET_CHANNEL_CHANNEL_H
#define PEDRONET_CHANNEL_CHANNEL_H

#include "pedronet/core/file.h"
#include "pedronet/event.h"

#include <pedrolib/format/formatter.h>
#include <pedrolib/noncopyable.h>
#include <pedrolib/nonmovable.h>
#include <pedrolib/timestamp.h>

#include <memory>

namespace pedronet {

using Timestamp = pedrolib::Timestamp;
using Duration = pedrolib::Duration;

struct Channel : pedrolib::noncopyable, pedrolib::nonmovable {

  // For pedronet::Selector.
  virtual core::File &File() noexcept = 0;
  virtual const core::File &File() const noexcept = 0;
  virtual void HandleEvents(ReceiveEvents events, Timestamp now) = 0;
  virtual std::string String() const = 0;
  virtual ~Channel() = default;
};

} // namespace pedronet

PEDROLIB_CLASS_FORMATTER(pedronet::Channel);
#endif // PEDRONET_CHANNEL_CHANNEL_H