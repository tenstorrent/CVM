// SPDX-FileCopyrightText: © 2026 Tenstorrent USA, Inc.
// SPDX-License-Identifier: Apache-2.0

#include "cvm/evcd.hpp"

#include <gtest/gtest.h>

#include <sstream>
#include <string>

#include "vpi_user.h"

// The parser logs through cvm::log, which links plusargs, which needs a VPI
// host. Stub it so this stays a plain cc_test. See test/logger/logger_test.cpp.
extern "C" PLI_INT32
vpi_get_vlog_info(p_vpi_vlog_info vlog_info_p) {
  static const char* argv[] = {"evcd_test"};
  vlog_info_p->argc = 1;
  vlog_info_p->argv = (PLI_BYTE8**)argv;
  return 1;
}

using cvm::evcd::drive;
using cvm::evcd::parser;
using cvm::evcd::step;

namespace {

// Every state character in IEEE 1364-2005 subclause 18.4.3.1.
constexpr const char* kInputChars = "DUNZdu";
constexpr const char* kOutputChars = "LHXTlh";
constexpr const char* kUnknownDirChars = "01?FAaBbCcf";

std::string
header(const std::string& vars) {
  return "$timescale 1ps $end\n$scope module tb.dut $end\n" + vars +
         "$upscope $end\n$enddefinitions $end\n";
}

} // namespace

// --- state table ---

TEST(EvcdState, EveryStandardCharacterDecodes) {
  for (const char* s : {kInputChars, kOutputChars, kUnknownDirChars}) {
    for (const char* c = s; *c != '\0'; ++c) {
      EXPECT_TRUE(cvm::evcd::decode_state(*c).has_value())
          << "state character `" << *c << "` should decode";
    }
  }
}

TEST(EvcdState, InputCharsDriveOnlyTheExternalSide) {
  for (const char* c = kInputChars; *c != '\0'; ++c) {
    const auto st = cvm::evcd::decode_state(*c);
    ASSERT_TRUE(st.has_value());
    EXPECT_NE(st->ext, drive::none) << *c;
    EXPECT_EQ(st->dut, drive::none) << *c;
  }
}

TEST(EvcdState, OutputCharsDriveOnlyTheDutSide) {
  for (const char* c = kOutputChars; *c != '\0'; ++c) {
    const auto st = cvm::evcd::decode_state(*c);
    ASSERT_TRUE(st.has_value());
    EXPECT_EQ(st->ext, drive::none) << *c;
    EXPECT_NE(st->dut, drive::none) << *c;
  }
}

TEST(EvcdState, UnknownDirectionCharsDriveBothSides) {
  for (const char* c = kUnknownDirChars; *c != '\0'; ++c) {
    const auto st = cvm::evcd::decode_state(*c);
    ASSERT_TRUE(st.has_value());
    EXPECT_NE(st->ext, drive::none) << *c;
    EXPECT_NE(st->dut, drive::none) << *c;
  }
}

TEST(EvcdState, ConflictCharactersCarryBothValues) {
  // Spot-check the asymmetric pairs, which are the ones easiest to transcribe
  // backwards: A is input 0 / output 1, B is input 1 / output 0.
  EXPECT_EQ(cvm::evcd::decode_state('A')->ext, drive::zero);
  EXPECT_EQ(cvm::evcd::decode_state('A')->dut, drive::one);
  EXPECT_EQ(cvm::evcd::decode_state('B')->ext, drive::one);
  EXPECT_EQ(cvm::evcd::decode_state('B')->dut, drive::zero);
  EXPECT_EQ(cvm::evcd::decode_state('C')->ext, drive::unknown);
  EXPECT_EQ(cvm::evcd::decode_state('C')->dut, drive::zero);
  EXPECT_EQ(cvm::evcd::decode_state('c')->ext, drive::unknown);
  EXPECT_EQ(cvm::evcd::decode_state('c')->dut, drive::one);
}

TEST(EvcdState, RejectsCharactersOutsideTheStandardSet) {
  for (const char c : {'G', 'g', 'Y', 'w', '2', '9', '-', '\0'}) {
    EXPECT_FALSE(cvm::evcd::decode_state(c).has_value()) << c;
  }
}

// --- parser ---

TEST(EvcdParser, ReadsPortDeclarations) {
  std::istringstream in(
      header("$var port 1 <0 clk $end\n$var port [7:0] <1 a $end\n"));
  parser p;
  ASSERT_TRUE(p.open(in)) << p.error();
  ASSERT_EQ(p.ports().size(), 2u);
  EXPECT_EQ(p.ports()[0].name, "clk");
  EXPECT_EQ(p.ports()[0].width, 1u);
  EXPECT_EQ(p.ports()[1].name, "a");
  EXPECT_EQ(p.ports()[1].width, 8u);
}

