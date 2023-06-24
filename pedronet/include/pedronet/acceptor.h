#ifndef PEDRONET_ACCEPTOR_H
#define PEDRONET_ACCEPTOR_H

#include "core/latch.h"
#include "channel.h"
#include "socket.h"

#include <algorithm>
#include <fmt/format.h>
#include <spdlog/spdlog.h>

namespace pedronet {

class Acceptor : public AbstractChannel<Acceptor> {
public:
  using NewConnectionCallback = std::function<void(Acception)>;

  struct Option {
    bool reuse_addr = true;
    bool reuse_port = true;
    bool keep_alive = true;
    bool tcp_no_delay = true;
  };

  enum class State {
    kListening,
    kClosed,
  };

protected:
  NewConnectionCallback new_connection_cb_{};
  State state_{Acceptor::State::kClosed};
  InetAddress address_;
  Socket socket_;

public:
  Acceptor(const InetAddress &address, const Option &option)
      : AbstractChannel(), address_(address), socket_(Socket::Create(address)) {
    spdlog::error("create acceptor");

    socket_.SetReuseAddr(option.reuse_addr);
    socket_.SetReusePort(option.reuse_port);
    socket_.SetKeepAlive(option.keep_alive);
    socket_.SetTcpNoDelay(option.tcp_no_delay);
  }

  ~Acceptor() override { Close(); }

  core::File &File() noexcept override { return socket_; }

  const InetAddress &ListenAddress() const noexcept { return address_; }

  void Bind() { socket_.Bind(address_); }

  void Listen(NewConnectionCallback cb) {
    state_ = State::kListening;
    new_connection_cb_ = std::move(cb);
    this->EnableEvent(Selector::kReadEvent);
    socket_.Listen();
  }

  State GetState() const noexcept { return state_; }

  void Close() {
    core::Latch latch(1);
    loop_->Submit([&] {
      SetEvents(Selector::kNoneEvent);
      Detach({});
      socket_.Close();
      latch.CountDown();
    });
    latch.Await();
  }

  void HandleAccept(core::Timestamp now) {
    while (!loop_->Closed()) {
      spdlog::info("invoke accept");
      Acception acc = socket_.Accept();
      if (!acc.file.Valid()) {
        if (acc.err == EAGAIN || acc.err == EWOULDBLOCK) {
          break;
        }
        spdlog::error("failed to accept {}:{}", acc.peer.Host(),
                      acc.peer.Port());
        continue;
      }
      new_connection_cb_(std::move(acc));
    }
  }

  void HandleRead(ReceiveEvent events, core::Timestamp now) override {
    loop_->AssertInsideLoop();
    HandleAccept(now);
    spdlog::info("handle read acceptor");
  }

  std::string String() override {
    return fmt::format("Acceptor[listen={}, state={}]", address_.String(), 0);
  }
};
} // namespace pedronet

#endif // PEDRONET_ACCEPTOR_H