#! /usr/bin/env python3

import os
import sys
import argparse
import yaml
from dataclasses import dataclass
import textwrap
import math
import pathlib
from topology_query import Query
import re

from mako.template import Template
from mako.runtime import Context
from mako import exceptions

@dataclass
class Field:
    name: str
    width: int

    @classmethod
    def load(cls, name, values):
        return cls(name, values['width'])

    def get_c_width(self):
        if self.width <= 8: return 8
        elif self.width <= 16: return 16
        elif self.width <= 32: return 32
        elif self.width <= 64: return 64

        return None

    def get_c_type(self):

        width = self.get_c_width()
        if width:
            return f"std::uint{width}_t"

        return f"std::bitset<{self.width}>"


@dataclass
class Packet:
    name: str
    domain: int
    num: int
    context: bool
    fields: list[Field]

    @classmethod
    def load(cls, name, values):
        # packets always need topology location
        fields = [Field.load("location", { "width" : 32 })]
        fields += [Field.load(name, v) for name,v in values['fields'].items()]
        return cls(name, values.get("domain", None), values.get("num", 1), values.get("context", False), fields)

    def to_c_enum(self):
        return 'MSG_NUMBER_' + self.name

    def to_sv_enum(self):
        return self.to_c_enum()

@dataclass
class Packets:
    name: str
    packets: list[Packet]


    @classmethod
    def load_file(cls, name, filename, topology):
        query = Query(topology)
        sub_matcher = re.compile(r'\$\{(.*)\}')
        def sub_constructor(loader, node):
            value = node.value
            expr = sub_matcher.match(value).group()[2:-1]
            variables = re.findall(r'\w+(?:\.\w+)+', expr)
            for variable in variables:
                pattern = variable.split('.')
                val = query.query(".".join(pattern[:-1]), pattern[-1])
                if type(val) != int:
                  raise Exception(f"attribute {variable} must be number")
                expr = expr.replace(variable, str(val))
            return eval(expr)

        yaml.add_implicit_resolver('!sub', sub_matcher, None, yaml.SafeLoader)
        yaml.add_constructor('!sub', sub_constructor, yaml.SafeLoader)

        with open(filename, "r") as stream:
            return cls(name, [Packet.load(name, values) for name,values in yaml.safe_load(stream).items()])

    def clog2(self, num):

        return math.ceil(math.log2(num))

    def enum_width(self):

        c = self.clog2(len(self.packets))
        if c == 0:
            c = 1
        return c

class PacketsGen:

    def __init__(self, packets):

        self.template_dir_path = pathlib.Path(os.path.abspath(__file__)).parent / 'templates'
        self.packets = packets

    def gen(self, which, buf, **data):

        ctx = Context(buf, packets=self.packets, **data)
        template = Template(filename = str(self.template_dir_path / ("template." + which)))
        try:
            template.render_context(ctx)
        except:
            raise Exception(exceptions.text_error_template().render()) from None



if __name__ == "__main__":

    parser = argparse.ArgumentParser()
    parser.add_argument("--definition", help="yml file containing packet definitions", required=True)
    parser.add_argument("--incdir"    , help="Path to strip off when generating #include", required=True)
    parser.add_argument("--name"      , help="name of output package and namespace", required=True)
    parser.add_argument("--hpp", help="name of generated hpp", required=True)
    parser.add_argument("--cpp", help="name of generated cpp", required=True)
    parser.add_argument("--sv" , help="name of generated sv", required=True)
    parser.add_argument("--topology", help="name of topology json", required=True)

    args = parser.parse_args()

    p = Packets.load_file(args.name, args.definition, args.topology)
    g = PacketsGen(p)

    for t in ['hpp', 'cpp', 'sv']:
        with open(getattr(args, t), 'w') as f:
            g.gen(t, f, hpp = args.hpp, sv = args.sv, incdir = args.incdir)
