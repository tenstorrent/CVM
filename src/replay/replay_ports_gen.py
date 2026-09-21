#! /usr/bin/env python3
# SPDX-FileCopyrightText: © 2026 Tenstorrent USA, Inc.
# SPDX-License-Identifier: Apache-2.0

"""Derive a replay port spec from arbitrary Verilog, with slang.

Nothing emitted here may depend on a define or on a parameter's value. A spec
holding a resolved number that came from either has silently stopped describing
every configuration of the DUT, which is the whole property replay's generated
interposer exists to provide. So a port's type is copied as the source spells it
-- `logic [NUM_CORES-1:0]`, not `logic [3:0]` -- and a port inside an `ifdef`
keeps the condition instead of the answer.

The one place a define is read is elaboration, because slang has to pick a
branch to elaborate at all. Ports in the branch it did not pick are recovered
from the directive's disabled tokens, which is why the spec must come out
identical whichever way those defines are set. That invariant is a test.
"""

import argparse
import re
import sys
from dataclasses import dataclass, field
from typing import Dict, List, Optional, Tuple

import yaml
from pyslang.ast import ArgumentDirection
from pyslang.driver import Driver
from pyslang.parsing import Token, TriviaKind
from pyslang.syntax import SyntaxTree

# A synthetic module name for reparsing a recovered branch. Never emitted.
FRAGMENT = "__cvm_replay_fragment"

DIRECTIONS = {
    ArgumentDirection.In: "in",
    ArgumentDirection.Out: "out",
    ArgumentDirection.InOut: "inout",
}

IDENTIFIER = re.compile(r"\b[A-Za-z_][A-Za-z_0-9]*\b")

# A conditional can hold anything -- statements, instances, whole always blocks
# -- so a skipped branch is only a candidate port list if it says so.
DIRECTION_KEYWORD = re.compile(r"\b(input|output|inout)\b")

# Printing a node or a token yields its leading trivia too, which is where a
# comment above a declaration ends up. Both paths below strip it the same way,
# so a type reads identically whether it was elaborated or recovered.
LEADING_TRIVIA = re.compile(r"\A(?:\s|//[^\n]*\n|/\*.*?\*/)*", re.DOTALL)


@dataclass
class Port:
    name: str
    dir: str
    type_text: str
    when: Tuple[str, ...] = ()
    # Where the declaration is, so ports come out in source order whether they
    # were elaborated or recovered from a skipped branch. Order decides the
    # bit layout, so it has to be the source's and not the accident of which
    # branch slang took.
    order: Tuple[object, int] = ("", 0)


@dataclass
class Spec:
    dut: str
    # Every port the DUT declares, in declaration order. `clock` and `exclude`
    # are checked against this and then dropped, so what is emitted is what
    # gets replayed.
    ports: List[Port] = field(default_factory=list)
    imports: List[str] = field(default_factory=list)
    parameters: Dict[str, Dict[str, str]] = field(default_factory=dict)
    localparams: List[str] = field(default_factory=list)


def die(message: str) -> None:
    sys.exit(f"replay_ports_gen: {message}")


def tokens_in_order(node):
    """Every token under `node`, in source order.

    Children come back in declaration order, which for a syntax tree is source
    order -- so this is what makes the directive stack below meaningful.
    """
    if node is None:
        return
    if isinstance(node, Token):
        yield node
        return
    for child in node:
        yield from tokens_in_order(child)


def position(location) -> Tuple[object, int]:
    return (str(location.buffer), location.offset)


def nodes(sequence):
    """The real children of a separated list, without the separators.

    slang's separated lists interleave the commas, so every one of them has to
    be stepped over.
    """
    for item in sequence:
        if item is not None and not isinstance(item, Token):
            yield item


def text_of(tokens) -> str:
    return "".join(str(t) for t in tokens).strip()


def written_text(node) -> str:
    return LEADING_TRIVIA.sub("", str(node)).strip()


def type_text(written: str, direction: str) -> str:
    """The declaration as written, made complete enough to re-declare.

    slang reports the data type alone, so the net-or-variable keyword is not in
    it and an implicit type is empty. An inout has to be a net, and everything
    else a variable, so that keyword is supplied here rather than guessed at by
    whoever reads the spec.
    """
    written = written.strip()
    kind = "wire" if direction == "inout" else "logic"
    if not written or written.startswith("["):
        return f"{kind} {written}".strip()
    if direction == "inout":
        return f"wire {written}"
    return written


