// SPDX-FileCopyrightText: © 2026 Tenstorrent USA, Inc.
// SPDX-License-Identifier: Apache-2.0

// Cycle-based replay, synthesizable so it runs on a hardware emulator.
//
//   observed   - the DUT boundary as it stands: the testbench's value on each
//                input slice, the DUT's value on each output slice
//   driven     - what to drive onto the DUT's inputs
//   drive_en   - which of those bits to actually drive. All ones over an input
//                slice and all zeros over an output one, so it only varies
//                across an `inout`, where the recording says bit by bit which
//                side had the net.
//   drive_val  - the recorded stimulus alone, with no bypass fallback. An
//                inout's driver has to come from here rather than from
//                `driven`: it shares the net it would otherwise read back
//                through, and `driven` falls back to `observed`, so the two
//                together are a combinational loop.
//
// Elements arrive over cvm_pipe, one per cycle that changes anything; see
// element_t. The host resolves X first, so every platform replays identical
// bits.
module cvm_replay_engine #(
    parameter int unsigned LOCATION = cvm_topology::nil,
    // Width of the flattened DUT boundary: every replayed port's bits, laid end
    // to end in the order the layout gives. Any width; the word padding the
    // transport needs is worked out below.
    parameter int    PORT_BITS  = 32,
    // Elements buffered in the transport. Sizes a memory, so a parameter.
    parameter int    PIPE_DEPTH = 4096,
    // In elements. One element is several words against a fixed push formal, so
    // any constant default fails to elaborate above some width; the generator
    // computes it.
    parameter int    PUSH_MAX_ELEMENTS = 1024
) (
    input  logic clk,
    input  logic reset_n,
    input  logic enable,
    output logic done,

    input  logic [PORT_BITS-1:0] observed,
    output logic [PORT_BITS-1:0] driven,
    output logic [PORT_BITS-1:0] drive_en,
    output logic [PORT_BITS-1:0] drive_val,

    // Exposed so a testbench need not use DPI. 64-bit because a long emulation
    // run outlasts a 32-bit cycle count, and mismatches counts bit-cycles.
    output logic [63:0]          mismatches,
    output logic [63:0]          first_fail_cycle,
    output logic [PORT_BITS-1:0] fail_bits
);

    import cvm_replay_pkg::*;
    import cvm_pipe_pkg::*;

    // The transport moves whole words, so the boundary is padded up to one.
    localparam int WORDS  = (PORT_BITS + 31) / 32;
    localparam int PADDED = WORDS * 32;

    // One element is a whole cycle's update. The host packs words LSB first, so
    // `cycle` is the low field and `care` the high one.
    typedef struct packed {
        logic [PADDED-1:0] care;      // which exp bits are compared
        logic [PADDED-1:0] exp;       // the recorded outputs for that cycle
        logic [PADDED-1:0] drive_en;  // which `in` bits to drive
        logic [PADDED-1:0] in;        // what to drive onto the DUT's inputs
        logic [63:0]       cycle;     // absolute, counted from enable
    } element_t;

    logic        pipe_valid, pipe_pop, pipe_eos;
    element_t    element;
    logic [31:0] pipe_min_occupancy, pipe_demands;

    cvm_pipe_in #(
        .LOCATION           (LOCATION),
        .T                  (element_t),
        .DEPTH              (PIPE_DEPTH),
        .PUSH_MAX_ELEMENTS (PUSH_MAX_ELEMENTS)
    ) u_pipe (
        .clk             (clk),
        .reset_n         (reset_n),
        .valid           (pipe_valid),
        .data            (element),
        .pop             (pipe_pop),
        .eos             (pipe_eos),
        // On from reset, not from `running`: the queue has to be filled
        // before enable, or the first cycle's element arrives a cycle late.
        .demand_en       (!done),
        .credit_calls    (),
        .demands         (pipe_demands),
        .demand_retries  (),
        .min_occupancy_o (pipe_min_occupancy)
    );

    logic                running, drive, reported, fail_seen;
    logic [63:0]         cyc;
    logic [PORT_BITS-1:0] drive_q, drive_en_q;
    // A cycle with no element keeps the previous expectation.
    logic [PORT_BITS-1:0] exp_cur, care_cur;

    // In BYPASS, or once replay is over, the testbench drives straight through
    // -- and the enable falls to zero, so any `inout` goes back to the
    // testbench rather than being held by a stale recorded value.
    assign driven    = drive ? drive_q    : observed;
    assign drive_en  = drive ? drive_en_q : '0;
    assign drive_val = drive_q;

    // Consume the element for this cycle, and only this cycle.
    assign pipe_pop = running && pipe_valid && (element.cycle == cyc);

    // At cycle k a dump holds in[k] beside the outputs *present* during cycle
    // k, which for a registered DUT are the response to in[k-1]; it never pairs
    // an input with the output it is about to cause. Reading exp_cur in
    // always_ff is itself the one cycle of delay, so there is no explicit
    // stage. Adding one still runs, and only a test built on the real
    // convention notices.
    logic [PORT_BITS-1:0] miss;
    assign miss = (observed ^ exp_cur) & care_cur;

    // The report hands the mask back a word at a time, so pad it once here
    // rather than part-selecting off the end of `fail_bits`.
    logic [PADDED-1:0] fail_words;
    assign fail_words = PADDED'(fail_bits);

    // Flopped, not printed: a $display in the datapath routes to the host on
    // some platforms, which is the stall the block split exists to avoid.
    logic        late_seen;
    logic [63:0] late_cycle, late_arrived;

    // --- Datapath: no DPI, no $error ---
    always_ff @(posedge clk) begin
        if (!reset_n) begin
            done             <= 1'b0;
            running          <= 1'b0;
            drive            <= 1'b0;
            fail_seen        <= 1'b0;
            cyc              <= '0;
            drive_q          <= '0;
            drive_en_q       <= '0;
            exp_cur          <= '0;
            // Zero so the check is inert until an element sets it.
            care_cur         <= '0;
            mismatches       <= '0;
            first_fail_cycle <= '1;
            fail_bits        <= '0;
            late_seen        <= 1'b0;
            late_cycle       <= '0;
            late_arrived     <= '0;
        end else begin
            if (enable && !running && !done) begin
                // Nothing to replay: the transport was closed before a single
                // element arrived. Finish without ever driving, so an
                // interposer with no recording stays a transparent wire.
                if (pipe_eos) begin
                    done <= 1'b1;
                end else begin
                    running <= 1'b1;
                    drive   <= 1'b1;
                end
            end

            if (running) begin
                cyc <= cyc + 64'd1;

                if (pipe_valid && element.cycle == cyc) begin
                    drive_q    <= element.in[PORT_BITS-1:0];
                    drive_en_q <= element.drive_en[PORT_BITS-1:0];
                    exp_cur  <= element.exp[PORT_BITS-1:0];
                    care_cur <= element.care[PORT_BITS-1:0];
                end else if (pipe_valid && element.cycle < cyc && !late_seen) begin
                    // Everything after it is shifted, and a shifted replay
                    // still runs, so this has to be reported.
                    late_seen    <= 1'b1;
                    late_cycle   <= element.cycle;
                    late_arrived <= cyc;
                end

                if (miss != '0) begin
                    mismatches <= mismatches + 64'($countones(miss));
                    fail_bits  <= fail_bits | miss;
                    if (!fail_seen) begin
                        fail_seen        <= 1'b1;
                        first_fail_cycle <= cyc;
                    end
                end

                // The last element is popped at posedge L, so eos is visible
                // at L+1 -- the same edge that compares its expectation. Both
                // happen here, so finishing now still includes it.
                if (pipe_eos) begin
                    running <= 1'b0;
                    drive   <= 1'b0;
                    done    <= 1'b1;
                end
            end
        end
    end

    // --- REPORT: everything that talks to the host or a console ---
    logic late_reported;
    always_ff @(posedge clk) begin
        if (!reset_n) begin
            reported      <= 1'b0;
            late_reported <= 1'b0;
        end else begin
            if (late_seen && !late_reported) begin
                late_reported <= 1'b1;
                $error("cvm_replay(%m): element for cycle %0d arrived at cycle %0d",
                       late_cycle, late_arrived);
            end

            // The cycle after done, so the final comparison is included. The
            // summary returns a value, so this block stalls the clock once.
            if (done && !reported) begin
                reported <= 1'b1;
                for (int i = 0; i < WORDS; i++) begin
                    cvm_replay_report_word(LOCATION, i, int'(fail_words[i*32 +: 32]));
                end
                void'(cvm_replay_report(LOCATION, longint'(mismatches),
                                        fail_seen ? longint'(first_fail_cycle) : -1,
                                        longint'(cyc), int'(pipe_min_occupancy),
                                        int'(pipe_demands)));
            end
        end
    end

endmodule
