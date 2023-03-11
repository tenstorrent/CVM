#pragma once

#include <vector>
#include <fstream>
#include <utility>

namespace cvm {
  namespace topology {
    typedef uint32_t loc_t;
    const inline loc_t null = 0;

    std::vector<loc_t> get(const std::string& module);
    loc_t get(const std::string& module, unsigned id);
    std::pair<bool, uint32_t> attr(const std::string& module, const std::string& attribute);
  }
}
