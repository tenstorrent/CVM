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
    # the same either way -- see the invariant test in test/replay.
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

    Two modes, and exactly one of them applies:

      `dut_lib` + `dut` + `clock` derive the port spec from the DUT's own
      Verilog with slang, so it cannot go stale. This is the one to use.

      `srcs` takes a hand-written spec instead, for a DUT slang cannot see or
      for a spec someone wants to keep by hand.

    `clock` and `exclude` are the only human input either way: a cycle-indexed
    recording samples once per cycle, so it cannot say which port is the clock,
    and a port on another clock domain is one no recording describes.

    Emits only SystemVerilog: the port layout is handed to the runtime by the
    generated module's bind calls, so there is no generated C++. The recording is
    not a build input either, so one build replays any number of conforming
    recordings.

    `deps` are verilog_library targets providing the packages the spec's port
    types name. They have to be elaborated before the interposer, so a spec that
    uses a package type without listing it here fails with the type reported as
    undeclared. `dut_lib` is added for you, since it carries its own packages.
    """

    if (srcs == None) == (dut_lib == None):
        fail("replay(%s): give exactly one of `srcs` (a hand-written spec) or " % name +
             "`dut_lib` (the DUT's verilog_library, for slang to read)")

    if dut_lib != None:
        if dut == None or clock == None:
            fail("replay(%s): `dut_lib` needs `dut` and `clock`" % name)
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
        srcs = [name + "_ports.yml"]
        # Also the elaboration order the interposer needs: its port types come
        # from this library's packages.
        deps = (deps or []) + [dut_lib]
    elif dut != None or clock != None or exclude != None or slang_defines != None:
        fail("replay(%s): `dut`, `clock`, `exclude` and `slang_defines` are " % name +
             "for the `dut_lib` mode; a hand-written spec states them itself")

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
