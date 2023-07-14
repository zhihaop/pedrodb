#ifndef PEDRODB_CACHE_READ_CACHE_H
#define PEDRODB_CACHE_READ_CACHE_H

#include "pedrodb/cache/lru_cache.h"
#include "pedrodb/defines.h"
#include "pedrodb/format/record_format.h"

namespace pedrodb {

constexpr size_t kCacheSegments = 64;

class ReadCache {

  std::unique_ptr<lru::Cache<record::Location>> segments_[kCacheSegments];

  uint64_t hits_{1};
  uint64_t total_{1};

  mutable std::mutex mu_[kCacheSegments];

 public:
  explicit ReadCache(size_t capacity) {
    size_t segment_capacity = capacity / kCacheSegments;
    for (auto& cache : segments_) {
      cache = std::make_unique<lru::Cache<record::Location>>(segment_capacity);
    }
  }

  double HitRatio() const noexcept {
    return static_cast<double>(hits_) / static_cast<double>(total_);
  }

  bool Put(record::Location location, std::string_view value) {
    size_t h = location.Hash();
    size_t b = h % kCacheSegments;
    std::unique_lock lock{mu_[b]};
    return segments_[b]->Put(location, value);
  }

  void Remove(record::Location location) {
    size_t h = location.Hash();
    size_t b = h % kCacheSegments;
    std::unique_lock lock{mu_[b]};
    segments_[b]->Remove(location);
  }

  bool Read(record::Location location, std::string* value) {
    size_t h = location.Hash();
    size_t b = h % kCacheSegments;
    std::unique_lock lock{mu_[b]};
    ++total_;
    auto v = segments_[b]->Get(location);
    if (v.empty()) {
      return false;
    }

    hits_++;
    value->assign(v);
    return true;
  }
};
}  // namespace pedrodb

#endif  // PEDRODB_CACHE_READ_CACHE_H
