#ifndef PRACTICE_BLOCKING_QUEUE_H
#define PRACTICE_BLOCKING_QUEUE_H

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <vector>

namespace pedro {

template <typename T> class BlockingQueue {
  std::vector<T> queue_;

  std::mutex mu_;
  std::condition_variable not_empty_;
  std::condition_variable not_full_;
  const size_t capacity_;

  std::atomic_size_t size_;
  size_t head_;
  size_t tail_;

  std::atomic_bool closed_;

  T remove_first() {
    T elem = std::move(queue_[head_++]);
    if (size_.fetch_add(-1, std::memory_order_relaxed) == capacity_) {
      not_full_.notify_all();
    }
    head_ = head_ == capacity_ ? 0 : head_;
    return std::move(elem);
  }

  template <typename R> void emplace_back(R &&r) {
    queue_[tail_++] = std::forward<R>(r);
    if (size_.fetch_add(1, std::memory_order_relaxed) == 0) {
      not_empty_.notify_all();
    }
    tail_ = tail_ == capacity_ ? 0 : tail_;
  }

public:
  explicit BlockingQueue(size_t capacity)
      : capacity_(capacity), size_(0), head_(0), tail_(0), queue_(capacity),
        closed_(false) {}

  std::optional<T> Take() {
    std::unique_lock<std::mutex> lock(mu_);
    while (size_.load(std::memory_order_relaxed) == 0) {
      if (closed_.load(std::memory_order_relaxed)) {
        return {};
      }
      not_empty_.wait(lock);
    }
    return remove_first();
  }

  std::optional<T> Poll() {
    if (Empty()) {
      return {};
    }
    std::unique_lock<std::mutex> lock(mu_);
    if (size_.load(std::memory_order_relaxed) == 0) {
      return {};
    }
    return remove_first();
  }

  template <class R> bool Put(R &&item) {
    if (Closed()) {
      return false;
    }
    std::unique_lock<std::mutex> lock(mu_);
    while (true) {
      if (closed_.load(std::memory_order_relaxed)) {
        return false;
      }
      if (size_.load(std::memory_order_relaxed) != capacity_) {
        break;
      }
      not_full_.wait(lock);
    }
    emplace_back(std::forward<R>(item));
    return true;
  }

  template <class R> bool Offer(R &&item) {
    if (Size() == capacity_ || Closed()) {
      return false;
    }
    std::unique_lock<std::mutex> lock(mu_);
    if (closed_.load(std::memory_order_relaxed)) {
      return false;
    }
    if (size_.load(std::memory_order_relaxed) == capacity_) {
      return false;
    }
    emplace_back(std::forward<R>(item));
    return true;
  }

  void Close() {
    std::unique_lock<std::mutex> lock(mu_);
    closed_ = true;
    not_empty_.notify_all();
    not_full_.notify_all();
  }

  bool Closed() const noexcept {
    return closed_.load(std::memory_order_acquire);
  }
  size_t Size() const noexcept { return size_.load(std::memory_order_acquire); }
  bool Empty() const noexcept { return Size() == 0; }
  size_t Capacity() const noexcept { return capacity_; }
};

} // namespace pedro
#endif // PRACTICE_BLOCKING_QUEUE_H
