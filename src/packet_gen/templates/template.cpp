#include "${hpp.removeprefix(incdir).lstrip('/')}"
#include "cvm/registry.hpp"
#include <type_traits>

extern "C" void ${packets.name}_message(const std::uint8_t* message) {

    ${packets.name}::message_number message_number = ${packets.name}::message_number(cvm::bitmanip::array_slice<std::underlying_type<${packets.name}::message_number>::type>(message, ${packets.enum_width()-1}, 0));
    std::uint${type(packets).location_width()}_t loc(cvm::bitmanip::array_slice<std::uint${type(packets).location_width()}_t>(message, ${type(packets).location_width() + packets.enum_width() -1}, ${packets.enum_width()}));

    switch(message_number) {
    %for packet in packets.packets:
    %for subpacket in packet:
        case ${packets.name}::${subpacket.to_c_enum()}: {
            const int b = ${packets.name}::${subpacket.port}::${subpacket.name}<${subpacket.subidx}>::packed_bytes;
            std::array<std::uint8_t, b> m;
            std::copy_n(message, b, std::begin(m));
            cvm::registry::messenger.signal_emplace<${packets.name}::${subpacket.port}::${subpacket.name}<${subpacket.subidx}>>(loc, std::move(m));
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
