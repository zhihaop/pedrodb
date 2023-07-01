#ifndef PEDRONET_CHANNEL_SOCKET_CHANNEL_H
#define PEDRONET_CHANNEL_SOCKET_CHANNEL_H

#include "pedronet/channel/channel.h"
#include "pedronet/core/debug.h"
#include "pedronet/core/latch.h"
#include "pedronet/eventloop.h"
#include "pedronet/socket.h"

namespace pedronet {

class SocketChannel final : public Channel {
protected:
  SelectEvents events_{SelectEvents::kNoneEvent};

  SelectorCallback close_callback_;
  SelectorCallback read_callback_;
  SelectorCallback error_callback_;
  SelectorCallback write_callback_;

  Selector *selector_{};
  Socket socket_;

public:
  explicit SocketChannel(Socket socket) : socket_(std::move(socket)) {}

  ~SocketChannel() override = default;

  void SetSelector(Selector *selector) { selector_ = selector; }

  void OnRead(SelectorCallback read_callback) {
    read_callback_ = std::move(read_callback);
  }

  void OnClose(SelectorCallback close_callback) {
    close_callback_ = std::move(close_callback);
  }

  void OnWrite(SelectorCallback write_callback) {
    write_callback_ = std::move(write_callback);
  }

  void OnError(SelectorCallback error_callback) {
    error_callback_ = std::move(error_callback);
  }

  void HandleEvents(ReceiveEvents events, core::Timestamp now) final;

  void SetReadable(bool on);

  bool Readable() const noexcept {
    return events_.Contains(SelectEvents::kReadEvent);
  }

  bool Writable() const noexcept {
    return events_.Contains(SelectEvents::kWriteEvent);
  }

  void SetWritable(bool on);

  Socket &File() noexcept final { return socket_; }
  const Socket &File() const noexcept final { return socket_; }

  std::string String() const override {
    return fmt::format("SocketChannel[fd={}]", socket_.Descriptor());
  }

  InetAddress GetLocalAddress() const noexcept {
    return socket_.GetLocalAddress();
  }

  InetAddress GetPeerAddress() const noexcept {
    return socket_.GetPeerAddress();
  }

  ssize_t Write(const void *buf, size_t n) { return socket_.Write(buf, n); }

  ssize_t Read(void *buf, size_t n) { return socket_.Read(buf, n); }

  Socket::Error GetError() const { return socket_.GetError(); }

  void CloseWrite() { return socket_.CloseWrite(); }

  void Shutdown() { return socket_.Shutdown(); }
};

} // namespace pedronet

PEDRONET_CLASS_FORMATTER(pedronet::SocketChannel);
#endif // PEDRONET_CHANNEL_SOCKET_CHANNEL_H