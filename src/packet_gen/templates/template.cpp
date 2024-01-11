#include "${hpp.removeprefix(incdir).lstrip('/')}"
#include "cvm/registry.hpp"
#include <type_traits>

extern "C" void ${packets.name}_message(const std::uint8_t* message) {

    ${packets.name}::message_number message_number = ${packets.name}::message_number(cvm::bitmanip::array_slice<std::underlying_type<${packets.name}::message_number>::type>(message, ${packets.enum_width()-1}, 0));

    switch(message_number) {
    %for packet in packets.packets:
    %for subpacket in packet:
        case ${packets.name}::${subpacket.to_c_enum()}: {
            ${packets.name}::${subpacket.port}::${subpacket.name}<${subpacket.subidx}> ${subpacket.name}(message, ${packets.enum_width()});
            cvm::registry::messenger.signal(${subpacket.name}.location, ${subpacket.name});
            break;
        }
    %endfor
    %endfor
        default: {
            assert(0 && "unexpected message number");
            break;
        }
    }
}

%for packet in packets.packets:
%for subpacket in packet:
% for bytes in subpacket.valid_groups_bytes(packets.enum_width()):
extern "C" void ${packets.name}_message_${subpacket.port}_${subpacket.name}_${subpacket.subidx}_bytes${bytes}(const std::uint8_t* message) {
    ${packets.name}_message(message);
}
%endfor
%endfor
%endfor
