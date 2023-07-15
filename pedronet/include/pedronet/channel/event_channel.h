#ifndef PEDRONET_CHANNEL_EVENT_CHANNEL_H
#define PEDRONET_CHANNEL_EVENT_CHANNEL_H

#include "pedronet/callbacks.h"
#include "pedronet/channel/channel.h"
#include "pedronet/selector/selector.h"

namespace pedronet {

class EventChannel final : public Channel {
  Callback event_callback_;
  File file_;

 public:
  EventChannel();
  ~EventChannel() override = default;

  void SetEventCallBack(Callback cb) {
    event_callback_ = std::move(cb);
  }

  void HandleEvents(ReceiveEvents event, Timestamp now) override;

  File& GetFile() noexcept override { return file_; }
  [[nodiscard]] const File& GetFile() const noexcept override { return file_; }

  [[nodiscard]] std::string String() const override;

  void WakeUp();
};
}  // namespace pedronet

PEDROLIB_CLASS_FORMATTER(pedronet::EventChannel);
#endif  // PEDRONET_CHANNEL_EVENT_CHANNEL_H