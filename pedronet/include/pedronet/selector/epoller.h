#ifndef PERDONET_SELECTOR_EPOLLER_H
#define PERDONET_SELECTOR_EPOLLER_H

#include "pedrolib/file/file.h"
#include "pedronet/channel/channel.h"
#include "pedronet/event.h"
#include "pedronet/selector/selector.h"

#include <vector>

struct epoll_event;

namespace pedronet {

class EpollSelector : public File, public Selector {
  std::vector<struct epoll_event> buf_;
  void internalUpdate(Channel* channel, int op, SelectEvents events);

 public:
  EpollSelector();
  ~EpollSelector() override;
  void SetBufferSize(size_t size);

  void Add(Channel* channel, SelectEvents events) override;
  void Remove(Channel* channel) override;
  void Update(Channel* channel, SelectEvents events) override;

  Error Wait(Duration timeout, SelectChannels* selected) override;
};
}  // namespace pedronet

#endif  // PERDONET_SELECTOR_EPOLLER_H