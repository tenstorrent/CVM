#pragma once

#include <cinttypes>
#include <algorithm>
#include <cassert>

namespace cvm {

    struct bitmanip {

        template <typename T>
            static constexpr T mask(size_t bits) {
                assert(8*sizeof(T) >= bits);
                if (8*sizeof(T) == bits) return ~T(0);
                return (T(1) << bits) - 1;
            }

        template <typename T>
            static constexpr T mask(size_t msb, size_t lsb) {
                assert(8*sizeof(T) > msb);
                return mask<T>(msb - lsb + 1) << lsb;
            }

        template <typename T>
            static constexpr T slice(const T& t, size_t msb, size_t lsb) {
                return (t & mask<T>(msb, lsb)) >> lsb;
            }

        template <typename V, typename T>
            static constexpr V array_slice(const T* arr, const size_t msb, const size_t lsb) {
                const std::size_t G = 8*sizeof(T);

                V v(0);

                for (size_t bit = lsb; bit <= msb; bit += G - (bit % G)) {
                    v |= V(slice(arr[bit / G], std::min(G-1, msb - bit), bit % G)) << (bit - lsb);
                }

                return v;
            };

    };
}
