#! /usr/bin/env python3

import os
import sys
import argparse
import yaml
from dataclasses import dataclass
import textwrap
import math
import pathlib

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

        raise Exception("Can't handle width " + str(self.width))

@dataclass
class Packet:
    name: str
    domain: int
    num: int
    fields: list[Field]

    @classmethod
    def load(cls, name, values):
        fields = [Field.load(name, v) for name,v in values['fields'].items()]
        return cls(name, values.get("domain", None), values.get("num", 1), fields)

    def to_c_enum(self):
        return 'MSG_NUMBER_' + self.name

    def to_sv_enum(self):
        return self.to_c_enum()

@dataclass
class Packets:
    name: str
    packets: list[Packet]


    @classmethod
    def load_file(cls, name, filename):
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
    parser.add_argument("--name"  , help="name of output package and namespace", required=True)
    parser.add_argument("--hpp", help="name of generated hpp", required=True)
    parser.add_argument("--cpp", help="name of generated cpp", required=True)
    parser.add_argument("--sv" , help="name of generated sv", required=True)

    args = parser.parse_args()

    p = Packets.load_file(args.name, args.definition)
    g = PacketsGen(p)

    for t in ['hpp', 'cpp', 'sv']:
        with open(getattr(args, t), 'w') as f:
            g.gen(t, f, hpp = args.hpp, sv = args.sv)
