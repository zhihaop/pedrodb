#include "pedronet/selector/epoller.h"
#include <memory>
#include <sys/epoll.h>

namespace pedronet {

inline static core::File CreateEpollFile() {
  int fd = ::epoll_create1(EPOLL_CLOEXEC);
  if (fd <= 0) {
    spdlog::error("failed to create epoll fd, errno[{}]", errno);
    std::terminate();
  }
  return core::File{fd};
}

EpollSelector::EpollSelector(size_t size) : core::File(CreateEpollFile()), buffer_(size) {}

EpollSelector::~EpollSelector() = default;

void EpollSelector::internalUpdate(Channel *channel, int op,
                            SelectEvents events) {
  struct epoll_event ev {};
  ev.events = events.Value();
  ev.data.ptr = channel;

  int fd = channel->File().Descriptor();
  if (::epoll_ctl(fd_, op, fd, op == EPOLL_CTL_DEL ? nullptr : &ev) < 0) {
    spdlog::error("failed to call epoll_ctl, reason[{}]", errno);
    std::terminate();
  }
}

void EpollSelector::Add(Channel *channel, SelectEvents events) {
  internalUpdate(channel, EPOLL_CTL_ADD, events);
}

void EpollSelector::Update(Channel *channel, SelectEvents events) {
  internalUpdate(channel, EPOLL_CTL_MOD, events);
}

void EpollSelector::Remove(Channel *channel) {
  internalUpdate(channel, EPOLL_CTL_DEL, SelectEvents::kNoneEvent);
}

void EpollSelector::Wait(core::Duration timeout, Selected *selected) {
  int nevents =
      ::epoll_wait(fd_, buffer_.data(), buffer_.size(), timeout.Milliseconds());
  selected->now = core::Timestamp::Now();
  selected->channels.clear();
  selected->events.clear();
  selected->error.Clear();

  if (nevents == 0) {
    return;
  }

  if (nevents < 0) {
    selected->error = core::File::Error{errno};
    return;
  }

  selected->channels.resize(nevents);
  selected->events.resize(nevents);
  for (int i = 0; i < nevents; ++i) {
    selected->channels[i] = static_cast<Channel *>(buffer_[i].data.ptr);
    selected->events[i] = ReceiveEvents{buffer_[i].events};
  }
}
} // namespace pedronet