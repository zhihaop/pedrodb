
#include <memory>

#include "pedrodb/compress.h"
#include "pedrodb/db_impl.h"

namespace pedrodb {

Status DBImpl::Get(const ReadOptions& options, std::string_view key,
                   std::string* value) {
  return HandleGet(options, key, value);
}

Status DBImpl::Put(const WriteOptions& options, std::string_view key,
                   std::string_view value) {
  return HandlePut(options, key, value);
}

Status DBImpl::Delete(const WriteOptions& options, std::string_view key) {
  return HandlePut(options, key, {});
}

void DBImpl::UpdateUnused(record::Location loc, size_t unused) {
  auto& hint = compact_hints_[loc.id];
  hint.free_bytes += unused;
  if (hint.free_bytes >= options_.compaction.threshold_bytes) {
    if (hint.state == CompactState::kNop) {
      hint.state = CompactState::kQueued;
      compact_tasks_.emplace_back(loc.id);
    }
  }
}

Status DBImpl::Init() {
  Status status = metadata_manager_->Init();
  if (status != Status::kOk) {
    return status;
  }

  PEDRODB_INFO("metadata manager init success");
  status = file_manager_->Init();
  if (status != Status::kOk) {
    return status;
  }

  PEDRODB_INFO("file manager init success");
  status = Recovery();
  if (status != Status::kOk) {
    return status;
  }

  PEDRODB_INFO("recovery success");

  std::weak_ptr<DBImpl> weak = shared_from_this();

  sync_worker_ = executor_->ScheduleEvery(
      options_.sync_interval, options_.sync_interval,
      [weak, failed_count = 0]() mutable {
        auto ptr = weak.lock();
        if (ptr == nullptr) {
          return;
        }

        auto err = ptr->file_manager_->Sync();
        if (err != Status::kOk) {
          failed_count++;
        }

        if (failed_count > ptr->options_.sync_max_io_error) {
          ptr->readonly_ = true;
          PEDRODB_ERROR("database is readonly because too many io error");
        }
      });

  compact_worker_ = executor_->ScheduleEvery(
      options_.compaction.interval, options_.compaction.interval, [weak] {
        auto ptr = weak.lock();
        if (ptr == nullptr) {
          return;
        }

        auto lock = ptr->AcquireLock();
        auto task = ptr->PollCompactTask();
        lock.unlock();

        std::for_each(task.begin(), task.end(), [weak, ptr](auto task) {
          ptr->executor_->Schedule([weak, task = std::move(task)] {
            auto ptr = weak.lock();
            if (ptr == nullptr) {
              return;
            }
            ptr->Compact(task);
          });
        });
      });

  return Status::kOk;
}

std::vector<file_id_t> DBImpl::PollCompactTask() {
  auto tasks = compact_tasks_;
  compact_tasks_.clear();
  for (auto file : tasks) {
    compact_hints_[file].state = CompactState::kScheduling;
  }
  return {tasks.begin(), tasks.end()};
}

Status DBImpl::Compact() {
  auto lock = AcquireLock();
  auto tasks = PollCompactTask();
  lock.unlock();

  std::sort(tasks.begin(), tasks.end());
  for (auto file : tasks) {
    Compact(file);
  }

  return Status::kOk;
}

DBImpl::DBImpl(const Options& options, const std::string& name)
    : options_(options), read_cache_(options.read_cache) {
  executor_ = options_.executor;
  metadata_manager_ = std::make_shared<MetadataManager>(name);
  file_manager_ = std::make_shared<FileManager>(metadata_manager_, executor_,
                                                options.max_open_files);

  read_cache_.SetFileOpener([this](file_id_t f, ReadableFile::Ptr* file) {
    return file_manager_->AcquireDataFile(f, file);
  });
}

DBImpl::~DBImpl() {
  executor_->ScheduleCancel(sync_worker_);
  executor_->ScheduleCancel(compact_worker_);
  file_manager_->Flush(true);
}

Status DBImpl::Recovery(file_id_t id) {
  ReadableFile::Ptr file;
  if (file_manager_->AcquireIndexFile(id, &file) == Status::kOk) {
    auto iter = IndexIterator(file);
    while (iter.Valid()) {
      Recovery(id, iter.Next());
    }
    return Status::kOk;
  }

  if (file_manager_->AcquireDataFile(id, &file) == Status::kOk) {
    auto iter = RecordIterator(file);
    while (iter.Valid()) {
      index::EntryView view;
      view.offset = iter.GetOffset();

      auto next = iter.Next();
      view.len = next.SizeOf();
      view.type = next.type;
      view.key = next.key;
      Recovery(id, view);
    }
    file_manager_->ReleaseDataFile(id);
    return Status::kOk;
  }

  return Status::kIOError;
}

void DBImpl::Compact(file_id_t id) {
  if (readonly_) {
    return;
  }

  ReadableFile::Ptr file;
  auto stat = file_manager_->AcquireDataFile(id, &file);
  if (stat != Status::kOk) {
    return;
  }

  PEDRODB_TRACE("start compacting {}", id);
  {
    auto lock = AcquireLock();
    auto& hints = compact_hints_[id];
    hints.state = CompactState::kCompacting;
  }

  auto iter = RecordIterator(file);
  while (iter.Valid()) {
    uint32_t offset = iter.GetOffset();
    auto next = iter.Next();
    if (next.type != record::Type::kSet) {
      continue;
    }

    // check if the key is valid.
    {
      auto lock = AcquireLock();
      auto it = indices_.find(next.key);
      if (it == indices_.end()) {
        continue;
      }

      // steal entry.
      auto& dir = it.value();
      if (dir.loc != record::Location(id, offset)) {
        continue;
      }
    }

    // move to active file because this file will be removed.
    record::Location loc;
    auto status = file_manager_->Append(next, &loc);
    if (status != Status::kOk) {
      return;
    }

    // update the memory index
    {
      auto lock = AcquireLock();
      auto it = indices_.find(next.key);
      if (it != indices_.end()) {
        if (it.value().loc < loc) {
          record::Dir dir;
          dir.loc = loc;
          dir.entry_size = next.SizeOf();
          indices_[next.key] = dir;
        } else {
          compact_hints_[loc.id].free_bytes += next.SizeOf();
        }
      } else {
        // the key has been deleted.
      }
    }
  }

  // erase compaction_state.
  {
    auto lock = AcquireLock();
    compact_hints_.erase(id);
    PEDRODB_IGNORE_ERROR(file_manager_->RemoveFile(id));
  }

  PEDRODB_TRACE("end compacting: {}", id);
}

Status DBImpl::HandlePut(const WriteOptions& options, std::string_view key,
                         std::string_view value) {
  if (readonly_) {
    return Status::kNotSupported;
  }

  record::EntryView entry;
  entry.type = value.empty() ? record::Type::kDelete : record::Type::kSet;
  entry.key = key;

  std::string compressed;
  if (options_.compress_value) {
    Compress(value, &compressed);
    entry.value = compressed;
  } else {
    entry.value = value;
  }
  entry.checksum = record::EntryView::Checksum(entry.key, entry.value);

  if (entry.SizeOf() > kMaxFileBytes) {
    PEDRODB_ERROR("key or value is too big");
    return Status::kNotSupported;
  }

  uint32_t timestamp = 0;
  entry.timestamp = timestamp;

  record::Location loc{};
  auto status = file_manager_->Append(entry, &loc);
  if (status != Status::kOk) {
    return status;
  }

  auto lock = AcquireLock();
  auto it = indices_.find(key);

  // only for insert.
  if (it == indices_.end()) {
    // invalid deletion.
    if (value.empty()) {
      UpdateUnused(loc, entry.SizeOf());
      return Status::kNotFound;
    }

    // insert.
    auto& dir = indices_[key];
    dir.loc = loc;
    dir.entry_size = entry.SizeOf();

    lock.unlock();
    if (options.sync) {
      file_manager_->Sync();
    }
    return Status::kOk;
  }

  // replace or delete.
  auto dir = it.value();
  UpdateUnused(dir.loc, dir.entry_size);

  // delete.
  indices_.erase(it);

  if (!value.empty()) {
    dir.loc = loc;
    dir.entry_size = entry.SizeOf();
    indices_[key] = dir;
  }

  lock.unlock();
  if (options.sync) {
    file_manager_->Sync();
  }
  return Status::kOk;
}

Status DBImpl::Flush() {
  return file_manager_->Flush(true);
}

Status DBImpl::Recovery() {
  for (auto file : metadata_manager_->GetFiles()) {
    PEDRODB_TRACE("crash recover: file {}", file);
    auto status = Recovery(file);
    if (status != Status::kOk) {
      return status;
    }
    PEDRODB_INFO("crash recover success: record[{}]", indices_.size());
  }
  return Status::kOk;
}

Status DBImpl::HandleGet(const ReadOptions& options, std::string_view key,
                         std::string* value) {

  auto lock = AcquireLock();
  auto it = indices_.find(key);
  if (it == indices_.end()) {
    return Status::kNotFound;
  }
  auto dir = it.value();
  lock.unlock();

  if (!options.use_read_cache || !options_.read_cache.enable) {
    ReadableFile::Ptr file;
    auto stat = file_manager_->AcquireDataFile(dir.loc.id, &file);
    if (stat != Status::kOk) {
      PEDRODB_ERROR("cannot get file {}", dir.loc.id);
      return stat;
    }

    RecordIterator iterator(file);
    iterator.Seek(dir.loc.offset);
    if (!iterator.Valid()) {
      PEDRODB_ERROR("failed to read from iterator");
      return Status::kCorruption;
    }

    auto entry = iterator.Next();
    if (!entry.Validate()) {
      PEDRODB_ERROR("checksum validation error");
      return Status::kCorruption;
    }

    if (options_.compress_value) {
      Uncompress(entry.value, value);
    } else {
      value->assign(entry.value);
    }
    return Status::kOk;
  }

  ReadCache::Context ctx(dir.loc, dir.entry_size);
  auto stat = read_cache_.Get(ctx);
  if (stat != Status::kOk) {
    return stat;
  }

  if (!ctx.GetEntry().Validate()) {
    PEDRODB_ERROR("checksum validation error");
    return Status::kCorruption;
  }

  if (options_.compress_value) {
    Uncompress(ctx.GetEntry().value, value);
  } else {
    value->assign(ctx.GetEntry().value);
  }
  return Status::kOk;
}

void DBImpl::Recovery(file_id_t id, index::EntryView entry) {
  record::Location loc(id, entry.offset);

  auto it = indices_.find(entry.key);
  if (entry.type == record::Type::kSet) {
    if (it == indices_.end()) {
      auto& dir = indices_[entry.key];
      dir.entry_size = entry.len;
      dir.loc = loc;
      return;
    }

    // indices has the newer version data.
    auto dir = it.value();
    if (dir.loc > loc) {
      UpdateUnused(loc, entry.len);
      return;
    }

    // never happen.
    if (dir.loc == loc) {
      PEDRODB_FATAL("meta.loc == loc should never happened");
    }

    // indices has the elder version data.
    UpdateUnused(dir.loc, dir.entry_size);

    // update indices.
    dir.loc = loc;
    dir.entry_size = entry.len;
    indices_[entry.key] = dir;
  }

  // a tombstone of deletion.
  if (entry.type == record::Type::kDelete) {
    UpdateUnused(loc, entry.len);
    if (it == indices_.end()) {
      return;
    }

    // Recover(file_id_t) should be called monotonously,
    // therefore entry.loc is always monotonously increased.
    // should not delete the latest version data.
    auto dir = it.value();

    if (dir.loc > loc) {
      return;
    }

    UpdateUnused(dir.loc, dir.entry_size);
    indices_.erase(it);
  }
}

Status DBImpl::GetIterator(EntryIterator::Ptr* iterator) {
  struct EntryIteratorImpl : public EntryIterator {
    tsl::htrie_map<char, record::Dir> indices_;
    tsl::htrie_map<char, record::Dir>::iterator it_;
    record::EntryView next_;
    DBImpl* parent_;
    FileManager* file_manager_;

    explicit EntryIteratorImpl(DBImpl* parent)
        : parent_(parent), file_manager_(parent_->file_manager_.get()) {
      auto lock = parent_->AcquireLock();
      indices_ = parent_->indices_;
      it_ = indices_.begin();
    }

    ~EntryIteratorImpl() override = default;

    bool Valid() override {
      for (;;) {
        if (it_ == indices_.end()) {
          return false;
        }

        auto dir = (it_++).value();
        ReadableFile::Ptr file;
        auto status = file_manager_->AcquireDataFile(dir.loc.id, &file);
        if (status != Status::kOk) {
          continue;
        }

        auto iter = RecordIterator(file);
        if (!iter.Valid()) {
          continue;
        }

        next_ = iter.Next();
        if (!next_.Validate()) {
          continue;
        }

        return true;
      }
    }

    record::EntryView Next() override { return next_; }

    void Close() override {}
  };

  *iterator = std::make_unique<EntryIteratorImpl>(this);
  return Status::kOk;
}
}  // namespace pedrodb