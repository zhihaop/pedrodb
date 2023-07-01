#ifndef PEDRONET_CHANNEL_TIMED_CHANNEL_H
#define PEDRONET_CHANNEL_TIMED_CHANNEL_H

#include "pedronet/callbacks.h"
#include "pedronet/channel/channel.h"
#include "pedronet/selector/selector.h"

namespace pedronet {

class TimerChannel final : public Channel {
  inline static const core::Duration kMinWakeUpDuration =
      core::Duration::Microseconds(100);

  SelectorCallback event_callback_;
  core::File file_;

public:
  TimerChannel();
  ~TimerChannel() override = default;

  void SetEventCallBack(SelectorCallback cb) {
    event_callback_ = std::move(cb);
  }

  void HandleEvents(ReceiveEvents events, core::Timestamp now) override;

  core::File &File() noexcept override { return file_; }
  const core::File &File() const noexcept override { return file_; }

  std::string String() const override;

  void WakeUpAt(core::Timestamp timestamp) {
    WakeUpAfter(timestamp - core::Timestamp::Now());
  }

  void WakeUpAfter(core::Duration duration);
};

} // namespace pedronet

PEDRONET_CLASS_FORMATTER(pedronet::TimerChannel);
#endif // PEDRONET_CHANNEL_TIMED_CHANNEL_H