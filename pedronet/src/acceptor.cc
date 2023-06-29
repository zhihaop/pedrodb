#include "pedronet/acceptor.h"

namespace pedronet {

Acceptor::Acceptor(EventLoop &eventloop, const InetAddress &address,
                   const Acceptor::Option &option)
    : address_(address), channel_(Socket::Create(address.Family())),
      eventloop_(eventloop) {
  spdlog::trace("Acceptor::Acceptor()");

  auto &socket = channel_.File();
  socket.SetReuseAddr(option.reuse_addr);
  socket.SetReusePort(option.reuse_port);
  socket.SetKeepAlive(option.keep_alive);
  socket.SetTcpNoDelay(option.tcp_no_delay);

  channel_.SetSelector(eventloop.GetSelector());

  channel_.OnRead([this](auto events, auto now) {
    while (true) {
      spdlog::trace("{}::HandleRead()", *this);
      Socket socket;
      Socket::Error err = channel_.File().Accept(address_, &socket);
      if (!err.Empty()) {
        if (err.GetCode() == EAGAIN || err.GetCode() == EWOULDBLOCK) {
          break;
        }
        spdlog::error("failed to accept [{}]", err);
        continue;
      }
      if (acceptor_callback_) {
        acceptor_callback_(std::move(socket));
      }
    }
  });
}
std::string Acceptor::String() const {
  return fmt::format("Acceptor[socket={}]", channel_.File());
}
void Acceptor::Close() {
  spdlog::trace("Acceptor::Close() enter");
  core::Latch latch(1);
  eventloop_.Submit([this, &latch] {
    channel_.SetReadable(false);
    channel_.SetWritable(false);
    eventloop_.Deregister(&channel_);
    latch.CountDown();
  });
  latch.Await();
  spdlog::trace("Acceptor::Close() exit");
}
void Acceptor::Listen() {
  eventloop_.Register(&channel_, [this] {
    channel_.SetReadable(true);
    channel_.File().Listen();
  });
}
} // namespace pedronet