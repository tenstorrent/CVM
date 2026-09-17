// SPDX-FileCopyrightText: © 2026 Tenstorrent USA, Inc.
// SPDX-License-Identifier: Apache-2.0

// A second package, so the DUT's header imports more than one and the spec has
// to carry both. A port type resolving only through this one is what makes the
// second import load bearing rather than decoration.
package alu_aux_pkg;

    typedef logic [7:0] byte_t;

endpackage
