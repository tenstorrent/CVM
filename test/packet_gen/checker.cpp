// SPDX-FileCopyrightText: © 2026 Tenstorrent USA, Inc.
// SPDX-License-Identifier: Apache-2.0

#include "cvm/topology.hpp"
#include "cvm/registry.hpp"
#include "cvm/topology_defs.hpp"
#include "cvm/type_traits.hpp"
#include "transactions.hpp"
#include <gtest/gtest.h>

// Track whether dpi_init was called
static int initialize_domain1_call_count = 0;
static int initialize_domain2_call_count = 0;

extern "C" void transactions_dpi_init_domain_1() {
    initialize_domain1_call_count++;
}
extern "C" void transactions_dpi_init_domain_2() {
    initialize_domain2_call_count++;
}

class checker {

    public:

        checker(cvm::topology::loc_t loc, unsigned int /*id*/) {
            check<uint32_t>("CORE.width", cvm::topology::attr(loc, "WIDTH").second, 2);
            check<uint32_t>("CORE.s", cvm::topology::list_attr(loc, "S").second.at(0), 1);
            check<std::string>("CORE.name", cvm::topology::name(loc), "CORE");

            auto axi0_loc = cvm::topology::get_from_hierarchy("TOP.CLUSTER.AXI", 0);
            check<uint32_t>("AXI[0].id_width", cvm::topology::attr(axi0_loc, "ID_WIDTH").second, 12);
            check<uint32_t>("AXI[0].addr_width", cvm::topology::attr(axi0_loc, "ADDR_WIDTH").second, 52);
            check<uint32_t>("AXI[0].data_width", cvm::topology::attr(axi0_loc, "DATA_WIDTH").second, 256);

            auto axi1_loc = cvm::topology::get_from_hierarchy("TOP.CLUSTER.AXI", 1);
            check<uint32_t>("AXI[1].id_width", cvm::topology::attr(axi1_loc, "ID_WIDTH").second, 10);
            check<uint32_t>("AXI[1].addr_width", cvm::topology::attr(axi1_loc, "ADDR_WIDTH").second, 64);
            check<uint32_t>("AXI[1].data_width", cvm::topology::attr(axi1_loc, "DATA_WIDTH").second, 256);

            auto cluster_loc = cvm::topology::get_from_hierarchy("TOP.CLUSTER", 0);
            auto axi1_id = cvm::topology::attr(cluster_loc, "AXI1_ID").second;
            check<uint32_t>("AXI1_ID", axi1_id, axi1_loc);

            auto cluster_attr = cvm::topology::attr(cluster_loc, "CLUSTER_ATTR").second;
            check<uint32_t>("CLUSTER_ATTR_AT_CORE", cluster_attr, cvm::topology::attr(axi0_loc, "CORE_CLUSTER_ATTR").second);


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
                [] (const transactions::dut::ctx<>& ret) {
                    transactions_finish();
                }
            );
            cvm::registry::messenger.connect<transactions::dut::anchor_test<>>(
                loc,
                [this] (const transactions::dut::anchor_test<>& ret) {
                    this->check(ret);
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

        void check(const transactions::dut::anchor_test<>& pkt) {
            check("shared_field1", pkt.shared_field1, decltype(pkt.shared_field1)(0x1234));
            check("shared_field2", pkt.shared_field2, decltype(pkt.shared_field2)(0xDEADBEEF));
            check("extra_field", pkt.extra_field, decltype(pkt.extra_field)(0x42));
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
        << "dpi_init function 'transactors_dpi_init_domain_1' was never called";
    EXPECT_EQ(initialize_domain2_call_count, 0) 
        << "dpi_init function 'transactors_dpi_init_domain_2' should NOT be called";
    cvm::registry::shutdown();
}
