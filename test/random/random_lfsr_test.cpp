#include <gtest/gtest.h>
#include "Vtop.h"
#include "verilated.h"


static unsigned lfsr_state;
extern "C" {
  void forward_lfsr_value(unsigned state) {
    lfsr_state = state;
  }
}

TEST(Random, LFSR) {

    Vtop top;
    unsigned ix = 0;
    unsigned sequence[] = {1, 4, 6, 7, 3, 5, 2, 1};

    top.clk = 1;
    top.rst = 1;
    top.eval();
    top.clk = !top.clk;
    top.eval();
    top.clk = !top.clk;
    top.eval();
    top.rst = 0;
    top.clk = !top.clk;
    top.eval();
    top.clk = !top.clk;
    while (true) {
        top.eval();
        top.clk = !top.clk;
        if (not top.rst and top.clk) {
          ASSERT_EQ(lfsr_state, sequence[ix]);
          ix++;
          if (ix == 8)
            break;
        }
    }
}
