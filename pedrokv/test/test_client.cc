#include <pedrokv/client.h>
#include <pedrokv/logger/logger.h>
#include <pedronet/logger/logger.h>

using pedrokv::ClientOptions;
using pedrolib::Logger;
using pedronet::EventLoopGroup;
using pedronet::InetAddress;

int main() {
  pedrokv::logger::SetLevel(Logger::Level::kInfo);
  pedronet::logger::SetLevel(Logger::Level::kTrace);

  auto address = InetAddress::Create("127.0.0.1", 1082);
  auto options = ClientOptions{};

  auto logger = Logger("test");
  logger.SetLevel(Logger::Level::kTrace);

  options.worker_group = EventLoopGroup::Create(24);
  pedrokv::Client client(address, options);
  logger.Info("before start");
  client.Start();
  logger.Info("client start");
  client.Put("hello", "world").get();
  logger.Info("Set success");

  auto f = client.Get("hello");
  logger.Info("Get {}", f.get().data);
  return 0;
}