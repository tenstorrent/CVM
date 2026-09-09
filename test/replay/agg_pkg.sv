// SPDX-FileCopyrightText: © 2026 Tenstorrent USA, Inc.
// SPDX-License-Identifier: Apache-2.0

// The interposer never sees these: a packed aggregate is assignment-compatible
// with a same-width vector, so the spec gives each port as a width.
package agg_pkg;

    typedef struct packed {
        logic [7:0] data;
        logic       valid;
    } lane_t;                       // 9 bits

    typedef struct packed {
        logic [3:0]  hdr;
        lane_t [1:0] lanes;         // packed array of structs
    } bundle_t;                     // 4 + 18 = 22 bits

endpackage
