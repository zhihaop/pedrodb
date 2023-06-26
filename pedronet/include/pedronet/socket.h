#ifndef PEDRONET_SOCKET_H
#define PEDRONET_SOCKET_H
#include "inet_address.h"
#include "pedronet/core/file.h"
#include "pedronet/inet_address.h"

namespace pedronet {

class TcpConnection;
using TcpConnectionPtr = std::shared_ptr<TcpConnection>;

class Socket : public core::File {
  Socket(int fd) : core::File(fd) {}

public:
  static Socket Create(int family);

  void Bind(const InetAddress &address);
  Error Accept(const InetAddress& local, TcpConnectionPtr *conn);
  void Listen();
  Error Connect(const InetAddress &address);

  void SetReuseAddr(bool on);
  void SetReusePort(bool on);
  void SetKeepAlive(bool on);
  void SetTcpNoDelay(bool on);

  InetAddress GetLocalAddress() const;
  InetAddress GetPeerAddress() const;

  void CloseWrite();

  Error GetError() const noexcept override;
};
} // namespace pedronet

#endif // PEDRONET_SOCKET_H