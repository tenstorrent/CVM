// SPDX-FileCopyrightText: © 2026 Tenstorrent USA, Inc.
// SPDX-License-Identifier: Apache-2.0

#include "cvm/replay.hpp"

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
      cvm::log(cvm::ERROR, "cvm::replay: {}\n", msg);
      return false;
    }

    bool source::open(std::istream& in, const std::string& layout,
                      const std::string& name) {
      reader_.emplace(in, name);
      if (!reader_->ok()) {
        error_ = reader_->error();
        return false;
      }
      to_bound_.assign(reader_->ports().size(), npos);

      // `name:width:offset:is_output;...`
      std::size_t pos = 0;
      while (pos < layout.size()) {
        const std::size_t end = layout.find(';', pos);
        const std::string record = layout.substr(
            pos, end == std::string::npos ? std::string::npos : end - pos);
        pos = (end == std::string::npos) ? layout.size() : end + 1;
        if (record.empty())
          continue;

        std::string field[5];
        std::size_t fp = 0;
        bool ok = true;
        for (int i = 0; i < 5; ++i) {
          const std::size_t colon = record.find(':', fp);
          field[i] = record.substr(
              fp, colon == std::string::npos ? std::string::npos : colon - fp);
          if (colon == std::string::npos) {
            ok = (i == 4);
            break;
          }
          fp = colon + 1;
        }
        if (!ok)
          return fail("malformed layout record `" + record + "`");

        const std::size_t width = std::stoul(field[1]);
        const std::size_t offset = std::stoul(field[2]);
        if (bind(field[0], width, field[3] == "1", offset, field[4] == "1") < 0) {
          return false;
        }
      }
      return true;
    }

    int source::bind(const std::string& name, std::size_t width, bool is_output,
                     std::size_t bit_offset, bool check) {
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
        bp.is_output = is_output;
        bp.check = check;
        bound_.push_back(bp);
        to_bound_[d] = bound_.size() - 1;
        recorded_.push_back(false);
        ever_recorded_.push_back(false);

        // Grow to cover this port. Doing it here rather than in open() means a
        // caller that binds directly gets a correctly sized buffer too.
        total_bits_ = std::max(total_bits_, bit_offset + width);
        if (current_.size() < words()) {
          current_.resize(words(), logic_word{0xFFFFFFFFu, 0xFFFFFFFFu});
        }
        return static_cast<int>(bound_.size()) - 1;
      }
      fail("port `" + name + "` is not declared in the dump");
      return -1;
    }

    void source::write_bit(std::size_t bit, evcd::drive d) {
      bool aval = false;
      bool bval = false;
      bit_encoding(d, aval, bval);

      logic_word& w = current_[bit / 32];
      const std::uint32_t mask = 1u << (bit % 32);
      w.aval = aval ? (w.aval | mask) : (w.aval & ~mask);
      w.bval = bval ? (w.bval | mask) : (w.bval & ~mask);
    }

    void source::set_port(std::size_t bound,
                          const std::vector<evcd::port_state>& state) {
      const bound_port& bp = bound_[bound];
      for (std::size_t b = 0; b < bp.width; ++b) {
        const evcd::port_state st = state[b];
        write_bit(bp.bit_offset + b,
                  bp.is_output ? st.dut_out() : st.dut_in());
      }
      recorded_[bound] = true;
      ever_recorded_[bound] = true;
    }

    void source::fill_port(std::size_t bound, evcd::drive d) {
      const bound_port& bp = bound_[bound];
      for (std::size_t b = 0; b < bp.width; ++b) {
        write_bit(bp.bit_offset + b, d);
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

        // Disagreement about direction is an error, not something to reinterpret.
        const std::string& name = reader_->ports()[c.port].name;
        if (!bp.is_output && !bp.saw_dut_in && bp.saw_dut_out) {
          return fail("port `" + name +
                      "` is declared `in` but the dump shows only the DUT driving "
                      "it");
        }
        if (bp.is_output && !bp.saw_dut_out && bp.saw_dut_in) {
          return fail("port `" + name +
                      "` is declared `out` but the dump shows only the test "
                      "fixture driving it");
        }

        set_port(bound, c.state);
      }

      out.time = step.time;
      out.value = current_;
      out.recorded = recorded_;
      return true;
    }

    bool source::next_cycle(cycle_element& out, bool x_fill_one) {
      replay_vector v;
      if (!next(v))
        return false;

      const std::size_t nw = words();
      out.cycle = v.time;
      out.in.assign(nw, 0u);
      out.exp.assign(nw, 0u);
      out.care.assign(nw, 0u);

      for (std::size_t i = 0; i < bound_.size(); ++i) {
        const bound_port& bp = bound_[i];
        const bool known_port = i < ever_recorded_.size() && ever_recorded_[i];
        for (std::size_t b = 0; b < bp.width; ++b) {
          const std::size_t bit = bp.bit_offset + b;
          const std::size_t w = bit / 32;
          const std::uint32_t m = 1u << (bit % 32);
          const bool a = (v.value[w].aval & m) != 0;
          const bool x = (v.value[w].bval & m) != 0;

          if (!bp.is_output) {
            if (x ? x_fill_one : a)
              out.in[w] |= m;
          } else {
            if (a)
              out.exp[w] |= m;
            if (bp.check && known_port && !x)
              out.care[w] |= m;
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

    std::optional<std::string>
    resolve_path(const std::string& flag_value, const std::string& key) {
      if (flag_value.empty())
        return std::nullopt;
      // A bare path applies to every instance.
      if (flag_value.find('=') == std::string::npos)
        return flag_value;

      std::size_t pos = 0;
      while (pos <= flag_value.size()) {
        const std::size_t comma = flag_value.find(',', pos);
        const std::string entry = flag_value.substr(
            pos, comma == std::string::npos ? std::string::npos : comma - pos);
        const std::size_t eq = entry.find('=');
        if (eq != std::string::npos && entry.substr(0, eq) == key) {
          return entry.substr(eq + 1);
        }
        if (comma == std::string::npos)
          break;
        pos = comma + 1;
      }
      return std::nullopt;
    }

  } // namespace replay
} // namespace cvm
