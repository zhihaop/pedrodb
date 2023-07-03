#ifndef PEDRODB_METADATA_H
#define PEDRODB_METADATA_H

#include "pedrodb/file/random_access_file.h"
#include <mutex>

namespace pedrodb {

using pedrolib::htobe;

struct Metadata {
  uint64_t version;
  uint32_t active_id;
  uint32_t name_length;
  char name[];
};

class MetadataManager {
  std::mutex mu_;
  uint64_t version_{};
  uint32_t active_id_{};

  std::string name_;
  RandomAccessFile file_;

public:
  MetadataManager() = default;
  ~MetadataManager() = default;

  const std::string &GetName() const noexcept { return name_; }

  Metadata *GetMetadata() { return reinterpret_cast<Metadata *>(file_.Data()); }

  Status Open(const std::string &filename) {
    size_t index = filename.find(".db");
    if (index == -1) {
      PEDRODB_ERROR("db filename[{}] error", filename);
      return Status::kInvalidArgument;
    }

    name_ = filename.substr(0, index);

    int fd = open(filename.c_str(), O_RDWR | O_CREAT, 0777);
    // TODO logging
    if (fd <= 0) {
      auto err = Error{errno};
      if (err.GetCode() != ENOENT) {
        PEDRODB_ERROR("cannot open filename[{}]: {}", filename, err);
        return Status::kIOError;
      }
    }

    uint64_t filesize = 0;
    auto err = pedrolib::GetFileSize(filename.c_str(), &filesize);
    if (!err.Empty()) {
      PEDRODB_ERROR("cannot get file size");
      return Status::kIOError;
    }

    if (filesize != 0) {
      file_ = RandomAccessFile(File{fd}, filesize);

      auto metadata = GetMetadata();
      version_ = htobe(metadata->version) + 1;
      active_id_ = htobe(metadata->active_id);
      return Status::kOk;
    }

    PEDRODB_INFO("db[{}] not exist, create one", name_);

    File file{fd};
    size_t metadata_bytes = sizeof(Metadata) + name_.size();
    std::vector<char> buf(metadata_bytes);
    file.Write(buf.data(), buf.size());
    file_ = RandomAccessFile(std::move(file), sizeof(uint64_t));

    auto metadata = GetMetadata();
    metadata->version = htobe(0);
    metadata->active_id = htobe(0);
    metadata->name_length = htobe(static_cast<uint32_t>(name_.size()));
    memcpy(metadata->name, name_.c_str(), name_.size());

    return Status::kOk;
  }

  uint32_t AddActiveID() {
    std::unique_lock<std::mutex> lock(mu_);
    return ++active_id_;
  }

  uint64_t AddVersion() {
    std::unique_lock<std::mutex> lock(mu_);
    return ++version_;
  }

  uint64_t GetVersion() const noexcept { return version_; }
  uint32_t GetActiveID() const noexcept { return active_id_; }

  Error Flush() {
    std::unique_lock<std::mutex> lock(mu_);

    auto metadata = GetMetadata();

    bool should_flush = false;
    if (version_ > htobe(metadata->version)) {
      should_flush = true;
      metadata->version = htobe(version_ + kBatchVersions);
    }

    if (active_id_ > metadata->active_id) {
      should_flush = true;
      metadata->active_id = htobe(active_id_);
    }

    if (should_flush) {
      return file_.Flush();
    }
    return Error::Success();
  }

  std::string GetFile(uint32_t id) {
    return fmt::format("{}_{}.data", name_, id);
  }
};

} // namespace pedrodb

#endif // PEDRODB_METADATA_H
