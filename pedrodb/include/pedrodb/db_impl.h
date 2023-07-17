#ifndef PEDRODB_DB_IMPL_H
#define PEDRODB_DB_IMPL_H

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
  uint32_t h;
  std::string key;
  std::string value;
  record::Location location{};
  uint32_t timestamp{};

  Record(uint32_t h, std::string key, std::string value,
         const record::Location& location, uint32_t timestamp);
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

class DBImpl : public DB {
  mutable std::mutex mu_;

  Options options_;
  uint64_t sync_worker_{};
  uint64_t compact_worker_{};
  std::unique_ptr<FileManager> file_manager_;
  std::unique_ptr<MetadataManager> metadata_manager_;
  std::shared_ptr<pedrolib::Executor> executor_;
  std::unordered_map<std::string, record::Dir> indices_;

  // for compaction.
  std::unordered_set<file_id_t> compact_tasks_;
  std::unordered_map<file_id_t, CompactHint> compact_hints_;

  void Recovery(file_id_t id, index::EntryView entry);
  Status Recovery(file_id_t id);

  void Compact(file_id_t id);

  Status FetchRecord(ReadableFile* file, const record::Location& loc,
                     size_t size, std::string* value);

  std::vector<file_id_t> GetFiles();

  void UpdateUnused(file_id_t id, size_t unused);

  std::vector<file_id_t> PollCompactTask();

  Status CompactBatch(file_id_t id, const std::vector<Record>& records);

  Status Recovery();

  auto AcquireLock() const { return std::unique_lock{mu_}; }

  Status HandlePut(const WriteOptions& options, const std::string& key,
                   std::string_view value);

  Status HandleGet(const ReadOptions& options, const std::string& key,
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
