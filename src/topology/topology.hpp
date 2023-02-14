#pragma once

#include <vector>
#include <fstream>

namespace cvm {
  namespace topology {
    typedef loc_t uint64_t;
    loc_t null = 0;

    // topology_gen implements these
    std::vector<loc_t> get(const std::string& module);
    loc_t get(const std::string& module, int id);
  }
}
