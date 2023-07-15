#include "pedronet/socket.h"
#include <netinet/tcp.h>
#include <csignal>
#include "pedronet/inetaddress_impl.h"
#include "pedronet/logger/logger.h"

namespace pedronet {

namespace {

struct OnInitialStartUp {
  OnInitialStartUp() { ::signal(SIGPIPE, SIG_IGN); }
} initialStartUp;
}  // namespace

Socket Socket::Create(int family, bool nonblocking) {
  int type = SOCK_STREAM | SOCK_CLOEXEC;
  if (nonblocking) {
    type |= SOCK_NONBLOCK;
  }
  int protocol = IPPROTO_TCP;
  int fd = ::socket(family, type, protocol);
  if (fd < 0) {
    PEDRONET_FATAL("failed to call ::socket({}, {}, {}), reason[{}]", family,
                   type, protocol, Error{errno});
  }
  return Socket{fd};
}

void Socket::Bind(const InetAddress& address) {
  auto& impl = address.impl_;
  if (::bind(fd_, impl->data(), impl->size())) {
    PEDRONET_FATAL("Socket::Bind({}) failed: {}", address, GetError());
  }
}

void Socket::Listen() {
  PEDRONET_TRACE("call listen fd{}", fd_);
  if (::listen(fd_, SOMAXCONN) < 0) {
    PEDRONET_FATAL("failed to listen, errno[{}]", errno);
  }
}

Error Socket::Connect(const InetAddress& address) {
  auto& impl = address.impl_;
  if (::connect(fd_, impl->data(), impl->size())) {
    return GetError();
  }
  return Error::Success();
}

InetAddress Socket::GetLocalAddress() const {
  auto impl = std::make_unique<InetAddressImpl>();
  socklen_t len = impl->size();
  if (::getsockname(fd_, impl->data(), &len) < 0) {
    PEDRONET_ERROR("{}::GetLocalAddress() failed: {}", *this, GetError());
    return InetAddress{};
  }

  return InetAddress{std::move(impl)};
}

InetAddress Socket::GetPeerAddress() const {
  auto impl = std::make_unique<InetAddressImpl>();
  socklen_t len = impl->size();
  if (::getpeername(fd_, impl->data(), &len) < 0) {
    PEDRONET_ERROR("{}::GetPeerAddress() failed: {}", *this, GetError());
    return InetAddress{};
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
    PEDRONET_FATAL("failed to close write end");
  }
}

Error Socket::GetError() const noexcept {
  int val;
  auto len = static_cast<socklen_t>(sizeof val);
  if (::getsockopt(fd_, SOL_SOCKET, SO_ERROR, &val, &len) < 0) {
    return Error{errno};
  } else {
    return Error{val};
  }
}

Error Socket::Accept(const InetAddress& local, Socket* socket) {
  auto impl = std::make_unique<InetAddressImpl>();
  auto len = impl->size();

  Socket file{::accept4(fd_, impl->data(), &len, SOCK_NONBLOCK | SOCK_CLOEXEC)};
  if (!file.Valid()) {
    return Error{errno};
  }

  PEDRONET_TRACE("Socket::Accept() {} {} -> {}", file, local,
                 file.GetPeerAddress());
  *socket = std::move(file);
  return Error::Success();
}

std::string Socket::String() const {
  return fmt::format("Socket[fd={}]", fd_);
}


Socket& Socket::operator=(Socket&& other) noexcept {
  if (this == &other) {
    return *this;
  }

  File::Close();
  std::swap(fd_, other.fd_);
  return *this;
}

Socket::~Socket() {
  if (fd_ != kInvalid) {
    PEDRONET_TRACE("{}::~Socket()", *this);
  }
}

void Socket::Shutdown() {
  if (::shutdown(fd_, SHUT_RDWR) < 0) {
    PEDRONET_FATAL("failed to close write end");
  }
}

}  // namespace pedronet