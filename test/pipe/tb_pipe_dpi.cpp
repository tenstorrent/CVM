// SPDX-FileCopyrightText: © 2026 Tenstorrent USA, Inc.
// SPDX-License-Identifier: Apache-2.0

// The host side of tb_pipe.sv: the producer, and what it still holds.

#include <atomic>
#include <cstdint>

#include <gflags/gflags.h>

#include "cvm/logger.hpp"
#include "cvm/pipe.hpp"
#include "cvm/registry.hpp"

// Fault injection: a producer that has nothing *right now* but is not finished,
// which is what the demand retry status exists for.
DEFINE_int32(dry_every, 0, "return no elements on every Nth producer call");
// Guards against a scenario going vacuous: a run meant to exercise the retry
// status has to show demand actually firing.
DEFINE_int32(expect_min_demands, 0, "fail if fewer demands than this occurred");
DEFINE_int32(expect_min_credits, 1, "fail if fewer credit returns than this");
// The discriminator for the credit path: if credit is doing the refilling,
// demand should barely fire. 0 means no limit.
DEFINE_int32(expect_max_demands, 0, "fail if more demands than this occurred");

namespace {

  // What the testbench tells the host about the run it elaborated.
  struct stimulus_config {
      std::uint32_t total = 0;
      std::size_t words_per_element = 0;
  };

  // Owns the pipe, as any producer does, and reaches it directly.
  class stimulus {
    public:
      stimulus(cvm::topology::loc_t loc, unsigned id) : pipe_(loc, id) {
        cvm::registry::messenger.connect<stimulus_config>(
            loc, [this](const stimulus_config& c) {
              total_ = c.total;
              wpe_ = c.words_per_element;
              pipe_.producer(
                  [this](std::uint32_t* out,
                         std::size_t max_elements) -> std::size_t {
                    return fill(out, max_elements);
                  },
                  wpe_);
            });
      }

    private:
      std::size_t fill(std::uint32_t* out, std::size_t max_elements) {
        ++calls_;
        if (FLAGS_dry_every > 0 &&
            (calls_ % static_cast<std::uint32_t>(FLAGS_dry_every)) == 0)
          return 0;

        std::size_t n = 0;
        while (n < max_elements && next_ < total_) {
          const std::uint32_t i = next_++;
          std::uint32_t* e = out + n * wpe_;
          for (std::size_t w = 0; w < wpe_; ++w)
            e[w] = (w == 0) ? i : ((w == 1) ? ~i : i + 1);
          ++n;
        }
        if (next_ >= total_)
          pipe_.close();
        return n;
      }

      cvm::pipe_in pipe_;
      std::uint32_t total_ = 0;
      std::uint32_t next_ = 0;
      std::uint32_t calls_ = 0;
      std::size_t wpe_ = 0;
  };

} // namespace

REGISTRY_register(stimulus, PIPE, cvm::registry::all)

extern "C" void tb_pipe_stimulus(unsigned int location, int total,
                                 int words_per_element) {
  // Async at the same priority as the pipe's own DPIs, so this lands on the
  // messenger's thread ahead of the credits that follow it.
  cvm::registry::messenger.signal_async<stimulus_config>(
      location,
      {static_cast<std::uint32_t>(total),
       static_cast<std::size_t>(words_per_element)},
      cvm::messenger::highest_priority);
}

extern "C" int tb_pipe_expect_max_demands() {
  return FLAGS_expect_max_demands;
}

extern "C" int tb_pipe_expect_min_credits() {
  return FLAGS_expect_min_credits;
}

extern "C" int tb_pipe_expect_retries() {
  return FLAGS_dry_every > 0 ? 1 : 0;
}

extern "C" int tb_pipe_expect_min_demands() {
  return FLAGS_expect_min_demands;
}

extern "C" int tb_pipe_pending(unsigned int location) {
  std::size_t elements = 0;
  std::atomic<bool> done(false);
  cvm::registry::messenger.signal_async<cvm::pipe_pending>(
      location, {&elements, &done}, cvm::messenger::highest_priority);
  done.wait(false);
  return static_cast<int>(elements);
}
