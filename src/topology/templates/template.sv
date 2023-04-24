package topology_pkg;

<%
  reversed = topo.locations[::-1]
%>

%for location in reversed:
  typedef struct packed {
    int unsigned id;
    int unsigned count;
  %for child in location.children:
    ${child.strip('~').upper()}_${topo.location(child).path_id}_t ${child.strip('~').upper()};
  %endfor
  } ${location.stripped_name()}_${location.path_id}_t;

%endfor

%for name, attrs in topo.attrs.items():
  typedef struct packed {
    %for attr in attrs:
      %if attr.value.isnumeric():
    int unsigned ${attr.name};
      %endif
    %endfor
  } ${name}_attrs_t;

%endfor

  typedef struct packed {
    top_${topo.location("top").path_id}_t TOP;
%for name in topo.attrs:
    ${name}_attrs_t ${name.upper()};
%endfor
  } topology_t;

  localparam topology_t mods = '{
<%def name="recurse(next)">
    '{${next.path_id}, ${len(next.instances)}\
  %for child in next.children:
, ${recurse(topo.location(child))}\
  %endfor
}\
</%def>\
    TOP:${recurse(topo.location("top"))},\

%for idx, (name, attrs) in enumerate(topo.attrs.items()):
    ${name.upper()}: '{\
  %for idx2, attr in enumerate(attrs):
    %if attr.value.isnumeric():
${attr.value}\
    %endif
    %if idx2 != len(attrs) - 1:
,\
    %endif
  %endfor
}\
  %if idx != len(topo.attrs) - 1:
,
  %endif
%endfor

  };

endpackage
