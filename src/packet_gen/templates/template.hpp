#pragma once

#include <cinttypes>
#include <bitset>
#include <array>
#include "cvm/bitmanip.hpp"

namespace ${packets.name} {

    typedef enum {
<% i = 0 %>\
%for packet in packets.packets:
%for subpacket in packet:
        ${subpacket.to_c_enum()} = ${i},
<% i += 1 %>\
%endfor
%endfor
    } message_number;

<%
  namespaces = dict()
%>

%for packet in packets.packets:
%for subpacket in packet:
%if subpacket.name not in namespaces.setdefault(subpacket.port, list()):
    // we can't template namespaces
    namespace ${subpacket.port} {
        template <int N = 0>
        struct ${subpacket.name}
        {
            ${subpacket.name}(...) = delete;
        };
    };
<% namespaces[subpacket.port].append(subpacket.name) %>
%endif
    namespace ${subpacket.port} {
        template <>
        struct ${subpacket.name} <${subpacket.subidx}> {
        % for i,field in enumerate(subpacket.fields):
            ${field.get_c_type()} ${field.name};
        %endfor
            constexpr ${subpacket.name}(
                % for i,field in enumerate(subpacket.fields):
                const ${field.get_c_type()}& ${field.name}${[",", ""][(i+1)//len(subpacket.fields)]}
                %endfor
            ) :
                % for i,field in enumerate(subpacket.fields):
                ${field.name}(${field.name})${[",", ""][(i+1)//len(subpacket.fields)]}
                %endfor
                {}
            static constexpr std::size_t packed_bits  = ${packets.enum_width() + sum(field.widths[0] for field in subpacket.fields)};
            static constexpr std::size_t packed_bytes = (packed_bits + 7)/8;

            constexpr ${subpacket.name}(const std::array<std::uint8_t, packed_bytes>& bytes) :
<% start = packets.enum_width() %>\
            % for i,field in enumerate(subpacket.fields):
                ${field.name}(cvm::bitmanip::array_slice<decltype(${field.name}), ${field.widths[0] + start - 1}, ${start}>(bytes.data()))${[",", ""][(i+1)//len(subpacket.fields)]}
<% start += field.widths[0] %>\
            %endfor
                {}
        };
    }
%endfor
%endfor

}

% if any(subpacket.context for packet in packets.packets for subpacket in packet):
extern "C" void ${packets.name}_finish();
% endif
