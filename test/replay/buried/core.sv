// SPDX-FileCopyrightText: © 2026 Tenstorrent USA, Inc.
// SPDX-License-Identifier: Apache-2.0

// A design with `alu` buried inside it four times: once on its own, twice as an
// instance array, and once left alone. Nothing here knows about replay -- no
// interposer, no intermediate nets, no parameter, no define. That is the point:
// a design nobody will let you modify can still be replayed.
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
    // The array, so replay has to reach an element of one.
    output logic [1:0][4:0]              result_lane,
    // Never replayed. Under replay it keeps following the design, which is how
    // this shows force reached the instances it was pointed at and no others.
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

    // An instance array, each element on its own inout net.
    wire [1:0][2:0] bus_lane;
    alu u_lane [1:0] (
        .clk      (clk),
        .rst_n    (rst_n),
        .opa      (opa),
        .opb      (opb),
        .cmd      (cmd),
        .mat      (mat),
        .sel      (sel),
        .no_gate  (no_gate),
        .bus      (bus_lane),
        .bus_echo (),
        .result   (result_lane),
        .valid    (),
        .resp     (),
        .sum      (),
        .sel_echo ()
    );

    wire [2:0] bus_plain;
    alu u_plain (
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
