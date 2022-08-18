// FIXME generate
#include "transactions.hpp"
#include <iostream>
#include "cvm/messenger.hpp"
#include <type_traits>

typedef void transaction_messenger(const std::uint8_t* message);

extern "C" void transactions_message(const std::uint8_t* message) {
    transactions::message_number message_number = transactions::message_number(cvm::bitmanip::array_slice<std::underlying_type<transactions::message_number>::type>(message, 0, 0));

    if (message_number == transactions::MSG_NUMBER_M_RET) {
        transactions::m_ret m_ret(message, 1);
        cvm::messenger<transactions::m_ret>::signal(m_ret);
    } else{
        assert(0);
    }
}

extern "C" void transactions_message_m_ret(const std::uint8_t* message) {
    transactions_message(message);
}
