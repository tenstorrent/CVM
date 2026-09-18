// SPDX-FileCopyrightText: © 2026 Tenstorrent USA, Inc.
// SPDX-License-Identifier: Apache-2.0

// A design with `alu` buried inside it, twice. Nothing here knows about replay:
// no interposer, no intermediate nets, no parameter, no define. That is the
// point -- the harness reaches in from outside, so a design nobody will let you
// modify can still be replayed.
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
    // A second instance on the same stimulus, never replayed. Under replay it
    // keeps following the design, which is how this shows that force reached
    // one instance and not the module.
    output logic [4:0]                   result_plain
);

    alu u_alu (
        .clk      (clk),
        .rst_n    (rst_n),
        .opa      (opa),
        .opb      (opb),
        .cmd      (cmd),
        .mat      (mat),
        .sel      (sel),
        .no_gate  (no_gate),
        .bus      (bus),
        .bus_echo (bus_echo),
        .result   (result),
        .valid    (valid),
        .resp     (resp),
        .sum      (sum),
        .sel_echo (sel_echo)
    );

    // Its own inout net, so the two do not contend.
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
