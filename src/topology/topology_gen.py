#!/usr/bin/python3

import os
import yaml
import argparse
import pathlib
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
class Location:
  name: str
  instances: list[Instance]

@dataclass
class Topology:
  locations: list[Location]

  @classmethod
  def load(cls, root):
    locations = list()
    loc_id = 1
    for node in LevelOrderIter(root, filter_=lambda n: n.name not in ('top')):
      instances = list()
      # continuously increasing id's across instances, fix later?
      for i in range(0, node.count):
        instances.append(Instance(i, loc_id))
        loc_id += 1
      locations.append(Location(node.name, instances))
    return cls(locations)

class TopologyGen:

  root = Node("top")

  def __init__(self, description: str):
    with open(description, 'r') as stream:
      try:
        topology = yaml.safe_load(stream)
        for key, val in topology.items():
          self.recurse(key, val, self.root, 1)
        # print(RenderTree(self.root, style=AsciiStyle()).by_attr())
      except yaml.YAMLError as exc:
        print(exc)

    # generate topology class
    self.topology = Topology.load(self.root)

  def recurse(self, name, children, parent, uppers):
    count = children["count"]*uppers
    new = Node(name, parent=parent, count=count)

    for key, val in children.items():
      if key != "count":
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
  parser.add_argument("--description", help="yml file describing topology", required=True)
  parser.add_argument("--cpp", help="cpp file to generate")
  parser.add_argument("--sv", help="sv file to generate")

  args = parser.parse_args()

  # parse yaml for topology structure
  p = TopologyGen(args.description)
  # generate SV and C++ headers
  for typ in ["cpp", "sv"]:
    with open(getattr(args, typ), 'w') as f:
      p.generate(f, typ)
