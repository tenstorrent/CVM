// SPDX-FileCopyrightText: © 2026 Tenstorrent USA, Inc.
// SPDX-License-Identifier: Apache-2.0

// Integrated testbench: DUT plus interposer, driving the testbench side with a
// passive monitor on the DUT side. The interposer never calls $finish; this
// module decides when the test ends.
//
// Every scenario runs off this module, switched by plusarg, with the checks
// here rather than in the wrapper so they travel to another simulator. One
// clock for everything -- the interposer takes it as infrastructure, not as a
// replayed port.
module top;

    import cvm_sim_pkg::*;

    // Kept running after replay so the monitor sees the testbench drive again.
    localparam int TAIL_CYCLES = 10;

    // Without this a replay that never finishes hangs against a clock that
    // never stops, which reads as neither pass nor fail.
    localparam int TIMEOUT_CYCLES = 1000;

    // The one delay: a clock has to come from somewhere.
    logic clk = 1'b0;
    always #5 clk = ~clk;

    logic reset_n, enable, done;

    logic       rst_n_tb, valid_tb;
    logic [3:0] opa_tb, opb_tb;
    logic [4:0] result_tb;

    logic       rst_n_dut, valid_dut;
    logic [3:0] opa_dut, opb_dut;
    logic [4:0] result_dut;

    logic [7:0] mon_edges;
    logic [4:0] mon_last_result;
    logic [4:0] mon_replay_result;
    // Cycles from enable rising to done, and the count at which done arrived.
    int         mon_done_cycles;
    int         cycles;

    // HIER keys the recording, so several interposers can replay different
    // ones.
    alu_replay #(.HIER("top.u_replay")) u_replay (
        .clk      (clk),
        .reset_n  (reset_n),
        .enable   (enable),
        .done     (done),
        .rst_n_tb (rst_n_tb),  .rst_n_dut (rst_n_dut),
        .opa_tb   (opa_tb),    .opa_dut   (opa_dut),
        .opb_tb   (opb_tb),    .opb_dut   (opb_dut),
        .result_dut (result_dut), .result_tb (result_tb),
        .valid_dut  (valid_dut),  .valid_tb  (valid_tb)
    );

    alu u_dut (
        .clk    (clk),
        .rst_n  (rst_n_dut),
        .opa    (opa_dut),
        .opb    (opb_dut),
        .result (result_dut),
        .valid  (valid_dut)
    );

    // Ignored in REPLAY until it finishes; drives in BYPASS. Released on
    // falling edges so they never race the rising edge the DUT uses.
    initial begin
        reset_n  = 1'b0;
        enable   = 1'b0;
        rst_n_tb = 1'b0;
        // Matches cycle 0 of the recordings, so the outputs standing during the
        // first recorded cycle are the ones the recording assumes.
        opa_tb   = 4'd1;
        opb_tb   = 4'd2;
    end

    // Knows nothing of the mode, and must see traffic either way.
    always_ff @(posedge clk) begin
        if (mon_edges !== 8'hFF) mon_edges <= mon_edges + 8'd1;
        if (valid_dut === 1'b1) begin
            mon_last_result <= result_dut;
            // Only while replay runs, so it is what the recording produced.
            if (done !== 1'b1) mon_replay_result <= result_dut;
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
        // source of truth: an absent plusarg yields the declared default, an
        // unknown name fails, and the type is checked. -1 means "do not check".
        automatic int expect_errors        = cvm_plusargs::get_int("expect_errors");
        automatic int expect_replay_result = cvm_plusargs::get_int("expect_replay_result");
        automatic int expect_last_result   = cvm_plusargs::get_int("expect_last_result");
        automatic int expect_done_cycles   = cvm_plusargs::get_int("expect_done_cycles");
        automatic int min_monitor_edges    = cvm_plusargs::get_int("min_monitor_edges");
        automatic int errors               = 0;

        // Before anything under test can log.
        cvm_error_count_start();

        mon_edges         = 8'd0;
        mon_last_result   = 5'd0;
        mon_replay_result = 5'd0;

        // The interposer loads out of reset, so reset precedes enable.
        repeat (4) @(negedge clk);
        reset_n  = 1'b1;
        rst_n_tb = 1'b1;
        repeat (2) @(negedge clk);
        enable = 1'b1;

        @(posedge done);
        // Keep running so the monitor sees the testbench drive the DUT again.
        repeat (TAIL_CYCLES) @(posedge clk);

        if (done !== 1'b1) begin
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

        $display("done_cycles=%0d dut_edges=%0d replay_result=%0d last_result=%0d",
                 mon_done_cycles, mon_edges, mon_replay_result, mon_last_result);
        if (errors != 0) $fatal(1, "%0d checks failed", errors);
        $display("PASS");
        $finish;
    end

endmodule
