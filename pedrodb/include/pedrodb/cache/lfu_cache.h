#ifndef PEDRODB_CACHE_LFU_CACHE_H
#define PEDRODB_CACHE_LFU_CACHE_H

#include <algorithm>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

// @unstable
namespace pedrodb::lfu {

template <class Key> struct HashEntry {

  struct GroupEntry {
    GroupEntry *prev{};
    GroupEntry *next{};
    HashEntry *head{};
    uint64_t freq{};

    static GroupEntry *New(HashEntry *head, uint64_t freq) {
      auto entry = new GroupEntry();
      entry->prev = entry;
      entry->next = entry;
      entry->head = head;
      entry->freq = freq;
      return entry;
    }
    static void Free(GroupEntry *entry) { delete entry; }
  };

  HashEntry *prev{};
  HashEntry *next{};
  GroupEntry *group{};

  HashEntry *hash{};

  Key key{};
  std::string data;

  static HashEntry *New() {
    auto entry = new HashEntry();
    entry->prev = entry;
    entry->next = entry;
    return entry;
  }

  static void Free(HashEntry *entry) { delete entry; }

  void RemoveGroup() {
    if (this == next) {
      group->prev->next = group->next;
      group->next->prev = group->prev;
      GroupEntry::Free(group);
      group = nullptr;

      prev = next = nullptr;
      return;
    }

    if (this == group->head) {
      group->head = next;
    }

    prev->next = next;
    next->prev = prev;
    prev = next = this;
    group = nullptr;
  }

  void AddFrequency() {
    uint64_t freq = group->freq;
    GroupEntry *g = group->next;
    RemoveGroup();

    if (g->freq != freq + 1) {
      group = GroupEntry::New(this, freq + 1);
      group->prev = g->prev;
      group->next = g;

      group->prev->next = group;
      group->next->prev = group;
      prev = next = this;
      return;
    }

    HashEntry *head = g->head;
    g->head = this;

    group = g;
    next = head;
    prev = head->prev;

    next->prev = this;
    prev->next = this;
  }
};

template <class Key> class Cache {
  constexpr static size_t kMinimumBuckets = 1024;
  constexpr static size_t kMaximumBuckets = 1 << 22;
  using Entry = HashEntry<Key>;
  using GroupEntry = typename Entry::GroupEntry;

  std::vector<Entry *> buckets_;
  GroupEntry head_;

  size_t size_{};
  const size_t capacity_;

  static size_t GetBucketsCount(size_t capacity) noexcept {
    return std::clamp((size_t)((double)capacity / sizeof(HashEntry<Key>)),
                      kMinimumBuckets, kMaximumBuckets);
  }

  HashEntry<Key> **GetBucketIterator(const Key &key) {
    size_t h = key.Hash();
    size_t b = h % buckets_.size();

    HashEntry<Key> **it = &buckets_[b];
    while (*it) {
      if ((*it)->key == key) {
        return it;
      }
      it = &(*it)->hash;
    }
    return it;
  }

public:
  explicit Cache(size_t capacity)
      : capacity_(capacity), buckets_(GetBucketsCount(capacity)) {
    head_.next = &head_;
    head_.prev = &head_;
    head_.head = nullptr;
    head_.freq = 0;
  }

  [[nodiscard]] size_t Capacity() const noexcept { return capacity_; }

  [[nodiscard]] size_t Size() const noexcept { return size_; }

  std::string_view Get(const Key &key) {
    auto it = GetBucketIterator(key);
    if (*it == nullptr) {
      return {};
    }

    Entry *entry = *it;
    entry->AddFrequency();
    return entry->data;
  }

  void Evict() {
    auto group = head_.next;
    if (group == &head_) {
      return;
    }

    Entry *entry = group->head;
    *GetBucketIterator(entry->key) = nullptr;
    entry->RemoveGroup();
    size_ -= entry->data.size();
    Entry::Free(entry);
  }

  bool Put(const Key &key, std::string_view value) {
    if (value.empty() || value.size() > capacity_) {
      return false;
    }

    auto it = GetBucketIterator(key);
    if (*it != nullptr) {
      return false;
    }

    while (size_ + value.size() > capacity_) {
      Evict();
    }

    Entry *entry = Entry::New();
    entry->key = key;
    entry->data.assign(value.begin(), value.end());
    *GetBucketIterator(key) = entry;
    size_ += value.size();

    GroupEntry *g = head_.next;
    if (g->freq != 1) {
      g = GroupEntry::New(entry, 1);
      g->next = head_.next;
      g->prev = &head_;
      g->prev->next = g;
      g->next->prev = g;
      entry->group = g;
      return true;
    }

    Entry *head = g->head;
    entry->group = g;
    entry->next = head;
    entry->prev = head->prev;
    entry->prev->next = entry;
    entry->next->prev = entry;
    return true;
  }

  void Remove(const Key &key) {
    auto it = GetBucketIterator(key);
    if (*it == nullptr) {
      return;
    }

    Entry *entry = *it;
    entry->RemoveGroup();
    size_ -= entry->data.size();
    Entry::Free(entry);
  }
};

} // namespace pedrodb::lfu
#endif // PEDRODB_CACHE_LFU_CACHE_H
