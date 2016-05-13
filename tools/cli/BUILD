cc_binary(
    name = "cli",
    srcs = ["main.cpp"],
    deps = [
        "@tclap//:tclap",
        "@json//:json",
        "//geonames",
    ],
    copts = [
        "-std=c++11",
        "-Wall",
    ],
    linkopts = [
        "-lstdc++",
        "-lm",
    ],
    visibility = ["//visibility:public"],
)
