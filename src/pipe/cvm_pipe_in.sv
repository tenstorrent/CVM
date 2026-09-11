// SPDX-FileCopyrightText: © 2026 Tenstorrent USA, Inc.
// SPDX-License-Identifier: Apache-2.0

// Buffered host-to-HDL stream over plain DPI. The payload is opaque: WIDTH bits
// per element, one element available per cycle.
//
// Flow control is credit based. RTL owns `rptr` and reports it; the host owns
// `q` and `wptr_nxt` and pushes when it has room. Two calls out:
//
//   credits  the read pointer moved. void, so the clock keeps running.
//   demand   the queue is low and `demand_en` says that matters. Returns a
//            status, so this is the only call that stalls the clock.
//
// One always block per DPI caller, because a platform scopes a clock stall to
// the block causing it; the datapath block calls nothing and reports nothing.
//
// Every storage element has one writer, which some platforms enforce by
// silently dropping the others: the host owns `q`, `wptr_nxt` and the error
// flags, so even reset goes through the export rather than from RTL.

// One export per formal size, each with its own name, so a tool sees a distinct
// signature and only the size this instance needs is elaborated. No comments
// inside the body: a `//` would swallow the line continuation.
`define CVM_PIPE_PUSH_EXPORT(N) \
    function void cvm_pipe_in_push_``N( \
        input int  unsigned count, \
        input int  unsigned data_words[N], \
        input byte unsigned is_last); \
        if (reset_n) begin \
            if (int'(ptr_t'(wptr_nxt - rptr)) + int'(count) > DEPTH) \
                overrun = 1'b1; \
            for (int i = 0; i < int'(count); i++) begin \
                automatic idx_t w = idx_t'((wptr_nxt + ptr_t'(i)) % ptr_t'(DEPTH)); \
                for (int b = 0; b < WIDTH; b++) begin \
                    q[w][b] = data_words[i * WORDS + (b / 32)][b % 32]; \
                end \
            end \
            wptr_nxt    = wptr_nxt + ptr_t'(count); \
            last_pushed = (is_last != 8'd0); \
        end \
    endfunction \
    export "DPI-C" function cvm_pipe_in_push_``N;

module cvm_pipe_in #(
    // Topology location: identity for the host side, the callbacks scope and
    // plusarg keying.
    parameter int unsigned LOCATION = cvm_topology::nil,
    // Payload bits per element.
    parameter int          WIDTH = 32,
    // Elements held. Sizes a memory, so a parameter not a plusarg.
    parameter int          DEPTH = 4096,
    // Most elements one push may carry.
    parameter int          EPOCH_MAX_ELEMENTS = 1024
) (
    input  logic clk,
    input  logic reset_n,

    output logic             valid,
    output logic [WIDTH-1:0] data,
    input  logic             pop,
    output logic             eos,

    // An empty queue is only a problem when the consumer says it is; sometimes
    // there is simply nothing pending.
    input  logic demand_en,

    // Margin a run actually had. Demands mean it ran correctly but slowly.
    output logic [31:0] credit_calls,
    output logic [31:0] demands,
    // Demands the host answered with "not now". Distinguishes a transport that
    // is merely being polled from one that is genuinely waiting on a producer.
    output logic [31:0] demand_retries,
    output logic [31:0] min_occupancy_o
);

    import cvm_pipe_pkg::*;
    import cvm_topology::*;

    `CVM_REGISTRY_SET_SCOPE(LOCATION)

    localparam int WORDS     = (WIDTH + 31) / 32;
    localparam int PUSH_SLOT = cvm_pipe_slot(EPOCH_MAX_ELEMENTS * WORDS);

    if (PUSH_SLOT == 0)
        $fatal(1, "cvm_pipe_in: EPOCH_MAX_ELEMENTS * WORDS exceeds the largest formal");

    typedef logic [$clog2(DEPTH + 1) - 1:0] ptr_t;
    typedef logic [$clog2(DEPTH) - 1:0]     idx_t;

    // Host-owned.
    logic [WIDTH-1:0] q [DEPTH];
    ptr_t             wptr_nxt;
    logic             last_pushed;
    logic             overrun;

    // RTL-owned.
    logic opened;
    ptr_t rptr, wptr;
    ptr_t min_occupancy;
    logic starved;
    // Latched at setup: these come from plusargs, and a returning import per
    // cycle would stall the clock every cycle.
    int   credit_every, demand_watermark, demand_every;
    int   credit_count, demand_count;
    logic demand_retry;
    // Reported to the host, so it must be what the host can do arithmetic on:
    // a monotonic element count, not the narrow pointer that wraps.
    logic [31:0] rptr_total;

    ptr_t occupancy;
    assign occupancy = wptr - rptr;

    assign valid = occupancy != '0;
    assign data  = q[idx_t'(rptr % ptr_t'(DEPTH))];

    // `wptr` lags the host-owned `wptr_nxt` by a cycle, so a final push into an
    // empty queue would otherwise assert eos before its own data was visible.
    assign eos = last_pushed && (occupancy == '0) && (wptr == wptr_nxt);

    assign min_occupancy_o = 32'(min_occupancy);

    // Only the size this instance needs is elaborated.
    generate
        case (PUSH_SLOT)
            8:     begin : g_push `CVM_PIPE_PUSH_EXPORT(8)     end
            32:    begin : g_push `CVM_PIPE_PUSH_EXPORT(32)    end
            128:   begin : g_push `CVM_PIPE_PUSH_EXPORT(128)   end
            512:   begin : g_push `CVM_PIPE_PUSH_EXPORT(512)   end
            2048:  begin : g_push `CVM_PIPE_PUSH_EXPORT(2048)  end
            8192:  begin : g_push `CVM_PIPE_PUSH_EXPORT(8192)  end
            32768: begin : g_push `CVM_PIPE_PUSH_EXPORT(32768) end
        endcase
    endgenerate

    function void cvm_pipe_in_zero();
        wptr_nxt    = '0;
        last_pushed = '0;
        overrun     = 1'b0;
    endfunction
    export "DPI-C" function cvm_pipe_in_zero;

    // --- Setup: reports geometry and zeroes the host-owned pointer ---
    always_ff @(posedge clk) begin
        if (!reset_n) begin
            opened           <= 1'b0;
            credit_every     <= 1;
            demand_watermark <= 0;
            demand_every     <= 1;
        end else if (!opened) begin
            automatic int ce = cvm_pipe_credit_every(LOCATION);
            automatic int de = cvm_pipe_demand_every(LOCATION);
            opened           <= 1'b1;
            credit_every     <= (ce > 0) ? ce : 1;
            demand_every     <= (de > 0) ? de : 1;
            demand_watermark <= cvm_pipe_demand_watermark(LOCATION);
            cvm_pipe_reset(LOCATION, DEPTH, WORDS, PUSH_SLOT);
        end
    end

    // --- Credits: void, so this block need not stall ---
    always_ff @(posedge clk) begin
        if (!reset_n) begin
            credit_count <= 0;
            credit_calls <= '0;
            rptr_total   <= '0;
        end else if (opened && valid && pop) begin
            rptr_total <= rptr_total + 32'd1;
            if (credit_count + 1 >= credit_every) begin
                credit_count <= 0;
                credit_calls <= credit_calls + 32'd1;
                // Pre-pop, because the overrun check below runs against the
                // pre-edge `rptr`: reporting the post-pop value would let the
                // host believe in a slot that does not exist yet.
                cvm_pipe_credits(LOCATION, rptr_total);
            end else begin
                credit_count <= credit_count + 1;
            end
        end
    end

    // --- Demand: the only steady-state stall, alone in its own block ---
    always_ff @(posedge clk) begin
        if (!reset_n) begin
            demand_count   <= 0;
            demands        <= '0;
            demand_retries <= '0;
            demand_retry   <= 1'b0;
        end else if (opened && demand_en && !last_pushed &&
                     occupancy <= ptr_t'(demand_watermark)) begin
            if (demand_retry || demand_count + 1 >= demand_every) begin
                automatic int status = cvm_pipe_demand(LOCATION, rptr_total);
                demand_count <= 0;
                demands      <= demands + 32'd1;
                if (status < 0) demand_retries <= demand_retries + 32'd1;
                // -1 is "not now, ask again"; anything else waits out the
                // interval, including 0, which means nothing is pending.
                demand_retry <= status < 0;
            end else begin
                demand_count <= demand_count + 1;
            end
        end else begin
            demand_count <= 0;
            demand_retry <= 1'b0;
        end
    end

    // --- Datapath: no DPI, no reporting, so nothing here forces a stall ---
    always_ff @(posedge clk) begin
        if (!reset_n) begin
            rptr          <= '0;
            wptr          <= '0;
            min_occupancy <= ptr_t'(DEPTH);
            starved       <= 1'b0;
        end else begin
            wptr <= wptr_nxt;
            if (valid && pop) rptr <= rptr + ptr_t'(1);
            if (valid && occupancy < min_occupancy) min_occupancy <= occupancy;
            // Only a fault if the consumer was expecting something.
            if (opened && pop && !valid && demand_en && !last_pushed)
                starved <= 1'b1;
        end
    end

    // --- Report: everything that talks to a console ---
    logic reported_starved, reported_overrun;
    always_ff @(posedge clk) begin
        if (!reset_n) begin
            reported_starved <= 1'b0;
            reported_overrun <= 1'b0;
        end else begin
            if (starved && !reported_starved) begin
                reported_starved <= 1'b1;
                $error("cvm_pipe_in(%0d): starved with demand enabled", LOCATION);
            end
            if (overrun && !reported_overrun) begin
                reported_overrun <= 1'b1;
                $error("cvm_pipe_in(%0d): queue overrun", LOCATION);
            end
        end
    end

endmodule

`undef CVM_PIPE_PUSH_EXPORT
