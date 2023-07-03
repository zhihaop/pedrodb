#ifndef PEDRONET_SOCKET_H
#define PEDRONET_SOCKET_H
#include "defines.h"
#include "pedronet/inetaddress.h"

namespace pedronet {

class Socket : public File {
  explicit Socket(int fd) : File(fd) {}

public:
  Socket() : Socket(kInvalid) {}
  ~Socket() override;
  Socket(Socket &&other) noexcept : Socket(other.fd_) { other.fd_ = kInvalid; }
  Socket &operator=(Socket &&other) noexcept;
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

  void Shutdown();

  Error GetError() const noexcept override;

  std::string String() const override;

  ssize_t Write(const void *buf, size_t size) noexcept override;
};
} // namespace pedronet

PEDROLIB_CLASS_FORMATTER(pedronet::Socket);

#endif // PEDRONET_SOCKET_H