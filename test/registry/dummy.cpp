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

REGISTRY_register(dummy, TOP.CLUSTER.CORE, 0)

extern "C" void cvm_registry_reset() {
    cvm::registry::build();
    cvm::registry::configure();
    cvm::registry::check();
    cvm::registry::shutdown();
};

TEST(Registry, Dummy) {
    EXPECT_EQ(cvm::registry::is_registered<dummy>(), true);
}
