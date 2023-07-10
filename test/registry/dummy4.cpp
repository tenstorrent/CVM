#include <gtest/gtest.h>
#include "cvm/registry.hpp"

int x = 0, y = 0;

class dummy4 {
  public:
    dummy4() {};

    void configure() {
      x = 1;
      return;
    }

    void check() {
      y = 1;
      return;
    }
};

REGISTRY_register_topology_agn(dummy4)

TEST(Registry, Dummy4) {
    EXPECT_EQ(_registry::dummy4_SUCCESS, true);
    EXPECT_EQ(x, 1);
    EXPECT_EQ(y, 1);
}
