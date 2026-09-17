// SPDX-FileCopyrightText: © 2026 Tenstorrent USA, Inc.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstddef>
#include <cstdint>

#include "cvm/replay_cycle_element.hpp"
#include "cvm/replay_source.hpp"

namespace cvm {
  namespace replay {

    // Turns a recording into the elements the transport carries: skips cycles
    // that change nothing, and packs each one as cvm_replay_engine's element_t
    // expects it -- the cycle as two words, then the stimulus, its drive enable,
    // the expectation and the care mask.
    //
    // Knows nothing of the transport beyond its element size.
    class encoder {
      public:
        // `words_per_element` is what the design reports at reset.
        void start(source& src, std::size_t words_per_element);

        // Writes up to `max_elements` whole elements and returns how many,
        // writing every word of each. 0 means the recording is spent, or that
        // start() has not been called.
        std::size_t fill(std::uint32_t* out, std::size_t max_elements);

        // No further call will produce anything -- the recording ran out, or
        // there was never one to start with.
        bool finished() const { return src_ == nullptr || finished_; }

        // Elements handed over so far.
        std::uint64_t produced() const { return produced_; }

      private:
        void pack(const cycle_element& e, std::uint32_t* out) const;

        source* src_ = nullptr;
        std::size_t words_per_element_ = 0;

        // Encoded on demand, so change detection persists between calls.
        cycle_element previous_;
        bool have_previous_ = false;
        bool finished_ = false;
        std::uint64_t produced_ = 0;
    };

  } // namespace replay
} // namespace cvm
