load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@bazel_tools//tools/build_defs/repo:utils.bzl", "maybe")

def cvm_dependencies():

    maybe(
        http_archive,
        name = "com_google_googletest",
        urls = ["https://github.com/google/googletest/archive/refs/tags/release-1.11.0.tar.gz"],
        sha256 = "b4870bf121ff7795ba20d20bcdd8627b8e088f2d1dab299a031c1034eddc93d5",
        strip_prefix = "googletest-release-1.11.0",
    )

    maybe(
        http_archive,
        name = "bazel_skylib",
        urls = [
            "https://github.com/bazelbuild/bazel-skylib/releases/download/1.2.0/bazel-skylib-1.2.0.tar.gz",
        ],
        sha256 = "af87959afe497dc8dfd4c6cb66e1279cb98ccc84284619ebfec27d9c09a903de",
    )

    # rules_hdl is provided by the local_repository in WORKSPACE pointing at
    # //rules_hdl_compat. rules_verilator + rules_verilog are loaded directly
    # in WORKSPACE; their rules are only invoked under Bazel 7 + bzlmod for
    # //test/... — not by the Bazel-6-WORKSPACE top-target build path.

    rules_python_version = "0.11.0"
    maybe(
        http_archive,
        name = "rules_python",
        sha256 = "c03246c11efd49266e8e41e12931090b613e12a59e6f55ba2efd29a7cb8b4258",
        strip_prefix = "rules_python-{}".format(rules_python_version),
        url = "https://github.com/bazelbuild/rules_python/archive/refs/tags/{VERSION}.tar.gz".format(VERSION = rules_python_version),
    )

    maybe(
        http_archive,
        name = "com_github_gflags_gflags",
        sha256 = "34af2f15cf7367513b352bdcd2493ab14ce43692d2dcd9dfc499492966c64dcf",
        url = "https://github.com/gflags/gflags/archive/refs/tags/v2.2.2.tar.gz",
        strip_prefix = "gflags-2.2.2",
    )

    maybe(
        http_archive,
        name = "fmt",
        url = "https://github.com/fmtlib/fmt/releases/download/9.1.0/fmt-9.1.0.zip",
        sha256 = "cceb4cb9366e18a5742128cb3524ce5f50e88b476f1e54737a47ffdf4df4c996",
        strip_prefix = "fmt-9.1.0",
        build_file_content = """
cc_library(
    name = "fmt",
    hdrs = glob(["include/**"]),
    srcs = ["src/format.cc", "src/os.cc"],
    includes = ["include"],
    visibility = ["//visibility:public"],
)
    """
    )
