#include <gtest/gtest.h>

#include "cvm/rpc_string.hpp"
#include "cvm/messenger.hpp"

// add two numbers and return the result

cvm::rpc rpc1;

using Add = cvm::pack<int(int, int)>;
// using Add = cvm::function<int(int, int)>;

int remote1(int a, int b) {
    return a + b;
}

TEST(RemoteProcedureCall, Add) {
    rpc1.connect<Add>(1, "remote1", &remote1); // rpc1.connect<cvm::pack<int(int, int)>>
    int response = rpc1.signal<Add>(1, "remote1", 1, 2);

    // printf("response: %d\n", response);
    // EXPECT_EQ(response, 3);
}