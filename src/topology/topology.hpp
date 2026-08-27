// SPDX-FileCopyrightText: © 2026 Tenstorrent USA, Inc.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <string>
#include <vector>
#include <utility>
#include <cstdint>
// Downstream translation units rely on this header transitively providing
// <fstream>; drop only alongside a sweep of those consumers.
#include <fstream>

#include "cvm/location_defs.hpp"

namespace cvm {
  namespace topology {

    std::vector<loc_t> get_from_type(const std::string& type);
    std::vector<loc_t> get_from_hierarchy(const std::string& hierarchy);
    loc_t get_from_type(const std::string& type, unsigned id);
    loc_t get_from_hierarchy(const std::string& hierarchy, unsigned id);
    std::pair<bool, uint32_t> attr(loc_t loc, const std::string& attribute);
    std::pair<bool, std::vector<uint32_t>> list_attr(loc_t loc, const std::string& attribute);
    std::string name(loc_t loc);
  }
}
