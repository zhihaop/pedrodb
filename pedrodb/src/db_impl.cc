#include "pedrodb/db_impl.h"

namespace pedrodb {

Status DBImpl::Get(const ReadOptions &options, std::string_view key,
                   std::string *value) {
  auto lock = AcquireLock();
  return HandleGet(options, Hash(key), key, value);
}

void DBImpl::UpdateCompactionHint(const KeyValueMetadata &metadata) {
  auto &state = compaction_state_[metadata.location.id];
  if (state.compacting) {
    return;
  }

  state.unused += metadata.length;
  if (state.unused > options_.compaction_threshold_bytes) {
    state.compacting = true;
    executor_->Schedule([=] { Compact(metadata.location.id); });
    PEDRODB_INFO("schedule compact {} {}", metadata.location.id, state.unused);
  }
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

  status = version_set_->Init();
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
  std::vector<uint32_t> files;
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
  version_set_ = std::make_unique<VersionSet>(metadata_manager_.get());
}

DBImpl::~DBImpl() {
  executor_->ScheduleCancel(sync_worker_);
  if (executor_.unique()) {
    executor_->Close();
  }
}

Status DBImpl::Recovery(uint32_t id) {
  ReadableFileGuard file;
  auto stat = file_manager_->AcquireDataFile(id, &file);
  if (stat != Status::kOk) {
    return stat;
  }

  buffer_.Reset();
  auto iter = RecordIterator(file.get(), &buffer_);
  while (iter.Valid()) {
    auto next = iter.Next();
    ValueLocation location{.id = id, .offset = next.offset};
    size_t h = Hash(next.key);

    auto it = GetMetadataIterator(h, next.key);
    if (next.type == RecordHeader::Type::kSet) {
      KeyValueMetadata metadata{.key = std::string{next.key},
                                .location = location,
                                .length = next.length,
                                .timestamp = next.timestamp};

      if (it == indices_.end()) {
        indices_.emplace(h, std::move(metadata));
        read_cache_->UpdateCache(location, next.value);
        continue;
      }

      auto &meta = it->second;
      if (meta.timestamp >= next.timestamp) {
        compaction_state_[location.id].unused += next.length;
        continue;
      }

      compaction_state_[meta.location.id].unused += meta.length;
      read_cache_->Remove(meta.location);
      read_cache_->UpdateCache(location, next.value);
      meta = std::move(metadata);
    }

    if (next.type == RecordHeader::Type::kDelete) {
      compaction_state_[location.id].unused += next.length;
      if (it == indices_.end()) {
        continue;
      }

      auto &meta = it->second;
      if (meta.timestamp >= next.timestamp) {
        continue;
      }

      compaction_state_[meta.location.id].unused += meta.length;
      read_cache_->Remove(meta.location);
      indices_.erase(it);
    }
  }

  PEDRODB_TRACE("crash recover success: file[{}], record[{}]", id,
                indices_.size());
  PEDRODB_IGNORE_ERROR(file_manager_->ReleaseDataFile(id));
  return Status::kOk;
}

Status DBImpl::CompactBatch(const std::vector<KeyValueRecord> &records) {
  ArrayBuffer buffer;
  auto lock = AcquireLock();

  for (auto &r : records) {
    auto it = GetMetadataIterator(r.h, r.key);
    if (it == indices_.end()) {
      continue;
    }

    auto &meta = it->second;
    if (meta.timestamp > r.timestamp) {
      continue;
    }

    // there must be another file have this record.
    if (meta.timestamp == r.timestamp && meta.location.id != r.id) {
      continue;
    }

    // move to active file because this file will be removed.
    buffer.Reset();
    RecordHeader header{
        .crc32 = 0,
        .type = RecordHeader::Type::kSet,
        .key_size = (uint16_t)r.key.size(),
        .value_size = (uint32_t)r.value.size(),
        .timestamp = r.timestamp,
    };

    header.Pack(&buffer);
    buffer.Append(r.key.data(), r.key.size());
    buffer.Append(r.value.data(), r.value.size());

    ValueLocation loc;
    auto _ = file_manager_->AcquireLock();
    auto status = file_manager_->WriteActiveFile(&buffer, &loc.id, &loc.offset);
    if (status != Status::kOk) {
      return status;
    }
    meta.location = loc;
    read_cache_->UpdateCache(loc, r.value);
  }

  {
    auto _ = file_manager_->AcquireLock();
    file_manager_->SyncActiveFile();
  }
  return Status::kOk;
}

void DBImpl::Compact(uint32_t id) {
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
    auto it = compaction_state_.find(id);
    if (it == compaction_state_.end()) {
      return;
    }

    it->second.compacting = true;
  }

