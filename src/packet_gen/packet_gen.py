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
from collections import OrderedDict, defaultdict
from functools import reduce

from mako.template import Template
from mako.runtime import Context
from mako import exceptions

@dataclass
class Field:
    name: str
    width: list[int, ...]
    qualify: str

    @classmethod
    def load(cls, name, values, variant = 0):
        width = values.get('widths', values.get('width'))
        if not isinstance(width, list):
            width = [(width,)]
        width = width[variant]
        if isinstance(width, int):
            width = [width]
        qualify = values.get('qualify')
        return cls(name, width, qualify)

    def get_c_type(self):
        def get_word(w):
            if w <= 8: return "std::uint8_t"
            elif w <= 16: return "std::uint16_t"
            elif w <= 32: return "std::uint32_t"
            elif w <= 64: return "std::uint64_t"
            return f"std::bitset<{w}>"

        def get_type(w):
            return get_word(w[0]) if len(w) == 1 else f"std::array<{get_type(w[1:])}, {w[0]}>"

        w = self.width
        return get_type(w)

    def get_sv_type(self):
        def get_type(w):
            return f"[{w[0] - 1}:0]" if len(w) == 1 else f"[{w[0] - 1}:0]{get_type(w[1:])}"

        w = self.width
        return f"logic {get_type(w)}"

    def total_width(self):
        return reduce(lambda w0, w1: w0*w1, self.width)


@dataclass
class Packet:
    name: str
    domain: int
    priority: str
    num: int
    context: bool
    port: str
    fields: list[Field]
    variant_id: int

    @classmethod
    def load(cls, name, values, port):
        assert all(k in Packet.keywords() for k in values.keys())

        if 'fields' in values:
            num_variants = len(next(iter(values['fields'].values())).get('widths', [0]))
            assert all(len(v.get('widths', [0])) == num_variants for v in values['fields'].values()) and "Need same number of widths (variants) for all fields"
        else:
            num_variants = 1

        # packets always need topology location
        elaborated = []
        for i in range(num_variants):
            p = [Field.load("location", {"width": PacketStore.location_width()})]
            p += [Field.load(name, v, i) for name,v in values.get('fields', dict()).items()]
            quals = len(set(field.qualify for field in p if field.qualify is not None))
            if quals:
                p.insert(0, Field.load("_packet_gen_valid", {"width": quals}))

            elaborated.append(p)

        domain = values.get("domain", None)
        if isinstance(domain, int) or domain is None:
            all_domains = [domain]
        else:
            all_domains = domain
        assert len(all_domains) == num_variants and "Need same number of domains as variants"

        return [cls(name, all_domains[i], values.get("priority", None), values.get("num", 1), values.get("context", False), port, e, i) for i, e in enumerate(elaborated)]

    def to_c_enum(self):
        return 'MSG_NUMBER_' + self.port + '_' + self.name + '_' + str(self.variant_id)

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
                d[field.qualify][1] += field.total_width()
            lsb += field.total_width()
        return d

    # This is the sweet spot on zebu ep1 fwc gen2
    VALID_GROUP_BUCKET_SIZE_BYTES = 48

    def valid_groups_words(self, padding):
        valid_groups = self.valid_groups()
        bits = set([sum(field.total_width() for field in self.fields) + padding])
        for lsb,msb in valid_groups.values():
            w = msb - lsb + 1
            bits |= set(s - w for s in bits)

        buckets = defaultdict(set)

        bucket_size_words = (Packet.VALID_GROUP_BUCKET_SIZE_BYTES + PacketStore.transfer_word_bytes() - 1) // PacketStore.transfer_word_bytes()
        bucket_size_bits  = bucket_size_words * PacketStore.transfer_word_bits()

        for n in bits:
            buckets[(n + bucket_size_bits - 1)//(bucket_size_bits)].add((n + PacketStore.transfer_word_bits() - 1)//PacketStore.transfer_word_bits())

        return OrderedDict(
            sorted(
                [[max(b), sorted(list(b))] for b in buckets.values()],
                key = lambda x: x[0]
            ),
        )

    @staticmethod
    def keywords():
        return ["domain", "num", "fields", "priority", "context"]


@dataclass
class PacketStore:
    name: str
    domains: dict[int, dict[str, str]] # only contains domains that have special attrs
    packets: list[list[Packet]]
    ports: dict[str, list[int]]
    max_byte_transfer: int # beyond this the transfer is chunked up

    @classmethod
    def load_file(cls, name, filename, topology):
        query = Query(topology)
        hierarchy = query.hierarchy()

        packets = []
        ports = dict()
        domains = dict()
        max_byte_transfer = cls.transfer_word_bytes() * (sys.maxsize//cls.transfer_word_bytes())

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
                elif f == "max_byte_transfer":
                    max_byte_transfer = values
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
        return cls(name, domains, packets, ports, max_byte_transfer)

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

    def chunk_transfer(self, words):
        w = 0
        chunks = []
        max_word_transfer = self.max_byte_transfer // self.transfer_word_bytes()
        if self.max_byte_transfer % self.transfer_word_bytes():
            raise Exception(f"max_byte_transfer ({self.max_byte_transfer}) not a multiple of transfer_word_bytes {self.transfer_word_bytes()}")
        while w != words:
            chunk = min(words - w, max_word_transfer)
            chunks.append(chunk)
            w += chunk
        return chunks

    @staticmethod
    # FIXME compress this
    def location_width():
        return 32

    @staticmethod
    def transfer_word_bytes():
        return 4

    @classmethod
    def transfer_word_bits(cls):
        return 8 * cls.transfer_word_bytes()

    @staticmethod
    def transfer_word_sv_type():
        return "int unsigned"

    @staticmethod
    def transfer_word_c_type():
        return "std::uint32_t"


class PacketsGen:

    def __init__(self, packet_store):

        self.template_dir_path = pathlib.Path(os.path.abspath(__file__)).parent / 'templates'
        self.packet_store = packet_store

    def gen(self, which, buf, **data):

        ctx = Context(buf, packet_store=self.packet_store, **data)
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

    p = PacketStore.load_file(args.name, args.definition, args.topology)
    g = PacketsGen(p)

    for t in ['hpp', 'cpp', 'sv']:
        with open(getattr(args, t), 'w') as f:
            g.gen(t, f, hpp = args.hpp, sv = args.sv, incdir = args.incdir)
