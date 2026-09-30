// SPDX-FileCopyrightText: © 2026 Tenstorrent USA, Inc.
// SPDX-License-Identifier: Apache-2.0

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
DEFINE_int64(testint64,     100, "Test int64");
DEFINE_uint32(testuint32,   7,   "Test uint32");
DEFINE_double(testdouble, 1.5, "Test double");

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
        "+undefok_regexp=\\+nonexistent",
        "+undefok_regexp=\\+anothernonexistentflag\\+file",
        "+nonexistentflag",
        "+anothernonexistentflag+file_abcde"
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
    // If a plusarg was previously specified make sure it reverts to its default
    EXPECT_EQ(FLAGS_testflag1infile,  1);
    EXPECT_EQ(FLAGS_testflag2infile, 42);
    EXPECT_EQ(FLAGS_testflag3infile,  3);

    {
        std::ofstream flagfile("flagfile");
        flagfile << "";
    }

    cvm::plusargs::parse();
    // another test to make sure it reverts to its default
    // for some reason the above test was not enough
    EXPECT_EQ(FLAGS_testflag1infile, 1);
    EXPECT_EQ(FLAGS_testflag2infile, 2);
    EXPECT_EQ(FLAGS_testflag3infile, 3);
}

TEST(Plusargs, DpiTypeCheck) {

    EXPECT_EQ(cvm_plusargs_get_bool("testsetbool"), FLAGS_testsetbool);
    EXPECT_EQ(cvm_plusargs_get_int32("testflag1infile"), FLAGS_testflag1infile);
    EXPECT_EQ(cvm_plusargs_get_uint32("testuint32"), FLAGS_testuint32);
    EXPECT_EQ(cvm_plusargs_get_int64("testint64"), FLAGS_testint64);
    EXPECT_EQ(cvm_plusargs_get_double("testdouble"), FLAGS_testdouble);
    EXPECT_STREQ(cvm_plusargs_get_string("teststring"), FLAGS_teststring.c_str());

    EXPECT_DEATH(cvm_plusargs_get_int32("testuint32"), "type mismatch.*defined as uint32 but accessed as int32");
    EXPECT_DEATH(cvm_plusargs_get_uint64("testint64"), "type mismatch.*defined as int64 but accessed as uint64");
    EXPECT_DEATH(cvm_plusargs_get_bool("testflag1infile"), "type mismatch.*defined as int32 but accessed as bool");
    EXPECT_DEATH(cvm_plusargs_get_int32("testint64"), "type mismatch.*defined as int64 but accessed as int32");
    EXPECT_DEATH(cvm_plusargs_get_int64("testsetbool"), "type mismatch.*defined as bool but accessed as int64");
    EXPECT_DEATH(cvm_plusargs_get_string("testflag1infile"), "type mismatch.*defined as int32 but accessed as string");
    EXPECT_DEATH(cvm_plusargs_get_int64("teststring"), "type mismatch.*defined as string but accessed as int64");
    EXPECT_DEATH(cvm_plusargs_get_double("testint64"), "type mismatch.*defined as int64 but accessed as double");
    EXPECT_DEATH(cvm_plusargs_get_int32("nonexistentflag"), "Plusarg not found");
}
