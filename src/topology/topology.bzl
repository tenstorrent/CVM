load("@rules_hdl//verilog:providers.bzl", "verilog_library")
load("@cvm//src/registry:registry.bzl", "registry_gen")

def _topology_gen_impl(ctx):
  name = ctx.attr.name

  cpp = ctx.outputs.cpp
  hpp = ctx.outputs.hpp
  sv = ctx.outputs.sv
  json = ctx.outputs.json
  merged = ctx.outputs.merged

  args = ctx.actions.args()
  args.add_all("--definitions", ctx.files.srcs)
  args.add("--cpp", cpp)
  args.add("--hpp", hpp)
  args.add("--sv", sv)
  args.add("--json", json)
  args.add("--merged", merged)

  outputs = [cpp, hpp, sv, json, merged]

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
      "hpp" : attr.output(
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
  # Fixed basename under a per-target directory so consumers include a stable
  # "cvm/topology_defs.hpp" regardless of which topology they are built against.
  hpp = name + "/topology_defs.hpp"
  sv = name + ".sv"
  json = name + ".json"
  merged = name + "_merged.yml"

  _topology_gen(
      name = name,
      cpp = cpp,
      hpp = hpp,
      sv = sv,
      json = json,
      merged = merged,
      visibility = visibility,
      **kwargs,
  )

  native.cc_library(
      name = name + '_cc',
      srcs = [cpp],
      hdrs = [hpp],
      deps = [
        "@cvm//src/topology:topology"
      ],
      strip_include_prefix = name,
      include_prefix = "cvm",
      visibility = visibility,
      **cc_attrs,
  )

  registry_gen(
      name = name + '_registry',
      topology = name,
      visibility = visibility,
      cc_attrs = cc_attrs,
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

