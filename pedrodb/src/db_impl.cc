
#include <memory>
#include <utility>

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
  hint.unused += unused;
  if (hint.unused >= options_.compaction_threshold_bytes) {
    if (hint.state == CompactState::kNop) {
      if (!compact_tasks_.count(loc.id)) {
        compact_tasks_.emplace(loc.id);
      }
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

  sync_worker_ = executor_->ScheduleEvery(
      options_.sync_interval, options_.sync_interval, [this] { Flush(); });

  compact_worker_ = executor_->ScheduleEvery(
      options_.compact_interval, options_.compact_interval, [this] {
        auto lock = AcquireLock();
        auto task = PollCompactTask();
        lock.unlock();

        std::for_each(task.begin(), task.end(), [this](auto task) {
          executor_->Schedule([this, task] { Compact(task); });
        });
      });

  return Status::kOk;
}

std::vector<file_id_t> DBImpl::GetFiles() {
  return metadata_manager_->GetFiles();
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
    : options_(options) {
  executor_ = options_.executor;
  metadata_manager_ = std::make_unique<MetadataManager>(name);
  file_manager_ = std::make_unique<FileManager>(
      metadata_manager_.get(), executor_.get(), options.max_open_files);
}

DBImpl::~DBImpl() {
  executor_->ScheduleCancel(sync_worker_);
  executor_->ScheduleCancel(compact_worker_);
  file_manager_->Flush(true);
}

Status DBImpl::Recovery(file_id_t id) {
  ReadableFile::Ptr file;
  if (file_manager_->AcquireIndexFile(id, &file) == Status::kOk) {
    auto iter = IndexIterator(file.get());
    while (iter.Valid()) {
      Recovery(id, iter.Next());
    }
    return Status::kOk;
  }

  if (file_manager_->AcquireDataFile(id, &file) == Status::kOk) {
    auto iter = RecordIterator(file.get());
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

Status DBImpl::CompactBatch(file_id_t id, const std::vector<Record>& records) {
  auto lock = AcquireLock();

  auto& hints = compact_hints_[id];
  hints.state = CompactState::kCompacting;
  for (auto& r : records) {
    auto it = indices_.find(r.key);
    if (it == indices_.end()) {
      continue;
    }

    // steal entry.
    auto& dir = it->second;
    if (dir.loc != r.location) {
      continue;
    }

    // move to active file because this file will be removed.
    record::EntryView entry;
    entry.type = record::Type::kSet;
    entry.key = r.key;
    entry.value = r.value;
    entry.timestamp = r.timestamp;
    entry.checksum = r.checksum;

    hints.unused += entry.SizeOf();

    record::Location loc;
    auto status = file_manager_->WriteActiveFile(entry, &loc);
    if (status != Status::kOk) {
      return status;
    }

    dir.loc = loc;
    dir.entry_size = entry.SizeOf();
  }
  return Status::kOk;
}

void DBImpl::Compact(file_id_t id) {
  ReadableFile::Ptr file;
  auto stat = file_manager_->AcquireDataFile(id, &file);
  if (stat != Status::kOk) {
    return;
  }

  PEDRODB_TRACE("start compacting {}", id);

  std::vector<Record> batch;
  size_t batch_bytes = 0;

  for (auto iter = RecordIterator(file.get()); iter.Valid();) {
    uint32_t offset = iter.GetOffset();
    auto next = iter.Next();
    if (next.type != record::Type::kSet) {
      continue;
    }
    batch_bytes += next.SizeOf();

    Record record;
    record.key.assign(next.key);
    record.value.assign(next.value);
    record.location = {id, offset};
    record.timestamp = next.checksum;
    batch.emplace_back(std::move(record));

    if (batch_bytes >= options_.compaction_batch_bytes) {
      CompactBatch(id, batch);
      batch.clear();
      batch_bytes = 0;
    }
  }

  if (batch_bytes > 0) {
    CompactBatch(id, batch);
    batch.clear();
  }

  // erase compaction_state.
  {
    auto lock = AcquireLock();
    compact_hints_.erase(id);
    compact_tasks_.erase(id);

    PEDRODB_IGNORE_ERROR(file_manager_->RemoveFile(id));
  }

  PEDRODB_TRACE("end compacting: {}", id);
}

Status DBImpl::HandlePut(const WriteOptions& options, std::string_view key,
                         std::string_view value) {
  record::EntryView entry;
  entry.type = value.empty() ? record::Type::kDelete : record::Type::kSet;
  entry.key = key;

  std::string compressed;
  if (options_.compress) {
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
  auto status = file_manager_->WriteActiveFile(entry, &loc);
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
  auto dir = it->second;
  UpdateUnused(dir.loc, dir.entry_size);

  if (value.empty()) {
    // delete.
    indices_.erase(it);
  } else {
    // replace.
    it->second.loc = loc;
    it->second.entry_size = entry.SizeOf();
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
  for (auto file : GetFiles()) {
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

  record::Dir dir;
  {
    auto lock = AcquireLock();
    auto it = indices_.find(key);
    if (it == indices_.end()) {
      return Status::kNotFound;
    }
    dir = it->second;
  }

  ReadableFile::Ptr file;
  auto stat = file_manager_->AcquireDataFile(dir.loc.id, &file);
  if (stat != Status::kOk) {
    PEDRODB_ERROR("cannot get file {}", dir.loc.id);
    return stat;
  }

  RecordIterator iterator(file.get());
  iterator.Seek(dir.loc.offset);
  if (!iterator.Valid()) {
    return Status::kCorruption;
  }

  auto entry = iterator.Next();
  if (!entry.Validate()) {
    return Status::kCorruption;
  }

  if (options_.compress) {
    Uncompress(entry.value, value);
  } else {
    value->assign(entry.value);
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
    auto dir = it->second;
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
    it->second.loc = loc;
    it->second.entry_size = entry.len;
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
    auto& dir = it->second;
    if (dir.loc > loc) {
      return;
    }

    UpdateUnused(dir.loc, dir.entry_size);
    indices_.erase(it);
  }
}

Status DBImpl::GetIterator(EntryIterator::Ptr* iterator) {

  struct EntryIteratorImpl : public EntryIterator {
    std::vector<file_id_t> files_;
    size_t index_{};
    DBImpl* parent_;

    file_id_t current_id_{};
    ReadableFile::Ptr current_file_{};
    std::unique_ptr<RecordIterator> current_iterator_{};

    explicit EntryIteratorImpl(DBImpl* parent)
        : parent_(parent), files_(parent->GetFiles()) {}

    ~EntryIteratorImpl() override = default;

    // TODO using linked hash map to help scan
    bool Valid() override {
      for (;;) {
        if (current_iterator_ == nullptr) {
          if (index_ >= files_.size()) {
            return false;
          }
          current_id_ = files_[index_++];
          auto status = parent_->file_manager_->AcquireDataFile(current_id_,
                                                                &current_file_);
          if (status != Status::kOk) {
            return false;
          }

          current_iterator_ =
              std::make_unique<RecordIterator>(current_file_.get());
        }

        while (current_iterator_->Valid()) {
          return true;
        }

        current_iterator_ = nullptr;
        current_file_ = nullptr;
      }
    }

    // TODO: fix compression
    record::EntryView Next() override { return current_iterator_->Next(); }

    void Close() override {
      current_iterator_ = nullptr;
      current_file_ = nullptr;
    }
  };

  *iterator = std::make_unique<EntryIteratorImpl>(this);
  return Status::kOk;
}
}  // namespace pedrodb