#pragma once

#include <random>
#include <limits>

namespace cvm {

    namespace rand {

        template<typename T, T LO = std::numeric_limits<T>::min(), T HI = std::numeric_limits<T>::max()>
        struct rng {
            static_assert(std::is_integral_v<T>, "T must be an integral type for uniform_int_distribution");
            std::mt19937 gen;
            std::uniform_int_distribution<T> distrib;
            rng() : distrib(LO, HI) {}
            T operator()() {
                return distrib(gen);
            }
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

        void seed(uint64_t seed);
        uint32_t get(std::string flag);
        std::pair<uint32_t,uint32_t> parse(std::string flag);
    }

}
