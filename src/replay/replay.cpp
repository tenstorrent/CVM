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

char four_state(bool aval, bool bval) {
  if (!bval)
    return aval ? '1' : '0';
  return aval ? 'x' : 'z';
}

} // namespace

bool source::fail(const std::string& msg) {
  error_ = msg;
  cvm::log(cvm::ERROR, "cvm::replay: {}\n", msg);
  return false;
}

bool source::open(std::istream& in, const std::string& layout) {
  if (!parser_.open(in)) {
    error_ = parser_.error();
    return false;
  }
  to_bound_.assign(parser_.ports().size(), npos);

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
  const auto& dump = parser_.ports();
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

void source::set_port(std::size_t bound, const std::string& state) {
  const bound_port& bp = bound_[bound];
  for (std::size_t b = 0; b < bp.width; ++b) { // state chars are MSB first
    const evcd::port_state st = *evcd::decode_state(state[b]);
    write_bit(bp.bit_offset + (bp.width - 1 - b),
              bp.is_output ? st.dut : st.ext);
  }
  recorded_[bound] = true;
}

void source::fill_port(std::size_t bound, evcd::drive d) {
  const bound_port& bp = bound_[bound];
  for (std::size_t b = 0; b < bp.width; ++b) {
    write_bit(bp.bit_offset + b, d);
  }
  recorded_[bound] = true;
}

bool source::next(replay_vector& out) {
  evcd::step step;
  if (!parser_.next(step)) {
    if (!parser_.error().empty())
      error_ = parser_.error();
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
    for (const char ch : c.state) {
      const evcd::port_state st = *evcd::decode_state(ch);
      if (st.ext != evcd::drive::none)
        bp.saw_ext = true;
      if (st.dut != evcd::drive::none)
        bp.saw_dut = true;
    }

    // Disagreement about direction is an error, not something to reinterpret.
    const std::string& name = parser_.ports()[c.port].name;
    if (!bp.is_output && !bp.saw_ext && bp.saw_dut) {
      return fail("port `" + name +
                  "` is declared `in` but the dump shows only the DUT driving "
                  "it");
    }
    if (bp.is_output && !bp.saw_dut && bp.saw_ext) {
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

std::size_t
source::check(const std::vector<logic_word>& observed, std::uint64_t sim_time,
              const std::string& hier) {
  std::size_t mismatches = 0;

  for (std::size_t i = 0; i < bound_.size(); ++i) {
    const bound_port& bp = bound_[i];
    if (!bp.is_output || !bp.check)
      continue;
    // Compared exactly when the recording wrote this port, not on every vector.
    if (i >= recorded_.size() || !recorded_[i])
      continue;

    for (std::size_t b = 0; b < bp.width; ++b) {
      const std::size_t bit = bp.bit_offset + b;
      if (bit / 32 >= observed.size() || bit / 32 >= current_.size())
        break;
      const std::uint32_t mask = 1u << (bit % 32);

      const bool exp_a = (current_[bit / 32].aval & mask) != 0;
      const bool exp_b = (current_[bit / 32].bval & mask) != 0;
      // Unknown bits are skipped unless strict: a 2-state simulator can never
      // reproduce a recorded X.
      if (exp_b && !strict_x_)
        continue;

      const bool act_a = (observed[bit / 32].aval & mask) != 0;
      const bool act_b = (observed[bit / 32].bval & mask) != 0;
      if (act_a == exp_a && act_b == exp_b)
        continue;

      ++mismatches;
      cvm::log(cvm::ERROR,
               "cvm::replay: {}: {}[{}] mismatch at {}: expected {}, got {}\n",
               hier, bp.name, b, sim_time, four_state(exp_a, exp_b),
               four_state(act_a, act_b));
    }
  }
  return mismatches;
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
