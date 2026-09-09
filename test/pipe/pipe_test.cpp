// SPDX-FileCopyrightText: © 2026 Tenstorrent USA, Inc.
// SPDX-License-Identifier: Apache-2.0

#include "cvm/pipe.hpp"

#include <gtest/gtest.h>

#include <string>

using cvm::pipe::in_stream;
using cvm::pipe::resolve_keyed;

// --- keyed plusarg values ---
//
// cvm_plusargs cannot key flag *names*, so one flag carries either a bare value
// or name=value pairs. This is the same shape +cvm_replay_file uses.

TEST(ResolveKeyed, BareValueAppliesToEveryKey) {
  const auto v = resolve_keyed("1024", "tb.u_pipe");
  ASSERT_TRUE(v.has_value());
  EXPECT_EQ(*v, "1024");
}

TEST(ResolveKeyed, PairsSelectByName) {
  const std::string f = "tb.a=64,tb.b=128";
  EXPECT_EQ(*resolve_keyed(f, "tb.a"), "64");
  EXPECT_EQ(*resolve_keyed(f, "tb.b"), "128");
}

TEST(ResolveKeyed, MissingNameIsAbsentSoTheCallerCanDefault) {
  EXPECT_FALSE(resolve_keyed("tb.a=64", "tb.c").has_value());
}

TEST(ResolveKeyed, EmptyFlagIsAbsent) {
  EXPECT_FALSE(resolve_keyed("", "tb.a").has_value());
}

TEST(ResolveKeyed, AValueContainingNoPairsButAnEqualsIsNotMistakenForBare) {
  // `=64` has no name, so no key matches it; it must not resolve as a bare
  // value, or a typo would silently apply everywhere.
  EXPECT_FALSE(resolve_keyed("=64", "tb.a").has_value());
}

// --- host-side queueing ---

TEST(InStream, QueuesElementsForLaterCollection) {
  in_stream s("test.queue", 2);
  EXPECT_EQ(s.pending(), 0u);

  const std::uint32_t a[2] = {1, 2};
  const std::uint32_t b[2] = {3, 4};
  s.push(a);
  s.push(b);
  EXPECT_EQ(s.pending(), 2u) << "pending counts elements, not words";
}

TEST(InStream, StreamsAreIndependentByName) {
  in_stream a("test.a", 1);
  in_stream b("test.b", 1);
  const std::uint32_t w = 7;
  a.push(&w);
  EXPECT_EQ(a.pending(), 1u);
  EXPECT_EQ(b.pending(), 0u);
}

TEST(InStream, CloseDoesNotDiscardQueuedElements) {
  // close() marks the final epoch; dropping what is queued would lose the tail.
  in_stream s("test.close", 1);
  const std::uint32_t w = 1;
  s.push(&w);
  s.close();
  EXPECT_EQ(s.pending(), 1u);
}
