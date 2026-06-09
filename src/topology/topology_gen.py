#!/usr/bin/python3

import os
import yaml

import argparse
import pathlib
import fileinput
import json
from dataclasses import dataclass
from anytree import Node, RenderTree, AsciiStyle, LevelOrderIter, Walker

from mako.template import Template
from mako.runtime import Context
from mako import exceptions

@dataclass
class Instance:
  instance_id: int
  # each instance has a unique ID
  loc: int

@dataclass
class Attribute:
  name: str
  value: str

  def __iter__(self):
    return iter((self.name, self.value))

@dataclass
class Location:
  name: str
  types: list[str]
  # each "path" has a unique ID
  path_id: int
  path: str
  children: list[str]
  # number of array slots / groups at this node (len(attributes) when is_array,
  # else the location's local count). NOT the number of physical instances —
  # see `instances` (length = sum(shards)) for that.
  shard: int
  # one Instance per physical instance; length = sum(shards) for is_array.
  instances: list[Instance]
  attributes: list()
  # per-slot physical-instance counts. For is_array, parallel to attributes.
  # For non-array, [shard]. sum(shards) == len(instances).
  shards: list = None

  @property
  def is_array(self):
    """Check if attributes are per-instance (array) or shared."""
    return (isinstance(self.attributes, list) and
            len(self.attributes) > 0 and
            isinstance(self.attributes[0], list))

@dataclass
class Topology:
  locations: list[Location]
  types: list[str]

  def location(self, name: str):
    for location in self.locations:
      if location.name == name:
        return location
    raise RuntimeError(f"Could not find location corresponding to {name}")

  @classmethod
  def load(cls, root):
    locations = list()
    types = list()
    loc_id = 1
    path_id = 0
    w = Walker()

    # null == 0, top (root) == 1
    locations.append(Location("top", ["top"], path_id, "TOP", [child.name for child in root.children], 1, [Instance(0, 1)], attributes=list(), shards=[1]))
    types.append("top")

    for node in LevelOrderIter(root, filter_=lambda n: n.name not in ('top')):
      instances = list()

      # continuously increasing id's across instances,
      path_id = loc_id + 1
      assert node.count != 0 and f"Node {node.name} specified with count of 0"
      for i in range(0, node.count):
        loc_id += 1
        instances.append(Instance(i, loc_id))

      path = "TOP"
      walk = list(list(w.walk(root, node))[2])
      for parent in walk:
        path += "." + parent.name.upper()

      for typ in node.types:
        if typ not in types:
          types.append(typ)

      locations.append(Location(node.name, node.types, path_id, path, [child.name for child in node.children], node.shard, instances, node.attributes, shards=node.shards))
    return cls(locations, types)

class TopologyGen:

  root = Node("top")
  names = []

  def __init__(self, definitions: list[str], merged: str):
    # cat >>
    with open(merged, 'wb') as ostream:
      for defin in definitions:
        with open(defin, 'rb') as istream:
          ostream.write(istream.read() + b'\n')

    with open(merged, 'r') as istream:
      try:
        topology = yaml.safe_load(istream)

        # delete all nodes not under top (anchors)
        keys_to_delete = []
        found_top = False
        for k in topology:
          if k != "top":
            keys_to_delete += [k]
          else:
            found_top = True
        for k in keys_to_delete:
          del topology[k]

        if not found_top:
          raise RuntimeError("Expecting top to be defined in topology")

        for key, val in topology["top"].items():
          self.recurse(key, val, self.root, uppers=1)
        # print(RenderTree(self.root, style=AsciiStyle()).by_attr())
      except yaml.YAMLError as exc:
        print(exc)
        raise Exception("Failed to parse merged topology file")

    # generate topology class
    self.topology = Topology.load(self.root)

  def _process_attributes(self, children):
    """Return (attributes, shards). For an attrs-list, each entry is a group
    and its per-entry `count` field (default 1) is its physical-instance count;
    the `count` key is stripped from the emitted attrs. For a scalar/dict attrs
    or no attrs, shards is None (caller fills it from the location's count)."""

    if "attrs" not in children or children["attrs"] is None:
      return list(), None

    attrs_value = children["attrs"]

    if isinstance(attrs_value, list):
      attrs_list = []
      shards = []
      for item in attrs_value:
        if isinstance(item, dict):
          group_count = item.get("count", 1)
          if not isinstance(group_count, int) or group_count < 1:
            raise RuntimeError(
              f"attrs entry count must be a positive int, got {group_count!r}")
          shards.append(group_count)
          attrs_list.append(list(
            Attribute(key, val) for key, val in item.items() if key != "count"))
        else:
          shards.append(1)
          attrs_list.append([])
      return attrs_list, shards

    if isinstance(attrs_value, dict):
      return list(Attribute(key, val) for key, val in attrs_value.items()), None

    return list(), None

  def recurse(self, name, children, parent, uppers):
    if not isinstance(children, dict):
      return

    if "type" not in children:
      raise RuntimeError(f"Must specify a `type` for each node in topology. Faulting node is {name} with children {children}")

    attributes, shards = self._process_attributes(children)

    # `shard` is the number of array slots / groups at this node:
    #   - list-attrs: one slot per yaml entry  (len(attrs))
    #   - else      : `count:` from yaml (or 1)
    # `total_insts` is the number of physical instances per parent:
    #   - list-attrs: sum of per-entry counts
    #   - else      : shard
    if shards is not None:
      shard = len(attributes)
      total_insts = sum(shards)
    else:
      shard = children.get("count", 1)
      total_insts = shard
      shards = [shard]   # one anonymous group of size `shard`

    count = total_insts * uppers
    children["count"] = shard

    new = Node(name, parent=parent,
               shard=shard,
               count=count,
               shards=shards,
               types=children["type"],
               attributes=attributes)

    for key, val in children.items():
      if key == "instances":
        raise RuntimeError("Reserved keyword: instances")
      if key not in ["count", "type", "attrs"] and isinstance(val, dict):
        self.recurse(key, val, new, count)

  def generate(self, buf, which):
    template_dir_path = pathlib.Path(os.path.abspath(__file__)).parent/'templates'
    ctx = Context(buf, topo=self.topology)
    template = Template(filename = str(template_dir_path / ("template." + which)))
    try:
      template.render_context(ctx)
    except:
      raise Exception(exceptions.text_error_template().render()) from None


if __name__ == "__main__":
  parser = argparse.ArgumentParser()
  parser.add_argument("--definitions", nargs='+', help="yml files describing topology", required=True)
  parser.add_argument("--cpp", help="cpp file to generate", required=True)
  parser.add_argument("--sv", help="sv file to generate", required=True)
  parser.add_argument("--json", help="populate json to be parsed by topology_query library", required=True)
  parser.add_argument("--merged", help="hierarchical topology to generate", required=True)

  args = parser.parse_args()

  # parse yaml for topology structure
  p = TopologyGen(args.definitions, args.merged)
  # generate SV and C++ headers
  for typ in ["cpp", "sv", "json"]:
    with open(getattr(args, typ), 'w') as f:
      p.generate(f, typ)