class Conditionals:
    """Where each token sits in the DUT's `ifdef` nesting, and what was skipped.

    slang keeps the branch the preprocessor did not take, as `disabledTokens` on
    the directive, and reports directives nested inside a skipped region in
    source order just as it does live ones. So one stack serves both: a
    declaration lands under the same chain whichever way the defines were set,
    which is what makes the spec define-independent.

    A frame is (prior, current): the conditions already tried and found false,
    and the one this branch is under. An `elsif` therefore reads as a nested
    `ifdef`/`else`.
    """

    def __init__(self) -> None:
        self.chain_at: Dict[object, Tuple[str, ...]] = {}
        self.skipped: List[Tuple[str, Tuple[str, ...], Tuple[object, int]]] = []

    def _chain(self, stack) -> Tuple[str, ...]:
        terms: List[str] = []
        for prior, current in stack:
            # Every earlier branch failed, so each contributes its inverse.
            for cond, negate in prior:
                terms.append(cond if negate else ("!" + cond))
            if current is not None:
                cond, negate = current
                terms.append(("!" + cond) if negate else cond)
        return tuple(terms)

    def scan(self, root) -> None:
        stack: List[List] = []
        for token in tokens_in_order(root):
            for trivia in token.trivia:
                if trivia.kind != TriviaKind.Directive:
                    continue
                self._directive(trivia.syntax(), stack)
            self.chain_at[token.location] = self._chain(stack)

    def _directive(self, node, stack) -> None:
        kind = node.kind.name
        if kind in ("IfDefDirective", "IfNDefDirective"):
            stack.append([[], (str(node.expr).strip(),
                               kind == "IfNDefDirective")])
        elif kind == "ElseDirective":
            if not stack:
                die("an `else` with no `ifdef` open")
            prior, current = stack[-1]
            if current is None:
                die("a second `else` for one `ifdef`")
            stack[-1] = [prior + [current], None]
        elif kind == "ElsIfDirective":
            if not stack:
                die("an `elsif` with no `ifdef` open")
            prior, current = stack[-1]
            if current is None:
                die("an `elsif` after an `else`")
            stack[-1] = [prior + [current],
                         (str(node.expr).strip(), False)]
        elif kind == "EndIfDirective":
            if not stack:
                die("an `endif` with no `ifdef` open")
            stack.pop()
        else:
            return

        disabled = getattr(node, "disabledTokens", None)
        if disabled is None:
            return
        body = text_of(disabled)
        if not body:
            return
        # The skipped tokens are real tokens of the same source, so they place
        # themselves.
        first = next(iter(disabled), None)
        where = position(first.location) if first is not None else ("", 0)
        self.skipped.append((body, self._chain(stack), where))


def parse_fragment(body: str, when: Tuple[str, ...],
                   where: Tuple[object, int]) -> List[Port]:
    """Port declarations recovered from a branch the preprocessor skipped.

    The tokens come back with no symbols behind them, so they are printed to
    exact source text and handed to slang's own parser: own printer to own
    parser, not pattern matching. A branch whose tokens do not stand alone as
    declarations -- half a port list, a macro expanding to one -- will not parse,
    and has to be reported rather than dropped.
    """
    # A branch that never mentions a direction is not a port list at all.
    if not DIRECTION_KEYWORD.search(body):
        return []
    stripped = body.rstrip().rstrip(",")
    # A branch reads either as part of an ANSI port list or as body port
    # declarations, depending on which style the DUT uses.
    attempts = (
        f"module {FRAGMENT} ({stripped});\nendmodule\n",
        f"module {FRAGMENT} ();\n{body}\nendmodule\n",
    )
    for source in attempts:
        ports = _fragment_ports(source, when)
        if ports:
            # One position for the whole branch; within it the parse order is
            # the source order.
            for index, port in enumerate(ports):
                port.order = (where[0], where[1] + index)
            return ports
    die(f"cannot read the skipped branch under {list(when)} as port "
        f"declarations:\n  {body}\n"
        "A branch has to stand alone as declarations to be recovered.")
    return []


def _module_declaration(root):
    """slang hands back the declaration itself when a file holds only one."""
    if root is None or isinstance(root, Token):
        return None
    if root.kind.name == "ModuleDeclaration":
        return root
    for child in root:
        found = _module_declaration(child)
        if found is not None:
            return found
    return None


def _fragment_ports(source: str, when: Tuple[str, ...]) -> List[Port]:
    module = _module_declaration(SyntaxTree.fromText(source).root)
    if module is None:
        return []

    found: List[Port] = []

    def take(direction_token, data_type, name: str) -> None:
        if direction_token is None:
            return
        # valueText, not str: the token carries any comment above it.
        direction = direction_token.valueText
        if direction not in ("input", "output", "inout"):
            return
        mapped = {"input": "in", "output": "out", "inout": "inout"}[direction]
        found.append(Port(name, mapped,
                          type_text(written_text(data_type), mapped), when))

    port_list = module.header.ports
    if port_list is not None and port_list.kind.name == "AnsiPortList":
        for node in nodes(port_list.ports):
            if node.kind.name != "ImplicitAnsiPort":
                return []
            take(node.header.direction, node.header.dataType,
                 node.declarator.name.valueText)

    for member in module.members:
        if member.kind.name != "PortDeclaration":
            continue
        header = member.header
        for declarator in nodes(member.declarators):
            take(header.direction, header.dataType,
                 declarator.name.valueText)

    return found


