#ifndef PEDRODB_CACHE_LRU_CACHE_H
#define PEDRODB_CACHE_LRU_CACHE_H

#include <algorithm>
#include <string>
#include <vector>

namespace pedrodb {

template <class Key, class Value, class KeyHash = std::hash<Key>>
class LRUCache {
  struct Entry {
    Entry* prev{};
    Entry* next{};
    Entry* hash{};

    Key key{};
    Value value{};

    static Entry* New() { return new Entry(); }
    static void Free(Entry* entry) { delete entry; }

    void Erase() {
      prev->next = next;
      next->prev = prev;

      prev = next = nullptr;
    }

    void InsertAfter(Entry* entry) {
      entry->prev = prev;
      entry->next = next;

      entry->prev->next = entry;
      entry->next->prev = entry;
    }
  };

  static size_t alignExp2(size_t x) noexcept {
    const size_t one{1};
    for (int i = 0; i < sizeof(size_t); ++i) {
      if (x >= (one << i)) {
        return (one << i);
      }
    }
    return -1;
  }

  size_t locate(const Key& key) const noexcept {
    // TODO(zhihaop) optimize mod to bitwise and.
    return hash_(key) % buckets_.size();
  }

  Entry** Find(const Key& key) {
    auto& bucket = buckets_[locate(key)];

    Entry** iter = &bucket;
    while (*iter != nullptr) {
      if ((*iter)->key == key) {
        break;
      }
      iter = &(*iter)->hash;
    }
    return iter;
  }

  void Erase(Entry** it) {
    if (*it == nullptr) {
      return;
    }

    *it = (*it)->hash;
  }

 public:
  using KeyType = Key;
  using ValueType = Value;

  explicit LRUCache(size_t capacity, double load_factor)
      : buckets_(alignExp2(capacity / load_factor)), capacity_(capacity) {

    lru_.next = &lru_;
    lru_.prev = &lru_;
  }

  explicit LRUCache(size_t capacity) : LRUCache(capacity, 0.75) {}

  ~LRUCache() {
    auto node = lru_.next;
    while (node != &lru_) {
      auto next = node->next;
      Entry::Free(node);
      node = next;
    }
  }

  [[nodiscard]] size_t Capacity() const noexcept { return capacity_; }

  [[nodiscard]] size_t Size() const noexcept { return size_; }

  bool Get(const Key& key, Value& value) {
    if (capacity_ == 0) {
      return false;
    }

    auto it = Find(key);
    if (*it == nullptr) {
      return false;
    }

    value = (*it)->value;

    auto ptr = *it;
    ptr->Erase();
    lru_.prev->InsertAfter(ptr);
    return true;
  }

  bool Remove(const Key& key, Value& value) {
    if (capacity_ == 0) {
      return false;
    }

    auto it = Find(key);
    auto ptr = *it;
    if (ptr == nullptr) {
      return false;
    }
    Erase(it);

    value = ptr->value;
    ptr->Erase();
    Entry::Free(ptr);
    return true;
  }

  void Evict() {
    if (capacity_ == 0) {
      return;
    }

    auto ptr = lru_.next;
    if (ptr == &lru_) {
      return;
    }

    Erase(Find(ptr->key));
    ptr->Erase();
    Entry::Free(ptr);

    size_--;
  }

  void Put(const Key& key, const Value& value) {
    if (capacity_ == 0) {
      return;
    }

    auto it = Find(key);
    if (*it != nullptr) {
      (*it)->Erase();
      lru_.prev->InsertAfter(*it);

      (*it)->value = value;
      return;
    }

    if (size_ + 1 == capacity_) {
      Evict();
    }

    Entry* ptr = Entry::New();
    lru_.prev->InsertAfter(ptr);
    *it = ptr;

    ptr->key = key;
    ptr->value = value;
    size_++;
  }

 private:
  std::vector<Entry*> buckets_;
  const size_t capacity_;
  size_t size_{};

  Entry lru_;
  KeyHash hash_;
};
}  // namespace pedrodb
#endif  // PEDRODB_CACHE_LRU_CACHE_H
