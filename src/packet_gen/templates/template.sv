<%
    import pathlib
    from collections import OrderedDict
    include_guard = '_' + str(pathlib.Path(sv).name).upper().replace('.', '_') + '_'

    by_domain = OrderedDict()
    for packet in packets.packets:
        for packet_variant in packet:
            if packet_variant.domain is not None:
                if packet_variant.domain not in by_domain:
                    by_domain[packet_variant.domain] = []
                by_domain[packet_variant.domain].append(packet_variant)
    bs = "\\"
%>\
`ifndef ${include_guard}
`define ${include_guard}

package ${packets.name};

    typedef enum logic[${packets.enum_width()}-1:0] {
    <% i = 0 %>\
    %for packet in packets.packets:
    %for packet_variant in packet:
        ${packet_variant.to_sv_enum()} = ${i}${[",",""][(i+1)//packets.total_packets()]}
    <% i += 1 %>\
    %endfor
    %endfor
    } message_number;
%for packet in packets.packets:
%for packet_variant in packet:

    typedef struct packed {
    %for field in reversed(packet_variant.fields):
        logic[${field.width-1}:0] ${field.name};
    %endfor
    } ${packet_variant.port}_${packet_variant.name}_${packet_variant.variant_id};

    typedef struct packed {
        ${packet_variant.port}_${packet_variant.name}_${packet_variant.variant_id} data;
        logic valid;
    } ${packet_variant.port}_${packet_variant.name}_${packet_variant.variant_id}_with_valid;
%endfor
%endfor

%for domain,domain_packets in by_domain.items():
    typedef struct packed {
    %for packet in domain_packets:
        ${packet.port}_${packet.name}_${packet.variant_id}_with_valid[${packets.ports[packet.port][packet.variant_id]}-1:0][${packet.num}-1:0] ${packet.port}_${packet.name}_${packet.variant_id}s;
    %endfor
    } domain_${domain};
%endfor

endpackage

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

%for packet in domain_packets:
    % for words in packet.valid_groups_words(packets.enum_width()):
    import "DPI-C" ${"context" if packet.context else ""} function void ${packets.name}_message_${packet.port}_${packet.name}_${packet.variant_id}_words${words}(${type(packets).transfer_word_sv_type()} message[${words}]);
    % endfor
%endfor

%for packet in domain_packets:
    %for port in range(packets.ports[packet.port][packet.variant_id]):
        %for i in range(packet.num):
<% prefix = f"{packet.port}_{packet.name}_{packet.variant_id}_{port}_{i}"%>\
    typedef struct packed {
        ${packets.name}::${packet.port}_${packet.name}_${packet.variant_id} data;
        ${packets.name}::message_number header;
    } ${prefix}_pkt_t;
    ${prefix}_pkt_t ${prefix}_pkt;
    localparam int ${prefix}_WW = $clog2((($bits(${prefix}_pkt)+${type(packets).transfer_word_bits()} - 1)/${type(packets).transfer_word_bits()})+1);
    logic[$clog2($bits(${prefix}_pkt.data)+1)-1:0] ${prefix}_b;
    logic[${prefix}_WW-1:0] ${prefix}_words_to_transfer;
            % for words in packet.valid_groups_words(packets.enum_width()):
    ${type(packets).transfer_word_sv_type()} ${prefix}_${words}_unpacked[${words}];
            %endfor
        %endfor
    %endfor
%endfor

    ${packets.domains.get(domain, {}).get('always_block_header', '')}
    /* verilator lint_off BLKSEQ */
    always @(posedge clk) begin
%for packet in domain_packets:
    %for port in range(packets.ports[packet.port][packet.variant_id]):
        %for i in range(packet.num):
        if (tx.${packet.port}_${packet.name}_${packet.variant_id}s[${port}][${i}].valid) begin
<% prefix = f"{packet.port}_{packet.name}_{packet.variant_id}_{port}_{i}"%>\
<% odata = f"tx.{packet.port}_{packet.name}_{packet.variant_id}s[{port}][{i}].data" %>\
            ${prefix}_pkt = '{data: ${odata}, header: ${packets.name}::${packet.to_sv_enum()}};
            ${prefix}_b = '0;
            ${prefix}_words_to_transfer = '0;
<% valid_groups = packet.valid_groups(); packet_size = sum(field.width for field in packet.fields)%>\
            %for index, valid in reversed(list(enumerate(valid_groups))):
<%lsb, msb = valid_groups[valid]%>\
            ${prefix}_pkt.data._packet_gen_valid[${index}] = ${formatted if (formatted := valid.format(data = odata)) != valid else odata + "." + valid};
            if (!${prefix}_pkt.data._packet_gen_valid[${index}]) begin
                ${prefix}_pkt.data = {(${msb}-${lsb}+1)'(0),
                % if msb < packet_size - 1:
                    ${prefix}_pkt.data[$bits(${prefix}_pkt.data)-1:${msb}+1],
                % endif
                    ${prefix}_pkt.data[${lsb}-1:0]
                };
                ${prefix}_b += ${msb}-${lsb}+1;
            end
            %endfor
            ${prefix}_words_to_transfer = ${prefix}_WW'(($bits(${prefix}_pkt)-32'(${prefix}_b)+${type(packets).transfer_word_bits()} - 1)/${type(packets).transfer_word_bits()});
            unique case(${prefix}_words_to_transfer) inside
