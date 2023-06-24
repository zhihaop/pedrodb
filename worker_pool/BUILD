load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_proto_library")
load("@rules_proto//proto:defs.bzl", "proto_library")

cc_library(
    name = "library",
    srcs = glob([
        "include/*.h",
        "src/*.cc",
    ]),
    hdrs = glob([
        "**/*.h",
    ]),
    deps = [
        ":proto_cc",
    ],
    includes = [
        "include",
        "."
    ],
    copts = [
        "-std=c++20",
        "-O2",
    ]
)

cc_binary(
    name = "main",
    srcs = ["src/main.cpp"],
    deps = [
        ":library",
    ],
    includes = [
        "include",
    ],
    copts = [
        "-std=c++20",
        "-O2",
    ],
)

cc_binary(
    name = "bench_blocking_queue",
    srcs = [
        "test/bench_blocking_queue.cc"
    ],
    deps = [
        ":library",
    ],
    copts = [
        "-std=c++20",
        "-O2",
    ],
)

cc_binary(
    name = "bench_thread_pool",
    srcs = [
        "test/bench_thread_pool.cc"
    ],
    deps = [
        ":library",
    ],
    copts = [
        "-std=c++20",
        "-O2",
    ],
)

cc_binary(
    name = "bench_process_pool",
    srcs = [
        "test/bench_process_pool.cc"
    ],
    deps = [
        ":library",
    ],
    copts = [
        "-std=c++20",
        "-O2",
    ],
)

proto_library(
    name = "proto",
    srcs = glob([
        "proto/*.proto",
    ]),
    deps = ["@com_google_protobuf//:any_proto"],
)

cc_proto_library(
    name = "proto_cc",
    deps = [":proto"],
)