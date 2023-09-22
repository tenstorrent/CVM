#include <gtest/gtest.h>
#include <unordered_set>
#include "cvm/topology.hpp"
#include "cvm/registry.hpp"

std::unordered_set<cvm::topology::loc_t> locations;
std::unordered_set<unsigned> ids;

class dummy2 {
  public:
    dummy2(cvm::topology::loc_t loc, unsigned id) {
      EXPECT_FALSE(locations.count(loc));
      EXPECT_FALSE(ids.count(id));
      locations.insert(loc);
      ids.insert(id);
      EXPECT_LT(id, 16);
    };
};

REGISTRY_register(dummy2, CORE, (cvm::registry::all))

TEST(Registry, Dummy2) {
    EXPECT_EQ(cvm::registry::is_registered<dummy2>(), true);
}
