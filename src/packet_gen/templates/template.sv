<%
    import pathlib
    from collections import OrderedDict
    include_guard = '_' + str(pathlib.Path(sv).name).upper().replace('.', '_') + '_'

    by_domain = OrderedDict()
    for packet in packets.packets:
        if packet.domain is not None:
            if packet.domain not in by_domain:
                by_domain[packet.domain] = []
            by_domain[packet.domain].append(packet)
    bs = "\\"
%>\
`ifndef ${include_guard}
`define ${include_guard}

package ${packets.name};

    typedef enum logic[${packets.enum_width()}-1:0] {
    %for i,packet in enumerate(packets.packets):
        ${packet.to_sv_enum()} = ${i}${[",",""][(i+1)//len(packets.packets)]}
    %endfor
    } message_number;
%for packet in packets.packets:

    typedef struct packed {
    %for field in reversed(packet.fields):
        logic[${field.width-1}:0] ${field.name};
    %endfor
    } ${packet.port}_${packet.name};

    typedef struct packed {
        ${packet.port}_${packet.name} data;
        logic valid;
    } ${packet.port}_${packet.name}_with_valid;
%endfor

%for domain,domain_packets in by_domain.items():
    typedef struct packed {
    %for packet in domain_packets:
        ${packet.port}_${packet.name}_with_valid[${packets.ports[packet.port]}-1:0][${packet.num}-1:0] ${packet.port}_${packet.name}s;
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

    localparam int NUM_PORTS_${packet.port}_${packet.name} = $size(tx.${packet.port}_${packet.name}s);

    localparam int DATA_${packet.port}_${packet.name}_BITS = $bits(${packets.name}::${packet.port}_${packet.name});
    localparam int MESSAGE_${packet.port}_${packet.name}_BYTES = (DATA_${packet.port}_${packet.name}_BITS + HEADER_BITS + 7) / 8;

    typedef byte unsigned ${packet.port}_${packet.name}_message_t[MESSAGE_${packet.port}_${packet.name}_BYTES];

    import "DPI-C" ${"context" if packet.context else ""} function void ${packets.name}_message_${packet.port}_${packet.name}(${packet.port}_${packet.name}_message_t message);

    function automatic ${packet.port}_${packet.name}_message_t ${packet.port}_${packet.name}_unpack(${packets.name}::${packet.port}_${packet.name} packet);
        localparam int B = MESSAGE_${packet.port}_${packet.name}_BYTES;
        localparam ${packets.name}::message_number N = ${packets.name}::${packet.to_sv_enum()};

        ${packet.port}_${packet.name}_message_t message;

        automatic logic[B*8 - 1:0] short = (8*B)'({packet, N});

        for (int b = 0; b < B; b++) begin
            message[b] = short[8*b +: 8];
        end

        return message;

    endfunction
%endfor

    always @(posedge clk) begin
%for packet in domain_packets:
    %for port in range(packets.ports[packet.port]):
        %for i in range(packet.num):
        if (tx.${packet.port}_${packet.name}s[${port}][${i}].valid) begin
            automatic ${packet.port}_${packet.name}_message_t pkt = ${packet.port}_${packet.name}_unpack(tx.${packet.port}_${packet.name}s[${port}][${i}].data);
            ${packets.name}_message_${packet.port}_${packet.name}(pkt);
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
      for packet in domain_packets:
          if packet.port == port:
              by_port[port] = by_port.get(port, list()) + [packet]
%>
%for port, port_packets in by_port.items():
`define ${packets.name.upper()}_OUTPUT_${port.upper()}                     ${bs}
    %for idx, packet in enumerate(port_packets):
    output ${packets.name}::${port}_${packet.name}_with_valid[${packet.num}-1:0] ${packet.name}s${", \\" if idx != len(port_packets) - 1 else ""}
    %endfor

`define ${packets.name.upper()}_SOURCE_${port.upper()}(domain, port_num)   ${bs}
    %for idx, packet in enumerate(port_packets):
    .${packet.name}s(tx_dom_``domain.${port}_${packet.name}s[port_num])${", \\" if idx != len(port_packets) - 1 else ""}
    %endfor

%endfor
`endif
