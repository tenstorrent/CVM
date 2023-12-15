<%
    import pathlib
    from collections import OrderedDict
    include_guard = '_' + str(pathlib.Path(sv).name).upper().replace('.', '_') + '_'

    by_domain = OrderedDict()
    for packet in packets.packets:
        for subpacket in packet:
            if subpacket.domain is not None:
                if subpacket.domain not in by_domain:
                    by_domain[subpacket.domain] = []
                by_domain[subpacket.domain].append(subpacket)
    bs = "\\"
%>\
`ifndef ${include_guard}
`define ${include_guard}

package ${packets.name};

    typedef enum logic[${packets.enum_width()}-1:0] {
    <% i = 0 %>\
    %for packet in packets.packets:
    %for subpacket in packet:
        ${subpacket.to_sv_enum()} = ${i}${[",",""][(i+1)//packets.total_packets()]}
    <% i += 1 %>\
    %endfor
    %endfor
    } message_number;
%for packet in packets.packets:
%for subpacket in packet:

    typedef struct packed {
    %for field in reversed(subpacket.fields):
        logic[${field.widths[0]-1}:0] ${field.name};
    %endfor
    } ${subpacket.port}_${subpacket.name}_${subpacket.subidx};

    typedef struct packed {
        ${subpacket.port}_${subpacket.name}_${subpacket.subidx} data;
        logic valid;
    } ${subpacket.port}_${subpacket.name}_${subpacket.subidx}_with_valid;
%endfor
%endfor

%for domain,domain_packets in by_domain.items():
    typedef struct packed {
    %for packet in domain_packets:
        ${packet.port}_${packet.name}_${packet.subidx}_with_valid[${packets.ports[packet.port][packet.subidx]}-1:0][${packet.num}-1:0] ${packet.port}_${packet.name}_${packet.subidx}s;
    %endfor
    } domain_${domain};
%endfor

endpackage

module ${packets.name}_write_message #(
    type T = logic,
    type E =   int,
    E    N =    '0,

    localparam int  M = $bits(E) + $bits(T),
    localparam int  B = (M+7)/8
) (
    input  clk,
    input  [$bits(T)-1:0] i,
    output byte unsigned message [B]
);

    always_comb begin
        automatic logic[B*8 - 1:0] short = (8*B)'({i, N});

        for (int b = 0; b < B; b++) begin
            message[b] = short[8*b +: 8];
        end
    end

endmodule

% for domain,domain_packets in by_domain.items():
module ${packets.name}_domain_${domain}(
    input clk,
    input ${packets.name}::domain_${domain} tx
);

    % if any(packet.context for packet in domain_packets):
    // TODO remove
    function void ${packets.name}_finish();
        $finish;
    endfunction
    export "DPI-C" function ${packets.name}_finish;
    % endif

    localparam int HEADER_BITS = $bits(${packets.name}::message_number);

%for packet in domain_packets:

    localparam int NUM_PORTS_${packet.port}_${packet.name}_${packet.subidx} = $size(tx.${packet.port}_${packet.name}_${packet.subidx}s);

    localparam int DATA_${packet.port}_${packet.name}_${packet.subidx}_BITS = $bits(${packets.name}::${packet.port}_${packet.name}_${packet.subidx});
    localparam int MESSAGE_${packet.port}_${packet.name}_${packet.subidx}_BYTES = (DATA_${packet.port}_${packet.name}_${packet.subidx}_BITS + HEADER_BITS + 7) / 8;

    typedef byte unsigned ${packet.port}_${packet.name}_${packet.subidx}_message_t[MESSAGE_${packet.port}_${packet.name}_${packet.subidx}_BYTES];

    import "DPI-C" ${"context" if packet.context else ""} function void ${packets.name}_message_${packet.port}_${packet.name}_${packet.subidx}(${packet.port}_${packet.name}_${packet.subidx}_message_t message);

    function automatic ${packet.port}_${packet.name}_${packet.subidx}_message_t ${packet.port}_${packet.name}_${packet.subidx}_unpack(${packets.name}::${packet.port}_${packet.name}_${packet.subidx} packet);
        localparam int B = MESSAGE_${packet.port}_${packet.name}_${packet.subidx}_BYTES;
        localparam ${packets.name}::message_number N = ${packets.name}::${packet.to_sv_enum()};

        ${packet.port}_${packet.name}_${packet.subidx}_message_t message;

        automatic logic[B*8 - 1:0] short = (8*B)'({packet, N});

        for (int b = 0; b < B; b++) begin
            message[b] = short[8*b +: 8];
        end

        return message;

    endfunction
%endfor

    ${packets.domains.get(domain, {}).get('always_block_header', '')}
    always @(posedge clk) begin
%for packet in domain_packets:
    %for port in range(packets.ports[packet.port][packet.subidx]):
        %for i in range(packet.num):
        if (tx.${packet.port}_${packet.name}_${packet.subidx}s[${port}][${i}].valid) begin
            automatic ${packet.port}_${packet.name}_${packet.subidx}_message_t pkt = ${packet.port}_${packet.name}_${packet.subidx}_unpack(tx.${packet.port}_${packet.name}_${packet.subidx}s[${port}][${i}].data);
            ${packets.name}_message_${packet.port}_${packet.name}_${packet.subidx}(pkt);
        end
        %endfor
    %endfor
%endfor
    end

endmodule
%endfor

`define ${packets.name.upper()}_DOMAIN(domain, clock) ${bs}
    ${packets.name}::domain_``domain tx_dom_``domain; ${bs}
    ${packets.name}_domain_``domain                   ${bs}
        ${packets.name}_domain_``domain (             ${bs}
            .clk(clock),                              ${bs}
            .tx(tx_dom_``domain),                     ${bs}
            .*                                        ${bs}
        );

<%
  by_port = {}
  for port in packets.ports:
      for domain_packets in by_domain.values():
          for packet in domain_packets:
              if packet.port == port:
                  by_port[port] = by_port.get(port, list()) + [packet]
%>
%for port, port_subpackets in by_port.items():

<%
port_declpackets = set([(subpacket.name, subpacket.num) for subpacket in port_subpackets])
end = len(port_declpackets) - 1
%>

`define ${packets.name.upper()}_${port.upper()}_OUTPUT_PARAMS                     ${bs}
    %for i,(declpacket, _) in enumerate(port_declpackets):
    type ${declpacket.upper()}_TYPE = int${", \\" if i != end else ""}
    %endfor

`define ${packets.name.upper()}_${port.upper()}_OUTPUT_PORTS                     ${bs}
    %for i,(declpacket, _) in enumerate(port_declpackets):
    output ${declpacket.upper()}_TYPE ${declpacket}s${", \\" if i != end else ""}
    %endfor

`define ${packets.name.upper()}_${port.upper()}_SOURCE_PARAMS(sub)   ${bs}
    %for i,(declpacket, num) in enumerate(port_declpackets):
    .${declpacket.upper()}_TYPE(${packets.name}::${port}_${declpacket}_``sub``_with_valid[${num}-1:0])${", \\" if i != end else ""}
    %endfor

`define ${packets.name.upper()}_${port.upper()}_SOURCE_PORTS(domain, port_num, sub)   ${bs}
    %for i,(declpacket, _) in enumerate(port_declpackets):
    .${declpacket}s(tx_dom_``domain.${port}_${declpacket}_``sub``s[port_num])${", \\" if i != end else ""}
    %endfor

%endfor
`endif
