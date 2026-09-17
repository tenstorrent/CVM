// SPDX-FileCopyrightText: © 2026 Tenstorrent USA, Inc.
// SPDX-License-Identifier: Apache-2.0

#include <atomic>

#include "cvm/replay_engine.hpp"
#include "cvm/registry.hpp"

// Replay owns its transport, so a consumer declares one node of this type and
// passes its location to the generated module.
REGISTRY_register(cvm::replay::engine, REPLAY, cvm::registry::all)

extern "C" {

  int cvm_replay_bind(unsigned int location, const char* name, int width,
                      int bit_offset, int dir) {
    int status = -1;
    std::atomic<bool> done(false);
    cvm::registry::messenger.signal_async<cvm::replay::bind_request>(
        location, {name, width, bit_offset, dir, &status, &done},
        cvm::messenger::highest_priority);
    done.wait(false);
    return status;
  }

  int cvm_replay_ignore(unsigned int location, const char* name) {
    int status = -1;
    std::atomic<bool> done(false);
    cvm::registry::messenger.signal_async<cvm::replay::ignore_request>(
        location, {name, &status, &done}, cvm::messenger::highest_priority);
    done.wait(false);
    return status;
  }

  int cvm_replay_load(unsigned int location, int port_bits, int elem_words) {
    int status = -1;
    std::atomic<bool> done(false);
    cvm::registry::messenger.signal_async<cvm::replay::load_request>(
        location, {port_bits, elem_words, &status, &done},
        cvm::messenger::highest_priority);
    done.wait(false);
    return status;
  }

  // Plain `int`, not a 4-state vector: the engine is 2-state, the host having
  // resolved X before the recording ever reached it.
  void cvm_replay_report_word(unsigned int location, int index,
                              unsigned int bits) {
    cvm::registry::messenger.signal_async<cvm::replay::report_word>(
        location, {index, bits}, cvm::messenger::highest_priority);
  }

  // Returns, so it is never streamed: the caller reads the host's error count
  // once this comes back. The queue is FIFO within a priority, so the staged
  // words are handled before it.
  int cvm_replay_report(unsigned int location, long long mismatches,
                        long long first_fail_cycle, long long cycles,
                        int min_occupancy, int demands) {
    std::atomic<bool> done(false);
    cvm::registry::messenger.signal_async<cvm::replay::report>(
        location,
        {mismatches, first_fail_cycle, cycles, min_occupancy, demands, &done},
        cvm::messenger::highest_priority);
    done.wait(false);
    return 0;
  }
}
