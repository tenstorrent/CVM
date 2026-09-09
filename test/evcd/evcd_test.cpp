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
using cvm::evcd::reader;
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

  // The decoded run a dump of `chars` (MSB first) should produce, LSB first.
  std::vector<cvm::evcd::port_state>
  decoded(const std::string& chars) {
    std::vector<cvm::evcd::port_state> out;
    for (auto c = chars.rbegin(); c != chars.rend(); ++c) {
      out.push_back(*cvm::evcd::decode_state(*c));
    }
    return out;
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
    EXPECT_NE(st->dut_in(), drive::none) << *c;
    EXPECT_EQ(st->dut_out(), drive::none) << *c;
  }
}

TEST(EvcdState, OutputCharsDriveOnlyTheDutSide) {
  for (const char* c = kOutputChars; *c != '\0'; ++c) {
    const auto st = cvm::evcd::decode_state(*c);
    ASSERT_TRUE(st.has_value());
    EXPECT_EQ(st->dut_in(), drive::none) << *c;
    EXPECT_NE(st->dut_out(), drive::none) << *c;
  }
}

TEST(EvcdState, UnknownDirectionCharsDriveBothSides) {
  for (const char* c = kUnknownDirChars; *c != '\0'; ++c) {
    const auto st = cvm::evcd::decode_state(*c);
    ASSERT_TRUE(st.has_value());
    EXPECT_NE(st->dut_in(), drive::none) << *c;
    EXPECT_NE(st->dut_out(), drive::none) << *c;
  }
}

TEST(EvcdState, ConflictCharactersCarryBothValues) {
  // Spot-check the asymmetric pairs, which are the ones easiest to transcribe
  // backwards: A is input 0 / output 1, B is input 1 / output 0.
  EXPECT_EQ(cvm::evcd::decode_state('A')->dut_in(), drive::zero);
  EXPECT_EQ(cvm::evcd::decode_state('A')->dut_out(), drive::one);
  EXPECT_EQ(cvm::evcd::decode_state('B')->dut_in(), drive::one);
  EXPECT_EQ(cvm::evcd::decode_state('B')->dut_out(), drive::zero);
  EXPECT_EQ(cvm::evcd::decode_state('C')->dut_in(), drive::unknown);
  EXPECT_EQ(cvm::evcd::decode_state('C')->dut_out(), drive::zero);
  EXPECT_EQ(cvm::evcd::decode_state('c')->dut_in(), drive::unknown);
  EXPECT_EQ(cvm::evcd::decode_state('c')->dut_out(), drive::one);
}

TEST(EvcdState, RejectsCharactersOutsideTheStandardSet) {
  for (const char c : {'G', 'g', 'Y', 'w', '2', '9', '-', '\0'}) {
    EXPECT_FALSE(cvm::evcd::decode_state(c).has_value()) << c;
  }
}

// --- parser ---

TEST(EvcdReader, ReadsPortDeclarations) {
  std::istringstream in(
      header("$var port 1 <0 clk $end\n$var port [7:0] <1 a $end\n"));
  reader r(in, "t.evcd");
  ASSERT_TRUE(r.ok()) << r.error();
  ASSERT_EQ(r.ports().size(), 2u);
  EXPECT_EQ(r.ports()[0].name, "clk");
  EXPECT_EQ(r.ports()[0].width, 1u);
  EXPECT_EQ(r.ports()[1].name, "a");
  EXPECT_EQ(r.ports()[1].width, 8u);
}

TEST(EvcdReader, RejectsMoreThanOneScope) {
  std::istringstream in(
      "$scope module a $end\n$var port 1 <0 clk $end\n$upscope $end\n"
      "$scope module b $end\n$upscope $end\n$enddefinitions $end\n");
  reader r(in, "t.evcd");
  EXPECT_FALSE(r.ok());
  EXPECT_NE(r.error().find("$scope"), std::string::npos) << r.error();
}

