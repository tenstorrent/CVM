// SPDX-FileCopyrightText: © 2026 Tenstorrent USA, Inc.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstddef>
#include <cstdint>
#include <istream>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace cvm {
  namespace evcd {

    enum class drive : std::uint8_t { none,
                                      zero,
                                      one,
                                      unknown,
                                      highz };

    class port_state {
      public:
        constexpr port_state() = default;
        constexpr port_state(drive in, drive out) : bits_(pack(in, out)) {}

        constexpr drive dut_in() const { return static_cast<drive>(bits_ & kMask); }
        constexpr drive dut_out() const {
          return static_cast<drive>(bits_ >> kBits);
        }

        bool operator==(const port_state&) const = default;

      private:
        static constexpr unsigned kBits = 3;
        static constexpr unsigned kMask = (1u << kBits) - 1u;

        static constexpr std::uint8_t pack(drive in, drive out) {
          if (static_cast<unsigned>(in) > kMask || static_cast<unsigned>(out) > kMask)
            throw "drive no longer fits in three bits";
          return static_cast<std::uint8_t>(static_cast<unsigned>(in) |
                                           (static_cast<unsigned>(out) << kBits));
        }

        std::uint8_t bits_ = 0;
    };

    static_assert(sizeof(port_state) == 1, "a decoded run must not cost more than "
                                           "the characters it replaces");
    static_assert(port_state{drive::highz, drive::highz}.dut_in() == drive::highz);
    static_assert(port_state{drive::highz, drive::highz}.dut_out() == drive::highz);
    static_assert(port_state{}.dut_in() == drive::none);

    // nullopt means parse error
    std::optional<port_state> decode_state(char c);

    // A port as the dump declares it.
    struct dump_port {
        std::string name;
        std::size_t width = 0;
    };

    // One port's recorded value, decoded, LSB first: state[b] is bit b.
    struct change {
        std::size_t port = 0; // index into reader::ports()
        std::vector<port_state> state;
    };

    struct step {
        std::uint64_t time = 0;
        std::vector<change> changes;
        // $dumpportsoff  - recording paused, drive X and don't check
        bool all_ports_unknown = false;
    };

    // Streaming EVCD reader
    class reader {
      public:
        // Reads the header. `name` appears in diagnostics; pass the path the stream
        // came from. Check ok() before calling next().
        reader(std::istream& in, std::string name);

        bool ok() const { return state_ != state::failed; }
        bool next(step& out);

        const std::vector<dump_port>& ports() const { return ports_; }
        const std::string& error() const { return error_; }

      private:
        enum class state : std::uint8_t {
          header,  // before $enddefinitions
          dumping, // recording value changes
          paused,  // between $dumpportsoff and $dumpportson
          at_end,  // stream exhausted
          failed,
        };

        bool read_header();
        bool token(std::string& t);
        bool skip_to_end();
        bool parse_var();
        bool identifier(std::string& id);
        bool fail(const std::string& msg);

        std::istream& in_;
        std::string name_;

        std::vector<dump_port> ports_;
        std::unordered_map<std::string, std::size_t> port_of_id_;

        std::string error_;
        state state_ = state::header;

        // Tokenizing is line-based so a failure can quote the line it happened on.
        std::string line_;
        std::size_t pos_ = 0;
        std::size_t line_no_ = 0;

        std::uint64_t pending_time_ = 0;
        bool have_pending_ = false;
    };

  } // namespace evcd
} // namespace cvm
