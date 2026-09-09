// SPDX-FileCopyrightText: © 2026 Tenstorrent USA, Inc.
// SPDX-License-Identifier: Apache-2.0

// The host side of tb_pipe.sv. Only what the testbench cannot do in
// SystemVerilog: queue the elements, and report what is still queued.

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <gflags/gflags.h>

#include "cvm/pipe.hpp"
#include "svdpi.h"

// Declared here rather than in a sim main() so the wrapper stays generic.
DEFINE_bool(produce_on_demand, false,
            "generate elements from a producer the transport pulls, instead of "
            "queueing them all up front");

namespace {

  // Kept alive for the run; the testbench refers to a stream by name.
  std::map<std::string, std::unique_ptr<cvm::pipe::in_stream>>& streams() {
    static std::map<std::string, std::unique_ptr<cvm::pipe::in_stream>> s;
    return s;
  }

} // namespace

extern "C" void tb_pipe_stimulus(const char* name, int elements,
                                 int words_per_element) {
  auto& slot = streams()[name];
  slot = std::make_unique<cvm::pipe::in_stream>(
      name, static_cast<std::size_t>(words_per_element));

  const std::size_t wpe = static_cast<std::size_t>(words_per_element);
  const std::uint32_t total = static_cast<std::uint32_t>(elements);

  // Element i is {i, ~i, i + 1}: the trailing words let the testbench tell a
  // whole element from a partly delivered one.
  const auto fill = [wpe](std::uint32_t* e, std::uint32_t i) {
    for (std::size_t w = 0; w < wpe; ++w) {
      e[w] = (w == 0) ? i : ((w == 1) ? ~i : i + 1);
    }
  };

  if (FLAGS_produce_on_demand) {
    // Nothing is queued up front. The transport pulls when it runs short, which
    // is what lets a producer stream something it cannot hold in memory.
    auto next = std::make_shared<std::uint32_t>(0);
    slot->on_demand([fill, next, total, wpe](std::uint32_t* out,
                                             std::size_t max_elements) {
      std::size_t n = 0;
      while (n < max_elements && *next < total) {
        fill(out + n * wpe, (*next)++);
        ++n;
      }
      // 0 means the producer is finished, and the stream closes itself.
      return n;
    });
    return;
  }

  std::vector<std::uint32_t> words(wpe);
  for (std::uint32_t i = 0; i < total; ++i) {
    fill(words.data(), i);
    slot->push(words);
  }
  slot->close();
}

extern "C" int tb_pipe_pending(const char* name) {
  const auto it = streams().find(name);
  if (it == streams().end())
    return 0;
  return static_cast<int>(it->second->pending());
}
