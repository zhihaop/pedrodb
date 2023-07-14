#ifndef PEDRONET_CHANNEL_TIMED_CHANNEL_H
#define PEDRONET_CHANNEL_TIMED_CHANNEL_H

#include "pedronet/callbacks.h"
#include "pedronet/channel/channel.h"
#include "pedronet/selector/selector.h"

namespace pedronet {

class TimerChannel final : public Channel {
  inline static const Duration kMinWakeUpDuration = Duration::Microseconds(100);

  SelectorCallback event_callback_;
  File file_;

 public:
  TimerChannel();
  ~TimerChannel() override = default;

  void SetEventCallBack(SelectorCallback cb) {
    event_callback_ = std::move(cb);
  }

  void HandleEvents(ReceiveEvents events, Timestamp now) override;

  File& GetFile() noexcept override { return file_; }
  const File& GetFile() const noexcept override { return file_; }

  std::string String() const override;

  void WakeUpAt(Timestamp timestamp) {
    WakeUpAfter(timestamp - Timestamp::Now());
  }

  void WakeUpAfter(Duration duration);
};

}  // namespace pedronet

PEDROLIB_CLASS_FORMATTER(pedronet::TimerChannel);
#endif  // PEDRONET_CHANNEL_TIMED_CHANNEL_H