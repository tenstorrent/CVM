#include "${hpp.removeprefix(incdir).lstrip('/')}"
#include "cvm/registry.hpp"
#include <type_traits>
#include <chrono>
#include <iostream>

const int packets = ${sum(1 for packet in packets.packets for subpacket in packet)};
std::array<std::tuple<std::chrono::nanoseconds, std::uint64_t>, packets> durations{};

std::array<const char*, packets> names = {
<% i = 0 %>\
%for packet in packets.packets:
%for subpacket in packet:
   "${packets.name}_message_${subpacket.port}_${subpacket.name}_${subpacket.subidx}",
<% i += 1 %>\
%endfor
%endfor
};

extern "C" void ${packets.name}_message(const std::uint8_t* message) {

    ${packets.name}::message_number message_number = ${packets.name}::message_number(cvm::bitmanip::array_slice<std::underlying_type<${packets.name}::message_number>::type, ${packets.enum_width()-1}, 0>(message));
    std::uint${type(packets).location_width()}_t loc(cvm::bitmanip::array_slice<std::uint${type(packets).location_width()}_t, ${type(packets).location_width() + packets.enum_width() -1}, ${packets.enum_width()}>(message));

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

<%i=0%>
%for packet in packets.packets:
%for subpacket in packet:
extern "C" ${"int" if subpacket.dummy_return else "void"} ${packets.name}_message_${subpacket.port}_${subpacket.name}_${subpacket.subidx}(const std::uint8_t* message) {
    if constexpr (1 /*${int(f"{packets.name}_message_{subpacket.port}_{subpacket.name}_{subpacket.subidx}" not in ["rv_tester_transactions_message_axi_sw_w_0", "rv_tester_transactions_message_axi_sw_aw_0"])}*/) {
    auto start = std::chrono::high_resolution_clock::now();
        ${packets.name}_message(message);
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = end - start;
    if(${int(subpacket.dummy_return and True or False)}) {
        auto& [total, count] = durations[${packets.name}::${subpacket.to_c_enum()}];
        total += duration;
        count++;
    }
    }
    ${" return 0;" if subpacket.dummy_return else ""}
}
%endfor
%endfor

void report_durations() {
    int i = 0;
    for(const auto& [total, count] : durations) {
        if(count) {
            std::cout << names[i] << ": total: " << std::dec << total << " count: " << count << " average: " << (total/count).count() << std::endl;
        }
        i++;
    }
}
