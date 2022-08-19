#! /usr/bin/env python3

import argparse
import yaml
from dataclasses import dataclass
import textwrap
import math
import pathlib

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

    def to_c_decl(self):
        return f'std::uint{self.get_c_width()}_t {self.name};'

    def to_sv_decl(self):
        return f'logic[{self.width-1}:0] {self.name};'

    def to_c_constructor(self, start):
        return self.name + '(cvm::bitmanip::array_slice<decltype(' + self.name + ')>' + \
            '(bytes,' + str(start + self.width - 1) + ' + offset, ' + str(start) + ' + offset))'

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

    def to_c_struct(self):
        nl = '\n'
        offset = 0
        return f"""struct {self.name} {{
{textwrap.indent(nl.join(f.to_c_decl() for f in self.fields), ' ' * 4)}
    constexpr {self.name}(const std::uint8_t* bytes, const size_t offset) :
{textwrap.indent((',' + nl).join(
    f.to_c_constructor(offset - f.width) for f in self.fields if (offset := offset + f.width) or True
), ' ' * 8)}
        {{}}
}};"""

    def to_sv_struct(self):
        nl = '\n'
        offset = 0
        return f"""typedef struct packed {{
{textwrap.indent(nl.join(f.to_sv_decl() for f in reversed(self.fields)), ' ' * 4)}
}} {self.name};"""

    def to_cpp_case(self, namespace, offset):

        return f"""case {namespace}::{self.to_c_enum()}: {{
    {namespace}::{self.name} {self.name}(message, {offset});
    cvm::messenger<{namespace}::{self.name}>::signal({self.name});
    break;
}}"""

    def to_c_dpi(self, namespace):

        return f"""extern "C" void {namespace}_message_{self.name}(const std::uint8_t* message) {{
    {namespace}_message(message);
}}
"""

    def to_sv_dpi_import(self, package):
        return  f'import "DPI-C" function void {package}_message_{self.name}(byte unsigned message[($bits(E) + $bits({package}::{self.name}) + 7)/8]);'


    def to_sv_dpi_case_call(self, package):
        return  f'{package}::{self.to_sv_enum()}: {package}_message_{self.name}(o);'

    def to_sv_messenger_inst(self, package):
        return f"""for (genvar i = 0; i < $size(tx.{self.name}s); i++) begin
    {package}_messenger #({package}::{self.name}, {package}::message_number, {package}::{self.to_sv_enum()}) {self.name}_messenger (clk, tx.{self.name}s[i].valid, tx.{self.name}s[i].data);
end"""


