load("@rules_hdl//verilog:providers.bzl", "verilog_library")

def _replay_impl(ctx):

    sv = ctx.outputs.sv
    merged = ctx.outputs.merged

    args = ctx.actions.args()
    args.add_all("--definitions", ctx.files.srcs)
    args.add("--sv", sv)
    args.add("--merged", merged)

    inputs = list(ctx.files.srcs)

    # Optional because it serves only ${...} interpolation of widths and depths
    # in the spec, which a spec using literal widths never needs. It is not the
    # topology replay needs at runtime: the interposer's LOCATION must name a
    # node of type `replay`, which topology_gen provides separately.
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

def replay(name, srcs, topology = None, deps = None, visibility = None, **kwargs):
    """Generate a replay interposer from a YAML spec.

    Emits only SystemVerilog: the port layout is handed to the runtime by the
    generated module's bind calls, so there is no generated C++. The recording is
    not a build input either, so one build replays any number of conforming
    recordings.

    `deps` are verilog_library targets providing the packages the spec's port
    types name. They have to be elaborated before the interposer, so a spec that
    uses a package type without listing it here fails with the type reported as
    undeclared.
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
        deps = ["@cvm//:replay_sv"] + (deps or []),
        visibility = visibility,
    )
