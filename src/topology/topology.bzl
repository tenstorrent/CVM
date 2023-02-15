load("@rules_hdl//verilog:providers.bzl", "verilog_library")

def _topology_gen_impl(ctx):
  name = ctx.attr.name

  cpp = ctx.outputs.cpp
  sv = ctx.outputs.sv

  args = ctx.actions.args()
  args.add("--description", ctx.file.src)
  args.add("--cpp", cpp)
  args.add("--sv", sv)

  outputs = [cpp, sv]

  ctx.actions.run(
      arguments = [args],
      executable = ctx.executable._gen,
      inputs = ctx.files.src,
      outputs = outputs,
  )

  return [
      DefaultInfo(
          files = depset(outputs,)
      ),
  ]

_topology_gen = rule(
  implementation = _topology_gen_impl,
  attrs = {
      "src" : attr.label(
        mandatory = True,
        allow_single_file = [".yml"],
      ),
      "cpp" : attr.output(
      ),
      "sv" : attr.output(
      ),
      "_gen": attr.label(
        default = "//src/topology:topology_gen",
        executable = True,
        cfg = "exec",
      ),
  },
  provides = [
      DefaultInfo,
  ],
)

def topology_gen(name, visibility = None, cc_attrs = {}, **kwargs):

  cpp = name + ".cpp"
  sv = name + ".sv"

  _topology_gen(
      name = name,
      cpp = cpp,
      sv = sv,
      visibility = visibility,
      **kwargs,
  )

  native.cc_library(
      name = name + '_cc',
      srcs = [cpp],
      deps = [
        "@cvm//src/topology:topology"
      ],
      visibility = visibility,
      **cc_attrs,
  )

  verilog_library(
      name = name + '_sv',
      srcs = [sv],
  )

