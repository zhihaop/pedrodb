#include "pedronet/socket.h"
#include "pedronet/tcp_connection.h"
#include <cerrno>

#include "pedronet/core/debug.h"
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>

namespace pedronet {

Socket Socket::Create(int family) {
  int type = SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC;
  int protocol = IPPROTO_TCP;
  int fd = ::socket(family, type, protocol);
  if (fd < 0) {
    spdlog::error("failed to call ::socket({}, {}, {}), reason[{}]", family, type,
                  protocol, Socket::Error{errno});
    std::terminate();
  }
  return Socket{fd};
}

void Socket::Bind(const InetAddress &address) {
  auto addr = address.data();
  int ret = ::bind(fd_, reinterpret_cast<const struct sockaddr *>(addr.data()),
                   addr.size());
  if (ret < 0) {
    spdlog::error("failed to bind address {}:{}", address.Host(),
                  address.Port());
    std::terminate();
  }
}

void Socket::Listen() {
  spdlog::info("call listen fd{}", fd_);
  if (::listen(fd_, SOMAXCONN) < 0) {
    spdlog::error("failed to listen, errno[{}]", errno);
    std::terminate();
  }
}

Socket::Error Socket::Connect(const InetAddress &address) {
  auto addr = address.data();
  if (::connect(fd_, reinterpret_cast<const struct sockaddr *>(addr.data()),
                addr.size())) {
    return Error{errno};
  }
  return Error{};
}

InetAddress Socket::GetLocalAddress() const {
  struct sockaddr_in6 addr {};
  socklen_t len = static_cast<socklen_t>(sizeof(sockaddr_in6));

  if (::getsockname(fd_, reinterpret_cast<struct sockaddr *>(&addr), &len) <
      0) {
    spdlog::error("failed to get local address, {}", GetError());
  }

  return InetAddress{addr};
}

InetAddress Socket::GetPeerAddress() const {
  struct sockaddr_in6 addr {};
  socklen_t len = static_cast<socklen_t>(sizeof(sockaddr_in6));

  if (::getpeername(fd_, reinterpret_cast<struct sockaddr *>(&addr), &len) <
      0) {
    spdlog::error("failed to get local address, reason[{}]",
                  GetError().GetReason());
  }

  return InetAddress{addr};
}

void Socket::SetReuseAddr(bool on) {
  int val = on ? 1 : 0;
  ::setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));
}

void Socket::SetReusePort(bool on) {
  int val = on ? 1 : 0;
  ::setsockopt(fd_, SOL_SOCKET, SO_REUSEPORT, &val, sizeof(val));
}
void Socket::SetKeepAlive(bool on) {
  int val = on ? 1 : 0;
  ::setsockopt(fd_, SOL_SOCKET, SO_KEEPALIVE, &val, sizeof(val));
}

void Socket::SetTcpNoDelay(bool on) {
  int val = on ? 1 : 0;
  ::setsockopt(fd_, IPPROTO_TCP, TCP_NODELAY, &val, sizeof(val));
}

void Socket::CloseWrite() {
  if (::shutdown(fd_, SHUT_WR) < 0) {
    spdlog::error("failed to close write end");
    std::terminate();
  }
}

Socket::Error Socket::GetError() const noexcept {
  int optval{};
  socklen_t optlen = static_cast<socklen_t>(sizeof(optlen));
  if (::getsockopt(fd_, SOL_SOCKET, SO_ERROR, &optval, &optlen) < 0) {
    return File::Error{errno};
  }
  return File::Error{optval};
}

Socket::Error Socket::Accept(const InetAddress &local, TcpConnectionPtr *conn) {
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
    return Error{errno};
  }
  *conn = std::make_shared<TcpConnection>(Socket{fd});
  return Error{};
}
} // namespace pedronet