// SPDX-FileCopyrightText: © 2026 Tenstorrent USA, Inc.
// SPDX-License-Identifier: Apache-2.0

package cvm_pipe_pkg;

    // Size of the push export's array formal, in words. Fixed, not
    // per-instance: a DPI function has one signature per name, so two
    // instances declaring different formal sizes is an error. Costs no
    // registers, being a function argument.
    localparam int CVM_PIPE_MAX_WORDS = 8192;

    // Binds this instance's scope to `name` and states how many elements one
    // push may carry, so the host clamps to what the formal holds.
    import "DPI-C" context function int cvm_pipe_open(
        string name,
        int    epoch_max_elements
    );

    // Confirms `epoch` consumed and asks for the next.
    //
    // void on purpose: a returning import stops the emulation clock, and this
    // is the call the normal path makes every epoch. The data arrives later on
    // the push path. `room` bounds the batch so the host cannot overrun.
    import "DPI-C" function void cvm_pipe_request(int handle, int epoch, int room);

    // Fallback when a push has not landed in time: returns elements inline, so
    // progress is guaranteed rather than likely. The host reclaims whatever
    // push is outstanding and returns it here, so nothing is delivered twice.
    // The only steady-state call that stops the clock.
    //
    // Open array so each instance can size its own relief buffer; a fixed
    // formal would force every consumer to allocate the largest one.
    import "DPI-C" function int cvm_pipe_relief(
        input  int handle,
        input  int max_elements,
        output int unsigned data_words[],
        output int unsigned is_last
    );

    import "DPI-C" function int cvm_pipe_epoch_size(string name);
    import "DPI-C" function int cvm_pipe_headroom(string name);

endpackage
