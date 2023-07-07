#ifndef PEDRODB_DB_IMPL_H
#define PEDRODB_DB_IMPL_H

#include "pedrodb/cache/read_cache.h"
#include "pedrodb/db.h"
#include "pedrodb/defines.h"
#include "pedrodb/file/writable_file.h"
#include "pedrodb/file_manager.h"
#include "pedrodb/iterator/record_iterator.h"
#include "pedrodb/logger/logger.h"
#include "pedrodb/metadata_manager.h"
#include "pedrodb/record_format.h"
#include <pedrolib/concurrent/latch.h>

#include <map>
#include <memory>
#include <mutex>
#include <pedrolib/executor/thread_pool_executor.h>
#include <unordered_map>
#include <vector>

namespace pedrodb {

struct RecordDir {

  struct Hash {
    size_t operator()(const RecordDir &other) const noexcept { return other.h; }
  };

  uint32_t h{};
  std::string key;
  mutable record::Location loc;
  mutable uint32_t size{};

  bool operator==(const RecordDir &other) const noexcept {
    return h == other.h;
  }
};

struct Record {
  uint32_t h;
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
  CompactState state;
};

class DBImpl : public DB {
  mutable std::mutex mu_;

  Options options_;
  ArrayBuffer buffer_;
  uint64_t sync_worker_{};
  uint64_t compact_worker_{};
  std::unique_ptr<ReadCache> read_cache_;
  std::unique_ptr<FileManager> file_manager_;
  std::unique_ptr<MetadataManager> metadata_manager_;
  std::shared_ptr<pedrolib::Executor> executor_;
  std::unordered_multiset<RecordDir, RecordDir::Hash> indices_;

  // for compaction.
  std::unordered_set<file_t> compact_tasks_;
  std::unordered_map<file_t, CompactHint> compact_hints_;

  Status Recovery(file_t id);

  void Compact(file_t id);

  auto GetMetadataIterator(uint32_t h, std::string_view key)
      -> decltype(indices_.begin());

  Status Recovery(file_t id, RecordEntry entry);

public:
  ~DBImpl() override;

  explicit DBImpl(const Options &options, const std::string &name);

  Status HandlePut(const WriteOptions &options, uint32_t h,
                   std::string_view key, std::string_view value);

  Status HandleGet(const ReadOptions &options, uint32_t h, std::string_view key,
                   std::string *value);

  auto AcquireLock() const { return std::unique_lock{mu_}; }

  Status FetchRecord(ReadableFile *file, const RecordDir &metadata,
                     std::string *value);

  Status Recovery();

  Status Flush();

  Status Compact() override;

  double CacheHitRatio() const { return read_cache_->HitRatio(); }

  Status Init();

  Status Get(const ReadOptions &options, std::string_view key,
             std::string *value) override;

  Status Put(const WriteOptions &options, std::string_view key,
             std::string_view value) override;

  Status Delete(const WriteOptions &options, std::string_view key) override;
  
  std::vector<file_t> GetFiles();
  
  void UpdateUnused(file_t id, size_t unused);
  
  std::vector<file_t> PollCompactTask();
  
  Status CompactBatch(file_t id, const std::vector<Record> &records);
};
} // namespace pedrodb

#endif // PEDRODB_DB_IMPL_H
