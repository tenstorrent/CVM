// SPDX-FileCopyrightText: © 2026 Tenstorrent USA, Inc.
// SPDX-License-Identifier: Apache-2.0

#include "cvm/replay_source.hpp"

#include <algorithm>

#include "cvm/logger.hpp"

namespace cvm {
  namespace replay {

    namespace {

      constexpr std::size_t npos = static_cast<std::size_t>(-1);

      // `none` becomes Z: nothing is driving.
      //
      // No `default` on purpose: it is what makes adding a `drive` a build failure
      // here, which is the only compile-time guarantee that a new one gets a 4-state
      // encoding and still fits port_state's three bits.
      void bit_encoding(evcd::drive d, bool& aval, bool& bval) {
        switch (d) {
        case evcd::drive::zero:
          aval = false;
          bval = false;
          break;
        case evcd::drive::one:
          aval = true;
          bval = false;
          break;
        case evcd::drive::highz:
          aval = false;
          bval = true;
          break;
        case evcd::drive::unknown:
          aval = true;
          bval = true;
          break;
        case evcd::drive::none:
          aval = false;
          bval = true;
          break;
        }
      }

    } // namespace

    bool source::fail(const std::string& msg) {
      error_ = msg;
      cvm::log(cvm::ERROR, "Error: cvm::replay: {}\n", msg);
      return false;
    }

    bool source::open(std::istream& in, const std::string& name) {
      reader_.emplace(in, name);
      if (!reader_->ok()) {
        error_ = reader_->error();
        return false;
      }
      to_bound_.assign(reader_->ports().size(), npos);
      return true;
    }

    int source::bind(const std::string& name, std::size_t width, direction dir,
                     std::size_t bit_offset) {
      const auto& dump = reader_->ports();
      for (std::size_t d = 0; d < dump.size(); ++d) {
        if (dump[d].name != name)
          continue;
        if (dump[d].width != width) {
          fail("port `" + name + "` width mismatch: spec says " +
               std::to_string(width) + ", dump says " +
               std::to_string(dump[d].width));
          return -1;
        }
        bound_port bp;
        bp.name = name;
        bp.dump_index = d;
        bp.width = width;
        bp.bit_offset = bit_offset;
        bp.dir = dir;
        bound_.push_back(bp);
        to_bound_[d] = bound_.size() - 1;
        recorded_.push_back(false);
        ever_recorded_.push_back(false);

        // Grow to cover this port. Doing it here rather than in open() means a
        // caller that binds directly gets a correctly sized buffer too.
        total_bits_ = std::max(total_bits_, bit_offset + width);
        if (current_in_.size() < words()) {
          current_in_.resize(words(), logic_word{0xFFFFFFFFu, 0xFFFFFFFFu});
          current_out_.resize(words(), logic_word{0xFFFFFFFFu, 0xFFFFFFFFu});
        }
        return static_cast<int>(bound_.size()) - 1;
      }
      fail("port `" + name + "` is not declared in the dump");
      return -1;
    }

    bool source::require_all_bound(const std::vector<std::string>& exempt) {
      std::string unbound;
      const auto& dump = reader_->ports();
      for (std::size_t d = 0; d < dump.size(); ++d) {
        if (to_bound_[d] != npos)
          continue;
        if (std::find(exempt.begin(), exempt.end(), dump[d].name) != exempt.end())
          continue;
        if (!unbound.empty())
          unbound += ", ";
        unbound += "`" + dump[d].name + "`";
      }
      if (unbound.empty())
        return true;
      return fail("the recording carries " + unbound +
                  ", which the interposer does not replay. Either the spec has "
                  "drifted from the DUT, or this recording is of a different "
                  "configuration");
    }

    void source::write_bit(std::vector<logic_word>& buf, std::size_t bit,
                           evcd::drive d) {
      bool aval = false;
      bool bval = false;
      bit_encoding(d, aval, bval);

      logic_word& w = buf[bit / 32];
      const std::uint32_t mask = 1u << (bit % 32);
      w.aval = aval ? (w.aval | mask) : (w.aval & ~mask);
      w.bval = bval ? (w.bval | mask) : (w.bval & ~mask);
    }

    void source::set_port(std::size_t bound,
                          const std::vector<evcd::port_state>& state) {
      // Both sides, always. Which one matters is the direction's business, and
      // for an `inout` the answer is both.
      const bound_port& bp = bound_[bound];
      for (std::size_t b = 0; b < bp.width; ++b) {
        const evcd::port_state st = state[b];
        write_bit(current_in_, bp.bit_offset + b, st.dut_in());
        write_bit(current_out_, bp.bit_offset + b, st.dut_out());
      }
      recorded_[bound] = true;
      ever_recorded_[bound] = true;
    }

