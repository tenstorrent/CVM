package cvm_topology_gen;

<%
  reversed_locations = topo.locations[::-1]
%>

%for location in reversed_locations:
  typedef struct packed {
    int unsigned ID;
    int unsigned SHARD;
    int unsigned TOTAL;
  %for child in location.children:
    <%
      child_loc = topo.location(child)
    %>
    %if child_loc.is_array:
    ${child}_${child_loc.path_id}_t [${child_loc.shard - 1}:0] ${child.upper()};
    %else:
    ${child}_${child_loc.path_id}_t ${child.upper()};
    %endif
  %endfor

  <%
    # Since its an array, pick the first node's attributes as the struct's attributes.
    struct_attrs = []
    if location.attributes:
      struct_attrs = location.attributes[0] if location.is_array else location.attributes
  %>
  %for (name, value) in struct_attrs:
    %if type(value) is int:
    int unsigned ${name.upper()};
    %elif type(value) is list:
    bit [${len(value) - 1}:0][31:0] ${name.upper()};
    %endif
  %endfor
  } ${location.name}_${location.path_id}_t;

%endfor

  typedef struct packed {
    top_${topo.location("top").path_id}_t TOP;
  } topology_t;

  localparam topology_t mods = '{
<%
  def build_struct(path_id, shard, total, children_str="", attrs_str=""):
    return "'{" + f"{path_id}, {shard}, {total}{children_str}{attrs_str}" + "}"

  def format_attr_value(value):
    if type(value) is int:
      return f", {value}"
    elif type(value) is list:
      items = reversed([f"'d{v}" for v in value])
      return ", '{" + ','.join(items) + "}"
    return ""

  def recurse(next):
    # `next.shard` is the number of array slots (groups).
    # `next.shards[g]` is group g's physical-instance count.
    # `total` (the per-slot TOTAL attr) is the sum across groups.
    if next.is_array and len(next.attributes) == next.shard:
      structs = []
      total = sum(next.shards)
      cumul = 0
      for idx in range(next.shard):
        inst_attrs = next.attributes[idx] if idx < len(next.attributes) else []
        slot_path_id = next.path_id + cumul
        slot_shard = next.shards[idx]

        children_str = ''.join(f", {recurse(topo.location(child))}" for child in next.children)
        attrs_str = ''.join(format_attr_value(value) for _, value in inst_attrs)

        structs.append(build_struct(slot_path_id, slot_shard, total, children_str, attrs_str))
        cumul += slot_shard

      return "'{" + ', '.join(reversed(structs)) + "}"
    else:
      attrs_to_use = next.attributes if next.attributes else []

      children_str = ''.join(f", {recurse(topo.location(child))}" for child in next.children)
      attrs_str = ''.join(format_attr_value(value) for _, value in attrs_to_use)

      return build_struct(next.path_id, next.shard, len(next.instances), children_str, attrs_str)
%>
    TOP:${recurse(topo.location("top"))}
  };

  function int unsigned get_location (int unsigned path_id, int unsigned num);

%for location in reversed_locations:
    if ((${location.path_id} == path_id) && (num < ${len(location.instances)})) begin
      return path_id + num;
    end
  %if location.is_array and location.shard > 1:
    <%
      cumul_offsets = []
      _c = 0
      for s in location.shards:
        cumul_offsets.append(_c)
        _c += s
    %>
    %for idx in range(1, location.shard):
    if ((${location.path_id + cumul_offsets[idx]} == path_id) && (num < ${location.shards[idx]})) begin
      return path_id + num;
    end
    %endfor
  %endif
%endfor
    else begin
      return cvm_topology::nil;
    end
  endfunction

endpackage
