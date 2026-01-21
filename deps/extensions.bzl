"""Module extensions for cvm internal dependencies."""

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

def _internal_deps_impl(ctx):
    """Implementation of internal_deps module extension."""

    # rules_vcs (cvm-specific, not needed by rules_verilator)
    http_archive(
        name = "rules_vcs",
        sha256 = "7a8fafb1dcab6d76b20d54e65153ed9f992ea073c1a817312dd1f4c5f4ba7c3a",
        strip_prefix = "rules_vcs-f674ef648b9d3a15c4a312f6186191f6b9d1dca3",
        url = "https://aus-gitlab.local.tenstorrent.com/riscv/rules_vcs/-/archive/f674ef648b9d3a15c4a312f6186191f6b9d1dca3/rules_vcs-f674ef648b9d3a15c4a312f6186191f6b9d1dca3.tar.bz2",
    )

internal_deps = module_extension(
    implementation = _internal_deps_impl,
)
