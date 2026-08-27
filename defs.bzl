load("//src/packet_gen:packet_gen.bzl", _packet_gen = "packet_gen")
load("//src/topology:topology.bzl", _topology_gen = "topology_gen")
load("//src/registry:registry.bzl", _registry_gen = "registry_gen")

packet_gen = _packet_gen
topology_gen = _topology_gen
registry_gen = _registry_gen
