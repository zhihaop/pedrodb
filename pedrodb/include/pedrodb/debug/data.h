#ifndef PEDRODB_DEBUG_DATA_H
#define PEDRODB_DEBUG_DATA_H
#include <fmt/format.h>
#include <pedrolib/executor/thread_pool_executor.h>
#include <random>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

namespace pedrodb::debug {
inline static std::string RandomString(std::string_view prefix, size_t n) {
  std::string s{prefix};
  s.reserve(n);

  std::uniform_int_distribution<char> dist(0, 127);
  thread_local std::mt19937_64 dev(
      std::chrono::system_clock::now().time_since_epoch().count());
  for (size_t i = 0; i < n - prefix.size(); ++i) {
    s += dist(dev);
  }
  return s;
}

inline static std::string PaddingString(std::string_view prefix, size_t n,
                                        char pad) {
  std::string s{prefix};
  s.reserve(n);
  for (size_t i = 0; i < n - prefix.size(); ++i) {
    s += pad;
  }
  return s;
}

struct KeyValueOptions {
  size_t key_size = 16;
  size_t value_size = 100;
  bool random_value = false;
  bool lazy_value = true;
};

class KeyValue {
  struct ValueOption {
    size_t index;
    size_t len;
    bool random;
  };
  std::string key_;
  mutable std::variant<std::string, ValueOption> value_;

 public:
  KeyValue() = default;
  KeyValue(std::string key, const std::variant<std::string, ValueOption>& value)
      : key_(std::move(key)), value_(value) {}

 public:
  static KeyValue Create(KeyValueOptions options, size_t index) {
    auto k = PaddingString(fmt::format("key{}", index), options.key_size, '*');
    if (options.lazy_value) {
      return {k, ValueOption{index, options.value_size, options.random_value}};
    }
    std::string v;
    if (options.random_value) {
      v = RandomString(fmt::format("value{}", index), options.value_size);
    } else {
      v = PaddingString(fmt::format("value{}", index), options.value_size, '*');
    }
    return {k, v};
  }

  [[nodiscard]] const std::string& key() const noexcept { return key_; }

  [[nodiscard]] const std::string& value() const {
    if (value_.index() == 1) {
      auto [index, len, random] = std::get<1>(value_);
      if (random) {
        value_ = RandomString(fmt::format("value{}", index), len);
      } else {
        value_ = PaddingString(fmt::format("value{}", index), len, '*');
      }
    }
    return std::get<0>(value_);
  }
};

inline static std::vector<KeyValue> Generator(size_t n,
                                              KeyValueOptions options) {

  std::vector<KeyValue> data(n);
  pedrolib::ThreadPoolExecutor executor;

  std::atomic_size_t c{0};
  pedrolib::for_each(&executor, data.begin(), data.end(), [&](auto& kv) {
    kv = KeyValue::Create(options, c.fetch_add(1));
  });
  return data;
}
}  // namespace pedrodb::debug
#endif  //PEDRODB_DEBUG_DATA_H
