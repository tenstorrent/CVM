#include "cvm/topology.hpp"

struct wrapper {
  wrapper() {
%for location in topo.locations:
    std::vector<loc_t> locs_${location.name};
  %for instance in location.instances:
    locs_${location.name}[${instance.real_id}] = ${instance.loc}
  %endfor
    locs.emplace({"${location.name}", locs_${location.name}});
%endfor
  }

  std::unordered_map<std::string, std::vector<loc_t>> locs;
}

static wrapper wrap;

std::vector<cvm::topology::loc_t>
cvm::topology::get(const std::string& module) {
  if (not wrap.locs.count(module))
    return cvm::topology::null;

  return locs.at(module);
}

cvm::topology::loc_t
cvm::topology::get(const std::string& module, int id) {
  if (not wrap.locs.count(module))
    return cvm::topology::null;
  if (id >= wrap.locs.at(module).size())
    return cvm::topology::null;

  return locs.at(instance).at(id);
}
