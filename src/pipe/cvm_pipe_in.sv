// SPDX-FileCopyrightText: © 2026 Tenstorrent USA, Inc.
// SPDX-License-Identifier: Apache-2.0

// Buffered host-to-HDL stream over plain DPI. The payload is opaque: WIDTH
// bits per element, one element available per cycle.
//
// Delivery is push (host, clock runs) with an inline relief fetch when a push
// is late. One always block per DPI caller, because a platform scopes a clock
// stall to the block causing it; the datapath block calls nothing and holds no
// $error, so it never stalls.
//
// Every storage element has one writer, which some platforms enforce by
// silently dropping the others:
//   * C owns `q` and `wptr_nxt` (the push and reset exports), so `wptr_nxt` is
//     reset through cvm_pipe_open calling the reset export, never from RTL
//   * the relief block owns `relief_q`, `relief_wr`, `relief_base` and
//     `last_relieved` -- hence two end-of-stream flags, one per path
//   * the datapath owns `rptr`, `relief_rd` and `wptr <= wptr_nxt`
module cvm_pipe_in #(
    // Identity for per-instance plusarg lookup.
    parameter string NAME  = "",
    // Payload bits per element.
    parameter int    WIDTH = 32,
    // Elements held. Sizes a memory, so a parameter not a plusarg.
    parameter int    DEPTH = 4096,
    // Relief buffer size, in elements not words, so widening the payload does
    // not quietly shrink how many cycles of cover it provides. Costs
    // RELIEF_DEPTH * WIDTH flops.
    parameter int    RELIEF_DEPTH = 8,
    // Most elements one push may carry. The host is told this at open.
    parameter int    EPOCH_MAX_ELEMENTS = 1024
) (
    input  logic clk,
    input  logic reset_n,

    output logic             valid,
    output logic [WIDTH-1:0] data,
    input  logic             pop,
    output logic             eos,

    // Margin a run actually had. Relief fetches mean it ran correctly but
    // slowly.
    output logic [31:0] requests,
    output logic [31:0] reliefs,
    output logic [31:0] min_occupancy_o
);

    import cvm_pipe_pkg::*;

    localparam int WORDS = (WIDTH + 31) / 32;

    if (EPOCH_MAX_ELEMENTS * WORDS > CVM_PIPE_MAX_WORDS)
        $fatal(1, "cvm_pipe_in: EPOCH_MAX_ELEMENTS * WORDS exceeds the push formal; reduce EPOCH_MAX_ELEMENTS");
    if (RELIEF_DEPTH < 1)
        $fatal(1, "cvm_pipe_in: RELIEF_DEPTH must be at least one element");

    typedef logic [$clog2(DEPTH + 1) - 1:0] ptr_t;
    typedef logic [$clog2(DEPTH) - 1:0]     idx_t;

    // C-owned.
    logic [WIDTH-1:0] q [DEPTH];
    ptr_t             wptr_nxt;
    logic             last_pushed;
    logic [7:0]       last_tag;
    logic             last_tag_valid;

    // CFG block.
    logic opened;
    int   handle, epoch_size, headroom, relief_mark;
    logic bad_headroom;

    // REQ block.
    logic [7:0]  requested_epoch;
    logic        requested;
    int          request_age;
    logic [31:0] reliefs_seen;

    // RELIEF block. Words, because that is what the import returns.
    int unsigned relief_q [RELIEF_DEPTH * WORDS];
    // `relief_base` is what `relief_rd` stood at when the batch was fetched, so
    // the datapath can index a slot without writing relief-owned state.
    logic [31:0] relief_wr, relief_base;
    logic        last_relieved;

    // Datapath block.
    ptr_t        rptr, wptr;
    logic [31:0] relief_rd;
    ptr_t        min_occupancy;
    logic        starved;

    // End of stream, whichever path carried it.
    logic last_seen;
    assign last_seen = last_pushed || last_relieved;

    ptr_t occupancy;
    assign occupancy = wptr - rptr;

    ptr_t room;
    assign room = ptr_t'(DEPTH) - occupancy;

    // Relief elements are older than anything in `q`: a fetch is taken only
    // when `q` is empty with nothing landing, so it reads first.
    logic relief_valid;
    assign relief_valid = relief_rd != relief_wr;

    logic [WIDTH-1:0] relief_data;
    always_comb begin
        automatic int slot = int'(relief_rd - relief_base);
        automatic int base = slot * WORDS;
        relief_data = '0;
        for (int b = 0; b < WIDTH; b++) begin
            relief_data[b] = relief_q[base + (b / 32)][b % 32];
        end
    end

    assign valid = relief_valid || (occupancy != '0);
    assign data  = relief_valid ? relief_data : q[idx_t'(rptr % ptr_t'(DEPTH))];

    // `wptr` lags C-owned `wptr_nxt` by a cycle, so a final push into an empty
    // queue would otherwise assert eos before its own data was visible. Only
    // reachable where exports are deferred, so no test covers this term; do not
    // drop it on the strength of that.
    assign eos = last_seen && !relief_valid && (occupancy == '0) &&
                 (wptr == wptr_nxt);

    assign min_occupancy_o = 32'(min_occupancy);

    function void cvm_pipe_in_push(
        input int  unsigned count,
        input int  unsigned data_words[CVM_PIPE_MAX_WORDS],
        input byte unsigned epoch,
        input byte unsigned is_last
    );
        if (reset_n) begin
            // Cannot be flopped out the way the datapath's are: an export
            // body has no clock.
            `ifndef CVM_PIPE_NO_ASSERTS_IN_DPI
            assert (int'(ptr_t'(wptr_nxt - rptr)) + int'(count) <= DEPTH)
                else $error("cvm_pipe_in(%s): queue overrun", NAME);
            // Checked against C-owned state, so it holds however late a push
            // lands. The RTL request counter would race: a push can land in
            // the same evaluation as the request that asked for it.
            assert (!last_tag_valid || epoch != last_tag)
                else $error("cvm_pipe_in(%s): epoch %0d delivered twice",
                            NAME, epoch);
            `endif
            for (int i = 0; i < int'(count); i++) begin
                automatic idx_t w = idx_t'((wptr_nxt + ptr_t'(i)) % ptr_t'(DEPTH));
                for (int b = 0; b < WIDTH; b++) begin
                    q[w][b] = data_words[i * WORDS + (b / 32)][b % 32];
                end
            end
            wptr_nxt       = wptr_nxt + ptr_t'(count);
            last_pushed    = (is_last != 8'd0);
            last_tag       = epoch;
            last_tag_valid = 1'b1;
        end
    endfunction
    export "DPI-C" function cvm_pipe_in_push;

    function void cvm_pipe_in_reset();
        wptr_nxt       = '0;
        last_pushed    = '0;
        last_tag       = '0;
        last_tag_valid = 1'b0;
    endfunction
    export "DPI-C" function cvm_pipe_in_reset;

    // --- CFG: startup only, so its stalls happen once ---
    //
    always_ff @(posedge clk) begin
        if (!reset_n) begin
            opened       <= 1'b0;
            bad_headroom <= 1'b0;
            // Read only once opened, but reset anyway: an X here would do
            // something silently if a condition were ever ungated.
            handle       <= -1;
            epoch_size   <= 0;
            headroom     <= 0;
            relief_mark  <= 0;
        end else if (!opened) begin
            automatic int hr = cvm_pipe_headroom(NAME);
            // At or above DEPTH it can never be exceeded, so the request would
            // latch once and never re-arm.
            if (hr >= DEPTH) begin
                bad_headroom <= 1'b1;
                hr = DEPTH / 2;
            end
            opened      <= 1'b1;
            handle      <= cvm_pipe_open(NAME, EPOCH_MAX_ELEMENTS);
            epoch_size  <= cvm_pipe_epoch_size(NAME);
            headroom    <= hr;
            // Well below the request watermark, so a merely slow push is not
            // treated as a late one.
            relief_mark <= (hr / 4 > 0) ? hr / 4 : 1;
        end
    end

    // --- REQ: the void request, so this block need not stall ---
    always_ff @(posedge clk) begin
        if (!reset_n) begin
            requested       <= 1'b0;
            request_age     <= 0;
            requested_epoch <= '0;
            requests        <= '0;
            reliefs_seen    <= '0;
        end else begin
            request_age <= requested ? request_age + 1 : 0;

            if (opened && !last_seen && !requested &&
                    occupancy <= ptr_t'(headroom)) begin
                requested       <= 1'b1;
                request_age     <= 0;
                requests        <= requests + 32'd1;
                requested_epoch <= requested_epoch + 8'd1;
                cvm_pipe_request(handle, int'(requested_epoch), int'(room));
            end else if (requested && occupancy > ptr_t'(headroom)) begin
                requested <= 1'b0;
            end

            // A relief fetch reclaimed the outstanding push, so the request
            // must be reissued. Observed, not written across blocks.
            if (reliefs != reliefs_seen) begin
                reliefs_seen <= reliefs;
                requested    <= 1'b0;
            end
        end
    end

    // --- RELIEF: the only steady-state stall, alone in its own block ---
    always_ff @(posedge clk) begin
        if (!reset_n) begin
            relief_wr     <= '0;
            relief_base   <= '0;
            reliefs       <= '0;
            last_relieved <= 1'b0;
        end else begin
            // Outstanding longer than headroom was meant to cover, with
            // occupancy nearly gone: the push is late, not merely slow.
            //
            // The age test separates late from not-yet-started -- at cold start
            // the queue is empty before any push could have landed. The
            // `wptr == wptr_nxt` gate matters because a push already landing
            // holds elements older than anything the host would return now.
            if (opened && !last_seen && !relief_valid && requested &&
                    request_age >= headroom &&
                    occupancy <= ptr_t'(relief_mark) && wptr == wptr_nxt) begin
                automatic int unsigned is_last;
                automatic int n = cvm_pipe_relief(handle, RELIEF_DEPTH,
                                                  relief_q, is_last);
                if (n > 0 || is_last != 0) begin
                    // Written from slot zero into an empty buffer, so the
                    // datapath indexes it as `relief_rd - relief_base`.
                    relief_base <= relief_rd;
                    relief_wr   <= relief_rd + 32'(n);
                    reliefs     <= reliefs + 32'd1;
                    if (is_last != 0) last_relieved <= 1'b1;
                end
            end
        end
    end

    // --- Datapath: no DPI, no $error, so nothing here forces a stall ---
    always_ff @(posedge clk) begin
        if (!reset_n) begin
            rptr          <= '0;
            wptr          <= '0;
            relief_rd     <= '0;
            min_occupancy <= ptr_t'(DEPTH);
            starved       <= 1'b0;
        end else begin
            wptr <= wptr_nxt;

            if (valid && pop) begin
                if (relief_valid) relief_rd <= relief_rd + 32'd1;
                else              rptr      <= rptr + ptr_t'(1);
            end
            if (valid && occupancy < min_occupancy) min_occupancy <= occupancy;

            // With relief in place, this is producer starvation rather than a
            // transport shortfall. Flopped, not printed: a $display here routes
            // to the host on some platforms, which is the stall this block
            // exists to avoid.
            if (opened && pop && !valid && !last_seen) starved <= 1'b1;
        end
    end

    // --- REPORT: everything that talks to a console, out of the datapath ---
    logic starved_reported, bad_headroom_reported;
    always_ff @(posedge clk) begin
        if (!reset_n) begin
            starved_reported      <= 1'b0;
            bad_headroom_reported <= 1'b0;
        end else begin
            if (bad_headroom && !bad_headroom_reported) begin
                bad_headroom_reported <= 1'b1;
                $error("cvm_pipe_in(%s): headroom >= DEPTH %0d, using %0d",
                       NAME, DEPTH, DEPTH / 2);
            end
            if (starved && !starved_reported) begin
                starved_reported <= 1'b1;
                $error("cvm_pipe_in(%s): starved, host has no elements queued",
                       NAME);
            end
        end
    end

endmodule
