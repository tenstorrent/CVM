load("//src/replay:replay.bzl", _replay = "replay", _replay_force = "replay_force", _replay_ports = "replay_ports")
load("//src/packet_gen:packet_gen.bzl", _packet_gen = "packet_gen")
load("//src/topology:topology.bzl", _topology_gen = "topology_gen")

replay = _replay
replay_force = _replay_force
replay_ports = _replay_ports
packet_gen = _packet_gen
topology_gen = _topology_gen
