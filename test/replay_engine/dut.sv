// SPDX-FileCopyrightText: © 2026 Tenstorrent USA, Inc.
// SPDX-License-Identifier: Apache-2.0

// Registered, so the alignment under test is real: y follows a by one cycle,
// and the +1 stops a misaligned comparison passing by accident. Reset so y is
// known from the first cycle, hence checkable.
module regadd (
    input  logic       clk,
    input  logic       reset_n,
    input  logic [7:0] a,
    output logic [7:0] y
);

    always_ff @(posedge clk) begin
        if (!reset_n) y <= 8'd0;
        else          y <= a + 8'd1;
    end

endmodule
