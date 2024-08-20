#include <gtest/gtest.h>

#include "cvm/rpc_types.hpp"

cvm::rpc rpc1;

using Add = cvm::rpc_function<int (int, int)>;

int remote1(int a, int b) {
    return a + b;
}

// TEST(RemoteProcedureCall, Add) {
//     std::cout << "meow" << std::endl;
//     cvm::log(cvm::DEBUG, "meow meow\n");
//     rpc1.connect<Add>(1, remote1);
//     auto ret = rpc1.signal<Add>(1, 2, 3);

//     EXPECT_EQ(ret, 5);
// }

int main(void) {
    rpc1.connect<Add>(1, remote1);
    auto ret = rpc1.signal<Add>(1, 2, 3);

    std::cout << "ret " << ret << std::endl;
}