@dataclass
class Packets:
    name: str
    packets: list[Packet]


    @classmethod
    def load_file(cls, name, filename):
        with open(filename, "r") as stream:
            return cls(name, [Packet.load(name, values) for name,values in yaml.safe_load(stream).items()])

    def to_hpp(self):

        nl = "\n"

        return f"""#pragma once

#include <cinttypes>
#include "cvm/bitmanip.hpp"

namespace {self.name} {{

    typedef enum {{
{textwrap.indent(("," + nl).join(p.to_c_enum() + ' = ' + str(i) for i,p in enumerate(self.packets)), ' ' * 8)}
    }} message_number;

{textwrap.indent(nl.join(p.to_c_struct() for p in self.packets), ' ' * 4)}

}}"""

    def clog2(self, num):

        return math.ceil(math.log2(num))

    def enum_width(self):

        c = self.clog2(len(self.packets))
        if c == 0:
            c = 1
        return c

    def to_cpp(self, hpp_name):

        nl = "\n"

        enum_width = self.enum_width()

        return f"""#include "{hpp_name}"
#include "cvm/messenger.hpp"
#include <type_traits>

extern "C" void {self.name}_message(const std::uint8_t* message) {{

    {self.name}::message_number message_number = {self.name}::message_number(cvm::bitmanip::array_slice<std::underlying_type<{self.name}::message_number>::type>(message, {enum_width-1}, 0));

    switch(message_number) {{
{textwrap.indent(nl.join(p.to_cpp_case(self.name, enum_width) for p in self.packets), 8 * ' ')}
        default: {{
            assert(0 && "unexpected message number");
            break;
        }}
    }}
}}

{nl.join(p.to_c_dpi(self.name) for p in self.packets)}"""

    def to_sv(self, sv):

        nl = "\n"
        include_guard = '_' + str(pathlib.Path(sv).name).upper().replace('.', '_') + '_'

        by_domain = {}
        for p in self.packets:
            if p.domain is not None:
                if p.domain not in by_domain:
                    by_domain[p.domain] = []
                by_domain[p.domain].append(p)

        domain_structs = []
        for domain in sorted(by_domain.keys()):
            domain_structs.append("typedef struct packed {\n" +
                textwrap.indent(
                    "\n".join(
                        f"{p.name}_with_valid[{p.num}-1:0] {p.name}s;"
                        for p in by_domain[domain]
                    ),
                    4 * ' ',
                ) + f"\n}} domain_{domain};"
            )

        package = f"""package {self.name};

    typedef enum logic[{self.enum_width()}-1:0] {{
{textwrap.indent(("," + nl).join(p.to_sv_enum() + ' = ' + str(i) for i,p in enumerate(self.packets)), ' ' * 8)}
    }} message_number;

{textwrap.indent(nl.join(p.to_sv_struct() for p in self.packets), 4*' ')}

{textwrap.indent(nl.join(
"typedef struct packed {" + nl + textwrap.indent(nl.join([
    p.name + " data;",
    "logic valid;",
]
), 4 * ' ') + nl + "} " + p.name + "_with_valid;"
for p in self.packets
), 4*' ')}

{textwrap.indent(nl.join(domain_structs), 4*' ')}

endpackage"""

        dpi_imports = [
            p.to_sv_dpi_import(self.name)
            for p in self.packets
        ]

        dpi_case_calls = [
            p.to_sv_dpi_case_call(self.name)
            for p in self.packets
        ]

        messenger = f"""module {self.name}_messenger #(
    type T = logic,
    type E =   int,
    E    N =    '0
) (
    input  clk,
    input  valid,
    input  [$bits(T)-1:0] i
);
    localparam int  B = ($bits(E) + $bits(T)+7)/8;

{textwrap.indent(nl.join(dpi_imports), 4*' ')}

    always @(posedge clk) begin
        if (valid) begin
            automatic logic[$bits(E) + $bits(T) - 1:0] message = {{i, N}};
            automatic byte unsigned  o[B];
            for (int i = 0; i < B-1; i++) begin
                o[i] = message[8*i +: 8];
            end
            o[B-1] = (8)'(message[8*(B-1) +: $bits(T) % 8]);
            unique case(N)
{textwrap.indent(nl.join(dpi_case_calls), 4*4*' ')}
                default: $error("unknown %d", N);
            endcase
        end
    end

endmodule"""

        domain_modules = []
        for domain in sorted(by_domain.keys()):
            domain_modules.append(f"""module {self.name}_domain_{domain}(
    input clk,
    input {self.name}::domain_{domain} tx
);

{textwrap.indent(nl.join(
    p.to_sv_messenger_inst(self.name)
    for p in by_domain[domain]
), 4*' ')}

endmodule""")

        define = '`define ' + self.name.upper() +  \
                """_DOMAIN(domain, clock)          \\
    transactions::domain_``domain tx_dom_``domain; \\
    transactions_domain_``domain                   \\
        transactions_domain_``domain (             \\
            .clk(clock),                           \\
            .tx(tx_dom_``domain),                  \\
            .*                                     \\
        );"""

        return f"""`ifndef {include_guard}
`define {include_guard}

{package}

{messenger}

{nl.join(domain_modules)}

{define}

`endif
"""

if __name__ == "__main__":

    parser = argparse.ArgumentParser()
    parser.add_argument("--definition", help="yml file containing packet definitions", required=True)
    parser.add_argument("--name"  , help="name of output package and namespace", required=True)
    parser.add_argument("--hpp", help="name of generated hpp", required=True)
    parser.add_argument("--cpp", help="name of generated cpp", required=True)
    parser.add_argument("--sv" , help="name of generated sv", required=True)

    args = parser.parse_args()

    p = Packets.load_file(args.name, args.definition)
    with open(args.hpp, "w") as f:
        f.write(p.to_hpp())
    with open(args.cpp, "w") as f:
        f.write(p.to_cpp(args.hpp))
    with open(args.sv, "w") as f:
        f.write(p.to_sv(args.sv))
