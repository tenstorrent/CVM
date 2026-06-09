// SPDX-FileCopyrightText: © 2026 Tenstorrent USA, Inc.
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>
#include <algorithm>
#include <limits>
#include "cvm/random.hpp"

TEST(Random, RandomInt) {

    // Test randomness
    cvm::rand::seed(1);
    cvm::rand::uniform_dist<int64_t> dist1;
    int64_t val1 = dist1();
    int64_t val2 = dist1();

    EXPECT_NE(val1, val2);

    // Test distribution within range
    cvm::rand::seed(2);
    std::vector<uint8_t> numbers(100);
    cvm::rand::uniform_dist<uint8_t> dist2(0,99);
    std::generate(numbers.begin(), numbers.end(), dist2);

    bool all_in_range = true;
    for (int n : numbers) {
      all_in_range &= (n >= 0 && n <= 99);
    }
    EXPECT_TRUE(all_in_range);

    // Test shuffle
    cvm::rand::seed(3);
    std::vector<int32_t> numbers3(100);
    std::iota(numbers3.begin(), numbers3.end(), 1);
    std::shuffle(std::begin(numbers3), std::end(numbers3), cvm::rand::gen);

    for (int n : numbers) {
      all_in_range &= (n >= 0 && n <= 99);
    }
    EXPECT_TRUE(all_in_range);

    // Test string based dist with global get
    std::string range4 = "1:20";
    cvm::rand::seed(4);
    uint32_t val3 = cvm::rand::get<uint32_t>(range4);
    uint32_t val4 = cvm::rand::get<uint32_t>(range4);

    EXPECT_NE(val3, val4);

    // Test discrete distribution
    std::vector<double> weights = {0.1, 0.2, 0.3, 0.4};
    cvm::rand::seed(5);
    cvm::rand::discrete_dist<int> dist5(weights);
    int val5 = dist5();
    int val6 = dist5();
    std::vector<double> p = dist5.probabilities();
    double sum = 0.0;
    for (auto n : p)
      sum += n;

    EXPECT_LT(val5, 4);
    EXPECT_LT(val6, 4);
    EXPECT_EQ(sum, 1.0);
}
