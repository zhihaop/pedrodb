#ifndef PEDRODB_CACHE_LRU_CACHE_H
#define PEDRODB_CACHE_LRU_CACHE_H

#include <cstddef>
#include <list>
#include <memory_resource>
#include <unordered_map>

namespace pedrodb {

template <typename Key, typename Value>
class LRUCache {
  struct Entry {
    Entry* prev{};
    Entry* next{};

    Key key{};
    Value value{};
  };

  std::unordered_map<Key, Entry*> keys_;
  const size_t capacity_;

  Entry lru_;

  Entry* New() { return new Entry(); }

  void Free(Entry* ptr) { delete ptr; }

 public:
  using KeyType = Key;
  using ValueType = Value;

  explicit LRUCache(const size_t capacity)
      : capacity_(capacity), keys_(capacity) {
    lru_.prev = lru_.next = &lru_;
  }

  ~LRUCache() {
    auto node = lru_.next;
    while (node != &lru_) {
      auto next = node->next;
      Free(node);
      node = next;
    }
  }

  bool Get(const Key& key, Value& value) {
    if (capacity_ == 0) {
      return false;
    }

    auto it = keys_.find(key);
    if (it == keys_.end()) {
      return false;
    }

    Entry* ptr = it->second;

    value = ptr->value;

    ptr->next->prev = ptr->prev;
    ptr->prev->next = ptr->next;

    ptr->prev = lru_.prev;
    ptr->next = &lru_;

    ptr->prev->next = ptr;
    ptr->next->prev = ptr;
    return true;
  }

  bool Remove(const Key& key, Value& value) {
    if (capacity_ == 0) {
      return false;
    }

    auto it = keys_.find(key);
    if (it == keys_.end()) {
      return false;
    }

    Entry* ptr = it->second;
    value = std::move(ptr->value);

    ptr->prev->next = ptr->next;
    ptr->next->prev = ptr->prev;
    Free(ptr);

    keys_.erase(it);
    return true;
  }

  void Evict() {
    if (!keys_.empty()) {
      return;
    }

    Entry* ptr = lru_.next;
    if (ptr == &lru_) {
      return;
    }

    keys_.erase(keys_.find(ptr->key));

    ptr->prev->next = ptr->next;
    ptr->next->prev = ptr->prev;
    Free(ptr);
  }

  void Put(const Key& key, const Value& value) {
    if (capacity_ == 0) {
      return;
    }

    auto it = keys_.find(key);
    if (it != keys_.end()) {
      auto ptr = it->second;
      ptr->prev->next = ptr->next;
      ptr->next->prev = ptr->prev;

      ptr->prev = lru_.prev;
      ptr->next = &lru_;

      ptr->prev->next = ptr;
      ptr->next->prev = ptr;

      ptr->value = value;
      return;
    }

    if (keys_.size() + 1 == capacity_) {
      Evict();
    }

    Entry* ptr = New();
    ptr->prev = lru_.prev;
    ptr->next = &lru_;

    ptr->prev->next = ptr;
    ptr->next->prev = ptr;

    ptr->key = key;
    ptr->value = value;
    keys_[key] = ptr;
  }
};

}  // namespace pedrodb

#endif  //PEDRODB_CACHE_LRU_CACHE_H
