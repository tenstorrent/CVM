load("//src/replay:replay.bzl", _replay = "replay", _replay_bind = "replay_bind", _replay_ports = "replay_ports")
load("//src/packet_gen:packet_gen.bzl", _packet_gen = "packet_gen")
load("//src/topology:topology.bzl", _topology_gen = "topology_gen")

replay = _replay
replay_bind = _replay_bind
replay_ports = _replay_ports
packet_gen = _packet_gen
topology_gen = _topology_gen
