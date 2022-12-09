#include "cvm/messenger.hpp"
#include "example/transactions.hpp"
#include <iostream>
#include "cvm/plusargs.hpp"

DEFINE_bool(log, true, "Print retires");

class retire_monitor {

    public:

        retire_monitor() {
            cvm::messenger<transactions::m_ret>::connect(
                [this] (const transactions::m_ret& ret) {
                    return this->retire(ret);
                }
             );
        }

    private:

        void retire(const transactions::m_ret& m_ret) {
            if (FLAGS_log) {
                std::cout << std::hex
                    <<  "arn  0x" << +m_ret.arn
                    << " prn  0x" << m_ret.prn
                    << " foo  0x" << m_ret.foo
                    << " bar  0x" << +m_ret.bar
                    << " wide 0x" << m_ret.wide
                    << "\n";
            }
        }

};

extern "C" void start_monitor() {
    cvm::plusargs::parse();
    static retire_monitor mon;
}

