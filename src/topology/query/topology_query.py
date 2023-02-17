#!/bin/python3

# library to query topology information

import json

class QueryError(Exception):
  pass

class Query:

  def __init__(self, json_path: str):
    f = open(json_path, 'r')
    self.info = json.load(f)

  def module_exists(self, module: str):
    return module in self.info

  def num_instances(self, module: str):
    try:
      return self.info[module]["count"]
    except:
      raise QueryException

  def query(self, module: str, attr: str):
    try:
      return self.info[module][attr]
    except:
      raise QueryException
