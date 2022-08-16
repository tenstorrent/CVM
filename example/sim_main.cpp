#include "Vtop.h"
#include "verilated.h"
#include <stdio.h>

int main(int argc, char** argv, char** env) {
    for (int i = 0; i < argc; i++) {
      printf("argv[%d]: %s\n", i, argv[i]);
    }

    Vtop top;
    Verilated::commandArgs(argc, argv);
    top.clk = 1;
    while (!Verilated::gotFinish()) {
        top.eval();
        top.clk = !top.clk;
    }
    exit(0);
}

