// SPDX-FileCopyrightText: © 2026 Tenstorrent USA, Inc.
// SPDX-License-Identifier: Apache-2.0

package cvm_replay_pkg;

    // Resolved once from +cvm_replay_mode. Affects only who drives the DUT.
    typedef enum int {
        CVM_REPLAY = 0,
        CVM_BYPASS = 1
    } cvm_replay_mode_e;

    import "DPI-C" function int cvm_replay_mode (string hier);

    // Opens the recording and installs the encoder the transport pulls from.
    // Returns 0, or -1 on failure. Called once, out of reset.
    import "DPI-C" function int cvm_replay_load (
        int unsigned location,
        string hier,
        string layout,
        int    padded,
        int    x_fill_one
    );

    // One element is a whole cycle's update, so one pop per cycle delivers
    // everything for that cycle. Bit layout, LSB first:
    //
    //   [31:0]                    cycle   absolute, counted from enable
    //   [32           +: PADDED]  in      what to drive onto the DUT's inputs
    //   [32 +   PADDED+: PADDED]  exp     the recorded outputs for that cycle
    //   [32 + 2*PADDED+: PADDED]  care    which exp bits are compared
    //
    // Absolute, not a delta: directly comparable to the engine's counter, so a
    // missed cycle is detectable instead of silently shifting the rest. Sparse
    // either way -- a cycle that changes nothing carries no element.
    function automatic int cvm_replay_elem_width(input int padded);
        return 32 + 3 * padded;
    endfunction

    // Failure summary, once at done. Keyed by hier rather than a handle so the
    // engine needs no session; the host maps bit indices back to port names.
    import "DPI-C" function void cvm_replay_report_word (string hier, int index, input logic [31:0] w);
    import "DPI-C" function void cvm_replay_report (
        string hier,
        int    mismatches,
        int    first_fail_cycle,   // -1 if none
        int    cycles,
        int    min_occupancy,      // closest the transport came to running dry
        int    demands
    );

endpackage
