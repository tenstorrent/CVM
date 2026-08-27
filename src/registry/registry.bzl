def registry_gen(name, topology, visibility = None, cc_attrs = {}, **kwargs):
  """Registry bound to one topology.

  Depending on this target instead of @cvm//:registry puts that topology's
  generated cvm/topology_defs.hpp on the include path, which is what the
  compile-time registration macros expand against. Emitted automatically by
  topology_gen as <topology>_registry.
  """

  native.cc_library(
      name = name,
      deps = [
        "@cvm//:registry",
        topology + "_cc",
      ],
      visibility = visibility,
      **cc_attrs,
  )
