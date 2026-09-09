// SPDX-FileCopyrightText: © 2026 Tenstorrent USA, Inc.
// SPDX-License-Identifier: Apache-2.0

// Verilator needs a main(); other simulators do not. This advances time until
// the design stops and reports how it stopped, and nothing else -- a testbench
// keeping its clock, stimulus and checks in SystemVerilog runs unchanged where
// it is elaborated directly, so nothing here may become load bearing. What a
// testbench needs from the host goes behind a DPI import it calls itself.
//
// Link with a verilated library whose top module is named `top`.

#include <memory>

#include "Vtop.h"
#include "cvm/logger.hpp"
#include "cvm/plusargs.hpp"
#include "verilated.h"

int main(int argc, char** argv) {
  const std::unique_ptr<VerilatedContext> ctx{new VerilatedContext};
  ctx->commandArgs(argc, argv);
  // Makes +plusargs visible to the gflags-backed cvm libraries.
  cvm::plusargs::parse();

  const std::unique_ptr<Vtop> top{new Vtop{ctx.get()}};

  top->eval();
  while (!ctx->gotFinish() && top->eventsPending()) {
    ctx->time(top->nextTimeSlot());
    top->eval();
  }

  if (!ctx->gotFinish()) {
    // Out of events without saying it was done, so whatever it waited for
    // never happened. Silence here would read as a pass.
    cvm::log(cvm::ERROR,
             "sim: no events left at time {} and $finish was never reached\n",
             ctx->time());
    return 1;
  }

  top->final();
  return 0;
}
