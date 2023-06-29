#include "pedronet/socket.h"
#include "pedronet/core/debug.h"
#include "pedronet/inetaddress_impl.h"
#include <netinet/tcp.h>

namespace pedronet {

Socket Socket::Create(int family) {
  int type = SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC;
  int protocol = IPPROTO_TCP;
  int fd = ::socket(family, type, protocol);
  if (fd < 0) {
    spdlog::error("failed to call ::socket({}, {}, {}), reason[{}]", family,
                  type, protocol, Socket::Error{errno});
    std::terminate();
  }
  return Socket{fd};
}

void Socket::Bind(const InetAddress &address) {
  auto &impl = address.impl_;
  if (::bind(fd_, impl->data(), impl->size())) {
    spdlog::error("Socket::Bind({}) failed: {}", address, GetError());
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
  auto &impl = address.impl_;
  if (::connect(fd_, impl->data(), impl->size())) {
    return GetError();
  }
  return Error::Success();
}

InetAddress Socket::GetLocalAddress() const {
  auto impl = std::make_unique<InetAddressImpl>();
  socklen_t len = impl->size();
  if (::getsockname(fd_, impl->data(), &len) < 0) {
    spdlog::error("{}::GetLocalAddress() failed: {}", *this, GetError());
    std::terminate();
  }

  return InetAddress{std::move(impl)};
}

InetAddress Socket::GetPeerAddress() const {
  auto impl = std::make_unique<InetAddressImpl>();
  socklen_t len = impl->size();
  if (::getpeername(fd_, impl->data(), &len) < 0) {
    spdlog::error("{}::GetPeerAddress() failed: {}", *this, GetError());
    std::terminate();
  }

  return InetAddress{std::move(impl)};
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
  int val{};
  auto len = static_cast<socklen_t>(sizeof(int));
  if (::getsockopt(fd_, SOL_SOCKET, SO_ERROR, &val, &len) < 0) {
    return File::Error{errno};
  }
  return File::Error{val};
}

Socket::Error Socket::Accept(const InetAddress &local, Socket *socket) {
  auto impl = std::make_unique<InetAddressImpl>();
  auto len = impl->size();
  
  Socket file{::accept4(fd_, impl->data(), &len, SOCK_NONBLOCK | SOCK_CLOEXEC)};
  if (!file.Valid()) {
    return Error{errno};
  }
  
  spdlog::info("Socket::Accept() {} {} -> {}", file, local, file.GetPeerAddress());
  *socket = std::move(file);
  return Error::Success();
}
std::string Socket::String() const { return fmt::format("Socket[fd={}]", fd_); }
} // namespace pedronet