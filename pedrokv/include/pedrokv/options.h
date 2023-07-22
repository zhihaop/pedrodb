#ifndef PEDROKV_OPTIONS_H
#define PEDROKV_OPTIONS_H
#include <pedrodb/options.h>
#include <pedronet/eventloopgroup.h>
#include "pedrokv/defines.h"

namespace pedrokv {

struct ServerOptions {
  std::shared_ptr<EventLoopGroup> boss_group = EventLoopGroup::Create(1);
  std::shared_ptr<EventLoopGroup> worker_group = EventLoopGroup::Create();
  
  size_t db_shards = 8;
  pedrodb::Options db_options;
  std::string db_path;
};

struct ClientOptions {
  std::shared_ptr<pedronet::EventLoopGroup> worker_group;
  size_t max_inflight{4096};
};
}  // namespace pedrokv
#endif  // PEDROKV_OPTIONS_H