def span_of(node) -> Tuple[object, int, int]:
    """The source range a syntax node covers, as (buffer, first, last)."""
    first = node.getFirstToken().location
    last = node.getLastToken().location
    return (str(first.buffer), first.offset, last.offset)


def within(where: Tuple[object, int], span: Tuple[object, int, int]) -> bool:
    buffer, start, end = span
    return where[0] == buffer and start <= where[1] <= end


def module_of(instance):
    node = instance.body.syntax
    while node is not None and node.kind.name != "ModuleDeclaration":
        node = node.parent
    if node is None:
        die(f"cannot find the declaration of module `{instance.name}`")
    return node


def collect_imports(module) -> List[str]:
    """The packages the DUT's header imports, by name.

    They have to be in the interposer's header too: a port declared with an
    unqualified package type does not resolve from a body import.

    Read per item rather than per declaration, because one declaration can
    import from several packages -- `import a::*, b::t;` is two of them.
    """
    names: List[str] = []
    for declaration in module.header.imports:
        for item in nodes(declaration.items):
            name = item.package.valueText
            if name and name not in names:
                names.append(name)
    return names


def collect_parameters(module) -> Dict[str, Dict[str, str]]:
    """The DUT's parameters with their default *expressions*, never their values.

    The testbench passes the same values to the interposer and to the DUT, so
    what matters here is that the interposer declares the same names with
    defaults that mean the same thing in the same scope.
    """
    out: Dict[str, Dict[str, str]] = {}
    port_list = module.header.parameters
    if port_list is None:
        return out
    for declaration in nodes(port_list.declarations):
        if not hasattr(declaration, "declarators"):
            continue
        is_type = declaration.kind.name == "TypeParameterDeclaration"
        for declarator in nodes(declaration.declarators):
            name = declarator.name.valueText
            if is_type:
                initializer = declarator.assignment
                out[name] = {
                    "type": "type",
                    "default": str(initializer).lstrip("= ").strip(),
                }
                continue
            if declarator.initializer is None:
                die(f"parameter `{name}` has no default, so the interposer "
                    "cannot declare it; give it one in the DUT")
            out[name] = {
                "type": str(declaration.type).strip() or "int",
                "default": str(declarator.initializer.expr).strip(),
            }
    return out


def collect_localparams(module, conditionals: Conditionals,
                        needed_by: List[str]) -> List[str]:
    """Body localparams the port types reach, copied verbatim and in order.

    A module can hide a width behind a localparam no other file can see, so the
    interposer has to restate it. Only the closure the port types actually
    reach, so an unrelated one is not dragged in.
    """
    declared: List[Tuple[str, str, Tuple[str, ...]]] = []
    for member in module.members:
        if member.kind.name != "ParameterDeclarationStatement":
            continue
        text = str(member).strip()
        if not text.startswith("localparam"):
            continue
        chain = conditionals.chain_at.get(member.getFirstToken().location, ())
        for declarator in nodes(member.parameter.declarators):
            declared.append((declarator.name.valueText, text, chain))

    wanted = set()
    for text in needed_by:
        wanted.update(item.group(0) for item in IDENTIFIER.finditer(text))

    # Reverse source order, so selecting one can pull in an earlier one it
    # references; legal SystemVerilog declares before use.
    selected: List[Tuple[str, str, Tuple[str, ...]]] = []
    for name, text, chain in reversed(declared):
        if name not in wanted:
            continue
        selected.append((name, text, chain))
        wanted.update(item.group(0) for item in IDENTIFIER.finditer(text))
    selected.reverse()

    out: List[str] = []
    for name, text, chain in selected:
        if chain:
            die(f"localparam `{name}` is needed by a port type but is itself "
                f"inside {list(chain)}; copying it would drop the condition")
        if text not in out:
            out.append(text)
    return out


