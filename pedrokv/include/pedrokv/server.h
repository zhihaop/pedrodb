#ifndef PEDROKV_KV_SERVER_H
#define PEDROKV_KV_SERVER_H

#include "pedrokv/defines.h"
#include "pedrokv/options.h"

#include <memory>
#include <pedronet/tcp_server.h>
#include <utility>
namespace pedrokv {

class Server : nonmovable,
               noncopyable,
               public std::enable_shared_from_this<Server> {
  pedronet::TcpServer server_;
  pedronet::InetAddress address_;
  Options options_;

public:
  Server(pedronet::InetAddress address, Options options)
      : address_(std::move(address)), options_(std::move(options)) {
    server_.SetGroup(options_.boss_group, options_.worker_group);
  }

  void Bind() { server_.Bind(address_); }
  void Start() { server_.Start(); }
};

} // namespace pedrokv

#endif // PEDROKV_KV_SERVER_H
