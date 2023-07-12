
#ifndef PEDRODB_CACHE_LRU_CACHE_H
#define PEDRODB_CACHE_LRU_CACHE_H

#include <algorithm>
#include <string>
#include <vector>

namespace pedrodb::lru {

template <class Key> struct HashEntry {
  HashEntry *prev{};
  HashEntry *next{};
  HashEntry *hash{};

  Key key{};
  std::string data;

  static HashEntry *New() { return new HashEntry(); }

  static void Free(HashEntry *entry) { delete entry; }
};

template <class Key> class Cache {
  constexpr static size_t kMinimumBuckets = 1024;
  constexpr static size_t kMaximumBuckets = 1 << 22;

  std::vector<HashEntry<Key> *> buckets_;
  const size_t capacity_;
  size_t size_{};

  HashEntry<Key> lru_;

  /**
   * Get the iterator of the buckets. (not the lru list).
   *
   * @param key the key.
   * @return the iterator.
   */
  HashEntry<Key> **GetIterator(const Key &key) {
    size_t h = key.Hash();
    size_t b = h % buckets_.size();

    HashEntry<Key> **node = &buckets_[b];
    while (*node) {
      if ((*node)->key == key) {
        return node;
      }
      node = &(*node)->hash;
    }
    return node;
  }

  static void RemoveFromList(HashEntry<Key> *entry) noexcept {
    entry->prev->next = entry->next;
    entry->next->prev = entry->prev;

    entry->prev = nullptr;
    entry->next = nullptr;
  }

  HashEntry<Key> *RemoveFromBucket(HashEntry<Key> **iter) noexcept {
    if (*iter != nullptr) {
      size_ -= (*iter)->data.size();

      HashEntry<Key> *entry = *iter;
      *iter = entry->hash;
    }
    return *iter;
  }

public:
  explicit Cache(size_t capacity)
      : capacity_(capacity),
        buckets_(std::clamp(
            (size_t)((double)capacity / sizeof(HashEntry<Key>) * 1.25),
            kMinimumBuckets, kMaximumBuckets)) {

    lru_.next = &lru_;
    lru_.prev = &lru_;
  }

  ~Cache() {
    while (size_ > 0) {
      Evict();
    }
  }

  [[nodiscard]] size_t Capacity() const noexcept { return capacity_; }

  [[nodiscard]] size_t Size() const noexcept { return size_; }

  std::string_view Get(const Key &key) noexcept {
    auto iter = GetIterator(key);
    if (*iter == nullptr) {
      return {};
    }

    HashEntry<Key> *entry = *iter;

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
    HashEntry<Key> *first = lru_.next;
    if (first == &lru_) {
      return;
    }

    // remove from the lru_ head.
    RemoveFromList(first);

    // remove from the buckets.
    auto iter = GetIterator(first->key);
    RemoveFromBucket(iter);

    HashEntry<Key>::Free(first);
  }

  bool Put(const Key &key, std::string_view value) noexcept {
    if (value.empty() || value.size() > capacity_) {
      return false;
    }

    auto iter = GetIterator(key);
    HashEntry<Key> *entry = *iter;
    if (entry != nullptr) {
      return false;
    }

    // create the hash entry, and insert to the buckets_.
    entry = HashEntry<Key>::New();
    *iter = entry;

    entry->key = key;
    entry->data = value;
    size_ += value.size();

    // insert to the back of lru_.
    entry->prev = lru_.prev;
    entry->next = &lru_;
    entry->prev->next = entry;
    entry->next->prev = entry;

    EvictFull();
    return true;
  }

  void EvictFull() {
    while (size_ > capacity_) {
      Evict();
    }
  }

  void Remove(const Key &key) noexcept {
    auto iter = GetIterator(key);
    if (*iter == nullptr) {
      return;
    }

    HashEntry<Key> *entry = *iter;
    RemoveFromList(entry);
    RemoveFromBucket(iter);
    HashEntry<Key>::Free(entry);
  }
};
} // namespace pedrodb::lru
#endif // PEDRODB_CACHE_LRU_CACHE_H
