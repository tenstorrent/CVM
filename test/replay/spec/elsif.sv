// SPDX-FileCopyrightText: © 2026 Tenstorrent USA, Inc.
// SPDX-License-Identifier: Apache-2.0

// Ports under an `elsif` chain, which the spec producer must reach the
// unelaborated branches of as it does for a plain `ifdef`/`else`.
module elsif_ports (
    input  logic clk,
    input  logic rst_n,
`ifdef MODE_X
    input  logic [3:0] mode_x, mode_x_also,
`elsif MODE_Y
    input  logic [7:0] mode_y, mode_y_also,
`else
    input  logic       mode_none, mode_none_also,
`endif
    output logic       out
);

    assign out = rst_n;

endmodule
