#include "pedronet/eventloop_impl.h"
#include "pedronet/tcp_client.h"

using namespace pedronet;

int main() {
  spdlog::set_level(spdlog::level::info);

  TcpClient client(InetAddress::Create("127.0.0.1", 1082));

  size_t n_workers = std::thread::hardware_concurrency();
  auto worker_group = EventLoopGroup::Create<EpollEventLoop>(n_workers);
  worker_group->Start();

  client.SetGroup(worker_group);

  client.OnConnect([](auto conn) { conn->Send("hello server"); });

  client.OnMessage([](auto conn, auto buffer, auto now) {
    std::string buf(buffer->ReadableBytes(), 0);
    buffer->Retrieve(buf.data(), buf.size());
    spdlog::info("receive from server: {}", buf);
    conn->Send(buf);
  });

  client.Start();
  worker_group->Join();

  return 0;
}