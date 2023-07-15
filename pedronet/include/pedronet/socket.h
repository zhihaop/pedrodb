#ifndef PEDRONET_SOCKET_H
#define PEDRONET_SOCKET_H
#include "defines.h"
#include "pedronet/inetaddress.h"

namespace pedronet {

struct Option {
  bool reuse_addr = true;
  bool reuse_port = true;
  bool keep_alive = true;
  bool tcp_no_delay = true;
};

class Socket : public File {
  explicit Socket(int fd) : File(fd) {}

 public:
  Socket() : Socket(kInvalid) {}
  ~Socket() override;
  Socket(Socket&& other) noexcept : Socket(other.fd_) { other.fd_ = kInvalid; }
  Socket& operator=(Socket&& other) noexcept;
  static Socket Create(int family, bool nonblocking);

  void Bind(const InetAddress& address);
  Error Accept(const InetAddress& local, Socket* socket);
  void Listen();
  Error Connect(const InetAddress& address);

  void SetReuseAddr(bool on);
  void SetReusePort(bool on);
  void SetKeepAlive(bool on);
  void SetTcpNoDelay(bool on);

  [[nodiscard]] InetAddress GetLocalAddress() const;
  [[nodiscard]] InetAddress GetPeerAddress() const;

  void CloseWrite();

  void Shutdown();

  [[nodiscard]] Error GetError() const noexcept override;

  [[nodiscard]] std::string String() const override;
};
}  // namespace pedronet

PEDROLIB_CLASS_FORMATTER(pedronet::Socket);

#endif  // PEDRONET_SOCKET_H