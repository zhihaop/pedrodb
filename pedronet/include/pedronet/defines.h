#ifndef PEDRONET_DEFINES_H
#define PEDRONET_DEFINES_H

#include <pedrolib/buffer/array_buffer.h>
#include <pedrolib/buffer/buffer.h>
#include <pedrolib/buffer/buffer_view.h>
#include <pedrolib/comparable.h>
#include <pedrolib/duration.h>
#include <pedrolib/executor/executor.h>
#include <pedrolib/file/error.h>
#include <pedrolib/file/file.h>
#include <pedrolib/timestamp.h>

namespace pedronet {
template <typename T> using Comparable = pedrolib::Comparable<T>;

using File = pedrolib::File;
using Error = pedrolib::Error;
using Duration = pedrolib::Duration;
using Timestamp = pedrolib::Timestamp;
using Executor = pedrolib::Executor;
using Buffer = pedrolib::Buffer;
using ArrayBuffer = pedrolib::ArrayBuffer;
using BufferView = pedrolib::BufferView;
} // namespace pedronet

#endif // PEDRONET_DEFINES_H
