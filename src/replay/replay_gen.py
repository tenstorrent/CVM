#! /usr/bin/env python3
# SPDX-FileCopyrightText: © 2026 Tenstorrent USA, Inc.
# SPDX-License-Identifier: Apache-2.0

"""Generate an EVCD replay interposer and its port table from a YAML spec.
"""

import argparse
import os
import pathlib
import re
import sys
from dataclasses import dataclass, field
from typing import Dict, List, Optional, Tuple

import yaml
from mako.template import Template

INTERPOLATION = re.compile(r"\$\{([^}]*)\}")

DIRECTIONS = ("in", "out", "inout")


def interpolate(value, topology, where):
    """Resolve ${A.B.C} against topology attributes, if a topology was given."""
    if not isinstance(value, str):
        return value
    if not INTERPOLATION.search(value):
        return value
    if topology is None:
        sys.exit(
            f"{where}: uses ${{...}} interpolation but no --topology was given. "
            "Either pass a topology or use a literal width."
        )
    def sub(match):
        key = match.group(1).strip()
        if key not in topology:
            sys.exit(f"{where}: `{key}` is not defined in the topology")
        return str(topology[key])
    return INTERPOLATION.sub(sub, value)


@dataclass
class Port:
    name: str
    dir: str
    # The port's declaration as written in the DUT, passed through to the
    # generated module untouched -- `logic [NUM_CORES-1:0]`, `my_pkg::cmd_t`.
    # Never parsed here: the interposer takes $bits of the declared signal, so
    # a width that depends on a parameter stays a parameter.
    type_text: str = ""
    # The literal alternative, for a hand-written spec that knows its widths.
    width: Optional[int] = None
    dump_name: str = ""
    # The chain of conditions the DUT declares this port under, outermost
    # first, re-emitted as nested `ifdef`s. The interposer then appears and
    # disappears with the port, so it needs no defines of its own.
    when: List[str] = field(default_factory=list)

    def sv_type(self) -> str:
        return self.type_text if self.type_text else f"logic [{self.width - 1}:0]"


