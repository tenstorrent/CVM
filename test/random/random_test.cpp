#include <gtest/gtest.h>
#include <limits>
#include "cvm/random.hpp"

TEST(Random, RandomInt) {

    // Test randomness
    cvm::rand::seed(1);
    cvm::rand::rng<int64_t> rng1;
    int64_t val1 = rng1();
    int64_t val2 = rng1();

    EXPECT_NE(val1, val2);

    // Test distribution within range
    cvm::rand::seed(2);
    std::vector<uint8_t> numbers(100);
    cvm::rand::rng<uint8_t, 0, 99> rng2;
    std::generate(numbers.begin(), numbers.end(), rng2);

    // Test string based randomization
    std::string dist = "1:10";
    cvm::rand::seed(3);
    uint32_t v1 = cvm::rand::get(dist);
    uint32_t v2 = cvm::rand::get(dist);

    EXPECT_NE(v1, v2);

    bool all_in_range = true;
    for (int n : numbers) {
      all_in_range &= (n >= 0 && n <= 99);
    }
    EXPECT_TRUE(all_in_range);

}
