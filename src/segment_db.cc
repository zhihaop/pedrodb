#include "pedrodb/segment_db.h"
#include <pedrolib/concurrent/latch.h>
#include "pedrodb/db_impl.h"

namespace pedrodb {

Status SegmentDB::Open(const Options& options, const std::string& path,
                       size_t n, std::shared_ptr<DB>* db) {
  auto impl = std::make_shared<SegmentDB>(n);
  impl->executor_ = std::make_shared<DefaultExecutor>();

  auto& segments = impl->segments_;
  auto& executor = impl->executor_;

  pedrolib::Latch latch(n);
  std::vector<Status> status(n, Status::kOk);
  for (int i = 0; i < n; ++i) {
    std::shared_ptr<DB> raw;
    std::string segment_name = fmt::format("{}.seg.{}.db", path, i);
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

Status SegmentDB::Get(const ReadOptions& options, std::string_view key,
                      std::string* value) {
  return GetDB(Hash(key))->Get(options, key, value);
}

Status SegmentDB::Put(const WriteOptions& options, std::string_view key,
                      std::string_view value) {
  return GetDB(Hash(key))->Put(options, key, value);
}

Status SegmentDB::Delete(const WriteOptions& options, std::string_view key) {
  return GetDB(Hash(key))->Delete(options, key);
}

Status SegmentDB::Compact() {
  Latch latch(segments_.size());
  for (auto& segment : segments_) {
    executor_->Schedule([&] {
      PEDRODB_IGNORE_ERROR(segment->Compact());
      latch.CountDown();
    });
  }
  latch.Await();
  return Status::kOk;
}

Status SegmentDB::Flush() {
  Latch latch(segments_.size());
  for (auto& segment : segments_) {
    executor_->Schedule([&] {
      PEDRODB_IGNORE_ERROR(segment->Flush());
      latch.CountDown();
    });
  }
  latch.Await();
  return Status::kOk;
}

Status SegmentDB::GetIterator(EntryIterator::Ptr* iterator) {
  struct IteratorImpl : public EntryIterator {
    std::vector<std::weak_ptr<DB>> db;
    size_t db_index{};

    EntryIterator::Ptr current;
    std::shared_ptr<DB> lock;

    ~IteratorImpl() override = default;
    bool Valid() override {
      for (;;) {
        if (current == nullptr) {
          if (db_index >= db.size()) {
            return false;
          }

          lock = db[db_index++].lock();
          if (lock == nullptr) {
            continue;
          }

          lock->GetIterator(&current);
        }

        if (current->Valid()) {
          return true;
        }

        current = nullptr;
        lock = nullptr;
      }
    }

    record::EntryView Next() override { return current->Next(); }
    void Close() override {
      current = nullptr;
      lock = nullptr;
    }
  };

  auto ptr = new IteratorImpl();
  ptr->db.resize(segments_.size());
  for (size_t i = 0; i < segments_.size(); ++i) {
    ptr->db[i] = segments_[i];
  }

  iterator->reset(ptr);
  return Status::kOk;
}
}  // namespace pedrodb