#include "${hpp.removeprefix(incdir).lstrip('/')}"
#include "cvm/registry.hpp"
#include <type_traits>

template<std::size_t N, typename W>
class array_wrapper {

    private:
        const W* temp_hold = nullptr;

    public:
        array_wrapper(const W* m) : temp_hold(m) {}
        operator std::array<W,N>() const {
            std::array<W, N> a;
            std::copy_n(temp_hold, N, a.begin());
            return a;
        }
};


static void ${packets.name}_message(const ${type(packets).transfer_word_c_type()}* message, ${packets.name}::message_number message_number, std::size_t words) {

    // ${packets.name}::message_number message_number = ${packets.name}::message_number(cvm::bitmanip::array_slice<std::underlying_type<${packets.name}::message_number>::type>(message, ${packets.enum_width()-1}, 0));

    switch(message_number) {
    %for packet in packets.packets:
    %for subpacket in packet:
        case ${packets.name}::${subpacket.to_c_enum()}: {
<%
    location_lsb = packets.enum_width()
    for field in subpacket.fields:
        if field.name == "location":
            break
        location_lsb += field.width
%>\
            const std::uint${type(packets).location_width()}_t loc(cvm::bitmanip::array_slice<std::uint${type(packets).location_width()}_t>(message, ${type(packets).location_width() + location_lsb - 1}, ${location_lsb}));
            switch (words) {
    % for words in subpacket.valid_groups_words(packets.enum_width()):
                case ${words}: {
                    cvm::registry::messenger.signal<${packets.name}::${subpacket.port}::${subpacket.name}<${subpacket.subidx}>, std::array<${type(packets).transfer_word_c_type()}, ${words}>>(loc, array_wrapper<${words}, ${type(packets).transfer_word_c_type()}>(message));
                    break;
                }
    %endfor
                default: {
                     assert(0 && "unexpected number of words");
                     break;
                 }
            }
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
% for words in subpacket.valid_groups_words(packets.enum_width()):
extern "C" void ${packets.name}_message_${subpacket.port}_${subpacket.name}_${subpacket.subidx}_words${words}(const ${type(packets).transfer_word_c_type()}* message) {
    ${packets.name}_message(message, ${packets.name}::message_number::${subpacket.to_c_enum()}, ${words});
}
%endfor
%endfor
%endfor
