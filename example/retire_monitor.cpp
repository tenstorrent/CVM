#include "cvm/messenger.hpp"
#include "transactions.hpp"

class retire_monitor {

    public:

        retire_monitor() {
            using namespace std::placeholders;
            cvm::messenger<transactions::m_ret>::connect(
                [this] (const transactions::m_ret& ret) {
                    return this->retire(ret);
                }
             );
        }

    private:

        void retire(const transactions::m_ret& m_ret) {
            std::cout << "arn " << +m_ret.arn << " prn " << m_ret.prn << "\n";
        }

};

extern "C" void start_monitor() {
    static retire_monitor mon;
}

