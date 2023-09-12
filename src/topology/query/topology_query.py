#!/bin/python3

# library to query topology information

import json

class Query:

  def __init__(self, json_path: str):
    with open(json_path, 'r') as f:
      self.info = json.load(f)

  def module_exists(self, module: str):
    return module in self.info

  def num_instances(self, module: str):
    try:
      return self.info[module]["NUM"]
    except:
      raise RuntimeError(f"Fatal: NUM not defined")

  def query(self, module: str, attr: str):
    try:
      return self.info[module][attr]
    except:
      raise RuntimeError(f"Undefined module or attr: {module}, {attr}")

  class Hierarchy: pass

  def hierarchy(self):

    base = Query.Hierarchy()

    for k,v in self.info.items():
      h = base
      for p in k.split('.'):
        if not hasattr(h, p):
          setattr(h, p, Query.Hierarchy())
        h = getattr(h, p)
        for k2, v2 in v.items():
          setattr(h, k2, v2)

    return base
