// SPDX-FileCopyrightText: © 2026 Tenstorrent USA, Inc.
// SPDX-License-Identifier: Apache-2.0

#include "cvm/evcd.hpp"

#include <cstdlib>

#include "cvm/logger.hpp"

namespace cvm {
namespace evcd {

namespace {

// $var size is either `1` or `[msb:lsb]`.
bool width_from_size(const std::string& size, std::size_t& width) {
  if (size.empty())
    return false;
  if (size.front() != '[') {
    char* end = nullptr;
    const long v = std::strtol(size.c_str(), &end, 10);
    if (end == size.c_str() || v <= 0)
      return false;
    width = static_cast<std::size_t>(v);
    return true;
  }
  const std::size_t colon = size.find(':');
  const std::size_t close = size.find(']');
  if (colon == std::string::npos || close == std::string::npos)
    return false;
  const long msb = std::strtol(size.substr(1, colon - 1).c_str(), nullptr, 10);
  const long lsb = std::strtol(
      size.substr(colon + 1, close - colon - 1).c_str(), nullptr, 10);
  width = static_cast<std::size_t>((msb > lsb ? msb - lsb : lsb - msb) + 1);
  return true;
}

// Identifier codes are `<N`, assigned sequentially from zero, so they index
// directly instead of hashing strings in the hot loop.
bool identifier_index(const std::string& tok, std::size_t& index) {
  if (tok.size() < 2 || tok.front() != '<')
    return false;
  char* end = nullptr;
  const long v = std::strtol(tok.c_str() + 1, &end, 10);
  if (end == tok.c_str() + 1 || v < 0)
    return false;
  index = static_cast<std::size_t>(v);
  return true;
}

} // namespace

bool parser::fail(const std::string& msg) {
  error_ = msg;
  cvm::log(cvm::ERROR, "cvm::evcd: {}\n", msg);
  return false;
}

bool parser::token(std::string& t) {
  if (in_ == nullptr)
    return false;
  if (!(*in_ >> t)) {
    eof_ = true;
    return false;
  }
  return true;
}

bool parser::skip_to_end() {
  std::string t;
  while (token(t)) {
    if (t == "$end")
      return true;
  }
  return false;
}

bool parser::parse_var() {
  std::string type;
  if (!token(type))
    return fail("truncated $var");
  if (type != "port")
    return fail("$var type must be `port`, got `" + type + "`");

  std::string size;
  if (!token(size))
    return fail("truncated $var size");
  std::size_t width = 0;
  if (!width_from_size(size, width))
    return fail("bad $var size `" + size + "`");

  std::string id;
  if (!token(id))
    return fail("truncated $var identifier");
  // Tolerate `<` and the integer arriving as separate tokens.
  if (id == "<") {
    std::string digits;
    if (!token(digits))
      return fail("truncated $var identifier");
    id = "<" + digits;
  }
  std::size_t index = 0;
  if (!identifier_index(id, index)) {
    return fail("bad $var identifier code `" + id + "`");
  }

  std::string reference;
  if (!token(reference))
    return fail("truncated $var reference");

  if (ports_.size() <= index)
    ports_.resize(index + 1);
  ports_[index].name = reference;
  ports_[index].width = width;
  return skip_to_end();
}

bool parser::open(std::istream& in) {
  in_ = &in;
  int scopes = 0;

  std::string t;
  while (token(t)) {
    if (t == "$var") {
      if (!parse_var())
        return false;
    } else if (t == "$scope") {
      if (++scopes > 1) {
        return fail(
            "dump has more than one $scope; flattening could match the "
            "wrong port, so this is rejected rather than guessed at");
      }
      if (!skip_to_end())
        return fail("truncated $scope");
    } else if (t == "$enddefinitions") {
      if (!skip_to_end())
        return fail("truncated $enddefinitions");
      header_done_ = true;
      break;
    } else if (!t.empty() && t.front() == '$') {
      // $timescale included: replay applies recorded deltas as raw numbers.
      if (!skip_to_end())
        return fail("truncated " + t);
    } else {
      return fail("unexpected token `" + t + "` in header");
    }
  }

  if (!header_done_)
    return fail("no $enddefinitions; header is truncated");
  return true;
}

bool parser::next(step& out) {
  if (in_ == nullptr || !header_done_) {
    return fail("next() called before a successful open()");
  }
  if (eof_ && !have_pending_)
    return false;

  out.changes.clear();
  out.all_ports_unknown = false;

  std::string t;
  bool emit = false;

  while (true) {
    if (!token(t)) {
      emit = have_pending_;
      if (emit)
        out.time = pending_time_;
      have_pending_ = false;
      break;
    }

    if (t.front() == '#') {
      const std::uint64_t stamp = std::strtoull(t.c_str() + 1, nullptr, 10);
      if (have_pending_) {
        out.time = pending_time_;
        pending_time_ = stamp;
        emit = true;
        break;
      }
      pending_time_ = stamp;
      have_pending_ = true;
      continue;
    }

    if (t == "$dumpports" || t == "$dumpportsall" || t == "$dumpportson") {
      dumping_ = true;
      continue;
    }
    if (t == "$dumpportsoff") {
      dumping_ = false;
      out.all_ports_unknown = true;
      continue;
    }
    if (t == "$end")
      continue;
    if (t.front() == '$') {
      if (!skip_to_end())
        break;
      continue;
    }

    if (t.front() == 'p') {
      std::string state = t.substr(1);
      if (state.empty() && !token(state)) {
        return fail("truncated value change: no state characters after `p`");
      }
      std::string a;
      if (!token(a))
        return fail("truncated value change after state");
      std::string id;
      if (!a.empty() && a.front() == '<') {
        id = a; // writer omitted the strength components
      } else {
        std::string b;
        if (!token(b))
          return fail("truncated value change strengths");
        if (!token(id))
          return fail("truncated value change identifier");
      }
      // Strength digits are discarded: replay reconstructs logic values, not
      // analogue drive contention.
      std::size_t index = 0;
      if (!identifier_index(id, index)) {
        return fail("bad identifier code `" + id + "` in value change");
      }
      if (index >= ports_.size()) {
        return fail("identifier code `" + id + "` was never declared");
      }
      if (state.size() != ports_[index].width) {
        return fail("port `" + ports_[index].name + "` expects " +
                    std::to_string(ports_[index].width) +
                    " state character(s), dump supplied " +
                    std::to_string(state.size()) + " (`" + state + "`)");
      }
      for (const char c : state) {
        if (!decode_state(c).has_value()) {
          return fail(std::string("unknown state character `") + c +
                      "` on port `" + ports_[index].name +
                      "` (IEEE 1364-2005 subclause 18.4.3.1)");
        }
      }
      if (dumping_)
        out.changes.push_back(change{index, std::move(state)});
      continue;
    }

    return fail("unexpected token `" + t + "` in value change section");
  }

  return emit;
}

} // namespace evcd
} // namespace cvm
