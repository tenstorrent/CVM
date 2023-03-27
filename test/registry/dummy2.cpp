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
    };
};

REGISTRY_register(dummy2, cluster, (cvm::registry::all))

TEST(Registry, Dummy2) {
    EXPECT_EQ(_registry::dummy2_SUCCESS, true);
}
