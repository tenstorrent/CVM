// SPDX-FileCopyrightText: © 2026 Tenstorrent USA, Inc.
// SPDX-License-Identifier: Apache-2.0

#include "cvm/evcd.hpp"

#include <cctype>
#include <cstdlib>
#include <utility>

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

      bool is_space(char c) {
        return std::isspace(static_cast<unsigned char>(c)) != 0;
      }

    } // namespace

    reader::reader(std::istream& in, std::string name)
        : in_(in), name_(std::move(name)) {
      read_header();
    }

    bool reader::fail(const std::string& msg) {
      state_ = state::failed;
      error_ = (name_.empty() ? std::string("<stream>") : name_) + ":" +
               std::to_string(line_no_) + ": " + msg;
      // The offending line, because a state character or identifier code means
      // little without the text it came from.
      cvm::log(cvm::ERROR, "Error: cvm::evcd: {}\n  {}\n", error_, line_);
      return false;
    }

    // Whitespace-delimited, crossing lines as needed, tracking which line we are on
    // so fail() can quote it.
    bool reader::token(std::string& t) {
      for (;;) {
        while (pos_ < line_.size() && is_space(line_[pos_]))
          ++pos_;
        if (pos_ < line_.size()) {
          const std::size_t start = pos_;
          while (pos_ < line_.size() && !is_space(line_[pos_]))
            ++pos_;
          t.assign(line_, start, pos_ - start);
          return true;
        }
        if (!std::getline(in_, line_)) {
          if (state_ != state::failed)
            state_ = state::at_end;
          return false;
        }
        ++line_no_;
        pos_ = 0;
      }
    }

    bool reader::skip_to_end() {
      std::string t;
      while (token(t)) {
        if (t == "$end")
          return true;
      }
      return false;
    }

    // Some writers emit `<` and the code as separate tokens.
    bool reader::identifier(std::string& id) {
      if (!token(id))
        return false;
      if (id == "<") {
        std::string rest;
        if (!token(rest))
          return false;
        id += rest;
      }
      return true;
    }

    bool reader::parse_var() {
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
      if (!identifier(id))
        return fail("truncated $var identifier");

      std::string reference;
      if (!token(reference))
        return fail("truncated $var reference");

      const auto [it, fresh] = port_of_id_.emplace(id, ports_.size());
      if (!fresh)
        return fail("identifier code `" + id + "` declared twice");
      ports_.push_back(dump_port{reference, width});
      return skip_to_end();
    }

    bool reader::read_header() {
      int scopes = 0;

      std::string t;
      while (token(t)) {
        if (t == "$var") {
          if (!parse_var())
            return false;
        } else if (t == "$scope") {
          if (++scopes > 1) {
            return fail("dump has more than one $scope; flattening could match the "
                        "wrong port, so this is rejected rather than guessed at");
          }
          if (!skip_to_end())
            return fail("truncated $scope");
        } else if (t == "$enddefinitions") {
          if (!skip_to_end())
            return fail("truncated $enddefinitions");
          state_ = state::dumping;
          return true;
        } else if (!t.empty() && t.front() == '$') {
          // $timescale included: replay applies recorded deltas as raw numbers.
          if (!skip_to_end())
            return fail("truncated " + t);
        } else {
          return fail("unexpected token `" + t + "` in header");
        }
      }

      return fail("no $enddefinitions; header is truncated");
    }

    bool reader::next(step& out) {
      // Returns false rather than failing again: error() already says why, and
      // overwriting it would hide the header diagnostic.
      if (!ok())
        return false;
      if (state_ == state::at_end && !have_pending_)
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
          state_ = state::dumping;
          continue;
        }
        if (t == "$dumpportsoff") {
          state_ = state::paused;
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
          std::string value = t.substr(1);
          if (value.empty() && !token(value))
            return fail("truncated value change: no state characters after `p`");

          std::string a;
          if (!token(a))
            return fail("truncated value change after state");
          std::string id;
          if (!a.empty() && a.front() == '<') {
            id = a; // writer omitted the strength components
          } else {
            // Strengths are discarded: replay reconstructs logic values, not
            // analogue drive contention.
            std::string b;
            if (!token(b))
              return fail("truncated value change strengths");
            if (!identifier(id))
              return fail("truncated value change identifier");
          }

          const auto it = port_of_id_.find(id);
          if (it == port_of_id_.end())
            return fail("identifier code `" + id + "` was never declared");
          const std::size_t index = it->second;

          if (value.size() != ports_[index].width) {
            return fail("port `" + ports_[index].name + "` expects " +
                        std::to_string(ports_[index].width) +
                        " state character(s), dump supplied " +
                        std::to_string(value.size()) + " (`" + value + "`)");
          }

          // Decoded here rather than validated and handed on: every character has
          // to be looked at anyway, and the consumer then never sees the format.
          std::vector<port_state> decoded(value.size());
          for (std::size_t i = 0; i < value.size(); ++i) {
            const std::optional<port_state> st = decode_state(value[i]);
            if (!st.has_value()) {
              return fail(std::string("unknown state character `") + value[i] +
                          "` on port `" + ports_[index].name +
                          "` (IEEE 1364-2005 subclause 18.4.3.1)");
            }
            // Characters are MSB first; store LSB first so state[b] is bit b.
            decoded[value.size() - 1 - i] = *st;
          }

          if (state_ == state::dumping)
            out.changes.push_back(change{index, std::move(decoded)});
          continue;
        }

        return fail("unexpected token `" + t + "` in value change section");
      }

      return emit;
    }

  } // namespace evcd
} // namespace cvm
