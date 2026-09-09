// SPDX-FileCopyrightText: © 2026 Tenstorrent USA, Inc.
// SPDX-License-Identifier: Apache-2.0

// The engine on its own, with synthetic elements rather than a recording, so
// the transport and the alignment can be tested without the EVCD layer. The
// only delay is the clock.
//
// Scenarios run off one build, selected by +scenario. See tb_engine_dpi.cpp.
module top;

    import cvm_sim_pkg::*;

    // Queues this scenario's elements on the host.
    import "DPI-C" function void tb_engine_stimulus(string hier, int padded);

    localparam int PADDED         = 32;
    localparam int TIMEOUT_CYCLES = 5000;

    // The one delay: a clock has to come from somewhere.
    logic clk = 1'b0;
    always #5 clk = ~clk;

    logic reset_n, enable;
    logic [7:0] tb_a;

    logic [PADDED-1:0] observed, driven;
    logic              done;
    logic [31:0]       mismatches, first_fail_cycle;
    logic [PADDED-1:0] fail_bits;

    logic [7:0] y;

    // The testbench's, not replayed: the recording starts out of reset.
    regadd u_dut (
        .clk     (clk),
        .reset_n (reset_n),
        .a       (driven[7:0]),
        .y       (y)
    );

    // The DUT boundary: the testbench's value on the input slice, the DUT's on
    // the output slice.
    assign observed = {16'd0, y, tb_a};

    cvm_replay_engine #(
        .HIER       ("top.u_replay"),
        .PADDED     (PADDED),
        .PIPE_DEPTH (64)
    ) u_replay (
        .clk              (clk),
        .reset_n          (reset_n),
        .enable           (enable),
        .done             (done),
        .observed         (observed),
        .driven           (driven),
        .mismatches       (mismatches),
        .first_fail_cycle (first_fail_cycle),
        .fail_bits        (fail_bits)
    );

    initial begin
        reset_n = 1'b0;
        enable  = 1'b0;
        // Matches kInputBeforeEnable in tb_engine_dpi.cpp.
        tb_a    = 8'h00;
    end

    initial begin
        repeat (TIMEOUT_CYCLES) @(posedge clk);
        $fatal(1, "timeout after %0d cycles waiting for done", TIMEOUT_CYCLES);
    end

    initial begin
        // -1 on expect_mismatches means "must be non-zero"; on
        // expect_first_fail_cycle it means "do not check".
        automatic int expect_mismatches = cvm_plusargs::get_int("expect_mismatches");
        automatic int expect_first_fail = cvm_plusargs::get_int("expect_first_fail_cycle");
        automatic int expect_errors     = cvm_plusargs::get_int("expect_errors");
        automatic int errors            = 0;

        cvm_error_count_start();
        tb_engine_stimulus("top.u_replay", PADDED);

        // Falling edges, so they never race the rising edge the DUT uses.
        repeat (4) @(negedge clk);
        reset_n = 1'b1;
        repeat (2) @(negedge clk);
        enable = 1'b1;

        @(posedge done);
        // The summary lands the cycle after done.
        repeat (2) @(posedge clk);

        if (done !== 1'b1) begin
            $display("FAIL: engine never reported done");
            errors++;
        end
        if (expect_mismatches >= 0 && int'(mismatches) != expect_mismatches) begin
            $display("FAIL: %0d mismatching bit-cycles, wanted %0d",
                     mismatches, expect_mismatches);
            errors++;
        end
        if (expect_mismatches < 0 && mismatches == 32'd0) begin
            $display("FAIL: expected mismatches and saw none");
            errors++;
        end
        if (expect_first_fail >= 0 && int'(first_fail_cycle) != expect_first_fail) begin
            $display("FAIL: first failure at cycle %0d, wanted %0d; the alignment depth is wrong",
                     first_fail_cycle, expect_first_fail);
            errors++;
        end
        if (cvm_error_count() != expect_errors) begin
            $display("FAIL: %0d ERROR reports, wanted %0d",
                     cvm_error_count(), expect_errors);
            errors++;
        end

        $display("mismatches=%0d first_fail_cycle=%0d fail_bits=%08x errors_logged=%0d",
                 mismatches, first_fail_cycle, fail_bits, cvm_error_count());

        if (errors != 0) $fatal(1, "%0d checks failed", errors);
        $display("PASS");
        $finish;
    end

endmodule
