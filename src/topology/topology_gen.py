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
  loc: int

@dataclass
class Attribute:
  name: str
  value: str

@dataclass
class Location:
  name: str
  path_id: int
  path: str
  children: list[str]
  instances: list[Instance]

@dataclass
class Topology:
  locations: list[Location]
  attrs: dict()

  def location(self, name: str):
    for location in self.locations:
      if location.name == name:
        return location
    raise RuntimeError(f"Could not find location corresponding to {name}")

  @classmethod
  def load(cls, root, attributes: dict):
    locations = list()
    attrs = attributes
    loc_id = 1
    path_id = 0
    w = Walker()

    # null == 0, top (root) == 1
    locations.append(Location("top", path_id, "TOP", [child.name for child in root.children], [Instance(0, 1)]))

    for node in LevelOrderIter(root, filter_=lambda n: n.name not in ('top')):
      instances = list()

      # continuously increasing id's across instances, fix later?
      for i in range(0, node.count):
        loc_id += 1
        instances.append(Instance(i, loc_id))

      path_id += 1
      path = "TOP"
      walk = list(list(w.walk(root, node))[2])
      for parent in walk:
        path += "." + parent.name.strip('~').upper()

      locations.append(Location(node.name, path_id, path, [child.name for child in node.children], instances))
    return cls(locations, attrs)

class TopologyGen:

  root = Node("top")
  attributes = {}
  names = []

  def __init__(self, definitions: list[str], merged: str):
    # cat >>
    with open(merged, 'wb') as ostream:
      for defin in definitions:
        with open(defin, 'rb') as istream:
          ostream.write(istream.read())

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

    # generate topology class
    self.topology = Topology.load(self.root, self.attributes)

  def recurse(self, name, children, parent, uppers):
    count = children["count"]*uppers

    real_name = name
    while real_name in self.attributes:
      real_name += '~'

    new = Node(real_name, parent=parent, count=count)

    if "attrs" in children and '~' in real_name:
      raise RuntimeError(f"Not allowed to redefine attributes for a module: {name}")
    elif "attrs" in children:
      self.attributes[name] = list(Attribute(key, str(val)) for key, val in children["attrs"].items())

    self.names += [real_name]
    for key, val in children.items():
      if key != "count" and key != "attrs":
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