TEST(EvcdParser, RejectsMoreThanOneScope) {
  std::istringstream in(
      "$scope module a $end\n$var port 1 <0 clk $end\n$upscope $end\n"
      "$scope module b $end\n$upscope $end\n$enddefinitions $end\n");
  parser p;
  EXPECT_FALSE(p.open(in));
  EXPECT_NE(p.error().find("$scope"), std::string::npos) << p.error();
}

TEST(EvcdParser, RejectsTruncatedHeader) {
  std::istringstream in("$timescale 1ps $end\n$var port 1 <0 clk $end\n");
  parser p;
  EXPECT_FALSE(p.open(in));
  EXPECT_NE(p.error().find("$enddefinitions"), std::string::npos) << p.error();
}

TEST(EvcdParser, YieldsChangesGroupedByTimestamp) {
  std::istringstream in(
      header("$var port 1 <0 a $end\n$var port 1 <1 b $end\n") +
      "#0\n$dumpports\npD 6 0 <0\npD 6 0 <1\n$end\n"
      "#10\npU 0 6 <0\n");
  parser p;
  ASSERT_TRUE(p.open(in)) << p.error();

  step s;
  ASSERT_TRUE(p.next(s)) << p.error();
  EXPECT_EQ(s.time, 0u);
  EXPECT_EQ(s.changes.size(), 2u);

  ASSERT_TRUE(p.next(s)) << p.error();
  EXPECT_EQ(s.time, 10u);
  ASSERT_EQ(s.changes.size(), 1u);
  EXPECT_EQ(s.changes[0].port, 0u);
  EXPECT_EQ(s.changes[0].state, "U");

  EXPECT_FALSE(p.next(s));
}

TEST(EvcdParser, PublishesTheFinalTimestamp) {
  // Regression: the end-of-stream path once dropped the timestamp, so the last
  // vector silently carried the previous one's time.
  std::istringstream in(header("$var port 1 <0 a $end\n") +
                        "#0\npD 6 0 <0\n#99\npU 0 6 <0\n");
  parser p;
  ASSERT_TRUE(p.open(in)) << p.error();

  step s;
  ASSERT_TRUE(p.next(s));
  ASSERT_TRUE(p.next(s));
  EXPECT_EQ(s.time, 99u);
}

TEST(EvcdParser, VectorPortsKeepOneCharacterPerBit) {
  std::istringstream in(header("$var port [7:0] <0 a $end\n") +
                        "#0\npDDDDDDUU 6 0 <0\n");
  parser p;
  ASSERT_TRUE(p.open(in)) << p.error();

  step s;
  ASSERT_TRUE(p.next(s)) << p.error();
  ASSERT_EQ(s.changes.size(), 1u);
  EXPECT_EQ(s.changes[0].state, "DDDDDDUU");
}

TEST(EvcdParser, RejectsWrongStateRunLength) {
  std::istringstream in(header("$var port [7:0] <0 a $end\n") +
                        "#0\npDD 6 0 <0\n");
  parser p;
  ASSERT_TRUE(p.open(in)) << p.error();

  step s;
  EXPECT_FALSE(p.next(s));
  EXPECT_NE(p.error().find("state character"), std::string::npos) << p.error();
}

TEST(EvcdParser, RejectsUnknownStateCharacter) {
  std::istringstream in(header("$var port 1 <0 a $end\n") + "#0\npG 6 0 <0\n");
  parser p;
  ASSERT_TRUE(p.open(in)) << p.error();

  step s;
  EXPECT_FALSE(p.next(s));
  EXPECT_NE(p.error().find("18.4.3.1"), std::string::npos) << p.error();
}

TEST(EvcdParser, DumpPortsOffDeclaresEveryPortUnknownAndStopsRecording) {
  std::istringstream in(header("$var port 1 <0 a $end\n") +
                        "#0\npD 6 0 <0\n#10\n$dumpportsoff\n"
                        "#20\npU 0 6 <0\n");
  parser p;
  ASSERT_TRUE(p.open(in)) << p.error();

  step s;
  ASSERT_TRUE(p.next(s));
  EXPECT_FALSE(s.all_ports_unknown);

  ASSERT_TRUE(p.next(s));
  EXPECT_EQ(s.time, 10u);
  EXPECT_TRUE(s.all_ports_unknown);

  ASSERT_TRUE(p.next(s));
  EXPECT_EQ(s.time, 20u);
  EXPECT_TRUE(s.changes.empty()) << "dumping is suspended until $dumpportson";
}

TEST(EvcdParser, StrengthDigitsDoNotAffectTheDecodedState) {
  std::istringstream a(header("$var port 1 <0 a $end\n") + "#0\npD 6 0 <0\n");
  std::istringstream b(header("$var port 1 <0 a $end\n") + "#0\npD 1 7 <0\n");
  parser pa;
  parser pb;
  ASSERT_TRUE(pa.open(a));
  ASSERT_TRUE(pb.open(b));

  step sa;
  step sb;
  ASSERT_TRUE(pa.next(sa));
  ASSERT_TRUE(pb.next(sb));
  EXPECT_EQ(sa.changes[0].state, sb.changes[0].state);
}
