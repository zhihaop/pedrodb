#ifndef WORKER_POOL_CONTROLLER_H
#define WORKER_POOL_CONTROLLER_H

#include "ipc.h"
#include "proto/rpc.pb.h"
#include <memory>

namespace pedro {
using MachineArch = pb::MachineArch;
class Ipc;
} // namespace pedro

namespace pedro::Controller {

std::unique_ptr<Message> Shutdown(Parameter);
std::unique_ptr<Message> Sync(Parameter);

MachineArch Arch();
void Listen(Ipc &rpc);
} // namespace pedro::Controller

#endif // WORKER_POOL_CONTROLLER_H
