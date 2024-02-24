#include "${hpp.removeprefix(incdir).lstrip('/')}"
#include "cvm/registry.hpp"
#include <type_traits>
#include <chrono>

template<std::size_t N, typename W>
class array_wrapper {

    private:
        const W* temp_hold = nullptr;

    public:
        std::chrono::time_point<std::chrono::high_resolution_clock> birth;
        mutable std::chrono::time_point<std::chrono::high_resolution_clock> signal_enqueued_time;

        void set_signal_enqueued_time() const {
            signal_enqueued_time = std::chrono::high_resolution_clock::now();
        }

        array_wrapper(const W* m, const std::chrono::time_point<std::chrono::high_resolution_clock>& birth) : temp_hold(m), birth(birth) {}
        static constexpr int X = (sizeof(birth) + sizeof(W) - 1)/sizeof(W);
        operator std::array<W,N+2*X>() const {
            std::array<W,N+2*X> a;
            std::copy_n(temp_hold, N, a.begin());
            std::copy_n((W*)(&birth), X, a.begin() + N);
            std::copy_n((W*)(&signal_enqueued_time), X, a.begin() + N + X);
            assert(birth < signal_enqueued_time && "signal_enqueued before birth");

            std::chrono::time_point<std::chrono::high_resolution_clock> birth2;
            std::chrono::time_point<std::chrono::high_resolution_clock> signal_enqueued_time2;
            static constexpr int M = N + 2*X;
            std::copy_n(a.begin() + M - 2*X, X, (W*)(&birth2) );
            std::copy_n(a.begin() + M - 1*X, X, (W*)(&signal_enqueued_time2) );
            assert(birth2 < signal_enqueued_time2 && "signal_enqueued before birth");
            return a;
        }
};


static void ${packets.name}_message(const ${type(packets).transfer_word_c_type()}* message, ${packets.name}::message_number message_number, std::size_t words, const std::chrono::time_point<std::chrono::high_resolution_clock>& now) {

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
                    using enum cvm::messenger::priority;
                    cvm::messenger::priority prio = ${packets.domains.get(subpacket.domain, {}).get("priority", "lowest_priority")};
                    if (${str(any("sysmod" in str(s) for s in  [packets.name, subpacket.port, subpacket.name, subpacket.subidx])).lower()}) {
                        prio = lowest_priority;
                    }
                    cvm::registry::messenger.signal<${packets.name}::${subpacket.port}::${subpacket.name}<${subpacket.subidx}>, std::array<${type(packets).transfer_word_c_type()}, ${words} + 2*((sizeof(std::chrono::time_point<std::chrono::high_resolution_clock>) + sizeof(${type(packets).transfer_word_c_type()}) - 1)/sizeof(${type(packets).transfer_word_c_type()}))>>(loc, array_wrapper<${words}, ${type(packets).transfer_word_c_type()}>(message, now), prio, cvm::messenger::launch::async);
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
    auto now = std::chrono::high_resolution_clock::now();
    ${packets.name}_message(message, ${packets.name}::message_number::${subpacket.to_c_enum()}, ${words}, now);
}
%endfor
%endfor
%endfor
