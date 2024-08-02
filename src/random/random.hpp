#pragma once

#include <random>
#include <limits>
#include <string>
#include <utility>
#include <sstream>
#include <iostream>
#include <cassert>

namespace cvm {

    namespace rand {

        // Declarations
        extern std::mt19937 gen;

        void seed(uint64_t seed);

        template<typename T>
        T get(std::string flag);

        template<typename T, typename Distribution>
        struct dist;

        // Helper type aliases for easier use
        template<typename T>
        using uniform_dist = dist<T, std::uniform_int_distribution<T>>;

        template<typename T>
        using discrete_dist = dist<T, std::discrete_distribution<T>>;

        // Specialization for uniform distribution
        template<typename T>
        struct dist<T, std::uniform_int_distribution<T>> {
            static_assert(std::is_integral_v<T>, "T must be an integral type for uniform_int_distribution");
            using result_type = T;
            std::uniform_int_distribution<T> distrib;
            T lo_;
            T hi_;

            dist(T lo = std::numeric_limits<T>::min(), T hi = std::numeric_limits<T>::max())
                : distrib(lo, hi), lo_(lo), hi_(hi) {}
            dist(std::string range) {
                auto [lo_, hi_] = parse(range);
                distrib = std::uniform_int_distribution<T>(lo_, hi_);
            }

            result_type operator()() {
                return distrib(gen);
            }
            result_type get() {
                return distrib(gen);
            }

            std::pair<T, T> parse(const std::string& range) {
                std::istringstream iss(range);
                T start, end;
                char delimiter;

                if (!(iss >> start >> delimiter >> end) || delimiter != ':') {
                    std::cout << "Plusarg: " << range << std::endl;
                    assert(0 && "Error: Invalid rand plusarg format");
                }

                return std::make_pair(start, end);
            }

            result_type min() const { return lo_; }
            result_type max() const { return hi_; }
        };

        // Specialization for discrete distribution
        template<typename T>
        struct dist<T, std::discrete_distribution<T>> {
            static_assert(std::is_integral_v<T>, "T must be an integral type for discrete_distribution");
            using result_type = T;
            std::discrete_distribution<T> distrib;
            T max_value;

            template<typename WeightIterator>
            dist(WeightIterator first, WeightIterator last)
                : distrib(first, last), max_value(std::distance(first, last) - 1) {}

            dist(const std::vector<double>& weights)
                : distrib(weights.begin(), weights.end()), max_value(weights.size() - 1) {}

            result_type operator()() {
                return distrib(gen);
            }

            result_type get() {
                return distrib(gen);
            }

            std::vector<double> probabilities() {
                return distrib.probabilities();
            }

            static constexpr result_type min() { return 0; }
            result_type max() const { return max_value; }
        };

        struct lcg {


          template <typename T>
          static T generate() {
              state = (a*state + c) % m;
              return state;
          }

          template <typename T>
          static T generate(T max) {
              return generate<T>()%max;
          }

          // magic numbers from glibc
          static inline uint64_t state = 1;
          static const uint64_t m = 1 << 31;
          static const uint64_t a = 1103515245;
          static const uint64_t c = 12345;
        };

    }

}
