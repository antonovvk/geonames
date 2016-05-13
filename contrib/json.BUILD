cc_library(
    name = "json",
    hdrs = [
        "src/json.hpp",
    ],
    copts = [
        "-std=c++11",
        "-Wall",
    ],
    visibility = ["//visibility:public"],
)

# TODO: add test input files to build
cc_test(
    name = "ut",
    srcs = [
        "test/unit.cpp",
        "test/catch.hpp",
    ],
    includes = [
        "src",
    ],
    copts = [
        "-Wall",
        "-Wextra -pedantic",
        "-Weffc++",
        "-Wcast-align",
        "-Wcast-qual",
        "-Wctor-dtor-privacy",
        "-Wdisabled-optimization",
        "-Wformat=2",
        "-Winit-self",
        "-Wmissing-declarations",
        "-Wmissing-include-dirs",
        "-Wold-style-cast",
        "-Woverloaded-virtual",
        "-Wredundant-decls",
        "-Wshadow",
        "-Wsign-conversion",
        "-Wsign-promo",
        "-Wstrict-overflow=5",
        "-Wswitch",
        "-Wundef",
        "-Wno-unused",
        "-Wnon-virtual-dtor",
        "-Wreorder",
        "-Wdeprecated",
        "-Wfloat-equal",
    ],
    deps = [
        ":json",
    ],
    linkopts = [
        "-lm",
        "-lstdc++",
    ],
)
