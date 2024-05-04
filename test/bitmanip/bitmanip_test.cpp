#include <gtest/gtest.h>
#include "cvm/bitmanip.hpp"
#include <bitset>

template<typename T>
void setbit(T* a, int bit) {
    a[bit / (8*sizeof(T))] |= 1 << (bit % (8*sizeof(T)));
}

TEST(Bitmanip, ArraySlice) {

    for (int start = 0; start < 64; start++) {
        for (int width = 1; width <= 64; width++) {
            std::uint32_t arr[4] = {0,};

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

            std::uint32_t arr[(128+64)/32] = {0,};

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

TEST(Bitmanip, ArraySliceArray) {

     for (int start = 0; start < 64; start++) {
        for (int width = 64; width <= 128; width++) {
            typedef std::array<uint32_t, (128+64)/32> T;

            std::uint32_t arr[(128+64)/32] = {0,};

            int lsb = start;
            int msb = start + width - 1;

            setbit(arr, lsb);
            setbit(arr, msb);

            T expected{};
            setbit(expected.data(), 0);
            setbit(expected.data(), width - 1);

            T actual = cvm::bitmanip::array_slice<T, 32>(arr, msb, lsb);
            EXPECT_EQ(actual, expected);

            actual = cvm::bitmanip::array_slice<T, 16>(arr, msb, lsb);

            T expected2{};
            setbit(expected2.data(), 0);
            setbit(expected2.data(), (width - 1) + 16*((width - 1)/16));
            EXPECT_EQ(actual, expected2);
        }
    }

}

TEST(Bitmanip, VectorByteFundamentalSlice) {

    for (int start = 0; start < 64; start++) {
        for (int width = 1; width <= 64; width++) {
            std::uint8_t arr[16] = {0,};

            int lsb = start;
            int msb = start + width - 1;

            setbit(arr, lsb);
            setbit(arr, msb);

            std::uint64_t sliced   = cvm::bitmanip::array_slice<std::uint64_t>(arr, msb, lsb);
            auto sliced_vector = cvm::bitmanip::slice<decltype(sliced), std::vector<uint8_t>>(sliced);

            uint64_t actual = 0;
            for (size_t i = 0; i < sliced_vector.size(); i++)
                actual |= uint64_t(sliced_vector[i]) << i*8;

            std::uint64_t expected = std::uint64_t(1) << (width - 1) | 1;

            EXPECT_EQ(actual, expected);
        }
    }

}

TEST(Bitmanip, VectorByteBitsetSlice) {

    for (int start = 0; start < 64; start++) {
        for (int width = 64; width <= 128; width++) {
            typedef std::bitset<128+64> T;

            std::uint8_t arr[(128+64)/8] = {0,};

            int lsb = start;
            int msb = start + width - 1;

            setbit(arr, lsb);
            setbit(arr, msb);

            T sliced   = cvm::bitmanip::array_slice<T>(arr, msb, lsb);
            auto sliced_vector = cvm::bitmanip::slice<decltype(sliced), std::vector<uint8_t>>(sliced);

            T actual = 0;
            for (size_t i = 0; i < sliced_vector.size(); i++)
                actual |= T(sliced_vector[i]) << i*8;

            T expected = T(1) << (width - 1) | T(1);
            EXPECT_EQ(actual, expected);
        }
    }

}

TEST(Bitmanip, VectorBoolFundamentalSlice) {

    for (int start = 0; start < 64; start++) {
        for (int width = 1; width <= 64; width++) {
            std::uint8_t arr[16] = {0,};

            int lsb = start;
            int msb = start + width - 1;

            setbit(arr, lsb);
            setbit(arr, msb);

            std::uint64_t sliced   = cvm::bitmanip::array_slice<std::uint64_t>(arr, msb, lsb);
            auto sliced_vector = cvm::bitmanip::slice<decltype(sliced), std::vector<bool>>(sliced);

            uint64_t actual = 0;
            for (size_t i = 0; i < sliced_vector.size(); i++)
                actual |= uint64_t(sliced_vector[i]) << i;

            std::uint64_t expected = std::uint64_t(1) << (width - 1) | 1;

            EXPECT_EQ(actual, expected);
        }
    }

}

TEST(Bitmanip, VectorBoolBitsetSlice) {

    for (int start = 0; start < 64; start++) {
        for (int width = 64; width <= 128; width++) {
            typedef std::bitset<128+64> T;

            std::uint8_t arr[(128+64)/8] = {0,};

            int lsb = start;
            int msb = start + width - 1;

            setbit(arr, lsb);
            setbit(arr, msb);

            T sliced   = cvm::bitmanip::array_slice<T>(arr, msb, lsb);
            auto sliced_vector = cvm::bitmanip::slice<decltype(sliced), std::vector<bool>>(sliced);

            T actual = 0;
            for (size_t i = 0; i < sliced_vector.size(); i++)
                actual |= T(sliced_vector[i]) << i;

            T expected = T(1) << (width - 1) | T(1);
            EXPECT_EQ(actual, expected);
        }
    }

}
