#include <gtest/gtest.h>
#include <iostream>
#include "Vtop.h"
#include "verilated.h"

TEST(Registry, Main) {

    Vtop top;
    while (!Verilated::gotFinish()) {
        top.eval();
    }

}
