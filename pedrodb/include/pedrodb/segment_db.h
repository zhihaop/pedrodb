#ifndef PEDRODB_SEGMENT_DB_H
#define PEDRODB_SEGMENT_DB_H

#include "pedrodb/db.h"
#include <cstdint>

namespace pedrodb {
class SegmentDB : public DB {
  std::vector<std::shared_ptr<DB>> segments_;

public:
  explicit SegmentDB(size_t n) : segments_(n) {}

  ~SegmentDB() override = default;

  static Status Open(const Options &options, const std::string &path, size_t n,
                     std::shared_ptr<DB> *db) {
    auto impl = std::make_shared<SegmentDB>(n);

    auto &segments = impl->segments_;
    for (int i = 0; i < n; ++i) {
      auto status = DB::Open(options, fmt::format("{}_segment{}.db", path, i),
                             &segments[i]);
      if (status != Status::kOk) {
        return status;
      }
    }
    *db = impl;
    return Status::kOk;
  }

  DB *GetDB(const std::string &key) {
    size_t h = std::hash<std::string>()(key);
    return segments_[h % segments_.size()].get();
  }

  Status Get(const ReadOptions &options, const std::string &key,
             std::string *value) override {
    return GetDB(key)->Get(options, key, value);
  }

  Status Put(const WriteOptions &options, const std::string &key,
             std::string_view value) override {
    return GetDB(key)->Put(options, key, value);
  }

  Status Delete(const WriteOptions &options, const std::string &key) override {
    return GetDB(key)->Delete(options, key);
  }

  Status Compact() override {
    for (auto &segment : segments_) {
      PEDRODB_IGNORE_ERROR(segment->Compact());
    }
    return Status::kOk;
  }
};
} // namespace pedrodb

#endif // PEDRODB_SEGMENT_DB_H
