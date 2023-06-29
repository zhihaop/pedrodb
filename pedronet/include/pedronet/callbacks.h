#ifndef PEDRONET_CALLBACK_H
#define PEDRONET_CALLBACK_H

#include "pedronet/core/file.h"
#include "pedronet/core/timestamp.h"
#include <functional>
#include <memory>
namespace pedronet {

class Buffer;
class TcpConnection;
class ReceiveEvents;
class SelectEvents;

using TcpConnectionPtr = std::shared_ptr<TcpConnection>;
using Callback = std::function<void()>;
using MessageCallback =
    std::function<void(const TcpConnectionPtr &, Buffer *, core::Timestamp)>;
using SelectorCallback =
    std::function<void(ReceiveEvents events, core::Timestamp)>;
using WriteCompleteCallback = std::function<void(const TcpConnectionPtr &)>;
using HighWatermarkCallback =
    std::function<void(const TcpConnectionPtr &, size_t)>;
using ErrorCallback = std::function<void(const TcpConnectionPtr &)>;
using CloseCallback = std::function<void(const TcpConnectionPtr &)>;
using ConnectionCallback = std::function<void(const TcpConnectionPtr &)>;

} // namespace pedronet

#endif // PEDRONET_CALLBACK_H