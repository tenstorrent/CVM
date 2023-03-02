#!/usr/bin/python3

import os
import yaml

import argparse
import pathlib
import fileinput
import json
from dataclasses import dataclass
from anytree import Node, RenderTree, AsciiStyle, LevelOrderIter

from mako.template import Template
from mako.runtime import Context
from mako import exceptions

@dataclass
class Instance:
  real_id: int
  loc: int

@dataclass
class Attribute:
  name: str
  value: str

@dataclass
class Location:
  name: str
  instances: list[Instance]
  attrs: list[Attribute]

@dataclass
class Topology:
  locations: list[Location]

  @classmethod
  def load(cls, root, attributes: dict):
    locations = list()
    locations.append(Location("top", [Instance(0, 1)], list()))

    loc_id = 2
    for node in LevelOrderIter(root, filter_=lambda n: n.name not in ('top')):
      instances = list()
      # continuously increasing id's across instances, fix later?
      for i in range(0, node.count):
        instances.append(Instance(i, loc_id))
        loc_id += 1
      locations.append(Location(node.name, instances, attributes[node.name]))
    return cls(locations)

class TopologyGen:

  root = Node("top")
  attributes = {}

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
          self.recurse(key, val, self.root, 1)
        # print(RenderTree(self.root, style=AsciiStyle()).by_attr())
      except yaml.YAMLError as exc:
        print(exc)

    # generate topology class
    self.topology = Topology.load(self.root, self.attributes)

  def recurse(self, name, children, parent, uppers):
    count = children["count"]*uppers
    new = Node(name, parent=parent, count=count)

    if "attrs" in children:
      self.attributes[name] = list(Attribute(key, str(val)) for key, val in children["attrs"].items())
    else:
      self.attributes[name] = list()

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
