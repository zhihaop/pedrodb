#ifndef PEDRODB_STATUS_H
#define PEDRODB_STATUS_H

#include <pedrolib/format/formatter.h>
#include <string_view>

namespace pedrodb {
class Status {
  uint32_t code_;

public:
  const static Status kOk;
  const static Status kNotFound;
  const static Status kCorruption;
  const static Status kNotSupported;
  const static Status kInvalidArgument;
  const static Status kIOError;

public:
  explicit Status(uint32_t code) : code_(code) {}
  ~Status() = default;
  Status(const Status &s) = default;
  Status &operator=(const Status &) = default;

  bool Empty() const noexcept { return code_ == 0; }

  bool operator==(const Status &status) const noexcept {
    return code_ == status.code_;
  }

  bool operator!=(const Status &status) const noexcept {
    return code_ != status.code_;
  }

  [[nodiscard]] std::string_view String() const noexcept {
    std::string_view msg[6] = {"ok",          "not found",        "corruption",
                               "not support", "invalid argument", "io error"};
    Status stats[6] = {kOk,           kNotFound,        kCorruption,
                       kNotSupported, kInvalidArgument, kIOError};

    for (size_t i = 0; i < std::size(stats); ++i) {
      if (code_ == stats[i].code_) {
        return msg[i];
      }
    }

    return "unknown";
  }
};
} // namespace pedrodb

PEDROLIB_CLASS_FORMATTER(pedrodb::Status);

#endif // PEDRODB_STATUS_H
