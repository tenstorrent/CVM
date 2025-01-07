#include <gtest/gtest.h>
#include <unordered_set>
#include "cvm/topology.hpp"
#include "cvm/registry.hpp"

unsigned dummy_loc = 0;

class dummy {
  public:
    dummy(cvm::topology::loc_t loc, unsigned id) {
      dummy_loc = loc;
      EXPECT_EQ(id, 0);
    };
};

REGISTRY_register(dummy, TOP.CLUSTER.DUMMY, 0)

extern "C" void cvm_registry_reset() {
    cvm::registry::build();
    cvm::registry::configure();
    cvm::registry::check();
    cvm::registry::shutdown();

    EXPECT_NE(dummy_loc, 0);
    cvm::registry::build(dummy_loc);
    cvm::registry::shutdown(dummy_loc);
};

TEST(Registry, Dummy) {
    EXPECT_EQ(cvm::registry::is_registered<dummy>(), true);
}
