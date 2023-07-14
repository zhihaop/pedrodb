#ifndef PEDRONET_INETADDRESS_H
#define PEDRONET_INETADDRESS_H

#include <pedrolib/format/formatter.h>
#include <memory>
#include <string>

namespace pedronet {

class Socket;
union InetAddressImpl;

class InetAddress {
  friend class Socket;

  std::unique_ptr<InetAddressImpl> impl_;
  std::string host_;

  explicit InetAddress(std::unique_ptr<InetAddressImpl> impl);

 public:
  InetAddress();
  ~InetAddress();
  InetAddress(const InetAddress& other);
  InetAddress(InetAddress&& other) noexcept;
  bool operator==(const InetAddress& other) const noexcept;
  InetAddress& operator=(const InetAddress& other);
  InetAddress& operator=(InetAddress&& other) noexcept;

  static InetAddress Create(const std::string& host, uint16_t port);

  static InetAddress CreateV6(const std::string& host, uint16_t port);

  bool IPv6() const noexcept;
  int Family() const;
  uint16_t Port() const noexcept;

  const std::string& Host() const noexcept { return host_; }

  std::string String() const noexcept;
};
}  // namespace pedronet

PEDROLIB_CLASS_FORMATTER(pedronet::InetAddress);
#endif  // PEDRONET_INETADDRESS_H