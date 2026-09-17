// SPDX-FileCopyrightText: © 2026 Tenstorrent USA, Inc.
// SPDX-License-Identifier: Apache-2.0

// Builds the elements tb_engine.sv replays, from a cycle timeline so the
// stimulus obeys a recording's convention: at cycle t the dump holds a[t]
// beside the outputs *present* during cycle t, which for this DUT is the
// response to a[t-1]. Pairing a[t] with the output it causes is scenario 1, and
// must fail.

#include <cstdint>
#include <vector>

#include <gflags/gflags.h>

#include "cvm/logger.hpp"
#include "cvm/pipe.hpp"
#include "cvm/registry.hpp"

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

  constexpr int kCycles = 40;
  // The cycle whose expectation scenario 4 corrupts.
  constexpr int kBadCycle = 10;

  std::uint32_t input_at(int t, bool sparse) {
    // Held for three cycles, so most cycles change nothing and carry no element.
    const int step = sparse ? t / 3 : t;
    return static_cast<std::uint32_t>(step + 1) & 0xFFu;
  }

  // What the DUT has already sampled when the first recorded cycle begins.
  // Cycle 0's outputs come from state the recording did not produce, so it is
  // checkable only if the DUT is already in the state the recording assumes --
  // which is why a recording should start at reset.
  constexpr std::uint32_t kInputBeforeEnable = 0u;

  // y <= a + 1, so each cycle shows the previous cycle's input plus one.
  std::uint32_t output_at(int t, bool sparse) {
    const std::uint32_t prev =
        (t == 0) ? kInputBeforeEnable : input_at(t - 1, sparse);
    return (prev + 1u) & 0xFFu;
  }

} // namespace

namespace {

  // Owns the pipe and builds the elements for the selected scenario. This
  // testbench drives cvm_replay_engine directly, so it supplies the host side
  // itself rather than linking replay's -- there is no recording to open.
  class stimulus {
    public:
      stimulus(cvm::topology::loc_t loc, unsigned id) : pipe_(loc, id) {
        cvm::registry::messenger.connect<int>(loc, [this](const int& bits) {
          build(bits);
          pipe_.producer(
              [this](std::uint32_t* out,
                     std::size_t max_elements) -> std::size_t {
                return fill(out, max_elements);
              },
              words_);
        });
      }

    private:
      void build(int boundary_bits) {
        const std::size_t pw =
            (static_cast<std::size_t>(boundary_bits) + 31) / 32;
        // Two words for the 64-bit cycle, then the boundary three times over.
        words_ = 2 + 3 * pw;

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
          std::uint32_t exp =
              ((FLAGS_scenario == 1) ? output_at(t + 1, sparse) : out) << 8;
          std::uint32_t care = 0xFF00u;

          if (FLAGS_scenario == 2) {
            // Wrong on purpose, but uncompared, so it must go unnoticed.
            exp = 0x5A00u;
            care = 0u;
          }
          if (FLAGS_scenario == 4 && t == kBadCycle)
            exp ^= 0x0100u;

          std::vector<std::uint32_t> e(words_, 0u);
          e[0] = static_cast<std::uint32_t>(t);
          e[2] = in;
          e[2 + pw] = exp;
          e[2 + 2 * pw] = care;
          queued_.insert(queued_.end(), e.begin(), e.end());
        }
      }

      std::size_t fill(std::uint32_t* out, std::size_t max_elements) {
        const std::size_t total = queued_.size() / words_;
        std::size_t n = 0;
        while (n < max_elements && sent_ < total) {
          const std::uint32_t* e = queued_.data() + sent_ * words_;
          for (std::size_t w = 0; w < words_; ++w)
            out[n * words_ + w] = e[w];
          ++sent_;
          ++n;
        }
        if (sent_ >= total)
          pipe_.close();
        return n;
      }

      cvm::pipe_in pipe_;
      std::vector<std::uint32_t> queued_;
      std::size_t words_ = 0;
      std::size_t sent_ = 0;
  };

} // namespace

REGISTRY_register(stimulus, REPLAY, cvm::registry::all)

extern "C" void tb_engine_stimulus(unsigned int location, int boundary_bits) {
  cvm::registry::messenger.signal_async<int>(location, boundary_bits,
                                             cvm::messenger::highest_priority);
}

// The engine's host calls, answered locally. Only the mismatch summary matters
// here: the testbench checks the error count, and nothing else reads these.
// cvm_replay_load is declared by cvm_replay_pkg, so it has to resolve even
// though this testbench drives the engine directly and opens no recording.
extern "C" {

  int cvm_replay_bind(unsigned int, const char*, int, int, int) { return 0; }

  int cvm_replay_load(unsigned int, int, int) { return 0; }

  void cvm_replay_report_word(unsigned int, int, unsigned int) {}

  int cvm_replay_report(unsigned int location, long long mismatches,
                        long long first_fail_cycle, long long cycles, int, int) {
    if (mismatches != 0) {
      cvm::log(cvm::ERROR,
               "Error: cvm::replay: {}: {} mismatching bit-cycles over {} cycles, "
               "first at cycle {}\n",
               cvm::topology::name(location), mismatches, cycles,
               first_fail_cycle);
    }
    return 0;
  }
}
