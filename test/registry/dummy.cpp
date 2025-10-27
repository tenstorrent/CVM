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
    cvm::registry::build(0);
    cvm::registry::configure();
    cvm::registry::check();
    cvm::registry::shutdown(0);
};

bool built = false;
bool shutdown = false;

class dummy4 {
  public:
    dummy4(cvm::topology::loc_t loc, unsigned id) {
      built = true;
      EXPECT_EQ(id, 0);
    };

    ~dummy4() {
      shutdown = true;
    };
};

REGISTRY_register_with_reset(dummy4, TOP.CLUSTER.DUMMY, 0, 1)

extern "C" void cvm_registry_reset2() {
    built = false;
    shutdown = false;

    EXPECT_EQ(built, false);
    EXPECT_EQ(shutdown, false);

    cvm::registry::build_domain(1);
    EXPECT_EQ(built, true);
    cvm::registry::shutdown_domain(1);
    EXPECT_EQ(shutdown, true);
}

TEST(Registry, Dummy) {
    EXPECT_EQ(cvm::registry::is_registered<dummy>(), true);
}
