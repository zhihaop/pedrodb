#ifndef PEDRONET_CORE_DEBUG_H
#define PEDRONET_CORE_DEBUG_H

#include <bits/iterator_concepts.h>
#include <fmt/core.h>
#include <spdlog/spdlog.h>
#include <type_traits>

#define PEDRONET_FORMATABLE_CLASS(T)                                           \
  template <> struct fmt::formatter<T> {                                       \
    constexpr auto parse(format_parse_context &ctx)                            \
        -> format_parse_context::iterator {                                    \
      return ctx.end();                                                        \
    }                                                                          \
    auto format(const T &item, format_context &ctx) const {                    \
      return fmt::format_to(ctx.out(), "{}", item.String());                   \
    };                                                                         \
  };

#endif // PEDRONET_CORE_DEBUG_H