#ifndef PEDRONET_EVENT_CHANNEL_H
#define PEDRONET_EVENT_CHANNEL_H

#include "channel.h"

namespace pedronet {

class EventChannel : public core::File, public Channel {
  SelectorCallback event_callback_;

public:
  EventChannel();
  ~EventChannel() override = default;

  void SetEventCallBack(SelectorCallback cb) {
    event_callback_ = std::move(cb);
  }

  void HandleEvents(ReceiveEvents event, core::Timestamp now) override;

  core::File &File() noexcept override { return *this; }

  void WakeUp() {
    uint64_t val = 1;
    Write(&val, sizeof(val));
  }
};
} // namespace pedronet
#endif // PEDRONET_EVENT_CHANNEL_H