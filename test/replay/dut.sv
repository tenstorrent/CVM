// SPDX-FileCopyrightText: © 2026 Tenstorrent USA, Inc.
// SPDX-License-Identifier: Apache-2.0

// Ordinary module under replay, with an async reset so cycle 0's outputs come
// from the recording rather than from what the testbench drove before.
//
// Some IO is packed aggregates -- a struct holding a packed array of structs, a
// 2-D packed array, a struct output -- because replay flattens the boundary to
// one vector and must land those fields where the DUT expects them.
module alu (
    input  logic             clk,
    input  logic             rst_n,
    input  logic [3:0]       opa,
    input  logic [3:0]       opb,
    input  alu_pkg::bundle_t cmd,
    input  logic [1:0][3:0]  mat,
    output logic [4:0]       result,
    output logic             valid,
    output alu_pkg::lane_t   resp,
    output logic [7:0]       sum
);

    always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            result <= 5'd0;
            valid  <= 1'b0;
            resp   <= '0;
            sum    <= '0;
        end else begin
            result     <= {1'b0, opa} + {1'b0, opb};
            valid      <= 1'b1;
            resp.data  <= cmd.lanes[0].data + cmd.lanes[1].data;
            resp.valid <= cmd.hdr != 4'd0;
            sum        <= {4'd0, mat[0]} + {4'd0, mat[1]};
        end
    end

endmodule
