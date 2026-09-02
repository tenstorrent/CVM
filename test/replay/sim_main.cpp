// SPDX-FileCopyrightText: © 2026 Tenstorrent USA, Inc.
// SPDX-License-Identifier: Apache-2.0

#include <gflags/gflags.h>
#include <gtest/gtest.h>

#include <memory>

#include "Vtop.h"
#include "cvm/logger.hpp"
#include "cvm/plusargs.hpp"
#include "verilated.h"

DEFINE_int32(expect_errors, 0,
             "how many ERROR-level reports this scenario should produce");
DEFINE_int32(expect_replay_result, -1,
             "result the monitor should have seen while replay was running, or "
             "-1 to skip");
DEFINE_int32(expect_last_result, -1,
             "result the monitor should have seen last of all, or -1 to skip");
DEFINE_int32(expect_done_time, -1,
             "simulation time replay should finish at, or -1 to skip");
DEFINE_int32(
    min_monitor_edges, 1,
    "the external monitor must observe at least this many clock edges");

namespace {

int g_errors = 0;

} // namespace

TEST(EvcdReplay, EndToEnd) {
  // cvm::log(cvm::ERROR, ...) is inert by default, so without this the
  // negative test would pass while detecting nothing.
  cvm::set_logger_handler(cvm::ERROR, []() { ++g_errors; });

  const std::unique_ptr<VerilatedContext> ctx{new VerilatedContext};
  const std::unique_ptr<Vtop> top{new Vtop{ctx.get()}};

  // A --timing model advances only when handed the next time slot; the other
  // sim mains here spin on eval() and would never progress a `#`.
  top->eval();
  while (!ctx->gotFinish() && top->eventsPending()) {
    ctx->time(top->nextTimeSlot());
    top->eval();
  }
  top->final();

  EXPECT_EQ(top->done, 1) << "the interposer should have reported done";
  EXPECT_GE(top->mon_edges, FLAGS_min_monitor_edges)
      << "the external monitor saw too little traffic on the DUT side";
  if (FLAGS_expect_replay_result >= 0) {
    EXPECT_EQ(top->mon_replay_result, FLAGS_expect_replay_result)
        << "the DUT did not produce the recorded result under replay";
  }
  if (FLAGS_expect_last_result >= 0) {
    EXPECT_EQ(top->mon_last_result, FLAGS_expect_last_result)
        << "after replay, the testbench should be driving the DUT again";
  }
  if (FLAGS_expect_done_time >= 0) {
    EXPECT_EQ(top->mon_done_time, FLAGS_expect_done_time)
        << "replay drifted: STROBE must not delay each successive vector";
  }
  EXPECT_EQ(g_errors, FLAGS_expect_errors);
}

int main(int argc, char** argv) {
  // Verilator's context is where cvm::plusargs::parse() finds the plusargs.
  Verilated::commandArgs(argc, argv);
  cvm::plusargs::parse();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
