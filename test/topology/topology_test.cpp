// SPDX-FileCopyrightText: © 2026 Tenstorrent USA, Inc.
// SPDX-License-Identifier: Apache-2.0

#include <set>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "cvm/topology.hpp"
#include "cvm/topology_defs.hpp"

namespace {

  using topo = cvm::static_topology;
  using loc_t = cvm::topology::loc_t;

  template <typename T>
  std::vector<T> to_vector(std::span<const T> values) {
    return std::vector<T>(values.begin(), values.end());
  }

  std::set<std::string> attribute_keys() {
    std::set<std::string> keys;
    for (const auto& entry : topo::attributes())
      keys.emplace(entry.key);
    return keys;
  }

  std::set<std::string> list_attribute_keys() {
    std::set<std::string> keys;
    for (const auto& entry : topo::list_attributes())
      keys.emplace(entry.key);
    return keys;
  }

}

// The generated tables are the only source of location data, so a lookup that
// resolves during translation proves the tables are usable in a constant
// expression rather than merely being constexpr-qualified.
static_assert(topo::type_exists("CORE"));
static_assert(not topo::type_exists("NOT_A_TYPE"));
static_assert(topo::hierarchy_exists("TOP.CLUSTER.CORE"));
static_assert(topo::hierarchy_exists("TOP.IO.PORT[1]"));
static_assert(not topo::hierarchy_exists("TOP.CLUSTER.NOPE"));
static_assert(topo::locations_of_type("CORE").size() == 6);
static_assert(topo::locations_of_type("CLUSTER").size() == 2);
static_assert(topo::locations_of_type("FABRIC").size() == 2);
static_assert(topo::location_of_type("CORE", 99) == topo::null);
static_assert(topo::location_of_hierarchy("TOP", 0) != topo::null);
static_assert(topo::attribute(topo::location_of_type("CORE", 0), "WIDTH") == 2);
static_assert(not topo::attribute(topo::location_of_type("SCRATCH", 0), "WIDTH").has_value());
static_assert(topo::list_attribute(topo::location_of_type("CORE", 0), "LANES")->size() == 3);
static_assert(topo::list_attribute(topo::location_of_type("CORE", 0), "EMPTY_LIST")->empty());
static_assert(topo::name(topo::location_of_type("CORE", 0)) == "CORE");

TEST(StaticTopology, TypeLookupsMatchRuntime) {
  for (const auto& entry : topo::types()) {
    const std::string key(entry.key);

    EXPECT_EQ(to_vector(topo::locations_of_type(key)), cvm::topology::get_from_type(key)) << key;

    const auto locations = topo::locations_of_type(key);
    for (unsigned id = 0; id < locations.size() + 2; ++id)
      EXPECT_EQ(topo::location_of_type(key, id), cvm::topology::get_from_type(key, id)) << key << "[" << id << "]";
  }
}

TEST(StaticTopology, HierarchyLookupsMatchRuntime) {
  for (const auto& entry : topo::hierarchies()) {
    const std::string key(entry.key);

    EXPECT_EQ(to_vector(topo::locations_of_hierarchy(key)), cvm::topology::get_from_hierarchy(key)) << key;

    const auto locations = topo::locations_of_hierarchy(key);
    for (unsigned id = 0; id < locations.size() + 2; ++id)
      EXPECT_EQ(topo::location_of_hierarchy(key, id), cvm::topology::get_from_hierarchy(key, id)) << key << "[" << id << "]";
  }
}

TEST(StaticTopology, NamesMatchRuntime) {
  for (loc_t loc = 0; loc < topo::names().size() + 4; ++loc)
    EXPECT_EQ(std::string(topo::name(loc)), cvm::topology::name(loc)) << loc;
}

// Every location is probed with every attribute key in the topology, not just
// the keys it declares, so a lookup that wrongly reports a hit is caught too.
TEST(StaticTopology, AttributesMatchRuntime) {
  const auto keys = attribute_keys();
  ASSERT_FALSE(keys.empty());

  for (loc_t loc = 0; loc < topo::names().size() + 4; ++loc) {
    for (const auto& key : keys) {
      const auto expected = cvm::topology::attr(loc, key);
      const auto actual = topo::attribute(loc, key);

      EXPECT_EQ(actual.has_value(), expected.first) << loc << "." << key;
      if (expected.first)
        EXPECT_EQ(*actual, expected.second) << loc << "." << key;
    }

    EXPECT_FALSE(topo::attribute(loc, "NOT_AN_ATTRIBUTE").has_value()) << loc;
  }
}