TEST(EvcdReader, RejectsTruncatedHeader) {
  std::istringstream in("$timescale 1ps $end\n$var port 1 <0 clk $end\n");
  reader r(in, "t.evcd");
  EXPECT_FALSE(r.ok());
  EXPECT_NE(r.error().find("$enddefinitions"), std::string::npos) << r.error();
}

TEST(EvcdReader, YieldsChangesGroupedByTimestamp) {
  std::istringstream in(
      header("$var port 1 <0 a $end\n$var port 1 <1 b $end\n") +
      "#0\n$dumpports\npD 6 0 <0\npD 6 0 <1\n$end\n"
      "#10\npU 0 6 <0\n");
  reader r(in, "t.evcd");
  ASSERT_TRUE(r.ok()) << r.error();

  step s;
  ASSERT_TRUE(r.next(s)) << r.error();
  EXPECT_EQ(s.time, 0u);
  EXPECT_EQ(s.changes.size(), 2u);

  ASSERT_TRUE(r.next(s)) << r.error();
  EXPECT_EQ(s.time, 10u);
  ASSERT_EQ(s.changes.size(), 1u);
  EXPECT_EQ(s.changes[0].port, 0u);
  EXPECT_EQ(s.changes[0].state, decoded("U"));

  EXPECT_FALSE(r.next(s));
}

TEST(EvcdReader, PublishesTheFinalTimestamp) {
  // Regression: the end-of-stream path once dropped the timestamp, so the last
  // vector silently carried the previous one's time.
  std::istringstream in(header("$var port 1 <0 a $end\n") +
                        "#0\npD 6 0 <0\n#99\npU 0 6 <0\n");
  reader r(in, "t.evcd");
  ASSERT_TRUE(r.ok()) << r.error();

  step s;
  ASSERT_TRUE(r.next(s));
  ASSERT_TRUE(r.next(s));
  EXPECT_EQ(s.time, 99u);
}

TEST(EvcdReader, VectorPortsKeepOneCharacterPerBit) {
  std::istringstream in(header("$var port [7:0] <0 a $end\n") +
                        "#0\npDDDDDDUU 6 0 <0\n");
  reader r(in, "t.evcd");
  ASSERT_TRUE(r.ok()) << r.error();

  step s;
  ASSERT_TRUE(r.next(s)) << r.error();
  ASSERT_EQ(s.changes.size(), 1u);
  // MSB first in the dump, LSB first once decoded.
  EXPECT_EQ(s.changes[0].state, decoded("DDDDDDUU"));
  EXPECT_EQ(s.changes[0].state[0].dut_in(), drive::one) << "bit 0 is the last character";
  EXPECT_EQ(s.changes[0].state[7].dut_in(), drive::zero);
}

TEST(EvcdReader, RejectsWrongStateRunLength) {
  std::istringstream in(header("$var port [7:0] <0 a $end\n") +
                        "#0\npDD 6 0 <0\n");
  reader r(in, "t.evcd");
  ASSERT_TRUE(r.ok()) << r.error();

  step s;
  EXPECT_FALSE(r.next(s));
  EXPECT_NE(r.error().find("state character"), std::string::npos) << r.error();
}

TEST(EvcdReader, RejectsUnknownStateCharacter) {
  std::istringstream in(header("$var port 1 <0 a $end\n") + "#0\npG 6 0 <0\n");
  reader r(in, "t.evcd");
  ASSERT_TRUE(r.ok()) << r.error();

  step s;
  EXPECT_FALSE(r.next(s));
  EXPECT_NE(r.error().find("18.4.3.1"), std::string::npos) << r.error();
}

