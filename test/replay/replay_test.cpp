// SPDX-FileCopyrightText: © 2026 Tenstorrent USA, Inc.
// SPDX-License-Identifier: Apache-2.0

#include "cvm/replay.hpp"

#include <gtest/gtest.h>

#include <sstream>
#include <string>

#include "vpi_user.h"

// The replay layer logs through cvm::log, which links plusargs, which needs a
// VPI host. Stub it so this stays a plain cc_test.
extern "C" PLI_INT32
vpi_get_vlog_info(p_vpi_vlog_info vlog_info_p) {
  static const char* argv[] = {"replay_test"};
  vlog_info_p->argc = 1;
  vlog_info_p->argv = (PLI_BYTE8**)argv;
  return 1;
}

using cvm::replay::logic_word;
using cvm::replay::replay_vector;
using cvm::replay::source;

namespace {

constexpr bool kInput = false;
constexpr bool kOutput = true;

std::string
header(const std::string& vars) {
  return "$timescale 1ps $end\n$scope module tb.dut $end\n" + vars +
         "$upscope $end\n$enddefinitions $end\n";
}

// The 4-state character a bit of the flattened vector holds.
char bit_at(const replay_vector& v, std::size_t bit) {
  const logic_word& w = v.value[bit / 32];
  const std::uint32_t mask = 1u << (bit % 32);
  const bool a = (w.aval & mask) != 0;
  const bool b = (w.bval & mask) != 0;
  if (!b)
    return a ? '1' : '0';
  return a ? 'x' : 'z';
}

} // namespace

// --- conformance, which bind() performs ---

TEST(Bind, AcceptsAMatchingPort) {
  std::istringstream in(header("$var port 1 <0 clk $end\n"));
  source s;
  ASSERT_TRUE(s.open(in, "")) << s.error();
  EXPECT_EQ(s.bind("clk", 1, kInput, 0), 0);
}

TEST(Bind, RejectsMissingPort) {
  std::istringstream in(header("$var port 1 <0 clk $end\n"));
  source s;
  ASSERT_TRUE(s.open(in, "")) << s.error();
  EXPECT_EQ(s.bind("rst_n", 1, kInput, 1), -1);
  EXPECT_NE(s.error().find("rst_n"), std::string::npos) << s.error();
}

TEST(Bind, RejectsWidthMismatch) {
  std::istringstream in(header("$var port [3:0] <0 a $end\n"));
  source s;
  ASSERT_TRUE(s.open(in, "")) << s.error();
  EXPECT_EQ(s.bind("a", 8, kInput, 0), -1);
  EXPECT_NE(s.error().find("width mismatch"), std::string::npos) << s.error();
}

TEST(Bind, ReturnsIndicesInBindOrderNotDumpOrder) {
  std::istringstream in(
      header("$var port 1 <0 b $end\n$var port 1 <1 a $end\n"));
  source s;
  ASSERT_TRUE(s.open(in, "")) << s.error();
  EXPECT_EQ(s.bind("a", 1, kInput, 0), 0);
  EXPECT_EQ(s.bind("b", 1, kInput, 1), 1);
}

TEST(Bind, UnboundDumpPortsAreIgnored) {
  std::istringstream in(
      header("$var port 1 <0 clk $end\n$var port 1 <1 spare $end\n") +
      "#0\npD 6 0 <0\npD 6 0 <1\n");
  source s;
  ASSERT_TRUE(s.open(in, "")) << s.error();
  ASSERT_EQ(s.bind("clk", 1, kInput, 0), 0);

  replay_vector v;
  ASSERT_TRUE(s.next(v)) << s.error();
  EXPECT_EQ(v.recorded.size(), 1u);
}

TEST(Bind, PortMayBeNamedDifferentlyInTheRecording) {
  std::istringstream in(header("$var port 1 <0 dbg_bus $end\n"));
  source s;
  ASSERT_TRUE(s.open(in, "")) << s.error();
  EXPECT_EQ(s.bind("dbg_bus", 1, kInput, 0), 0);
}

TEST(Bind, DirectionContradictionIsAnError) {
  // `y` is bound as an input but only the DUT ever drives it.
  std::istringstream in(header("$var port 1 <0 y $end\n") + "#0\npH 0 6 <0\n");
  source s;
  ASSERT_TRUE(s.open(in, "")) << s.error();
  ASSERT_EQ(s.bind("y", 1, kInput, 0), 0);

  replay_vector v;
  EXPECT_FALSE(s.next(v));
  EXPECT_NE(s.error().find("declared `in`"), std::string::npos) << s.error();
}

// --- flattening ---

TEST(Flatten, TakesFixtureSideForInputsAndDutSideForOutputs) {
  std::istringstream in(
      header("$var port 1 <0 a $end\n$var port 1 <1 y $end\n") +
      "#0\npD 6 0 <0\npH 0 6 <1\n");
  source s;
  ASSERT_TRUE(s.open(in, "")) << s.error();
  ASSERT_EQ(s.bind("a", 1, kInput, 0), 0);
  ASSERT_EQ(s.bind("y", 1, kOutput, 1), 1);

  replay_vector v;
  ASSERT_TRUE(s.next(v)) << s.error();
  EXPECT_EQ(bit_at(v, 0), '0'); // D: fixture drives low
  EXPECT_EQ(bit_at(v, 1), '1'); // H: DUT drives high
}

