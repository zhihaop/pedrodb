#ifndef PEDRODB_STATUS_H
#define PEDRODB_STATUS_H

#include <pedrolib/format/formatter.h>
#include <string_view>

namespace pedrodb {

enum class Status {
  kOk,
  kNotFound,
  kCorruption,
  kNotSupported,
  kInvalidArgument,
  kIOError
};

} // namespace pedrodb

template <> struct fmt::formatter<pedrodb::Status> {
  static constexpr auto parse(format_parse_context &ctx)
      -> format_parse_context::iterator {
    return ctx.end();
  }
  static auto format(const pedrodb::Status &status, format_context &ctx) {
    using namespace pedrodb;
    std::string_view msg[6] = {"ok",          "not found",        "corruption",
                               "not support", "invalid argument", "io error"};
    return fmt::format_to(ctx.out(), "{}", msg[static_cast<size_t>(status)]);
  }
};

#endif // PEDRODB_STATUS_H
