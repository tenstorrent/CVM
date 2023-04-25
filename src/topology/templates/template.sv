package topology_pkg;

<%
  reversed = topo.locations[::-1]
%>

%for location in reversed:
  typedef struct packed {
    int unsigned id;
    int unsigned count;
  %for child in location.children:
    ${child}_${topo.location(child).path_id}_t ${child.upper()};
  %endfor
  %for (name, value) in location.attributes:
    %if value.isnumeric():
    int unsigned ${name};
    %endif
  %endfor
  } ${location.name}_${location.path_id}_t;

%endfor

  typedef struct packed {
    top_${topo.location("top").path_id}_t TOP;
  } topology_t;

  localparam topology_t mods = '{
<%def name="recurse(next)">
    '{${next.path_id}, ${len(next.instances)}\
  %for child in next.children:
, ${recurse(topo.location(child))}\
  %endfor
  %for name, value in next.attributes:
    %if value.isnumeric():
, ${value}
    %endif
  %endfor
}\
</%def>\
    TOP:${recurse(topo.location("top"))}
  };

endpackage
