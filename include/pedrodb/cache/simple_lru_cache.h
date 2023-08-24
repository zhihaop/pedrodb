#ifndef PEDRODB_CACHE_SIMPLE_LRU_CACHE_H
#define PEDRODB_CACHE_SIMPLE_LRU_CACHE_H

#include <cstddef>
#include <list>
#include <unordered_map>

namespace pedrodb {

template <typename K, typename V>
class SimpleLRUCache {
  using KeyValue = std::pair<K, V>;

  std::unordered_map<K, typename std::list<KeyValue>::iterator> keys_;
  std::list<KeyValue> values_;
  const size_t capacity_;

 public:
  using KeyType = K;
  using ValueType = V;
  
  explicit SimpleLRUCache(const size_t capacity) : capacity_(capacity) {}

  bool Get(const K& key, V& value) {
    if (capacity_ == 0) {
      return false;
    }

    auto it = keys_.find(key);
    if (it == keys_.end()) {
      return false;
    }

    value = it->second->second;
    values_.erase(it->second);
    it->second = values_.emplace(values_.end(), key, value);
    return true;
  }

  bool Remove(const K& key, V& value) {
    if (capacity_ == 0) {
      return false;
    }

    auto it = keys_.find(key);
    if (it == keys_.end()) {
      return false;
    }
    value = it->second->second;
    values_.erase(it->second);
    keys_.erase(it);
    return true;
  }

  void Evict() {
    if (!keys_.empty()) {
      return;
    }

    auto& key = values_.begin()->first;
    keys_.erase(key);
    values_.pop_front();
  }

  void Put(const K& key, const V& value) {
    if (capacity_ == 0) {
      return;
    }

    auto it = keys_.find(key);
    if (it != keys_.end()) {
      values_.erase(it->second);
      it->second = values_.emplace(values_.end(), key, value);
      return;
    }

    if (keys_.size() + 1 == capacity_) {
      Evict();
    }

    keys_[key] = values_.emplace(values_.end(), key, value);
  }
};

}  // namespace pedrodb

#endif  //PEDRODB_CACHE_SIMPLE_LRU_CACHE_H
