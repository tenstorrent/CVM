#include "cvm/topology.hpp"
#include <unordered_map>
#include <vector>
#include <memory>
#include <utility>

struct wrapper {
  wrapper() {
%for type in topo.types:
    std::vector<cvm::topology::loc_t> locs_${type};
%endfor
%for location in topo.locations:
    std::vector<cvm::topology::loc_t> locs_${location.name}_${location.path_id};
    std::unordered_map<std::string, uint32_t> attrs_${location.name}_${location.path_id};
    attrs_${location.name}_${location.path_id}["SHARD"] = ${location.shard};
    attrs_${location.name}_${location.path_id}["TOTAL"] = ${len(location.instances)};
    %for (name, value) in location.attributes:
      %if value.isnumeric():
    attrs_${location.name}_${location.path_id}["${name.upper()}"] = ${value};
      %endif
    %endfor
  %for instance in location.instances:
    locs_${location.name}_${location.path_id}.push_back(${instance.loc});
    %for typ in location.types:
    locs_${typ}.push_back(${instance.loc});
    %endfor
    names[${instance.loc}] = "${location.name.upper()}";
  %if location.attributes:
    attrs[${instance.loc}] = attrs_${location.name}_${location.path_id};
  %endif
  %endfor
    str_hierarchy.insert({"${location.path}", locs_${location.name}_${location.path_id}});
    int_hierarchy.insert({${location.path_id}, locs_${location.name}_${location.path_id}});
%endfor
%for type in topo.types:
    str_type["${type.upper()}"] = locs_${type};
%endfor
  }

  std::unordered_map<std::string, std::vector<cvm::topology::loc_t>> str_hierarchy;
  std::unordered_map<uint32_t,    std::vector<cvm::topology::loc_t>> int_hierarchy;
  std::unordered_map<std::string, std::vector<cvm::topology::loc_t>> str_type;
  std::unordered_map<cvm::topology::loc_t, std::unordered_map<std::string, uint32_t>> attrs;
  std::unordered_map<cvm::topology::loc_t, std::string> names;
};

namespace cvm {
  namespace topology {

    auto& wrap() {
      static wrapper wrap_;
      return wrap_;
    }

    std::vector<loc_t> get_from_type(const std::string& type) {
      try {
        return wrap().str_type.at(type);
      }
      catch (...) {
        return {};
      }
    }

    std::vector<loc_t> get_from_hierarchy(const std::string& hierarchy) {
      try {
        return wrap().str_hierarchy.at(hierarchy);
      }
      catch (...) {
        return {};
      }
    }

    loc_t get_from_type(const std::string& type, unsigned id) {
      try {
          return wrap().str_type.at(type).at(id);
      }
      catch (...) {
        return null;
      }
    }

    loc_t get_from_hierarchy(const std::string& hierarchy, unsigned id) {
      try {
          return wrap().str_hierarchy.at(hierarchy).at(id);
      }
      catch (...) {
        return null;
      }
    }

    loc_t get(uint32_t hierarchy, unsigned id) {
      try {
        return wrap().int_hierarchy.at(hierarchy).at(id);
      }
      catch (...) {
        return null;
      }
    }

    std::pair<bool, uint32_t> attr(cvm::topology::loc_t loc, const std::string& attribute) {
      try {
        return std::make_pair(true, wrap().attrs.at(loc).at(attribute));
      }
      catch (...) {
        return std::make_pair(false, uint32_t(0));
      }
    }

    std::string name(cvm::topology::loc_t loc) {
      try {
        return wrap().names.at(loc);
      }
      catch (...) {
        return {};
      }
    }
  }
}

extern "C" {

    uint32_t cvm_topology_get_location(uint32_t hierarchy, uint32_t id) {
        return cvm::topology::get(hierarchy, id);
    }
}
