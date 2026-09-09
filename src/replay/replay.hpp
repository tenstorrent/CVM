// SPDX-FileCopyrightText: © 2026 Tenstorrent USA, Inc.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstddef>
#include <cstdint>
#include <istream>
#include <optional>
#include <string>
#include <vector>

#include "cvm/evcd.hpp"

namespace cvm {
  namespace replay {

    // Layout-compatible with svLogicVecVal. {bval,aval}: 00=0, 01=1, 10=Z, 11=X.
    struct logic_word {
        std::uint32_t aval = 0;
        std::uint32_t bval = 0;
    };

    // `recorded` marks ports the dump wrote at this timestamp, so an output is
    // checked exactly when the dump has a value for it. Indexed by bind order.
    struct replay_vector {
        std::uint64_t time = 0;
        std::vector<logic_word> value;
        std::vector<bool> recorded;
    };

    // One cycle's update, as the engine consumes it. `cycle` is the recorded
    // timestamp, which under cycle semantics *is* the cycle index.
    //
    // Two-state: an emulator has no X, so unknown inputs are resolved here and
    // unknown outputs left out of `care`, and every platform replays the same bits.
    struct cycle_element {
        std::uint64_t cycle = 0;
        std::vector<std::uint32_t> in;
        std::vector<std::uint32_t> exp;
        std::vector<std::uint32_t> care;

        bool same_payload(const cycle_element& other) const {
          return in == other.in && exp == other.exp && care == other.care;
        }
    };

    // Drives a flattened 4-state vector from a recorded vector stream.
    //
    // The port layout is supplied at runtime by bind(), not compiled in: the
    // generated SystemVerilog already knows every port's name, width, direction and
    // bit offset, so there is no reason to generate a second copy of that table in
    // C++.
    class source {
      public:
        source() = default;

        // `layout` is the port list as `name:width:offset:is_output:check;...`,
        // emitted by the generated interposer. Parsing it here binds every port and
        // runs the conformance check in one call, instead of a per-port handshake
        // back across the DPI boundary.
        // `name` appears in parse diagnostics; pass the path `in` came from.
        bool open(std::istream& in, const std::string& layout,
                  const std::string& name = "");

        // Declares one port and checks the dump agrees about it. Returns the bind
        // index, or -1 if the dump has no such port or disagrees on width. This is
        // the conformance check. `is_output` selects which side of the recorded value
        // to take: an input takes what the test fixture drove, an output takes what
        // the DUT drove.
        int bind(const std::string& name, std::size_t width, bool is_output,
                 std::size_t bit_offset, bool check = true);

        bool next(replay_vector& out);

        // Next recorded cycle, split into stimulus, expectation and care mask.
        // `x_fill_one` resolves unknown *input* bits to 1 rather than 0.
        bool next_cycle(cycle_element& out, bool x_fill_one);

        // Compares `observed` against the current vector for every checked output,
        // reporting each differing bit. Done here rather than in SystemVerilog
        // because everything needed already lives here: the recorded values, the
        // known-bit mask, the port names, and which ports are outputs at all.
        // Returns the number of mismatching bits.
        // Names the bits set in `bits`, as "port[bit]". The engine reports indices
        // because it knows nothing of ports; the layout lives here.
        std::vector<std::string>
        failing_bits(const std::vector<std::uint32_t>& bits) const;

        const std::string& error() const { return error_; }
        std::size_t words() const { return (total_bits_ + 31) / 32; }

        // Reports every bit as known, so recorded X compares exactly. Meaningful only
        // on a 4-state simulator.
      private:
        struct bound_port {
            std::string name;
            std::size_t dump_index = 0;
            std::size_t width = 0;
            std::size_t bit_offset = 0;
            bool is_output = false;
            bool check = true;
            bool saw_dut_in = false;
            bool saw_dut_out = false;
        };

        void write_bit(std::size_t bit, evcd::drive d);
        void set_port(std::size_t bound,
                      const std::vector<evcd::port_state>& state);
        void fill_port(std::size_t bound, evcd::drive d);
        bool fail(const std::string& msg);

        // Constructed in open(), which is where the stream arrives.
        std::optional<evcd::reader> reader_;
        std::size_t total_bits_ = 0;
        std::string error_;

        std::vector<bound_port> bound_;
        // Dump port index -> bind index, or npos when nothing bound it.
        std::vector<std::size_t> to_bound_;

        // A dump records only what changes, so values carry forward.
        std::vector<logic_word> current_;
        std::vector<bool> recorded_;
        // Ports the dump has written at any point, not just at this timestamp. An
        // expectation persists between elements, so what may be checked must too;
        // `recorded_` alone drops checking on any cycle not changing that output.
        std::vector<bool> ever_recorded_;
    };

    // Resolves `+cvm_replay_file` for `key`: a bare path, or `key=path` pairs.
    std::optional<std::string> resolve_path(const std::string& flag_value,
                                            const std::string& key);

  } // namespace replay
} // namespace cvm
