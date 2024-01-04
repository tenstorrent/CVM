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
import io
from collections import OrderedDict

from mako.template import Template
from mako.runtime import Context
from mako import exceptions

@dataclass
class Field:
    name: str
    width: int
    qualify: str

    @classmethod
    def load(cls, name, values, subidx = 0):
        width = values.get('widths', [values.get('width')])[subidx]
        qualify = values.get('qualify')
        return cls(name, width, qualify)

    def get_c_type(self):

        w = self.width
        if w <= 8: return "std::uint8_t"
        elif w <= 16: return "std::uint16_t"
        elif w <= 32: return "std::uint32_t"
        elif w <= 64: return "std::uint64_t"
        return f"std::bitset<{w}>"


@dataclass
class Packet:
    name: str
    domain: int
    num: int
    context: bool
    port: str
    fields: list[Field]
    subidx: int

    @classmethod
    def load(cls, name, values, port):
        num_subpackets = len(next(iter(values['fields'].values())).get('widths', [0]))
        assert all(len(v.get('widths', [0])) == num_subpackets for v in values['fields'].values()) and "Need same number of widths for all fields"
        # packets always need topology location
        elaborated = []
        for i in range(num_subpackets):
            p = [Field.load("location", {"width": 32})]
            p += [Field.load(name, v, i) for name,v in values['fields'].items()]
            quals = len(set(field.qualify for field in p if field.qualify is not None))
            if quals:
                p.insert(0, Field.load("_packet_gen_valid", {"width": quals}))

            elaborated.append(p)
        return [cls(name, values.get("domain", None), values.get("num", 1), values.get("context", False), port, e, i) for i, e in enumerate(elaborated)]

    def to_c_enum(self):
        return 'MSG_NUMBER_' + self.port + '_' + self.name + '_' + str(self.subidx)

    def to_sv_enum(self):
        return self.to_c_enum()

    def valid_groups(self):
        d = OrderedDict()
        lsb = 0
        for field in self.fields:
            if field.qualify:
                if field.qualify in d:
                    if d[field.qualify][1] != lsb-1:
                        raise Exception(f"Fields with the same qualify should be contiguous, field '{field.name}' with qualification '{field.qualify}' not contiguous from previous field with same qualification ending at bit {d[field.qualify][1]}")
                else:
                    d[field.qualify] = [lsb, lsb-1]
                d[field.qualify][1] += field.width
            lsb += field.width
        return d

    def valid_groups_bytes(self, padding):
        valid_groups = self.valid_groups()
        bits = set([sum(field.width for field in self.fields) + padding])
        for lsb,msb in valid_groups.values():
            w = msb - lsb + 1
            bits |= set(s - w for s in bits)
        return sorted(list(set((b + 7)//8 for b in bits)))

@dataclass
class Packets:
    name: str
    domains: dict[int, dict[str, str]] # only contains domains that have special attrs
    packets: list[list[Packet]]
    ports: dict[str, list[int]]

    @classmethod
    def load_file(cls, name, filename, topology):
        query = Query(topology)
        hierarchy = query.hierarchy()

        packets = []
        ports = dict()
        domains = dict()

        rendered = io.StringIO()
        ctx = Context(rendered, **{k: getattr(hierarchy, k) for k in dir(hierarchy) if not k.startswith('__')})
        template = Template(filename = filename)
        try:
            template.render_context(ctx)
        except:
            raise Exception(exceptions.text_error_template().render()) from None

        for port, values in yaml.safe_load(rendered.getvalue()).items():

            if port.startswith("__"): # special fields
                f = port[2:]
                if f == "domains":
                    domains = values
                else:
                    raise Exception(f"Unrecognized key {port}. Names starting with '__' are not recognized as ports")
                continue


            #FIXME: can't think of a good fix for this, there would need to be ifdefs in SV/C++ for code that depends on a packet to be generated
            # even though it might not be needed
            num = values.get("num", [0])
            if isinstance(num, int):
                ports[port] = [num]
            else:
                ports[port] = num

            for packet_name, packet_values in values.items():
                if packet_name != "num": # ignore "num"
                    p = Packet.load(packet_name, packet_values, port)
                    assert len(p) == len(ports[port]) and "Must specify same number of widths among all packets in a port"
                    packets.append(p)
        return cls(name, domains, packets, ports)

    def clog2(self, num):

        return math.ceil(math.log2(num))

    def total_packets(self):

        sum = 0
        for packet in self.packets:
            sum += len(packet)

        return sum

    def enum_width(self):

        c = self.clog2(self.total_packets())
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
