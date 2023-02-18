package topology_pkg;

<% caps = [x.name.upper() for x in topo.locations]
%>
  typedef struct packed {
%for cap in caps:
    int unsigned ${cap};
%endfor
  } topology_t;

  localparam topology_t mods = '{
%for idx, cap in enumerate(caps):
    ${cap}: ${idx}\
    %if idx != len(caps) - 1:
,
    %endif
%endfor
  };

  localparam longint unsigned nul = 0;

endpackage
