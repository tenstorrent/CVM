#pragma once

#include <vector>
#include <fstream>
#include <utility>
#include <cstdint>

namespace cvm {
  namespace topology {
    typedef uint32_t loc_t;
    const inline loc_t null = 0;

    std::vector<loc_t> get_from_type(const std::string& type);
    std::vector<loc_t> get_from_hierarchy(const std::string& hierarchy);
    loc_t get_from_type(const std::string& type, unsigned id);
    loc_t get_from_hierarchy(const std::string& hierarchy, unsigned id);
    std::pair<bool, uint32_t> attr(loc_t loc, const std::string& attribute);
    std::pair<bool, std::vector<uint32_t>> list_attr(cvm::topology::loc_t loc, const std::string& attribute);
    std::string name(loc_t loc);
  }
}
