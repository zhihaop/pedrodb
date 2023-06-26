#include "pedronet/epoll_event_loop.h"
#include "pedronet/tcp_client.h"
#include "pedronet/tcp_server.h"
#include <future>
#include <spdlog/common.h>
#include "pedronet/core/debug.h"
#include <thread>

using namespace std::chrono_literals;

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

  auto task = std::async(std::launch::async, [worker_group] {
    while (true) {
      pedronet::TcpClient client(pedronet::InetAddress::Create("127.0.0.1", 1082));
      client.SetGroup(worker_group);
      client.Start();
      std::this_thread::sleep_for(1s);
      client.Close();
    }
  });

  boss_group->Join();
  worker_group->Join();

  return 0;
}