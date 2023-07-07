#ifndef PEDROLIB_FILE_ERROR_H
#define PEDROLIB_FILE_ERROR_H

#include "pedrolib/format/formatter.h"
#include <string>

namespace pedrolib {
class Error {
  int code_{};

public:
  const static Error kOk;

public:
  explicit Error(int code) : code_(code) {}

  static Error Success() { return {}; }

  Error() = default;
  ~Error() = default;
  Error(const Error &) = default;
  Error &operator=(const Error &) = default;

  [[nodiscard]] bool Empty() const noexcept { return code_ == 0; }
  bool operator==(const Error &err) const noexcept {
    return code_ == err.code_;
  }
  bool operator!=(const Error &err) const noexcept {
    return code_ != err.code_;
  }
  [[nodiscard]] int GetCode() const noexcept { return code_; }
  [[nodiscard]] const char *GetReason() const noexcept;
  void Clear() { code_ = 0; }

  [[nodiscard]] std::string String() const {
    return fmt::format("Error[code:{}, reason:{}]", code_, GetReason());
  }
};
} // namespace pedrolib

PEDROLIB_CLASS_FORMATTER(pedrolib::Error);
#endif // PEDROLIB_FILE_ERROR_H
