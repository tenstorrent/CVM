// SPDX-FileCopyrightText: © 2026 Tenstorrent USA, Inc.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <string>
#include <vector>
#include <fstream>
#include <optional>
#include <string>
#include <utility>
#include <cstdint>

namespace cvm {
  namespace topology {

    typedef uint32_t loc_t;
    inline constexpr loc_t null = 0;

    std::vector<loc_t> get_from_type(const std::string& type);
    std::vector<loc_t> get_from_hierarchy(const std::string& hierarchy);
    loc_t get_from_type(const std::string& type, unsigned id);
    loc_t get_from_hierarchy(const std::string& hierarchy, unsigned id);
    std::pair<bool, uint32_t> attr(loc_t loc, const std::string& attribute);
    std::pair<bool, std::vector<uint32_t>> list_attr(loc_t loc, const std::string& attribute);
    std::string name(loc_t loc);

    // A plusarg value that is either bare, or `KEY=value` pairs where KEY is a
    // hierarchy path. Bare applies everywhere; a path covers every instance at
    // it.
    inline std::optional<std::string> resolve_keyed(const std::string& flag_value, loc_t loc) {
      if (flag_value.empty())
        return std::nullopt;
      if (flag_value.find('=') == std::string::npos)
        return flag_value;

      std::size_t pos = 0;
      while (pos <= flag_value.size()) {
        const std::size_t comma = flag_value.find(',', pos);
        const std::string entry = flag_value.substr(pos, comma == std::string::npos ? std::string::npos : comma - pos);
        const std::size_t eq = entry.find('=');
        if (eq != std::string::npos) {
          for (const loc_t l : get_from_hierarchy(entry.substr(0, eq))) {
            if (l == loc)
              return entry.substr(eq + 1);
          }
        }
        if (comma == std::string::npos)
          break;
        pos = comma + 1;
      }
      return std::nullopt;
    }
  }
}
