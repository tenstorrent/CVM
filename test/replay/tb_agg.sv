// SPDX-FileCopyrightText: © 2026 Tenstorrent USA, Inc.
// SPDX-License-Identifier: Apache-2.0

// Replay through an interposer whose ports are packed aggregates. The point is
// that nothing special is needed: its ports are plain vectors of the right
// width and the aggregate signals connect straight to them.
module top;

    import cvm_sim_pkg::*;

    localparam int TAIL_CYCLES    = 10;
    localparam int TIMEOUT_CYCLES = 1000;

    logic clk = 1'b0;
    always #5 clk = ~clk;

    logic reset_n, enable, done;

    logic             rst_n_tb, rst_n_dut;
    agg_pkg::bundle_t cmd_tb, cmd_dut;
    logic [1:0][3:0]  mat_tb, mat_dut;
    agg_pkg::lane_t   resp_tb, resp_dut;
    logic [7:0]       sum_tb, sum_dut;

    int cycles, mon_done_cycles;
    logic [7:0]     mon_edges;
    agg_pkg::lane_t mon_resp;
    logic [7:0]     mon_sum;

    localparam cvm_topology_gen::topology_t topo = cvm_topology_gen::mods;

    agg_replay #(
        .HIER     ("top.u_replay"),
        .LOCATION (cvm_topology_gen::get_location(topo.TOP.REPLAY.ID, 0))
    ) u_replay (
        .clk      (clk),
        .reset_n  (reset_n),
        .enable   (enable),
        .done     (done),
        .rst_n_tb (rst_n_tb), .rst_n_dut (rst_n_dut),
        .cmd_tb   (cmd_tb),   .cmd_dut   (cmd_dut),
        .mat_tb   (mat_tb),   .mat_dut   (mat_dut),
        .resp_dut (resp_dut), .resp_tb   (resp_tb),
        .sum_dut  (sum_dut),  .sum_tb    (sum_tb)
    );

    agg u_dut (
        .clk   (clk),
        .rst_n (rst_n_dut),
        .cmd   (cmd_dut),
        .mat   (mat_dut),
        .resp  (resp_dut),
        .sum   (sum_dut)
    );

    initial begin
        reset_n  = 1'b0;
        enable   = 1'b0;
        rst_n_tb = 1'b0;
        cmd_tb   = '0;
        mat_tb   = '0;
    end

    // A passive monitor on the DUT side, which knows nothing about the mode.
    // Captured while replay runs: after done the testbench drives zeros, so
    // the live outputs are no longer what the recording produced.
    always_ff @(posedge clk) begin
        if (mon_edges !== 8'hFF) mon_edges <= mon_edges + 8'd1;
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
        automatic int expect_errors      = cvm_plusargs::get_int("expect_errors");
        automatic int expect_done_cycles = cvm_plusargs::get_int("expect_done_cycles");
        automatic int errors             = 0;

        cvm_error_count_start();
        mon_edges = 8'd0;
        mon_resp  = '0;
        mon_sum   = 8'd0;

        repeat (4) @(negedge clk);
        reset_n  = 1'b1;
        rst_n_tb = 1'b1;
        repeat (2) @(negedge clk);
        enable = 1'b1;

        @(posedge done);
        repeat (TAIL_CYCLES) @(posedge clk);

        if (done !== 1'b1) begin
            $display("FAIL: the interposer should have reported done");
            errors++;
        end
        // 5 + 4 = 9 with hdr == 0, so valid is low. Read back through the
        // struct, which is what shows the flattening put fields where the DUT
        // expects them.
        if (mon_resp.data !== 8'd9 || mon_resp.valid !== 1'b0) begin
            $display("FAIL: resp.data=%0d valid=%0b, wanted 9 and 0",
                     mon_resp.data, mon_resp.valid);
            errors++;
        end
        if (mon_sum !== 8'd7) begin
            $display("FAIL: sum=%0d, wanted 7", mon_sum);
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

        $display("done_cycles=%0d resp.data=%0d resp.valid=%0b sum=%0d",
                 mon_done_cycles, mon_resp.data, mon_resp.valid, mon_sum);
        if (errors != 0) $fatal(1, "%0d checks failed", errors);
        $display("PASS");
        $finish;
    end

endmodule
