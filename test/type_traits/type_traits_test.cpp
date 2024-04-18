#include <gtest/gtest.h>
#include "cvm/type_traits.hpp"

TEST(LOGGER, TypeName) {
    int a;
    ASSERT_EQ(cvm::type_traits::name<decltype(a)>(), "int");

    double b;
    ASSERT_EQ(cvm::type_traits::name<decltype(b)>(), "double");
}
