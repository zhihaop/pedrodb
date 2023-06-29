#include "pedronet/eventloop_impl.h"
#include "pedronet/tcp_server.h"

using namespace std::chrono_literals;
using namespace pedronet;

int main() {
  spdlog::set_level(spdlog::level::info);

  TcpServer server;

  size_t n_workers = std::thread::hardware_concurrency();
  auto boss_group = EventLoopGroup::Create<EpollEventLoop>(1);
  auto worker_group = EventLoopGroup::Create<EpollEventLoop>(n_workers);

  boss_group->Start();
  worker_group->Start();

  server.SetGroup(boss_group, worker_group);
  server.OnConnect([](auto conn) {
    spdlog::info("client connect: {}", *conn);
    conn->Send("hello client");
  });

  server.OnClose([](const TcpConnectionPtr &conn) {
    spdlog::info("client disconnect: {}", *conn);
  });

  server.OnMessage([](auto conn, auto buffer, auto now) {
    std::string buf(buffer->ReadableBytes(), 0);
    buffer->Retrieve(buf.data(), buf.size());

    if (buf.find("exit") != std::string::npos) {
      spdlog::info("Server receive exit");
      conn->Close();
      return;
    }
    conn->Send(buf);
  });

  server.Bind(InetAddress::Create("0.0.0.0", 1082));
  server.Start();

  boss_group->Join();
  worker_group->Join();

  return 0;
}