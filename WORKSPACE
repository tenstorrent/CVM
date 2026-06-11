workspace(name = "cvm")

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

local_repository(
    name = "rules_hdl",
    path = "bazel/rules_hdl_compat",
)

# Needed by rules_hdl_compat's load of @rules_verilog. Small tarball; under
# Bazel 6 + WORKSPACE the rules here are never invoked from the top BUILD.bazel
# chain — only the VerilogInfo provider symbol is referenced.
http_archive(
    name = "rules_verilog",
    url = "https://github.com/hw-bzl/bazel_rules_verilog/releases/download/v1.1.0/bazel_rules_verilog-1.1.0.tar.gz",
    sha256 = "043196310d1ba692ec217c3778663da0d232a3746ba6291d3a12d6461de24021",
    strip_prefix = "bazel_rules_verilog-1.1.0",
)

load("//deps:repositories.bzl", "cvm_dependencies")
cvm_dependencies()

load("//deps:toolchains1.bzl", "cvm_toolchains1")
cvm_toolchains1()

load("//deps:toolchains2.bzl", "cvm_toolchains2")
cvm_toolchains2()
