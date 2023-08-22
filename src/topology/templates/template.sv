package topology_pkg;

<%
  reversed = topo.locations[::-1]
%>

%for location in reversed:
  typedef struct packed {
    int unsigned ID;
    int unsigned SHARD;
    int unsigned TOTAL;
  %for child in location.children:
    ${child}_${topo.location(child).path_id}_t ${child.upper()};
  %endfor
  %for (name, value) in location.attributes:
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
<%def name="recurse(next)">\
    '{${next.path_id}, ${next.shard}, ${len(next.instances)}\
  %for child in next.children:
, ${recurse(topo.location(child))}\
  %endfor
  %for name, value in next.attributes:
    %if type(value) is int:
, ${value}\
    %elif type(value) is list:
    <% value = ["'d" + str(v) for v in value] %>
, '{${','.join(value)}}\
    %endif
  %endfor
}\
</%def>\
    TOP:${recurse(topo.location("top"))}
  };

endpackage
