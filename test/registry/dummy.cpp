#include <gtest/gtest.h>
#include <unordered_set>
#include "cvm/topology.hpp"
#include "cvm/registry.hpp"

unsigned dummy_loc = 0;

bool built = false;
bool shutdown = false;

class dummy {
  public:
    dummy(cvm::topology::loc_t loc, unsigned id) {
      dummy_loc = loc;
      built = true;
      EXPECT_EQ(id, 0);
    };

    ~dummy() {
      shutdown = true;
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

extern std::unordered_set<cvm::topology::loc_t> locations;
extern std::unordered_set<unsigned> ids;

extern "C" void cvm_registry_reset2() {
    built = false;
    shutdown = false;

    // These also need to be reset (from dummy2).
    locations.clear();
    ids.clear();

    auto avoid = cvm::topology::get_from_hierarchy("TOP.CLUSTER.DUMMY", 0);
    cvm::registry::build_all_except(std::unordered_set<cvm::topology::loc_t>{avoid});
    EXPECT_EQ(built, false);
    cvm::registry::shutdown_all_except(std::unordered_set<cvm::topology::loc_t>{avoid});
    EXPECT_EQ(shutdown, false);
}

TEST(Registry, Dummy) {
    EXPECT_EQ(cvm::registry::is_registered<dummy>(), true);
}
