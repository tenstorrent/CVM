// SPDX-FileCopyrightText: © 2026 Tenstorrent USA, Inc.
// SPDX-License-Identifier: Apache-2.0

package cvm_pipe_pkg;

    // A DPI function has one signature per name, so a family of names is what
    // lets each instance use an array size that fits it.
    localparam int CVM_PIPE_MAX_WORDS = 32768;

    // Smallest size that holds `words`, or 0 if none does.
    function automatic int cvm_pipe_slot(input int words);
        if (words <=     8) return     8;
        if (words <=    32) return    32;
        if (words <=   128) return   128;
        if (words <=   512) return   512;
        if (words <=  2048) return  2048;
        if (words <=  8192) return  8192;
        if (words <= 32768) return 32768;
        return 0;
    endfunction

    // Reports this instance's geometry and zeroes the C-owned write pointer
    // through the export. Called once out of reset.
    // `context`: the implementation calls the cvm_pipe_in_zero export back.
    import "DPI-C" context function void cvm_pipe_reset(
        int unsigned location,
        int unsigned depth,
        int unsigned words_per_element,
        int unsigned push_slot_words
    );

    // Runtime knobs, read once at setup.
    import "DPI-C" function int cvm_pipe_credit_every(int unsigned location);
    import "DPI-C" function int cvm_pipe_demand_watermark(int unsigned location);
    import "DPI-C" function int cvm_pipe_demand_every(int unsigned location);

    // Returns credit by reporting the absolute read pointer. void, so the
    // clock keeps running: this is the call the normal path makes.
    import "DPI-C" function void cvm_pipe_credits(
        int unsigned location,
        int unsigned rptr
    );

    // Asks for elements now. 1 pushed, 0 nothing pending, -1 retry next cycle.
    // Has a return value to stall the emulation clock
    import "DPI-C" function int cvm_pipe_demand(
        int unsigned location,
        int unsigned rptr
    );

endpackage
