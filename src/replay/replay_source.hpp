// SPDX-FileCopyrightText: © 2026 Tenstorrent USA, Inc.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstddef>
#include <cstdint>
#include <istream>
#include <optional>
#include <string>
#include <vector>

#include "cvm/replay_cycle_element.hpp"
#include "cvm/evcd.hpp"

namespace cvm {
  namespace replay {

    // A recording, read as the cycles the engine replays. The port layout
    // arrives at runtime through open() rather than being generated a second
    // time in C++.
    class source {
      public:
        // `layout` is the port list as `name:width:offset:is_output:check;...`,
        // emitted by the generated interposer: one call rather than a per-port
        // handshake back across the DPI boundary. Binding is the conformance
        // check, so a dump that disagrees fails here. `name` appears in parse
        // diagnostics; pass the path `in` came from.
        bool open(std::istream& in, const std::string& layout,
                  const std::string& name = "");

        // Next recorded cycle, split into stimulus, expectation and care mask.
        // An emulator has no X, so an unknown *input* bit is resolved to 0 here
        // and every platform replays identical bits.
        bool next_cycle(cycle_element& out);

        // Names the bits set in `bits`, as "port[bit]". The engine reports
        // indices because it knows nothing of ports; the layout lives here.
        std::vector<std::string>
        failing_bits(const std::vector<std::uint32_t>& bits) const;

        // Width of the flattened boundary the layout describes.
        std::size_t total_bits() const { return total_bits_; }

        const std::string& error() const { return error_; }

      private:
        // {bval,aval}: 00=0, 01=1, 10=Z, 11=X. The recording is 4-state; only
        // next_cycle()'s output is not.
        struct logic_word {
            std::uint32_t aval = 0;
            std::uint32_t bval = 0;
        };

        // One timestamp of the flattened vector. `recorded` marks ports the
        // dump wrote at this timestamp, indexed by bind order.
        struct replay_vector {
            std::uint64_t time = 0;
            std::vector<logic_word> value;
            std::vector<bool> recorded;
        };

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

        // Declares one port and checks the dump agrees about it. Returns the
        // bind index, or -1. `is_output` selects which side of the recorded
        // value to take: what the fixture drove, or what the DUT did.
        int bind(const std::string& name, std::size_t width, bool is_output,
                 std::size_t bit_offset, bool check = true);
        bool next(replay_vector& out);

        void write_bit(std::size_t bit, evcd::drive d);
        void set_port(std::size_t bound,
                      const std::vector<evcd::port_state>& state);
        void fill_port(std::size_t bound, evcd::drive d);
        bool fail(const std::string& msg);

        std::size_t words() const { return (total_bits_ + 31) / 32; }

        std::optional<evcd::reader> reader_;
        std::size_t total_bits_ = 0;
        std::string error_;

        std::vector<bound_port> bound_;
        // Dump port index -> bind index, or npos when nothing bound it.
        std::vector<std::size_t> to_bound_;

        // A dump records only what changes, so values carry forward.
        std::vector<logic_word> current_;
        std::vector<bool> recorded_;
        // Ports the dump has written at any point, not just at this timestamp.
        // An expectation persists between elements, so what may be checked must
        // too; `recorded_` alone drops checking on any cycle not changing that
        // output.
        std::vector<bool> ever_recorded_;
    };

  } // namespace replay
} // namespace cvm
