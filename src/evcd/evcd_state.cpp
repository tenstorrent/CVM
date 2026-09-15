// SPDX-FileCopyrightText: © 2026 Tenstorrent USA, Inc.
// SPDX-License-Identifier: Apache-2.0

// The EVCD state-character table
// Strength is discarded

#include "cvm/evcd.hpp"

namespace cvm {
  namespace evcd {

    std::optional<port_state>
    decode_state(char c) {
      // clang-format off
  // Aligned on purpose: this file exists so the mapping reads against the
  // standard at a glance.
  switch (c) {
    // INPUT (test fixture drives; DUT side not recorded)
    case 'D': return port_state{drive::zero,    drive::none};
    case 'U': return port_state{drive::one,     drive::none};
    case 'N': return port_state{drive::unknown, drive::none};
    case 'Z': return port_state{drive::highz,   drive::none};
    case 'd': return port_state{drive::zero,    drive::none};
    case 'u': return port_state{drive::one,     drive::none};

    // OUTPUT (DUT drives; external side not recorded)
    case 'L': return port_state{drive::none, drive::zero};
    case 'H': return port_state{drive::none, drive::one};
    case 'X': return port_state{drive::none, drive::unknown};
    case 'T': return port_state{drive::none, drive::highz};
    case 'l': return port_state{drive::none, drive::zero};
    case 'h': return port_state{drive::none, drive::one};

    // UNKNOWN DIRECTION (both sides active)
    case '0': return port_state{drive::zero,    drive::zero};
    case '1': return port_state{drive::one,     drive::one};
    case '?': return port_state{drive::unknown, drive::unknown};
    case 'F': return port_state{drive::highz,   drive::highz};
    case 'A': return port_state{drive::zero,    drive::one};
    case 'a': return port_state{drive::zero,    drive::unknown};
    case 'B': return port_state{drive::one,     drive::zero};
    case 'b': return port_state{drive::one,     drive::unknown};
    case 'C': return port_state{drive::unknown, drive::zero};
    case 'c': return port_state{drive::unknown, drive::one};
    case 'f': return port_state{drive::highz,   drive::highz};

    default: return std::nullopt;
  }
      // clang-format on
    }

  } // namespace evcd
} // namespace cvm
