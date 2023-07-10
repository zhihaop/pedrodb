#ifndef PEDROKV_OPTIONS_H
#define PEDROKV_OPTIONS_H
#include "pedrokv/defines.h"
#include <pedrodb/options.h>
#include <pedronet/eventloopgroup.h>

namespace pedrokv {

struct ServerOptions {
  std::shared_ptr<pedronet::EventLoopGroup> boss_group;
  std::shared_ptr<pedronet::EventLoopGroup> worker_group;

  pedrodb::Options db_options;
  std::string db_path;
};

struct ClientOptions {
  std::shared_ptr<pedronet::EventLoopGroup> worker_group;
};
} // namespace pedrokv
#endif // PEDROKV_OPTIONS_H
