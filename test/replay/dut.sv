// SPDX-FileCopyrightText: © 2026 Tenstorrent USA, Inc.
// SPDX-License-Identifier: Apache-2.0

// Ordinary module under replay, with an async reset so cycle 0's outputs come
// from the recording rather than from what the testbench drove before.
//
// The shapes here are the ones a real block uses and a literal width cannot
// describe: a struct holding a packed array of structs, a 2-D packed array
// sized by a package parameter, a struct output, and a width that is a $clog2
// expression. Replay flattens all of them into one vector and must land the
// fields where the DUT expects them.
//
// FEAT_GATE adds a port pair, so one generated interposer has to be correct
// both with the define and without it.
module alu (
    input  logic             clk,
    input  logic             rst_n,
    input  logic [3:0]       opa,
    input  logic [3:0]       opb,
    input  alu_pkg::bundle_t cmd,
    input  logic [alu_pkg::LANES-1:0][3:0] mat,
    input  logic [$clog2(alu_pkg::LANES*4)-1:0] sel,
`ifdef FEAT_GATE
    input  logic             gate,
    output logic             gate_echo,
`endif
    output logic [4:0]       result,
    output logic             valid,
    output alu_pkg::lane_t   resp,
    output logic [7:0]       sum,
    output logic [$clog2(alu_pkg::LANES*4)-1:0] sel_echo
);

    always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            result <= 5'd0;
            valid  <= 1'b0;
            resp   <= '0;
            sum    <= '0;
            sel_echo <= '0;
`ifdef FEAT_GATE
            gate_echo <= 1'b0;
`endif
        end else begin
            result     <= {1'b0, opa} + {1'b0, opb};
            valid      <= 1'b1;
            resp.data  <= cmd.lanes[0].data + cmd.lanes[1].data;
            resp.valid <= cmd.hdr != 4'd0;
            sum        <= {4'd0, mat[0]} + {4'd0, mat[1]};
            sel_echo   <= sel;
`ifdef FEAT_GATE
            gate_echo  <= gate;
`endif
        end
    end

endmodule
