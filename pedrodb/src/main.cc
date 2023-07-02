#include <chrono>
#include <iostream>
#include <pedrodb/logger/logger.h>
#include <pedronet/eventloopgroup.h>
#include <pedrodb/db.h>
using namespace std::chrono_literals;
using pedronet::EventLoopGroup;
namespace logger = pedrodb::logger;

int main() {
  logger::SetLevel(logger::Level::kInfo);
  auto g = EventLoopGroup::Create(1);
  g->ScheduleEvery(0s, 1s, [=] { std::cout << "hello world" << std::endl; });
  
  pedrodb::DB::Open({}, "hello", nullptr);
  

  return 0;
}