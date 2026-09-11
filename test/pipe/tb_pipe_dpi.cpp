// SPDX-FileCopyrightText: © 2026 Tenstorrent USA, Inc.
// SPDX-License-Identifier: Apache-2.0

// The host side of tb_pipe.sv: the producer, and what it still holds.

#include <atomic>
#include <cstdint>
#include <memory>

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

// The owner registers the pipe, not the pipe library.
REGISTRY_register(cvm::pipe_in, PIPE, cvm::registry::all)

    extern "C" void tb_pipe_stimulus(unsigned int location, int total,
                                     int words_per_element) {
  const std::size_t wpe = static_cast<std::size_t>(words_per_element);
  auto next = std::make_shared<std::uint32_t>(0);
  auto calls = std::make_shared<std::uint32_t>(0);
  const std::uint32_t count = static_cast<std::uint32_t>(total);

  cvm::pipe_producer producer;
  producer.words_per_element = wpe;
  producer.fill =
      [location, next, calls, count, wpe](std::uint32_t* out,
                                          std::size_t max_elements) -> std::size_t {
    ++*calls;
    if (FLAGS_dry_every > 0 &&
        (*calls % static_cast<std::uint32_t>(FLAGS_dry_every)) == 0)
      return 0;

    std::size_t n = 0;
    while (n < max_elements && *next < count) {
      const std::uint32_t i = (*next)++;
      std::uint32_t* e = out + n * wpe;
      for (std::size_t w = 0; w < wpe; ++w)
        e[w] = (w == 0) ? i : ((w == 1) ? ~i : i + 1);
      ++n;
    }
    if (*next >= count)
      cvm::registry::messenger.signal<cvm::pipe_close>(location, {});
    return n;
  };
  // Async at the same priority as the pipe's own DPIs, so this install is
  // serviced on the same thread and in order with them.
  cvm::registry::messenger.signal_async<cvm::pipe_producer>(
      location, producer, cvm::messenger::highest_priority);
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
