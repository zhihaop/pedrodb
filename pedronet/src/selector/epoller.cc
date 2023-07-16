#include "pedronet/selector/epoller.h"
#include <sys/epoll.h>
#include "pedronet/logger/logger.h"

namespace pedronet {

inline static File CreateEpollFile() {
  int fd = ::epoll_create1(EPOLL_CLOEXEC);
  if (fd <= 0) {
    PEDRONET_FATAL("failed to create epoll fd, errno[{}]", errno);
  }
  return File{fd};
}

EpollSelector::EpollSelector() : File(CreateEpollFile()), buf_(4096) {}

EpollSelector::~EpollSelector() = default;

void EpollSelector::internalUpdate(Channel* channel, int op,
                                   SelectEvents events) {
  struct epoll_event ev {};
  ev.events = events.Value();
  ev.data.ptr = channel;

  int fd = channel->GetFile().Descriptor();
  if (::epoll_ctl(fd_, op, fd, op == EPOLL_CTL_DEL ? nullptr : &ev) < 0) {
    PEDRONET_FATAL("epoll_ctl({}) failed, reason[{}]", *channel, Error{errno});
  }
}

void EpollSelector::Add(Channel* channel, SelectEvents events) {
  internalUpdate(channel, EPOLL_CTL_ADD, events);
}

void EpollSelector::Update(Channel* channel, SelectEvents events) {
  internalUpdate(channel, EPOLL_CTL_MOD, events);
}

void EpollSelector::Remove(Channel* channel) {
  internalUpdate(channel, EPOLL_CTL_DEL, SelectEvents::kNoneEvent);
}

Error EpollSelector::Wait(Duration timeout, SelectChannels* selected) {
  int n = ::epoll_wait(fd_, buf_.data(), (int)buf_.size(),
                       (int)timeout.Milliseconds());
  if (n < 0) {
    return Error{errno};
  }

  selected->now = Timestamp::Now();

  selected->channels.resize(n);
  selected->events.resize(n);

  for (int i = 0; i < n; ++i) {
    selected->channels[i] = static_cast<Channel*>(buf_[i].data.ptr);
    selected->events[i] = ReceiveEvents{buf_[i].events};
  }
  return Error::Success();
}

void EpollSelector::SetBufferSize(size_t size) {
  buf_.resize(size);
}
}  // namespace pedronet