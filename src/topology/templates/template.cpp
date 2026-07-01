#include "cvm/topology.hpp"
#include <unordered_map>
#include <vector>
#include <memory>
#include <utility>

struct wrapper {
  wrapper() {
%for typ in topo.types:
    std::vector<cvm::topology::loc_t> locs_${typ};
%endfor
%for location in topo.locations:
    std::vector<cvm::topology::loc_t> locs_${location.name}_${location.path_id};
    %if not location.is_array:
    std::unordered_map<std::string, uint32_t> attrs_${location.name}_${location.path_id};
    std::unordered_map<std::string, std::vector<uint32_t>> list_attrs_${location.name}_${location.path_id};
    attrs_${location.name}_${location.path_id}["SHARD"] = ${location.shard};
    attrs_${location.name}_${location.path_id}["TOTAL"] = ${len(location.instances)};
    <%
      attrs_to_use = location.attributes if not location.is_array else []
    %>
    %for (name, value) in attrs_to_use:
      %if type(value) is int:
    attrs_${location.name}_${location.path_id}["${name.upper()}"] = ${value};
      %elif type(value) is list:
      <% value = [str(v) for v in value] %>
    list_attrs_${location.name}_${location.path_id}["${name.upper()}"] = {${','.join(value)}};
      %endif
    %endfor
    %endif
  <%
    has_attributes = location.attributes is not None and len(location.attributes) > 0
    # Build per-instance (loc-ordered) view: which group each instance belongs
    # to, and the offset of each group's first instance within `instances`.
    if location.is_array:
      group_of_inst = []
      group_starts = []
      _c = 0
      for g, sh in enumerate(location.shards):
        group_starts.append(_c)
        for _ in range(sh):
          group_of_inst.append(g)
        _c += sh
      total_insts = sum(location.shards)
    else:
      group_of_inst = [0] * len(location.instances)
      group_starts = [0]
      total_insts = len(location.instances)
  %>

  %for idx, instance in enumerate(location.instances):
    locs_${location.name}_${location.path_id}.push_back(${instance.loc});
    %for typ in location.types:
    locs_${typ}.push_back(${instance.loc});
    %endfor
    names[${instance.loc}] = "${location.name.upper()}";

    %if has_attributes:
      %if location.is_array:
        <%
          g = group_of_inst[idx]
          group_attrs = location.attributes[g]
          group_shard = location.shards[g]
        %>
        ## Create unique attributes for this instance (reflecting its group)
    std::unordered_map<std::string, uint32_t> attrs_inst_${instance.loc};
    std::unordered_map<std::string, std::vector<uint32_t>> list_attrs_inst_${instance.loc};
    attrs_inst_${instance.loc}["SHARD"] = ${group_shard};
    attrs_inst_${instance.loc}["TOTAL"] = ${total_insts};
        %for (name, value) in group_attrs:
          %if type(value) is int:
    attrs_inst_${instance.loc}["${name.upper()}"] = ${value};
          %elif type(value) is list:
          <% value_str = ','.join(str(v) for v in value) %>
    list_attrs_inst_${instance.loc}["${name.upper()}"] = {${value_str}};
          %endif
        %endfor
    attrs[${instance.loc}] = attrs_inst_${instance.loc};
    list_attrs[${instance.loc}] = list_attrs_inst_${instance.loc};
      %else:
        ## Use shared attributes
    attrs[${instance.loc}] = attrs_${location.name}_${location.path_id};
    list_attrs[${instance.loc}] = list_attrs_${location.name}_${location.path_id};
      %endif
    %endif
  %endfor
    str_hierarchy.insert({"${location.path}", locs_${location.name}_${location.path_id}});
    int_hierarchy.insert({${location.path_id}, locs_${location.name}_${location.path_id}});
    %if location.is_array:
    ## One PATH[g] entry per group, pointing at all the locs in that group.
    %for g, sh in enumerate(location.shards):
    str_hierarchy.insert({"${location.path}[${g}]", {\
      %for k in range(sh):
locs_${location.name}_${location.path_id}[${group_starts[g] + k}]${"," if k < sh - 1 else ""}\
      %endfor
}});
    %endfor
    %endif
%endfor
%for typ in topo.types:
    str_type["${typ.upper()}"] = locs_${typ};
%endfor
  }

  std::unordered_map<std::string, std::vector<cvm::topology::loc_t>> str_hierarchy;
  std::unordered_map<uint32_t,    std::vector<cvm::topology::loc_t>> int_hierarchy;
  std::unordered_map<std::string, std::vector<cvm::topology::loc_t>> str_type;
  std::unordered_map<cvm::topology::loc_t, std::unordered_map<std::string, uint32_t>> attrs;
  std::unordered_map<cvm::topology::loc_t, std::unordered_map<std::string, std::vector<uint32_t>>> list_attrs;
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

    std::pair<bool, std::vector<uint32_t>> list_attr(cvm::topology::loc_t loc, const std::string& attribute) {
      try {
        return std::make_pair(true, wrap().list_attrs.at(loc).at(attribute));
      }
      catch (...) {
        return std::make_pair(false, std::vector<uint32_t>{});
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
