load("@rules_hdl//verilog:providers.bzl", "verilog_library")
load("@rules_verilog//verilog:defs.bzl", "VerilogInfo")

def _replay_ports_impl(ctx):
    """Derive the port spec from the DUT's own Verilog."""

    ports = ctx.outputs.ports
    info = ctx.attr.dut_lib[VerilogInfo]

    args = ctx.actions.args()
    args.add_all(ctx.files.dut_lib, before_each = "--source")
    # An include a DUT reaches for has to be reachable here too, or a width
    # behind a macro stops resolving.
    args.add_all(info.includes, before_each = "--include-dir")
    args.add("--top", ctx.attr.dut)
    args.add("--clock", ctx.attr.clock)
    args.add_all(ctx.attr.exclude, before_each = "--exclude")
    # Read only so slang can pick a branch to elaborate. The spec must come out
    # the same either way -- see the invariant test in test/replay/spec.
    args.add_all(ctx.attr.defines, before_each = "--define")
    args.add("--name", ctx.attr.spec_name)
    args.add("--out", ports)

    ctx.actions.run(
        arguments = [args],
        executable = ctx.executable._gen,
        inputs = depset(ctx.files.dut_lib),
        outputs = [ports],
        mnemonic = "CVMReplayPorts",
    )

    return [DefaultInfo(files = depset([ports]))]

_replay_ports = rule(
    _replay_ports_impl,
    attrs = {
        "dut_lib": attr.label(
            mandatory = True,
            providers = [VerilogInfo],
        ),
        "dut": attr.string(mandatory = True),
        "clock": attr.string(mandatory = True),
        "exclude": attr.string_list(),
        "defines": attr.string_list(),
        "spec_name": attr.string(mandatory = True),
        "ports": attr.output(),
        "_gen": attr.label(
            default = "//src/replay:replay_ports_gen",
            executable = True,
            cfg = "exec",
        ),
    },
    provides = [DefaultInfo],
)

def _replay_gen_impl(ctx):
    """Run the generator, emitting whichever outputs the caller asked for."""

    args = ctx.actions.args()
    args.add_all("--definitions", ctx.files.srcs)

    outputs = []
    for flag, output in (
        ("--sv", ctx.outputs.sv),
        ("--attach-sv", ctx.outputs.attach_sv),
        ("--merged", ctx.outputs.merged),
    ):
        if output:
            args.add(flag, output)
            outputs.append(output)

    inputs = list(ctx.files.srcs)

    # Optional because it serves only ${...} interpolation of widths and depths
    # in the spec, which a spec using literal widths never needs. It is not the
    # topology replay needs at runtime: the interposer's LOCATION must name a
    # node of type `replay`, which topology_gen provides separately.
    if ctx.file.topology:
        args.add("--topology", ctx.file.topology)
        inputs.append(ctx.file.topology)

    ctx.actions.run(
        arguments = [args],
        executable = ctx.executable._gen,
        inputs = inputs,
        outputs = outputs,
        mnemonic = "CVMReplayGen",
    )

    return [DefaultInfo(files = depset(outputs))]

_replay_gen = rule(
    _replay_gen_impl,
    attrs = {
        "srcs": attr.label_list(
            mandatory = True,
            allow_files = True,
        ),
        "topology": attr.label(
            mandatory = False,
            allow_single_file = [".json"],
        ),
        # `sv` for replay(), `attach_sv` for replay_attach().
        "sv": attr.output(),
        "attach_sv": attr.output(),
        "merged": attr.output(),
        "_gen": attr.label(
            default = "//src/replay:replay_gen",
            executable = True,
            cfg = "exec",
        ),
    },
    provides = [DefaultInfo],
)

