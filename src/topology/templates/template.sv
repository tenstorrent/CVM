package topology_pkg;

<% caps = [x.name.upper() for x in topo.locations]
%>
  typedef struct packed {
%for cap in caps:
    int unsigned ${cap};
%endfor
  } topology_t;

  localparam topology_t t = '{
%for idx, cap in enumerate(caps):
    ${cap}: ${idx}\
    %if idx != len(caps) - 1:
,
    %endif
%endfor
  };

  localparam longint unsigned nul = 0;

  function longint unsigned to_loc(int unsigned mod, int unsigned id);
    case(mod)
%for idx, cap in enumerate(caps):
<% instances = topo.locations[idx].instances
%>
      ${idx}: begin
        case(id)
  %for instance in instances:
          'd${instance.real_id}: return ${instance.loc};
  %endfor
          default: return 0;
        endcase
      end
%endfor
      default: return 0;
    endcase
  endfunction

endpackage;
