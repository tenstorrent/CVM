#pragma once

#include <vector>
#include <fstream>

namespace cvm {
  namespace topology {
    typedef uint64_t loc_t;
    extern loc_t null;

    // topology_gen implements these
    // extern "C" std::vector<loc_t> get_all(const std::string& module);
    // extern "C" loc_t get_one(const std::string& module, unsigned id);

    std::vector<loc_t> get(const std::string& module);

    loc_t get(const std::string& module, unsigned id);
  }
}
