#! /usr/bin/env python3
# SPDX-FileCopyrightText: © 2026 Tenstorrent USA, Inc.
# SPDX-License-Identifier: Apache-2.0

"""Generate an EVCD replay interposer and its port table from a YAML spec.

Nothing here reads an EVCD: the dump is validated against the generated port
table at runtime, which is what lets one build replay any number of dumps.
"""

import argparse
import os
import pathlib
import re
import sys
from dataclasses import dataclass, field
from typing import Dict, List, Optional

import yaml
from mako.template import Template

INTERPOLATION = re.compile(r"\$\{([^}]*)\}")

DIRECTIONS = ("in", "out", "inout")

# Must match CVM_PIPE_MAX_WORDS in src/pipe/cvm_pipe_pkg.sv: the largest array
# formal the sized DPI families provide.
PIPE_MAX_WORDS = 32768


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
    width: int
    dir: str
    dump_name: str = ""
    check: bool = True
    source: str = "replay"        # "replay" or "external"
    bit_offset: int = 0

    @property
    def is_external(self) -> bool:
        return self.source == "external"

    def sv_range(self) -> str:
        return f"[{self.width - 1}:0]"


@dataclass
class Spec:
    name: str
    dut: str
    ports: List[Port]
    # Unknown recorded input bits resolve to this. Not "whatever the simulator
    # collapses X to": that is neither reproducible nor available on an
    # emulator.
    x_fill: str = "zero"
    pipe_depth: int = 4096
    # 0 means "compute it from the payload width"; see push_max_elements.
    push_max: int = 0
    tb_suffix: str = "_tb"
    dut_suffix: str = "_dut"
    standalone_top: bool = False
    total_bits: int = 0

    @property
    def words(self) -> int:
        return (self.total_bits + 31) // 32

    @property
    def padded_bits(self) -> int:
        return self.words * 32

    @property
    def element_words(self) -> int:
        """Words the transport carries per cycle: a cycle number plus the
        stimulus, the expectation and the care mask."""
        return 1 + 3 * self.words

    @property
    def push_max_elements(self) -> int:
        """Most elements one push may carry.

        Narrowed from the width where needed: one element is several words, so
        a fixed default in elements can exceed the largest array formal for a
        wide DUT. Emitted into the generated source so it is visible.
        """
        if self.push_max:
            return self.push_max
        # Enough to amortise the host round trip without sizing the formal for
        # the sake of it; narrowed only when the ladder cannot hold it.
        return max(1, min(1024, PIPE_MAX_WORDS // self.element_words))

    def inputs(self) -> List[Port]:
        return [p for p in self.ports if p.dir == "in"]

    def outputs(self) -> List[Port]:
        return [p for p in self.ports if p.dir == "out"]

    def driven_inputs(self) -> List[Port]:
        """DUT inputs the interposer drives from the dump."""
        return [p for p in self.inputs() if not p.is_external]

    def external_inputs(self) -> List[Port]:
        return [p for p in self.inputs() if p.is_external]

    def checked_outputs(self) -> List[Port]:
        return [p for p in self.outputs() if p.check]

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

        suffixes = body.get("suffixes") or {}
        spec = cls(
            name=name,
            dut=dut,
            ports=[],
            x_fill=str(interpolate(body.get("x_fill", "zero"), topology, name)),
            pipe_depth=int(interpolate(body.get("pipe_depth", 4096), topology, name)),
            push_max=int(interpolate(body.get("push_max", 0), topology, name)),
            tb_suffix=suffixes.get("tb", "_tb"),
            dut_suffix=suffixes.get("dut", "_dut"),
            standalone_top=bool(body.get("standalone_top", False)),
        )

        offset = 0
        for port_name, attrs in raw_ports.items():
            assert isinstance(attrs, dict), f"{name}.{port_name}: must be a mapping"
            where = f"{name}.{port_name}"

            width = interpolate(attrs.get("width"), topology, where)
            assert width is not None, f"{where}: `width` is required"
            width = int(width)
            assert width > 0, f"{where}: width must be positive, got {width}"

            direction = attrs.get("dir")
            assert direction in DIRECTIONS, (
                f"{where}: `dir` must be one of {DIRECTIONS}, got {direction!r}. "
                "Direction is always stated from the DUT's perspective."
            )
            if direction == "inout":
                sys.exit(
                    f"{where}: `dir: inout` is not supported. A pass-through "
                    "interposer would need tristate resolution in both "
                    "directions, which is out of scope for now."
                )

            source = attrs.get("source", "replay")
            assert source in ("replay", "external"), (
                f"{where}: `source` must be `replay` or `external`, got {source!r}"
            )
            if source == "external" and direction != "in":
                sys.exit(f"{where}: `source: external` only applies to `dir: in`")

            spec.ports.append(
                Port(
                    name=port_name,
                    width=width,
                    dir=direction,
                    dump_name=attrs.get("dump_name", "") or port_name,
                    check=bool(attrs.get("check", True)),
                    source=source,
                    bit_offset=offset,
                )
            )
            offset += width

        spec.total_bits = offset

        # The limit that actually exists, replacing a per-port width cap that
        # mirrored a constant no longer in cvm_replay_pkg.sv. Nothing carries a
        # single port any more: ports are slices of one flat vector, and what is
        # bounded is how much of it fits in one push.
        if spec.push_max_elements * spec.element_words > PIPE_MAX_WORDS:
            sys.exit(
                f"{name}: {spec.total_bits} bits of ports need "
                f"{spec.element_words} words per element, so a push of "
                f"{spec.push_max_elements} exceeds the push formal "
                f"({PIPE_MAX_WORDS} words). Lower `push_max`, or raise "
                "CVM_PIPE_MAX_WORDS in src/pipe/cvm_pipe_pkg.sv."
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
                    "total_bits": spec.total_bits,
                    "ports": {
                        p.name: {
                            "width": p.width,
                            "dir": p.dir,
                            "dump_name": p.dump_name,
                            "check": p.check,
                            "source": p.source,
                            "bit_offset": p.bit_offset,
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
