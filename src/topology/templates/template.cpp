#include "cvm/topology.hpp"
#include <unordered_map>
#include <vector>
#include <memory>
#include <utility>

<%
  def strip(name: str):
    return name.strip('~')
%>
struct wrapper {
  wrapper() {
%for location in topo.locations:
    std::vector<cvm::topology::loc_t> locs_${strip(location.name)}_${location.path_id};
  %for instance in location.instances:
    locs_${strip(location.name)}_${location.path_id}.push_back(${instance.loc});
  %endfor
    locs_str.insert({"${location.path}", locs_${strip(location.name)}_${location.path_id}});
    locs_int.insert({${location.path_id}, locs_${strip(location.name)}_${location.path_id}});
%endfor
%for name, attrs in topo.attrs.items():
    std::unordered_map<std::string, uint32_t> attrs_${name};
  %for attr in attrs:
    %if attr.value.isnumeric():
      attrs_${name}["${attr.name}"] = ${attr.value};
    %endif
  %endfor
    attrs.insert({"${name.upper()}", attrs_${name}});
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

    std::vector<loc_t> get(const std::string& hierarchy) {
      if (not wrap().locs_str.count(hierarchy))
        return {};

      return wrap().locs_str.at(hierarchy);
    }

    loc_t get(const std::string& hierarchy, unsigned id) {
      if (not wrap().locs_str.count(hierarchy))
        return null;
      if (size_t(id) >= wrap().locs_str.at(hierarchy).size())
        return null;

      return wrap().locs_str.at(hierarchy).at(id);
    }

    loc_t get(uint32_t hierarchy, unsigned id) {
      if (not wrap().locs_int.count(hierarchy))
        return null;
      if (size_t(id) >= wrap().locs_int.at(hierarchy).size())
        return null;

      return wrap().locs_int.at(hierarchy).at(id);
    }

    std::pair<bool, uint32_t> attr(const std::string& hierarchy, const std::string& attribute) {
      if (not wrap().attrs.count(hierarchy))
        return std::make_pair(false, uint32_t(0));
      if (not wrap().attrs[hierarchy].count(attribute))
        return std::make_pair(false, uint32_t(0));

      return std::make_pair(true, wrap().attrs.at(hierarchy).at(attribute));
    }
  }
}

extern "C" {

    uint32_t cvm_topology_get_location(uint32_t hierarchy, uint32_t id) {
        return cvm::topology::get(hierarchy, id);
    }
}
