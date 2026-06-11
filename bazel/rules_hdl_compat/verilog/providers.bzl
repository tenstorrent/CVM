# Compatibility shim providing the `@rules_hdl//verilog:providers.bzl` surface
# downstream Tenstorrent repos still load. Each verilog_library target emits
# two providers:
#   - The legacy VerilogInfo defined here (dag/plis), for downstream callers.
#   - The upstream @rules_verilog VerilogInfo, so @rules_verilator can consume
#     cvm's verilog_library aggregators directly via the `module = ...` attr.
#
# Delete this directory (along with the @rules_hdl wiring in MODULE.bazel /
# WORKSPACE) once downstream consumers migrate to @rules_verilog.

load("@rules_verilog//verilog:defs.bzl", _UpstreamVerilogInfo = "VerilogInfo")

VerilogInfo = provider(
    doc = "Legacy DAG-based VerilogInfo. Provider identity must stay stable for downstream compatibility.",
    fields = {
        "dag": "depset of DAG entries (struct of srcs/hdrs/libs/fs/views/deps/label/strip_include_prefix).",
        "plis": "depset (unused today; kept for downstream field compatibility).",
    },
)

def make_dag_entry(srcs, hdrs, libs, fs, views, deps, label, strip_include_prefix):
    return struct(
        srcs = tuple(srcs),
        hdrs = tuple(hdrs),
        libs = tuple(libs),
        fs = tuple(fs),
        views = tuple(views),
        deps = tuple(deps),
        label = label,
        strip_include_prefix = strip_include_prefix,
    )

def make_verilog_info(new_entries = (), old_infos = ()):
    return VerilogInfo(
        dag = depset(
            direct = new_entries,
            order = "postorder",
            transitive = [x.dag for x in old_infos],
        ),
        plis = depset(),
    )

def _verilog_library_impl(ctx):
    legacy = make_verilog_info(
        new_entries = [make_dag_entry(
            srcs = ctx.files.srcs,
            hdrs = ctx.files.hdrs,
            libs = ctx.files.libs,
            fs = ctx.files.fs,
            views = ctx.attr.views,
            deps = ctx.attr.deps,
            label = ctx.label,
            strip_include_prefix = ctx.attr.strip_include_prefix,
        )],
        old_infos = [dep[VerilogInfo] for dep in ctx.attr.deps],
    )

    # Flatten the legacy dag into upstream's flat srcs/hdrs view, so that
    # @rules_verilator's verilator_cc_library can consume these targets via
    # `module = ...` directly.
    all_srcs = []
    all_hdrs = []
    for entry in legacy.dag.to_list():
        all_srcs += list(entry.srcs)
        all_hdrs += list(entry.hdrs)

    upstream = _UpstreamVerilogInfo(
        srcs = depset(all_srcs),
        hdrs = depset(all_hdrs),
        includes = depset(),
        data = depset(),
        standard = "",
        top_module = ctx.attr.top_module,
        deps = depset(),
    )

    return [legacy, upstream, DefaultInfo(files = depset(all_srcs + all_hdrs))]

verilog_library = rule(
    attrs = {
        "srcs": attr.label_list(allow_files = True),
        "hdrs": attr.label_list(allow_files = True),
        "libs": attr.label_list(allow_files = True),
        "fs": attr.label_list(allow_files = True),
        "views": attr.string_list(),
        "deps": attr.label_list(providers = [VerilogInfo]),
        "strip_include_prefix": attr.string(),
        "top_module": attr.string(
            default = "",
            doc = "Top-module name. Surfaced on the upstream VerilogInfo so verilator_cc_library can find it.",
        ),
    },
    implementation = _verilog_library_impl,
)
