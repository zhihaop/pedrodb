#ifndef PEDRODB_SEGMENT_DB_H
#define PEDRODB_SEGMENT_DB_H

#include "pedrodb/db.h"
#include <cstdint>

namespace pedrodb {
class SegmentDB : public DB {
  std::vector<std::shared_ptr<DBImpl>> segments_;
  std::shared_ptr<Executor> executor_;

public:
  explicit SegmentDB(size_t n) : segments_(n) {}

  ~SegmentDB() override = default;

  static Status Open(const Options &options, const std::string &path, size_t n,
                     std::shared_ptr<DB> *db) {
    auto impl = std::make_shared<SegmentDB>(n);
    impl->executor_ = options.executor;

    auto &segments = impl->segments_;
    auto &executor = impl->executor_;

    pedrolib::Latch latch(n);
    std::vector<Status> status(n, Status::kOk);
    for (int i = 0; i < n; ++i) {
      std::shared_ptr<DB> raw;
      std::string segment_name = fmt::format("{}_segment{}.db", path, i);
      segments[i] = std::make_shared<DBImpl>(options, segment_name);

      executor->Schedule([i, &segments, &status, &latch] {
        status[i] = segments[i]->Init();
        latch.CountDown();
      });
    }
    latch.Await();

    for (int i = 0; i < n; ++i) {
      if (status[i] != Status::kOk) {
        return status[i];
      }
    }

    *db = impl;
    return Status::kOk;
  }

  DBImpl *GetDB(size_t h) { return segments_[h % segments_.size()].get(); }
  
  Status Get(const ReadOptions &options, std::string_view key,
             std::string *value) override {
    auto h = Hash(key);
    auto db = GetDB(h);
    auto lock = db->AcquireLock();
    return db->HandleGet(options, h, key, value);
  }

  Status Put(const WriteOptions &options, std::string_view key,
             std::string_view value) override {
    auto h = Hash(key);
    auto db = GetDB(h);
    auto lock = db->AcquireLock();
    return db->HandlePut(options, h, key, value);
  }

  Status Delete(const WriteOptions &options, std::string_view key) override {
    auto h = Hash(key);
    auto db = GetDB(h);
    auto lock = db->AcquireLock();
    return db->HandlePut(options, h, key, {});
  }

  Status Compact() override {
    pedrolib::Latch latch(segments_.size());
    for (auto &segment : segments_) {
      executor_->Schedule([&] {
        PEDRODB_IGNORE_ERROR(segment->Compact());
        latch.CountDown();
      });
    }
    latch.Await();
    return Status::kOk;
  }
};
} // namespace pedrodb

#endif // PEDRODB_SEGMENT_DB_H
