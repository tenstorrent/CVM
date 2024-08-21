#include <gtest/gtest.h>

#include "cvm/messenger.hpp"

cvm::messenger m1;

// add two numbers and return them

CVM_MESSENGER_procedure_call(Add1, int (int, int));

int remote1(int a, int b) {
    return a + b;
}

TEST(RemoteProcedureCall, Add) {
    m1.procedure<Add1>(1, remote1);
    auto ret = m1.call<Add1>(1, 2, 3);

    EXPECT_EQ(ret, 5);
}

// two different functions with same underlying type

cvm::messenger m2;

CVM_MESSENGER_procedure_call(Add2, int (int, int));
CVM_MESSENGER_procedure_call(Sub2, int (int, int));

TEST(RemoteProcedureCall, TwoSameListeners) {
    m2.procedure<Add2>(2, [](int a, int b) {return a + b;});
    m2.procedure<Sub2>(2, [](int a, int b) {return a - b;});

    EXPECT_EQ(m2.call<Add2>(2, 7, 8), 7 + 8);
    EXPECT_EQ(m2.call<Sub2>(2, 11, 7), 11 - 7);
}

// two locations with same Type

cvm::messenger m3;

CVM_MESSENGER_procedure_call(Ints3, int (int, int));

TEST(RemoteProcedureCall, TwoLocations) {
    m3.procedure<Ints3>(31, [](int a, int b) {return a + b;});
    m3.procedure<Ints3>(32, [](int a, int b) {return a * b;});

    EXPECT_EQ(m3.call<Ints3>(31, 7, 9), 7 + 9);
    EXPECT_EQ(m3.call<Ints3>(32, 11, 13), 11 * 13);
}

// two different functions with different types

cvm::messenger m4;

CVM_MESSENGER_procedure_call(Add4, int (int, int, int));
CVM_MESSENGER_procedure_call(Sub4, int (int, int));

TEST(RemoteProcedureCall, TwoDifferentListeners) {
    m4.procedure<Add4>(4, [](int a, int b, int c) {return a + b + c;});
    m4.procedure<Sub4>(4, [](int a, int b) {return a - b;});

    EXPECT_EQ(m4.call<Add4>(4, 7, 8, 9), 7 + 8 + 9);
    EXPECT_EQ(m4.call<Sub4>(4, 11, 7), 11 - 7);
}
