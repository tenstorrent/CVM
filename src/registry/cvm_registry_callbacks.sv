// SPDX-FileCopyrightText: © 2026 Tenstorrent USA, Inc.
// SPDX-License-Identifier: Apache-2.0

// Drives the framework's queued callbacks from the design's own clock, as
// rv_tester does. The host queues work there and something has to run it;
// doing it here rather than in a host-side loop keeps that true on any
// simulator.
//
// Instantiate once per testbench, on the clock the design runs on.
module cvm_registry_callbacks #(
    // Which edge runs the queue. The negedge settles a callback's writes before
    // the posedge a design samples on; a platform that cannot schedule on the
    // negedge sets this instead.
    parameter bit ON_POSEDGE = 1'b0
) (
    input logic clk,
    input logic reset_n
);

    // Returns a value so a simulator cannot reorder it around the design's own
    // writes.
    import "DPI-C" function byte unsigned cvm_registry_flush_callbacks();

    logic opened, poll;

    always_ff @(posedge clk) begin
        if (!reset_n) begin
            opened <= 1'b0;
            poll   <= 1'b0;
        end else if (!opened) begin
            opened <= 1'b1;
            // Under cb_async a worker thread owns the queue and holds the flush
            // lock for the whole run, so polling as well would block here until
            // the end of simulation.
            poll   <= cvm_plusargs::get_bool("cb_async") == 8'd0;
        end
    end

    generate
        if (ON_POSEDGE) begin : g_posedge
            always @(posedge clk) begin
                if (poll) void'(cvm_registry_flush_callbacks());
            end
        end else begin : g_negedge
            always @(negedge clk) begin
                if (poll) void'(cvm_registry_flush_callbacks());
            end
        end
    endgenerate

endmodule
