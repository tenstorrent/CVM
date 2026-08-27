#!/usr/bin/python3
# SPDX-FileCopyrightText: © 2026 Tenstorrent USA, Inc.
# SPDX-License-Identifier: Apache-2.0


import os
import yaml

import argparse
import pathlib
import fileinput
import json
import re
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

  def _instance_groups(self, location):
    """Per-instance group index, the group's own instance count, and the
    location total, matching how the runtime tables are populated."""
    if location.is_array:
      groups = [g for g, sh in enumerate(location.shards) for _ in range(sh)]
      return groups, location.shards, sum(location.shards)
    return [0] * len(location.instances), [location.shard], len(location.instances)

  def locations_by_type(self):
    items = {t.upper(): [] for t in self.types}
    for location in self.locations:
      for instance in location.instances:
        for typ in location.types:
          items[typ.upper()].append(instance.loc)
    return items

  def locations_by_hierarchy(self):
    # setdefault mirrors str_hierarchy.insert(), which leaves an existing key
    # untouched rather than overwriting it.
    items = dict()
    for location in self.locations:
      locs = [instance.loc for instance in location.instances]
      items.setdefault(location.path, locs)
      if location.is_array:
        start = 0
        for group, shard in enumerate(location.shards):
          items.setdefault(f"{location.path}[{group}]", locs[start:start + shard])
          start += shard
    return items

  def _attributes_by_location(self, wanted):
    items = dict()
    for location in self.locations:
      if not location.attributes:
        continue
      groups, shards, total = self._instance_groups(location)
      for index, instance in enumerate(location.instances):
        group = groups[index]
        declared = location.attributes[group] if location.is_array else location.attributes
        entries = {name.upper(): value for (name, value) in declared if type(value) is wanted}
        if wanted is int:
          entries = {"SHARD": shards[group], "TOTAL": total, **entries}
        if entries:
          items[instance.loc] = entries
    return items

  def attributes_by_location(self):
    return self._attributes_by_location(int)

  def list_attributes_by_location(self):
    return self._attributes_by_location(list)

  def names_by_location(self):
    return {instance.loc: location.name.upper()
            for location in self.locations for instance in location.instances}

  def constexpr_tables(self):
    """Flatten the lookup tables into sorted, index-plus-pool form so the
    generated header can resolve them with lower_bound rather than a linear
    chain of comparisons over every key."""

    def flatten(mapping):
      pool, index = list(), list()
      for key in sorted(mapping):
        values = mapping[key]
        index.append((key, len(pool), len(values)))
        pool.extend(values)
      return pool, index

    type_pool, type_index = flatten(self.locations_by_type())
    hierarchy_pool, hierarchy_index = flatten(self.locations_by_hierarchy())

    attributes = [(loc, key, value)
                  for loc, entries in self.attributes_by_location().items()
                  for key, value in sorted(entries.items())]
    attributes.sort(key=lambda e: (e[0], e[1]))

    list_pool, list_index = list(), list()
    for loc, entries in sorted(self.list_attributes_by_location().items()):
      for key, values in sorted(entries.items()):
        list_index.append((loc, key, len(list_pool), len(values)))
        list_pool.extend(values)

    names = self.names_by_location()
    name_table = [names.get(loc, "") for loc in range(max(names, default=0) + 1)]

    return {
      "type_pool": type_pool,
      "type_index": type_index,
      "hierarchy_pool": hierarchy_pool,
      "hierarchy_index": hierarchy_index,
      "attributes": attributes,
      "list_pool": list_pool,
      "list_index": list_index,
      "names": name_table,
    }

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

    self.topology = Topology.load(self.root)
    self.resolve_references()

  CONNECTION_REF = re.compile(
    r'^([A-Za-z0-9_.]+)\[(\d+)\](?:\.([A-Za-z_][A-Za-z0-9_]*))?$')

  def resolve_references(self):
    # Precompute, per hierarchy path, its ordered instance locs, and per loc its
    # attribute map exactly as the runtime exposes it (SHARD/TOTAL + declared
    # attrs, picking the right per-group attrs for array locations). A reference
    # then resolves to either the loc (.ID) or one of these attribute values.
    path_to_locs = {}
    attrs_by_loc = {}
    for loc in self.topology.locations:
      path_to_locs[loc.path] = [inst.loc for inst in loc.instances]
      groups = ([g for g, sh in enumerate(loc.shards) for _ in range(sh)]
                if loc.is_array else [0] * len(loc.instances))
      total = sum(loc.shards) if loc.is_array else len(loc.instances)
      for i, inst in enumerate(loc.instances):
        group = loc.attributes[groups[i]] if loc.is_array else loc.attributes
        attrs_by_loc[inst.loc] = {"SHARD": loc.shards[groups[i]], "TOTAL": total,
                                  **{a.name.upper(): a.value for a in group}}

    def parse_ref(ref):
      """'@PATH[idx].FIELD' -> (path, idx, field); idx defaults to 0 and field to
      'ID'. The bracket-less form is all dot-separated, so PATH/FIELD is split by
      longest-matching hierarchy (whole string preferred, else peel last seg)."""
      body = ref[1:]  # drop leading '@'
      m = TopologyGen.CONNECTION_REF.match(body)
      if m:
        return m.group(1).upper(), int(m.group(2)), (m.group(3) or "ID").upper()
      dotted = body.upper()
      if dotted in path_to_locs:
        return dotted, 0, "ID"
      path, _, field = dotted.rpartition(".")
      return path, 0, field

    def resolve(value):
      if not isinstance(value, str) or not value.strip().startswith("@"):
        return value
      path, idx, field = parse_ref(value.strip())
      locs = path_to_locs.get(path, [])
      if idx >= len(locs):
        raise RuntimeError(f"unresolvable topology connection reference '{value}'")
      loc = locs[idx]
      if field == "ID":
        return loc
      if field not in attrs_by_loc[loc]:
        raise RuntimeError(
          f"topology connection reference '{value}': no attribute '{field}'")
      return resolve(attrs_by_loc[loc][field])

    for loc in self.topology.locations:
      for group in (loc.attributes if loc.is_array else [loc.attributes]):
        for attr in group:
          attr.value = resolve(attr.value)

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
  parser.add_argument("--hpp", help="constexpr header to generate", required=True)
  parser.add_argument("--sv", help="sv file to generate", required=True)
  parser.add_argument("--json", help="populate json to be parsed by topology_query library", required=True)
  parser.add_argument("--merged", help="hierarchical topology to generate", required=True)

  args = parser.parse_args()

  # parse yaml for topology structure
  p = TopologyGen(args.definitions, args.merged)
  # generate SV and C++ headers
  for typ in ["cpp", "hpp", "sv", "json"]:
    with open(getattr(args, typ), 'w') as f:
      p.generate(f, typ)
