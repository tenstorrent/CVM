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
    } ${packet.name};

    typedef struct packed {
        ${packet.name} data;
        logic valid;
    } ${packet.name}_with_valid;
%endfor

%for domain,domain_packets in by_domain.items():
    typedef struct packed {
    %for packet in domain_packets:
        ${packet.name}_with_valid[${packet.num}-1:0] ${packet.name}s;
    %endfor
    } domain_${domain};
%endfor

endpackage

module ${packets.name}_messenger #(
    type T = logic,
    type E =   int,
    E    N =    '0
) (
    input  clk,
    input  valid,
    input  [$bits(T)-1:0] i
);
    localparam int  M = $bits(E) + $bits(T);
    localparam int  B = (M+7)/8;

%for packet in packets.packets:
    import "DPI-C" context function void ${packets.name}_message_${packet.name}(byte unsigned message[($bits(E) + $bits(${packets.name}::${packet.name}) + 7)/8]);
%endfor

    function void ${packets.name}_finish();
        $finish;
    endfunction
    export "DPI-C" function ${packets.name}_finish;

    typedef byte unsigned message_t[B];
    message_t message;
    always_comb begin
        automatic logic[B*8 - 1:0] short = (8*B)'({i, N});

        for (int b = 0; b < B; b++) begin
            message[b] = short[8*b +: 8];
        end
    end

    case(N)
%for packet in packets.packets:
        ${packets.name}::${packet.to_c_enum()}: always @(posedge clk) if (valid) ${packets.name}_message_${packet.name}(message);
%endfor
        default: $error("unknown %d", N);
    endcase

endmodule

% for domain,domain_packets in by_domain.items():
module transactions_domain_${domain}(
    input clk,
    input transactions::domain_${domain} tx
);

    %for packet in domain_packets:
    for (genvar i = 0; i < $size(tx.${packet.name}s); i++) begin
        ${packets.name}_messenger #(${packets.name}::${packet.name}, ${packets.name}::message_number, ${packets.name}::${packet.to_sv_enum()}) ${packet.name}_messenger (clk, tx.${packet.name}s[i].valid, tx.${packet.name}s[i].data);
    end
    %endfor

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

`endif
