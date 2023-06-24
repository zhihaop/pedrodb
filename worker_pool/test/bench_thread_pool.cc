#include "blocking_queue.h"
#include "thread_pool.h"
#include "matrix.h"
#include "timer.h"

#include "proto/matrix.pb.h"

#include <fstream>

using namespace std;

void test_light_weight_direct() {
  std::atomic_int64_t val;
  std::function<void(void)> task = [&val] {
    val.fetch_add(1, std::memory_order_relaxed);
  };

  auto cost = duration([&] {
    for (int i = 0; i < 1000000; ++i) {
      task();
    }
  });
  cout << cost << "ms" << endl;
}

void test_light_weight(size_t threads) {
  std::atomic_int64_t val;
  pedro::FixedThreadPoolExecutor executor(threads, 1024);

  auto cost = duration([&] {
    for (int i = 0; i < 1000000; ++i) {
      auto task = [&val] { val.fetch_add(1, std::memory_order_relaxed); };
      if (!executor.TrySubmit(task)) {
        // yield.
        task();
      }
    }
    executor.Shutdown();
  });
  cout << cost << "ms\n";
}

void make_test_file(int n, const std::string &name) {
  pedro::pb::Vector vector;
  for (int i = 0; i < n; ++i) {
    vector.add_data(i);
  }
  std::fstream fs(name, std::ios::binary | std::ios::out);
  vector.SerializeToOstream(&fs);
}

void test_write_io_job(size_t threads) {
  pedro::FixedThreadPoolExecutor executor(threads, 1);

  int n = 512;
  auto cost = duration([&] {
    for (int i = 0; i < n; ++i) {
      auto task = [i] {
        std::string name = "test" + std::to_string(i) + ".proto";
        make_test_file(1000000, name);
      };
      while (!executor.Submit(std::move(task))) {
        // yield policy.
        this_thread::yield();
      }
    }
    executor.Shutdown();
    executor.Join();
  });
  cout << cost << "ms" << endl;
}

void test_read_io_job(size_t threads) {
  pedro::FixedThreadPoolExecutor executor(threads, 1);

  int n = 512;
  std::atomic_int64_t val = 0;
  auto cost = duration([&] {
    for (int i = 0; i < n; ++i) {
      auto task = [i, &val] {
        std::string name = "test" + std::to_string(i) + ".proto";
        std::fstream is(name, std::ios::binary | std::ios::in);
        pedro::pb::Vector vector;
        vector.ParseFromIstream(&is);
        val += vector.data()[0];
      };
      while (!executor.Submit(task)) {
        // yield policy.
        this_thread::yield();
      }
    }
    executor.Shutdown();
    executor.Join();
  });
  cout << cost << "ms" << endl;
}

void test_calc_job(size_t threads) {
  std::atomic_int64_t val;
  pedro::FixedThreadPoolExecutor executor(threads, 1024);

  int n = 10000;
  int m = 100;

  Matrix mat(m, m);
  auto task = [&val, x = mat, y = mat] {
    Matrix z(x.rows(), y.cols());
    for (int i = 0; i < z.rows(); ++i) {
      for (int k = 0; k < x.cols(); ++k) {
        for (int j = 0; j < z.cols(); ++j) {
          z[i][j] += x[i][k] * y[k][j];
        }
      }
    }
    val += z[0][0];
  };

  auto cost = duration([&] {
    for (int i = 0; i < n; ++i) {
      while (!executor.Submit(std::move(task))) {
        // yield policy.
        this_thread::yield();
      }
    }
    executor.Shutdown();
    executor.Join();
  });
  cout << cost << "ms" << endl;
}

template <class Fn> void test(Fn &fn) {
  fn(1);
  fn(2);
  fn(4);
  fn(8);
}

int main() {
  test_light_weight_direct();
  cout << "[test light weight]\n";
  test(test_light_weight);
  // cout << "[test calc job]\n";
  // test(test_calc_job);
  // cout << "[test write io job]\n";
  // test(test_write_io_job);
  // cout << "[test read io job]\n";
  // test(test_read_io_job);
  return 0;
}