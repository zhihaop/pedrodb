#ifndef PEDROKV_DEFINES_H
#define PEDROKV_DEFINES_H

#include <pedrolib/noncopyable.h>
#include <pedrolib/nonmovable.h>

#include <pedrolib/buffer/buffer.h>
#include <pedrolib/duration.h>
#include <pedrolib/timestamp.h>

#include <pedronet/inetaddress.h>
#include <pedronet/tcp_connection.h>

namespace pedrokv {
using noncopyable = pedrolib::noncopyable;
using nonmovable = pedrolib::nonmovable;

using Timestamp = pedrolib::Timestamp;
using Duration = pedrolib::Duration;

using Buffer = pedrolib::Buffer;
using Error = pedrolib::Error;

using TcpConnection = pedronet::TcpConnection;
using InetAddress = pedronet::InetAddress;
} // namespace pedrokv
#endif // PEDROKV_DEFINES_H
