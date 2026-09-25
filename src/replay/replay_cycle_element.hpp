// SPDX-FileCopyrightText: © 2026 Tenstorrent USA, Inc.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>
#include <vector>

namespace cvm {
  namespace replay {

    // One cycle's update, as the engine consumes it. `cycle` is the recorded
    // timestamp, which under cycle semantics *is* the cycle index.
    //
    // Two-state, because an emulator has no X: unknown inputs are resolved here
    // and unknown outputs left out of `care`.
    struct cycle_element {
        std::uint64_t cycle = 0;
        std::vector<std::uint32_t> in;
        // Which bits of `in` to actually drive. All ones for an input and all
        // zeros for an output, so this only ever varies for an `inout` -- but
        // it is full width for every port, which is the price of one uniform
        // element shape.
        std::vector<std::uint32_t> drive_en;
        std::vector<std::uint32_t> exp;
        std::vector<std::uint32_t> care;

        bool same_payload(const cycle_element& other) const {
          return in == other.in && drive_en == other.drive_en &&
                 exp == other.exp && care == other.care;
        }
    };

  } // namespace replay
} // namespace cvm
