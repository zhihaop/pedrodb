#ifndef PEDRONET_ACCEPTOR_H
#define PEDRONET_ACCEPTOR_H

#include "pedronet/callbacks.h"
#include "pedronet/channel/socket_channel.h"
#include "pedronet/core/debug.h"
#include "pedronet/core/latch.h"
#include "pedronet/core/noncopyable.h"
#include "pedronet/core/nonmovable.h"
#include "pedronet/event.h"
#include "pedronet/socket.h"

#include <algorithm>

namespace pedronet {

using AcceptorCallback = std::function<void(Socket)>;

class Acceptor : core::noncopyable, core::nonmovable {
public:
  struct Option {
    bool reuse_addr = true;
    bool reuse_port = true;
    bool keep_alive = true;
    bool tcp_no_delay = true;
  };

protected:
  AcceptorCallback acceptor_callback_;
  InetAddress address_;

  SocketChannel channel_;
  EventLoop &eventloop_;

public:
  Acceptor(EventLoop &eventloop, const InetAddress &address,
           const Option &option);

  ~Acceptor() { Close(); }

  void Bind() { channel_.File().Bind(address_); }

  void OnAccept(AcceptorCallback acceptor_callback) {
    acceptor_callback_ = std::move(acceptor_callback);
  }

  void Listen();

  void Close();

  std::string String() const;
};
} // namespace pedronet

PEDRONET_CLASS_FORMATTER(pedronet::Acceptor)

#endif // PEDRONET_ACCEPTOR_H