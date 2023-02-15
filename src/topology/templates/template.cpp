#include "cvm/topology.hpp"
#include <unordered_map>
#include <vector>
#include <memory>

struct wrapper {
  wrapper() {
%for location in topo.locations:
    std::vector<cvm::topology::loc_t> locs_${location.name};
  %for instance in location.instances:
    locs_${location.name}.push_back(${instance.loc});
  %endfor
    locs.insert({"${location.name}", locs_${location.name}});
%endfor
  }

  std::unordered_map<std::string, std::vector<cvm::topology::loc_t>> locs;
};

namespace cvm {
  namespace topology {
    loc_t null = 0;
    std::unique_ptr<wrapper> wrap;

    std::vector<loc_t> get(const std::string& module) {
      if (not wrap)
        wrap = std::make_unique<wrapper>();

      if (not wrap->locs.count(module))
        return {};

      return wrap->locs.at(module);
    }

    loc_t get(const std::string& module, unsigned id) {
      if (not wrap)
        wrap = std::make_unique<wrapper>();

      if (not wrap->locs.count(module))
        return null;
      if (size_t(id) >= wrap->locs.at(module).size())
        return null;

      return wrap->locs.at(module).at(id);
    }
  }
}
