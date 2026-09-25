// SPDX-FileCopyrightText: © 2026 Tenstorrent USA, Inc.
// SPDX-License-Identifier: Apache-2.0

#include <string>

#include <gtest/gtest.h>

#include "cvm/topology.hpp"
#include "cvm/topology_defs.hpp"

namespace {

  using topo = cvm::static_topology;
  using loc_t = cvm::topology::loc_t;

}

// The generated tables are the only source of location data, so a lookup that
// resolves during translation proves the tables are usable in a constant
// expression rather than merely being constexpr-qualified.
static_assert(topo::type_exists("CORE"));
static_assert(not topo::type_exists("NOT_A_TYPE"));
static_assert(topo::hierarchy_exists("TOP.CLUSTER.CORE"));
static_assert(topo::hierarchy_exists("TOP.IO.PORT[1]"));
static_assert(not topo::hierarchy_exists("TOP.CLUSTER.NOPE"));
static_assert(topo::get_from_type("CORE").size() == 6);
static_assert(topo::get_from_type("CLUSTER").size() == 2);
static_assert(topo::get_from_type("FABRIC").size() == 2);
static_assert(topo::get_from_type("NOT_A_TYPE").empty());
static_assert(topo::get_from_hierarchy("TOP.NOT_A_PATH").empty());
static_assert(topo::get_from_type("CORE", 99) == topo::null);
static_assert(topo::get_from_hierarchy("TOP.NOT_A_PATH", 0) == topo::null);
static_assert(topo::get_from_hierarchy("TOP", 0) != topo::null);
static_assert(topo::attr(topo::get_from_type("CORE", 0), "WIDTH") == 2);
static_assert(not topo::attr(topo::get_from_type("SCRATCH", 0), "WIDTH").has_value());
static_assert(topo::list_attr(topo::get_from_type("CORE", 0), "LANES")->size() == 3);
static_assert(topo::list_attr(topo::get_from_type("CORE", 0), "EMPTY_LIST")->empty());
static_assert(topo::name(topo::get_from_type("CORE", 0)) == "CORE");

// Attributes that are neither integers nor integer lists are dropped by the
// generator.
static_assert(not topo::attr(topo::get_from_type("PORT", 0), "LABEL").has_value());
static_assert(not topo::list_attr(topo::get_from_type("PORT", 0), "LABEL").has_value());

// One PORT_A instance, then two PORT_B instances under TOP.IO.PORT; each array
// group carries its own attribute values.
static_assert(topo::get_from_hierarchy("TOP.IO.PORT").size() == 3);
static_assert(topo::get_from_hierarchy("TOP.IO.PORT[0]").size() == 1);
static_assert(topo::get_from_hierarchy("TOP.IO.PORT[1]").size() == 2);
static_assert(topo::attr(topo::get_from_hierarchy("TOP.IO.PORT", 0), "ADDR_WIDTH") == 52);
static_assert(topo::attr(topo::get_from_hierarchy("TOP.IO.PORT", 0), "ID_WIDTH") == 12);
static_assert(topo::attr(topo::get_from_hierarchy("TOP.IO.PORT", 0), "SHARD") == 1);
static_assert(topo::attr(topo::get_from_hierarchy("TOP.IO.PORT", 0), "TOTAL") == 3);
static_assert(topo::attr(topo::get_from_hierarchy("TOP.IO.PORT", 1), "ADDR_WIDTH") == 64);
static_assert(topo::attr(topo::get_from_hierarchy("TOP.IO.PORT", 1), "ID_WIDTH") == 10);
static_assert(topo::attr(topo::get_from_hierarchy("TOP.IO.PORT", 1), "SHARD") == 2);
static_assert(topo::attr(topo::get_from_hierarchy("TOP.IO.PORT", 1), "TOTAL") == 3);

// @-references resolve to concrete values at generation time.
static_assert(topo::attr(topo::get_from_type("CLUSTER", 0), "CLUSTER_ATTR") == 1000);
static_assert(topo::attr(topo::get_from_type("CLUSTER", 0), "CORE_WIDTH") == 2);
static_assert(topo::attr(topo::get_from_type("IO", 0), "PORT1_ID") ==
              topo::get_from_hierarchy("TOP.IO.PORT[1]", 0));

// The runtime cvm::topology functions are generated forwarders onto the same
// tables; each is exercised once for the string/vector conversions they own.
TEST(StaticTopology, RuntimeFacadeForwards) {
  const loc_t core = cvm::topology::get_from_type("CORE", 0);
  ASSERT_NE(core, topo::null);

  EXPECT_EQ(cvm::topology::get_from_type("CORE").size(), 6u);
  EXPECT_NE(cvm::topology::get_from_hierarchy("TOP.CLUSTER.CORE", 0), topo::null);
  EXPECT_EQ(cvm::topology::get_from_hierarchy("TOP.NOT_A_PATH").size(), 0u);
  EXPECT_EQ(cvm::topology::get_from_type("CORE", 99), topo::null);
  EXPECT_EQ(cvm::topology::attr(core, "WIDTH"), std::make_pair(true, 2u));
  EXPECT_FALSE(cvm::topology::attr(core, "NOT_AN_ATTRIBUTE").first);
  EXPECT_EQ(cvm::topology::list_attr(core, "LANES").second.size(), 3u);
  EXPECT_FALSE(cvm::topology::list_attr(core, "NOT_A_LIST").first);
  EXPECT_EQ(cvm::topology::name(core), "CORE");
}
