// SPDX-FileCopyrightText: © 2026 Tenstorrent USA, Inc.
// SPDX-License-Identifier: Apache-2.0

#include "cvm/replay_encoder.hpp"
#include "cvm/replay_source.hpp"

#include <gtest/gtest.h>

#include <sstream>
#include <tuple>
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

using cvm::replay::cycle_element;
using cvm::replay::source;

namespace {

  std::string
  header(const std::string& vars) {
    return "$timescale 1ps $end\n$scope module tb.dut $end\n" + vars +
           "$upscope $end\n$enddefinitions $end\n";
  }

  // Opens and binds the way the generated interposer does: read the header,
  // then declare each port.
  bool
  open_bound(source& s, std::istream& in,
             const std::vector<std::tuple<std::string, std::size_t, bool,
                                          std::size_t>>& ports) {
    if (!s.open(in))
      return false;
    for (const auto& [name, width, is_output, offset] : ports) {
      if (s.bind(name, width, is_output, offset) < 0)
        return false;
    }
    return true;
  }

  bool bit_set(const std::vector<std::uint32_t>& words, std::size_t bit) {
    return (words[bit / 32] & (1u << (bit % 32))) != 0;
  }

} // namespace

// --- conformance, which open() performs while binding ---

TEST(Conformance, AcceptsAMatchingPort) {
  std::istringstream in(header("$var port 1 <0 clk $end\n"));
  source s;
  EXPECT_TRUE(open_bound(s, in, {{"clk", 1, false, 0}})) << s.error();
}

TEST(Conformance, RejectsMissingPort) {
  std::istringstream in(header("$var port 1 <0 clk $end\n"));
  source s;
  EXPECT_FALSE(open_bound(s, in, {{"rst_n", 1, false, 0}}));
  EXPECT_NE(s.error().find("rst_n"), std::string::npos) << s.error();
}

TEST(Conformance, RejectsWidthMismatch) {
  std::istringstream in(header("$var port [3:0] <0 a $end\n"));
  source s;
  EXPECT_FALSE(open_bound(s, in, {{"a", 8, false, 0}}));
  EXPECT_NE(s.error().find("width mismatch"), std::string::npos) << s.error();
}

TEST(Conformance, LayoutOrderDecidesOffsetsNotDumpOrder) {
  std::istringstream in(
      header("$var port 1 <0 b $end\n$var port 1 <1 a $end\n") +
      "#0\npU 6 0 <0\npD 6 0 <1\n");
  source s;
  ASSERT_TRUE(open_bound(s, in, {{"a", 1, false, 0}, {"b", 1, false, 1}}))
      << s.error();

  cycle_element e;
  ASSERT_TRUE(s.next_cycle(e)) << s.error();
  EXPECT_FALSE(bit_set(e.in, 0)) << "a is bit 0 however the dump ordered it";
  EXPECT_TRUE(bit_set(e.in, 1));
}

TEST(Conformance, AnUnboundDumpPortIsFatal) {
  // The mirror of the missing-port check. A spec that has drifted from the DUT
  // would otherwise replay the ports it still lists and say nothing about the
  // rest, which reads exactly like a pass.
  std::istringstream in(
      header("$var port 1 <0 clk $end\n$var port 1 <1 spare $end\n") +
      "#0\npD 6 0 <0\npD 6 0 <1\n");
  source s;
  ASSERT_TRUE(open_bound(s, in, {{"clk", 1, false, 0}})) << s.error();
  EXPECT_FALSE(s.require_all_bound({}));
  EXPECT_NE(s.error().find("`spare`"), std::string::npos) << s.error();
}

TEST(Conformance, AnExemptDumpPortNeedNotBeBound) {
  // How the clock passes: it is in every recording and must never be replayed.
  std::istringstream in(
      header("$var port 1 <0 clk $end\n$var port 1 <1 a $end\n") +
      "#0\npD 6 0 <0\npD 6 0 <1\n");
  source s;
  ASSERT_TRUE(open_bound(s, in, {{"a", 1, false, 1}})) << s.error();
  EXPECT_TRUE(s.require_all_bound({"clk"})) << s.error();
}

