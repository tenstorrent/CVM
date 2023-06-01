#include <gtest/gtest.h>
#include "cvm/topology.hpp"
#include "cvm/registry.hpp"

class dummy3 {
  public:
    dummy3(cvm::topology::loc_t loc, unsigned id, int x) {
      EXPECT_EQ(x, 5);
    };
};

REGISTRY_register(dummy3, CORE, (cvm::registry::all), 5)

TEST(Registry, Dummy3) {
    EXPECT_EQ(_registry::dummy3_SUCCESS, true);
}
