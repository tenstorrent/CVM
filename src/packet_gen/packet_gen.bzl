load("@rules_hdl//verilog:providers.bzl", "verilog_library")

def _packet_gen_impl(ctx):

    name = ctx.attr.name
    package = ctx.attr.package

    hpp = ctx.outputs.hpp
    cpp = ctx.outputs.cpp
    sv  = ctx.outputs.sv
    merged = ctx.outputs.merged

    incdir = "/".join([
        hpp.root.path,
        hpp.owner.workspace_root,
    ])

    args = ctx.actions.args()
    args.add_all("--definitions", ctx.files.srcs)
    args.add("--hpp"       , hpp)
    args.add("--cpp"       , cpp)
    args.add("--sv"        , sv )
    args.add("--merged"    , merged)
    args.add("--incdir"    , incdir)

    if not package:
      args.add("--name"      , name)
    else:
      args.add("--name"      , package)

    args.add("--topology"  , ctx.file.topology)

    inputs = ctx.files.srcs + [ctx.file.topology]
    outputs = [hpp, cpp, sv, merged]

    ctx.actions.run(
        arguments = [args],
        executable = ctx.executable._gen,
        inputs  = inputs,
        outputs = outputs,
        mnemonic = "CVMPacketGen"
    )

    return [
        DefaultInfo(
            files = depset(outputs,)
        ),
    ]

_packet_gen = rule(
    _packet_gen_impl,
    attrs = {
        "srcs": attr.label_list(
            mandatory = True,
            allow_files = True,
        ),
        "topology": attr.label(
            mandatory = True,
            allow_single_file = [".json"],
        ),
        "package": attr.string(
        ),
        "hpp": attr.output(
        ),
        "cpp": attr.output(
        ),
        "sv": attr.output(
        ),
        "merged": attr.output(
        ),
        "_gen": attr.label(
            default = "//src/packet_gen:packet_gen",
            executable = True,
            cfg = "exec",
        ),
    },
    provides = [
        DefaultInfo,
    ],
)

def packet_gen(name, topology, package = "", visibility = None, cc_attrs = {}, **kwargs):

    hpp = name + ".hpp"
    cpp = name + ".cpp"
    sv  = name + ".sv"
    merged = name + "_merged.yml"

    if package:
      hpp = name + "/" + package + ".hpp"
      cpp = name + "/" + package + ".cpp"
      sv  = name + "/" + package + ".sv"
      merged = name + "/" + package + "_merged.yml"

    _packet_gen(
        name = name,
        hpp  = hpp,
        cpp  = cpp,
        sv   = sv,
        merged = merged,
        topology = topology + "_json",
        package = package,
        visibility = visibility,
        **kwargs,
    )

    native.cc_library(
        name = name + '_cc',
        srcs = [cpp],
        hdrs = [hpp],
        deps = [
            "@cvm//:bitmanip",
            topology,
        ],
        visibility = visibility,
        strip_include_prefix = name if package else ".",
        **cc_attrs,
    )

    verilog_library(
        name = name + '_sv',
        srcs  = [sv],
    )
