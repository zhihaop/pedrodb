#ifndef PEDRONET_INETADDRESS_IMPL_H
#define PEDRONET_INETADDRESS_IMPL_H

#include <arpa/inet.h>
#include <bits/sockaddr.h>
#include <netinet/in.h>
#include <string>

namespace pedronet {
union InetAddressImpl {
  struct sockaddr_in6 in6;
  struct sockaddr_in in4;

  [[nodiscard]] sa_family_t family() const noexcept { return in4.sin_family; }
  [[nodiscard]] uint16_t port() const noexcept {
    if (family() == AF_INET) {
      return htobe16(in4.sin_port);
    } else {
      return htobe16(in6.sin6_port);
    }
  }
  [[nodiscard]] std::string host() const noexcept {
    if (family() == AF_INET) {
      char buf[INET_ADDRSTRLEN];
      inet_ntop(AF_INET, &this->in4.sin_addr, buf, sizeof(buf));
      return buf;
    } else {
      char buf[INET6_ADDRSTRLEN];
      inet_ntop(AF_INET6, &this->in6.sin6_addr, buf, sizeof(buf));
      return buf;
    }
  }
  [[nodiscard]] socklen_t size() const noexcept {
    if (family() == AF_INET) {
      return sizeof(struct sockaddr_in);
    } else {
      return sizeof(struct sockaddr_in6);
    }
  }
  struct sockaddr* data() noexcept {
    return reinterpret_cast<struct sockaddr*>(this);
  }
};
}  // namespace pedronet

#endif  // PEDRONET_INETADDRESS_IMPL_H
