// SPDX-FileCopyrightText: © 2026 Tenstorrent USA, Inc.
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>

#include "cvm/topology.hpp"
#include "svdpi.h"
#include "vpi_user.h"

// Host-only: the pipe links the registry, which links the simulator interface
// this test never uses.
extern "C" PLI_INT32
vpi_get_vlog_info(p_vpi_vlog_info vlog_info_p) {
  static const char* argv[] = {"pipe_test"};
  vlog_info_p->argc = 1;
  vlog_info_p->argv = (PLI_BYTE8**)argv;
  return 1;
}
extern "C" svScope svGetScope() { return nullptr; }
extern "C" svScope svSetScope(const svScope) { return nullptr; }

using cvm::topology::resolve_keyed;

namespace {

  cvm::topology::loc_t pipe_loc() {
    return cvm::topology::get_from_hierarchy("TOP.PIPE", 0);
  }

} // namespace

TEST(ResolveKeyed, TheTopologyUnderTestHasOnePipe) {
  // Everything below leans on this, so it is worth failing here rather than
  // as a confusing absence later.
  ASSERT_NE(pipe_loc(), cvm::topology::null);
  EXPECT_EQ(cvm::topology::get_from_hierarchy("TOP.PIPE").size(), 1u);
}

TEST(ResolveKeyed, BareValueAppliesToEveryInstance) {
  const auto v = resolve_keyed("1024", pipe_loc());
  ASSERT_TRUE(v.has_value());
  EXPECT_EQ(*v, "1024");
}

TEST(ResolveKeyed, PathSelectsTheInstance) {
  EXPECT_EQ(*resolve_keyed("TOP.PIPE=64", pipe_loc()), "64");
}

TEST(ResolveKeyed, LaterPairsAreReachable) {
  const std::string f = "TOP.NOPE=32,TOP.PIPE=128";
  EXPECT_EQ(*resolve_keyed(f, pipe_loc()), "128");
}

TEST(ResolveKeyed, UnknownPathIsAbsentSoTheCallerCanDefault) {
  EXPECT_FALSE(resolve_keyed("TOP.NOPE=64", pipe_loc()).has_value());
}

TEST(ResolveKeyed, EmptyFlagIsAbsent) {
  EXPECT_FALSE(resolve_keyed("", pipe_loc()).has_value());
}
