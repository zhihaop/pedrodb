#include "pedronet/inetaddress.h"
#include "pedronet/inetaddress_impl.h"
#include "pedronet/logger/logger.h"

namespace pedronet {

InetAddress::InetAddress() : impl_(std::make_unique<InetAddressImpl>()) {}
InetAddress::~InetAddress() = default;

InetAddress::InetAddress(const InetAddress& other) : host_(other.host_) {
  if (other.impl_) {
    impl_ = std::make_unique<InetAddressImpl>();
    memcpy(impl_.get(), other.impl_.get(), sizeof(InetAddressImpl));
  }
}
InetAddress::InetAddress(InetAddress&& other) noexcept
    : impl_(std::move(other.impl_)), host_(std::move(other.host_)) {}

bool InetAddress::operator==(const InetAddress& other) const noexcept {
  return Port() == other.Port() && host_ == other.host_;
}

InetAddress& InetAddress::operator=(const InetAddress& other) {
  if (this == &other) {
    return *this;
  }
  if (other.impl_) {
    if (impl_ == nullptr) {
      impl_ = std::make_unique<InetAddressImpl>();
    }
    memcpy(impl_.get(), other.impl_.get(), sizeof(InetAddressImpl));
  }
  host_ = other.host_;
  return *this;
}

InetAddress& InetAddress::operator=(InetAddress&& other) noexcept {
  if (this == &other) {
    return *this;
  }
  impl_ = std::move(other.impl_);
  host_ = std::move(other.host_);
  return *this;
}

int InetAddress::Family() const {
  return impl_->family();
}

InetAddress InetAddress::Create(const std::string& host, uint16_t port) {
  auto impl = std::make_unique<InetAddressImpl>();
  impl->in4.sin_family = AF_INET;
  impl->in4.sin_port = htobe16(port);
  if (::inet_pton(AF_INET, host.c_str(), &impl->in4.sin_addr) <= 0) {
    PEDRONET_FATAL("incorrect host[{}]: {}", strerror(errno));
  }
  return InetAddress{std::move(impl)};
}

InetAddress InetAddress::CreateV6(const std::string& host, uint16_t port) {
  auto impl = std::make_unique<InetAddressImpl>();
  impl->in6.sin6_family = AF_INET6;
  impl->in6.sin6_port = htobe16(port);
  if (::inet_pton(AF_INET6, host.c_str(), &impl->in6.sin6_addr) <= 0) {
    PEDRONET_FATAL("incorrect host[{}]: {}", strerror(errno));
  }
  return InetAddress{std::move(impl)};
}

bool InetAddress::IPv6() const noexcept {
  return impl_->family() == AF_INET6;
}

uint16_t InetAddress::Port() const noexcept {
  if (impl_ == nullptr) {
    return 0;
  }
  return impl_->port();
}

std::string InetAddress::String() const noexcept {
  return fmt::format("InetAddress[{}:{}]", host_, Port());
}

InetAddress::InetAddress(std::unique_ptr<InetAddressImpl> impl)
    : impl_(std::move(impl)), host_(impl_->host()) {}

}  // namespace pedronet