TEST(Flatten, UnknownAndThreeStateSurviveAsFourState) {
  std::istringstream in(
      header("$var port 1 <0 a $end\n$var port 1 <1 b $end\n") +
      "#0\npN 6 6 <0\npZ 6 6 <1\n");
  source s;
  ASSERT_TRUE(s.open(in, "")) << s.error();
  ASSERT_EQ(s.bind("a", 1, kInput, 0), 0);
  ASSERT_EQ(s.bind("b", 1, kInput, 1), 1);

  replay_vector v;
  ASSERT_TRUE(s.next(v)) << s.error();
  EXPECT_EQ(bit_at(v, 0), 'x');
  EXPECT_EQ(bit_at(v, 1), 'z');
}

TEST(Flatten, VectorBitsLandMsbFirst) {
  std::istringstream in(header("$var port [7:0] <0 a $end\n") +
                        "#0\npDDDDDDUU 6 0 <0\n");
  source s;
  ASSERT_TRUE(s.open(in, "")) << s.error();
  ASSERT_EQ(s.bind("a", 8, kInput, 0), 0);

  replay_vector v;
  ASSERT_TRUE(s.next(v)) << s.error();
  EXPECT_EQ(bit_at(v, 0), '1'); // DDDDDDUU is 0b00000011
  EXPECT_EQ(bit_at(v, 1), '1');
  for (std::size_t b = 2; b < 8; ++b) {
    EXPECT_EQ(bit_at(v, b), '0') << "bit " << b;
  }
}

TEST(Flatten, HonoursBitOffset) {
  std::istringstream in(header("$var port [3:0] <0 a $end\n") +
                        "#0\npDDDU 6 0 <0\n");
  source s;
  ASSERT_TRUE(s.open(in, "")) << s.error();
  ASSERT_EQ(s.bind("a", 4, kInput, 8), 0);

  replay_vector v;
  ASSERT_TRUE(s.next(v)) << s.error();
  EXPECT_EQ(bit_at(v, 8), '1');
  EXPECT_EQ(bit_at(v, 9), '0');
}

TEST(Flatten, RecordedMarksOnlyPortsWrittenAtThisTimestamp) {
  std::istringstream in(
      header("$var port 1 <0 a $end\n$var port 1 <1 b $end\n") +
      "#0\npD 6 0 <0\npD 6 0 <1\n#10\npU 0 6 <0\n");
  source s;
  ASSERT_TRUE(s.open(in, "")) << s.error();
  ASSERT_EQ(s.bind("a", 1, kInput, 0), 0);
  ASSERT_EQ(s.bind("b", 1, kInput, 1), 1);

  replay_vector v;
  ASSERT_TRUE(s.next(v)) << s.error();
  EXPECT_TRUE(v.recorded[0]);
  EXPECT_TRUE(v.recorded[1]);

  ASSERT_TRUE(s.next(v)) << s.error();
  EXPECT_EQ(v.time, 10u);
  EXPECT_TRUE(v.recorded[0]);
  EXPECT_FALSE(v.recorded[1]);
  EXPECT_EQ(bit_at(v, 1), '0') << "unchanged values carry forward";
}

TEST(Flatten, DumpPortsOffDrivesEveryPortUnknown) {
  // Regression: routing this through an 'X' state character gave Z on inputs,
  // because X is output-class and every character also picks a side.
  std::istringstream in(header("$var port 1 <0 a $end\n") +
                        "#0\npD 6 0 <0\n#10\n$dumpportsoff\n");
  source s;
  ASSERT_TRUE(s.open(in, "")) << s.error();
  ASSERT_EQ(s.bind("a", 1, kInput, 0), 0);

  replay_vector v;
  ASSERT_TRUE(s.next(v));
  EXPECT_EQ(bit_at(v, 0), '0');

  ASSERT_TRUE(s.next(v));
  EXPECT_EQ(v.time, 10u);
  EXPECT_EQ(bit_at(v, 0), 'x');
}

// --- plusarg path resolution ---

TEST(Path, BarePathAppliesToEveryKey) {
  const auto p = cvm::replay::resolve_path("dump.evcd", "tb.u_replay");
  ASSERT_TRUE(p.has_value());
  EXPECT_EQ(*p, "dump.evcd");
}

TEST(Path, KeyedListSelectsByKey) {
  const std::string v = "tb.a=a.evcd,tb.b=b.evcd";
  EXPECT_EQ(*cvm::replay::resolve_path(v, "tb.a"), "a.evcd");
  EXPECT_EQ(*cvm::replay::resolve_path(v, "tb.b"), "b.evcd");
}

TEST(Path, KeyedListMissingKeyIsAbsent) {
  EXPECT_FALSE(cvm::replay::resolve_path("tb.a=a.evcd", "tb.c").has_value());
}

TEST(Path, EmptyFlagIsAbsent) {
  EXPECT_FALSE(cvm::replay::resolve_path("", "tb.a").has_value());
}
