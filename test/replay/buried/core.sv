// SPDX-FileCopyrightText: © 2026 Tenstorrent USA, Inc.
// SPDX-License-Identifier: Apache-2.0

// A design with `alu` buried inside it, twice, and the committed edit that puts
// replay around one of them. That edit is the whole cost of the buried flow:
// intermediate nets, the interposer instantiated beside the DUT, and the DUT's
// connections repointed.
//
// Two things it deliberately does not do. It has no `ifdef`: the interposer is
// always instantiated, and its own REPLAY_ENABLE parameter decides whether it
// is a mux or plain wires. And it does not move `u_alu`, which keeps its name
// and so its hierarchical path -- which is what every waveform script and
// constraint file in a real design depends on.
module core import alu_pkg::*, alu_aux_pkg::*; (
    input  logic                         clk,
    input  logic                         rst_n,
    input  logic [3:0]                   opa,
    input  logic [3:0]                   opb,
    input  bundle_t                      cmd,
    input  logic [LANES-1:0][3:0]        mat,
    input  logic [$clog2(LANES*4)-1:0]   sel,
    input  logic                         no_gate,
    inout  wire  [2:0]                   bus,
    output logic                         bus_echo,
    output logic [4:0]                   result,
    output logic                         valid,
    output lane_t                        resp,
    output byte_t                        sum,
    output logic [$clog2(LANES*4)-1:0]   sel_echo,
    // The second, uninterposed instance, on the same stimulus. Under replay it
    // keeps following the testbench, which is how this shows that only the one
    // site was taken over.
    output logic [4:0]                   result_plain
);

    // The interposer's DUT side.
    logic                       rst_n_r, no_gate_r;
    logic [3:0]                 opa_r, opb_r;
    bundle_t                    cmd_r;
    logic [LANES-1:0][3:0]      mat_r;
    logic [$clog2(LANES*4)-1:0] sel_r, sel_echo_r;
    logic                       bus_echo_r, valid_r;
    logic [4:0]                 result_r;
    lane_t                      resp_r;
    byte_t                      sum_r;

    alu_interposer u_alu_replay (
        .clk (clk),
        .rst_n_outer (rst_n),
        .rst_n_inner (rst_n_r),
        .opa_outer (opa),
        .opa_inner (opa_r),
        .opb_outer (opb),
        .opb_inner (opb_r),
        .cmd_outer (cmd),
        .cmd_inner (cmd_r),
        .mat_outer (mat),
        .mat_inner (mat_r),
        .sel_outer (sel),
        .sel_inner (sel_r),
        .no_gate_outer (no_gate),
        .no_gate_inner (no_gate_r),
        // An inout is not repointed: one net, and the interposer taps it.
        .bus (bus),
        .bus_echo_inner (bus_echo_r),
        .bus_echo_outer (bus_echo),
        .result_inner (result_r),
        .result_outer (result),
        .valid_inner (valid_r),
        .valid_outer (valid),
        .resp_inner (resp_r),
        .resp_outer (resp),
        .sum_inner (sum_r),
        .sum_outer (sum),
        .sel_echo_inner (sel_echo_r),
        .sel_echo_outer (sel_echo)
    );

    alu u_alu (
        .clk      (clk),
        .rst_n    (rst_n_r),
        .opa      (opa_r),
        .opb      (opb_r),
        .cmd      (cmd_r),
        .mat      (mat_r),
        .sel      (sel_r),
        .no_gate  (no_gate_r),
        .bus      (bus),
        .bus_echo (bus_echo_r),
        .result   (result_r),
        .valid    (valid_r),
        .resp     (resp_r),
        .sum      (sum_r),
        .sel_echo (sel_echo_r)
    );

    // Untouched, and on its own inout net so the two do not contend.
    wire [2:0] bus_plain;
    alu u_alu_plain (
        .clk      (clk),
        .rst_n    (rst_n),
        .opa      (opa),
        .opb      (opb),
        .cmd      (cmd),
        .mat      (mat),
        .sel      (sel),
        .no_gate  (no_gate),
        .bus      (bus_plain),
        .bus_echo (),
        .result   (result_plain),
        .valid    (),
        .resp     (),
        .sum      (),
        .sel_echo ()
    );

endmodule
