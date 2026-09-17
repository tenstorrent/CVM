# SPDX-FileCopyrightText: © 2026 Tenstorrent USA, Inc.
# SPDX-License-Identifier: Apache-2.0

"""The two slang capabilities replay's port-spec producer rests on.

Both are load-bearing and neither is obvious from slang's JSON output, which
serializes no bit width at all and reports types already elaborated. If either
stops working, a generated interposer would hold only for the one
parameterization slang happened to see -- and would say nothing about it.

Run under the build's interpreter on purpose: the wheel that matters is the one
bazel resolves, not whatever is installed system-wide.
"""

import unittest

from pyslang.ast import Compilation
from pyslang.parsing import TriviaKind
from pyslang.syntax import SyntaxTree


class TypeAsWritten(unittest.TestCase):
    """A port's type must come back as the source spells it, not resolved."""

    SRC = """
package p;
    parameter int W = 5;
    typedef struct packed { logic [3:0] a; logic b; } t;
endpackage

module m import p::*; (
    input  logic [W-1:0]              v,
    input  t                          s,
    output logic [$clog2(W*4)-1:0]    o
);
endmodule
"""

    def setUp(self):
        compilation = Compilation()
        compilation.addSyntaxTree(SyntaxTree.fromText(self.SRC))
        top = [i for i in compilation.getRoot().topInstances if i.name == "m"]
        self.assertEqual(len(top), 1)
        self.ports = {p.name: p for p in top[0].body.portList}

    def as_written(self, name):
        # The type hangs off the internal net/variable, not the port itself, and
        # the syntax carries its leading trivia.
        declared = self.ports[name].internalSymbol.declaredType
        return str(declared.typeSyntax).strip()

    def test_a_parameter_in_a_range_survives(self):
        self.assertEqual(self.as_written("v"), "logic [W-1:0]")
        self.assertEqual(str(self.ports["v"].type), "logic[4:0]")

    def test_a_typedef_stays_a_name(self):
        self.assertEqual(self.as_written("s"), "t")

    def test_a_clog2_expression_survives(self):
        self.assertEqual(self.as_written("o"), "logic [$clog2(W*4)-1:0]")
        self.assertEqual(str(self.ports["o"].type), "logic[4:0]")


class UntakenBranch(unittest.TestCase):
    """A port in a branch the preprocessor skipped must stay recoverable.

    slang keeps it as `disabledTokens` on the directive node. That is what lets
    the interposer carry the DUT's own `ifdef`s instead of being generated per
    define setting.
    """

    SRC = """module m (
    input logic a,
`ifdef FEAT
    input logic [7:0] b,
`endif
    output logic y
);
endmodule
"""

    def directives(self):
        found = []

        def collect(node):
            for attr in dir(node):
                if attr.startswith("_"):
                    continue
                try:
                    value = getattr(node, attr)
                except Exception:
                    continue
                if not hasattr(value, "trivia"):
                    continue  # not a Token
                for trivia in value.trivia:
                    if trivia.kind == TriviaKind.Directive:
                        found.append(trivia.syntax())

        SyntaxTree.fromText(self.SRC).root.visit(collect)
        return found

    def test_the_skipped_declaration_is_recoverable(self):
        # FEAT is undefined, so `b` is the branch that was not taken.
        conditionals = [d for d in self.directives() if hasattr(d, "expr")]
        self.assertEqual(len(conditionals), 1)

        branch = conditionals[0]
        self.assertEqual(str(branch.expr).strip(), "FEAT")

        text = "".join(str(t) for t in branch.disabledTokens).strip()
        self.assertEqual(text, "input logic [7:0] b,")


if __name__ == "__main__":
    unittest.main()
