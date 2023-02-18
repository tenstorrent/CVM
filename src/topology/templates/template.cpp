#include "cvm/topology.hpp"
#include <unordered_map>
#include <vector>
#include <memory>

struct wrapper {
  wrapper() {
%for idx, location in enumerate(topo.locations):
    std::vector<cvm::topology::loc_t> locs_${location.name};
  %for instance in location.instances:
    locs_${location.name}.push_back(${instance.loc});
  %endfor
    locs0.insert({"${location.name}", locs_${location.name}});
    locs1.insert({${idx}, locs_${location.name}});
%endfor
  }

  std::unordered_map<std::string, std::vector<cvm::topology::loc_t>> locs0;
  std::unordered_map<uint32_t,    std::vector<cvm::topology::loc_t>> locs1;
};

namespace cvm {
  namespace topology {
    loc_t null = 0;
    std::unique_ptr<wrapper> wrap;

    std::vector<loc_t> get(const std::string& module) {
      if (not wrap)
        wrap = std::make_unique<wrapper>();

      if (not wrap->locs0.count(module))
        return {};

      return wrap->locs0.at(module);
    }

    loc_t get(const std::string& module, unsigned id) {
      if (not wrap)
        wrap = std::make_unique<wrapper>();

      if (not wrap->locs0.count(module))
        return null;
      if (size_t(id) >= wrap->locs0.at(module).size())
        return null;

      return wrap->locs0.at(module).at(id);
    }

    loc_t get(uint32_t module, unsigned id) {
      if (not wrap)
        wrap = std::make_unique<wrapper>();

      if (not wrap->locs1.count(module))
        return null;
      if (size_t(id) >= wrap->locs1.at(module).size())
        return null;

      return wrap->locs1.at(module).at(id);
    }
  }
}

extern "C" {

    uint64_t cvm_topology_get_location(uint32_t module, uint32_t id) {
        return cvm::topology::get(module, id);
    }
}
