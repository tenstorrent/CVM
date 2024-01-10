#include <gtest/gtest.h>
#include "Vtop.h"
#include "verilated.h"

TEST(Transactions, Checker) {

    Vtop top;
    top.clk = 1;
    while (!Verilated::gotFinish()) {
        top.eval();
        top.clk = !top.clk;
    }

}

