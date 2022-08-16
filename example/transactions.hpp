// FIXME generate
#pragma once

#include <cinttypes>
#include "cvm/bitmanip.hpp"
#include <iostream>

namespace transactions {

    typedef enum {
        MSG_NUMBER_M_RET = 0,
    } message_number;

    struct m_ret {
        std::uint8_t  arn;
        std::uint16_t prn;
        m_ret(const std::uint8_t* bytes) {
            arn = cvm::bitmanip::array_slice<decltype(arn)>(bytes,  4, 0);
            prn = cvm::bitmanip::array_slice<decltype(prn)>(bytes, 14, 5);
        }
    };

}
