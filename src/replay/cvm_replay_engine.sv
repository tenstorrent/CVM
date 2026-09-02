// SPDX-FileCopyrightText: © 2026 Tenstorrent USA, Inc.
// SPDX-License-Identifier: Apache-2.0

// The whole replay mechanism: mode resolution, opening the recording, advancing
// time, driving the DUT's inputs, and checking its outputs. The generated
// interposer instantiates one of these and does nothing but pack and unpack.
//
// Ports are flattened into single vectors so this module is width-generic and
// needs no knowledge of any particular port list:
//
//   observed - the DUT boundary as it stands: the testbench's value on each
//              input slice, the DUT's value on each output slice
//   driven   - what to drive onto the DUT's inputs
//
// LAYOUT carries the port list as `name:width:offset:is_output:check;...`, so
// binding, the conformance check, and output checking all happen in the runtime.
module cvm_replay_engine #(
    parameter string  HIER   = "",
    parameter int     PADDED = 32,
    parameter longint STROBE = 0,
    parameter string  LAYOUT = ""
) (
    input  logic              enable,
    output logic              done,
    input  logic [PADDED-1:0] observed,
    output logic [PADDED-1:0] driven
);

    import cvm_replay_pkg::*;

    localparam int NWORDS = (PADDED + 31) / 32;

    logic [PADDED-1:0] vec;
    logic              drive = 1'b0;
    int                handle = -1;
    time               origin;

    // In BYPASS, or once replay is over, the testbench drives straight through.
    assign driven = drive ? vec : observed;

    task automatic run();
        longint unsigned at;
        time target;
        forever begin
            if (cvm_replay_next(handle, at) == 0) break;

            // Wait until the recorded time measured from the origin, rather
            // than accumulating deltas: STROBE would otherwise push every
            // subsequent vector later and the replay would drift.
            target = origin + at;
            // The guard proves the delay is non-zero, but the expression is not
            // statically known, which is exactly what ZERODLY flags.
            /* verilator lint_off ZERODLY */
            if ($time < target) #(target - $time);
            /* verilator lint_on ZERODLY */
            if (!enable) break;  // enable falling stops replay permanently

            for (int i = 0; i < NWORDS; i++) begin
                vec[i*32 +: 32] = word(handle, i);
            end

            if (STROBE > 0) #(STROBE);

            // Hand the settled boundary to the runtime and let it compare.
            // Doing this synchronously, rather than signalling the interposer,
            // is what keeps the check on the right vector: the runtime replaces
            // its current vector as soon as the loop asks for the next one.
            for (int i = 0; i < NWORDS; i++) begin
                cvm_replay_observed(handle, i, observed[i*32 +: 32]);
            end
            void'(cvm_replay_check(handle, longint'($time)));
        end
    endtask

    initial begin
        done = 1'b0;
        vec  = 'x;

        // enable's rising edge is the time origin: recorded timestamps are
        // applied relative to it, so the testbench can initialise first.
        if (enable !== 1'b1) @(posedge enable);
        origin = $time;

        if (cvm_replay_mode(HIER) == CVM_REPLAY) begin
            handle = cvm_replay_open(HIER, LAYOUT);
            if (handle >= 0) begin
                drive = 1'b1;
                run();
            end
        end

        // Hand the inputs back to the testbench, matching the pre-start
        // behaviour so there are two regimes rather than three.
        drive = 1'b0;
        done  = 1'b1;
    end

endmodule
