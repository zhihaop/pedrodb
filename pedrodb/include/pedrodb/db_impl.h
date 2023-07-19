#ifndef PEDRODB_DB_IMPL_H
#define PEDRODB_DB_IMPL_H

#include <absl/container/flat_hash_map.h>
#include <pedrolib/concurrent/latch.h>
#include <pedrolib/executor/thread_pool_executor.h>
#include <map>
#include <memory>
#include <mutex>
#include <unordered_set>
#include <vector>

#include "pedrodb/db.h"
#include "pedrodb/defines.h"
#include "pedrodb/file/readwrite_file.h"
#include "pedrodb/file_manager.h"
#include "pedrodb/format/index_format.h"
#include "pedrodb/format/record_format.h"
#include "pedrodb/iterator/index_iterator.h"
#include "pedrodb/iterator/record_iterator.h"
#include "pedrodb/logger/logger.h"
#include "pedrodb/metadata_manager.h"

namespace pedrodb {

struct Record {
  uint32_t checksum{};
  std::string key;
  std::string value;
  record::Location location{};
  uint32_t timestamp{};
};

enum class CompactState {
  kNop,
  kScheduling,
  kCompacting,
};

struct CompactHint {
  size_t unused{};
  CompactState state{CompactState::kNop};
};

class small_string {
  const char* data_{};

  template <class String>
  static std::string_view string_view_cast(const String& s) {
    if constexpr (std::is_same_v<String, small_string>) {
      return s.view();
    } else {
      return {s};
    }
  }

 public:
  small_string() = default;
  explicit small_string(std::string_view s) { assign(s); }
  small_string(const small_string& s) {
    if (s.data_ != nullptr) {
      assign(s.view());
    }
  }
  small_string(small_string&& s) noexcept : data_(s.data_) {
    s.data_ = nullptr;
  }
  small_string& operator=(const small_string& s) {
    if (this == &s) {
      return *this;
    }
    assign(s.view());
    return *this;
  }
  small_string& operator=(small_string&& s) noexcept {
    if (this == &s) {
      return *this;
    }
    clear();
    std::swap(data_, s.data_);
    return *this;
  }

  ~small_string() { clear(); }

  void clear() {
    if (data_ != nullptr) {
      delete[] data_;
      data_ = nullptr;
    }
  }

  void assign(std::string_view s) {
    clear();

    if (s.empty()) {
      return;
    }

    char* data = new char[s.size() + 1];
    data[0] = static_cast<char>(s.size());
    memcpy(data + 1, s.data(), s.size());
    data_ = data;
  }

  [[nodiscard]] const char* data() const noexcept {
    return data_ ? data_ + 1 : nullptr;
  }

  [[nodiscard]] bool empty() const noexcept { return data_ == nullptr; }
  [[nodiscard]] size_t size() const noexcept { return data_ ? data_[0] : 0; }

  char operator[](size_t index) const noexcept { return data_[index + 1]; }

  [[nodiscard]] std::string_view view() const noexcept {
    return empty() ? std::string_view{} : std::string_view{data(), size()};
  }

  template <class This, class That>
  friend bool operator==(const This& s, const That& t) noexcept {
    return string_view_cast(s) == string_view_cast(t);
  }
};

struct HashHelper {
  using is_transparent = std::true_type;

  template <class Key>
  size_t operator()(const Key& key) const noexcept {
    if constexpr (std::is_same_v<Key, small_string>) {
      return record::Hash(key.view());
    } else {
      return record::Hash(key);
    }
  }

  template <class Key1, class Key2>
  bool operator()(const Key1& key1, const Key2& key2) const noexcept {
    return key1 == key2;
  }
};

class DBImpl : public DB {
  mutable std::mutex mu_;

  Options options_;
  uint64_t sync_worker_{};
  uint64_t compact_worker_{};
  std::unique_ptr<FileManager> file_manager_;
  std::unique_ptr<MetadataManager> metadata_manager_;
  std::shared_ptr<pedrolib::Executor> executor_;
  absl::flat_hash_map<small_string, record::Dir, HashHelper, HashHelper>
      indices_;

  // for compaction.
  std::unordered_set<file_id_t> compact_tasks_;
  std::unordered_map<file_id_t, CompactHint> compact_hints_;

  void Recovery(file_id_t id, index::EntryView entry);
  Status Recovery(file_id_t id);

  void Compact(file_id_t id);

  std::vector<file_id_t> GetFiles();

  void UpdateUnused(record::Location loc, size_t unused);

  std::vector<file_id_t> PollCompactTask();

  Status CompactBatch(file_id_t id, const std::vector<Record>& records);

  Status Recovery();

  auto AcquireLock() const { return std::unique_lock{mu_}; }

  Status HandlePut(const WriteOptions& options, std::string_view key,
                   std::string_view value);

  Status HandleGet(const ReadOptions& options, std::string_view key,
                   std::string* value);

 public:
  ~DBImpl() override;

  explicit DBImpl(const Options& options, const std::string& name);

  Status Flush() override;

  Status Compact() override;

  Status Init();

  Status Get(const ReadOptions& options, std::string_view key,
             std::string* value) override;

  Status Put(const WriteOptions& options, std::string_view key,
             std::string_view value) override;

  Status GetIterator(EntryIterator::Ptr* iterator) override;

  Status Delete(const WriteOptions& options, std::string_view key) override;
};
}  // namespace pedrodb

#endif  // PEDRODB_DB_IMPL_H
