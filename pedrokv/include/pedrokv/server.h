#ifndef PEDROKV_KV_SERVER_H
#define PEDROKV_KV_SERVER_H

#include "pedrokv/codec/server_codec.h"
#include "pedrokv/defines.h"
#include "pedrokv/logger/logger.h"
#include "pedrokv/options.h"

#include <memory>
#include <pedrodb/db.h>
#include <pedronet/tcp_server.h>
#include <utility>
namespace pedrokv {

class Server : nonmovable,
               noncopyable,
               public std::enable_shared_from_this<Server> {
  pedronet::TcpServer server_;
  pedronet::InetAddress address_;
  ServerOptions options_;

  std::shared_ptr<pedrodb::DB> db_;
  ServerCodec codec_;

  Response ProcessRequest(ServerCodecContext &ctx, const Request &request) {
    Response response;
    response.id = request.id;
    switch (request.type) {
    case Request::Type::kGet: {
      PEDROKV_INFO("handle Get");
      auto status = db_->Get({}, request.key, &response.data);
      if (status != pedrodb::Status::kOk) {
        response.type = Response::Type::kError;
        response.data = fmt::format("err: {}", status);
        return response;
      }
      response.type = Response::Type::kOk;
      return response;
    }
    case Request::Type::kDelete: {
      PEDROKV_INFO("handle Delete");
      auto status = db_->Delete({}, request.key);
      if (status != pedrodb::Status::kOk) {
        response.type = Response::Type::kError;
        response.data = fmt::format("err: {}", status);
        return response;
      }
      response.type = Response::Type::kOk;
      return response;
    }
    case Request::Type::kSet: {
      PEDROKV_INFO("handle Set");
      auto status = db_->Put({}, request.key, request.value);
      if (status != pedrodb::Status::kOk) {
        response.type = Response::Type::kError;
        response.data = fmt::format("err: {}", status);
        return response;
      }
      response.type = Response::Type::kOk;
      return response;
    }
    default: {
      PEDROKV_WARN("invalid request receive");
      break;
    }
    }
    return response;
  }

public:
  Server(pedronet::InetAddress address, ServerOptions options)
      : address_(std::move(address)), options_(std::move(options)) {
    server_.SetGroup(options_.boss_group, options_.worker_group);

    auto stat = pedrodb::DB::Open(options_.db_options, options_.db_path, &db_);
    if (stat != pedrodb::Status::kOk) {
      PEDROKV_FATAL("failed to open db {}", options_.db_path);
    }

    codec_.OnMessage([this](ServerCodecContext &ctx, const Request &request) {
      Response response = ProcessRequest(ctx, request);
      ctx.SetResponse(response);
    });

    codec_.OnConnect([](const pedronet::TcpConnectionPtr &conn) {
      PEDROKV_INFO("connect to client {}", *conn);
    });

    codec_.OnClose([](const pedronet::TcpConnectionPtr &conn) {
      PEDROKV_INFO("disconnect to client {}", *conn);
    });

    codec_.OnError([](const pedronet::TcpConnectionPtr &conn, Error err) {
      PEDROKV_ERROR("client {} error: {}", *conn, err);
    });

    server_.OnConnect(codec_.GetOnConnect());
    server_.OnClose(codec_.GetOnClose());
    server_.OnMessage(codec_.GetOnMessage());
    server_.OnError(codec_.GetOnError());
    server_.OnHighWatermark(codec_.GetOnHighWatermark());
  }

  void Bind() {
    server_.Bind(address_);
    PEDROKV_INFO("server bind success: {}", address_);
  }

  void Start() {
    server_.Start();
    PEDROKV_INFO("server start");
  }
};

} // namespace pedrokv

#endif // PEDROKV_KV_SERVER_H
