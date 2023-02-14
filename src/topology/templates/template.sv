package topology;
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

function longint unsigned to_loc(loc mod, int id);
  case(mod)
%for idx, cap in enumerate(caps):
<% instances = topo.locations[idx].instances
%>
    ${cap} : begin
      case(id)
  %for instance in instances:
        ${instance.real_id} : return ${instance.loc}
  %endfor
      endcase
    end
%endfor
  endcase
endfunction
endpackage;