TEST(Conformance, DirectionContradictionIsAnError) {
  // `y` is bound as an input but only the DUT ever drives it.
  std::istringstream in(header("$var port 1 <0 y $end\n") + "#0\npH 0 6 <0\n");
  source s;
  ASSERT_TRUE(open_bound(s, in, {{"y", 1, false, 0}})) << s.error();

  cycle_element e;
  EXPECT_FALSE(s.next_cycle(e));
  EXPECT_NE(s.error().find("declared `in`"), std::string::npos) << s.error();
}

// --- flattening ---

TEST(Flatten, TakesFixtureSideForInputsAndDutSideForOutputs) {
  std::istringstream in(
      header("$var port 1 <0 a $end\n$var port 1 <1 y $end\n") +
      "#0\npD 6 0 <0\npH 0 6 <1\n");
  source s;
  ASSERT_TRUE(open_bound(s, in, {{"a", 1, false, 0}, {"y", 1, true, 1}}))
      << s.error();

  cycle_element e;
  ASSERT_TRUE(s.next_cycle(e)) << s.error();
  EXPECT_FALSE(bit_set(e.in, 0)); // D: fixture drives low
  EXPECT_TRUE(bit_set(e.exp, 1)); // H: DUT drives high
}

TEST(Flatten, VectorBitsLandMsbFirst) {
  std::istringstream in(header("$var port [7:0] <0 a $end\n") +
                        "#0\npDDDDDDUU 6 0 <0\n");
  source s;
  ASSERT_TRUE(open_bound(s, in, {{"a", 8, false, 0}})) << s.error();

  cycle_element e;
  ASSERT_TRUE(s.next_cycle(e)) << s.error();
  EXPECT_TRUE(bit_set(e.in, 0)); // DDDDDDUU is 0b00000011
  EXPECT_TRUE(bit_set(e.in, 1));
  for (std::size_t b = 2; b < 8; ++b) {
    EXPECT_FALSE(bit_set(e.in, b)) << "bit " << b;
  }
}

TEST(Flatten, HonoursBitOffset) {
  std::istringstream in(header("$var port [3:0] <0 a $end\n") +
                        "#0\npDDDU 6 0 <0\n");
  source s;
  ASSERT_TRUE(open_bound(s, in, {{"a", 4, false, 8}})) << s.error();

  cycle_element e;
  ASSERT_TRUE(s.next_cycle(e)) << s.error();
  EXPECT_TRUE(bit_set(e.in, 8));
  EXPECT_FALSE(bit_set(e.in, 9));
}

TEST(Flatten, UnchangedValuesCarryForward) {
  std::istringstream in(
      header("$var port 1 <0 a $end\n$var port 1 <1 b $end\n") +
      "#0\npU 6 0 <0\npU 6 0 <1\n#10\npD 6 0 <0\n");
  source s;
  ASSERT_TRUE(open_bound(s, in, {{"a", 1, false, 0}, {"b", 1, false, 1}}))
      << s.error();

  cycle_element e;
  ASSERT_TRUE(s.next_cycle(e)) << s.error();
  ASSERT_TRUE(bit_set(e.in, 1));

  ASSERT_TRUE(s.next_cycle(e)) << s.error();
  EXPECT_EQ(e.cycle, 10u);
  EXPECT_FALSE(bit_set(e.in, 0));
  EXPECT_TRUE(bit_set(e.in, 1)) << "b was not rewritten, so it carries forward";
}

TEST(Flatten, DumpPortsOffDrivesEveryPortUnknown) {
  // Regression: routing this through an 'X' state character gave Z on inputs,
  // because X is output-class and every character also picks a side. Recorded
  // as 1, so the resolved 0 is distinguishable from it.
  std::istringstream in(header("$var port 1 <0 a $end\n") +
                        "#0\npU 6 0 <0\n#10\n$dumpportsoff\n");
  source s;
  ASSERT_TRUE(open_bound(s, in, {{"a", 1, false, 0}})) << s.error();

  cycle_element e;
  ASSERT_TRUE(s.next_cycle(e)) << s.error();
  EXPECT_TRUE(bit_set(e.in, 0));

  ASSERT_TRUE(s.next_cycle(e)) << s.error();
  EXPECT_EQ(e.cycle, 10u);
  EXPECT_FALSE(bit_set(e.in, 0)) << "unknown, so resolved to 0";
}

