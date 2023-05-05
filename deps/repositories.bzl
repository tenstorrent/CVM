load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@bazel_tools//tools/build_defs/repo:utils.bzl", "maybe")

def cvm_dependencies():

    maybe(
        http_archive,
        name = "com_google_googletest",
        urls = ["https://aus-gitlab.local.tenstorrent.com/riscv/forks/googletest/-/archive/release-1.11.0/googletest-release-1.11.0.tar.bz2"],
        sha256 = "b93e7c392d235153757b52583ffc5159783d5e5f754f392c51d5b664f6d46b3b",
        strip_prefix = "googletest-release-1.11.0",
    )

    maybe(
        http_archive,
        name = "bazel_skylib",
        urls = [
        "https://aus-gitlab.local.tenstorrent.com/riscv/forks/bazel-skylib/-/archive/1.2.0/bazel-skylib-1.2.0.tar.bz2",
        ],
        sha256 = "beba08273c596c2e16c70e70f52689d38c0f19dc449551165e7918415e9dfb80",
        strip_prefix = "bazel-skylib-1.2.0",
    )

    rules_hdl_hash="5ded43332b61f7fadf118ef8b5ebd26385d91500"
    maybe(
        http_archive,
        name = "rules_hdl",
        sha256 = "ab4b245c4fcf62d69a151e9238a38e8f26ea7138e0283bce0f64a1557641d614",
        strip_prefix = "bazel_rules_hdl-{commit}".format(commit=rules_hdl_hash),
        url = "https://aus-gitlab.local.tenstorrent.com/riscv/bazel_rules_hdl/-/archive/{commit}/bazel_rules_hdl-{commit}.tar.bz2".format(commit=rules_hdl_hash),
    )

    rules_verilator_hash="75eea8daa648bc297427930289673ac1a3a7c1bb"
    maybe(
        http_archive,
        name = "rules_verilator",
        sha256 = "ecfc54b739d564dbdbfb230c159cba16c68fee2106f014f2f872372dddd82867",
        strip_prefix = "rules_verilator-{commit}".format(commit=rules_verilator_hash),
        url = "https://aus-gitlab.local.tenstorrent.com/riscv/rules_verilator/-/archive/{commit}/rules_verilator-{commit}.tar.bz2".format(commit=rules_verilator_hash),
    )

    rules_vcs_hash="b76677b84ac59aa7daaf0003e6e0167aaeb75d20"
    maybe(
        http_archive,
        name = "rules_vcs",
        sha256 = "2705e6b6f06368216641d1cbbe9bd87e078b64ee15ef7f08915783107971e237",
        strip_prefix = "rules_vcs-{commit}".format(commit=rules_vcs_hash),
        url = "https://aus-gitlab.local.tenstorrent.com/riscv/rules_vcs/-/archive/{commit}/rules_vcs-{commit}.tar.bz2".format(commit=rules_vcs_hash),
    )

    rules_python_version = "0.11.0"

    maybe(
        http_archive,
        name = "rules_python",
        sha256 = "94e2f4790b55823cf2a58d5e48fccf932ff879b5e868b10bd1e0fa9100ac0311",
        strip_prefix = "rules_python-{}".format(rules_python_version),
        url = "https://aus-gitlab.local.tenstorrent.com/riscv/forks/rules_python/-/archive/{VERSION}/rules_python-{VERSION}.tar.bz2".format(VERSION=rules_python_version),
    )

    gflags_version = "2.2.2"
    maybe(
        http_archive,
        name = "com_github_gflags_gflags",
        sha256 = "018f1973b45e90cc4e8c8bb2f685072ff3b20638f7830b8de19f164e10a7b97f",
        url = "https://aus-gitlab.local.tenstorrent.com/riscv/forks/gflags/-/archive/v{VERSION}/gflags-v{VERSION}.tar.bz2".format(VERSION=gflags_version),
        strip_prefix = "gflags-v{}".format(gflags_version),
    )

    maybe(
        http_archive,
        name = "fmt",
        url = "https://aus-gitlab.local.tenstorrent.com/riscv/forks/fmt/-/archive/8.1.1/fmt-8.1.1.tar.gz",
        sha256 = "3d794d3cf67633b34b2771eb9f073bde87e846e0d395d254df7b211ef1ec7346",
        strip_prefix = "fmt-8.1.1",
        patches = ["@cvm//deps:fmt.patch"],
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
