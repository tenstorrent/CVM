#include "cvm/topology.hpp"
#include <unordered_map>
#include <vector>
#include <memory>
#include <utility>

struct wrapper {
  wrapper() {
%for idx, location in enumerate(topo.locations):
    std::vector<cvm::topology::loc_t> locs_${location.name};
    std::unordered_map<std::string, uint32_t> attrs_${location.name};
  %for instance in location.instances:
    locs_${location.name}.push_back(${instance.loc});
  %endfor
  %for attr in location.attrs:
    %if attr.value.isnumeric():
    attrs_${location.name}["${attr.name}"] = ${attr.value};
    %endif
  %endfor
    attrs.insert({"${location.name}", attrs_${location.name}});
    locs_str.insert({"${location.name}", locs_${location.name}});
    locs_int.insert({${idx}, locs_${location.name}});
%endfor
  }

  std::unordered_map<std::string, std::vector<cvm::topology::loc_t>> locs_str;
  std::unordered_map<uint32_t,    std::vector<cvm::topology::loc_t>> locs_int;
  std::unordered_map<std::string, std::unordered_map<std::string, uint32_t>> attrs;
};

namespace cvm {
  namespace topology {

    auto& wrap() {
      static wrapper wrap_;
      return wrap_;
    }

    std::vector<loc_t> get(const std::string& module) {
      if (not wrap().locs_str.count(module))
        return {};

      return wrap().locs_str.at(module);
    }

    loc_t get(const std::string& module, unsigned id) {
      if (not wrap().locs_str.count(module))
        return null;
      if (size_t(id) >= wrap().locs_str.at(module).size())
        return null;

      return wrap().locs_str.at(module).at(id);
    }

    loc_t get(uint32_t module, unsigned id) {
      if (not wrap().locs_int.count(module))
        return null;
      if (size_t(id) >= wrap().locs_int.at(module).size())
        return null;

      return wrap().locs_int.at(module).at(id);
    }

    std::pair<bool, uint32_t> attr(const std::string& module, const std::string& attribute) {
      if (not wrap().attrs.count(module))
        return std::make_pair(false, uint32_t(0));
      if (not wrap().attrs[module].count(attribute))
        return std::make_pair(false, uint32_t(0));

      return std::make_pair(true, wrap().attrs.at(module).at(attribute));
    }
  }
}

extern "C" {

    uint32_t cvm_topology_get_location(uint32_t module, uint32_t id) {
        return cvm::topology::get(module, id);
    }
}
