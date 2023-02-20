#include <gtest/gtest.h>
#include <unordered_set>
#include "cvm/topology.hpp"
#include "cvm/registry.hpp"

class dummy {
  public:
    dummy(cvm::topology::loc_t loc, unsigned id) {
      EXPECT_EQ(id, 0);
    };
};

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

REGISTRY_register(dummy, core, 0)
REGISTRY_register(dummy2, cluster, (cvm::registry::all))

extern "C" void cvm_registry_reset();

TEST(Registry, Dummy) {
    EXPECT_EQ(dummy_SUCCESS, true);
    EXPECT_EQ(dummy2_SUCCESS, true);
    cvm::registry::reset();
}
