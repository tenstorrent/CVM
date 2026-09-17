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

    logic             rst_n_outer, valid_outer;
    logic [3:0]       opa_outer, opb_outer;
    logic [4:0]       result_outer;
    alu_pkg::bundle_t cmd_outer;
    logic [alu_pkg::LANES-1:0][3:0] mat_outer;
    logic [$clog2(alu_pkg::LANES*4)-1:0] sel_outer, sel_echo_outer;
    alu_pkg::lane_t   resp_outer;
    logic [7:0]       sum_outer;
    logic             bus_echo_outer;
`ifdef FEAT_GATE
    logic             gate_outer, gate_echo_outer;
`else
    logic             no_gate_outer;
`endif

    logic             rst_n_inner, valid_inner;
    logic [3:0]       opa_inner, opb_inner;
    logic [4:0]       result_inner;
    alu_pkg::bundle_t cmd_inner;
    logic [alu_pkg::LANES-1:0][3:0] mat_inner;
    logic [$clog2(alu_pkg::LANES*4)-1:0] sel_inner, sel_echo_inner;
    alu_pkg::lane_t   resp_inner;
    logic [7:0]       sum_inner;
    logic             bus_echo_inner;
    // One net for the inout, connected to the interposer and to the DUT, which
    // is what joins them. Deliberately undriven here: the recording owns the
    // bits it says the outside drove, and the DUT owns the rest.
    wire  [2:0]       bus;
`ifdef FEAT_GATE
    logic             gate_inner, gate_echo_inner;
`else
    logic             no_gate_inner;
`endif

    logic [7:0] mon_edges;
    logic [4:0] mon_last_result;
    logic [4:0] mon_replay_result;
    // Read back through the struct, which shows the flattening put fields
    // where the DUT expects them.
    alu_pkg::lane_t mon_resp;
    logic [7:0]     mon_sum;
    logic           mon_bus_echo;
    logic [2:0]     mon_bus;
`ifdef FEAT_GATE
    logic           mon_gate_echo;
`endif
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
        .rst_n_outer (rst_n_outer),  .rst_n_inner (rst_n_inner),
        .opa_outer   (opa_outer),    .opa_inner   (opa_inner),
        .opb_outer   (opb_outer),    .opb_inner   (opb_inner),
        .cmd_outer   (cmd_outer),    .cmd_inner   (cmd_inner),
        .mat_outer   (mat_outer),    .mat_inner   (mat_inner),
        .sel_outer   (sel_outer),    .sel_inner   (sel_inner),
`ifdef FEAT_GATE
        .gate_outer  (gate_outer),   .gate_inner  (gate_inner),
        .gate_echo_inner (gate_echo_inner), .gate_echo_outer (gate_echo_outer),
`else
        .no_gate_outer (no_gate_outer), .no_gate_inner (no_gate_inner),
`endif
        .result_inner (result_inner), .result_outer (result_outer),
        .valid_inner  (valid_inner),  .valid_outer  (valid_outer),
        .resp_inner   (resp_inner),   .resp_outer   (resp_outer),
        .sum_inner    (sum_inner),    .sum_outer    (sum_outer),
        .sel_echo_inner (sel_echo_inner), .sel_echo_outer (sel_echo_outer),
        .bus          (bus),
        .bus_echo_inner (bus_echo_inner),  .bus_echo_outer (bus_echo_outer)
    );

    alu u_dut (
        .clk    (clk),
        .rst_n  (rst_n_inner),
        .opa    (opa_inner),
        .opb    (opb_inner),
        .cmd    (cmd_inner),
        .mat    (mat_inner),
        .sel    (sel_inner),
`ifdef FEAT_GATE
        .gate      (gate_inner),
        .gate_echo (gate_echo_inner),
`else
        .no_gate   (no_gate_inner),
`endif
        .result (result_inner),
        .valid  (valid_inner),
        .resp   (resp_inner),
        .sum    (sum_inner),
        .sel_echo (sel_echo_inner),
        .bus      (bus),
        .bus_echo (bus_echo_inner)
    );

    // Ignored in REPLAY until it finishes; drives in BYPASS.
    initial begin
        reset_n  = 1'b0;
        enable   = 1'b0;
        rst_n_outer = 1'b0;
        // Matches cycle 0 of the recordings, so the outputs standing during the
        // first recorded cycle are the ones the recording assumes.
        opa_outer   = 4'd1;
        opb_outer   = 4'd2;
        cmd_outer   = '0;
        mat_outer   = '0;
        sel_outer   = '0;
`ifdef FEAT_GATE
        // Low, so a gate_echo still high after replay would mean the recording
        // never handed the port back.
        gate_outer  = 1'b0;
`else
        no_gate_outer = 1'b0;
`endif
    end

    // Knows nothing of the mode, and must see traffic either way.
    always_ff @(posedge clk) begin
        if (mon_edges !== 8'hFF) mon_edges <= mon_edges + 8'd1;
        if (valid_inner === 1'b1) begin
            mon_last_result <= result_inner;
            // Only while replay runs, so it is what the recording produced.
            if (done !== 1'b1) mon_replay_result <= result_inner;
        end
        // After done the testbench drives zeros, so these stop being what the
        // recording produced.
        if (done !== 1'b1) begin
            mon_resp <= resp_inner;
            mon_sum  <= sum_inner;
            mon_bus_echo <= bus_echo_inner;
            mon_bus      <= bus;
`ifdef FEAT_GATE
            mon_gate_echo <= gate_echo_inner;
`endif
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
        mon_bus_echo      = 1'b0;
        mon_bus           = 3'b000;
`ifdef FEAT_GATE
        mon_gate_echo     = 1'b0;
`endif

        // The interposer loads out of reset, so reset precedes enable.
        repeat (4) @(negedge clk);
        reset_n  = 1'b1;
        rst_n_outer = 1'b1;
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
`ifdef FEAT_GATE
        // The conditional port pair, end to end: the recording drove gate and
        // the DUT echoed it. Under the same build without the define neither
        // port exists, in the DUT or in the interposer.
        if (expect_replay_result >= 0 && mon_gate_echo !== 1'b1) begin
            $display("FAIL: gate_echo=%0b under replay, wanted 1", mon_gate_echo);
            errors++;
        end
        if (gate_echo_inner !== 1'b0) begin
            $display("FAIL: gate_echo=%0b after replay, wanted 0; the testbench should drive gate again",
                     gate_echo_inner);
            errors++;
        end
`endif
        // The inout, all three of its cases at once. Bit 0 is the recording's,
        // and bus_echo proves the DUT read what was driven onto it. Bit 1 is
        // the DUT's, so the interposer had to release it for the DUT's value to
        // appear at all -- and it is checked, which a held bit could not be.
        // Bit 2 is nobody's, and must produce no mismatch.
        if (expect_replay_result >= 0) begin
            if (mon_bus_echo !== 1'b1) begin
                $display("FAIL: bus_echo=%0b under replay, wanted 1; the DUT should have read the driven bit",
                         mon_bus_echo);
                errors++;
            end
            if (mon_bus[1] !== 1'b1) begin
                $display("FAIL: bus[1]=%0b under replay, wanted 1; the DUT drives it, so the interposer must release it",
                         mon_bus[1]);
                errors++;
            end
            // Bit 2, which nobody drives, is not asserted on here: what an
            // all-high-Z net reads back as is the simulator's business, and
            // one of them resolves it to 0. That it is never *checked* is what
            // matters, and expect_errors=0 above is that check.
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
