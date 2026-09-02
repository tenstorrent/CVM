// SPDX-FileCopyrightText: © 2026 Tenstorrent USA, Inc.
// SPDX-License-Identifier: Apache-2.0

#include <algorithm>
#include <unordered_set>
#include <vector>

#include <gtest/gtest.h>

#include "cvm/registry.hpp"
#include "cvm/topology_defs.hpp"

namespace {

  std::unordered_set<cvm::topology::loc_t> static_locations;
  std::unordered_set<cvm::topology::loc_t> static_hierarchy_locations;

  // These register into reset domain 1, the domain the testbench actually
  // builds (cvm_registry_reset2 drives build_domain(1)), so the constructors
  // below do run. Locations are checked there rather than from a test body,
  // which would run before any construction.
  void expect_one_of(cvm::topology::loc_t loc, const std::vector<cvm::topology::loc_t>& expected) {
    EXPECT_NE(std::find(expected.begin(), expected.end(), loc), expected.end())
      << "constructed at location " << loc << ", which the runtime lookup does not report";
  }

}

class dummy_static {
  public:
    dummy_static(cvm::topology::loc_t loc, unsigned) {
      expect_one_of(loc, cvm::topology::get_from_type("DUMMY"));
      EXPECT_FALSE(static_locations.count(loc));
      static_locations.insert(loc);
    };
};

class dummy_static_hierarchy {
  public:
    dummy_static_hierarchy(cvm::topology::loc_t loc, unsigned) {
      EXPECT_EQ(loc, cvm::topology::get_from_hierarchy("TOP.CLUSTER.CORE", 0));
      EXPECT_FALSE(static_hierarchy_locations.count(loc));
      static_hierarchy_locations.insert(loc);
    };
};

class dummy_static_absent {
  public:
    dummy_static_absent(cvm::topology::loc_t, unsigned) {
      ADD_FAILURE() << "constructed for a path the topology does not contain";
    };
};

REGISTRY_register_required_with_reset(dummy_static, DUMMY, (cvm::registry::all), 1)
REGISTRY_register_required_with_reset(dummy_static_hierarchy, TOP.CLUSTER.CORE, 0, 1)
REGISTRY_register_with_reset(dummy_static_absent, TOP.CLUSTER.NOT_PRESENT, 0, 1)

// The predicate the _required macros assert on, checked directly so both
// answers are covered without a build that is expected to fail.
static_assert(cvm::registry::resolvable<cvm::static_topology, "DUMMY", cvm::registry::all>());
static_assert(cvm::registry::resolvable<cvm::static_topology, "TOP.CLUSTER.CORE", 0>());
static_assert(not cvm::registry::resolvable<cvm::static_topology, "TOP.CLUSTER.NOT_PRESENT", 0>());
static_assert(not cvm::registry::resolvable<cvm::static_topology, "NOT_A_TYPE", cvm::registry::all>());
static_assert(not cvm::registry::resolvable<cvm::static_topology, "DUMMY", 99>());

TEST(Registry, StaticRegistrationResolvesLikeRuntime) {
  EXPECT_TRUE(cvm::registry::is_registered<dummy_static>());
  EXPECT_TRUE(cvm::registry::is_registered<dummy_static_hierarchy>());

  const auto by_type = cvm::static_topology::locations_of_type("DUMMY");
  EXPECT_EQ(std::vector<cvm::topology::loc_t>(by_type.begin(), by_type.end()),
            cvm::topology::get_from_type("DUMMY"));

  EXPECT_EQ(cvm::static_topology::location_of_hierarchy("TOP.CLUSTER.CORE", 0),
            cvm::topology::get_from_hierarchy("TOP.CLUSTER.CORE", 0));
}

TEST(Registry, StaticRegistrationSkipsAbsentPaths) {
  EXPECT_FALSE(cvm::registry::is_registered<dummy_static_absent>());
}
