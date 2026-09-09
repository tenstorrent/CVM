// SPDX-FileCopyrightText: © 2026 Tenstorrent USA, Inc.
// SPDX-License-Identifier: Apache-2.0

// Clock, stimulus and checks live here so this runs on any simulator that can
// elaborate it; the wrapper only advances time. DEPTH is far below the element
// count, so the run cannot complete without repeatedly refilling.
//
// Scenarios run off one build, switched by plusarg: pushes landing normally,
// pushes suppressed so everything comes through relief, and nothing queued up
// front so everything comes from a producer. Delivery must be identical.
module top;

    localparam int ELEMENTS = 500;
    localparam int WIDTH    = 96;
    localparam int WORDS    = WIDTH / 32;
    localparam int TIMEOUT  = 200000;

    // Element i is {i, ~i, i+1}, so a checker can tell a whole element from a
    // partly delivered one.
    import "DPI-C" function void tb_pipe_stimulus(
        string name, int elements, int words_per_element);

    // Elements still queued on the host.
    import "DPI-C" function int tb_pipe_pending(string name);

    // The one delay: a clock has to come from somewhere. On a platform that
    // supplies its own, this is the only line that changes.
    logic clk = 1'b0;
    always #5 clk = ~clk;

    logic reset_n = 1'b0;

    logic             valid, pop, eos;
    logic [WIDTH-1:0] data;
    logic [31:0]      requests, reliefs, min_occupancy;

    cvm_pipe_in #(
        .NAME  ("tb.u_pipe"),
        .WIDTH (WIDTH),
        .DEPTH (64)
    ) u_pipe (
        .clk             (clk),
        .reset_n         (reset_n),
        .valid           (valid),
        .data            (data),
        .pop             (pop),
        .eos             (eos),
        .requests        (requests),
        .reliefs         (reliefs),
        .min_occupancy_o (min_occupancy)
    );

    assign pop = valid;

    logic [31:0] got;
    logic        order_ok, whole_ok, saw_eos;

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
                // the same cycle.
                if (data[63:32] != ~got || data[95:64] != got + 32'd1)
                    whole_ok <= 1'b0;
                got <= got + 32'd1;
            end
            if (eos) saw_eos <= 1'b1;
        end
    end

    initial begin
        automatic int           errors = 0;
        automatic byte unsigned suppress [1024];
        automatic bit           relief_only;

        // cvm_plusargs, not $test$plusargs: this reads the same declaration
        // the pipe does, so an unknown name fails instead of reading as "not
        // set" and silently running the wrong mode.
        cvm_plusargs::get_string_bytes_1024("cvm_pipe_suppress_push", suppress);
        relief_only = suppress[0] != 8'd0;

        tb_pipe_stimulus("tb.u_pipe", ELEMENTS, WORDS);

        repeat (4) @(posedge clk);
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
        if (got != ELEMENTS) begin
            $display("FAIL: delivered %0d of %0d elements", got, ELEMENTS);
            errors++;
        end
        if (tb_pipe_pending("tb.u_pipe") != 0) begin
            $display("FAIL: %0d elements left queued on the host",
                     tb_pipe_pending("tb.u_pipe"));
            errors++;
        end

        // Guards against going vacuous: a larger DEPTH would deliver
        // everything at once. The paths refill at different rates.
        if (relief_only) begin
            if (reliefs <= 5) begin
                $display("FAIL: only %0d relief fetches; this run no longer tests the path",
                         reliefs);
                errors++;
            end
        end else begin
            if (requests <= 10) begin
                $display("FAIL: only %0d requests; this run no longer tests the protocol",
                         requests);
                errors++;
            end
            // Without this the relief path is dead code here: a push always
            // lands before the watermark.
            if (reliefs != 0) begin
                $display("FAIL: %0d relief fetches although every push landed in time",
                         reliefs);
                errors++;
            end
        end

        $display("elements=%0d requests=%0d reliefs=%0d min_occupancy=%0d",
                 got, requests, reliefs, min_occupancy);

        if (errors != 0) $fatal(1, "%0d checks failed", errors);
        $display("PASS");
        $finish;
    end

    initial begin
        repeat (TIMEOUT) @(posedge clk);
        $fatal(1, "timeout after %0d cycles with %0d of %0d elements delivered",
               TIMEOUT, got, ELEMENTS);
    end

endmodule
