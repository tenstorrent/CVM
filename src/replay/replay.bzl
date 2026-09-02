load("@rules_hdl//verilog:providers.bzl", "verilog_library")

def _replay_impl(ctx):

    sv = ctx.outputs.sv
    merged = ctx.outputs.merged

    args = ctx.actions.args()
    args.add_all("--definitions", ctx.files.srcs)
    args.add("--sv", sv)
    args.add("--merged", merged)

    inputs = list(ctx.files.srcs)

    # Unlike packet_gen, topology is OPTIONAL: replaying an arbitrary module
    # should not force the user to define a topology first. Only pass the flag
    # when one was actually given.
    if ctx.file.topology:
        args.add("--topology", ctx.file.topology)
        inputs.append(ctx.file.topology)

    outputs = [sv, merged]

    ctx.actions.run(
        arguments = [args],
        executable = ctx.executable._gen,
        inputs = inputs,
        outputs = outputs,
        mnemonic = "CVMReplayGen",
    )

    return [
        DefaultInfo(
            files = depset(outputs),
        ),
    ]

_replay = rule(
    _replay_impl,
    attrs = {
        "srcs": attr.label_list(
            mandatory = True,
            allow_files = True,
        ),
        "topology": attr.label(
            mandatory = False,
            allow_single_file = [".json"],
        ),
        "sv": attr.output(),
        "merged": attr.output(),
        "_gen": attr.label(
            default = "//src/replay:replay_gen",
            executable = True,
            cfg = "exec",
        ),
    },
    provides = [
        DefaultInfo,
    ],
)

def replay(name, srcs, topology = None, visibility = None, **kwargs):
    """Generate a replay interposer from a YAML spec.

    Emits only SystemVerilog: the port layout is handed to the runtime by the
    generated module's bind calls, so there is no generated C++. The recording is
    not a build input either, so one build replays any number of conforming
    recordings.
    """

    sv = name + ".sv"
    merged = name + "_merged.yml"

    _replay(
        name = name,
        srcs = srcs,
        sv = sv,
        merged = merged,
        topology = topology,
        visibility = visibility,
        **kwargs
    )

    verilog_library(
        name = name + "_sv",
        srcs = [sv],
        deps = ["@cvm//:replay_sv"],
        visibility = visibility,
    )
