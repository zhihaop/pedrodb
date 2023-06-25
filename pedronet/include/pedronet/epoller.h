#ifndef PERDONET_EPOLL_H
#define PERDONET_EPOLL_H


#include "core/file.h"

#include "selector.h"
#include "channel.h"
#include <vector>

struct epoll_event;

namespace pedronet {

struct Selected {
  core::Timestamp now;
  std::vector<Channel *> channels;
  std::vector<ReceiveEvents> events;
  core::File::Error error;
};

class Epoller : public core::File {
  std::unique_ptr<struct epoll_event, void (*)(void *)> buffer_;
  const size_t buffer_size_;

public:
  static const uint32_t kAdd;
  static const uint32_t kDel;
  static const uint32_t kMod;

public:
  Epoller(size_t size);
  ~Epoller() override = default;

  void Update(Channel *channel, uint32_t op, SelectEvents events);
  void Wait(core::Duration timeout, Selected *selected);
};
} // namespace pedronet

#endif // PERDONET_EPOLL_H