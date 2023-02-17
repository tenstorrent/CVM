#pragma once

#include <vector>
#include <fstream>

namespace cvm {
  namespace topology {
    typedef uint64_t loc_t;
    extern loc_t null;

    std::vector<loc_t> get(const std::string& module);
    loc_t get(const std::string& module, unsigned id);
  }
}
