#ifndef PEDROKV_OPTIONS_H
#define PEDROKV_OPTIONS_H
#include "pedrokv/defines.h"
#include <pedronet/eventloopgroup.h>

namespace pedrokv {

struct Options {
  std::shared_ptr<pedronet::EventLoopGroup> boss_group;
  std::shared_ptr<pedronet::EventLoopGroup> worker_group;
};
} // namespace pedrokv
#endif // PEDROKV_OPTIONS_H
