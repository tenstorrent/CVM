#include "cvm/topology.hpp"
#include "cvm/registry.hpp"
#include "transactions.hpp"
#include <gtest/gtest.h>

class checker {

    public:

        checker() {
            auto loc = cvm::topology::get("TOP.CLUSTER.CORE", 0);
            check<uint32_t>("CORE.width", cvm::topology::attr(loc, "width").second, 2);
            cvm::registry::messenger.connect<transactions::pkt>(
                loc,
                [this] (const transactions::pkt& ret) {
                    return this->check(ret);
                }
             );
            cvm::registry::messenger.connect<transactions::ctx>(
                loc,
                [this] (const transactions::ctx& ret) {
                    transactions_finish();
                }
             );
        }

    private:

        int count_ = 0;

        template<typename T>
            void check(const std::string& name, const T& actual, const T& expected) {
                ASSERT_EQ(actual, expected);
            }

        void check(const transactions::pkt& pkt) {
            check("num", pkt.num, decltype(pkt.num)(count_ % 8));
            check("x256", pkt.x256, decltype(pkt.x256)(1) << 255 | decltype(pkt.x256)(count_ / 8));
            check("x54", pkt.x54, decltype(pkt.x54)(1) << 53 | decltype(pkt.x54)(count_ / 8));
            count_++;
        }

};

extern "C" void start_checker() {
    static checker mon;
}

