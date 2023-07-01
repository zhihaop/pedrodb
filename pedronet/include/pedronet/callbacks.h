#ifndef PEDRONET_CALLBACK_H
#define PEDRONET_CALLBACK_H

#include "pedronet/core/timestamp.h"
#include "pedronet/socket.h"
#include <functional>
#include <memory>
namespace pedronet {

class Buffer;
class TcpConnection;
class ReceiveEvents;
class SelectEvents;

using Callback = std::function<void()>;
using SelectorCallback =
    std::function<void(ReceiveEvents events, core::Timestamp)>;

using TcpConnectionPtr = std::shared_ptr<TcpConnection>;
using MessageCallback =
    std::function<void(const TcpConnectionPtr &, Buffer&, core::Timestamp)>;
using WriteCompleteCallback = std::function<void(const TcpConnectionPtr &)>;
using HighWatermarkCallback =
    std::function<void(const TcpConnectionPtr &, size_t)>;
using ErrorCallback =
    std::function<void(const TcpConnectionPtr &, Socket::Error)>;
using CloseCallback = std::function<void(const TcpConnectionPtr &)>;
using ConnectionCallback = std::function<void(const TcpConnectionPtr &)>;

} // namespace pedronet

#endif // PEDRONET_CALLBACK_H