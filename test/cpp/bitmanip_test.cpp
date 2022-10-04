#include <gtest/gtest.h>
#include "cvm/bitmanip.hpp" 
#include <bitset>

void setbit(std::uint8_t* a, int bit) {
    a[bit / 8] |= 1 << (bit % 8);
}

TEST(Bitmanip, ArraySlice) {

    for (int start = 0; start < 64; start++) {
        for (int width = 1; width <= 64; width++) {
            std::uint8_t arr[16] = {0,};

            int lsb = start;
            int msb = start + width - 1;

            setbit(arr, lsb);
            setbit(arr, msb);

            std::uint64_t actual   = cvm::bitmanip::array_slice<std::uint64_t>(arr, msb, lsb);
            std::uint64_t expected = std::uint64_t(1) << (width - 1) | 1;

            EXPECT_EQ(actual, expected);
        }
    }

}

TEST(Bitmanip, ArraySliceBitset) {

    for (int start = 0; start < 64; start++) {
        for (int width = 64; width <= 128; width++) {
            typedef std::bitset<128+64> T;

            std::uint8_t arr[(128+64)/8] = {0,};

            int lsb = start;
            int msb = start + width - 1;

            setbit(arr, lsb);
            setbit(arr, msb);

            T actual   = cvm::bitmanip::array_slice<T>(arr, msb, lsb);
            T expected = T(1) << (width - 1) | T(1);

            EXPECT_EQ(actual, expected);
        }
    }

}
