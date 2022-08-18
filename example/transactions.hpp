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
        constexpr m_ret(const std::uint8_t* bytes, const size_t offset) :
            arn(cvm::bitmanip::array_slice<decltype(arn)>(bytes,  4+offset, 0+offset)),
            prn(cvm::bitmanip::array_slice<decltype(prn)>(bytes, 14+offset, 5+offset))
            {}
    };

}
