// SPDX-FileCopyrightText: © 2026 Tenstorrent USA, Inc.
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>
#include "cvm/callbacks.hpp"

svScope make_fake_scope(uintptr_t v) {
    return reinterpret_cast<svScope>(v);
}

TEST(Callbacks, SetScopeThenPushByLocDispatches) {
    cvm::callbacks c;
    int counter = 0;
    cvm::topology::loc_t loc = 42;
    c.set_scope(loc, make_fake_scope(0xdead));
    c.push(loc, [&counter]() { counter++; });
    c.flush();
    EXPECT_EQ(counter, 1);
}

TEST(Callbacks, PushBySvScopeDispatches) {
    cvm::callbacks c;
    int counter = 0;
    c.push(make_fake_scope(0x1), [&counter]() { counter++; });
    c.flush();
    EXPECT_EQ(counter, 1);
}

TEST(Callbacks, MultipleLocsDispatchIndependently) {
    cvm::callbacks c;
    int a_count = 0, b_count = 0;
    c.set_scope(10, make_fake_scope(0xa));
    c.set_scope(20, make_fake_scope(0xb));
    c.push(10, [&a_count]() { a_count++; });
    c.push(20, [&b_count]() { b_count++; });
    c.push(10, [&a_count]() { a_count++; });
    c.flush();
    EXPECT_EQ(a_count, 2);
    EXPECT_EQ(b_count, 1);
}

TEST(Callbacks, CallByLocRunsImmediatelyWithRegisteredScope) {
    cvm::callbacks c;
    int counter = 0;
    cvm::topology::loc_t loc = 5;
    svScope scope = make_fake_scope(0xbeef);
    c.set_scope(loc, scope);
    c.call(loc, [&]() {
        counter++;
        EXPECT_EQ(svGetScope(), scope);
    });
    EXPECT_EQ(counter, 1);
}

TEST(Callbacks, CallRestoresPreviousScope) {
    cvm::callbacks c;
    cvm::topology::loc_t loc = 6;
    c.set_scope(loc, make_fake_scope(0xbeef));
    svScope prev = make_fake_scope(0x111);
    svSetScope(prev);
    c.call(loc, []() {});
    EXPECT_EQ(svGetScope(), prev);
}

TEST(Callbacks, PushByLocDispatchesWithRegisteredScope) {
    cvm::callbacks c;
    cvm::topology::loc_t loc = 7;
    svScope scope = make_fake_scope(0xabc);
    c.set_scope(loc, scope);
    int counter = 0;
    c.push(loc, [&]() {
        counter++;
        EXPECT_EQ(svGetScope(), scope);
    });
    c.flush();
    EXPECT_EQ(counter, 1);
}

TEST(Callbacks, PushBeforeSetScopeDispatchesAtFlush) {
    cvm::callbacks c;
    cvm::topology::loc_t loc = 8;
    int counter = 0;
    c.push(loc, [&counter]() { counter++; });
    c.set_scope(loc, make_fake_scope(0xdef));
    c.flush();
    EXPECT_EQ(counter, 1);
}
