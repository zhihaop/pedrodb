#include <pedrolib/executor/thread_pool_executor.h>
#include <chrono>
#include <iostream>

using namespace std::chrono_literals;
using pedrolib::ThreadPoolExecutor;

int main() {
  ThreadPoolExecutor executor(3);

  std::atomic_int counter{};
  uint64_t id;
  id = executor.ScheduleEvery(0s, 1s, [&] {
    std::cout << "hello world" << std::endl;
    if (counter++ == 3) {
      executor.ScheduleCancel(id);
      executor.ScheduleAfter(2s, [&] { executor.Close(); });
    }
  });

  executor.Join();
  return 0;
}