#include "${hpp.removeprefix(incdir).lstrip('/')}"
#include "cvm/registry.hpp"
#include <type_traits>

extern "C" void ${packets.name}_message(const std::uint8_t* message) {

    ${packets.name}::message_number message_number = ${packets.name}::message_number(cvm::bitmanip::array_slice<std::underlying_type<${packets.name}::message_number>::type>(message, ${packets.enum_width()-1}, 0));

    switch(message_number) {
    %for packet in packets.packets:
        case ${packets.name}::${packet.to_c_enum()}: {
            ${packets.name}::${packet.port}::${packet.name} ${packet.name}(message, ${packets.enum_width()});
            cvm::registry::messenger.signal<${packets.name}::${packet.port}::${packet.name}>(${packet.name}.location, ${packet.name});
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
extern "C" ${"int" if packet.dummy_return else "void"} ${packets.name}_message_${packet.port}_${packet.name}(const std::uint8_t* message) {
    ${packets.name}_message(message);${" return 0;" if packet.dummy_return else ""}
}
%endfor
