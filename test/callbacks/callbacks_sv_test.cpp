#include "Vtop.h"
#include "verilated.h"
#include "cvm/callbacks.hpp"
#include "cvm/registry.hpp"
#include "cvm/topology.hpp"
#include <gtest/gtest.h>

extern "C" {

    void increment();

    void cvm_callbacks_run_test() {
        cvm::topology::loc_t loc = 7;
        cvm::registry::callbacks.push(loc, []() { increment(); });
        cvm::registry::callbacks.push(loc, []() { increment(); });
        cvm::registry::callbacks.push(loc, []() { increment(); });
        cvm::registry::callbacks.flush();
    }

}

TEST(Callbacks, SvRoundTrip) {
    Vtop top;
    while (!Verilated::gotFinish()) top.eval();
}
