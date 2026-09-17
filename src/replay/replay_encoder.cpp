// SPDX-FileCopyrightText: © 2026 Tenstorrent USA, Inc.
// SPDX-License-Identifier: Apache-2.0

#include "cvm/replay_encoder.hpp"

namespace cvm {
  namespace replay {

    void encoder::start(source& src, std::size_t words_per_element) {
      src_ = &src;
      words_per_element_ = words_per_element;
      have_previous_ = false;
      finished_ = false;
    }

    void encoder::pack(const cycle_element& e, std::uint32_t* out) const {
      const std::size_t n = e.in.size();
      out[0] = static_cast<std::uint32_t>(e.cycle);
      out[1] = static_cast<std::uint32_t>(e.cycle >> 32);
      for (std::size_t i = 0; i < n; ++i) {
        out[2 + i] = e.in[i];
        out[2 + n + i] = i < e.drive_en.size() ? e.drive_en[i] : 0u;
        out[2 + 2 * n + i] = i < e.exp.size() ? e.exp[i] : 0u;
        out[2 + 3 * n + i] = i < e.care.size() ? e.care[i] : 0u;
      }
    }

    std::size_t encoder::fill(std::uint32_t* out, std::size_t max_elements) {
      if (src_ == nullptr || finished_)
        return 0;

      std::size_t produced = 0;
      cycle_element e;
      while (produced < max_elements) {
        if (!src_->next_cycle(e)) {
          finished_ = true;
          break;
        }
        // A recording can hold a timestamp moving nothing the spec binds, the
        // clock especially, and that costs no element.
        if (have_previous_ && e.same_payload(previous_))
          continue;

        pack(e, out + produced * words_per_element_);
        previous_ = e;
        have_previous_ = true;
        ++produced;
        ++produced_;
      }
      return produced;
    }
  } // namespace replay
} // namespace cvm
