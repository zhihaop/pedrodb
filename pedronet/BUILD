load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library")

cc_library(
    name = "pedronet",
    srcs = glob([
        "src/**/*.cc",
        'src/**/*.c',
        "src/**/*.cpp",
        "include/**/*.h",
        "include/**/*.hpp",
    ]),
    hdrs = glob([
        "include/**/*.h",
        "include/**/*.hpp",
    ]),
    includes = ["include"],
    linkopts = [
        "-lspdlog",
        "-lfmt"
    ],
    copts = [
        "-std=c++17",
    ],
)

cc_binary(
    name = "test_event_loop",
    srcs = ["test/test_event_loop.cc"],
    deps = [":pedronet"],
    copts = [
        "-std=c++17",
    ],
)

cc_binary(
    name = "test_tcp_server",
    srcs = ["test/test_tcp_server.cc"],
    deps = [":pedronet"],
    copts = [
        "-std=c++17",
    ],
)