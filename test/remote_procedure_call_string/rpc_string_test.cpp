#include <gtest/gtest.h>

#include "cvm/rpc_string.hpp"

// add two numbers and return the result

cvm::rpc rpc1;

int remote1(int a, int b) {
    return a + b;
}

TEST(RemoteProcedureCall, Add) {
    rpc1.connect<int, int, int>(1, "remote1", [] (int a, int b) {return a + b;});
    int response = rpc1.signal<int, int, int>(1, "remote1", 1, 2);

    printf("response: %d\n", response);
    EXPECT_EQ(response, 3);
}