@dataclass
class Spec:
    name: str
    dut: str
    ports: List[Port]
    # The DUT port carrying the clock. Named here because a cycle-indexed
    # recording samples once per cycle, so a clock reads as a constant in it --
    # the dump cannot say which port is special. Never replayed.
    clock: str = ""
    # Packages the port types below resolve in, and DUT-private widths they
    # reference, both emitted into the generated module verbatim.
    imports: List[str] = field(default_factory=list)
    localparams: str = ""
    # Ports the recording may carry that this interposer does not replay. A
    # port the dump carries and nobody binds is otherwise fatal, which is what
    # stops a spec that has drifted from the DUT narrowing the test in silence.
    exclude: List[str] = field(default_factory=list)
    # Parameters the port types reference. The testbench must pass the same
    # values to the interposer and to the DUT.
    parameters: Dict = field(default_factory=dict)
    pipe_depth: int = 4096
    # 0 means "let the generated module compute it from the element size".
    push_max: int = 0
    tb_suffix: str = "_tb"
    dut_suffix: str = "_dut"
    standalone_top: bool = False

    def inputs(self) -> List[Port]:
        return [p for p in self.ports if p.dir == "in"]

    def outputs(self) -> List[Port]:
        return [p for p in self.ports if p.dir == "out"]

    def ignored(self) -> List[str]:
        """Dump ports deliberately left unreplayed -- the clock, plus `exclude`."""
        return [self.clock] + self.exclude

    def groups(self) -> List[Tuple[List[str], List[Port]]]:
        """Ports in order, with consecutive same-condition runs coalesced.

        One `ifdef` around a run of ports rather than around each of them,
        which for a DUT whose conditionals gate whole port blocks is the
        difference between a readable module and one guard per line.
        """
        out: List[Tuple[List[str], List[Port]]] = []
        for port in self.ports:
            if out and out[-1][0] == port.when:
                out[-1][1].append(port)
            else:
                out.append((port.when, [port]))
        return out

    @classmethod
    def load(cls, definitions: List[str], topology: Optional[Dict]) -> "Spec":
        merged: Dict = {}
        for path in definitions:
            with open(path) as handle:
                loaded = yaml.safe_load(handle) or {}
            assert isinstance(loaded, dict), f"{path}: top level must be a mapping"
            merged.update(loaded)

        assert len(merged) == 1, (
            f"expected exactly one spec, found {sorted(merged)}. One interposer "
            "per rule keeps the generated module name unambiguous."
        )
        name, body = next(iter(merged.items()))
        assert isinstance(body, dict), f"{name}: body must be a mapping"

        dut = body.get("dut")
        assert dut, f"{name}: `dut` is required (the module the IO flows through to)"

        raw_ports = body.get("ports")
        assert raw_ports, f"{name}: `ports` is required and must be non-empty"

        clock = body.get("clock")
        assert clock, (
            f"{name}: `clock` is required -- name the DUT's clock port. A "
            "cycle-indexed recording samples once per cycle, so a clock reads "
            "as a constant in it and must never be replayed."
        )

        suffixes = body.get("suffixes") or {}
        spec = cls(
            name=name,
            dut=dut,
            ports=[],
            clock=clock,
            imports=list(body.get("imports") or []),
            localparams=body.get("localparams", "") or "",
            exclude=[str(name) for name in (body.get("exclude") or [])],
            parameters=body.get("parameters") or {},
            pipe_depth=int(interpolate(body.get("pipe_depth", 4096), topology, name)),
            push_max=int(interpolate(body.get("push_max", 0), topology, name)),
            tb_suffix=suffixes.get("tb", "_tb"),
            dut_suffix=suffixes.get("dut", "_dut"),
            standalone_top=bool(body.get("standalone_top", False)),
        )

        for port_name, attrs in raw_ports.items():
            assert isinstance(attrs, dict), f"{name}.{port_name}: must be a mapping"
            where = f"{name}.{port_name}"
            assert port_name != clock, (
                f"{where}: this is the clock, so it must not be a replayed port"
            )

            direction = attrs.get("dir")
            assert direction in DIRECTIONS, (
                f"{where}: `dir` must be one of {DIRECTIONS}, got {direction!r}. "
                "Direction is always stated from the DUT's perspective."
            )
            if direction == "inout":
                sys.exit(
                    f"{where}: `dir: inout` is not supported yet."
                )

            type_text = attrs.get("type")
            width = attrs.get("width")
            assert (type_text is None) != (width is None), (
                f"{where}: give exactly one of `type` (the declaration as "
                "written, so the width stays parametric) or `width` (a literal)"
            )
            if width is not None:
                width = int(interpolate(width, topology, where))
                assert width > 0, f"{where}: width must be positive, got {width}"

            spec.ports.append(
                Port(
                    name=port_name,
                    dir=direction,
                    type_text=str(type_text) if type_text is not None else "",
                    width=width,
                    dump_name=attrs.get("dump_name", "") or port_name,
                    when=[str(c) for c in (attrs.get("when") or [])],
                )
            )

        return spec


def load_topology(path: Optional[str]) -> Optional[Dict]:
    """Flatten the topology JSON's attribute tree into dotted keys."""
    if not path:
        return None
    sys.path.insert(0, "src/topology/query")
    import json

    with open(path) as handle:
        data = json.load(handle)

    flat: Dict[str, object] = {}

    def walk(node, prefix):
        if isinstance(node, dict):
            for key, value in node.items():
                walk(value, f"{prefix}.{key}" if prefix else key)
        else:
            flat[prefix] = node

    walk(data, "")
    return flat


def render(template_path: str, output_path: str, spec: Spec) -> None:
    template = Template(filename=template_path)
    with open(output_path, "w") as handle:
        handle.write(template.render(spec=spec))


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--definitions", nargs="+", required=True)
    parser.add_argument("--topology", default=None)
    parser.add_argument("--sv", required=True)
    parser.add_argument("--merged", required=True)
    args = parser.parse_args()

    topology = load_topology(args.topology)
    spec = Spec.load(args.definitions, topology)

    # Templates are runfiles beside this script, as packet_gen.py does it.
    templates = pathlib.Path(os.path.abspath(__file__)).parent / "templates"
    render(str(templates / "template.sv"), args.sv, spec)

    with open(args.merged, "w") as handle:
        yaml.safe_dump(
            {
                spec.name: {
                    "dut": spec.dut,
                    "clock": spec.clock,
                    **({"exclude": spec.exclude} if spec.exclude else {}),
                    "ports": {
                        p.name: {
                            "dir": p.dir,
                            "type": p.sv_type(),
                            "dump_name": p.dump_name,
                            **({"when": p.when} if p.when else {}),
                        }
                        for p in spec.ports
                    },
                }
            },
            handle,
            sort_keys=False,
        )


if __name__ == "__main__":
    main()
