#pragma once

#include <cinttypes>
#include <bitset>
#include "cvm/bitmanip.hpp"

namespace ${packets.name} {

    typedef enum {
%for i,packet in enumerate(packets.packets):
        ${packet.to_c_enum()} = ${i},
%endfor
    } message_number;

    // TODO: namespace by ports
%for packet in packets.packets:
    struct ${packet.name} {
    % for i,field in enumerate(packet.fields):
        ${field.get_c_type()} ${field.name};
    %endfor
        constexpr ${packet.name}(
            % for i,field in enumerate(packet.fields):
            const ${field.get_c_type()}& ${field.name}${[",", ""][(i+1)//len(packet.fields)]}
            %endfor
        ) :
            % for i,field in enumerate(packet.fields):
            ${field.name}(${field.name})${[",", ""][(i+1)//len(packet.fields)]}
            %endfor
            {}
        constexpr ${packet.name}(const std::uint8_t* bytes, const size_t offset) :
<% start = 0 %>\
        % for i,field in enumerate(packet.fields):
            ${field.name}(cvm::bitmanip::array_slice<decltype(${field.name})>(bytes, ${field.width + start - 1} + offset, ${start} + offset))${[",", ""][(i+1)//len(packet.fields)]}
<% start += field.width %>\
        %endfor
            {}
    };
%endfor

}

% if any(packet.context for packet in packets.packets):
extern "C" void ${packets.name}_finish();
% endif
