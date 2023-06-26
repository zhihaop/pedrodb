#include "pedronet/inet_address.h"
#include <arpa/inet.h>
#include <memory>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "pedronet/core/debug.h"

#include <variant>
namespace pedronet {

struct InetAddressImpl {
  std::variant<sockaddr_in, sockaddr_in6> data;

  template <typename T> explicit InetAddressImpl(T value) : data(value) {}

  const struct sockaddr_in6 &Addr6() const { return std::get<1>(data); }
  const struct sockaddr_in &Addr4() const { return std::get<0>(data); }
  bool IPv6() const noexcept { return data.index(); }

  sa_family_t Family() const noexcept {
    if (IPv6()) {
      return Addr6().sin6_family;
    } else {
      return Addr4().sin_family;
    }
  }

  std::string_view Data() const {
    if (IPv6()) {
      auto buf = reinterpret_cast<const char *>(&Addr6());
      return {buf, sizeof(struct sockaddr_in6)};
    } else {
      auto buf = reinterpret_cast<const char *>(&Addr4());
      return {buf, sizeof(struct sockaddr_in)};
    }
  }
};

InetAddress::InetAddress() {}
InetAddress::~InetAddress() {}
InetAddress::InetAddress(std::unique_ptr<InetAddressImpl> impl,
                         std::string host)
    : impl_(std::move(impl)), host_(std::move(host)) {}

InetAddress::InetAddress(const struct sockaddr_in &addr) {
  char buf[INET_ADDRSTRLEN];
  inet_ntop(AF_INET, &addr, buf, sizeof(buf));
  impl_ = std::make_unique<InetAddressImpl>(addr);
  host_ = buf;
}

InetAddress::InetAddress(const struct sockaddr_in6 &addr) {
  char buf[INET6_ADDRSTRLEN];
  inet_ntop(AF_INET6, &addr, buf, sizeof(buf));
  impl_ = std::make_unique<InetAddressImpl>(addr);
  host_ = buf;
}

InetAddress::InetAddress(const InetAddress &other) : host_(other.host_) {
  if (other.impl_) {
    impl_ = std::make_unique<InetAddressImpl>(other.impl_->data);
  }
}
InetAddress::InetAddress(InetAddress &&other) noexcept
    : impl_(std::move(other.impl_)), host_(std::move(other.host_)) {}

bool InetAddress::operator==(const InetAddress &other) const noexcept {
  return Port() == other.Port() && host_ == other.host_;
}

InetAddress &InetAddress::operator=(const InetAddress &other) {
  if (this == &other) {
    return *this;
  }
  if (other.impl_) {
    if (impl_ == nullptr) {
      impl_ = std::make_unique<InetAddressImpl>(other.impl_->data);
    } else {
      impl_->data = other.impl_->data;
    }
  }
  host_ = other.host_;
  return *this;
}

InetAddress &InetAddress::operator=(InetAddress &&other) {
  if (this == &other) {
    return *this;
  }
  impl_ = std::move(other.impl_);
  host_ = std::move(other.host_);
  return *this;
}

std::string_view InetAddress::data() const { return impl_->Data(); }
int InetAddress::Family() const { return impl_->Family(); }

InetAddress InetAddress::Create(const std::string &host, uint16_t port) {
  struct sockaddr_in addr;
  addr.sin_family = AF_INET;
  addr.sin_port = htobe16(port);
  if (::inet_pton(AF_INET, host.c_str(), &addr.sin_addr) <= 0) {
    spdlog::error("incorrect host format {}", host);
    std::terminate();
  }
  return {std::make_unique<InetAddressImpl>(addr), host};
}

InetAddress InetAddress::CreateV6(const std::string &host, uint16_t port) {
  struct sockaddr_in6 addr;
  addr.sin6_family = AF_INET6;
  addr.sin6_port = htobe16(port);
  if (::inet_pton(AF_INET6, host.c_str(), &addr.sin6_addr) <= 0) {
    spdlog::error("incorrect host format {}", host);
    std::terminate();
  }
  return {std::make_unique<InetAddressImpl>(addr), host};
}

bool InetAddress::IPv6() const noexcept { return impl_ && impl_->IPv6(); }

uint16_t InetAddress::Port() const noexcept {
  if (!impl_) {
    return 0;
  }
  if (IPv6()) {
    return htobe16(impl_->Addr6().sin6_port);
  } else {
    return htobe16(impl_->Addr4().sin_port);
  }
}

} // namespace pedronet