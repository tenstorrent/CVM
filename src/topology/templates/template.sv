package topology_pkg;

%for location in topo.locations:

  typedef struct packed {
    int unsigned id;
    int unsigned count;
  %for attr in location.attrs:
    %if attr.value.isnumeric():
    int unsigned ${attr.name};
    %endif
  %endfor
  } ${location.name}_t;
%endfor

  typedef struct packed {
%for location in topo.locations:
    ${location.name}_t ${location.name.upper()};
%endfor
  } topology_t;

  localparam topology_t mods = '{
%for idx, location in enumerate(topo.locations):
    ${location.name.upper()}: '{${idx}, ${len(location.instances)}\
  %for attr in location.attrs:
    %if attr.value.isnumeric():
, ${attr.value}\
    %endif
  %endfor
}\
    %if idx != len(topo.locations) - 1:
,
    %endif
%endfor

  };

endpackage
