// SPDX-FileCopyrightText: © 2026 Tenstorrent USA, Inc.
// SPDX-License-Identifier: Apache-2.0

// Integrated testbench: instantiates the DUT and interposer, drives the
// testbench side, and hangs an external passive monitor off the DUT side. The
// interposer never calls $finish; this module decides when the test ends.
module top (
    output logic       done,
    output logic [7:0] mon_edges,
    output logic [4:0] mon_last_result,
    output logic [4:0] mon_replay_result,
    // When replay finished. Pins the no-drift property: STROBE must not push
    // each successive vector later.
    output int         mon_done_time
);

    logic       clk_tb, rst_n_tb, valid_tb;
    logic [3:0] opa_tb, opb_tb;
    logic [4:0] result_tb;

    logic       clk_dut, rst_n_dut, valid_dut;
    logic [3:0] opa_dut, opb_dut;
    logic [4:0] result_dut;

    logic enable;

    // HIER keys this instance's dump in +cvm_replay_file, so several
    // interposers can replay different dumps.
    alu_replay #(.HIER("top.u_replay")) u_replay (
        .enable   (enable),
        .done     (done),
        .clk_tb   (clk_tb),    .clk_dut   (clk_dut),
        .rst_n_tb (rst_n_tb),  .rst_n_dut (rst_n_dut),
        .opa_tb   (opa_tb),    .opa_dut   (opa_dut),
        .opb_tb   (opb_tb),    .opb_dut   (opb_dut),
        .result_dut (result_dut), .result_tb (result_tb),
        .valid_dut  (valid_dut),  .valid_tb  (valid_tb)
    );

    alu u_dut (
        .clk    (clk_dut),
        .rst_n  (rst_n_dut),
        .opa    (opa_dut),
        .opb    (opb_dut),
        .result (result_dut),
        .valid  (valid_dut)
    );

    // In REPLAY this is ignored until replay finishes; in BYPASS it drives.
    initial begin
        clk_tb   = 1'b0;
        rst_n_tb = 1'b0;
        opa_tb   = 4'd5;
        opb_tb   = 4'd6;
        #3 rst_n_tb = 1'b1;
        forever #5 clk_tb = ~clk_tb;
    end

    // External passive monitor with no knowledge of the replay mode. It must
    // see traffic on the DUT side either way.
    always_ff @(posedge clk_dut) begin
        if (mon_edges !== 8'hFF) mon_edges <= mon_edges + 8'd1;
        if (valid_dut === 1'b1) begin
            mon_last_result <= result_dut;
            // Only while replay runs, so this is what the recorded stimulus
            // produced rather than what the testbench produces afterwards.
            if (done !== 1'b1) mon_replay_result <= result_dut;
        end
    end

    initial begin
        mon_edges         = 8'd0;
        mon_last_result   = 5'd0;
        mon_replay_result = 5'd0;
        mon_done_time     = 0;
        enable          = 1'b0;
        // Non-zero start: recorded timestamps are relative to this edge.
        #2 enable = 1'b1;
        wait (done === 1'b1);
        mon_done_time = int'($time);
        // Keep running so the monitor sees the testbench drive the DUT again.
        #100;
        $finish;
    end

endmodule
