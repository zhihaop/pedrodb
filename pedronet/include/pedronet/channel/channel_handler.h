#ifndef PEDRONET_CHANNEL_HANDLER_H
#define PEDRONET_CHANNEL_HANDLER_H
#include <any>
#include <future>
#include <vector>

namespace pedronet {
struct SocketAddress {};

struct ChannelHandlerContext {};

struct ChannelHandler {
  virtual void OnHandlerAdded(ChannelHandlerContext& ctx) = 0;
  virtual void OnHandlerRemoved(ChannelHandlerContext& ctx) = 0;
};

struct ChannelInboundHandler : public ChannelHandler {
  virtual void OnChannelRegistered(ChannelHandlerContext& ctx) = 0;
  virtual void OnChannelUnregistered(ChannelHandlerContext& ctx) = 0;
  virtual void OnChannelActive(ChannelHandlerContext& ctx) = 0;
  virtual void OnChannelInactive(ChannelHandlerContext& ctx) = 0;
  virtual void OnChannelRead(ChannelHandlerContext& ctx, std::any& msg) = 0;
  virtual void OnChannelReadComplete(ChannelHandlerContext& ctx) = 0;
  virtual void OnUserEventTriggered(ChannelHandlerContext& ctx,
                                    std::any& event) = 0;
  virtual void OnChannelWritablityChange(ChannelHandlerContext& ctx);
};

struct ChannelOutboundHandler : public ChannelHandler {
  virtual void Bind(ChannelHandlerContext& ctx, SocketAddress local,
                    std::promise<bool> success) = 0;
  virtual void Connect(ChannelHandlerContext& ctx, SocketAddress local,
                       SocketAddress remote, std::promise<bool> success) = 0;
  virtual void Disconnect(ChannelHandlerContext& ctx,
                          std::promise<bool> success) = 0;
  virtual void Close(ChannelHandlerContext& ctx,
                     std::promise<bool> success) = 0;
  virtual void Deregister(ChannelHandlerContext& ctx,
                          std::promise<bool> success) = 0;
  virtual void Read(ChannelHandlerContext& ctx) = 0;
  virtual void Write(ChannelHandlerContext& ctx, std::any& msg,
                     std::promise<bool> success) = 0;
  virtual void Flush(ChannelHandlerContext& ctx) = 0;
};

class ChannelPipeline {
  std::vector<std::shared_ptr<ChannelInboundHandler>> inbound_handlers_;
  std::vector<std::shared_ptr<ChannelOutboundHandler>> outbound_handlers_;

 public:
};
}  // namespace pedronet
#endif  // PEDRONET_CHANNEL_HANDLER_H