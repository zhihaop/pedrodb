#include "worker.h"
#include "proto/rpc.pb.h"

#include <future>
#include <memory>
#include <wait.h>

namespace pedro {

Worker::~Worker() { Close(); }

std::shared_ptr<Worker> Worker::CreateWorker() {
  std::unique_ptr<Ipc> ipc = Ipc::Create();
  pid_t pid = fork();
  if (pid == -1) {
    return {};
  } else if (pid == 0) {
    Controller::Listen(*ipc);
    std::exit(0);
  } else {
    return std::make_shared<Worker>(pid, std::move(ipc));
  }
}

bool Worker::Joinable() const noexcept { return process_id_ != 0; }

void Worker::Join() {
  if (!Joinable())
    return;

  for (;;) {
    int status;
    waitpid(process_id_, &status, 0);
    if (WIFEXITED(status)) {
      process_id_ = 0;
      break;
    }
  }

  ipc_->Close();
}

void Worker::Sync() {
  ipc_->Call(pedro::Controller::Sync, IpcType::REQUEST_RESPONSE, {}).get();
}

void Worker::Close() {
  if (Joinable()) {
    ipc_->Call(pedro::Controller::Shutdown, IpcType::REQUEST_RESPONSE, {}).get();
    Join();
  }
}
} // namespace pedro