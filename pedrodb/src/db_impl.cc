#include "pedrodb/db_impl.h"

namespace pedrodb {

Status DBImpl::Get(const ReadOptions &options, std::string_view key,
                   std::string *value) {
  auto lock = AcquireLock();
  return HandleGet(options, Hash(key), key, value);
}

Status DBImpl::Put(const WriteOptions &options, std::string_view key,
                   std::string_view value) {
  auto lock = AcquireLock();
  return HandlePut(options, Hash(key), key, value);
}

Status DBImpl::Delete(const WriteOptions &options, std::string_view key) {
  auto lock = AcquireLock();
  return HandlePut(options, Hash(key), key, {});
}

Status DBImpl::Init() {
  Status status = metadata_manager_->Init();
  if (status != Status::kOk) {
    return status;
  }

  status = file_manager_->Init();
  if (status != Status::kOk) {
    return status;
  }

  status = Recovery();
  if (status != Status::kOk) {
    return status;
  }

  sync_worker_ = executor_->ScheduleEvery(
      options_.sync_interval, options_.sync_interval, [this] { Flush(); });

  return Status::kOk;
}

Status DBImpl::Compact() {
  std::vector<file_t> files;
  {
    auto _ = metadata_manager_->AcquireLock();
    auto all_files = metadata_manager_->GetFiles();
    files.assign(all_files.begin(), all_files.end());
  }

  std::sort(files.begin(), files.end());
  if (!files.empty()) {
    files.pop_back();
  }

  for (auto f : files) {
    Compact(f);
  }
  return Status::kOk;
}

DBImpl::DBImpl(const Options &options, const std::string &name)
    : options_(options) {
  read_cache_ = std::make_unique<ReadCache>(options_.read_cache_bytes);
  executor_ = options_.executor;
  metadata_manager_ = std::make_unique<MetadataManager>(name);
  file_manager_ = std::make_unique<FileManager>(
      metadata_manager_.get(), read_cache_.get(), options.max_open_files);
}

DBImpl::~DBImpl() {
  executor_->ScheduleCancel(sync_worker_);
  if (executor_.unique()) {
    executor_->Close();
  }
}

Status DBImpl::Recovery(file_t id) {
  ReadableFileGuard file;
  auto status = file_manager_->AcquireDataFile(id, &file);
  if (status != Status::kOk) {
    return status;
  }

  buffer_.Reset();
  auto iter = RecordIterator(file.get(), &buffer_);
  while (iter.Valid()) {
    PEDRODB_IGNORE_ERROR(Recovery(id, iter.Next()));
  }

  PEDRODB_TRACE("crash recover success: file[{}], record[{}]", id,
                indices_.size());
  PEDRODB_IGNORE_ERROR(file_manager_->ReleaseDataFile(id));
  return Status::kOk;
}

Status DBImpl::CompactBatch(const std::vector<Record> &records) {
  ArrayBuffer buffer;
  auto lock = AcquireLock();

  for (auto &r : records) {
    auto it = GetMetadataIterator(r.h, r.key);
    if (it == indices_.end()) {
      continue;
    }

    // steal record.
    if (it->loc != r.location) {
      continue;
    }

    // move to active file because this file will be removed.
    buffer.Reset();
    record::Header header{
        .crc32 = 0,
        .type = record::Type::kSet,
        .key_size = (uint8_t)r.key.size(),
        .value_size = (uint32_t)r.value.size(),
        .timestamp = r.timestamp,
    };

    header.Pack(&buffer);
    buffer.Append(r.key.data(), r.key.size());
    buffer.Append(r.value.data(), r.value.size());

    record::Location loc;
    auto _ = file_manager_->AcquireLock();
    auto status = file_manager_->WriteActiveFile(&buffer, &loc.id, &loc.offset);
    if (status != Status::kOk) {
      return status;
    }

    it->loc = loc;
    it->size = record::SizeOf(r.key.size(), r.value.size());
    read_cache_->UpdateCache(loc, r.value);
  }

  {
    auto _ = file_manager_->AcquireLock();
    file_manager_->SyncActiveFile();
  }
  return Status::kOk;
}

void DBImpl::Compact(file_t id) {
  ReadableFileGuard file;
  {
    auto _ = file_manager_->AcquireLock();
    auto stat = file_manager_->AcquireDataFile(id, &file);
    if (stat != Status::kOk) {
      return;
    }
  }

  // check compaction state.
  {
    auto lock = AcquireLock();
    auto it = compact_hints_.find(id);
    if (it == compact_hints_.end()) {
      return;
    }

    it->second.compacting = true;
  }

  PEDRODB_INFO("start compacting {}", id);

  std::vector<Record> batch;
  size_t batch_bytes = 0;

  ArrayBuffer buffer;
  for (auto iter = RecordIterator(file.get(), &buffer); iter.Valid();) {
    auto next = iter.Next();
    if (next.type != record::Type::kSet) {
      continue;
    }
    batch_bytes += record::SizeOf(next.key.size(), next.value.size());
    batch.emplace_back(Record{
        .h = Hash(next.key),
        .key = std::string{next.key},
        .value = std::string{next.value},
        .location = {.id = id, .offset = next.offset},
    });

    if (batch_bytes >= options_.compaction_batch_bytes) {
      CompactBatch(batch);
      batch.clear();
      batch_bytes = 0;
    }
  }

  if (batch_bytes > 0) {
    CompactBatch(batch);
    batch.clear();
  }

  // erase compaction_state.
  {
    auto lock = AcquireLock();
    compact_hints_.erase(id);
  }

  // remove file.
  {
    auto _ = file_manager_->AcquireLock();
    PEDRODB_IGNORE_ERROR(file_manager_->ReleaseDataFile(id));
    PEDRODB_IGNORE_ERROR(file_manager_->RemoveDataFile(id));
  }

  PEDRODB_TRACE("end compacting: {}", id);
}

Status DBImpl::HandlePut(const WriteOptions &options, uint32_t h,
                         std::string_view key, std::string_view value) {

  uint32_t record_size = record::SizeOf(key.size(), value.size());
  if (record_size > kMaxFileBytes) {
    PEDRODB_ERROR("key or value is too big");
    return Status::kNotSupported;
  }

  // check valid deletion.
  auto it = GetMetadataIterator(h, key);
  if (it == indices_.end() && value.empty()) {
    return Status::kNotFound;
  }

  uint32_t timestamp = 0;
  record::Header header{.crc32 = 0,
                        .type = value.empty() ? record::Type::kDelete
                                              : record::Type::kSet,
                        .key_size = (uint8_t)key.size(),
                        .value_size = (uint32_t)value.size(),
                        .timestamp = timestamp};

  buffer_.Reset();
  header.Pack(&buffer_);
  buffer_.Append(key.data(), key.size());
  buffer_.Append(value.data(), value.size());

  record::Location loc{};
  auto status = file_manager_->WriteActiveFile(&buffer_, &loc.id, &loc.offset);
  if (status != Status::kOk) {
    return status;
  }

  // replace or delete.
  if (it != indices_.end()) {

    // update compact hints.
    auto &state = compact_hints_[it->loc.id];
    if (!state.compacting) {
      state.unused += it->size;
      if (state.unused > options_.compaction_threshold_bytes) {
        state.compacting = true;
        executor_->Schedule([=] { Compact(it->loc.id); });
        PEDRODB_INFO("schedule compact {} {}", it->loc.id, state.unused);
      }
    }

    if (value.empty()) {
      // delete.
      read_cache_->Remove(it->loc);
      indices_.erase(it);
    } else {
      // replace.
      it->loc = loc;
      it->size = record_size;
      read_cache_->UpdateCache(loc, value);
    }
    return Status::kOk;
  }

  // insert.
  indices_.emplace(RecordDir{
      .h = h, .key = std::string{key}, .loc = loc, .size = record_size});
  read_cache_->UpdateCache(loc, value);
  return Status::kOk;
}

Status DBImpl::Flush() {
  auto lock = file_manager_->AcquireLock();
  file_manager_->SyncActiveFile();
  return Status::kOk;
}

Status DBImpl::FetchRecord(ReadableFile *file, const RecordDir &metadata,
                           std::string *value) {
  record::Location l = metadata.loc;

  buffer_.Reset();
  buffer_.EnsureWriteable(metadata.size);
  ssize_t r = file->Read(l.offset, buffer_.WriteIndex(), metadata.size);
  if (r != metadata.size) {
    PEDRODB_ERROR("failed to read from file {}, returns {}: {}", l.id, r,
                  file->GetError());
    return Status::kIOError;
  }
  buffer_.Append(metadata.size);

  record::Header header{};
  if (!header.UnPack(&buffer_)) {
    return Status::kCorruption;
  }

  if (buffer_.ReadableBytes() != header.key_size + header.value_size) {
    return Status::kCorruption;
  }

  buffer_.Retrieve(header.key_size);
  value->resize(header.value_size);
  buffer_.Retrieve(value->data(), value->size());
  read_cache_->UpdateCache(l, *value);
  return Status::kOk;
}

Status DBImpl::Recovery() {
  auto all_files = metadata_manager_->GetFiles();
  std::vector<file_t> files(all_files.begin(), all_files.end());
  std::sort(files.begin(), files.end());

  for (auto file : files) {
    PEDRODB_TRACE("crash recover: file {}", file);
    auto status = Recovery(file);
    if (status != Status::kOk) {
      return status;
    }
  }
  file_manager_->ReleaseDataFile(files.back());
  return Status::kOk;
}

auto DBImpl::GetMetadataIterator(uint32_t h, std::string_view key)
    -> decltype(indices_.begin()) {
  auto [s, t] = indices_.equal_range(RecordDir{.h = h});
  auto it = std::find_if(s, t, [=](auto &k) { return k.key == key; });
  return it == t ? indices_.end() : it;
}

Status DBImpl::HandleGet(const ReadOptions &options, uint32_t h,
                         std::string_view key, std::string *value) {
  auto it = GetMetadataIterator(h, key);
  if (it == indices_.end()) {
    return Status::kNotFound;
  }

  if (read_cache_->Read(it->loc, value)) {
    return Status::kOk;
  }

  ReadableFileGuard file;
  {
    auto lock = file_manager_->AcquireLock();
    auto stat = file_manager_->AcquireDataFile(it->loc.id, &file);
    if (stat != Status::kOk) {
      PEDRODB_ERROR("cannot get file {}", it->loc.id);
      return stat;
    }
  }

  return FetchRecord(file.get(), *it, value);
}

Status DBImpl::Recovery(file_t id, RecordEntry entry) {
  record::Location loc{.id = id, .offset = entry.offset};
  uint32_t size = record::SizeOf(entry.key.size(), entry.value.size());

  uint32_t h = Hash(entry.key);
  auto it = GetMetadataIterator(h, entry.key);

  if (entry.type == record::Type::kSet) {
    if (it == indices_.end()) {
      indices_.emplace(RecordDir{
          .h = h, .key = std::string{entry.key}, .loc = loc, .size = size});
      read_cache_->UpdateCache(loc, entry.value);
      return Status::kOk;
    }

    // indices has the newer version data.
    if (it->loc > loc) {
      auto &state = compact_hints_[id];
      state.unused += record::SizeOf(entry.key.size(), entry.value.size());
      return Status::kOk;
    }

    // never happen.
    if (it->loc == loc) {
      PEDRODB_FATAL("meta.loc == loc should never happened");
    }

    // indices has the elder version data.
    compact_hints_[it->loc.id].unused += it->size;
    read_cache_->Remove(it->loc);
    read_cache_->UpdateCache(loc, entry.value);

    // update indices.
    it->loc = loc;
    it->size = size;
  }

  // a tombstone of deletion.
  if (entry.type == record::Type::kDelete) {
    auto &state = compact_hints_[id];
    state.unused += record::SizeOf(entry.key.size(), entry.value.size());
    if (it == indices_.end()) {
      return Status::kOk;
    }

    // Recover(file_t) should be called monotonously,
    // therefore entry.loc is always monotonously increased.
    // should not delete the latest version data.
    if (it->loc > loc) {
      return Status::kOk;
    }

    compact_hints_[it->loc.id].unused += it->size;
    read_cache_->Remove(it->loc);
    indices_.erase(it);
  }

  return Status::kOk;
}
} // namespace pedrodb