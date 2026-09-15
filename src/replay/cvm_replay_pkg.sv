// SPDX-FileCopyrightText: © 2026 Tenstorrent USA, Inc.
// SPDX-License-Identifier: Apache-2.0

package cvm_replay_pkg;

    // Returns 0, or -1 on failure. Called once, out of reset.
    import "DPI-C" function int cvm_replay_load (
        int unsigned location,
        string layout,
        int    port_bits
    );

    // Failure summary, once at done. The host maps bit indices to port names.
    //
    // The summary returns a value so that it cannot be streamed: a testbench
    // reads the host's error count straight afterwards, so the logging has to
    // have happened by the time this call comes back. Once per run, so the
    // stall costs nothing.
    import "DPI-C" function void cvm_replay_report_word (int unsigned location, int index, int unsigned w);
    import "DPI-C" function int cvm_replay_report (
        int unsigned location,
        longint mismatches,
        longint first_fail_cycle,  // -1 if none
        longint cycles,
        int     min_occupancy,     // closest the transport came to running dry
        int     demands
    );

endpackage
