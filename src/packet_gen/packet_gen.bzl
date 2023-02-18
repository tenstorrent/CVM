load("@rules_hdl//verilog:providers.bzl", "verilog_library")

def _packet_gen_impl(ctx):

    name = ctx.attr.name

    hpp = ctx.outputs.hpp
    cpp = ctx.outputs.cpp
    sv  = ctx.outputs.sv

    incdir = "/".join([
        hpp.root.path,
        hpp.owner.workspace_root,
    ])

    args = ctx.actions.args()
    args.add("--definition", ctx.file.src)
    args.add("--hpp"       , hpp)
    args.add("--cpp"       , cpp)
    args.add("--sv"        , sv )
    args.add("--incdir"    , incdir)
    args.add("--name"      , name)
    args.add("--topology"  , ctx.file.topology)

    inputs = [ctx.file.src, ctx.file.topology]
    outputs = [hpp, cpp, sv]

    ctx.actions.run(
        arguments = [args],
        executable = ctx.executable._gen,
        inputs  = inputs,
        outputs = outputs,
        mnemonic = "CMVPacketGen"
    )

    return [
        DefaultInfo(
            files = depset(outputs,)
        ),
    ]

_packet_gen = rule(
    _packet_gen_impl,
    attrs = {
        "src": attr.label(
            mandatory = True,
            allow_single_file = True
        ),
        "topology": attr.label(
            mandatory = True,
            allow_single_file = [".json"],
        ),
        "hpp": attr.output(
        ),
        "cpp": attr.output(
        ),
        "sv": attr.output(
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

def packet_gen(name, topology, visibility = None, cc_attrs = {}, **kwargs):

    hpp = name + ".hpp"
    cpp = name + ".cpp"
    sv  = name + ".sv"

    _packet_gen(
        name = name,
        hpp  = hpp,
        cpp  = cpp,
        sv   = sv,
        topology = topology + "_json",
        visibility = visibility,
        **kwargs,
    )

    native.cc_library(
        name = name + '_cc',
        srcs = [cpp],
        hdrs = [hpp],
        deps = [
            "@cvm//:bitmanip",
            "@cvm//src/messenger:messenger"
        ],
        visibility = visibility,
        # FIXME: need a better solution for this
        strip_include_prefix = ".",
        **cc_attrs,
    )

    verilog_library(
        name = name + '_sv',
        srcs  = [sv],
    )
