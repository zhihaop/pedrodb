#ifndef PEDRODB_DB_IMPL_H
#define PEDRODB_DB_IMPL_H

#include "pedrodb/cache/read_cache.h"
#include "pedrodb/db.h"
#include "pedrodb/defines.h"
#include "pedrodb/file/writable_file.h"
#include "pedrodb/file_manager.h"
#include "pedrodb/header.h"
#include "pedrodb/iterator/record_iterator.h"
#include "pedrodb/logger/logger.h"
#include "pedrodb/metadata_manager.h"
#include <pedrolib/concurrent/latch.h>

#include <map>
#include <memory>
#include <pedrolib/executor/thread_pool_executor.h>
#include <shared_mutex>
#include <unordered_map>
#include <vector>

namespace pedrodb {

struct ValueMetadata {
  ValueLocation location;
  uint32_t length{};
  uint32_t timestamp{};
};

struct CompactionState {
  size_t unused{};
  bool compacting{};
};

class DBImpl : public DB {
  mutable std::shared_mutex mu_;

  Options options_;
  ArrayBuffer buffer_;
  uint64_t sync_worker_{};
  std::unique_ptr<ReadCache> read_cache_;
  std::unique_ptr<FileManager> file_manager_;
  std::unique_ptr<MetadataManager> metadata_;
  std::unique_ptr<pedrolib::Executor> executor_;
  std::unordered_map<std::string, ValueMetadata> indices_;
  std::unordered_map<uint32_t, CompactionState> compaction_state_;

  Status WriteDisk(Buffer *buf, WritableFile *file, WriteOptions options);

  Status RebuildIndices(uint32_t id);

  void Compact(uint32_t id);

  Status GetActiveFile(WritableFile **file, uint32_t *id, size_t record_length);

  Status HandlePut(const WriteOptions &options, const std::string &key,
                     std::string_view value);
public:
  ~DBImpl() override;

  explicit DBImpl(const Options &options, const std::string &name);

  Status Compact() override;

  double CacheHitRatio() const { return read_cache_->HitRatio(); }

  Status Init();

  auto AcquireLock() const { return std::unique_lock{mu_}; }
  auto AcquireReadLock() const { return std::shared_lock{mu_}; }

  Status Get(const ReadOptions &options, const std::string &key,
             std::string *value) override;

  void UpdateCompactionHint(ValueMetadata metadata);

  Status Put(const WriteOptions &options, const std::string &key,
             std::string_view value) override;

  Status Delete(const WriteOptions &options, const std::string &key) override;
};
} // namespace pedrodb

#endif // PEDRODB_DB_IMPL_H
