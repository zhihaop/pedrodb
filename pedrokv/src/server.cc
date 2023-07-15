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

  codec_.OnMessage([this](auto&& conn, auto&& sender, auto&& requests) {
    HandleRequest(conn, sender, requests);
  });

  codec_.OnConnect([](const pedronet::TcpConnectionPtr& conn) {
    PEDROKV_INFO("connect to client {}", *conn);
  });

  codec_.OnClose([](const pedronet::TcpConnectionPtr& conn) {
    PEDROKV_INFO("disconnect to client {}", *conn);
  });

  server_.OnError([](const pedronet::TcpConnectionPtr& conn, Error err) {
    PEDROKV_ERROR("client {} error: {}", *conn, err);
  });

  server_.OnConnect(codec_.GetOnConnect());
  server_.OnClose(codec_.GetOnClose());
  server_.OnMessage(codec_.GetOnMessage());
}

void Server::HandleRequest(const TcpConnectionPtr&,
                           const ResponseSender& sender,
                           const RequestView& request) {

  Response response;
  response.id = request.id;
  pedrodb::Status status = pedrodb::Status::kOk;
  switch (request.type) {
    case RequestType::kGet: {
      status = db_->Get({}, request.key, &response.data);
      break;
    }
    case RequestType::kDelete: {
      status = db_->Delete({}, request.key);
      break;
    }
    case RequestType::kPut: {
      status = db_->Put({}, request.key, request.value);
      break;
    }
    default: {
      PEDROKV_WARN("invalid request receive, {}", (uint32_t)response.type);
      break;
    }
  }

  if (status != pedrodb::Status::kOk) {
    response.type = ResponseType::kError;
    response.data = fmt::format("err: {}", status);
  } else {
    response.type = ResponseType::kOk;
  }

  sender(std::move(response));
}
}  // namespace pedrokv