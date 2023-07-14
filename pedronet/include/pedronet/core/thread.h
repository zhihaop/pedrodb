#ifndef PEDRONET_CORE_THREAD_H
#define PEDRONET_CORE_THREAD_H

#include <string>

namespace pedronet {
class EventLoop;
namespace core {
class Thread {
  const EventLoop* loop_{};
  std::string name_;

 public:
  static Thread& Current();
  void BindEventLoop(const EventLoop* loop);
  void UnbindEventLoop(const EventLoop* loop);
  bool CheckUnderLoop(const EventLoop* loop) const noexcept {
    return loop == loop_;
  }
  void SetAlias(std::string name);
  const std::string& GetAlias() const noexcept { return name_; }
};
}  // namespace core
}  // namespace pedronet

#endif  // PEDRONET_CORE_THREAD_H