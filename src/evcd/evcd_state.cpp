// SPDX-FileCopyrightText: © 2026 Tenstorrent USA, Inc.
// SPDX-License-Identifier: Apache-2.0

// The EVCD state-character table, from IEEE Std 1364-2005 subclause 18.4.3.1.
//
// Isolated in its own translation unit because it is the single place
// correctness concentrates: a wrong entry gives a silently wrong replay rather
// than a parse error. The character's class is what picks the driving side,
// which is why decoding yields a pair rather than a value.
//
// `d`/`u` and `l`/`h` are strength-conflict resolutions (18.4.3.2): both sides
// drive, but only the winner's value is recorded, so the loser is left `none`.
// Drive strengths themselves are not represented -- replay reconstructs logic
// values, not analogue contention -- so the two strength digits are discarded.

#include "cvm/evcd.hpp"

namespace cvm {
namespace evcd {

std::optional<port_state>
decode_state(char c) {
  // clang-format off
  // Kept as an aligned table: this file exists so the mapping can be read
  // against the standard's text at a glance, and ColumnLimit: 0 would
  // otherwise put every case label and value on separate lines.
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
