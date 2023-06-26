#ifndef PERDONET_EPOLL_H
#define PERDONET_EPOLL_H

#include "pedronet/core/file.h"

#include "pedronet/channel/channel.h"
#include "pedronet/event.h"
#include "pedronet/selector/selector.h"
#include <vector>

struct epoll_event;

namespace pedronet {

class Epoller : public core::File, public Selector {
  std::vector<struct epoll_event> buffer_;

  void epollerUpdate(Channel *channel, uint32_t op, SelectEvents events);

public:
  Epoller(size_t size);
  ~Epoller() override;

  void Add(Channel *channel, SelectEvents events) override;
  void Remove(Channel *channel) override;
  void Update(Channel *channel, SelectEvents events) override;

  void Update(Channel *channel, uint32_t op, SelectEvents events);
  void Wait(core::Duration timeout, Selected *selected) override;
};
} // namespace pedronet

#endif // PERDONET_EPOLL_H