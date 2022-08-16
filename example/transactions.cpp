// FIXME generate
#include "transactions.hpp"
#include <iostream>
#include "cvm/messenger.hpp"

extern "C" void transactions_message(std::int32_t message_number, const std::uint8_t* message) {
    if (message_number == transactions::MSG_NUMBER_M_RET) {
        transactions::m_ret m_ret(message);
        cvm::messenger<transactions::m_ret>::signal(m_ret);
    }
}
