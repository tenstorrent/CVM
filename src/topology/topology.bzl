load("@rules_hdl//verilog:providers.bzl", "verilog_library")

def _topology_gen_impl(ctx):
  name = ctx.attr.name

  cpp = ctx.outputs.cpp
  sv = ctx.outputs.sv
  json = ctx.outputs.json
  merged = ctx.outputs.merged

  args = ctx.actions.args()
  args.add_all("--definitions", ctx.files.srcs)
  args.add("--cpp", cpp)
  args.add("--sv", sv)
  args.add("--json", json)
  args.add("--merged", merged)

  outputs = [cpp, sv, json, merged]

  ctx.actions.run(
      arguments = [args],
      executable = ctx.executable._gen,
      inputs = ctx.files.srcs,
      outputs = outputs,
      mnemonic = "CVMTopologyGen"
  )

  return [
      DefaultInfo(
          files = depset(outputs,)
      ),
  ]

_topology_gen = rule(
  implementation = _topology_gen_impl,
  attrs = {
      "srcs" : attr.label_list(
        mandatory = True,
        allow_files = True,
      ),
      "cpp" : attr.output(
      ),
      "sv" : attr.output(
      ),
      "json" : attr.output(
      ),
      "merged" : attr.output(
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
  json = name + ".json"
  merged = name + "_merged.yml"

  _topology_gen(
      name = name,
      cpp = cpp,
      sv = sv,
      json = json,
      merged = merged,
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
      deps = [
        "@cvm//:topology_sv"
      ],
  )

  native.filegroup(
      name = name + '_json',
      srcs = [json],
  )

