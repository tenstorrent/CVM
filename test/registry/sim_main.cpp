// SPDX-FileCopyrightText: © 2026 Tenstorrent USA, Inc.
// SPDX-License-Identifier: Apache-2.0

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
