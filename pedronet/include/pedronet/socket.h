#ifndef PEDRONET_SOCKET_H
#define PEDRONET_SOCKET_H
#include "core/file.h"
#include "inet_address.h"
#include <asm-generic/socket.h>
#include <cerrno>
#include <cstring>
#include <exception>
#include <memory>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <optional>
#include <spdlog/spdlog.h>
#include <sys/socket.h>
#include <unistd.h>

namespace pedronet {

class Socket;
struct Acception;

class Socket : public core::File {
  Socket(int fd) : core::File(fd) {}

public:
  static Socket Create(const InetAddress &addr) {
    int fd = ::socket(addr.Family(), SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC,
                      IPPROTO_TCP);
    if (fd < 0) {
      spdlog::error("failed to call ::socket");
      std::terminate();
    }
    return Socket{fd};
  }

  void Bind(const InetAddress &address) {
    SocketAddrSlice slice = address.SockAddr();
    int ret = ::bind(fd_, slice.addr, slice.len);
    if (ret < 0) {
      spdlog::error("failed to bind address {}:{}", address.Host(),
                    address.Port());
      std::terminate();
    }
  }

  Acception Accept();

  void Listen() {
    spdlog::info("call listen fd{}", fd_);
    if (::listen(fd_, SOMAXCONN) < 0) {
      spdlog::error("failed to listen, errno[{}]", errno);
      std::terminate();
    }
  }

  void SetReuseAddr(bool on) {
    int val = on ? 1 : 0;
    ::setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));
  }

  void SetReusePort(bool on) {
    int val = on ? 1 : 0;
    ::setsockopt(fd_, SOL_SOCKET, SO_REUSEPORT, &val, sizeof(val));
  }
  void SetKeepAlive(bool on) {
    int val = on ? 1 : 0;
    ::setsockopt(fd_, SOL_SOCKET, SO_KEEPALIVE, &val, sizeof(val));
  }

  void SetTcpNoDelay(bool on) {
    int val = on ? 1 : 0;
    ::setsockopt(fd_, IPPROTO_TCP, TCP_NODELAY, &val, sizeof(val));
  }

  void CloseWrite() {
    if (::shutdown(fd_, SHUT_WR) < 0) {
      spdlog::error("failed to close write end");
      std::terminate();
    }
  }

  File::Error GetError() const noexcept override {
    int optval{};
    socklen_t optlen = static_cast<socklen_t>(sizeof(optlen));
    if (::getsockopt(fd_, SOL_SOCKET, SO_ERROR, &optval, &optlen) < 0) {
      return File::Error{errno};
    }
    return File::Error{optval};
  }
};

struct Acception {
  InetAddress peer;
  Socket file;
  int err;
};

inline Acception Socket::Accept() {
  union {
    struct sockaddr_in6 v6;
    struct sockaddr_in v4;
  } addr;

  auto addr_size = static_cast<socklen_t>(sizeof(addr));
  int fd = ::accept4(fd_, reinterpret_cast<struct sockaddr *>(&addr),
                     &addr_size, SOCK_NONBLOCK | SOCK_CLOEXEC);

  InetAddress address;
  if (addr.v4.sin_family == AF_INET) {
    address = InetAddress{addr.v4};
  } else if (addr.v6.sin6_family == AF_INET6) {
    address = InetAddress{addr.v6};
  }

  if (fd <= 0) {
    return Acception{std::move(address), {-1}, errno};
  }
  return Acception{std::move(address), Socket{fd}, 0};
}
} // namespace pedronet

#endif // PEDRONET_SOCKET_H