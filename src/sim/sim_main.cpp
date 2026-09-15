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
#include "cvm/registry.hpp"
#include "verilated.h"

// From //:sim_dpi, which every simulation links: the testbench arms this at the
// start of the run, so it is also how an end-of-run check gets noticed.
extern "C" int cvm_error_count();

int main(int argc, char** argv) {
  const std::unique_ptr<VerilatedContext> ctx{new VerilatedContext};
  ctx->commandArgs(argc, argv);
  // Makes +plusargs visible to the gflags-backed cvm libraries.
  cvm::plusargs::parse();

  // Constructs the registered components before the design runs.
  cvm::registry::build();

  const std::unique_ptr<Vtop> top{new Vtop{ctx.get()}};

  // Drains the framework's callback queue each slot, as a testbench does on
  // any other simulator. Libraries queue work here; nothing else would run it
  // -- unless a worker thread already owns the queue, in which case flushing
  // here would block on a lock that worker never gives up.
  const auto step = [&] {
    top->eval();
    if (!FLAGS_cb_async)
      cvm::registry::callbacks.flush();
  };

  step();
  while (!ctx->gotFinish() && top->eventsPending()) {
    ctx->time(top->nextTimeSlot());
    step();
  }

  const bool finished = ctx->gotFinish();

  // End-of-run component checks. They run after $finish, so the testbench's own
  // error count has already been read -- the exit status is what carries them.
  const int errors_before = cvm_error_count();
  cvm::registry::check();
  const bool checks_failed = cvm_error_count() != errors_before;

  // Tears the framework down while the design is still alive, as a testbench
  // does on any other simulator: a component being destroyed may still want an
  // export. It reports "not ready" while work is in flight, so keep draining.
  bool down = false;
  for (int tries = 0; tries < 1000 && !down; ++tries) {
    down = cvm::registry::shutdown();
    if (!down)
      cvm::registry::callbacks.flush();
  }

  top->final();

  if (!finished) {
    // Out of events without saying it was done, so whatever it waited for
    // never happened. Silence here would read as a pass.
    cvm::log(cvm::ERROR,
             "Error: sim: no events left at time {} and $finish was never reached\n",
             ctx->time());
    return 1;
  }
  if (!down) {
    cvm::log(cvm::ERROR, "Error: sim: the framework never finished shutting down\n");
    return 1;
  }
  if (checks_failed)
    return 1;
  return 0;
}
