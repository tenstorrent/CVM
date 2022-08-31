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

                size_t bits_left_in_g;
                for (size_t bit = lsb; bit <= msb; bit += bits_left_in_g) {

                    size_t bits_left = msb - bit + 1;
                    size_t lsb_g = bit % G;
                    bits_left_in_g = G - lsb_g;
                    size_t bits_to_take_in_g = std::min(bits_left, bits_left_in_g);
                    size_t msb_g = bits_to_take_in_g + lsb_g - 1;

                    v |= V(slice(arr[bit / G], msb_g, lsb_g)) << (bit - lsb);
                }

                return v;
            };

    };
}
