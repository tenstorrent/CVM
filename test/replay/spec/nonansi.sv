// SPDX-FileCopyrightText: © 2026 Tenstorrent USA, Inc.
// SPDX-License-Identifier: Apache-2.0

// Non-ANSI ports: a multi-port conditional group, the null port its disabled
// header names leave, and ordering by declaration rather than header name.
module nonansi_ports (
    clk,
    rst_n,
    out,
`ifdef FEAT_WIDE
    wide_a,
    wide_b,
    wide_c,
`endif
);

    input  logic clk;
    input  logic rst_n;
    output logic out;
`ifdef FEAT_WIDE
    // Three in one branch, and `wide_b` inherits its direction and type.
    input  logic [7:0] wide_a, wide_b;
    output logic [3:0] wide_c;
`endif
    assign out = rst_n;

endmodule
