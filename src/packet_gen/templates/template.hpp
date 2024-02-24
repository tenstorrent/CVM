#pragma once

#include <cinttypes>
#include <bitset>
#include "cvm/bitmanip.hpp"
#include <array>
#include <chrono>

namespace ${packets.name} {

    typedef enum {
<% i = 0 %>\
%for packet in packets.packets:
%for subpacket in packet:
        ${subpacket.to_c_enum()} = ${i},
<% i += 1 %>\
%endfor
%endfor
    } message_number;

<%
  namespaces = dict()
%>

%for packet in packets.packets:
%for subpacket in packet:
%if subpacket.name not in namespaces.setdefault(subpacket.port, list()):
    // we can't template namespaces
    namespace ${subpacket.port} {
        template <int N = 0>
        struct ${subpacket.name}
        {
            ${subpacket.name}(...) = delete;
        };
    };
<% namespaces[subpacket.port].append(subpacket.name) %>
%endif
    namespace ${subpacket.port} {
        template <>
        struct ${subpacket.name} <${subpacket.subidx}> {
        % for i,field in enumerate(subpacket.fields):
            ${field.get_c_type()} ${field.name};
        %endfor
            std::chrono::time_point<std::chrono::high_resolution_clock> birth, signal_enqueued_time, prev_func_start_time, prev_func_finish_time, sleep_time, wakeup_time, signal_swap_time, dispatch_time;
            static constexpr int X = (sizeof(birth) + sizeof(${type(packets).transfer_word_c_type()}) - 1)/(sizeof(${type(packets).transfer_word_c_type()}));
            constexpr ${subpacket.name}(
                % for i,field in enumerate(subpacket.fields):
                const ${field.get_c_type()}& ${field.name}${[",", ""][(i+1)//len(subpacket.fields)]}
                %endfor
            ) :
                % for i,field in enumerate(subpacket.fields):
                ${field.name}(${field.name})${[",", ""][(i+1)//len(subpacket.fields)]}
                %endfor
                {}
            template <std::size_t N>
            constexpr ${subpacket.name}(const std::array<${type(packets).transfer_word_c_type()}, N>& words) :
<%
    start = 0
    valid_groups = subpacket.valid_groups()
    valid_groups_values = list(valid_groups.values()) + [[1<<31, 1<<31]]
    valid_index = 0
%>\
            % for i,field in enumerate(subpacket.fields):
                <%
                if start > valid_groups_values[valid_index][1]:
                    valid_index += 1
                valids_offset = "+".join(f"(!cvm::bitmanip::index<{i}>(_packet_gen_valid) ? ({valid[1]} - {valid[0]} + 1) : 0)" for i,valid in enumerate(valid_groups_values[:valid_index]))
                qualify = ""
                if start >= valid_groups_values[valid_index][0] and start <= valid_groups_values[valid_index][1]:
                    qualify = f"!cvm::bitmanip::index<{valid_index}>(_packet_gen_valid) ? 0 : "

                %>\
                ${field.name}(${qualify}cvm::bitmanip::array_slice<decltype(${field.name})>(words.data(), ${field.width + start - 1} + ${packets.enum_width()} - (${valids_offset or 0}), ${start} + ${packets.enum_width()} - (${valids_offset or 0})))${[",", ""][(i+1)//len(subpacket.fields)]}
<% start += field.width %>\
            %endfor
            {
                std::copy_n(words.begin() + N - 2*X, X, (${type(packets).transfer_word_c_type()}*)(&birth) );
                std::copy_n(words.begin() + N - 1*X, X, (${type(packets).transfer_word_c_type()}*)(&signal_enqueued_time) );
                assert(birth < signal_enqueued_time && "signal_enqueued before birth");
            }
        };
    }
%endfor
%endfor

}

% if any(subpacket.context for packet in packets.packets for subpacket in packet):
extern "C" void ${packets.name}_finish();
% endif
