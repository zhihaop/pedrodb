#ifndef PEDRODB_SEGMENT_DB_H
#define PEDRODB_SEGMENT_DB_H

#include "pedrodb/db.h"
#include <cstdint>

namespace pedrodb {
class SegmentDB : public DB {
  std::vector<std::shared_ptr<DBImpl>> segments_;

public:
  explicit SegmentDB(size_t n) : segments_(n) {}

  ~SegmentDB() override = default;

  static Status Open(const Options &options, const std::string &path, size_t n,
                     std::shared_ptr<DB> *db) {
    auto impl = std::make_shared<SegmentDB>(n);

    auto &segments = impl->segments_;
    for (int i = 0; i < n; ++i) {
      std::shared_ptr<DB> raw;
      std::string segment_name = fmt::format("{}_segment{}.db", path, i);
      auto status = DB::Open(options, segment_name, &raw);

      segments[i] = std::dynamic_pointer_cast<DBImpl>(raw);
      if (status != Status::kOk) {
        return status;
      }
    }
    *db = impl;
    return Status::kOk;
  }

  DBImpl *GetDB(size_t h) { return segments_[h % segments_.size()].get(); }

  static size_t GetHash(std::string_view key) noexcept {
    return std::hash<std::string_view>()(key);
  }

  Status Get(const ReadOptions &options, std::string_view key,
             std::string *value) override {
    size_t h = GetHash(key);
    auto db = GetDB(h);
    auto lock = db->AcquireLock();
    return db->HandleGet(options, h, key, value);
  }

  Status Put(const WriteOptions &options, std::string_view key,
             std::string_view value) override {
    size_t h = GetHash(key);
    auto db = GetDB(h);
    auto lock = db->AcquireLock();
    return db->HandlePut(options, h, key, value);
  }

  Status Delete(const WriteOptions &options, std::string_view key) override {
    size_t h = GetHash(key);
    auto db = GetDB(h);
    auto lock = db->AcquireLock();
    return db->HandlePut(options, h, key, {});
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
