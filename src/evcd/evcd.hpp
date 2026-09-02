// SPDX-FileCopyrightText: © 2026 Tenstorrent USA, Inc.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstddef>
#include <cstdint>
#include <istream>
#include <optional>
#include <string>
#include <vector>

namespace cvm {
namespace evcd {

// Value a driver is presenting. `none` means that side is not driving.
enum class drive : std::uint8_t { none,
                                  zero,
                                  one,
                                  unknown,
                                  highz };

// Both sides of one port. Two fields because the standard's unknown-direction
// characters genuinely record two values at once: `A` is input 0 *and* output
// 1, `B` is input 1 and output 0. That is a drive conflict on a bidirectional
// port, which is a large part of why EVCD exists. For the single-direction
// characters the inactive side is `none`, meaning the recording says nothing
// about it, which is distinct from `highz`, meaning it was recorded as
// three-stated.
//
// Replay picks a side per port: an input takes `ext`, an output takes `dut`.
struct port_state {
  drive ext = drive::none; // test fixture
  drive dut = drive::none; // device under test
};

// nullopt outside the standard set; callers must treat that as a parse error.
std::optional<port_state> decode_state(char c);

// A port as the dump declares it.
struct dump_port {
  std::string name;
  std::size_t width = 0;
};

// One port's recorded value: a run of state characters, MSB first.
struct change {
  std::size_t port = 0; // index into parser::ports()
  std::string state;
};

struct step {
  std::uint64_t time = 0;
  std::vector<change> changes;
  // $dumpportsoff (18.3.2): the recording stops here, and says so by declaring
  // every port unknown from this time forward. It is a statement about the
  // recording, not about the design -- the ports did not become X, the
  // recording simply stopped tracking them. Replay drives X and checks nothing
  // until $dumpportson, rather than holding stale values it can no longer
  // vouch for.
  //
  // A flag rather than a change, because every state character also picks a
  // side and so cannot express "unknown, both sides".
  bool all_ports_unknown = false;
};

// Streaming EVCD reader. Knows the file format and nothing else.
class parser {
public:
  // Header only. False on failure, with error() set.
  bool open(std::istream& in);
  bool next(step& out);

  const std::vector<dump_port>& ports() const { return ports_; }
  const std::string& error() const { return error_; }

private:
  bool token(std::string& t);
  bool skip_to_end();
  bool parse_var();
  bool fail(const std::string& msg);

  std::vector<dump_port> ports_;
  std::string error_;
  std::istream* in_ = nullptr;
  bool header_done_ = false;
  bool eof_ = false;
  bool dumping_ = true;
  std::uint64_t pending_time_ = 0;
  bool have_pending_ = false;
};

} // namespace evcd
} // namespace cvm