TEST(StaticTopology, ListAttributesMatchRuntime) {
  const auto keys = list_attribute_keys();
  ASSERT_FALSE(keys.empty());

  for (loc_t loc = 0; loc < topo::names().size() + 4; ++loc) {
    for (const auto& key : keys) {
      const auto expected = cvm::topology::list_attr(loc, key);
      const auto actual = topo::list_attribute(loc, key);

      EXPECT_EQ(actual.has_value(), expected.first) << loc << "." << key;
      if (expected.first)
        EXPECT_EQ(to_vector(*actual), expected.second) << loc << "." << key;
    }

    EXPECT_FALSE(topo::list_attribute(loc, "NOT_A_LIST").has_value()) << loc;
  }
}

TEST(StaticTopology, UnknownKeysResolveEmpty) {
  EXPECT_TRUE(topo::locations_of_type("NOT_A_TYPE").empty());
  EXPECT_TRUE(topo::locations_of_hierarchy("TOP.NOT_A_PATH").empty());
  EXPECT_EQ(topo::location_of_type("NOT_A_TYPE", 0), topo::null);
  EXPECT_EQ(topo::location_of_hierarchy("TOP.NOT_A_PATH", 0), topo::null);

  EXPECT_EQ(cvm::topology::get_from_type("NOT_A_TYPE").size(), 0u);
  EXPECT_EQ(cvm::topology::get_from_hierarchy("TOP.NOT_A_PATH").size(), 0u);
}

// Attributes that are neither integers nor integer lists are dropped by the
// generator; both lookup paths must agree that they are absent.
TEST(StaticTopology, StringAttributesAreNotEmitted) {
  const loc_t port = topo::location_of_type("PORT", 0);
  ASSERT_NE(port, topo::null);

  EXPECT_FALSE(topo::attribute(port, "LABEL").has_value());
  EXPECT_FALSE(topo::list_attribute(port, "LABEL").has_value());
  EXPECT_FALSE(cvm::topology::attr(port, "LABEL").first);
}

TEST(StaticTopology, ArrayLocationsCarryPerGroupAttributes) {
  const auto ports = topo::locations_of_hierarchy("TOP.IO.PORT");
  ASSERT_EQ(ports.size(), 3u);

  // One PORT_A instance, then two PORT_B instances.
  EXPECT_EQ(topo::attribute(ports[0], "ADDR_WIDTH"), 52u);
  EXPECT_EQ(topo::attribute(ports[0], "ID_WIDTH"), 12u);
  EXPECT_EQ(topo::attribute(ports[0], "SHARD"), 1u);
  EXPECT_EQ(topo::attribute(ports[0], "TOTAL"), 3u);

  EXPECT_EQ(topo::attribute(ports[1], "ADDR_WIDTH"), 64u);
  EXPECT_EQ(topo::attribute(ports[1], "ID_WIDTH"), 10u);
  EXPECT_EQ(topo::attribute(ports[1], "SHARD"), 2u);
  EXPECT_EQ(topo::attribute(ports[1], "TOTAL"), 3u);

  EXPECT_EQ(to_vector(topo::locations_of_hierarchy("TOP.IO.PORT[0]")).size(), 1u);
  EXPECT_EQ(to_vector(topo::locations_of_hierarchy("TOP.IO.PORT[1]")).size(), 2u);
}

TEST(StaticTopology, ReferencesResolveToConcreteValues) {
  const loc_t cluster = topo::location_of_type("CLUSTER", 0);
  ASSERT_NE(cluster, topo::null);

  EXPECT_EQ(topo::attribute(cluster, "CLUSTER_ATTR"), 1000u);
  EXPECT_EQ(topo::attribute(cluster, "CORE_WIDTH"), 2u);

  const loc_t io = topo::location_of_type("IO", 0);
  ASSERT_NE(io, topo::null);
  EXPECT_EQ(topo::attribute(io, "PORT1_ID"),
            topo::locations_of_hierarchy("TOP.IO.PORT[1]")[0]);
}