  PEDRODB_INFO("start compacting {}", id);

  std::vector<KeyValueRecord> batch;
  size_t batch_bytes = 0;

  ArrayBuffer buffer;
  for (auto iter = RecordIterator(file.get(), &buffer); iter.Valid();) {
    auto next = iter.Next();
    if (next.type != RecordHeader::Type::kSet) {
      continue;
    }
    batch_bytes += next.length;
    batch.emplace_back(KeyValueRecord{
        .h = Hash(next.key),
        .key = std::string{next.key},
        .value = std::string{next.value},
        .id = id,
        .offset = next.offset,
        .timestamp = next.timestamp,
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
    compaction_state_.erase(id);
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

  uint32_t record_length = RecordHeader::SizeOf() + key.size() + value.size();
  if (record_length > kMaxFileBytes) {
    PEDRODB_ERROR("key or value is too big");
    return Status::kNotSupported;
  }

  // check valid deletion.
  auto it = GetMetadataIterator(h, key);
  if (it == indices_.end() && value.empty()) {
    return Status::kNotFound;
  }

  uint32_t timestamp = version_set_->IncreaseVersion();

  RecordHeader header{.crc32 = 0,
                      .type = value.empty() ? RecordHeader::Type::kDelete
                                            : RecordHeader::Type::kSet,
                      .key_size = (uint16_t)key.size(),
                      .value_size = (uint16_t)value.size(),
                      .timestamp = version_set_->IncreaseVersion()};

  buffer_.Reset();
  header.Pack(&buffer_);
  buffer_.Append(key.data(), key.size());
  buffer_.Append(value.data(), value.size());

  ValueLocation loc{};
  auto status = file_manager_->WriteActiveFile(&buffer_, &loc.id, &loc.offset);
  if (status != Status::kOk) {
    return status;
  }

  // replace or delete.
  if (it != indices_.end()) {
    auto &metadata = it->second;
    UpdateCompactionHint(metadata);
    read_cache_->Remove(metadata.location);

    if (value.empty()) {
      indices_.erase(it);
    } else {
      metadata = metadata;
      read_cache_->UpdateCache(metadata.location, value);
    }
    return Status::kOk;
  }

  // insert.
  indices_.emplace(h, KeyValueMetadata{.key = std::string{key},
                                       .location = loc,
                                       .length = record_length,
                                       .timestamp = timestamp});
  read_cache_->UpdateCache(loc, value);
  return Status::kOk;
}

Status DBImpl::Flush() {
  auto lock = file_manager_->AcquireLock();
  file_manager_->SyncActiveFile();
  return Status::kOk;
}

Status DBImpl::FetchRecord(ReadableFile *file, const KeyValueMetadata &metadata,
                           std::string *value) {
  ValueLocation l = metadata.location;

  buffer_.Reset();
  buffer_.EnsureWriteable(metadata.length);
  ssize_t r = file->Read(l.offset, buffer_.WriteIndex(), metadata.length);
  if (r != metadata.length) {
    PEDRODB_ERROR("failed to read from file {}, returns {}: {}", l.id, r,
                  file->GetError());
    return Status::kIOError;
  }
  buffer_.Append(metadata.length);

  RecordHeader header{};
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
  std::vector<uint32_t> files(all_files.begin(), all_files.end());
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
  auto [s, t] = indices_.equal_range(h);
  for (auto it = s; it != t; ++it) {
    if (it->second.key == key) {
      return it;
    }
  }
  return indices_.end();
}
Status DBImpl::HandleGet(const ReadOptions &options, uint32_t h,
                         std::string_view key, std::string *value) {
  auto it = GetMetadataIterator(h, key);
  if (it == indices_.end()) {
    return Status::kNotFound;
  }

  auto &metadata = it->second;
  if (read_cache_->Read(metadata.location, value)) {
    return Status::kOk;
  }

  ReadableFileGuard file;
  {
    auto lock = file_manager_->AcquireLock();
    auto stat = file_manager_->AcquireDataFile(metadata.location.id, &file);
    if (stat != Status::kOk) {
      PEDRODB_ERROR("cannot get file {}", metadata.location.id);
      return stat;
    }
  }

  return FetchRecord(file.get(), metadata, value);
}
} // namespace pedrodb