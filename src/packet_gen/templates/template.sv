<%
    import pathlib
    from collections import OrderedDict
    include_guard = '_' + str(pathlib.Path(sv).name).upper().replace('.', '_') + '_'

    by_domain = OrderedDict()
    for packet in packet_store.packets:
        for packet_variant in packet:
            for packet_domain in packet_store.packet_domains(packet_variant):
                by_domain.setdefault(packet_domain, []).append(packet_variant)
    bs = "\\"
%>\
`ifndef ${include_guard}
`define ${include_guard}

package ${packet_store.name};

    typedef enum logic[${packet_store.enum_width()}-1:0] {
    <% i = 0 %>\
    %for packet in packet_store.packets:
    %for packet_variant in packet:
        ${packet_variant.to_sv_enum()} = ${i}${[",",""][(i+1)//packet_store.total_packets()]}
    <% i += 1 %>\
    %endfor
    %endfor
    } message_number;
%for packet in packet_store.packets:
%for packet_variant in packet:

    typedef struct packed {
    %for field in reversed(list(packet_variant.valid_fields())):
    ${field.get_sv_type()} ${field.name};
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
        ${packet.port}_${packet.name}_${packet.variant_id}_with_valid[${packet_store.ports[packet.port][packet.variant_id]}-1:0][${packet.num}-1:0] ${packet.port}_${packet.name}_${packet.variant_id}s;
    %endfor
    } domain_${domain};
%endfor

endpackage

% for domain,domain_packets in by_domain.items():
<% init = packet_store.domains.get(domain, {}).get('dpi_init', '') or '' %>\
<% init_function = packet_store.domains.get(domain, {}).get('dpi_init_function', '') or f"{packet_store.name}_dpi_init_domain_{domain}" %>\

module ${packet_store.name}_domain_${domain}(
    input clk,
    % if ( init != '' ):
    input ${init},
    % endif
    input ${packet_store.name}::domain_${domain} tx
);

    % if any(packet.context for packet in domain_packets):
    // TODO remove
    function void ${packet_store.name}_finish();
        $finish;
    endfunction
    export "DPI-C" function ${packet_store.name}_finish;
    % endif

%for packet in domain_packets:
    % for define in packet.disabling_defines:
`ifndef ${define}
    % endfor
    % for words in packet.valid_groups_words(packet_store.enum_width()):
        <% chunks = packet_store.chunk_transfer(words) %>\
        % for i,chunk in enumerate(chunks):
import "DPI-C" ${"context" if packet.context else ""} function void ${packet_store.name}_message_${packet.port}_${packet.name}_${packet.variant_id}_words${words}${[f"_chunk{i}",""][int(len(chunks)==1)]}(${type(packet_store).transfer_word_sv_type()} message[${chunk}]);
        % endfor
    % endfor
    % for _ in packet.disabling_defines:
`endif
    % endfor
%endfor

%for packet in domain_packets:
    %for port in range(packet_store.ports[packet.port][packet.variant_id]):
        % if packet.domain_of(port) == domain:
        %for i in range(packet.num):
<% prefix = f"{packet.port}_{packet.name}_{packet.variant_id}_{port}_{i}"%>\
    typedef struct packed {
        ${packet_store.name}::${packet.port}_${packet.name}_${packet.variant_id} data;
        ${packet_store.name}::message_number header;
    } ${prefix}_pkt_t;
    ${prefix}_pkt_t ${prefix}_pkt;
    localparam int ${prefix}_WW = $clog2((($bits(${prefix}_pkt)+${type(packet_store).transfer_word_bits()} - 1)/${type(packet_store).transfer_word_bits()})+1);
    logic[$clog2($bits(${prefix}_pkt.data)+1)-1:0] ${prefix}_b;
    logic[${prefix}_WW-1:0] ${prefix}_words_to_transfer;
            % for words in packet.valid_groups_words(packet_store.enum_width()):
    ${type(packet_store).transfer_word_sv_type()} ${prefix}_${words}_unpacked[${words}];
            %endfor
        %endfor
        % endif
    %endfor
%endfor
    %if (init != '') :
        import "DPI-C" function void ${init_function}();
    %endif

    ${packet_store.domains.get(domain, {}).get('always_block_header', '')}
    /* verilator lint_off BLKSEQ */
    always @(posedge clk) begin
       %if (init != '') :
       if (${init} == 1'b1) ${init_function}();
       %endif
%for packet in domain_packets:
    % for define in packet.disabling_defines:
`ifndef ${define}
    % endfor
    %for port in range(packet_store.ports[packet.port][packet.variant_id]):
        % if packet.domain_of(port) == domain:
        %for i in range(packet.num):
        if (tx.${packet.port}_${packet.name}_${packet.variant_id}s[${port}][${i}].valid) begin
<% prefix = f"{packet.port}_{packet.name}_{packet.variant_id}_{port}_{i}"%>\
<% odata = f"tx.{packet.port}_{packet.name}_{packet.variant_id}s[{port}][{i}].data" %>\
            ${prefix}_pkt = '{data: ${odata}, header: ${packet_store.name}::${packet.to_sv_enum()}};
            ${prefix}_b = '0;
            ${prefix}_words_to_transfer = '0;
<% valid_groups = packet.valid_groups(); packet_size = sum(field.total_width() for field in packet.valid_fields())%>\
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
                ${prefix}_b += $bits(${prefix}_b)'(${msb}-${lsb}+1);
            end
            %endfor
            ${prefix}_words_to_transfer = ${prefix}_WW'(($bits(${prefix}_pkt)-32'(${prefix}_b)+${type(packet_store).transfer_word_bits()} - 1)/${type(packet_store).transfer_word_bits()});
            unique case(${prefix}_words_to_transfer) inside
<% prev = -1 %>\
            % for words in packet.valid_groups_words(packet_store.enum_width()):
                [${prefix}_WW'(${prev+1}):${prefix}_WW'(${words})]: begin // Need to limit the number of lines with DPI calls otherwise zebu blows up FWC resources. Even if only one of them is guaranteed to be called at a time.
                    //automatic ${type(packet_store).transfer_word_sv_type()} ${prefix}_unpacked[${words}];
<% chunks = packet_store.chunk_transfer(words) %>\
                    % for i,chunk in enumerate(chunks):
                        automatic ${packet_store.transfer_word_sv_type()} temp${i}[${chunk}]; // need temp to workaround verilator internal error
                    %endfor
                    for (int i = 0; i < ${words}; i++) begin // zebu can't handle using words_to_transfer as the loop bound, it can't unroll it
                        if (i < int'(${prefix}_words_to_transfer)) begin
                            ${prefix}_${words}_unpacked[i] = ${type(packet_store).transfer_word_bits()}'(${prefix}_pkt >> (${type(packet_store).transfer_word_bits()}*i));
                        end
                    end
                    % for i,chunk in enumerate(chunks):
                        temp${i} = ${prefix}_${words}_unpacked[${sum(chunks[:i])} : ${sum(chunks[:i]) + chunk - 1}];
                        ${packet_store.name}_message_${packet.port}_${packet.name}_${packet.variant_id}_words${words}${[f"_chunk{i}",""][int(len(chunks)==1)]}(temp${i});
                    % endfor
                end
<% prev = words %>
            % endfor
                default: begin : ${prefix}_no_valid_function
                    ${prefix}_valid_words: assert(1'b0) else $error("No valid function found for ${packet.port}_${packet.name}_${packet.variant_id} to send %0d words", ${prefix}_words_to_transfer);
                end
            endcase
        end
        %endfor
        % endif
    %endfor
    % for _ in packet.disabling_defines:
`endif
    % endfor
%endfor
    end
    /* verilator lint_on BLKSEQ */

endmodule
%endfor

`define ${packet_store.name.upper()}_DOMAIN(domain, clock) ${bs}
    ${packet_store.name}::domain_``domain tx_dom_``domain; ${bs}
    ${packet_store.name}_domain_``domain                   ${bs}
        ${packet_store.name}_domain_``domain (             ${bs}
            .clk(clock),                              ${bs}
            % if ( init != '' ):
            .${init}(${init}),              ${bs}
            % endif
            .tx(tx_dom_``domain),                     ${bs}
            .*                                        ${bs}
        );

<%
  lines = []
  for domain in by_domain:
      init = (packet_store.domains.get(domain) or {}).get('dpi_init', '') or ''
      init_port = f".{init}({init}), " if init else ""
      lines.append(f"    {packet_store.name}::domain_{domain} tx_dom_{domain};")
      lines.append(f"    {packet_store.name}_domain_{domain} {packet_store.name}_domain_{domain} (.clk(clocks[{domain}]), {init_port}.tx(tx_dom_{domain}));")
%>\
`define ${packet_store.name.upper()}_DOMAINS(clocks) ${bs}
${f" {bs}\n".join(lines)}

<%
  by_port = {}
  sent_packets = [packet for packet in packet_store.packets if packet_store.packet_domains(packet[0])]
  for port in packet_store.ports:
      for packet in sent_packets:
          if packet[0].port == port:
              by_port[port] = by_port.get(port, list()) + [(packet[0].name, packet[0].num)]
%>
%for port, port_packets in by_port.items():

<%
end = len(port_packets) - 1
%>

`define ${packet_store.name.upper()}_${port.upper()}_OUTPUT_PARAMS                     ${bs}
    %for i,(declpacket, _) in enumerate(port_packets):
    type ${declpacket.upper()}_TYPE = int${", \\" if i != end else ""}
    %endfor

`define ${packet_store.name.upper()}_${port.upper()}_OUTPUT_PORTS                     ${bs}
    %for i,(declpacket, _) in enumerate(port_packets):
    output ${declpacket.upper()}_TYPE ${declpacket}s${", \\" if i != end else ""}
    %endfor

`define ${packet_store.name.upper()}_${port.upper()}_SOURCE_PARAMS(sub)   ${bs}
    %for i,(declpacket, num) in enumerate(port_packets):
    .${declpacket.upper()}_TYPE(${packet_store.name}::${port}_${declpacket}_``sub``_with_valid[${num}-1:0])${", \\" if i != end else ""}
    %endfor

`define ${packet_store.name.upper()}_${port.upper()}_SOURCE_PORTS(domain, port_num, sub)   ${bs}
    %for i,(declpacket, _) in enumerate(port_packets):
    .${declpacket}s(tx_dom_``domain.${port}_${declpacket}_``sub``s[port_num])${", \\" if i != end else ""}
    %endfor
    %for variant_id, instances in enumerate(packet_store.ports[port]):
        % if instances:
<%
  lines = []
  for packet in sent_packets:
      if packet[0].port != port:
          continue
      packet_variant = packet[variant_id]
      member = f"{port}_{packet_variant.name}_{variant_id}"
      lines.append(f"    {packet_store.name}::{member}_with_valid [{instances}-1:0][{packet_variant.num}-1:0] tx_{member}s;")
      for instance in range(instances):
          lines.append(f"    assign tx_dom_{packet_variant.domain_of(instance)}.{member}s[{instance}] = tx_{member}s[{instance}];")
%>\

`define ${packet_store.name.upper()}_${port.upper()}_${variant_id}_INSTANCE_SOURCES ${bs}
${f" {bs}\n".join(lines)}

`define ${packet_store.name.upper()}_${port.upper()}_${variant_id}_INSTANCE_PORTS(port_num)   ${bs}
            %for i,(declpacket, _) in enumerate(port_packets):
    .${declpacket}s(tx_${port}_${declpacket}_${variant_id}s[port_num])${", \\" if i != end else ""}
            %endfor
        % endif
    %endfor

%endfor

%for packet in packet_store.packets:
%for packet_variant in packet:
%for field in packet_variant.fields:
%if field.total_width() == 0:
`define ${packet_store.name.upper()}_${packet_variant.name.upper()}_${packet_variant.variant_id}_${field.name.upper()}_UNDEF
%endif
%endfor
%endfor
%endfor

`endif
