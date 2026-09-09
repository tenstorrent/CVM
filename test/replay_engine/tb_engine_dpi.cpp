// SPDX-FileCopyrightText: © 2026 Tenstorrent USA, Inc.
// SPDX-License-Identifier: Apache-2.0

// Builds the elements tb_engine.sv replays, from a cycle timeline so the
// stimulus obeys a recording's convention: at cycle t the dump holds a[t]
// beside the outputs *present* during cycle t, which for this DUT is the
// response to a[t-1], and only a cycle that changed something gets an element.
// Pairing a[t] with the output it causes is scenario 1, and must fail.
//
// Synthetic, so the engine can be tested without the EVCD layer.

#include <cstdint>
#include <memory>
#include <vector>

#include <gflags/gflags.h>

#include "cvm/pipe.hpp"

// Declared here rather than in a sim main() so the wrapper stays generic.
DEFINE_int32(scenario, 0,
             "0 clean, 1 each input paired with the output it causes, 2 wrong "
             "but not cared about, 3 sparse with idle cycles, 4 one wrong bit");
DEFINE_int32(expect_mismatches, 0,
             "mismatching bit-cycles the engine should count, or -1 for "
             "'must be non-zero'");
DEFINE_int32(expect_first_fail_cycle, -1,
             "cycle of the first mismatch, or -1 to skip");
DEFINE_int32(expect_errors, 0,
             "how many ERROR-level reports this scenario should produce");

namespace {

  std::unique_ptr<cvm::pipe::in_stream>& stream() {
    static std::unique_ptr<cvm::pipe::in_stream> s;
    return s;
  }

  constexpr int kCycles = 40;
  // The cycle whose expectation scenario 4 corrupts.
  constexpr int kBadCycle = 10;

  std::uint32_t input_at(int t, bool sparse) {
    // Held for three cycles, so most cycles change nothing and carry no element.
    const int step = sparse ? t / 3 : t;
    return static_cast<std::uint32_t>(step + 1) & 0xFFu;
  }

  // What the testbench drives before enable, so what the DUT has already sampled
  // when the first recorded cycle begins. Cycle 0's outputs come from state the
  // recording did not produce, so it is only checkable if the DUT is already in
  // the state the recording assumes -- which is why a recording should start at
  // reset.
  constexpr std::uint32_t kInputBeforeEnable = 0u;

  // y <= a + 1, so each cycle shows the previous cycle's input plus one.
  std::uint32_t output_at(int t, bool sparse) {
    const std::uint32_t prev =
        (t == 0) ? kInputBeforeEnable : input_at(t - 1, sparse);
    return (prev + 1u) & 0xFFu;
  }

} // namespace

extern "C" void tb_engine_stimulus(const char* hier, int padded) {
  // The layout is word aligned, which keeps the packing below readable.
  const int pw = padded / 32;
  const int words = 1 + 3 * pw;
  stream() = std::make_unique<cvm::pipe::in_stream>(
      hier, static_cast<std::size_t>(words));

  const bool sparse = FLAGS_scenario == 3;

  std::uint32_t prev_in = 0;
  std::uint32_t prev_out = 0;
  for (int t = 0; t < kCycles; ++t) {
    const std::uint32_t in = input_at(t, sparse);
    const std::uint32_t out = output_at(t, sparse);
    if (t != 0 && in == prev_in && out == prev_out)
      continue;
    prev_in = in;
    prev_out = out;

    // Scenario 1 uses the pairing a dump never does.
    std::uint32_t exp = ((FLAGS_scenario == 1) ? output_at(t + 1, sparse) : out)
                        << 8;
    std::uint32_t care = 0xFF00u;

    if (FLAGS_scenario == 2) {
      // Wrong on purpose, but uncompared, so it must go unnoticed.
      exp = 0x5A00u;
      care = 0u;
    }
    if (FLAGS_scenario == 4 && t == kBadCycle)
      exp ^= 0x0100u;

    std::vector<std::uint32_t> e(static_cast<std::size_t>(words), 0u);
    e[0] = static_cast<std::uint32_t>(t);
    e[1] = in;
    e[1 + static_cast<std::size_t>(pw)] = exp;
    e[1 + 2 * static_cast<std::size_t>(pw)] = care;
    stream()->push(e);
  }
  stream()->close();
}
