// SPDX-FileCopyrightText: © 2026 Tenstorrent USA, Inc.
// SPDX-License-Identifier: Apache-2.0

// The scenario expectations. Nothing in C++ reads these -- the testbench does,
// through cvm_plusargs::get_int -- but declaring them makes this the single
// source of truth: the defaults below are what a scenario gets when it omits
// the plusarg, and an undeclared name fails the run instead of being ignored.
// A misspelled +expect_errors would otherwise leave the expectation at its
// default and the scenario would pass having verified nothing.
//
// In a separate file, not a sim main(), so the wrapper stays generic. gflags
// registers from any translation unit given alwayslink.

#include <gflags/gflags.h>

DEFINE_int32(expect_errors, 0,
             "how many ERROR-level reports this scenario should produce");
DEFINE_int32(expect_replay_result, -1,
             "result the monitor should have seen while replay was running, or "
             "-1 to skip");
DEFINE_int32(expect_last_result, -1,
             "result the monitor should have seen last of all, or -1 to skip");
DEFINE_int32(expect_done_cycles, -1,
             "testbench clock cycles from enable to done, or -1 to skip");
DEFINE_int32(
    min_monitor_edges, 1,
    "the external monitor must observe at least this many clock edges");
// Leaves a loaded recording unreplayed, which the host must notice at end of
// run. The testbench itself passes, so only the exit status carries it.
DEFINE_int32(skip_enable, 0, "never assert enable");
