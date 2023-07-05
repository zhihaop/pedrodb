#ifndef PEDRODB_VERSIONSET_H
#define PEDRODB_VERSIONSET_H
#include "pedrodb/metadata_manager.h"

namespace pedrodb {
class VersionSet {
  MetadataManager *metadata_manager_;

  std::atomic_uint32_t max_version_{};
  std::atomic_uint32_t version_{};

public:
  explicit VersionSet(MetadataManager *metadata_manager)
      : metadata_manager_(metadata_manager) {}

  Status Init() {
    version_ = metadata_manager_->GetCurrentVersion();
    max_version_ = metadata_manager_->GetCurrentVersion();
    return Status::kOk;
  }

  uint32_t IncreaseVersion() {
    uint32_t v = version_.fetch_add(1);
    while (v < max_version_.load()) {
      auto _ = metadata_manager_->AcquireLock();
      if (v >= max_version_.load()) {
        return v;
      }
      max_version_ = metadata_manager_->AcquireVersion();
    }
    return v;
  }
};
} // namespace pedrodb
#endif // PEDRODB_VERSIONSET_H
