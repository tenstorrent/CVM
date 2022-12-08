load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@bazel_tools//tools/build_defs/repo:utils.bzl", "maybe")

def cvm_dependencies():

    maybe(
        http_archive,
        name = "com_google_googletest",
        urls = ["https://github.com/google/googletest/archive/release-1.11.0.zip"],
        sha256 = "353571c2440176ded91c2de6d6cd88ddd41401d14692ec1f99e35d013feda55a",
        strip_prefix = "googletest-release-1.11.0",
    )

    maybe(
        http_archive,
        name = "bazel_skylib",
        urls = [
        "https://mirror.bazel.build/github.com/bazelbuild/bazel-skylib/releases/download/1.2.0/bazel-skylib-1.2.0.tar.gz",
        "https://github.com/bazelbuild/bazel-skylib/releases/download/1.2.0/bazel-skylib-1.2.0.tar.gz",
        ],
        sha256 = "af87959afe497dc8dfd4c6cb66e1279cb98ccc84284619ebfec27d9c09a903de",
    )

    rules_hdl_hash="5ded43332b61f7fadf118ef8b5ebd26385d91500"
    maybe(
        http_archive,
        name = "rules_hdl",
        sha256 = "ab4b245c4fcf62d69a151e9238a38e8f26ea7138e0283bce0f64a1557641d614",
        strip_prefix = "bazel_rules_hdl-{commit}".format(commit=rules_hdl_hash),
        url = "https://aus-gitlab.local.tenstorrent.com/riscv/bazel_rules_hdl/-/archive/{commit}/bazel_rules_hdl-{commit}.tar.bz2".format(commit=rules_hdl_hash),
    )

    rules_verilator_hash="8d22521c5ea5cb72831301676bae7667d02b268b"
    maybe(
        http_archive,
        name = "rules_verilator",
        sha256 = "e162ea171686c3eb696e0be46dff03508bc5ad2c8161397c329a13a5bf2f56ab",
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
        sha256 = "1fe4f7f532a7af16bbe157a7757d7550c23f64798be07638f1f2df521bcf0d3c",
        strip_prefix = "rules_python-{}".format(rules_python_version),
        url = "https://github.com/bazelbuild/rules_python/archive/{}.zip".format(rules_python_version),
    )
