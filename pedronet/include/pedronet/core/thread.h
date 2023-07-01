#ifndef PEDRONET_CORE_THREAD_H
#define PEDRONET_CORE_THREAD_H

#include <optional>
#include <unistd.h>

namespace pedronet::core {
class Thread {
public:
  static pid_t GetID() noexcept;
};
} // namespace pedronet

#endif // PEDRONET_CORE_THREAD_H