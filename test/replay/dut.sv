// SPDX-FileCopyrightText: © 2026 Tenstorrent USA, Inc.
// SPDX-License-Identifier: Apache-2.0

// Ordinary module under replay: one combinational path and one registered path,
// with an async reset. Nothing here knows about EVCD.
module alu (
    input  logic       clk,
    input  logic       rst_n,
    input  logic [3:0] opa,
    input  logic [3:0] opb,
    output logic [4:0] result,
    output logic       valid
);

    always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            result <= 5'd0;
            valid  <= 1'b0;
        end else begin
            result <= {1'b0, opa} + {1'b0, opb};
            valid  <= 1'b1;
        end
    end

endmodule