    void source::fill_port(std::size_t bound, evcd::drive d) {
      const bound_port& bp = bound_[bound];
      for (std::size_t b = 0; b < bp.width; ++b) {
        write_bit(current_in_, bp.bit_offset + b, d);
        write_bit(current_out_, bp.bit_offset + b, d);
      }
      recorded_[bound] = true;
      ever_recorded_[bound] = true;
    }

    bool source::next(replay_vector& out) {
      evcd::step step;
      if (!reader_->next(step)) {
        if (!reader_->error().empty())
          error_ = reader_->error();
        return false;
      }

      recorded_.assign(bound_.size(), false);

      if (step.all_ports_unknown) {
        for (std::size_t i = 0; i < bound_.size(); ++i) {
          fill_port(i, evcd::drive::unknown);
        }
      }

      for (const evcd::change& c : step.changes) {
        const std::size_t bound = to_bound_[c.port];
        if (bound == npos)
          continue; // dump port nothing bound

        bound_port& bp = bound_[bound];
        for (const evcd::port_state st : c.state) {
          if (st.dut_in() != evcd::drive::none)
            bp.saw_dut_in = true;
          if (st.dut_out() != evcd::drive::none)
            bp.saw_dut_out = true;
        }

        // Disagreement about direction is an error, not something to
        // reinterpret. An `inout` claims both sides, so neither is a surprise.
        const std::string& name = reader_->ports()[c.port].name;
        if (bp.dir == direction::in && !bp.saw_dut_in && bp.saw_dut_out) {
          return fail("port `" + name +
                      "` is declared `in` but the dump shows only the DUT driving "
                      "it");
        }
        if (bp.dir == direction::out && !bp.saw_dut_out && bp.saw_dut_in) {
          return fail("port `" + name +
                      "` is declared `out` but the dump shows only the test "
                      "fixture driving it");
        }

        set_port(bound, c.state);
      }

      out.time = step.time;
      out.driven_in = current_in_;
      out.driven_out = current_out_;
      out.recorded = recorded_;
      return true;
    }

    bool source::next_cycle(cycle_element& out) {
      replay_vector v;
      if (!next(v))
        return false;

      const std::size_t nw = words();
      out.cycle = v.time;
      out.in.assign(nw, 0u);
      out.drive_en.assign(nw, 0u);
      out.exp.assign(nw, 0u);
      out.care.assign(nw, 0u);

      for (std::size_t i = 0; i < bound_.size(); ++i) {
        const bound_port& bp = bound_[i];
        const bool known_port = i < ever_recorded_.size() && ever_recorded_[i];
        for (std::size_t b = 0; b < bp.width; ++b) {
          const std::size_t bit = bp.bit_offset + b;
          const std::size_t w = bit / 32;
          const std::uint32_t m = 1u << (bit % 32);
          // {aval,bval}: bval set is Z or X, so `!x` is "the side was driving a
          // level". Both sides are read here and the direction picks.
          const bool in_a = (v.driven_in[w].aval & m) != 0;
          const bool in_x = (v.driven_in[w].bval & m) != 0;
          const bool out_a = (v.driven_out[w].aval & m) != 0;
          const bool out_x = (v.driven_out[w].bval & m) != 0;

          switch (bp.dir) {
          case direction::in:
            // Driven unconditionally, and an unknown resolves to 0 so every
            // platform replays identical bits.
            out.drive_en[w] |= m;
            if (!in_x && in_a)
              out.in[w] |= m;
            break;
          case direction::out:
            if (out_a)
              out.exp[w] |= m;
            if (known_port && !out_x)
              out.care[w] |= m;
            break;
          case direction::inout:
            // The recording says which side had the net, bit by bit. Drive
            // where the outside did, and check only where the DUT alone did:
            // a bit this side is driving cannot also be read back off the DUT,
            // because the interposer shares the net rather than isolating it.
            if (!in_x) {
              out.drive_en[w] |= m;
              if (in_a)
                out.in[w] |= m;
            } else {
              if (out_a)
                out.exp[w] |= m;
              if (known_port && !out_x)
                out.care[w] |= m;
            }
            break;
          }
        }
      }
      return true;
    }

    std::vector<std::string>
    source::failing_bits(const std::vector<std::uint32_t>& bits) const {
      std::vector<std::string> out;
      for (const bound_port& bp : bound_) {
        for (std::size_t b = 0; b < bp.width; ++b) {
          const std::size_t bit = bp.bit_offset + b;
          const std::size_t word = bit / 32;
          if (word >= bits.size())
            continue;
          if ((bits[word] & (1u << (bit % 32))) != 0)
            out.push_back(bp.name + "[" + std::to_string(b) + "]");
        }
      }
      return out;
    }
  } // namespace replay
} // namespace cvm
