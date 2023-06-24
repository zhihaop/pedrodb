#include "blocking_queue.h"
#include "thread_pool.h"

#include <future>
#include <deque>
#include <iostream>
using namespace std;

int main() {
  std::vector<std::future<void>> rthreads;
  std::vector<std::future<void>> wthreads;
  pedro::BlockingQueue<pedro::Runnable> q(4096);
  std::atomic_bool shutdown;
  for (int i = 0; i < 8; ++i) {
    rthreads.emplace_back(std::async(std::launch::async, [&] {
      while (!shutdown) {
        q.Take();
      }
    }));
  }

  std::deque<std::atomic_size_t> counter;
  for (int i = 0; i < 1; ++i) {
    std::atomic_size_t& c = counter.emplace_back(0);
    wthreads.emplace_back(std::async(std::launch::async, [&] {
      while (!shutdown) {
        c.fetch_add(1);
        q.Offer([] {
            std::cout << "hello" << std::endl;
        });
      }
    }));
  }

  auto ticker = std::async(std::launch::async, [&] {
    while (!shutdown) {
      std::this_thread::sleep_for(1s);
      size_t val = 0;
      for (auto& x: counter) {
        val += x.exchange(0);
      }
      cout << "ops=" << val << endl;
    }
  });

  cin.get();

  shutdown = true;
  q.Close();

  return 0;
}