// SPDX-FileCopyrightText: © 2026 Tenstorrent USA, Inc.
// SPDX-License-Identifier: Apache-2.0

// Scenarios run off one build, switched by plusarg: credits every element,
// credits returned coarsely so demand has to do more, and a producer that runs
// dry without finishing so the retry status is exercised. Delivery must be
// identical in all three.
module top;

    import cvm_sim_pkg::*;

    localparam int TOTAL             = 500;
    localparam type T                = logic[100-1:0];
    // Deliberately not a multiple of 32: every replay width is, so this is the
    // only cover for the push export's partial-top-word path.
    localparam int WIDTH             = $bits(T);
    localparam int WORDS_PER_ELEMENT = (WIDTH + 31) / 32;
    localparam int TIMEOUT           = 200000;

    localparam cvm_topology_gen::topology_t topo = cvm_topology_gen::mods;
    localparam int unsigned LOCATION =
        cvm_topology_gen::get_location(topo.TOP.PIPE.ID, 0);

    // Element i is {i, ~i, i+1}, so a checker can tell a whole element from a
    // partly delivered one.
    import "DPI-C" function void tb_pipe_stimulus(
        int unsigned location, int total, int words_per_element);
    import "DPI-C" function int tb_pipe_pending(int unsigned location);
    import "DPI-C" function int tb_pipe_expect_min_demands();
    import "DPI-C" function int tb_pipe_expect_retries();
    import "DPI-C" function int tb_pipe_expect_min_credits();
    import "DPI-C" function int tb_pipe_expect_max_demands();

    // The one delay: a clock has to come from somewhere.
    logic clk = 1'b0;
    always #5 clk = ~clk;

    logic reset_n = 1'b0;

    logic        valid, pop, eos;
    T            data;
    logic [31:0] credit_calls, demands, demand_retries, min_occupancy;

    logic [31:0] got;
    logic        order_ok, whole_ok, saw_eos;

    // Nothing is pending only once the stream has ended, so demand stays on
    // until then.
    logic demand_en;
    assign demand_en = !saw_eos;

    cvm_registry_callbacks u_cb (.clk(clk), .reset_n(reset_n));

    cvm_pipe_in #(
        .LOCATION (LOCATION),
        .T        (T),
        .DEPTH    (64)
    ) u_pipe (
        .clk             (clk),
        .reset_n         (reset_n),
        .valid           (valid),
        .data            (data),
        .pop             (pop),
        .eos             (eos),
        .demand_en       (demand_en),
        .credit_calls    (credit_calls),
        .demands         (demands),
        .demand_retries  (demand_retries),
        .min_occupancy_o (min_occupancy)
    );

    assign pop = valid;

    always_ff @(posedge clk) begin
        if (!reset_n) begin
            got      <= '0;
            order_ok <= 1'b1;
            whole_ok <= 1'b1;
            saw_eos  <= 1'b0;
        end else begin
            if (valid && pop) begin
                if (data[31:0] != got) order_ok <= 1'b0;
                // The trailing words show whether the whole element landed on
                // the same cycle; the top one is partial.
                if (data[63:32] != ~got || data[95:64] != got + 32'd1 ||
                    data[99:96] != 4'(got + 32'd1))
                    whole_ok <= 1'b0;
                got <= got + 32'd1;
            end
            if (eos) saw_eos <= 1'b1;
        end
    end

    initial begin
        repeat (TIMEOUT) @(posedge clk);
        $fatal(1, "timeout after %0d cycles with %0d of %0d elements delivered",
               TIMEOUT, got, TOTAL);
    end

    initial begin
        automatic int errors = 0;

        cvm_error_count_start();
        tb_pipe_stimulus(LOCATION, TOTAL, WORDS_PER_ELEMENT);

        repeat (4) @(negedge clk);
        reset_n = 1'b1;

        @(posedge saw_eos);
        @(posedge clk);

        if (!order_ok) begin
            $display("FAIL: elements arrived out of order or duplicated");
            errors++;
        end
        if (!whole_ok) begin
            $display("FAIL: a multi-word element did not arrive whole in one cycle");
            errors++;
        end
        if (got != TOTAL) begin
            $display("FAIL: delivered %0d of %0d elements", got, TOTAL);
            errors++;
        end
        if (tb_pipe_pending(LOCATION) != 0) begin
            $display("FAIL: %0d elements left queued on the host",
                     tb_pipe_pending(LOCATION));
            errors++;
        end
        // Guards against going vacuous: a larger DEPTH would deliver everything
        // at once, and credit has to actually be returned for the queue to
        // refill at all.
        // Delivery can run on demand alone, so this is per scenario rather
        // than a blanket requirement.
        if (int'(credit_calls) < tb_pipe_expect_min_credits()) begin
            $display("FAIL: %0d credit returns, wanted at least %0d",
                     credit_calls, tb_pipe_expect_min_credits());
            errors++;
        end
        if (int'(demands) < tb_pipe_expect_min_demands()) begin
            $display("FAIL: only %0d demands, wanted at least %0d; this run no longer tests the path",
                     demands, tb_pipe_expect_min_demands());
            errors++;
        end
        // What makes the credit path load bearing: if credit is refilling the
        // queue, demand should hardly fire. Without this, ignoring credit
        // entirely still passes, because demand reports the read pointer too.
        if (tb_pipe_expect_max_demands() != 0 &&
                int'(demands) > tb_pipe_expect_max_demands()) begin
            $display("FAIL: %0d demands, wanted at most %0d; credit is not doing the refilling",
                     demands, tb_pipe_expect_max_demands());
            errors++;
        end
        // A dry producer is the only way to reach the retry status, so a run
        // that claims to test it has to show one.
        if (tb_pipe_expect_retries() != 0 && demand_retries == 0) begin
            $display("FAIL: no demand was ever answered with a retry");
            errors++;
        end
        if (cvm_error_count() != 0) begin
            $display("FAIL: %0d ERROR reports", cvm_error_count());
            errors++;
        end

        $display("elements=%0d credits=%0d demands=%0d retries=%0d min_occupancy=%0d",
                 got, credit_calls, demands, demand_retries, min_occupancy);
        if (errors != 0) $fatal(1, "%0d checks failed", errors);
        $display("PASS");
        $finish;
    end

endmodule
