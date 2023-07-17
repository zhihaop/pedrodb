#ifndef PEDRODB_SEGMENT_DB_H
#define PEDRODB_SEGMENT_DB_H

#include <cstdint>
#include "pedrodb/db_impl.h"

namespace pedrodb {
class SegmentDB : public DB {
  std::vector<std::shared_ptr<DBImpl>> segments_;
  std::shared_ptr<Executor> executor_;

 public:
  explicit SegmentDB(size_t n) : segments_(n) {}

  ~SegmentDB() override = default;

  static Status Open(const Options& options, const std::string& path, size_t n,
                     std::shared_ptr<DB>* db);

  DBImpl* GetDB(size_t h) { return segments_[h % segments_.size()].get(); }

  Status Get(const ReadOptions& options, std::string_view key,
             std::string* value) override;

  Status Put(const WriteOptions& options, std::string_view key,
             std::string_view value) override;

  Status Delete(const WriteOptions& options, std::string_view key) override;

  Status Flush() override;

  Status Compact() override;

  Status GetIterator(EntryIterator::Ptr* ptr) override;
};
}  // namespace pedrodb

#endif  // PEDRODB_SEGMENT_DB_H
