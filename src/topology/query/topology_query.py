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

  def query(self, module: str, attr: str, index: int = None):
    try:
      module_data = self.info[module]
      if isinstance(module_data, list):
        return module_data[0][attr] if index is None else module_data[index][attr]
      else:
        return module_data[attr]
    except:
      raise RuntimeError(f"Undefined module or attr: {module}, {attr}, index={index}")

  class Hierarchy:
    def __getitem__(self, index):
      if hasattr(self, '_array') and isinstance(self._array, list):
        if index < len(self._array):
          h = Query.Hierarchy()
          for k, v in self._array[index].items():
            setattr(h, k, v)
          return h
        else:
          raise IndexError(f"Index {index} out of range for array of size {len(self._array)}")
      else:
        raise TypeError(f"'{self.__class__.__name__}' object is not subscriptable (not an array)")

  def hierarchy(self):

    base = Query.Hierarchy()

    for k,v in self.info.items():
      h = base
      for p in k.split('.'):
        if not hasattr(h, p):
          setattr(h, p, Query.Hierarchy())
        h = getattr(h, p)
        if isinstance(v, dict):
          for k2, v2 in v.items():
            setattr(h, k2, v2)
        elif isinstance(v, list):
          setattr(h, '_array', v)
          if v and isinstance(v[0], dict):
            for k2, v2 in v[0].items():
              setattr(h, k2, v2)

    return base
