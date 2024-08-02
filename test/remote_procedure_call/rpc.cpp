#include <gtest/gtest.h>

#include "cvm/rpc.hpp"

// Add two numbers and return them

struct transaction1 {
    int a;
    int b;
};

cvm::rpc rpc1;

int remote1 (const transaction1& t) {
    return t.a + t.b;
}

TEST(RemoteProcedureCall, Add) {
    rpc1.connect<transaction1, int>(1, remote1);
    int response = rpc1.signal<transaction1, int>(1, transaction1{1, 2});

    EXPECT_EQ(response, 3);

}

// struct as response

struct transaction2 {
    int a; 
    int b;
};

struct transaction2_resp : transaction2 {
    int c;
};

cvm::rpc rpc2;

transaction2_resp remote2 (const transaction2& t) {
    auto resp = (transaction2_resp) t;
    resp.c = t.a + t.b;

    return resp;
}

TEST(RemoteProcedureCall, StructResp) {
    rpc2.connect<transaction2, transaction2_resp>(2, remote2);
    auto resp = rpc2.signal<transaction2, transaction2_resp>(2, transaction2{3, 4});

    EXPECT_EQ(resp.c, resp.a + resp.b);
}

// Two functions and two structs

struct transaction3_mult {
    int a;
    int b;
};

struct transaction3_div {
    int a;
    int b;
};

cvm::rpc rpc3;

int remote3_mult (const transaction3_mult& t) {
    return t.a * t.b;
}

int remote3_div (const transaction3_div& t) {
    return t.a / t.b;
}

TEST(RemoteProcedureCall, TwoFunctions) {
    rpc3.connect<transaction3_mult, int>(3, remote3_mult);
    rpc3.connect<transaction3_div, int>(3, remote3_div);
    auto resp1 = rpc3.signal<transaction3_mult, int>(3, transaction3_mult{5,6});
    auto resp2 = rpc3.signal<transaction3_div, int>(3, transaction3_div{20, 5});

    EXPECT_EQ(resp1, 5*6);
    EXPECT_EQ(resp2, 20/5);
}

// two locations with same datatype
struct transaction4 {
    int a;
    int b;
};

cvm::rpc rpc4;

int remote4_add(const transaction4& t) {
    return t.a + t.b;
}

int remote4_sub(const transaction4& t) {
    return t.a - t.b;
}

TEST(RemoteProcedureCall, TwoLocations) {
    rpc4.connect<transaction4, int>(41, remote4_add);
    rpc4.connect<transaction4, int>(42, remote4_sub);
    auto resp1 = rpc4.signal<transaction4, int>(41, transaction4{3, 4});
    auto resp2 = rpc4.signal<transaction4, int>(42, transaction4{10, 7});

    EXPECT_EQ(resp1, 3+4);
    EXPECT_EQ(resp2, 10-7);
}


// test 3: Register two functions and cause an assertion

// test 4: Call signal with no connect

