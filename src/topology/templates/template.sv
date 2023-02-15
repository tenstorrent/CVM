package topology_pkg;

<% caps = [x.name.upper() for x in topo.locations]
%>
typedef enum {
%for idx, cap in enumerate(caps):
  %if idx == (len(caps) - 1):
  ${cap}
  %else:
  ${cap},
  %endif
%endfor
} loc;

function longint unsigned to_loc(loc mod, int unsigned id);
  case(mod)
%for idx, cap in enumerate(caps):
<% instances = topo.locations[idx].instances
%>
    ${cap}: begin
      case(id)
  %for instance in instances:
        32'd${instance.real_id}: begin
          return ${instance.loc};
        end
  %endfor
        default: return 0;
      endcase
    end
%endfor
    default: return 0;
  endcase
endfunction

endpackage;
