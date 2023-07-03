#ifndef PEDRODB_COLLECTION_LRU_UNORDERED_MAP_H
#define PEDRODB_COLLECTION_LRU_UNORDERED_MAP_H
#include <list>
#include <unordered_map>

namespace pedrolib::lru {
template <typename K, typename V> class unordered_map {

  struct KeyValue {
    K key;
    V value;
  };

  using KeyValueIterator = typename std::list<KeyValue>::iterator;

  std::unordered_map<K, KeyValueIterator> indices_;
  std::list<KeyValue> data_;
  std::size_t capacity_;

public:
  explicit unordered_map(std::size_t capacity) : capacity_(capacity) {}

  void clear() noexcept {
    data_.clear();
    indices_.clear();
  }

  void evict() noexcept {
    if (data_.empty()) {
      return;
    }

    auto iter = data_.begin();
    KeyValue kv = std::move(*iter);
    data_.erase(iter);
    indices_.erase(kv.key);
  }

  void erase(const K &key) noexcept {
    auto iter = indices_.find(key);
    if (iter == indices_.end()) {
      return;
    }

    data_.erase(iter->second);
    indices_.erase(iter);
  }

  bool contains(const K &key) const noexcept {
    return indices_.find(key) != indices_.end();
  }

  template <typename Value> void update(const K &key, Value &&value) {
    if (contains(key)) {
      (*this)[key] = std::forward<Value>(value);
      return;
    }

    if (indices_.size() == capacity_) {
      evict();
    }
    KeyValue kv{};
    kv.key = key;
    kv.value = std::forward<Value>(value);
    indices_[key] = data_.insert(data_.end(), std::move(kv));
  }

  V &operator[](const K &key) noexcept {
    auto &iter = indices_[key];
    KeyValue kv = std::move(*iter);
    data_.erase(iter);
    iter = data_.insert(data_.end(), std::move(kv));
    return iter->value;
  }
};
} // namespace pedrolib::lru

#endif // PEDRODB_COLLECTION_LRU_UNORDERED_MAP_H
