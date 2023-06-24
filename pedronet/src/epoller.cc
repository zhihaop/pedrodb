#include "pedronet/epoller.h"
#include <memory>
#include <sys/epoll.h>

namespace pedronet {
const uint32_t Epoller::kAdd = EPOLL_CTL_ADD;
const uint32_t Epoller::kDel = EPOLL_CTL_DEL;
const uint32_t Epoller::kMod = EPOLL_CTL_MOD;

inline static core::File CreateEpollFile() {
  int fd = ::epoll_create1(EPOLL_CLOEXEC);
  if (fd <= 0) {
    spdlog::error("failed to create epoll fd, errno[{}]", errno);
    std::terminate();
  }
  return core::File{fd};
}

inline static struct epoll_event *CreateBuffer(size_t size) {
  return static_cast<struct epoll_event *>(
      std::calloc(sizeof(struct epoll_event), size));
}

Epoller::Epoller(size_t size)
    : core::File(CreateEpollFile()), buffer_(CreateBuffer(size), free),
      buffer_size_(size) {}

void Epoller::Update(Channel *channel, uint32_t op, SelectorEvents events) {
  struct epoll_event ev {};
  ev.events = events.Value();
  ev.data.ptr = channel;

  int fd = channel->File().Descriptor();
  if (::epoll_ctl(fd_, op, fd, op == kDel ? nullptr : &ev) < 0) {
    spdlog::error("failed to call epoll_ctl, reason[{}]", errno);
    std::terminate();
  }
}

void Epoller::Wait(core::Duration timeout, Selected *selected) {
  struct epoll_event *buffer = buffer_.get();
  int nevents = ::epoll_wait(fd_, buffer, buffer_size_, timeout.Milliseconds());
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
    selected->channels[i] = static_cast<Channel *>(buffer[i].data.ptr);
    selected->events[i] = buffer[i].events;
  }
}

} // namespace pedronet