<% prev = -1 %>\
            % for words in packet.valid_groups_words(packets.enum_width()):
                [${prefix}_WW'(${prev+1}):${prefix}_WW'(${words})]: begin // Need to limit the number of lines with DPI calls otherwise zebu blows up FWC resources. Even if only one of them is guaranteed to be called at a time.
                    //automatic ${type(packets).transfer_word_sv_type()} ${prefix}_unpacked[${words}];
                    for (int i = 0; i < ${words}; i++) begin // zebu can't handle using words_to_transfer as the loop bound, it can't unroll it
                        if (i < ${prefix}_words_to_transfer) begin
                            ${prefix}_${words}_unpacked[i] = ${type(packets).transfer_word_bits()}'(${prefix}_pkt >> (${type(packets).transfer_word_bits()}*i));
                        end
                    end
                    ${packets.name}_message_${packet.port}_${packet.name}_${packet.variant_id}_words${words}(${prefix}_${words}_unpacked);
                end
<% prev = words %>
            % endfor
                default: begin
                    assert(1'b0) else $error("No valid function found for ${packet.port}_${packet.name}_${packet.variant_id} to send %0d words", ${prefix}_words_to_transfer);
                end
            endcase
        end
        %endfor
    %endfor
%endfor
    end
    /* verilator lint_on BLKSEQ */

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
              if packet.port == port and packet.variant_id == 0:
                  by_port[port] = by_port.get(port, list()) + [(packet.name, packet.num)]
%>
%for port, port_packets in by_port.items():

<%
end = len(port_packets) - 1
%>

`define ${packets.name.upper()}_${port.upper()}_OUTPUT_PARAMS                     ${bs}
    %for i,(declpacket, _) in enumerate(port_packets):
    type ${declpacket.upper()}_TYPE = int${", \\" if i != end else ""}
    %endfor

`define ${packets.name.upper()}_${port.upper()}_OUTPUT_PORTS                     ${bs}
    %for i,(declpacket, _) in enumerate(port_packets):
    output ${declpacket.upper()}_TYPE ${declpacket}s${", \\" if i != end else ""}
    %endfor

`define ${packets.name.upper()}_${port.upper()}_SOURCE_PARAMS(sub)   ${bs}
    %for i,(declpacket, num) in enumerate(port_packets):
    .${declpacket.upper()}_TYPE(${packets.name}::${port}_${declpacket}_``sub``_with_valid[${num}-1:0])${", \\" if i != end else ""}
    %endfor

`define ${packets.name.upper()}_${port.upper()}_SOURCE_PORTS(domain, port_num, sub)   ${bs}
    %for i,(declpacket, _) in enumerate(port_packets):
    .${declpacket}s(tx_dom_``domain.${port}_${declpacket}_``sub``s[port_num])${", \\" if i != end else ""}
    %endfor

%endfor
`endif
