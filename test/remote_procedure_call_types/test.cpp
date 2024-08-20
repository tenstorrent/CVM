#include <gtest/gtest.h>

#include "cvm/rpc_types.hpp"

cvm::rpc rpc1;

// add two numbers and return them

MakeRPC(Add1, int (int, int));

int remote1(int a, int b) {
    return a + b;
}

TEST(RemoteProcedureCall, Add) {
    rpc1.connect<Add1>(1, remote1);
    auto ret = rpc1.signal<Add1>(1, 2, 3);

    EXPECT_EQ(ret, 5);
}

// two different functions with same underlying type

cvm::rpc rpc2;

MakeRPC(Add2, int (int, int));
MakeRPC(Sub2, int (int, int));

TEST(RemoteProcedureCall, TwoSameListeners) {
    rpc2.connect<Add2>(2, [](int a, int b) {return a + b;});
    rpc2.connect<Sub2>(2, [](int a, int b) {return a - b;});

    EXPECT_EQ(rpc2.signal<Add2>(2, 7, 8), 7 + 8);
    EXPECT_EQ(rpc2.signal<Sub2>(2, 11, 7), 11 - 7);
}

// two locations with same Type

cvm::rpc rpc3;

MakeRPC(Ints3, int (int, int));

TEST(RemoteProcedureCall, TwoLocations) {
    rpc3.connect<Ints3>(31, [](int a, int b) {return a + b;});
    rpc3.connect<Ints3>(32, [](int a, int b) {return a * b;});

    EXPECT_EQ(rpc3.signal<Ints3>(31, 7, 9), 7 + 9);
    EXPECT_EQ(rpc3.signal<Ints3>(32, 11, 13), 11 * 13);
}

// two different functions with different types

cvm::rpc rpc4;

MakeRPC(Add4, int (int, int, int));
MakeRPC(Sub4, int (int, int));

TEST(RemoteProcedureCall, TwoDifferentListeners) {
    rpc2.connect<Add4>(4, [](int a, int b, int c) {return a + b + c;});
    rpc2.connect<Sub4>(4, [](int a, int b) {return a - b;});

    EXPECT_EQ(rpc2.signal<Add4>(4, 7, 8, 9), 7 + 8 + 9);
    EXPECT_EQ(rpc2.signal<Sub4>(4, 11, 7), 11 - 7);
}
