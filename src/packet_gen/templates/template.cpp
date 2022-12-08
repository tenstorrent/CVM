#include "${hpp.removeprefix(incdir).lstrip('/')}"
#include "cvm/messenger.hpp"
#include <type_traits>

extern "C" void ${packets.name}_message(const std::uint8_t* message) {

    ${packets.name}::message_number message_number = ${packets.name}::message_number(cvm::bitmanip::array_slice<std::underlying_type<${packets.name}::message_number>::type>(message, ${packets.enum_width()-1}, 0));

    switch(message_number) {
    %for packet in packets.packets:
        case ${packets.name}::${packet.to_c_enum()}: {
            ${packets.name}::${packet.name} ${packet.name}(message, ${packets.enum_width()});
            cvm::messenger<${packets.name}::${packet.name}>::signal(${packet.name});
            break;
        }
    %endfor
        default: {
            assert(0 && "unexpected message number");
            break;
        }
    }
}

%for packet in packets.packets:
extern "C" void ${packets.name}_message_${packet.name}(const std::uint8_t* message) {
    ${packets.name}_message(message);
}
%endfor