// --- cycle encoding, which is what the engine consumes ---

namespace {

  // Two inputs and one output, so the split by direction is visible.
  std::string
  two_in_one_out() {
    return header("$var port 1 <0 a $end\n"
                  "$var port 1 <1 b $end\n"
                  "$var port 1 <2 y $end\n");
  }

  const std::vector<std::tuple<std::string, std::size_t, bool, std::size_t>>&
  abc_ports() {
    static const std::vector<
        std::tuple<std::string, std::size_t, bool, std::size_t>>
        p{{"a", 1, false, 0}, {"b", 1, false, 1}, {"y", 1, true, 2}};
    return p;
  }

} // namespace

TEST(Encode, SplitsByDirectionAndKeepsTheRecordedTimestampAsTheCycle) {
  std::istringstream in(two_in_one_out() +
                        "#0\npU 6 0 <0\npD 6 0 <1\npH 0 6 <2\n#7\npD 6 0 <0\n");
  source s;
  ASSERT_TRUE(open_bound(s, in, abc_ports())) << s.error();

  cycle_element e;
  ASSERT_TRUE(s.next_cycle(e));
  EXPECT_EQ(e.cycle, 0u);
  EXPECT_TRUE(bit_set(e.in, 0));
  EXPECT_FALSE(bit_set(e.in, 1));
  EXPECT_FALSE(bit_set(e.in, 2)) << "an output must not appear in the stimulus";
  EXPECT_TRUE(bit_set(e.exp, 2));
  EXPECT_TRUE(bit_set(e.care, 2));
  EXPECT_FALSE(bit_set(e.care, 0)) << "inputs are driven, never compared";

  ASSERT_TRUE(s.next_cycle(e));
  // One `#1` is one clock, so a timestamp is already a cycle index.
  EXPECT_EQ(e.cycle, 7u);
}

TEST(Encode, ResolvesUnknownInputBitsToZero) {
  // An emulator has no X, so this is settled on the host rather than left to
  // whatever a simulator collapses it to.
  std::istringstream in(two_in_one_out() + "#0\npN 6 0 <0\n");
  source s;
  ASSERT_TRUE(open_bound(s, in, {{"a", 1, false, 0}})) << s.error();

  cycle_element e;
  ASSERT_TRUE(s.next_cycle(e));
  EXPECT_FALSE(bit_set(e.in, 0));
}

TEST(Encode, UnknownOutputBitsAreNotCompared) {
  std::istringstream in(two_in_one_out() + "#0\npX 0 6 <2\n#1\npH 0 6 <2\n");
  source s;
  ASSERT_TRUE(open_bound(s, in, {{"y", 1, true, 2}})) << s.error();

  cycle_element e;
  ASSERT_TRUE(s.next_cycle(e));
  EXPECT_FALSE(bit_set(e.care, 2)) << "nothing to compare an X against";

  ASSERT_TRUE(s.next_cycle(e));
  EXPECT_TRUE(bit_set(e.care, 2));
}

TEST(Encode, CareSurvivesCyclesThatDoNotRewriteTheOutput) {
  // The engine holds an expectation until the next element, so what may be
  // compared has to persist with it. Keying care off "written at this
  // timestamp" would drop checking on every cycle that changed only an input,
  // and a dropped check reads exactly like a pass.
  std::istringstream in(two_in_one_out() +
                        "#0\npD 6 0 <0\npH 0 6 <2\n#1\npU 6 0 <0\n");
  source s;
  ASSERT_TRUE(open_bound(s, in, {{"a", 1, false, 0}, {"y", 1, true, 2}}))
      << s.error();

  cycle_element e;
  ASSERT_TRUE(s.next_cycle(e));
  ASSERT_TRUE(bit_set(e.care, 2));

  ASSERT_TRUE(s.next_cycle(e));
  EXPECT_EQ(e.cycle, 1u);
  EXPECT_TRUE(bit_set(e.exp, 2)) << "unchanged values carry forward";
  EXPECT_TRUE(bit_set(e.care, 2));
}

