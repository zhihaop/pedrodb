#ifndef PEDROLIB_FORMATT_FORMATTER_H
#define PEDROLIB_FORMATT_FORMATTER_H

#include <fmt/core.h>

#define PEDROLIB_CLASS_FORMATTER(T)                                            \
  template <> struct fmt::formatter<T> {                                       \
    constexpr auto parse(format_parse_context &ctx)                            \
        -> format_parse_context::iterator {                                    \
      return ctx.end();                                                        \
    }                                                                          \
    auto format(const T &item, format_context &ctx) const {                    \
      return fmt::format_to(ctx.out(), "{}", item.String());                   \
    };                                                                         \
  }

#endif // PEDROLIB_FORMATT_FORMATTER_H