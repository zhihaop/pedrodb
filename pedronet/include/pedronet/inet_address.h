#ifndef PEDRONET_INET_ADDRESS_H
#define PEDRONET_INET_ADDRESS_H

#include "channel_handler.h"
#include <endian.h>
#include <exception>
#include <fmt/format.h>
#include <netinet/in.h>
#include <spdlog/spdlog.h>
#include <string>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <variant>

namespace pedronet {

struct SocketAddrSlice {
  const struct sockaddr *addr;
  socklen_t len;
};

class InetAddress {
  std::variant<sockaddr_in, sockaddr_in6> addr_{};
  std::string host_;

  const auto &sockaddr_v6() const { return std::get<1>(addr_); }

  const auto &sockaddr_v4() const { return std::get<0>(addr_); }

public:
  InetAddress() = default;

  explicit InetAddress(const struct sockaddr_in &addr) : addr_(addr) {
    char buf[INET_ADDRSTRLEN];
    ::inet_ntop(AF_INET, &addr.sin_addr, buf, INET_ADDRSTRLEN);
    host_ = buf;
  }

  explicit InetAddress(const struct sockaddr_in6 &addr) : addr_(addr) {
    char buf[INET6_ADDRSTRLEN];
    ::inet_ntop(AF_INET, &addr.sin6_addr, buf, INET_ADDRSTRLEN);
    host_ = buf;
  }

  static InetAddress Create(const std::string &host, uint16_t port) {
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htobe16(port);
    if (::inet_pton(AF_INET, host.c_str(), &addr.sin_addr) <= 0) {
      spdlog::error("incorrect host format {}", host);
      std::terminate();
    }
    return InetAddress{addr};
  }

  static InetAddress CreateV6(const std::string &host, uint16_t port) {
    struct sockaddr_in6 addr;
    addr.sin6_family = AF_INET6;
    addr.sin6_port = htobe16(port);
    if (::inet_pton(AF_INET6, host.c_str(), &addr.sin6_addr) <= 0) {
      spdlog::error("incorrect host format {}", host);
      std::terminate();
    }
    return InetAddress{addr};
  }

  sa_family_t Family() const noexcept {
    if (IPv6()) {
      return sockaddr_v6().sin6_family;
    } else {
      return sockaddr_v4().sin_family;
    }
  }

  bool IPv6() const noexcept { return addr_.index(); }

  int Port() const noexcept {
    if (IPv6()) {
      return htobe16(sockaddr_v6().sin6_port);
    } else {
      return htobe16(sockaddr_v4().sin_port);
    }
  }

  const std::string &Host() const noexcept { return host_; }

  SocketAddrSlice SockAddr() const noexcept {
    SocketAddrSlice slice{};
    if (IPv6()) {
      slice.addr = reinterpret_cast<const struct sockaddr *>(&sockaddr_v6());
      slice.len = static_cast<socklen_t>(sizeof(struct sockaddr_in6));
    } else {
      slice.addr = reinterpret_cast<const struct sockaddr *>(&sockaddr_v4());
      slice.len = static_cast<socklen_t>(sizeof(struct sockaddr_in));
    }
    return slice;
  }

  std::string String() const noexcept {
    return fmt::format("{}:{}", host_, Port());
  }
};
} // namespace pedronet

#endif // PEDRONET_INET_ADDRESS_H