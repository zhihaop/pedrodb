#ifndef PEDRONET_CORE_ERROR_H
#define PEDRONET_CORE_ERROR_H

#include <pedrolib/format/formatter.h>
#include <string>

namespace pedronet {
namespace core {
class Error {
  int code_{};

public:
  explicit Error(int code) : code_(code) {}

  static Error Success() { return {}; }

  Error() = default;
  ~Error() = default;
  Error(const Error &) = default;
  Error &operator=(const Error &) = default;

  bool Empty() const noexcept { return code_ == 0; }
  bool operator==(const Error &err) const noexcept {
    return code_ == err.code_;
  }
  int GetCode() const noexcept { return code_; }
  const char *GetReason() const noexcept;
  void Clear() { code_ = 0; }

  std::string String() const {
    return fmt::format("Error[code:{}, reason:{}]", code_, GetReason());
  }
};
} // namespace core
} // namespace pedronet

PEDROLIB_CLASS_FORMATTER(pedronet::core::Error);
#endif // PEDRONET_CORE_ERROR_H
