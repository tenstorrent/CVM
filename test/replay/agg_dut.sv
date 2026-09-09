// SPDX-FileCopyrightText: © 2026 Tenstorrent USA, Inc.
// SPDX-License-Identifier: Apache-2.0

// IO is packed aggregates: a struct holding a packed array of structs, a 2-D
// packed array, and a struct output. Asynchronous reset, so cycle 0's outputs
// come from the recording rather than from what the testbench drove before.
module agg (
    input  logic            clk,
    input  logic            rst_n,
    input  agg_pkg::bundle_t cmd,
    input  logic [1:0][3:0]  mat,
    output agg_pkg::lane_t   resp,
    output logic [7:0]       sum
);

    always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            resp <= '0;
            sum  <= '0;
        end else begin
            resp.data  <= cmd.lanes[0].data + cmd.lanes[1].data;
            resp.valid <= cmd.hdr != 4'd0;
            sum        <= {4'd0, mat[0]} + {4'd0, mat[1]};
        end
    end

endmodule
