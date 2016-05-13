cc_library(
    name = "mms",
    srcs = glob([
        "include/mms/impl/*.h",
    ]),
    hdrs = glob([
        "include/mms/features/*/*.h",
        "include/mms/features/*.h",
        "include/mms/*.h",
    ]),
    copts = [
        "-Iexternal/mms/include/mms",
    ],
    linkopts = [
        "-lstdc++",
    ],
    visibility = ["//visibility:public"],
)
