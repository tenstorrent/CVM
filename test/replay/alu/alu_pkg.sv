// SPDX-FileCopyrightText: © 2026 Tenstorrent USA, Inc.
// SPDX-License-Identifier: Apache-2.0

// The interposer declares its ports with these types and measures them with
// $bits, so a width stated here is the only place it is stated.
package alu_pkg;

    // A width the DUT's ports derive from, so the spec cannot state a literal
    // without duplicating it.
    parameter int LANES = 2;

    typedef struct packed {
        logic [7:0] data;
        logic       valid;
    } lane_t;                       // 9 bits

    typedef struct packed {
        logic [3:0]  hdr;
        lane_t [1:0] lanes;         // packed array of structs
    } bundle_t;                     // 4 + 18 = 22 bits

endpackage
