// SPDX-FileCopyrightText: © 2026 Tenstorrent USA, Inc.
// SPDX-License-Identifier: Apache-2.0

// DUT plus interposer, with a passive monitor on the DUT side. Every scenario
// runs off this module, switched by plusarg, with the checks here rather than
// in the wrapper so they travel to another simulator.
module top;

    import cvm_sim_pkg::*;

    // Kept running after replay so the monitor sees the testbench drive again.
    localparam int TAIL_CYCLES = 10;

    // A replay that never finishes would otherwise hang against a clock that
    // never stops, reading as neither pass nor fail.
    localparam int TIMEOUT_CYCLES = 1000;

    // The one delay: a clock has to come from somewhere.
    logic clk = 1'b0;
    always #5 clk = ~clk;

    logic reset_n, enable, done;

    logic             rst_n_tb, valid_tb;
    logic [3:0]       opa_tb, opb_tb;
    logic [4:0]       result_tb;
    alu_pkg::bundle_t cmd_tb;
    logic [alu_pkg::LANES-1:0][3:0] mat_tb;
    logic [$clog2(alu_pkg::LANES*4)-1:0] sel_tb, sel_echo_tb;
    alu_pkg::lane_t   resp_tb;
    logic [7:0]       sum_tb;

    logic             rst_n_dut, valid_dut;
    logic [3:0]       opa_dut, opb_dut;
    logic [4:0]       result_dut;
    alu_pkg::bundle_t cmd_dut;
    logic [alu_pkg::LANES-1:0][3:0] mat_dut;
    logic [$clog2(alu_pkg::LANES*4)-1:0] sel_dut, sel_echo_dut;
    alu_pkg::lane_t   resp_dut;
    logic [7:0]       sum_dut;

    logic [7:0] mon_edges;
    logic [4:0] mon_last_result;
    logic [4:0] mon_replay_result;
    // Read back through the struct, which shows the flattening put fields
    // where the DUT expects them.
    alu_pkg::lane_t mon_resp;
    logic [7:0]     mon_sum;
    // Cycles from enable rising to done, and the count at which done arrived.
    int         mon_done_cycles;
    int         cycles;

    localparam cvm_topology_gen::topology_t topo = cvm_topology_gen::mods;

    cvm_registry_callbacks u_cb (.clk(clk), .reset_n(reset_n));

    alu_replay #(
        .LOCATION (cvm_topology_gen::get_location(topo.TOP.REPLAY.ID, 0))
    ) u_replay (
        .clk      (clk),
        .reset_n  (reset_n),
        .enable   (enable),
        .done     (done),
        .rst_n_tb (rst_n_tb),  .rst_n_dut (rst_n_dut),
        .opa_tb   (opa_tb),    .opa_dut   (opa_dut),
        .opb_tb   (opb_tb),    .opb_dut   (opb_dut),
        .cmd_tb   (cmd_tb),    .cmd_dut   (cmd_dut),
        .mat_tb   (mat_tb),    .mat_dut   (mat_dut),
        .sel_tb   (sel_tb),    .sel_dut   (sel_dut),
        .result_dut (result_dut), .result_tb (result_tb),
        .valid_dut  (valid_dut),  .valid_tb  (valid_tb),
        .resp_dut   (resp_dut),   .resp_tb   (resp_tb),
        .sum_dut    (sum_dut),    .sum_tb    (sum_tb),
        .sel_echo_dut (sel_echo_dut), .sel_echo_tb (sel_echo_tb)
    );

    alu u_dut (
        .clk    (clk),
        .rst_n  (rst_n_dut),
        .opa    (opa_dut),
        .opb    (opb_dut),
        .cmd    (cmd_dut),
        .mat    (mat_dut),
        .sel    (sel_dut),
        .result (result_dut),
        .valid  (valid_dut),
        .resp   (resp_dut),
        .sum    (sum_dut),
        .sel_echo (sel_echo_dut)
    );

    // Ignored in REPLAY until it finishes; drives in BYPASS.
    initial begin
        reset_n  = 1'b0;
        enable   = 1'b0;
        rst_n_tb = 1'b0;
        // Matches cycle 0 of the recordings, so the outputs standing during the
        // first recorded cycle are the ones the recording assumes.
        opa_tb   = 4'd1;
        opb_tb   = 4'd2;
        cmd_tb   = '0;
        mat_tb   = '0;
        sel_tb   = '0;
    end

    // Knows nothing of the mode, and must see traffic either way.
    always_ff @(posedge clk) begin
        if (mon_edges !== 8'hFF) mon_edges <= mon_edges + 8'd1;
        if (valid_dut === 1'b1) begin
            mon_last_result <= result_dut;
            // Only while replay runs, so it is what the recording produced.
            if (done !== 1'b1) mon_replay_result <= result_dut;
        end
        // After done the testbench drives zeros, so these stop being what the
        // recording produced.
        if (done !== 1'b1) begin
            mon_resp <= resp_dut;
            mon_sum  <= sum_dut;
        end
    end

    always_ff @(posedge clk) begin
        if (!enable) begin
            cycles          <= 0;
            mon_done_cycles <= -1;
        end else begin
            cycles <= cycles + 1;
            if (done === 1'b1 && mon_done_cycles < 0) mon_done_cycles <= cycles;
        end
    end

    initial begin
        repeat (TIMEOUT_CYCLES) @(posedge clk);
        $fatal(1, "timeout after %0d cycles waiting for done", TIMEOUT_CYCLES);
    end

    initial begin
        // cvm_plusargs, not $value$plusargs, so tb_flags.cpp is the single
        // source of truth and an unknown name fails. -1 means "do not check".
        automatic int expect_errors        = cvm_plusargs::get_int("expect_errors");
        automatic int expect_replay_result = cvm_plusargs::get_int("expect_replay_result");
        automatic int expect_last_result   = cvm_plusargs::get_int("expect_last_result");
        automatic int expect_done_cycles   = cvm_plusargs::get_int("expect_done_cycles");
        automatic int min_monitor_edges    = cvm_plusargs::get_int("min_monitor_edges");
        automatic int skip_enable          = cvm_plusargs::get_int("skip_enable");
        automatic int errors               = 0;

        cvm_error_count_start();

        mon_edges         = 8'd0;
        mon_last_result   = 5'd0;
        mon_replay_result = 5'd0;
        mon_resp          = '0;
        mon_sum           = 8'd0;

        // The interposer loads out of reset, so reset precedes enable.
        repeat (4) @(negedge clk);
        reset_n  = 1'b1;
        rst_n_tb = 1'b1;
        repeat (2) @(negedge clk);
        if (skip_enable == 0) begin
            enable = 1'b1;
            @(posedge done);
        end
        // Keep running so the monitor sees the testbench drive the DUT again.
        repeat (TAIL_CYCLES) @(posedge clk);

        if (skip_enable == 0 && done !== 1'b1) begin
            $display("FAIL: the interposer should have reported done");
            errors++;
        end
        if (int'(mon_edges) < min_monitor_edges) begin
            $display("FAIL: monitor saw %0d clock edges on the DUT side, wanted %0d",
                     mon_edges, min_monitor_edges);
            errors++;
        end
        if (expect_replay_result >= 0 &&
                int'(mon_replay_result) != expect_replay_result) begin
            $display("FAIL: DUT produced %0d under replay, recording says %0d",
                     mon_replay_result, expect_replay_result);
            errors++;
        end
        if (expect_last_result >= 0 &&
                int'(mon_last_result) != expect_last_result) begin
            $display("FAIL: last result %0d, wanted %0d; after replay the testbench should drive again",
                     mon_last_result, expect_last_result);
            errors++;
        end
        // 5 + 4 = 9 with hdr == 0, so valid is low.
        if (expect_replay_result >= 0 &&
                (mon_resp.data !== 8'd9 || mon_resp.valid !== 1'b0 ||
                 mon_sum !== 8'd7)) begin
            $display("FAIL: resp.data=%0d resp.valid=%0b sum=%0d, wanted 9, 0 and 7",
                     mon_resp.data, mon_resp.valid, mon_sum);
            errors++;
        end
        if (expect_done_cycles >= 0 && mon_done_cycles != expect_done_cycles) begin
            $display("FAIL: replay finished after %0d cycles, wanted %0d",
                     mon_done_cycles, expect_done_cycles);
            errors++;
        end
        // cvm::log does not stop the simulation, so a scenario that stopped
        // checking would otherwise look exactly like one that passed.
        if (cvm_error_count() != expect_errors) begin
            $display("FAIL: %0d ERROR reports, wanted %0d",
                     cvm_error_count(), expect_errors);
            errors++;
        end

        $display("done_cycles=%0d dut_edges=%0d replay_result=%0d last_result=%0d resp.data=%0d sum=%0d",
                 mon_done_cycles, mon_edges, mon_replay_result, mon_last_result,
                 mon_resp.data, mon_sum);
        if (errors != 0) $fatal(1, "%0d checks failed", errors);
        $display("PASS");
        $finish;
    end

endmodule
