#include <gtest/gtest.h>
#include "cvm/bitmanip.hpp"
#include <bitset>
#include <iostream>

void setbit(std::uint8_t* a, int bit) {
    a[bit / 8] |= 1 << (bit % 8);
}

// https://stackoverflow.com/a/47563100
template<std::size_t N>
struct num { static const constexpr auto value = N; };

template <std::size_t N, typename F>
void for_(F func) {
  for_(func, std::make_index_sequence<N>());
}

template <class F, std::size_t... Is>
void for_(F func, std::index_sequence<Is...>) {
      (func(num<Is>{}), ...);
}


TEST(Bitmanip, ArraySlice) {

    for_<64> ([&](auto s) {
        for_<64> ([&](auto w) {

            constexpr auto start = s.value;
            constexpr auto width = w.value + 1;

            std::uint8_t arr[16] = {0,};

            constexpr int lsb = start;
            constexpr int msb = start + width - 1;

            setbit(arr, lsb);
            setbit(arr, msb);

            std::uint64_t actual   = cvm::bitmanip::array_slice<std::uint64_t, msb, lsb>(arr);
            std::uint64_t expected = std::uint64_t(1) << (width - 1) | 1;

            EXPECT_EQ(actual, expected);
        });
    });

}

TEST(Bitmanip, ArraySliceBitset) {

    for_<64> ([&](auto s) {
        for_<65> ([&](auto w) {
            typedef std::bitset<128+64> T;

            std::uint8_t arr[(128+64)/8] = {0,};

            constexpr int start = s.value;
            constexpr int width = w.value + 64;

            constexpr int lsb = start;
            constexpr int msb = start + width - 1;

            setbit(arr, lsb);
            setbit(arr, msb);

            T actual   = cvm::bitmanip::array_slice<T, msb, lsb>(arr);
            T expected = T(1) << (width - 1) | T(1);

            EXPECT_EQ(actual, expected);
        });
    });

}

template<typename T, typename U>
void test_vector(int width_start, int width_end) {
    constexpr bool is_bool = std::is_same_v<typename U::value_type, bool        >;
    constexpr bool is_byte = std::is_same_v<typename U::value_type, std::uint8_t>;
    static_assert(is_bool != is_byte, "Unknown vector type");

    for (int width = width_start; width <= width_end; width++) {

        const T expected = T(1) << (width - 1) | T(1);
        auto sliced_vector = cvm::bitmanip::slice<T, U>(expected);

        T actual = 0;
        for (size_t i = 0; i < sliced_vector.size(); i++)
            actual |= T(sliced_vector[i]) << i*(is_bool ? 1 : 8);

        EXPECT_EQ(actual, expected);
    }

}


TEST(Bitmanip, VectorByteFundamentalSlice) {

    test_vector<std::uint64_t, std::vector<uint8_t>>(1, 64);

}

TEST(Bitmanip, VectorByteBitsetSlice) {

    test_vector<std::bitset<128+64>, std::vector<uint8_t>>(64, 128);

}

TEST(Bitmanip, VectorBoolFundamentalSlice) {

    test_vector<std::uint64_t, std::vector<bool>>(1, 64);

}

TEST(Bitmanip, VectorBoolBitsetSlice) {

    test_vector<std::bitset<128+64>, std::vector<bool>>(64, 128);

}
