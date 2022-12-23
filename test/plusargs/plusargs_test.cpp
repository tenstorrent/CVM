#include "Vplusargs_test.h"
#include "verilated.h"
#include "cvm/plusargs.hpp"
#include <gtest/gtest.h>
#include "tools/cpp/runfiles/runfiles.h"

DEFINE_bool(testsetbool, false, "Test set bool");
DEFINE_bool(testclrbool, true, "Test clear bool");
DEFINE_string(teststring, "", "Test string");
DEFINE_int32(testflaginfile, 0, "Test int in file");

TEST(Plusargs, Plusargs) {

    using bazel::tools::cpp::runfiles::Runfiles;

    auto runfiles = Runfiles::CreateForTest();
    std::string flagfile = runfiles->Rlocation("cvm/test/plusargs/plusargs_test_flagfile");
    std::string flagfileplusarg = std::string("+flagfile=") + flagfile;


    const char* argv[] = {
        "./plusargs_test",
        "+testsetbool",
        "+notestclrbool",
        "+teststring=test",
        flagfileplusarg.c_str(),
    };

    Verilated::commandArgs(sizeof(argv)/sizeof(argv[0]), argv);
    cvm::plusargs::parse();

    EXPECT_EQ(FLAGS_testsetbool, true);
    EXPECT_EQ(FLAGS_testclrbool, false);
    EXPECT_EQ(FLAGS_teststring, "test");
    EXPECT_EQ(FLAGS_testflaginfile, 42);

    Vplusargs_test top;
    while (!Verilated::gotFinish()) {
        top.eval();
    }
}
