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

    rules_verilator_hash="a665eda"
    maybe(
        http_archive,
        name = "rules_verilator",
        sha256 = "fef80f179c48b2b755ec0896b3b266331752ab255f5328d075a784ec302e56b6",
        strip_prefix = "rules_verilator-{commit}".format(commit=rules_verilator_hash),
        url = "https://aus-gitlab.local.tenstorrent.com/riscv/rules_verilator/-/archive/{commit}/rules_verilator-{commit}.tar.bz2".format(commit=rules_verilator_hash),
    )

    rules_vcs_hash="f674ef648b9d3a15c4a312f6186191f6b9d1dca3"
    maybe(
        http_archive,
        name = "rules_vcs",
        sha256 = "7a8fafb1dcab6d76b20d54e65153ed9f992ea073c1a817312dd1f4c5f4ba7c3a",
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

    com_github_gflags_gflags_hash="8df06cdb4a997b5b7ae186537cda18ecb43ea9cc"
    maybe(
        http_archive,
        name = "com_github_gflags_gflags",
        sha256 = "46e90f6ed081ad08c31f3a643a0182e28a162d12a48d81a85804363fccbdda3a",
        url = "https://aus-gitlab.local.tenstorrent.com/opensrc/opensrc-gflags/-/archive/{commit}/opensrc-gflags-{commit}.tar.bz2".format(commit=com_github_gflags_gflags_hash),
        strip_prefix = "opensrc-gflags-{commit}".format(commit=com_github_gflags_gflags_hash),
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
