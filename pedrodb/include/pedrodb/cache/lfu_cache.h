#ifndef PEDRODB_CACHE_LFU_CACHE_H
#define PEDRODB_CACHE_LFU_CACHE_H

#include <vector>

namespace pedrodb {

template <class Key> struct HashEntry {
  HashEntry *prev{};
  HashEntry *next{};

  HashEntry *hash{};

  Key key{};
  uint64_t freq{};
  std::string data;

  static HashEntry *New() {
    auto entry = new HashEntry();
    entry->prev = entry;
    entry->next = entry;
    return entry;
  }

  static void Free(HashEntry *entry) { delete entry; }
};

template <class Key> class LFUCache {
  constexpr static size_t kMinimumBuckets = 1024;
  constexpr static size_t kMaximumBuckets = 1 << 22;

  std::vector<HashEntry<Key> *> key_buckets_;
  std::vector<HashEntry<Key> *> freq_buckets_;
  uint64_t min_freq_;
  size_t size_{};
  const size_t capacity_;

  static size_t GetBucketsCount(size_t capacity) noexcept {
    return std::clamp((size_t)((double)capacity / sizeof(HashEntry<Key>)),
                      kMinimumBuckets, kMaximumBuckets);
  }

public:
  explicit LFUCache(size_t capacity)
      : capacity_(capacity), key_buckets_(GetBucketsCount(capacity)),
        freq_buckets_(GetBucketsCount(capacity)), min_freq_{0} {}

  [[nodiscard]] size_t Capacity() const noexcept { return capacity_; }
};

} // namespace pedrodb
#endif // PEDRODB_CACHE_LFU_CACHE_H
