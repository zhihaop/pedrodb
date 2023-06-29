#ifndef PEDRONET_CORE_THREAD_H
#define PEDRONET_CORE_THREAD_H

#include <optional>
#include <unistd.h>

namespace pedronet::core {
class Thread {
public:
  static pid_t GetID() noexcept {
    thread_local std::optional<pid_t> cache_pid;
    if (!cache_pid) {
      cache_pid = gettid();
    }
    return *cache_pid;
  }
};
} // namespace pedronet

#endif // PEDRONET_CORE_THREAD_H