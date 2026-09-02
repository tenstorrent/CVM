// SPDX-FileCopyrightText: © 2026 Tenstorrent USA, Inc.
// SPDX-License-Identifier: Apache-2.0

package cvm_replay_pkg;

    // Runtime mode, resolved once at the time origin from +cvm_replay_mode.
    // Affects only who drives the DUT's inputs.
    typedef enum int {
        CVM_REPLAY = 0,
        CVM_BYPASS = 1
    } cvm_replay_mode_e;

    import "DPI-C" function int  cvm_replay_open     (string hier, string layout);
    import "DPI-C" function int  cvm_replay_next     (int handle, output longint unsigned at);
    import "DPI-C" function void cvm_replay_word     (int handle, int index, output logic [31:0] w);
    import "DPI-C" function void cvm_replay_observed (int handle, int index, input logic [31:0] w);
    import "DPI-C" function int  cvm_replay_check    (int handle, longint unsigned sim_time);
    import "DPI-C" function int  cvm_replay_mode     (string hier);
    import "DPI-C" function void cvm_replay_error    (string msg);

    function automatic logic [31:0] word(input int handle, input int index);
        logic [31:0] w;
        cvm_replay_word(handle, index, w);
        return w;
    endfunction

endpackage
