#ifndef PEDRODB_CACHE_SEGMENT_CACHE_H
#define PEDRODB_CACHE_SEGMENT_CACHE_H

#include <pedrolib/collection/static_vector.h>
#include "pedrodb/defines.h"
#include "pedrodb/status.h"

namespace pedrodb {
template <class Cache, class Mutex = std::mutex,
          class KeyHash = std::hash<typename Cache::KeyType>>
class SegmentCache {
  template <class T>
  using Vector = pedrolib::StaticVector<T>;

  using KeyType = typename Cache::KeyType;
  using ValueType = typename Cache::ValueType;

  struct alignas(64) Segment {
    Cache cache_;
    Mutex mu_;

    template <class... Args>
    explicit Segment(Args&&... args) : cache_(std::forward<Args>(args)...) {}
  };

  Vector<Segment> segments_;
  KeyHash hash_;

  size_t locate(const KeyType& key) const noexcept {
    return hash_(key) % segments_.size();
  }

 public:
  explicit SegmentCache(size_t segments) : segments_(segments) {}

  SegmentCache() : SegmentCache(std::thread::hardware_concurrency()) {}

  [[nodiscard]] size_t SegmentCapacity() const noexcept {
    return segments_.capacity();
  }

  [[nodiscard]] size_t SegmentSize() const noexcept { return segments_.size(); }

  template <typename... Args>
  void SegmentAdd(Args&&... args) {
    segments_.emplace_back(std::forward<Args>(args)...);
  }

  bool Get(const KeyType& key, ValueType& value) {
    auto& seg = segments_[locate(key)];
    std::lock_guard guard{seg.mu_};
    return seg.cache_.Get(key, value);
  }

  bool Remove(const KeyType& key, ValueType& value) {
    auto& seg = segments_[locate(key)];
    std::lock_guard guard{seg.mu_};
    return seg.cache_.Remove(key, value);
  }

  void Put(const KeyType& key, const ValueType& value) {
    auto& seg = segments_[locate(key)];
    std::lock_guard guard{seg.mu_};
    seg.cache_.Put(key, value);
  }

  template <class Supplier>
  Status GetOrCompute(const KeyType& key, ValueType& value,
                      Supplier&& supplier) {
    auto& seg = segments_[locate(key)];
    std::lock_guard guard{seg.mu_};
    if (seg.cache_.Get(key, value)) {
      return Status::kOk;
    }

    Status status;
    std::tie(status, value) = supplier();
    if (status == Status::kOk) {
      seg.cache_.Put(key, value);
    }
    return status;
  }
};
}  // namespace pedrodb

#endif  //PEDRODB_CACHE_SEGMENT_CACHE_H
