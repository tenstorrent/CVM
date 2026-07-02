// SPDX-FileCopyrightText: © 2026 Tenstorrent USA, Inc.
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>
#include <unordered_set>
#include "cvm/topology.hpp"
#include "cvm/registry.hpp"

unsigned dummy_loc = 0;

class dummy {
  public:
    dummy(cvm::topology::loc_t loc, unsigned id) {
      dummy_loc = loc;
      EXPECT_EQ(id, 0);
    };
};

REGISTRY_register(dummy, TOP.CLUSTER.DUMMY, 0)

extern "C" void cvm_registry_reset() {
    cvm::registry::build(0);
    cvm::registry::configure();
    cvm::registry::check();
    cvm::registry::shutdown(0);
};

bool built = false;
bool shutdown = false;

class dummy4 {
  public:
    dummy4(cvm::topology::loc_t loc, unsigned id) {
      built = true;
      EXPECT_EQ(id, 0);
    };

    ~dummy4() {
      shutdown = true;
    };
};

REGISTRY_register_with_reset(dummy4, TOP.CLUSTER.DUMMY, 0, 1)

extern "C" void cvm_registry_reset2() {
    built = false;
    shutdown = false;

    EXPECT_EQ(built, false);
    EXPECT_EQ(shutdown, false);

    cvm::registry::build_domain(1);
    EXPECT_EQ(built, true);
    cvm::registry::shutdown_domain(1);
    EXPECT_EQ(shutdown, true);
}

TEST(Registry, Dummy) {
    EXPECT_EQ(cvm::registry::is_registered<dummy>(), true);
}

TEST(Topology, ConnectionReferenceResolve) {
    auto cluster = cvm::topology::get_from_hierarchy("TOP.CLUSTER", 0);

    auto foo0 = cvm::topology::get_from_hierarchy("TOP.CLUSTER.FOO", 0);
    auto val  = cvm::topology::attr(cluster, "FOO0_ID").second;
    EXPECT_EQ(val, foo0);

    auto core0 = cvm::topology::get_from_hierarchy("TOP.CLUSTER.CORE", 0);
    auto val2  = cvm::topology::attr(cluster, "CORE0_ATTR1").second;
    auto val3  = cvm::topology::attr(core0, "ATTR1").second;
    EXPECT_EQ(val2, val3);

    auto cluster_attr = cvm::topology::attr(cluster, "CLUSTER_ATTR").second;
    auto core_cluster_attr = cvm::topology::attr(core0, "CORE_CLUSTER_ATTR").second;
    EXPECT_EQ(cluster_attr, core_cluster_attr);

}
