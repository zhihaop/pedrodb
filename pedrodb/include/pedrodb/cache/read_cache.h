#ifndef PEDRODB_CACHE_READ_CACHE_H
#define PEDRODB_CACHE_READ_CACHE_H

#include "pedrodb/defines.h"
#include "pedrodb/record_format.h"

namespace pedrodb {

struct HashEntry {
  HashEntry *prev{};
  HashEntry *next{};
  HashEntry *hash{};

  record::Location loc{};
  std::string data;

  static HashEntry *New() { return new HashEntry(); }

  static void Free(HashEntry *entry) { delete entry; }
};

class LRUCache {
  constexpr static size_t kMinimumBuckets = 1024;
  constexpr static size_t kMaximumBuckets = 1 << 22;

  std::vector<HashEntry *> buckets_;
  const size_t capacity_;
  size_t size_{};

  HashEntry lru_;

  /**
   * Get the iterator of the buckets. (not the lru list).
   *
   * @param loc the location of record.
   * @return the iterator.
   */
  HashEntry **GetIterator(record::Location loc) {
    size_t h = loc.Hash();
    size_t b = h % buckets_.size();

    HashEntry **node = &buckets_[b];
    while (*node) {
      if ((*node)->loc == loc) {
        return node;
      }
      node = &(*node)->hash;
    }
    return node;
  }

  static void RemoveFromList(HashEntry *entry) noexcept {
    entry->prev->next = entry->next;
    entry->next->prev = entry->prev;

    entry->prev = nullptr;
    entry->next = nullptr;
  }

  HashEntry *RemoveFromBucket(HashEntry **iter) noexcept {
    if (*iter != nullptr) {
      size_ -= (*iter)->data.size();

      HashEntry *entry = *iter;
      *iter = entry->hash;
    }
    return *iter;
  }

public:
  explicit LRUCache(size_t capacity)
      : capacity_(capacity),
        buckets_(std::clamp((size_t)(capacity / sizeof(HashEntry) * 1.25),
                            kMinimumBuckets, kMaximumBuckets)) {

    spdlog::info("create buckets {}", buckets_.size());
    lru_.next = &lru_;
    lru_.prev = &lru_;
  }

  [[nodiscard]] size_t Capacity() const noexcept { return capacity_; }

  [[nodiscard]] size_t Size() const noexcept { return size_; }

  std::string_view Get(record::Location loc) noexcept {
    HashEntry **iter = GetIterator(loc);
    if (*iter == nullptr) {
      return {};
    }

    HashEntry *entry = *iter;

    // detach from lru_ list.
    RemoveFromList(entry);

    // insert to lru_ end.
    entry->next = &lru_;
    entry->prev = lru_.prev;
    entry->prev->next = entry;
    entry->next->prev = entry;
    return {entry->data};
  }

  void Evict() noexcept {
    HashEntry *first = lru_.next;
    if (first == &lru_) {
      return;
    }

    // remove from the lru_ head.
    RemoveFromList(first);

    // remove from the buckets.
    HashEntry **iter = GetIterator(first->loc);
    RemoveFromBucket(iter);

    HashEntry::Free(first);
  }

  void Put(record::Location loc, std::string_view value) noexcept {
    if (value.empty() || value.size() > capacity_) {
      return;
    }

    HashEntry **iter = GetIterator(loc);
    HashEntry *entry = *iter;
    if (entry == nullptr) {
      // create the hash entry, and insert to the buckets_.
      entry = HashEntry::New();
      *iter = entry;
    } else {
      RemoveFromList(entry);
      size_ -= entry->data.size();
      entry->data.clear();
    }

    entry->loc = loc;
    entry->data = value;
    size_ += value.size();

    // insert to the back of lru_.
    entry->prev = lru_.prev;
    entry->next = &lru_;
    entry->prev->next = entry;
    entry->next->prev = entry;

    EvictFull();
  }

  void EvictFull() {
    while (size_ > capacity_) {
      Evict();
    }
  }

  void Remove(record::Location loc) noexcept {
    HashEntry **iter = GetIterator(loc);
    if (*iter == nullptr) {
      return;
    }

    HashEntry *entry = *iter;
    RemoveFromList(entry);
    RemoveFromBucket(iter);
    HashEntry::Free(entry);
  }
};

class ReadCache {
  LRUCache cache_;

  std::unordered_map<uint32_t, std::string> active_cache_;
  file_t active_id_{};

  uint64_t hits_{1};
  uint64_t total_{1};

  mutable std::mutex mu_;

public:
  explicit ReadCache(size_t capacity) : cache_(capacity) {}

  double HitRatio() const noexcept {
    std::unique_lock lock{mu_};
    return static_cast<double>(hits_) / static_cast<double>(total_);
  }

  void UpdateCache(record::Location location, std::string_view value) {
    std::unique_lock lock{mu_};
    if (location.id == active_id_) {
      active_cache_.emplace(location.offset, std::string{value});
    } else {
      cache_.Put(location, std::string{value});
    }
  }

  void Remove(record::Location location) {
    std::unique_lock lock{mu_};
    if (location.id != active_id_) {
      cache_.Remove(location);
    }
  }

  bool Read(record::Location location, std::string *value) {
    std::unique_lock lock{mu_};
    ++total_;
    if (location.id == active_id_) {
      *value = active_cache_[location.offset];
      ++hits_;
      return true;
    }

    auto v = cache_.Get(location);
    if (v.empty()) {
      return false;
    }

    hits_++;
    value->assign(v);
    return true;
  }

  void UpdateActiveID(file_t active_id) {
    std::unique_lock lock{mu_};
    if (active_id > active_id_) {
      active_id_ = active_id;
      active_cache_.clear();
    }
  }
};
} // namespace pedrodb

#endif // PEDRODB_CACHE_READ_CACHE_H
