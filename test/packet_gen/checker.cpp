#include "cvm/topology.hpp"
#include "cvm/registry.hpp"
#include "cvm/type_traits.hpp"
#include "transactions.hpp"
#include <gtest/gtest.h>

// Track whether dpi_init was called
static int initialize_domain1_call_count = 0;

extern "C" void transactions_dpi_init_domain_1() {
    initialize_domain1_call_count++;
}

class checker {

    public:

        checker(cvm::topology::loc_t loc, unsigned int /*id*/) {
            check<uint32_t>("CORE.width", cvm::topology::attr(loc, "WIDTH").second, 2);
            check<uint32_t>("CORE.s", cvm::topology::list_attr(loc, "S").second.at(0), 1);
            check<std::string>("CORE.name", cvm::topology::name(loc), "CORE");
            cvm::registry::messenger.connect<transactions::dut::pkt<>>(
                loc,
                [this] (const transactions::dut::pkt<>& ret) {
                    this->check(ret);
                }
            );
            cvm::registry::messenger.connect<transactions::dut2::pkt2<>>(
                loc,
                [this] (const transactions::dut2::pkt2<>& ret) {
                    this->check(ret);
                }
            );
            cvm::registry::messenger.connect<transactions::dut2::pkt2<1>>(
                loc,
                [this] (const transactions::dut2::pkt2<1>& ret) {
                    this->check(ret);
                }
            );
            cvm::registry::messenger.connect<transactions::dut::never_seen<>>(
                loc,
                [] (const transactions::dut::never_seen<>&) {
                    FAIL() << "This transaction should never be seen.";
                }
            );
            cvm::registry::messenger.connect<transactions::all_disabled::never_seen2<>>(
                loc,
                [] (const transactions::all_disabled::never_seen2<>&) {
                    FAIL() << "This transaction should never be seen.";
                }
            );
            cvm::registry::messenger.connect<transactions::dut::ctx<>>(
                loc,
                [] (const transactions::dut::ctx<>&) {
                    transactions_finish();
                }
            );
        }

    private:

        int count_ = 0;

        template<typename T>
            void check(const std::string& name, const T& actual, const T& expected) {
                ASSERT_EQ(actual, expected) << name;
            }

        void check(const transactions::dut::pkt<>& pkt) {
            auto c = count_ / 8;
            check("num", pkt.num, decltype(pkt.num)(count_ % 8));
            check("num1[0][0]", pkt.num1.at(0).at(0), cvm::type_traits::remove_all_array_extents<decltype(pkt.num1)>::type(1));
            check("num1[0][1]", pkt.num1.at(0).at(1), cvm::type_traits::remove_all_array_extents<decltype(pkt.num1)>::type(4));
            check("num1[1][0]", pkt.num1.at(1).at(0), cvm::type_traits::remove_all_array_extents<decltype(pkt.num1)>::type(3));
            check("num1[1][1]", pkt.num1.at(1).at(1), cvm::type_traits::remove_all_array_extents<decltype(pkt.num1)>::type(5));
            check("num2[0][0][0]", pkt.num2.at(0).at(0).at(0), cvm::type_traits::remove_all_array_extents<decltype(pkt.num2)>::type(0x1));
            check("num2[0][1][0]", pkt.num2.at(0).at(1).at(0), cvm::type_traits::remove_all_array_extents<decltype(pkt.num2)>::type(0x1000));
            check("num2[1][0][1]", pkt.num2.at(1).at(0).at(1), cvm::type_traits::remove_all_array_extents<decltype(pkt.num2)>::type(0x11));
            check("num2[2][1][2]", pkt.num2.at(2).at(1).at(2), cvm::type_traits::remove_all_array_extents<decltype(pkt.num2)>::type(0x1100));
            check("x256", pkt.x256, decltype(pkt.x256)(1) << 255 | decltype(pkt.x256)(c));
            check("x54", pkt.x54, decltype(pkt.x54)(1) << 53 | decltype(pkt.x54)(c));

            check("_packet_gen_valid", pkt._packet_gen_valid, decltype(pkt._packet_gen_valid)(int(c == 4) | int(c == 5) << 1 | int(c == 6 || c == 7) << 2));

            check("valid1"   , pkt.valid1   , decltype(pkt.valid1   )(c == 4));
            check("optional1", pkt.optional1, decltype(pkt.optional1)(c == 4 ? 4 : 0));
            check("valid2"   , pkt.valid2   , decltype(pkt.valid2   )(c == 5));
            check("optional2", pkt.optional2, decltype(pkt.optional2)(c == 5 ? 5 : 0));
            check("valid3a"  , pkt.valid3a  , decltype(pkt.valid3a  )(c == 6 || c == 7));
            check("valid3b"  , pkt.valid3b  , decltype(pkt.valid3b  )(c == 7));
            check("optional3", pkt.optional3, decltype(pkt.optional3)((c == 6 || c == 7) ? c : 0));

            count_++;
        }

        void check(const transactions::dut2::pkt2<0>& pkt2) {
            check("dummy2", pkt2.dummy2, decltype(pkt2.dummy2)(3));
        }

        void check(const transactions::dut2::pkt2<1>& pkt2) {
            check("dummy2", pkt2.dummy2, decltype(pkt2.dummy2)(3));
        }

};

REGISTRY_register(checker, TOP.CLUSTER.CORE, 0)

template <typename T>
concept HasDummy = requires(T t) {
    { t.dummy }; // just being able to access it
};

static_assert(not HasDummy<transactions::dut::zero<>>);

extern "C" void start_checker() {
    cvm::registry::build();
    cvm::registry::configure();
    cvm::registry::check();
}

extern "C" void end_checker() {
    // Verify dpi_init was called at least once
    EXPECT_GT(initialize_domain1_call_count, 0) 
        << "dpi_init function 'initialize_domain1' was never called";
    cvm::registry::shutdown();
}
