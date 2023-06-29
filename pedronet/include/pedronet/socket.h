#ifndef PEDRONET_SOCKET_H
#define PEDRONET_SOCKET_H
#include "pedronet/core/file.h"
#include "pedronet/inetaddress.h"

namespace pedronet {

class Socket : public core::File {
  explicit Socket(int fd) : core::File(fd) {}

public:
  Socket() : Socket(kInvalid) {}
  static Socket Create(int family);

  void Bind(const InetAddress &address);
  Error Accept(const InetAddress &local, Socket *socket);
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

  std::string String() const override;
};
} // namespace pedronet

PEDRONET_FORMATABLE_CLASS(pedronet::Socket)

#endif // PEDRONET_SOCKET_H