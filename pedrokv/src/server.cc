#include "pedrokv/server.h"

namespace pedrokv {
pedrokv::Server::Server(pedronet::InetAddress address,
                        pedrokv::ServerOptions options)
    : address_(std::move(address)), options_(std::move(options)) {
  server_.SetGroup(options_.boss_group, options_.worker_group);

  auto stat = pedrodb::DB::Open(options_.db_options, options_.db_path, &db_);
  if (stat != pedrodb::Status::kOk) {
    PEDROKV_FATAL("failed to open db {}", options_.db_path);
  }

  codec_.OnMessage([this](const auto &conn, auto &&requests) {
    HandleRequest(conn, requests);
  });

  codec_.OnConnect([](const pedronet::TcpConnectionPtr &conn) {
    PEDROKV_INFO("connect to client {}", *conn);
  });

  codec_.OnClose([](const pedronet::TcpConnectionPtr &conn) {
    PEDROKV_INFO("disconnect to client {}", *conn);
  });

  server_.OnError([](const pedronet::TcpConnectionPtr &conn, Error err) {
    PEDROKV_ERROR("client {} error: {}", *conn, err);
  });

  server_.OnConnect(codec_.GetOnConnect());
  server_.OnClose(codec_.GetOnClose());
  server_.OnMessage(codec_.GetOnMessage());
}

void Server::HandleRequest(const std::shared_ptr<TcpConnection> &conn,
                           std::queue<Request> &requests) {
  auto buffer = std::make_shared<pedrolib::ArrayBuffer>();
  Response response;

  while (!requests.empty()) {
    Request request = std::move(requests.front());
    requests.pop();

    response.id = request.id;
    pedrodb::Status status = pedrodb::Status::kOk;
    switch (request.type) {
    case Request::Type::kGet: {
      status = db_->Get({}, request.key, &response.data);
      break;
    }
    case Request::Type::kDelete: {
      status = db_->Delete({}, request.key);
      break;
    }
    case Request::Type::kSet: {
      status = db_->Put({}, request.key, request.value);
      break;
    }
    default: {
      PEDROKV_WARN("invalid request receive");
      break;
    }
    }

    if (status != pedrodb::Status::kOk) {
      response.type = Response::Type::kError;
      response.data = fmt::format("err: {}", status);
    } else {
      response.type = Response::Type::kOk;
    }

    buffer->AppendInt(response.SizeOf());
    response.Pack(buffer.get());
  }
  conn->Send(buffer);
}
} // namespace pedrokv