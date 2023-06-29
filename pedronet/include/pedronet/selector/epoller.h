#ifndef PERDONET_SELECTOR_EPOLLER_H
#define PERDONET_SELECTOR_EPOLLER_H

#include "pedronet/core/file.h"

#include "pedronet/channel/channel.h"
#include "pedronet/event.h"
#include "pedronet/selector/selector.h"
#include <vector>

struct epoll_event;

namespace pedronet {

class EpollSelector : public core::File, public Selector {
  std::vector<struct epoll_event> buffer_;

  void internalUpdate(Channel *channel, int op, SelectEvents events);

public:
  explicit EpollSelector(size_t size);
  ~EpollSelector() override;

  void Add(Channel *channel, SelectEvents events) override;
  void Remove(Channel *channel) override;
  void Update(Channel *channel, SelectEvents events) override;

  void Wait(core::Duration timeout, Selected *selected) override;
};
} // namespace pedronet

#endif // PERDONET_SELECTOR_EPOLLER_H