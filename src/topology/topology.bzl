load("@rules_hdl//verilog:providers.bzl", "verilog_library")
load("@bazel_tools//tools/cpp:toolchain_utils.bzl", "find_cpp_toolchain")

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

  cc_toolchain = find_cpp_toolchain(ctx)
  feature_configuration = cc_common.configure_features(
      ctx = ctx,
      cc_toolchain = cc_toolchain,
      requested_features = ctx.features,
      unsupported_features = ctx.disabled_features,
  )
  dep_cc_infos = [dep[CcInfo] for dep in ctx.attr.deps]
  compilation_context, compilation_outputs = cc_common.compile(
      name = name,
      actions = ctx.actions,
      feature_configuration = feature_configuration,
      cc_toolchain = cc_toolchain,
      srcs = [cpp],
      public_hdrs = [hpp],
      strip_include_prefix = name,
      include_prefix = "cvm",
      compilation_contexts = [info.compilation_context for info in dep_cc_infos],
  )
  linking_context, _ = cc_common.create_linking_context_from_compilation_outputs(
      name = name,
      actions = ctx.actions,
      feature_configuration = feature_configuration,
      cc_toolchain = cc_toolchain,
      compilation_outputs = compilation_outputs,
      linking_contexts = [info.linking_context for info in dep_cc_infos],
      # Matches the alwayslink/linkstatic cc_attrs consumers previously
      # applied to the generated cc_library.
      alwayslink = True,
      disallow_dynamic_library = True,
  )

  return [
      DefaultInfo(
          files = depset(outputs,)
      ),
      OutputGroupInfo(
          cpp = depset([cpp]),
          hpp = depset([hpp]),
          sv = depset([sv]),
          json = depset([json]),
          merged = depset([merged]),
      ),
      CcInfo(
          compilation_context = compilation_context,
          linking_context = linking_context,
      ),
  ]

_topology_gen = rule(
  implementation = _topology_gen_impl,
  attrs = {
      "srcs" : attr.label_list(
        mandatory = True,
        allow_files = True,
      ),
      "deps" : attr.label_list(
        providers = [CcInfo],
        default = [
            "@cvm//src/topology:topology",
            "@cvm//:registry",
        ],
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
      "_cc_toolchain": attr.label(
        default = "@bazel_tools//tools/cpp:current_cc_toolchain",
      ),
  },
  fragments = ["cpp"],
  toolchains = ["@bazel_tools//tools/cpp:toolchain_type"],
  provides = [
      DefaultInfo,
      CcInfo,
  ],
)

def topology_gen(name, visibility = None, cc_attrs = {}, **kwargs):

  cpp = name + ".cpp"
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
