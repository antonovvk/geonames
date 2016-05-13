cc_library(
    name = "geonames",
    srcs = ["geonames.cpp"],
    hdrs = ["geonames.h"],
    deps = [
        "@mms//:mms",
    ],
    copts = [
        "-std=c++11",
        "-Wall",
    ],
    linkopts = [
        "-lstdc++",
    ],
    visibility = ["//visibility:public"],
)

cc_test(
    name = "ut",
    srcs = ["geonames_ut.cpp"],
    copts = [
        "-Iexternal/gtest/include",
    ],
    deps = [
        ":geonames",
        "@gtest//:main",
    ],
)
