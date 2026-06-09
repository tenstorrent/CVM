// SPDX-FileCopyrightText: © 2026 Tenstorrent USA, Inc.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cinttypes>
#include <bitset>
#include "cvm/bitmanip.hpp"
#include <array>

namespace ${packet_store.name} {

    typedef enum {
<% i = 0 %>\
%for packet in packet_store.packets:
%for packet_variant in packet:
        ${packet_variant.to_c_enum()} = ${i},
<% i += 1 %>\
%endfor
%endfor
    } message_number;

<%
  namespaces = dict()
%>

%for packet in packet_store.packets:
%for packet_variant in packet:
%if packet_variant.name not in namespaces.setdefault(packet_variant.port, list()):
    namespace ${packet_variant.port} {
        template <int N = 0>
        struct ${packet_variant.name}
        {
            ${packet_variant.name}(...) = delete;
        };
    };
<% namespaces[packet_variant.port].append(packet_variant.name) %>
%endif
    namespace ${packet_variant.port} {
        template <>
        struct ${packet_variant.name} <${packet_variant.variant_id}> {
        % for i,field in enumerate(packet_variant.valid_fields()):
            ${field.get_c_type()} ${field.name};
        % endfor
            ${packet_variant.name}() = default;
            constexpr ${packet_variant.name}(
                % for i,field in enumerate(packet_variant.valid_fields()):
                const ${field.get_c_type()}& ${field.name}${[",", ""][(i+1)//len(list(packet_variant.valid_fields()))]}
                % endfor
            ) :
                % for i,field in enumerate(packet_variant.valid_fields()):
                ${field.name}(${field.name})${[",", ""][(i+1)//len(list(packet_variant.valid_fields()))]}
                % endfor
                {}
            template <std::size_t N>
            constexpr ${packet_variant.name}(const std::array<${type(packet_store).transfer_word_c_type()}, N>& bytes) :
<%
    start = 0
    valid_groups = packet_variant.valid_groups()
    valid_groups_values = list(valid_groups.values()) + [[1<<31, 1<<31]]
    valid_index = 0
%>\
            % for i,field in enumerate(packet_variant.valid_fields()):
                <%
                if start > valid_groups_values[valid_index][1]:
                    valid_index += 1
                valids_offset = "+".join(f"(!cvm::bitmanip::index<{i}>(_packet_gen_valid) ? ({valid[1]} - {valid[0]} + 1) : 0)" for i,valid in enumerate(valid_groups_values[:valid_index]))
                qualify = ""
                if start >= valid_groups_values[valid_index][0] and start <= valid_groups_values[valid_index][1]:
                    qualify = f"!cvm::bitmanip::index<{valid_index}>(_packet_gen_valid) ? 0 : "

                %>\
                ${field.name}(${qualify}cvm::bitmanip::array_slice<decltype(${field.name})${[f",{field.width[-1]}", ""][len(field.width) == 1]}>(bytes.data(), ${field.total_width() + start - 1} + ${packet_store.enum_width()} - (${valids_offset or 0}), ${start} + ${packet_store.enum_width()} - (${valids_offset or 0})))${[",", ""][(i+1)//len(list(packet_variant.valid_fields()))]}
<% start += field.total_width() %>\
            %endfor
                {}
        };
    }
%endfor
%endfor

}

% if any(packet_variant.context for packet in packet_store.packets for packet_variant in packet):
extern "C" void ${packet_store.name}_finish();
% endif