def _spec_srcs(kind, name, srcs, dut, dut_lib, clock, exclude, slang_defines,
               deps, visibility):
    """Resolve the two ways a spec arrives; return the srcs and deps to use.

    Either it is handed over as `srcs`, or it is derived from the DUT's own
    Verilog -- and then the DUT's library is a dependency too, because the
    generated module's port types come from its packages.
    """

    if (srcs == None) == (dut_lib == None):
        fail("%s(%s): give exactly one of `srcs` (a hand-written spec) or " % (kind, name) +
             "`dut_lib` (the DUT's verilog_library, for slang to read)")

    if dut_lib == None:
        if dut != None or clock != None or exclude != None or slang_defines != None:
            fail("%s(%s): `dut`, `clock`, `exclude` and `slang_defines` are " % (kind, name) +
                 "for the `dut_lib` mode; a hand-written spec states them itself")
        return srcs, (deps or [])

    if dut == None or clock == None:
        fail("%s(%s): `dut_lib` needs `dut` and `clock`" % (kind, name))

    replay_ports(
        name = name + "_ports",
        dut_lib = dut_lib,
        dut = dut,
        clock = clock,
        exclude = exclude,
        slang_defines = slang_defines,
        spec_name = name,
        visibility = visibility,
    )
    return [name + "_ports.yml"], (deps or []) + [dut_lib]

def replay_attach(
        name,
        srcs = None,
        dut = None,
        dut_lib = None,
        clock = None,
        exclude = None,
        slang_defines = None,
        deps = None,
        visibility = None):
    """Replay a DUT that is lower down in the hierarchy.

    Use `replay()` instead when the block is extracted and replayed on its own.
    There is no hierarchy to point at.

    Emits `<name>.sv`, which has:

      the attach macro, which takes the instance as an argument, reads its
      boundary by hierarchical reference and drives it with `force`.

      the replay module the macro instantiates -- the transport, the host calls
      and the boundary arithmetic. It observes through `_obs` ports and answers
      with `_rep` and `_en`, so it knows nothing about where the DUT is.

    A testbench invokes the macro once per instance. It expands to a named
    generate block, so several invocations coexist. Each site is a separate
    registry component, so each needs its own topology node.

    """

    srcs, deps = _spec_srcs("replay_attach", name, srcs, dut, dut_lib, clock,
                            exclude, slang_defines, deps, visibility)

    _replay_gen(
        name = name,
        srcs = srcs,
        attach_sv = name + ".sv",
        merged = name + "_merged.yml",
        visibility = visibility,
    )

    verilog_library(
        name = name + "_sv",
        srcs = [name + ".sv"],
        deps = ["@cvm//:replay_sv"] + (deps or []),
        visibility = visibility,
    )

def replay_ports(
        name,
        dut,
        dut_lib,
        clock,
        exclude = None,
        slang_defines = None,
        spec_name = None,
        visibility = None):
    """Derive a replay port spec from a DUT's Verilog, and nothing else.

    `replay(dut_lib = ...)` runs this for you. Use it directly to compare two
    readings of the same DUT -- which is how the spec is held to describing the
    module rather than the configuration slang happened to elaborate.

    `spec_name` is the key the spec is written under, and so the module name the
    interposer would take. It defaults to `name`.
    """

    _replay_ports(
        name = name,
        dut_lib = dut_lib,
        dut = dut,
        clock = clock,
        exclude = exclude or [],
        defines = slang_defines or [],
        spec_name = spec_name or name,
        ports = name + ".yml",
        visibility = visibility,
    )

def replay(
        name,
        srcs = None,
        dut = None,
        dut_lib = None,
        clock = None,
        exclude = None,
        slang_defines = None,
        topology = None,
        deps = None,
        visibility = None,
        **kwargs):
    """Generate a replay interposer for a Verilog module.

    Two modes:

      `dut_lib` + `dut` + `clock` derive the port spec from the DUT's own
      Verilog with slang.

      `srcs` takes a hand-written spec

    `deps` are verilog_library targets providing the packages the spec's port
    types name.
    """

    srcs, deps = _spec_srcs("replay", name, srcs, dut, dut_lib, clock,
                            exclude, slang_defines, deps, visibility)

    _replay_gen(
        name = name,
        srcs = srcs,
        sv = name + ".sv",
        merged = name + "_merged.yml",
        topology = topology,
        visibility = visibility,
        **kwargs
    )

    verilog_library(
        name = name + "_sv",
        srcs = [name + ".sv"],
        deps = ["@cvm//:replay_sv"] + (deps or []),
        visibility = visibility,
    )
