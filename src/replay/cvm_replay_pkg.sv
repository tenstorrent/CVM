// SPDX-FileCopyrightText: © 2026 Tenstorrent USA, Inc.
// SPDX-License-Identifier: Apache-2.0

package cvm_replay_pkg;

    // Declares one port and checks the recording agrees about it -- the
    // conformance check. Port by port rather than one formatted string, because
    // a width is a parameter in the generated module and a string would have to
    // be built at elaboration. Called once each, out of reset, before the load.
    import "DPI-C" function int cvm_replay_bind (
        int unsigned location,
        string       name,
        int          width,
        int          bit_offset,
        int          is_output
    );

    // Returns 0, or -1 on failure. Called once, after the binds.
    // `elem_words` is the transport's element size, computed by the generated
    // module so the host does not re-derive it.
    import "DPI-C" function int cvm_replay_load (
        int unsigned location,
        int          port_bits,
        int          elem_words
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
