// SPDX-FileCopyrightText: © 2026 Tenstorrent USA, Inc.
// SPDX-License-Identifier: Apache-2.0

// Replay of a DUT buried in a larger design, with the design untouched. The
// harness is an ordinary module instantiated here: it reaches into core by
// hierarchical name and takes the boundary with force, so the testbench owns
// LOCATION, reset, enable and done as ordinary parameters and ports.
module top;

    import cvm_sim_pkg::*;

    localparam int TAIL_CYCLES    = 10;
    localparam int TIMEOUT_CYCLES = 1000;

    logic clk = 1'b0;
    always #5 clk = ~clk;

    logic tb_reset_n, tb_enable, tb_done;

    logic                       rst_n, no_gate;
    logic [3:0]                 opa, opb;
    alu_pkg::bundle_t           cmd;
    logic [alu_pkg::LANES-1:0][3:0] mat;
    logic [$clog2(alu_pkg::LANES*4)-1:0] sel, sel_echo;
    wire  [2:0]                 bus;
    logic                       bus_echo, valid;
    logic [4:0]                 result, result_plain;
    alu_pkg::lane_t             resp;
    alu_aux_pkg::byte_t         sum;

    // Drives the inout only when replay will not, so the untouched run has
    // something real to carry: alu echoes bus[0] onto bus[1].
    logic drive_bus;
    assign bus[0] = drive_bus ? 1'b1 : 1'bz;

    logic [4:0] mon_replay_result, mon_last_result, mon_plain_result;
    int         mon_done_cycles, cycles;

    localparam cvm_topology_gen::topology_t topo = cvm_topology_gen::mods;

    cvm_registry_callbacks u_cb (.clk(clk), .reset_n(tb_reset_n));

    core u_core (
        .clk(clk), .rst_n(rst_n), .opa(opa), .opb(opb), .cmd(cmd), .mat(mat),
        .sel(sel), .no_gate(no_gate), .bus(bus), .bus_echo(bus_echo),
        .result(result), .valid(valid), .resp(resp), .sum(sum),
        .sel_echo(sel_echo), .result_plain(result_plain)
    );

    // Points at top.u_core.u_alu, which is generated into it. Nothing about
    // core or alu changes to accommodate this.
    alu_replay #(
        .LOCATION (cvm_topology_gen::get_location(topo.TOP.REPLAY.ID, 0))
    ) u_replay (
        .clk     (clk),
        .reset_n (tb_reset_n),
        .enable  (tb_enable),
        .done    (tb_done)
    );

    initial begin
        tb_reset_n = 1'b0;
        tb_enable  = 1'b0;
        rst_n      = 1'b0;
        opa        = 4'd1;
        opb        = 4'd2;
        cmd        = '0;
        mat        = '0;
        sel        = '0;
        no_gate    = 1'b1;
    end

    always_ff @(posedge clk) begin
        if (valid === 1'b1) begin
            mon_last_result <= result;
            if (tb_done !== 1'b1) mon_replay_result <= result;
        end
        // Only while replay runs, so it shows the sibling was never forced
        // rather than what the design drove afterwards.
        if (tb_done !== 1'b1) mon_plain_result <= result_plain;
    end

    always_ff @(posedge clk) begin
        if (!tb_enable) begin
            cycles          <= 0;
            mon_done_cycles <= -1;
        end else begin
            cycles <= cycles + 1;
            if (tb_done === 1'b1 && mon_done_cycles < 0) mon_done_cycles <= cycles;
        end
    end

    initial begin
        repeat (TIMEOUT_CYCLES) @(posedge clk);
        $fatal(1, "timeout after %0d cycles", TIMEOUT_CYCLES);
    end

    initial begin
        automatic int expect_errors        = cvm_plusargs::get_int("expect_errors");
        automatic int expect_replay_result = cvm_plusargs::get_int("expect_replay_result");
        automatic int expect_last_result   = cvm_plusargs::get_int("expect_last_result");
        automatic int expect_done_cycles   = cvm_plusargs::get_int("expect_done_cycles");
        automatic int skip_enable          = cvm_plusargs::get_int("skip_enable");
        automatic int errors               = 0;

        cvm_error_count_start();
        drive_bus         = (skip_enable != 0);
        mon_replay_result = 5'd0;
        mon_last_result   = 5'd0;
        mon_plain_result  = 5'd0;

        repeat (4) @(negedge clk);
        tb_reset_n = 1'b1;
        rst_n      = 1'b1;
        repeat (2) @(negedge clk);

        if (skip_enable == 0) begin
            tb_enable = 1'b1;
            @(posedge tb_done);
            // Release restores a port to its driver's *next* evaluation, so
            // the design has to drive for the block to come back. Moving the
            // stimulus here is what a live design does every cycle anyway.
            opa = 4'd4;
            opb = 4'd5;
        end
        repeat (TAIL_CYCLES) @(posedge clk);

        if (expect_replay_result >= 0 &&
                int'(mon_replay_result) != expect_replay_result) begin
            $display("FAIL: buried DUT produced %0d under replay, recording says %0d",
                     mon_replay_result, expect_replay_result);
            errors++;
        end
        if (expect_last_result >= 0 &&
                int'(mon_last_result) != expect_last_result) begin
            $display("FAIL: last result %0d, wanted %0d; after replay the design should drive again",
                     mon_last_result, expect_last_result);
            errors++;
        end
        // Nothing was forced, so the design's inout runs design -> block ->
        // design exactly as it did before the harness existed.
        if (skip_enable != 0 && (bus[1] !== 1'b1 || bus_echo !== 1'b1)) begin
            $display("FAIL: bus[1]=%0b bus_echo=%0b, wanted both 1; an unforced design should be untouched",
                     bus[1], bus_echo);
            errors++;
        end
        // The DUT is at the path it always was -- force needed no interposing,
        // so nothing moved.
        if (u_core.u_alu.result !== result) begin
            $display("FAIL: core.u_alu.result=%0d but core.result=%0d",
                     u_core.u_alu.result, result);
            errors++;
        end
        // The sibling instance is on the same stimulus and was never forced.
        if (int'(mon_plain_result) != 3) begin
            $display("FAIL: the unforced alu produced %0d, wanted 3; force should reach one instance",
                     mon_plain_result);
            errors++;
        end
        if (expect_done_cycles >= 0 && mon_done_cycles != expect_done_cycles) begin
            $display("FAIL: replay finished after %0d cycles, wanted %0d",
                     mon_done_cycles, expect_done_cycles);
            errors++;
        end
        if (cvm_error_count() != expect_errors) begin
            $display("FAIL: %0d ERROR reports, wanted %0d",
                     cvm_error_count(), expect_errors);
            errors++;
        end

        $display("skip_enable=%0d done_cycles=%0d replay_result=%0d last_result=%0d plain=%0d",
                 skip_enable, mon_done_cycles, mon_replay_result, mon_last_result,
                 mon_plain_result);
        if (errors != 0) $fatal(1, "%0d checks failed", errors);
        $display("PASS");
        $finish;
    end

endmodule
