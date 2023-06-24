#include "process_pool.h"
#include "matrix.h"
#include "timer.h"
#include "proto/counter.pb.h"
#include "proto/matrix.pb.h"

#include <fstream>
using namespace std;

int64_t counter;
std::unique_ptr<pedro::Message> add(pedro::Parameter args) {
  pedro::pb::CounterRequest request;
  args->UnpackTo(&request);
  counter += request.delta();
  return {};
}

std::unique_ptr<pedro::Message> count(pedro::Parameter args) {
  auto response = std::make_unique<pedro::pb::CounterResponse>();
  response->set_value(counter);
  return response;
}

std::unique_ptr<pedro::Message> matrix_multiply(pedro::Parameter args) {
  pedro::pb::MatrixOperation operation;
  args->UnpackTo(&operation);

  Matrix x(operation.left());
  Matrix y(operation.right());

  Matrix z(x.rows(), y.cols());
  for (int i = 0; i < z.rows(); ++i) {
    for (int k = 0; k < x.cols(); ++k) {
      for (int j = 0; j < z.cols(); ++j) {
        z[i][j] += x[i][k] * y[k][j];
      }
    }
  }

  auto response = std::make_unique<pedro::pb::Matrix>();
  z.CopyTo(*response);
  return response;
}

std::unique_ptr<pedro::Message> read_proto(pedro::Parameter args) {
  pedro::pb::CounterRequest request;
  args->UnpackTo(&request);
  std::string name = "test" + std::to_string(request.delta()) + ".proto";
  std::fstream is(name, std::ios::binary | std::ios::in);
  pedro::pb::Vector vector;
  vector.ParseFromIstream(&is);
  counter += vector.data()[0];
  return {};
}

std::unique_ptr<pedro::Message> write_proto(pedro::Parameter args) {
  pedro::pb::CounterRequest request;
  args->UnpackTo(&request);
  std::string name = "test" + std::to_string(request.delta()) + ".proto";
  pedro::pb::Vector vector;
  for (int i = 0; i < 1000000; ++i) {
    vector.add_data(i);
  }
  std::fstream fs(name, std::ios::binary | std::ios::out);
  vector.SerializeToOstream(&fs);
  return {};
}

class ProcessPoolBenchmark {
  std::vector<std::shared_ptr<pedro::Worker>> workers_;

public:
  void ensure_workers(size_t workers) {
    while (workers_.size() < workers) {
      workers_.emplace_back(pedro::Worker::CreateWorker());
    }
  }

  pedro::ProcessPool create_pool(size_t workers) {
    ensure_workers(workers);
    pedro::ProcessPool pool_;
    for (size_t i = 0; i < workers; ++i) {
      pool_.AddWorker(workers_[i]);
    }
    return pool_;
  }

  void make_test_file(int n, const std::string &name) {
    pedro::pb::Vector vector;
    for (int i = 0; i < n; ++i) {
      vector.add_data(i);
    }
    std::fstream fs(name, std::ios::binary | std::ios::out);
    vector.SerializeToOstream(&fs);
  }

  void test_read_io_job(size_t workers) {
    pedro::ProcessPool pool = create_pool(workers);
    int n = 512;

    auto cost = duration([&] {
      for (int i = 0; i < n; ++i) {
        pedro::pb::CounterRequest r;
        r.set_delta(i);
        pool.Submit(write_proto, pedro::MakeParameter(&r));
      }
      pool.Sync();
    });
    cout << cost << "ms" << endl;
  }

  void test_write_io_job(size_t workers) {
    pedro::ProcessPool pool = create_pool(workers);
    int n = 512;
    auto cost = duration([&] {
      for (int i = 0; i < n; ++i) {
        pedro::pb::CounterRequest r;
        r.set_delta(i);
        pool.Submit(write_proto, pedro::MakeParameter(&r));
      }
      pool.Sync();
    });
    cout << cost << "ms" << endl;
  }

  void test_calc_job(size_t workers) {
    pedro::ProcessPool pool = create_pool(workers);

    int n = 10000, m = 100;

    Matrix mat(m, m);
    for (int i = 0; i < m; ++i) {
      for (int j = 0; j < m; ++j) {
        mat[i][j] = i + j;
      }
    }
    pedro::pb::MatrixOperation operation;
    mat.CopyTo(*operation.mutable_left());
    mat.CopyTo(*operation.mutable_right());
    operation.set_type(pedro::pb::MatrixOperationType::MULTIPLY);

    auto args = pedro::MakeParameter(&operation);
    auto cost = duration([&] {
      for (int i = 0; i < n; ++i) {
        pool.Submit(matrix_multiply, args);
      }
      pool.Sync();
    });
    cout << cost << "ms" << endl;
  }

  void test_light_weight(size_t workers) {
    pedro::ProcessPool pool = create_pool(workers);

    int n = 1000000;
    pedro::pb::CounterRequest request;

    auto args = pedro::MakeParameter(&request);
    auto cost = duration([&] {
      for (int i = 0; i < n; ++i) {
        pool.Submit(add, args);
      }
      pool.Sync();
    });
    cout << cost << "ms" << endl;
  }
};

int main() {
  ProcessPoolBenchmark bench;
  cout << "[test light weight]\n";
  for (int i = 1; i <= 8; i *= 2) {
    bench.test_light_weight(i);
  }

  cout << "[test calc job]\n";
  for (int i = 1; i <= 8; i *= 2) {
    bench.test_calc_job(i);
  }

  cout << "[test write io job]\n";
  for (int i = 1; i <= 8; i *= 2) {
    bench.test_write_io_job(i);
  }

  cout << "[test read io job]\n";
  for (int i = 1; i <= 8; i *= 2) {
    bench.test_read_io_job(i);
  }
  return 0;
}