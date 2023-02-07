#include "Vplusargs_test.h"
#include "verilated.h"
#include "cvm/plusargs.hpp"
#include <gtest/gtest.h>
#include <iostream>
#include <fstream>

DEFINE_bool(testsetbool, false, "Test set bool");
DEFINE_bool(testclrbool, true, "Test clear bool");
DEFINE_string(teststring, "", "Test string");
DEFINE_int32(testflag1infile, 1, "Test int in file");
DEFINE_int32(testflag2infile, 2, "Test int in file");
DEFINE_int32(testflag3infile, 3, "Test int in file");

TEST(Plusargs, Plusargs) {

    {
        std::ofstream flagfile("flagfile");
        flagfile << "--testflag1infile=41\n";
    }

    const char* argv[] = {
        "./plusargs_test",
        "+testsetbool",
        "+notestclrbool",
        "+teststring=test",
        "+flagfile=./flagfile",
    };

    VerilatedContext ctx;
    ctx.commandArgs(sizeof(argv)/sizeof(argv[0]), argv);
    cvm::plusargs::parse();

    EXPECT_EQ(FLAGS_testsetbool, true);
    EXPECT_EQ(FLAGS_testclrbool, false);
    EXPECT_EQ(FLAGS_teststring, "test");
    EXPECT_EQ(FLAGS_testflag1infile, 41);
    EXPECT_EQ(FLAGS_testflag2infile,  2);
    EXPECT_EQ(FLAGS_testflag3infile,  3);

    {
        std::ofstream flagfile("flagfile");
        flagfile << "--testflag2infile=42\n";
    }

    cvm::plusargs::parse();
    // This is undesirable, but the way it is
    // If a plusarg was previously specified it won't revert to its default
    EXPECT_EQ(FLAGS_testflag1infile, 41);
    EXPECT_EQ(FLAGS_testflag2infile, 42);
    EXPECT_EQ(FLAGS_testflag3infile,  3);
}
