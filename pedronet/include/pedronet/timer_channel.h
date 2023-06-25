#ifndef PEDRONET_TIMED_CHANNEL_H
#define PEDRONET_TIMED_CHANNEL_H

#include "channel.h"
#include "selector.h"

namespace pedronet {

class TimerChannel : public core::File, public Channel {
  inline static const core::Duration kMinWakeUpDuration =
      core::Duration::Microseconds(100);

  SelectorCallback event_callback_;

public:
  TimerChannel();
  ~TimerChannel() override = default;

  void SetEventCallBack(SelectorCallback cb) {
    event_callback_ = std::move(cb);
  }

  void HandleEvents(ReceiveEvents events, core::Timestamp now) override;

  core::File &File() noexcept override { return *this; }

  void WakeUpAt(core::Timestamp timestamp) {
    WakeUpAfter(timestamp - core::Timestamp::Now());
  }

  void WakeUpAfter(core::Duration duration);
};

} // namespace pedronet
#endif // PEDRONET_TIMED_CHANNEL_H