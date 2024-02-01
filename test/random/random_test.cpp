#include <gtest/gtest.h>
#include <limits>
#include "cvm/random.hpp"

TEST(Random, RandomInt) {

    // Test randomness
    uint64_t seed1 = 1;
    cvm::rng<int64_t> rng1(seed1);
    int64_t val1 = rng1();
    int64_t val2 = rng1();

    EXPECT_NE(val1, val2);

    // Test distribution within range
    uint64_t seed2 = 2;
    std::vector<uint8_t> numbers(100);
    cvm::rng<uint8_t, 0, 99> rng2(seed2);
    std::generate(numbers.begin(), numbers.end(), rng2);

    bool all_in_range = true;
    for (int n : numbers) {
      all_in_range &= (n >= 0 && n <= 99);
    }
    EXPECT_TRUE(all_in_range);

}
