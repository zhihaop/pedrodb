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

struct KeyValueMetadata {
  std::string key;
  ValueLocation location;
  uint32_t length{};
  uint32_t timestamp{};
};

struct KeyValueRecord {
  uint32_t h;
  std::string key;
  std::string value;
  ValueLocation location{};
  uint32_t timestamp{};
};

struct CompactionState {
  size_t unused{};
  bool compacting{};
};

class DBImpl : public DB {
  mutable std::mutex mu_;

  Options options_;
  ArrayBuffer buffer_;
  uint64_t sync_worker_{};
  std::unique_ptr<ReadCache> read_cache_;
  std::unique_ptr<FileManager> file_manager_;
  std::unique_ptr<MetadataManager> metadata_manager_;
  std::shared_ptr<pedrolib::Executor> executor_;
  std::unordered_multimap<uint32_t, KeyValueMetadata> indices_;
  std::unordered_map<uint32_t, CompactionState> compaction_state_;

  Status CompactBatch(const std::vector<KeyValueRecord> &records);

  Status Recovery(uint32_t id);

  void Compact(uint32_t id);

  auto GetMetadataIterator(uint32_t h, std::string_view key)
      -> decltype(indices_.begin());

public:
  ~DBImpl() override;

  explicit DBImpl(const Options &options, const std::string &name);

  Status HandlePut(const WriteOptions &options, uint32_t h,
                   std::string_view key, std::string_view value);

  Status HandleGet(const ReadOptions &options, uint32_t h, std::string_view key,
                   std::string *value);

  auto AcquireLock() const { return std::unique_lock{mu_}; }

  Status FetchRecord(ReadableFile *file, const KeyValueMetadata &metadata,
                     std::string *value);

  Status Recovery();

  Status Flush();

  Status Compact() override;

  double CacheHitRatio() const { return read_cache_->HitRatio(); }

  Status Init();

  Status Get(const ReadOptions &options, std::string_view key,
             std::string *value) override;

  void UpdateCompactionHint(const KeyValueMetadata &metadata);

  Status Put(const WriteOptions &options, std::string_view key,
             std::string_view value) override;

  Status Delete(const WriteOptions &options, std::string_view key) override;
};
} // namespace pedrodb

#endif // PEDRODB_DB_IMPL_H
