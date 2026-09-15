// SPDX-FileCopyrightText: © 2026 Tenstorrent USA, Inc.
// SPDX-License-Identifier: Apache-2.0

// Host facts a testbench cannot get from SystemVerilog. Behind DPI imports so a
// testbench keeps its own checks and still needs no host-side driver.
package cvm_sim_pkg;

    // Zeroes the count. Call before the code under test can log anything.
    import "DPI-C" function void cvm_error_count_start();

    // ERROR reports since the count started. Checked rather than the exit
    // code, because cvm::log does not stop the simulation.
    import "DPI-C" function int cvm_error_count();

endpackage
