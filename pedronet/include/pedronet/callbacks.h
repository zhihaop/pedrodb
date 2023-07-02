#ifndef PEDRONET_CALLBACK_H
#define PEDRONET_CALLBACK_H

#include "pedronet/socket.h"

#include <functional>
#include <memory>
#include <pedrolib/timestamp.h>
namespace pedronet {

using Timestamp = pedrolib::Timestamp;
using Duration = pedrolib::Duration;

class Buffer;
class TcpConnection;
class ReceiveEvents;
class SelectEvents;
class Channel;

using Callback = std::function<void()>;
using SelectorCallback = std::function<void(ReceiveEvents events, Timestamp)>;
using TcpConnectionPtr = std::shared_ptr<TcpConnection>;
using MessageCallback =
    std::function<void(const TcpConnectionPtr &, Buffer &, Timestamp)>;
using WriteCompleteCallback = std::function<void(const TcpConnectionPtr &)>;
using HighWatermarkCallback =
    std::function<void(const TcpConnectionPtr &, size_t)>;
using ErrorCallback =
    std::function<void(const TcpConnectionPtr &, core::Error)>;
using CloseCallback = std::function<void(const TcpConnectionPtr &)>;
using ConnectionCallback = std::function<void(const TcpConnectionPtr &)>;

} // namespace pedronet

#endif // PEDRONET_CALLBACK_H