def registry_gen(name, topology, visibility = None, cc_attrs = {}, **kwargs):
  """Compatibility shim for the registry-bound-to-a-topology target.

  The topology target emitted by topology_gen now carries CcInfo for the
  generated tables merged with @cvm//:registry, so depending on the topology
  label directly is sufficient. Kept as an alias so existing deps on
  <topology>_registry keep resolving to the single compiled copy.
  """

  native.alias(
      name = name,
      actual = topology,
      visibility = visibility,
  )
