#include "pedronet/epoll_event_loop.h"
#include "pedronet/tcp_server.h"
#include <spdlog/common.h>
#include <spdlog/spdlog.h>

int main() {
  pedronet::TcpServer tcp_server;

  auto boss_group =
      pedronet::EventLoopGroup::Create<pedronet::EpollEventLoop>(1);
  auto worker_group =
      pedronet::EventLoopGroup::Create<pedronet::EpollEventLoop>(1);
  boss_group->Start();
  worker_group->Start();

  spdlog::set_level(spdlog::level::trace);

  tcp_server.SetGroup(boss_group, worker_group);
  tcp_server.Bind(pedronet::InetAddress::Create("0.0.0.0", 1082));
  tcp_server.Start();

  boss_group->Join();
  worker_group->Join();

  return 0;
}