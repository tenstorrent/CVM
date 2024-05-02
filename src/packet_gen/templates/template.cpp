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
    %for packet_variant in packet:
        case ${packets.name}::${packet_variant.to_c_enum()}: {
<%
    location_lsb = packets.enum_width()
    for field in packet_variant.fields:
        if field.name == "location":
            break
        location_lsb += field.width
%>\
            const std::uint${type(packets).location_width()}_t loc(cvm::bitmanip::array_slice<std::uint${type(packets).location_width()}_t>(message, ${type(packets).location_width() + location_lsb - 1}, ${location_lsb}));
            switch (words) {
    % for words in packet_variant.valid_groups_words(packets.enum_width()):
                case ${words}: {
                    cvm::registry::messenger.signal_async<${packets.name}::${packet_variant.port}::${packet_variant.name}<${packet_variant.variant_id}>, std::array<${type(packets).transfer_word_c_type()}, ${words}>>(loc, array_wrapper<${words}, ${type(packets).transfer_word_c_type()}>(message), cvm::messenger::priority::${packet_variant.priority or packets.domains.get(packet_variant.domain, {}).get("priority", "lowest_priority")});
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
%for packet_variant in packet:
% for words in packet_variant.valid_groups_words(packets.enum_width()):
extern "C" void ${packets.name}_message_${packet_variant.port}_${packet_variant.name}_${packet_variant.variant_id}_words${words}(const ${type(packets).transfer_word_c_type()}* message) {
    ${packets.name}_message(message, ${packets.name}::message_number::${packet_variant.to_c_enum()}, ${words});
}
%endfor
%endfor
%endfor
