#include "pedronet/core/thread.h"
#include "pedronet/eventloop.h"
#include "pedronet/logger/logger.h"

#include <pthread.h>
namespace pedronet::core {

void Thread::BindEventLoop(const EventLoop* loop) {
  if (loop_ != nullptr) {
    PEDRONET_ERROR("event loop has been bound to this thread");
    return;
  }
  loop_ = loop;
}

void Thread::UnbindEventLoop(const EventLoop* loop) {
  if (!CheckUnderLoop(loop)) {
    PEDRONET_ERROR("don't unbind loops in other thread");
    return;
  }
  loop_ = nullptr;
}

Thread& Thread::Current() {
  static thread_local Thread thread;
  return thread;
}

void Thread::SetAlias(std::string name) {
  name_ = std::move(name);
  pthread_setname_np(pthread_self(), name_.c_str());
}
}  // namespace pedronet::core