#include "pedronet/core/thread.h"

namespace pedronet::core {
pid_t Thread::GetID() noexcept {
  thread_local std::optional<pid_t> cache_pid;
  if (!cache_pid) {
    cache_pid = gettid();
  }
  return *cache_pid;
}
} // namespace pedronet::core