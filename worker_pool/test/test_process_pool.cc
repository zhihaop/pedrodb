#include "process_pool.h"
#include "proto/counter.pb.h"
#include "proto/sum.pb.h"

#include <iostream>
using namespace std;

int64_t counter;
std::unique_ptr<pedro::Message> add(const pedro::Any &msg) {
  pedro::pb::CounterRequest request;
  msg.UnpackTo(&request);
  counter += request.delta();
  return {};
}

std::unique_ptr<pedro::Message> count(const pedro::Any &) {
  auto response = std::make_unique<pedro::pb::CounterResponse>();
  response->set_value(counter);
  return response;
}

std::unique_ptr<pedro::Message> sum(const pedro::Any& msg) {
  pedro::pb::SumRequest request;
  msg.UnpackTo(&request);

  for (int v: request.data()) {
    for (int w: request.data()) {
      counter += v * w;
    }
  }
  
  auto response = std::make_unique<pedro::pb::SumResponse>();
  response->set_sum(counter);
  return response;
}

int main() {
  pedro::ProcessPool pool;
  for (int i = 0; i < 16; ++i) {
    pool.AddWorker(pedro::Worker::CreateWorker());
  }

  cout << "created worker!" << endl;

  auto st = chrono::steady_clock::now();
  int x = 1;
  for (int i = 1; i <= 10000; ++i) {
    pedro::pb::SumRequest request;
    for (int j = 0; j < 1000; ++j) {
      request.add_data(x++);
    }
    pool.Submit(sum, request);
  }
  auto et = chrono::steady_clock::now();

  int64_t value = 0;
  for (auto worker : pool.Workers()) {
    auto response =
        worker->Call<pedro::pb::CounterResponse>(count, pedro::pb::Void{});
    value += response.value();
  }

  cout << value << endl;
  cout << (et - st) / 1ms << endl;
  return 0;
}