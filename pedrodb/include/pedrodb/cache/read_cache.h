#ifndef PEDRODB_CACHE_READ_CACHE_H
#define PEDRODB_CACHE_READ_CACHE_H

#include "pedrodb/cache/lfu_cache.h"
#include "pedrodb/cache/lru_cache.h"
#include "pedrodb/defines.h"
#include "pedrodb/record_format.h"

namespace pedrodb {

class ReadCache {
  lru::Cache<record::Location> cache_;

  uint64_t hits_{1};
  uint64_t total_{1};

  mutable std::mutex mu_;

public:
  explicit ReadCache(size_t capacity) : cache_(capacity) {}

  double HitRatio() const noexcept {
    std::unique_lock lock{mu_};
    return static_cast<double>(hits_) / static_cast<double>(total_);
  }

  bool Put(record::Location location, std::string_view value) {
    std::unique_lock lock{mu_};
    return cache_.Put(location, value);
  }

  void Remove(record::Location location) {
    std::unique_lock lock{mu_};
    cache_.Remove(location);
  }

  bool Read(record::Location location, std::string *value) {
    std::unique_lock lock{mu_};
    ++total_;
    auto v = cache_.Get(location);
    if (v.empty()) {
      return false;
    }

    hits_++;
    value->assign(v);
    return true;
  }
};
} // namespace pedrodb

#endif // PEDRODB_CACHE_READ_CACHE_H