TEST(EvcdReader, DumpPortsOffDeclaresEveryPortUnknownAndStopsRecording) {
  std::istringstream in(header("$var port 1 <0 a $end\n") +
                        "#0\npD 6 0 <0\n#10\n$dumpportsoff\n"
                        "#20\npU 0 6 <0\n");
  reader r(in, "t.evcd");
  ASSERT_TRUE(r.ok()) << r.error();

  step s;
  ASSERT_TRUE(r.next(s));
  EXPECT_FALSE(s.all_ports_unknown);

  ASSERT_TRUE(r.next(s));
  EXPECT_EQ(s.time, 10u);
  EXPECT_TRUE(s.all_ports_unknown);

  ASSERT_TRUE(r.next(s));
  EXPECT_EQ(s.time, 20u);
  EXPECT_TRUE(s.changes.empty()) << "dumping is suspended until $dumpportson";
}

TEST(EvcdReader, NextOnAFailedReaderKeepsTheHeaderDiagnostic) {
  std::istringstream in("$var port 1 <0 a $end\n");
  reader r(in, "t.evcd");
  ASSERT_FALSE(r.ok());
  const std::string first = r.error();

  step s;
  EXPECT_FALSE(r.next(s));
  EXPECT_EQ(r.error(), first) << "next() must not overwrite why the header failed";
}

TEST(EvcdReader, IdentifierCodesMayBeArbitraryStrings) {
  // IEEE gives EVCD `<N`, but writers emit other codes, and nothing here needs
  // them to be numbers or to be dense.
  std::istringstream in(header("$var port 1 <0 a $end\n"
                               "$var port [3:0] !#$ b $end\n") +
                        "#0\npD 6 0 <0\npDDDU 6 0 !#$\n");
  reader r(in, "t.evcd");
  ASSERT_TRUE(r.ok()) << r.error();
  ASSERT_EQ(r.ports().size(), 2u);
  EXPECT_EQ(r.ports()[1].name, "b");
  EXPECT_EQ(r.ports()[1].width, 4u);

  step s;
  ASSERT_TRUE(r.next(s)) << r.error();
  ASSERT_EQ(s.changes.size(), 2u);
  // Ports are indexed in declaration order, not by the identifier's value.
  EXPECT_EQ(s.changes[1].port, 1u);
  EXPECT_EQ(s.changes[1].state, decoded("DDDU"));
}

TEST(EvcdReader, DuplicateIdentifierCodeIsAnError) {
  std::istringstream in(header("$var port 1 <0 a $end\n"
                               "$var port 1 <0 b $end\n"));
  reader r(in, "t.evcd");
  EXPECT_FALSE(r.ok());
  EXPECT_NE(r.error().find("declared twice"), std::string::npos) << r.error();
}

TEST(EvcdReader, ParseFailureNamesTheFileAndLine) {
  // Without the location a bad state character says nothing about where it is.
  std::istringstream in(header("$var port 1 <0 a $end\n") +
                        "#0\npD 6 0 <0\n#10\npQ 6 0 <0\n");
  reader r(in, "alu.evcd");
  ASSERT_TRUE(r.ok()) << r.error();

  step s;
  ASSERT_TRUE(r.next(s)) << r.error();
  EXPECT_FALSE(r.next(s));
  EXPECT_EQ(r.error().rfind("alu.evcd:", 0), 0u) << r.error();
  // Five header lines, then #0, its change, #10, and the bad change.
  EXPECT_NE(r.error().find(":9:"), std::string::npos) << r.error();
  EXPECT_NE(r.error().find("`Q`"), std::string::npos) << r.error();
}

TEST(EvcdReader, StrengthDigitsDoNotAffectTheDecodedState) {
  std::istringstream a(header("$var port 1 <0 a $end\n") + "#0\npD 6 0 <0\n");
  std::istringstream b(header("$var port 1 <0 a $end\n") + "#0\npD 1 7 <0\n");
  reader ra(a, "a.evcd");
  reader rb(b, "b.evcd");
  ASSERT_TRUE(ra.ok());
  ASSERT_TRUE(rb.ok());

  step sa;
  step sb;
  ASSERT_TRUE(ra.next(sa));
  ASSERT_TRUE(rb.next(sb));
  EXPECT_EQ(sa.changes[0].state, sb.changes[0].state);
}