def build(driver, top: str) -> Spec:
    compilation = driver.createCompilation()
    instances = [i for i in compilation.getRoot().topInstances if i.name == top]
    if not instances:
        names = sorted(i.name for i in compilation.getRoot().topInstances)
        die(f"`{top}` is not a top-level instance; slang elaborated {names}")
    instance = instances[0]
    module = module_of(instance)

    conditionals = Conditionals()
    for tree in driver.syntaxTrees:
        conditionals.scan(tree.root)

    # Skipped branches elsewhere in the source set belong to other modules;
    # reparsing those as port declarations is wrong and slow.
    dut_span = span_of(module)

    spec = Spec(dut=top)
    seen = set()

    for port in instance.body.portList:
        direction = DIRECTIONS.get(port.direction)
        if direction is None:
            die(f"port `{port.name}` is `{port.direction}`, which a recording "
                "does not describe; exclude it and wire it directly")
        syntax = port.syntax
        if syntax is not None and getattr(syntax, "dimensions", None):
            die(f"port `{port.name}` has an unpacked dimension, which slang "
                "does not report as part of the type; exclude it")
        written = written_text(port.internalSymbol.declaredType.typeSyntax)
        when = ()
        where: Tuple[object, int] = ("", 0)
        if syntax is not None:
            location = syntax.getFirstToken().location
            when = conditionals.chain_at.get(location, ())
            where = position(location)
        spec.ports.append(Port(port.name, direction,
                               type_text(written, direction), when, where))
        seen.add(port.name)

    # The other branch. These are ports for every configuration but the one
    # slang elaborated, so leaving them out would make the spec a description of
    # this build rather than of the module.
    for body, when, where in conditionals.skipped:
        if not within(where, dut_span):
            continue
        for port in parse_fragment(body, when, where):
            if port.name in seen:
                continue
            spec.ports.append(port)
            seen.add(port.name)

    spec.ports.sort(key=lambda p: p.order)

    spec.imports = collect_imports(module)
    spec.parameters = collect_parameters(module)
    spec.localparams = collect_localparams(
        module, conditionals,
        [p.type_text for p in spec.ports] +
        [p["default"] for p in spec.parameters.values()])
    return spec


def emit(spec: Spec, name: str, clock: str, exclude: List[str]) -> str:
    skip = set(exclude) | {clock}
    body: Dict[str, object] = {"dut": spec.dut, "clock": clock}
    if spec.imports:
        body["imports"] = spec.imports
    if spec.parameters:
        body["parameters"] = spec.parameters
    if spec.localparams:
        body["localparams"] = "".join(f"    {line}\n"
                                      for line in spec.localparams)
    # Carried into the spec, not just dropped from `ports`: the module reports
    # them as unreplayed, or a recording carrying one fails conformance.
    if exclude:
        body["exclude"] = list(exclude)
    ports: Dict[str, object] = {}
    for port in spec.ports:
        if port.name in skip:
            continue
        entry: Dict[str, object] = {"dir": port.dir, "type": port.type_text}
        if port.when:
            entry["when"] = list(port.when)
        ports[port.name] = entry
    body["ports"] = ports

    header = (
        "# Generated by src/replay/replay_ports_gen.py -- do not edit.\n"
        f"# Every port of {spec.dut} as its source declares it, so this spec\n"
        "# describes the module rather than one configuration of it.\n")
    return header + yaml.safe_dump({name: body}, sort_keys=False, width=200)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", action="append", required=True,
                        help="a Verilog file; repeat for each")
    parser.add_argument("--include-dir", action="append", default=[])
    parser.add_argument("--define", action="append", default=[],
                        help="read only to elaborate; the spec must not depend "
                             "on it")
    parser.add_argument("--top", required=True, help="the module to replay")
    parser.add_argument("--clock", required=True,
                        help="the DUT's clock port, which is never replayed")
    parser.add_argument("--exclude", action="append", default=[],
                        help="a port to leave unreplayed; repeat for each")
    parser.add_argument("--name", default=None,
                        help="the spec key, and so the interposer's module "
                             "name (default <top>_replay)")
    parser.add_argument("--out", required=True)
    args = parser.parse_args()

    command = ["slang"] + list(args.source)
    for directory in args.include_dir:
        command += ["-I", directory]
    for define in args.define:
        command += ["-D", define]
    command += ["--top", args.top]

    driver = Driver()
    driver.addStandardArgs()
    if not driver.parseCommandLine(" ".join(command)):
        die("slang rejected its own command line")
    if not driver.processOptions():
        die("slang rejected its options")
    if not driver.parseAllSources():
        die(f"could not parse the sources for `{args.top}`")

    spec = build(driver, args.top)
    if not spec.ports:
        die(f"`{args.top}` has no ports")
    names = {port.name for port in spec.ports}
    if args.clock not in names:
        die(f"`{args.clock}` is not a port of `{args.top}`; its ports are "
            f"{sorted(names)}")
    for name in args.exclude:
        if name not in names:
            die(f"excluded port `{name}` is not a port of `{args.top}`")

    name = args.name or f"{args.top}_replay"
    with open(args.out, "w") as handle:
        handle.write(emit(spec, name, args.clock, args.exclude))


if __name__ == "__main__":
    main()