TEST(Encode, ReportsFailingBitsByPortName) {
  std::istringstream in(two_in_one_out() + "#0\npH 0 6 <2\n");
  source s;
  ASSERT_TRUE(open_bound(s, in, {{"y", 1, true, 2}})) << s.error();

  // The engine reports bit indices because it has no idea what a port is.
  const std::vector<std::string> names = s.failing_bits({0b100u});
  ASSERT_EQ(names.size(), 1u);
  EXPECT_EQ(names[0], "y[0]");
}

// --- encoding, which is what the transport carries ---

TEST(Encoder, PacksTheCycleAsTwoWordsLowFirst) {
  // A timestamp above 2^32. This is the only way to reach the cycle's high
  // word: matching one in the design would take four billion cycles.
  std::istringstream in(header("$var port 1 <0 a $end\n") +
                        "#4294967303\npU 6 0 <0\n");
  source s;
  ASSERT_TRUE(open_bound(s, in, {{"a", 1, false, 0}})) << s.error();

  cvm::replay::encoder enc;
  enc.start(s, 2 + 3 * 1);
  std::vector<std::uint32_t> w(5, 0xA5A5A5A5u);
  ASSERT_EQ(enc.fill(w.data(), 1), 1u);

  EXPECT_EQ(w[0], 7u) << "low half of 2^32 + 7";
  EXPECT_EQ(w[1], 1u) << "high half";
  EXPECT_EQ(w[2], 1u) << "stimulus";
  EXPECT_EQ(w[3], 0u) << "expectation";
  EXPECT_EQ(w[4], 0u) << "care";
}

// The transport hands the producer a buffer it does not clear, so a word left
// unwritten would deliver whatever the previous element had there.
TEST(Encoder, WritesEveryWordOfEveryElement) {
  std::istringstream in(header("$var port 1 <0 a $end\n") +
                        "#0\npD 6 0 <0\n#1\npU 6 0 <0\n");
  source s;
  ASSERT_TRUE(open_bound(s, in, {{"a", 1, false, 0}})) << s.error();

  cvm::replay::encoder enc;
  enc.start(s, 5);
  std::vector<std::uint32_t> w(10, 0xA5A5A5A5u);
  ASSERT_EQ(enc.fill(w.data(), 2), 2u);
  for (std::size_t i = 0; i < w.size(); ++i)
    EXPECT_NE(w[i], 0xA5A5A5A5u) << "word " << i;
}

TEST(Encoder, CyclesThatChangeNothingCostNoElement) {
  // The second timestamp rewrites `a` with the value it already had, which is
  // what a dump does when something outside the spec moved.
  std::istringstream in(header("$var port 1 <0 a $end\n") +
                        "#0\npU 6 0 <0\n#1\npU 6 0 <0\n#2\npD 6 0 <0\n");
  source s;
  ASSERT_TRUE(open_bound(s, in, {{"a", 1, false, 0}})) << s.error();

  cvm::replay::encoder enc;
  enc.start(s, 5);
  std::vector<std::uint32_t> w(15, 0u);
  ASSERT_EQ(enc.fill(w.data(), 3), 2u);
  EXPECT_EQ(w[0], 0u) << "cycle 0";
  EXPECT_EQ(w[5], 2u) << "cycle 1 changed nothing, so cycle 2 follows it";
}

TEST(Encoder, FinishedOnceTheRecordingIsSpent) {
  std::istringstream in(header("$var port 1 <0 a $end\n") +
                        "#0\npU 6 0 <0\n");
  source s;
  ASSERT_TRUE(open_bound(s, in, {{"a", 1, false, 0}})) << s.error();

  cvm::replay::encoder enc;
  enc.start(s, 5);
  std::vector<std::uint32_t> w(10, 0u);
  EXPECT_FALSE(enc.finished());
  EXPECT_EQ(enc.fill(w.data(), 2), 1u);
  EXPECT_TRUE(enc.finished());
  EXPECT_EQ(enc.fill(w.data(), 2), 0u);
}
