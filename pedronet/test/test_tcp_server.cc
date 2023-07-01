
#include <pedronet/eventloopgroup.h>
#include <pedronet/selector/epoller.h>
#include <pedronet/tcp_server.h>

using namespace std::chrono_literals;
using pedronet::BufferView;
using pedronet::EpollSelector;
using pedronet::EventLoopGroup;
using pedronet::InetAddress;
using pedronet::TcpConnectionPtr;
using pedronet::TcpServer;

int main() {
  spdlog::set_level(spdlog::level::info);

  TcpServer server;

  size_t n_workers = std::thread::hardware_concurrency();
  auto boss_group = EventLoopGroup::Create<EpollSelector>(1);
  auto worker_group = EventLoopGroup::Create<EpollSelector>(n_workers);
  
  server.SetGroup(boss_group, worker_group);
  server.OnConnect([](auto &&conn) { PEDRONET_INFO("connect: {}", *conn); });
  server.OnClose([](auto &&conn) { PEDRONET_INFO("disconnect: {}", *conn); });
  server.OnMessage([=](auto &&conn, auto &buf, auto) { conn->Send(&buf); });
  server.Bind(InetAddress::Create("0.0.0.0", 1082));
  server.Start();

  boss_group->Join();
  worker_group->Join();

  return 0;
}