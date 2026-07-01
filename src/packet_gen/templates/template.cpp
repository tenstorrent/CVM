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


static void ${packet_store.name}_message(const ${type(packet_store).transfer_word_c_type()}* message, ${packet_store.name}::message_number message_number, std::size_t words) {

    // ${packet_store.name}::message_number message_number = ${packet_store.name}::message_number(cvm::bitmanip::array_slice<std::underlying_type<${packet_store.name}::message_number>::type>(message, ${packet_store.enum_width()-1}, 0));

    switch(message_number) {
    %for packet in packet_store.packets:
    %for packet_variant in packet:
        case ${packet_store.name}::${packet_variant.to_c_enum()}: {
<%
    location_lsb = packet_store.enum_width()
    for field in packet_variant.fields:
        if field.name == "location":
            break
        location_lsb += field.total_width()
%>\
            const std::uint${type(packet_store).location_width()}_t loc(cvm::bitmanip::array_slice<std::uint${type(packet_store).location_width()}_t>(message, ${type(packet_store).location_width() + location_lsb - 1}, ${location_lsb}));
            switch (words) {
    % for words in packet_variant.valid_groups_words(packet_store.enum_width()):
                case ${words}: {
                    cvm::registry::messenger.signal_async<${packet_store.name}::${packet_variant.port}::${packet_variant.name}<${packet_variant.variant_id}>, std::array<${type(packet_store).transfer_word_c_type()}, ${words}>>(loc, array_wrapper<${words}, ${type(packet_store).transfer_word_c_type()}>(message), cvm::messenger::priority::${packet_variant.priority or packet_store.domains.get(packet_variant.domain, {}).get("priority", "lowest_priority")});
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

%for packet in packet_store.packets:
%for packet_variant in packet:
% for words in packet_variant.valid_groups_words(packet_store.enum_width()):
<%
    chunks = packet_store.chunk_transfer(words)
    chunky = len(chunks) > 1
    suff   = f"{packet_store.name}_message_{packet_variant.port}_{packet_variant.name}_{packet_variant.variant_id}_words{words}"
%>\
% if chunky:
static void(*${suff}_prev_dpi)(const ${type(packet_store).transfer_word_c_type()}*);
% endif
% for i,chunk in enumerate(chunks):
<%
    last = i == len(chunks) - 1
    name = f"{suff}{['', '_chunk' + str(i)][int(chunky)]}"
%>\
% if chunky and not last:
static std::array<${type(packet_store).transfer_word_c_type()}, ${chunk}> ${name}_save;
% endif
extern "C" void ${name}(const ${type(packet_store).transfer_word_c_type()} message[${chunk}]) {
% if not last:
    std::copy(message, message + ${chunk}, std::begin(${name}_save));
% else:
    % if chunky:
    std::array<${type(packet_store).transfer_word_c_type()}, ${words}> m;
        % for j in range(i):
    std::copy(std::begin(${suff}_chunk${j}_save), std::end(${suff}_chunk${j}_save), std::begin(m) + ${sum(chunks[:j])});
        % endfor
    std::copy(message, message + ${chunk}, std::begin(m) + ${sum(chunks[:-1])});
    const auto* msg = m.data();
    % else:
    const auto* msg = message;
    % endif
    ${packet_store.name}_message(msg, ${packet_store.name}::message_number::${packet_variant.to_c_enum()}, ${words});
% endif
% if chunky:
    % if i > 0:
    assert(${suff}_prev_dpi == ${suff}_chunk${i-1});
    % endif
    ${suff}_prev_dpi = ${name};
% endif
}
%endfor
%endfor
%endfor
%endfor
