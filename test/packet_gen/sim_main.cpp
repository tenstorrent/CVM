// SPDX-FileCopyrightText: © 2026 Tenstorrent USA, Inc.
// SPDX-License-Identifier: Apache-2.0

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
    top.final();
}

