#include <pedrodb/logger/logger.h>
#include <pedrokv/options.h>
#include <pedrokv/server.h>
#include <pedronet/logger/logger.h>
using pedrokv::Server;
using pedrokv::ServerOptions;
using pedrolib::Logger;
using pedronet::EventLoopGroup;
using pedronet::InetAddress;

int main() {
  pedrokv::logger::SetLevel(Logger::Level::kInfo);
  pedrodb::logger::SetLevel(Logger::Level::kInfo);
  pedronet::logger::SetLevel(Logger::Level::kInfo);

  auto address = InetAddress::Create("0.0.0.0", 1082);
  ServerOptions options;
  options.db_path = "/tmp/test.db";

  auto server = pedrokv::Server(address, options);
  server.Bind();
  server.Start();
  
  EventLoopGroup::Joins(options.worker_group, options.boss_group);

  return 0;
}