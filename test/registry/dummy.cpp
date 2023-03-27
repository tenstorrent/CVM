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

REGISTRY_register(dummy, core, 0)

extern "C" void cvm_registry_reset() {
    cvm::registry::reset();
};

TEST(Registry, Dummy) {
    EXPECT_EQ(_registry::dummy_SUCCESS, true);
}
