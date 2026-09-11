// SPDX-FileCopyrightText: © 2026 Tenstorrent USA, Inc.
// SPDX-License-Identifier: Apache-2.0

// Cycle-based replay: synchronous, reset-based and synthesizable, so it runs on
// a hardware emulator as well as a simulator. One clock, one edge, setup off
// reset, and no $time, delays, forever loop or per-cycle DPI traffic.
//
//   observed - the DUT boundary as it stands: the testbench's value on each
//              input slice, the DUT's value on each output slice
//   driven   - what to drive onto the DUT's inputs
//
// Elements arrive over cvm_pipe, one per cycle that changes anything; see
// cvm_replay_elem_width for the layout. The host resolves X first, so every
// platform replays identical bits.
module cvm_replay_engine #(
    parameter string HIER       = "",
    // Identity for the transport. Replay still keys its own plusargs off HIER;
    // that collapses into LOCATION in a later step.
    parameter int unsigned LOCATION = cvm_topology::nil,
    parameter int    PADDED     = 32,
    // Elements buffered in the transport. Sizes a memory, so a parameter.
    parameter int    PIPE_DEPTH = 4096,
    // In elements. One element is 1 + 3*(PADDED/32) words against a fixed push
    // formal, so any constant default fails to elaborate above some width; the
    // generator computes it.
    parameter int    PUSH_MAX_ELEMENTS = 1024
) (
    input  logic clk,
    input  logic reset_n,
    input  logic enable,
    output logic done,

    input  logic [PADDED-1:0] observed,
    output logic [PADDED-1:0] driven,

    // Also handed to the host at done; exposed so a testbench need not use DPI.
    output logic [31:0]       mismatches,
    output logic [31:0]       first_fail_cycle,
    output logic [PADDED-1:0] fail_bits
);

    import cvm_replay_pkg::*;
    import cvm_pipe_pkg::*;

    localparam int EW     = 32 + 3 * PADDED;
    localparam int NWORDS = (PADDED + 31) / 32;

    logic          pipe_valid, pipe_pop, pipe_eos;
    logic [EW-1:0] pipe_data;
    logic [31:0]   pipe_min_occupancy, pipe_demands;

    cvm_pipe_in #(
        .LOCATION           (LOCATION),
        .WIDTH              (EW),
        .DEPTH              (PIPE_DEPTH),
        .PUSH_MAX_ELEMENTS (PUSH_MAX_ELEMENTS)
    ) u_pipe (
        .clk             (clk),
        .reset_n         (reset_n),
        .valid           (pipe_valid),
        .data            (pipe_data),
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

    logic [31:0]       e_cycle;
    logic [PADDED-1:0] e_in, e_exp, e_care;
    assign e_cycle = pipe_data[31:0];
    assign e_in    = pipe_data[32               +: PADDED];
    assign e_exp   = pipe_data[32 +     PADDED  +: PADDED];
    assign e_care  = pipe_data[32 + 2 * PADDED  +: PADDED];

    logic              running, drive, opened, is_replay, reported, fail_seen;
    logic [31:0]       cyc;
    logic [PADDED-1:0] drive_q;
    // Registers, not a queue: the check runs every cycle, and a cycle with no
    // element keeps the previous expectation.
    logic [PADDED-1:0] exp_cur, care_cur;

    // In BYPASS, or once replay is over, the testbench drives straight through.
    assign driven = drive ? drive_q : observed;

    // Consume the element for this cycle, and only this cycle.
    assign pipe_pop = running && pipe_valid && (e_cycle == cyc);

    // The main off-by-one risk, and it turns on what a recording contains: at
    // cycle k the dump holds in[k] beside the outputs *present* during cycle k,
    // which for a registered DUT are the response to in[k-1]. A dump never
    // pairs an input with the output it is about to cause.
    //
    //   posedge k    drive_q <= in[k], exp_cur <= out[k]
    //   cycle  k     the DUT's outputs are out[k], as recorded
    //   posedge k+1  reading `observed` yields what cycle k held
    //
    // So compare `observed` against out[k] at posedge k+1 -- which is what
    // reading exp_cur there gives, since reading a register in always_ff is
    // itself the one cycle of delay. No explicit stage. Add one and every
    // expectation is checked against the next cycle's outputs, while everything
    // still runs; only a test built on the real convention notices.
    logic [PADDED-1:0] miss;
    assign miss = (observed ^ exp_cur) & care_cur;

    // Flopped, not printed: a $display in the datapath routes to the host on
    // some platforms, which is the stall the block split exists to avoid.
    logic        late_seen;
    logic [31:0] late_cycle, late_arrived;

    // --- CFG: resolves the mode once, out of reset. Its own block because a
    // returning import stalls the clock, and sharing would mark that block. ---
    always_ff @(posedge clk) begin
        if (!reset_n) begin
            opened    <= 1'b0;
            is_replay <= 1'b0;
        end else if (!opened) begin
            opened    <= 1'b1;
            // From a plusarg, so a runtime read, but it cannot change mid-run.
            is_replay <= cvm_replay_mode(HIER) == int'(CVM_REPLAY);
        end
    end

    // --- Datapath: no DPI, no $error ---
    always_ff @(posedge clk) begin
        if (!reset_n) begin
            done             <= 1'b0;
            running          <= 1'b0;
            drive            <= 1'b0;
            fail_seen        <= 1'b0;
            cyc              <= '0;
            drive_q          <= '0;
            exp_cur          <= '0;
            // Zero so the check is inert until an element sets it.
            care_cur         <= '0;
            mismatches       <= '0;
            first_fail_cycle <= 32'hFFFF_FFFF;
            fail_bits        <= '0;
            late_seen        <= 1'b0;
            late_cycle       <= '0;
            late_arrived     <= '0;
        end else begin
            if (opened && !running && !done) begin
                if (enable) begin
                    if (is_replay) begin
                        running <= 1'b1;
                        drive   <= 1'b1;
                    end else begin
                        // BYPASS is a transparent wire: nothing to replay.
                        done <= 1'b1;
                    end
                end
            end

            if (running) begin
                cyc <= cyc + 32'd1;

                if (pipe_valid && e_cycle == cyc) begin
                    drive_q  <= e_in;
                    exp_cur  <= e_exp;
                    care_cur <= e_care;
                end else if (pipe_valid && e_cycle < cyc && !late_seen) begin
                    // Stimulus arrived after its cycle passed, so everything
                    // after it is shifted -- and a shifted replay still runs.
                    late_seen    <= 1'b1;
                    late_cycle   <= e_cycle;
                    late_arrived <= cyc;
                end

                if (miss != '0) begin
                    mismatches <= mismatches + 32'($countones(miss));
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
                $error("cvm_replay(%s): element for cycle %0d arrived at cycle %0d",
                       HIER, late_cycle, late_arrived);
            end

            // The cycle after done, so the final comparison is included. Both
            // imports are void, so neither stalls.
            if (done && !reported) begin
                reported <= 1'b1;
                for (int i = 0; i < NWORDS; i++) begin
                    cvm_replay_report_word(HIER, i, fail_bits[i*32 +: 32]);
                end
                cvm_replay_report(HIER, int'(mismatches),
                                  fail_seen ? int'(first_fail_cycle) : -1,
                                  int'(cyc), int'(pipe_min_occupancy),
                                  int'(pipe_demands));
            end
        end
    end

endmodule
