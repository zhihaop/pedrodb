#ifndef PEDRODB_COLLECTION_LRU_UNORDERED_MAP_H
#define PEDRODB_COLLECTION_LRU_UNORDERED_MAP_H
#include <list>
#include <unordered_map>

namespace pedrolib::lru {
template <typename K, typename V, typename Hash = std::hash<K>>
class unordered_map {

  struct KeyValue {
    K key;
    V value;
  };

  using KeyValueIterator = typename std::list<KeyValue>::iterator;

  std::unordered_map<K, KeyValueIterator, Hash> indices_;
  std::list<KeyValue> data_;
  std::size_t capacity_;

public:
  explicit unordered_map(std::size_t capacity) : capacity_(capacity) {}
  unordered_map() : capacity_(std::numeric_limits<size_t>::max()) {}

  void clear() noexcept {
    data_.clear();
    indices_.clear();
  }

  size_t size() const noexcept { return indices_.size(); }

  V evict() noexcept {
    auto iter = data_.begin();
    KeyValue kv = std::move(*iter);
    data_.erase(iter);
    indices_.erase(kv.key);
    return std::move(kv.value);
  }

  V erase(const K &key) noexcept {
    auto iter = indices_.find(key);
    KeyValue kv = std::move(*iter->second);
    data_.erase(iter->second);
    indices_.erase(iter);
    return std::move(kv.value);
  }

  bool contains(const K &key) const noexcept {
    return indices_.find(key) != indices_.end();
  }

  template <typename Value> Value &update(const K &key, Value &&value) {
    if (contains(key)) {
      return (*this)[key] = std::forward<Value>(value);
    }

    if (indices_.size() == capacity_) {
      evict();
    }
    KeyValue kv{};
    kv.key = key;
    kv.value = std::forward<Value>(value);
    return (indices_[key] = data_.insert(data_.end(), std::move(kv)))->value;
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
