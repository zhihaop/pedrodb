#ifndef PEDRONET_CHANNEL_EVENT_CHANNEL_H
#define PEDRONET_CHANNEL_EVENT_CHANNEL_H

#include "pedronet/callbacks.h"
#include "pedronet/channel/channel.h"
#include "pedronet/selector/selector.h"

namespace pedronet {

class EventChannel : public Channel {
  SelectorCallback event_callback_;
  core::File file_;

public:
  EventChannel();
  ~EventChannel() override = default;

  void SetEventCallBack(SelectorCallback cb) {
    event_callback_ = std::move(cb);
  }

  void HandleEvents(ReceiveEvents event, core::Timestamp now) override;

  core::File &File() noexcept override { return file_; }
  const core::File &File() const noexcept override { return file_; }

  std::string String() const override;

  void WakeUp() {
    uint64_t val = 1;
    file_.Write(&val, sizeof(val));
  }
};
} // namespace pedronet
#endif // PEDRONET_CHANNEL_EVENT_CHANNEL_H