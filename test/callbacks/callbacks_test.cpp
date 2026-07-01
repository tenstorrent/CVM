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
