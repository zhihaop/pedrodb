#ifndef PEDRONET_INET_ADDRESS_H
#define PEDRONET_INET_ADDRESS_H

#include "pedronet/core/debug.h"
#include <memory>
#include <string>

struct sockaddr_in;
struct sockaddr_in6;
namespace pedronet {

class Socket;
class InetAddressImpl;

class InetAddress {
  friend class Socket;

  std::unique_ptr<InetAddressImpl> impl_;

  std::string host_;

  InetAddress(std::unique_ptr<InetAddressImpl> impl, std::string host);

  explicit InetAddress(const struct sockaddr_in &addr);
  explicit InetAddress(const struct sockaddr_in6 &addr);

  std::string_view data() const;

public:
  InetAddress();
  ~InetAddress();
  InetAddress(const InetAddress &other);
  InetAddress(InetAddress &&other) noexcept;
  bool operator==(const InetAddress &other) const noexcept;
  InetAddress &operator=(const InetAddress &other);
  InetAddress &operator=(InetAddress &&other);

  static InetAddress Create(const std::string &host, uint16_t port);

  static InetAddress CreateV6(const std::string &host, uint16_t port);

  bool IPv6() const noexcept;
  int Family() const;
  uint16_t Port() const noexcept;

  const std::string &Host() const noexcept { return host_; }

  std::string String() const noexcept {
    return fmt::format("{}:{}", host_, Port());
  }
};
} // namespace pedronet

PEDRONET_FORMATABLE_CLASS(pedronet::InetAddress);
#endif // PEDRONET_INET_ADDRESS_H